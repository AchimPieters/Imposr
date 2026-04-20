import fs from 'node:fs/promises';
import { LicenseError } from '@utils/errors';

interface FileLockOptions {
  retryDelayMs?: number;
  timeoutMs?: number;
  staleLockMs?: number;
}

const DEFAULT_RETRY_DELAY_MS = 20;
const DEFAULT_TIMEOUT_MS = 5_000;
const DEFAULT_STALE_LOCK_MS = 30_000;

function wait(delayMs: number): Promise<void> {
  return new Promise((resolve) => {
    setTimeout(resolve, delayMs);
  });
}

/**
 * Runs an async operation while holding an advisory lock file.
 */
export async function withFileLock<T>(
  lockPath: string,
  operation: () => Promise<T>,
  options: FileLockOptions = {}
): Promise<T> {
  const retryDelayMs = options.retryDelayMs ?? DEFAULT_RETRY_DELAY_MS;
  const timeoutMs = options.timeoutMs ?? DEFAULT_TIMEOUT_MS;
  const staleLockMs = options.staleLockMs ?? DEFAULT_STALE_LOCK_MS;
  const start = Date.now();

  while (true) {
    let handle: fs.FileHandle | null = null;
    try {
      handle = await fs.open(lockPath, 'wx');
      break;
    } catch (error) {
      const nodeError = error as NodeJS.ErrnoException;
      if (nodeError.code !== 'EEXIST') {
        throw new LicenseError('Unable to acquire storage lock', {
          lockPath,
          cause: nodeError.message,
        });
      }

      if (Date.now() - start > timeoutMs) {
        throw new LicenseError('Timed out waiting for storage lock', { lockPath, timeoutMs });
      }

      try {
        const stats = await fs.stat(lockPath);
        if (Date.now() - stats.mtimeMs > staleLockMs) {
          await fs.rm(lockPath, { force: true });
          continue;
        }
      } catch {
        // lock file may have disappeared between checks
      }

      await wait(retryDelayMs);
    } finally {
      if (handle) {
        await handle.close();
      }
    }
  }

  try {
    return await operation();
  } finally {
    await fs.rm(lockPath, { force: true });
  }
}
