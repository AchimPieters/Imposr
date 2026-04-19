import { randomUUID } from 'node:crypto';
import { BatchProcessingError, LicenseError } from '@utils/errors';
import { LicenseManager } from './LicenseManager';
import { MachineFingerprintService } from './MachineId';
import { LicenseAuditLogger, NoopLicenseAuditLogger } from './LicenseAuditLogger';

/** Persisted activation state record. */
export interface ActivationRecord {
  activationId: string;
  licenseId: string;
  machineFingerprint: string;
  activatedAt: string;
}

/** Storage contract for local activation persistence. */
export interface ActivationStore {
  getByLicenseId(licenseId: string): Promise<ActivationRecord | null>;
  save(record: ActivationRecord): Promise<void>;
  deleteByLicenseId(licenseId: string): Promise<void>;
}

/**
 * In-memory activation store for development/tests.
 */
export class InMemoryActivationStore implements ActivationStore {
  private readonly records = new Map<string, ActivationRecord>();

  async getByLicenseId(licenseId: string): Promise<ActivationRecord | null> {
    return this.records.get(licenseId) ?? null;
  }

  async save(record: ActivationRecord): Promise<void> {
    this.records.set(record.licenseId, record);
  }

  async deleteByLicenseId(licenseId: string): Promise<void> {
    this.records.delete(licenseId);
  }
}

/**
 * Handles machine-bound activation and deactivation lifecycle.
 */
export class ActivationService {
  private readonly licenseManager: LicenseManager;
  private readonly machineFingerprintService: MachineFingerprintService;
  private readonly store: ActivationStore;
  private readonly now: () => Date;
  private readonly auditLogger: LicenseAuditLogger;

  /**
   * @param licenseManager License verification component.
   * @param machineFingerprintService Machine fingerprint provider.
   * @param store Persistent activation store.
   * @param now Optional injected clock.
   * @param auditLogger Optional audit logger.
   */
  constructor(
    licenseManager: LicenseManager,
    machineFingerprintService: MachineFingerprintService,
    store: ActivationStore,
    now: () => Date = () => new Date(),
    auditLogger: LicenseAuditLogger = new NoopLicenseAuditLogger()
  ) {
    this.licenseManager = licenseManager;
    this.machineFingerprintService = machineFingerprintService;
    this.store = store;
    this.now = now;
    this.auditLogger = auditLogger;
  }

  /**
   * Activates the current machine for a valid license key.
   * @throws LicenseError If license is invalid or already activated on another machine.
   */
  async activate(licenseKey: string): Promise<ActivationRecord> {
    const payload = this.licenseManager.assertValid(licenseKey);
    const machineFingerprint = await this.machineFingerprintService.getFingerprint();

    try {
      const existing = await this.store.getByLicenseId(payload.id);
      if (existing && existing.machineFingerprint !== machineFingerprint) {
        throw new LicenseError('License is already activated on another machine', {
          licenseId: payload.id,
          activationId: existing.activationId,
        });
      }

      const record: ActivationRecord = {
        activationId: existing?.activationId ?? randomUUID(),
        licenseId: payload.id,
        machineFingerprint,
        activatedAt: existing?.activatedAt ?? this.now().toISOString(),
      };

      await this.store.save(record);
      await this.auditLogger.log({
        eventType: 'license.activate',
        timestamp: this.now().toISOString(),
        success: true,
        details: { licenseId: payload.id, activationId: record.activationId },
      });
      return record;
    } catch (error) {
      await this.auditLogger.log({
        eventType: 'license.activate',
        timestamp: this.now().toISOString(),
        success: false,
        details: {
          licenseId: payload.id,
          reason: error instanceof Error ? error.message : 'unknown',
        },
      });

      if (error instanceof LicenseError) {
        throw error;
      }

      throw new BatchProcessingError('Unable to persist activation record', payload.id, {
        cause: error instanceof Error ? error.message : 'unknown',
      });
    }
  }

  /**
   * Removes local activation for a valid license key.
   */
  async deactivate(licenseKey: string): Promise<void> {
    const payload = this.licenseManager.assertValid(licenseKey);

    try {
      await this.store.deleteByLicenseId(payload.id);
      await this.auditLogger.log({
        eventType: 'license.deactivate',
        timestamp: this.now().toISOString(),
        success: true,
        details: { licenseId: payload.id },
      });
    } catch (error) {
      await this.auditLogger.log({
        eventType: 'license.deactivate',
        timestamp: this.now().toISOString(),
        success: false,
        details: {
          licenseId: payload.id,
          reason: error instanceof Error ? error.message : 'unknown',
        },
      });

      throw new BatchProcessingError('Unable to remove activation record', payload.id, {
        cause: error instanceof Error ? error.message : 'unknown',
      });
    }
  }

  /**
   * Returns activation state for current machine and license key.
   */
  async getActivation(licenseKey: string): Promise<ActivationRecord | null> {
    const payload = this.licenseManager.assertValid(licenseKey);
    const machineFingerprint = await this.machineFingerprintService.getFingerprint();

    const record = await this.store.getByLicenseId(payload.id);
    if (!record) {
      return null;
    }

    if (record.machineFingerprint !== machineFingerprint) {
      throw new LicenseError('Activation exists but is bound to another machine', {
        licenseId: payload.id,
      });
    }

    return record;
  }
}
