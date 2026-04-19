import fs from 'node:fs/promises';
import path from 'node:path';
import { ActivationRecord, ActivationStore } from './ActivationService';
import { LicenseError } from '@utils/errors';

interface ActivationStoreFile {
  records: Record<string, ActivationRecord>;
}

/**
 * JSON-file-backed activation store for persistent desktop activation state.
 */
export class FileActivationStore implements ActivationStore {
  private readonly filePath: string;

  /**
   * @param filePath Absolute path to JSON store file.
   */
  constructor(filePath: string) {
    this.filePath = filePath;
  }

  async getByLicenseId(licenseId: string): Promise<ActivationRecord | null> {
    const store = await this.readStore();
    return store.records[licenseId] ?? null;
  }

  async save(record: ActivationRecord): Promise<void> {
    const store = await this.readStore();
    store.records[record.licenseId] = record;
    await this.writeStore(store);
  }

  async deleteByLicenseId(licenseId: string): Promise<void> {
    const store = await this.readStore();
    delete store.records[licenseId];
    await this.writeStore(store);
  }

  private async readStore(): Promise<ActivationStoreFile> {
    try {
      const raw = await fs.readFile(this.filePath, 'utf8');
      const parsed = JSON.parse(raw) as ActivationStoreFile;
      if (!parsed || typeof parsed !== 'object' || !parsed.records) {
        throw new LicenseError('Activation store is malformed', { filePath: this.filePath });
      }

      return parsed;
    } catch (error) {
      const nodeError = error as NodeJS.ErrnoException;
      if (nodeError?.code === 'ENOENT') {
        return { records: {} };
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
      await fs.writeFile(tempFilePath, JSON.stringify(store, null, 2), 'utf8');
      await fs.rename(tempFilePath, this.filePath);
    } catch (error) {
      throw new LicenseError('Unable to write activation store', {
        filePath: this.filePath,
        cause: error instanceof Error ? error.message : 'unknown',
      });
    }
  }
}
