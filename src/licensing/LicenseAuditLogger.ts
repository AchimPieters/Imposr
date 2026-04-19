import fs from 'node:fs/promises';
import path from 'node:path';
import { LicenseError } from '@utils/errors';

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
  constructor(private readonly filePath: string) {}

  async log(record: LicenseAuditRecord): Promise<void> {
    try {
      const dir = path.dirname(this.filePath);
      await fs.mkdir(dir, { recursive: true });
      await fs.appendFile(this.filePath, `${JSON.stringify(record)}\n`, 'utf8');
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
      const content = await fs.readFile(this.filePath, 'utf8');
      const lines = content
        .split('\n')
        .map((line) => line.trim())
        .filter((line) => line.length > 0);
      const parsed = lines.map((line) => JSON.parse(line) as LicenseAuditRecord);
      return parsed.slice(-limit).reverse();
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
}
