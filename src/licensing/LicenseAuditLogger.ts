import fs from 'node:fs/promises';
import path from 'node:path';
import { LicenseError } from '@utils/errors';
import { withFileLock } from './FileLock';
import { createFileCodec, FileCodec } from './EncryptedFileCodec';

/** Supported audit event names for commercial licensing flows. */
export type LicenseAuditEventType =
  | 'license.verify'
  | 'license.activate'
  | 'license.deactivate'
  | 'offline.validate'
  | 'payment.webhook'
  | 'license.issue.trial'
  | 'license.issue.paid';

/** Normalized audit record payload. */
export interface LicenseAuditRecord {
  eventType: LicenseAuditEventType;
  timestamp: string;
  success: boolean;
  details: Record<string, unknown>;
}

/** Logging contract for license/commercial audit events. */
export interface LicenseAuditLogger {
  log(record: LicenseAuditRecord): Promise<void>;
  readRecent(limit: number): Promise<LicenseAuditRecord[]>;
}

/**
 * No-op audit logger used when audit persistence is disabled.
 */
export class NoopLicenseAuditLogger implements LicenseAuditLogger {
  async log(_record: LicenseAuditRecord): Promise<void> {
    // intentionally no-op
  }

  async readRecent(_limit: number): Promise<LicenseAuditRecord[]> {
    return [];
  }
}

/**
 * File-backed append-only NDJSON audit logger.
 */
export class FileLicenseAuditLogger implements LicenseAuditLogger {
  private readonly lockPath: string;
  private readonly codec: FileCodec;

  constructor(filePath: string, options: { encryptionSecret?: string } = {}) {
    this.filePath = filePath;
    this.lockPath = `${filePath}.lock`;
    this.codec = createFileCodec(options.encryptionSecret);
  }

  private readonly filePath: string;

  async log(record: LicenseAuditRecord): Promise<void> {
    try {
      const dir = path.dirname(this.filePath);
      await fs.mkdir(dir, { recursive: true });
      await withFileLock(this.lockPath, async () => {
        if (this.codec.enabled) {
          const records = await this.readAll();
          records.push(record);
          await this.writeAll(records);
          return;
        }

        await fs.appendFile(this.filePath, `${JSON.stringify(record)}\n`, 'utf8');
      });
    } catch (error) {
      throw new LicenseError('Unable to write license audit log', {
        filePath: this.filePath,
        cause: error instanceof Error ? error.message : 'unknown',
      });
    }
  }

  async readRecent(limit: number): Promise<LicenseAuditRecord[]> {
    if (!Number.isInteger(limit) || limit <= 0 || limit > 1000) {
      throw new LicenseError('Audit read limit must be an integer between 1 and 1000');
    }

    try {
      return withFileLock(this.lockPath, async () => {
        const parsed = await this.readAll();
        return parsed.slice(-limit).reverse();
      });
    } catch (error) {
      const nodeError = error as NodeJS.ErrnoException;
      if (nodeError?.code === 'ENOENT') {
        return [];
      }

      throw new LicenseError('Unable to read license audit log', {
        filePath: this.filePath,
        cause: error instanceof Error ? error.message : 'unknown',
      });
    }
  }

  private parseNdjson(content: string): LicenseAuditRecord[] {
    return content
      .split('\n')
      .map((line) => line.trim())
      .filter((line) => line.length > 0)
      .map((line) => JSON.parse(line) as LicenseAuditRecord);
  }

  private async readAll(): Promise<LicenseAuditRecord[]> {
    let content: string;
    try {
      content = await fs.readFile(this.filePath, 'utf8');
    } catch (error) {
      const nodeError = error as NodeJS.ErrnoException;
      if (nodeError?.code === 'ENOENT') {
        return [];
      }
      throw error;
    }

    let decoded = content;
    let migrated = false;
    if (this.codec.enabled) {
      try {
        decoded = this.codec.decode(content);
      } catch {
        // Backward compatibility: encryption enabled while existing file is plaintext.
        migrated = true;
      }
    }

    const trimmed = decoded.trim();
    if (trimmed.length === 0) {
      return [];
    }

    if (trimmed.startsWith('[')) {
      const parsed = JSON.parse(trimmed) as LicenseAuditRecord[];
      await this.writeAll(parsed);
      return parsed;
    }

    const records = this.parseNdjson(decoded);
    if (migrated) {
      await this.writeAll(records);
    }
    return records;
  }

  private async writeAll(records: LicenseAuditRecord[]): Promise<void> {
    const serialized = records.map((record) => JSON.stringify(record)).join('\n');
    const payload = serialized.length > 0 ? `${serialized}\n` : '';
    const encoded = this.codec.enabled ? this.codec.encode(payload) : payload;
    const tempPath = `${this.filePath}.tmp`;
    await fs.writeFile(tempPath, encoded, 'utf8');
    await fs.rename(tempPath, this.filePath);
  }
}
