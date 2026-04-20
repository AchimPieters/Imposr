import fs from 'node:fs/promises';
import path from 'node:path';
import { ActivationRecord, ActivationStore } from './ActivationService';
import { LicenseError } from '@utils/errors';
import { withFileLock } from './FileLock';
import { createFileCodec, FileCodec } from './EncryptedFileCodec';

interface ActivationStoreFile {
  records: Record<string, ActivationRecord>;
}

interface FileActivationStoreOptions {
  encryptionSecret?: string;
}

interface ReadStoreResult {
  store: ActivationStoreFile;
  migrated: boolean;
}

/**
 * JSON-file-backed activation store for persistent desktop activation state.
 */
export class FileActivationStore implements ActivationStore {
  private readonly filePath: string;
  private readonly lockPath: string;
  private readonly codec: FileCodec;

  /**
   * @param filePath Absolute path to JSON store file.
   */
  constructor(filePath: string, options: FileActivationStoreOptions = {}) {
    this.filePath = filePath;
    this.lockPath = `${filePath}.lock`;
    this.codec = createFileCodec(options.encryptionSecret);
  }

  async getByLicenseId(licenseId: string): Promise<ActivationRecord | null> {
    return this.withLock(async () => {
      const { store, migrated } = await this.readStore();
      if (migrated) {
        await this.writeStore(store);
      }
      return store.records[licenseId] ?? null;
    });
  }

  async save(record: ActivationRecord): Promise<void> {
    await this.withLock(async () => {
      const { store } = await this.readStore();
      store.records[record.licenseId] = record;
      await this.writeStore(store);
    });
  }

  async deleteByLicenseId(licenseId: string): Promise<void> {
    await this.withLock(async () => {
      const { store } = await this.readStore();
      delete store.records[licenseId];
      await this.writeStore(store);
    });
  }

  private async readStore(): Promise<ReadStoreResult> {
    try {
      const raw = await fs.readFile(this.filePath, 'utf8');
      let decoded = raw;
      let migrated = false;

      if (this.codec.enabled) {
        try {
          decoded = this.codec.decode(raw);
        } catch {
          // Backward compatibility: existing plaintext file with encryption newly enabled.
          migrated = true;
        }
      }

      const normalized = this.normalizeParsedStore(JSON.parse(decoded) as unknown);
      return {
        store: normalized.store,
        migrated: normalized.migrated || migrated,
      };
    } catch (error) {
      const nodeError = error as NodeJS.ErrnoException;
      if (nodeError?.code === 'ENOENT') {
        return { store: { records: {} }, migrated: false };
      }

      if (error instanceof LicenseError) {
        throw error;
      }

      throw new LicenseError('Unable to read activation store', {
        filePath: this.filePath,
        cause: error instanceof Error ? error.message : 'unknown',
      });
    }
  }

  private async writeStore(store: ActivationStoreFile): Promise<void> {
    try {
      const directory = path.dirname(this.filePath);
      await fs.mkdir(directory, { recursive: true });
      const tempFilePath = `${this.filePath}.tmp`;
      const serialized = JSON.stringify(store, null, 2);
      const encoded = this.codec.enabled ? this.codec.encode(serialized) : serialized;
      await fs.writeFile(tempFilePath, encoded, 'utf8');
      await fs.rename(tempFilePath, this.filePath);
    } catch (error) {
      throw new LicenseError('Unable to write activation store', {
        filePath: this.filePath,
        cause: error instanceof Error ? error.message : 'unknown',
      });
    }
  }

  private async withLock<T>(operation: () => Promise<T>): Promise<T> {
    return withFileLock(this.lockPath, operation);
  }

  private normalizeParsedStore(parsed: unknown): ReadStoreResult {
    if (!parsed || typeof parsed !== 'object') {
      throw new LicenseError('Activation store is malformed', { filePath: this.filePath });
    }

    if ('records' in parsed) {
      const records = this.normalizeRecords((parsed as { records: unknown }).records);
      return { store: { records }, migrated: false };
    }

    const records = this.normalizeRecords(parsed);
    return { store: { records }, migrated: true };
  }

  private normalizeRecords(input: unknown): Record<string, ActivationRecord> {
    if (!input || typeof input !== 'object') {
      throw new LicenseError('Activation store records are malformed', { filePath: this.filePath });
    }

    const normalized: Record<string, ActivationRecord> = {};
    for (const [licenseId, value] of Object.entries(input)) {
      if (!value || typeof value !== 'object') {
        throw new LicenseError('Activation record is malformed', { filePath: this.filePath, licenseId });
      }

      const candidate = value as Partial<ActivationRecord> & { machineId?: string };
      const machineFingerprint = candidate.machineFingerprint ?? candidate.machineId;
      if (!candidate.activationId || !machineFingerprint || !candidate.activatedAt) {
        throw new LicenseError('Activation record is missing required fields', {
          filePath: this.filePath,
          licenseId,
        });
      }

      normalized[licenseId] = {
        activationId: candidate.activationId,
        licenseId: candidate.licenseId ?? licenseId,
        machineFingerprint,
        activatedAt: candidate.activatedAt,
      };
    }

    return normalized;
  }
}
