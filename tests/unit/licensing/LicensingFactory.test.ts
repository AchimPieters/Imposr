import { LicenseError } from '../../../src/utils/errors';
import { getLicensingRuntime, resetLicensingRuntime } from '../../../src/licensing/LicensingFactory';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';

describe('LicensingFactory', () => {
  const originalNodeEnv = process.env.NODE_ENV;
  const originalSigningSecret = process.env.LICENSE_SIGNING_SECRET;
  const originalActivationSecret = process.env.OFFLINE_ACTIVATION_SECRET;
  const originalWebhookSecret = process.env.PAYMENT_WEBHOOK_SECRET;
  const originalSigningSecrets = process.env.LICENSE_SIGNING_SECRETS;
  const originalLegacyActivationPath = process.env.LEGACY_ACTIVATION_STORE_PATH;
  const originalLegacyAuditPath = process.env.LEGACY_LICENSE_AUDIT_LOG_PATH;
  const originalActivationStorePath = process.env.ACTIVATION_STORE_PATH;
  const originalAuditPath = process.env.LICENSE_AUDIT_LOG_PATH;
  const originalPaymentProvider = process.env.PAYMENT_PROVIDER;
  const originalSignatureTolerance = process.env.PAYMENT_WEBHOOK_SIGNATURE_TOLERANCE_MS;

  afterEach(() => {
    process.env.NODE_ENV = originalNodeEnv;
    process.env.LICENSE_SIGNING_SECRET = originalSigningSecret;
    process.env.OFFLINE_ACTIVATION_SECRET = originalActivationSecret;
    process.env.PAYMENT_WEBHOOK_SECRET = originalWebhookSecret;
    process.env.LICENSE_SIGNING_SECRETS = originalSigningSecrets;
    process.env.LEGACY_ACTIVATION_STORE_PATH = originalLegacyActivationPath;
    process.env.LEGACY_LICENSE_AUDIT_LOG_PATH = originalLegacyAuditPath;
    process.env.ACTIVATION_STORE_PATH = originalActivationStorePath;
    process.env.LICENSE_AUDIT_LOG_PATH = originalAuditPath;
    process.env.PAYMENT_PROVIDER = originalPaymentProvider;
    process.env.PAYMENT_WEBHOOK_SIGNATURE_TOLERANCE_MS = originalSignatureTolerance;
    resetLicensingRuntime();
  });

  beforeEach(() => {
    delete process.env.PAYMENT_PROVIDER;
    delete process.env.PAYMENT_WEBHOOK_SIGNATURE_TOLERANCE_MS;
  });

  it('returns cached runtime instance', () => {
    process.env.NODE_ENV = 'test';
    const first = getLicensingRuntime();
    const second = getLicensingRuntime();

    expect(first).toBe(second);
  });

  it('supports rotated key parsing from env', () => {
    process.env.NODE_ENV = 'test';
    process.env.LICENSE_SIGNING_SECRET = 'current-signing-secret-123456';
    process.env.LICENSE_SIGNING_SECRETS = 'legacy:legacyrotatedsecret123456';
    process.env.OFFLINE_ACTIVATION_SECRET = 'offline-secret-value-123456';
    process.env.PAYMENT_WEBHOOK_SECRET = 'webhook-secret-value-123456';

    expect(() => getLicensingRuntime()).not.toThrow();
  });

  it('throws in production when secrets are missing', () => {
    process.env.NODE_ENV = 'production';
    delete process.env.LICENSE_SIGNING_SECRET;
    delete process.env.OFFLINE_ACTIVATION_SECRET;
    delete process.env.PAYMENT_WEBHOOK_SECRET;

    expect(() => getLicensingRuntime()).toThrow(LicenseError);
  });

  it('migrates legacy file paths when new paths are empty', () => {
    process.env.NODE_ENV = 'test';
    process.env.LICENSE_SIGNING_SECRET = 'current-signing-secret-123456';
    delete process.env.LICENSE_SIGNING_SECRETS;
    process.env.OFFLINE_ACTIVATION_SECRET = 'offline-secret-value-123456';
    process.env.PAYMENT_WEBHOOK_SECRET = 'webhook-secret-value-123456';

    const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'imposr-licensing-factory-'));
    const legacyActivation = path.join(dir, 'legacy-activation.json');
    const legacyAudit = path.join(dir, 'legacy-audit.ndjson');
    const activationPath = path.join(dir, 'activation.json');
    const auditPath = path.join(dir, 'audit.ndjson');

    fs.writeFileSync(legacyActivation, JSON.stringify({ records: {} }), 'utf8');
    fs.writeFileSync(legacyAudit, '', 'utf8');

    process.env.LEGACY_ACTIVATION_STORE_PATH = legacyActivation;
    process.env.LEGACY_LICENSE_AUDIT_LOG_PATH = legacyAudit;
    process.env.ACTIVATION_STORE_PATH = activationPath;
    process.env.LICENSE_AUDIT_LOG_PATH = auditPath;

    expect(() => getLicensingRuntime()).not.toThrow();
    expect(fs.existsSync(activationPath)).toBe(true);
    expect(fs.existsSync(auditPath)).toBe(true);
  });

  it('throws for unsupported payment provider', () => {
    process.env.NODE_ENV = 'test';
    process.env.LICENSE_SIGNING_SECRET = 'current-signing-secret-123456';
    process.env.OFFLINE_ACTIVATION_SECRET = 'offline-secret-value-123456';
    process.env.PAYMENT_WEBHOOK_SECRET = 'webhook-secret-value-123456';
    process.env.PAYMENT_PROVIDER = 'unknown-provider';

    expect(() => getLicensingRuntime()).toThrow(LicenseError);
  });
});
