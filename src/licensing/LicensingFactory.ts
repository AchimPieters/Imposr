import path from 'node:path';
import os from 'node:os';
import fs from 'node:fs';
import { ActivationService, ActivationStore } from './ActivationService';
import { FeatureGate } from './FeatureGate';
import { LicenseManager } from './LicenseManager';
import { MachineFingerprintService } from './MachineId';
import { OfflineValidator } from './OfflineValidator';
import { FileActivationStore } from './FileActivationStore';
import { LicenseError } from '@utils/errors';
import { PaymentHandler } from './PaymentHandler';
import { FileLicenseAuditLogger, LicenseAuditLogger } from './LicenseAuditLogger';
import { InMemoryWebhookReplayStore } from './WebhookReplayStore';
import { createProviderAdapter } from './providers/PaymentProviderAdapter';

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

  const defaultKeyId = process.env.LICENSE_SIGNING_KEY_ID ?? 'v1';
  const signingSecret = resolveSecret('LICENSE_SIGNING_SECRET', 'dev-license-signing-secret-12345');
  const rotatedSecrets = parseSigningSecrets(process.env.LICENSE_SIGNING_SECRETS);
  const activationSecret = resolveSecret('OFFLINE_ACTIVATION_SECRET', 'dev-offline-secret-123456');
  const webhookSecret = resolveSecret('PAYMENT_WEBHOOK_SECRET', 'dev-payment-webhook-secret-1234');
  const paymentProvider = process.env.PAYMENT_PROVIDER ?? 'generic';
  const signatureToleranceMs = parsePositiveInt(
    process.env.PAYMENT_WEBHOOK_SIGNATURE_TOLERANCE_MS,
    5 * 60 * 1000
  );

  const auditPath = process.env.LICENSE_AUDIT_LOG_PATH ?? path.join(os.tmpdir(), 'imposr', 'license-audit.ndjson');
  const auditEncryptionSecret = process.env.LICENSE_AUDIT_ENCRYPTION_SECRET;
  migrateLegacyFileIfNeeded(auditPath, process.env.LEGACY_LICENSE_AUDIT_LOG_PATH);
  const auditLogger: LicenseAuditLogger = new FileLicenseAuditLogger(auditPath, {
    encryptionSecret: auditEncryptionSecret,
  });

  const licenseManager = new LicenseManager({
    signingSecret,
    signingSecrets: { ...rotatedSecrets, [defaultKeyId]: signingSecret },
    defaultKeyId,
  });

  const featureGate = new FeatureGate({
    imposition: 'starter',
    templates: 'starter',
    batch: 'pro',
    api: 'enterprise',
  });

  const storePath =
    process.env.ACTIVATION_STORE_PATH ?? path.join(os.tmpdir(), 'imposr', 'activation-store.json');
  const activationEncryptionSecret = process.env.ACTIVATION_STORE_ENCRYPTION_SECRET;
  migrateLegacyFileIfNeeded(storePath, process.env.LEGACY_ACTIVATION_STORE_PATH);
  const store: ActivationStore = new FileActivationStore(storePath, {
    encryptionSecret: activationEncryptionSecret,
  });
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
    replayStore: new InMemoryWebhookReplayStore(),
    providerAdapter: createProviderAdapter(paymentProvider),
    signatureToleranceMs,
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

function parseSigningSecrets(raw: string | undefined): Record<string, string> {
  if (!raw || raw.trim().length === 0) {
    return {};
  }

  const result: Record<string, string> = {};
  const entries = raw.split(',').map((entry) => entry.trim()).filter(Boolean);

  for (const entry of entries) {
    const [keyId, secret] = entry.split(':');
    if (!keyId || !secret) {
      throw new LicenseError('Invalid LICENSE_SIGNING_SECRETS entry; expected keyId:secret', { entry });
    }
    result[keyId] = secret;
  }

  return result;
}

function parsePositiveInt(raw: string | undefined, fallback: number): number {
  if (!raw || raw.trim().length === 0 || raw === 'undefined' || raw === 'null') {
    return fallback;
  }

  const parsed = Number(raw);
  if (!Number.isInteger(parsed) || parsed <= 0) {
    throw new LicenseError('Expected positive integer env value', { raw });
  }

  return parsed;
}

function migrateLegacyFileIfNeeded(targetPath: string, legacyPath: string | undefined): void {
  if (!legacyPath || legacyPath.trim().length === 0 || legacyPath === targetPath) {
    return;
  }

  try {
    fs.accessSync(targetPath);
    return;
  } catch {
    // no-op, continue fallback lookup
  }

  try {
    const legacyData = fs.readFileSync(legacyPath, 'utf8');
    const targetDir = path.dirname(targetPath);
    fs.mkdirSync(targetDir, { recursive: true });
    fs.writeFileSync(targetPath, legacyData, 'utf8');
  } catch (error) {
    const nodeError = error as NodeJS.ErrnoException;
    if (nodeError.code === 'ENOENT') {
      return;
    }

    throw new LicenseError('Unable to migrate legacy licensing state file', {
      targetPath,
      legacyPath,
      cause: error instanceof Error ? error.message : 'unknown',
    });
  }
}
