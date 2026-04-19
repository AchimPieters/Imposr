import path from 'node:path';
import os from 'node:os';
import { ActivationService, ActivationStore } from './ActivationService';
import { FeatureGate } from './FeatureGate';
import { LicenseManager } from './LicenseManager';
import { MachineFingerprintService } from './MachineId';
import { OfflineValidator } from './OfflineValidator';
import { FileActivationStore } from './FileActivationStore';
import { LicenseError } from '@utils/errors';
import { PaymentHandler } from './PaymentHandler';
import { FileLicenseAuditLogger, LicenseAuditLogger } from './LicenseAuditLogger';

/** Shared licensing runtime dependencies. */
export interface LicensingRuntime {
  licenseManager: LicenseManager;
  featureGate: FeatureGate;
  activationService: ActivationService;
  offlineValidator: OfflineValidator;
  paymentHandler: PaymentHandler;
  auditLogger: LicenseAuditLogger;
}

let cachedRuntime: LicensingRuntime | null = null;

/**
 * Creates or returns singleton licensing runtime.
 */
export function getLicensingRuntime(): LicensingRuntime {
  if (cachedRuntime) {
    return cachedRuntime;
  }

  const signingSecret = resolveSecret('LICENSE_SIGNING_SECRET', 'dev-license-signing-secret-12345');
  const activationSecret = resolveSecret('OFFLINE_ACTIVATION_SECRET', 'dev-offline-secret-123456');
  const webhookSecret = resolveSecret('PAYMENT_WEBHOOK_SECRET', 'dev-payment-webhook-secret-1234');

  const auditPath = process.env.LICENSE_AUDIT_LOG_PATH ?? path.join(os.tmpdir(), 'imposr', 'license-audit.ndjson');
  const auditLogger: LicenseAuditLogger = new FileLicenseAuditLogger(auditPath);

  const licenseManager = new LicenseManager({ signingSecret });
  const featureGate = new FeatureGate({
    imposition: 'starter',
    templates: 'starter',
    batch: 'pro',
    api: 'enterprise',
  });

  const storePath =
    process.env.ACTIVATION_STORE_PATH ?? path.join(os.tmpdir(), 'imposr', 'activation-store.json');
  const store: ActivationStore = new FileActivationStore(storePath);
  const activationService = new ActivationService(
    licenseManager,
    new MachineFingerprintService(),
    store,
    () => new Date(),
    auditLogger
  );

  const offlineValidator = new OfflineValidator({ activationSecret, licenseManager });
  const paymentHandler = new PaymentHandler({
    webhookSecret,
    licenseManager,
    activationService,
    auditLogger,
  });

  cachedRuntime = {
    licenseManager,
    featureGate,
    activationService,
    offlineValidator,
    paymentHandler,
    auditLogger,
  };

  return cachedRuntime;
}

/**
 * Resets cached runtime for test isolation.
 */
export function resetLicensingRuntime(): void {
  cachedRuntime = null;
}

function resolveSecret(name: string, developmentDefault: string): string {
  const value = process.env[name];
  if (value && value.trim().length > 0) {
    return value;
  }

  if (process.env.NODE_ENV === 'production') {
    throw new LicenseError(`${name} must be configured in production`);
  }

  return developmentDefault;
}
