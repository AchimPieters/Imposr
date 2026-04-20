import fs from 'node:fs/promises';
import os from 'node:os';
import path from 'node:path';
import { withFileLock } from '../../../src/licensing/FileLock';

describe('withFileLock', () => {
  it('reclaims stale lock files', async () => {
    const dir = await fs.mkdtemp(path.join(os.tmpdir(), 'imposr-file-lock-'));
    const lockPath = path.join(dir, 'resource.lock');
    await fs.writeFile(lockPath, 'stale', 'utf8');

    const staleDate = new Date(Date.now() - 60_000);
    await fs.utimes(lockPath, staleDate, staleDate);

    await expect(
      withFileLock(
        lockPath,
        async () => {
          await fs.writeFile(path.join(dir, 'result.txt'), 'ok', 'utf8');
        },
        { staleLockMs: 1_000, timeoutMs: 2_000, retryDelayMs: 10 }
      )
    ).resolves.toBeUndefined();
  });
});
