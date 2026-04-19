import { machineId } from 'node-machine-id';
import { createHash } from 'node:crypto';
import { LicenseError } from '@utils/errors';

/** Portable interface for obtaining machine fingerprints. */
export interface MachineIdProvider {
  getRawId(): Promise<string>;
}

/**
 * Default provider backed by node-machine-id.
 */
export class NodeMachineIdProvider implements MachineIdProvider {
  /**
   * Reads the OS-level machine id in non-original mode.
   */
  async getRawId(): Promise<string> {
    return machineId(false);
  }
}

/**
 * Deterministic machine-fingerprint service used by activation flows.
 */
export class MachineFingerprintService {
  private readonly provider: MachineIdProvider;

  /**
   * @param provider Optional custom provider for tests/mocks.
   */
  constructor(provider: MachineIdProvider = new NodeMachineIdProvider()) {
    this.provider = provider;
  }

  /**
   * Returns a normalized and privacy-preserving machine fingerprint.
   * @throws LicenseError If machine id retrieval fails.
   */
  async getFingerprint(): Promise<string> {
    try {
      const rawId = await this.provider.getRawId();
      if (!rawId || rawId.trim().length === 0) {
        throw new LicenseError('Machine id is empty');
      }

      return createHash('sha256').update(rawId, 'utf8').digest('hex');
    } catch (error) {
      if (error instanceof LicenseError) {
        throw error;
      }

      throw new LicenseError('Unable to retrieve machine id', {
        cause: error instanceof Error ? error.message : 'unknown',
      });
    }
  }
}
