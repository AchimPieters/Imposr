import { OfflineValidator, OfflineActivationPayload } from '../../../src/licensing/OfflineValidator';
import { LicenseManager, LicensePayload } from '../../../src/licensing/LicenseManager';

describe('OfflineValidator', () => {
  const licensePayload: LicensePayload = {
    id: 'lic_999',
    tier: 'enterprise',
    issuedAt: '2026-01-01T00:00:00.000Z',
    expiresAt: '2026-12-31T00:00:00.000Z',
    customerEmail: 'enterprise@example.com',
    maxActivations: 50,
    features: ['imposition', 'batch', 'api'],
  };

  const machineFingerprint = 'a'.repeat(64);

  it('validates matching license, token and machine fingerprint', () => {
    const licenseManager = new LicenseManager({ signingSecret: 'license-secret-123456' });
    const validator = new OfflineValidator({
      activationSecret: 'activation-secret-123456',
      licenseManager,
      now: () => new Date('2026-04-19T00:00:00.000Z'),
    });

    const licenseKey = licenseManager.sign(licensePayload);
    const tokenPayload: OfflineActivationPayload = {
      licenseId: licensePayload.id,
      machineFingerprint,
      activatedAt: '2026-04-01T00:00:00.000Z',
      expiresAt: '2026-05-01T00:00:00.000Z',
    };

    const token = validator.signToken(tokenPayload);
    const result = validator.validate(licenseKey, token, machineFingerprint);

    expect(result.valid).toBe(true);
    expect(result.license?.id).toBe(licensePayload.id);
  });

  it('fails for mismatched machine fingerprint', () => {
    const licenseManager = new LicenseManager({ signingSecret: 'license-secret-123456' });
    const validator = new OfflineValidator({
      activationSecret: 'activation-secret-123456',
      licenseManager,
    });

    const licenseKey = licenseManager.sign(licensePayload);
    const token = validator.signToken({
      licenseId: licensePayload.id,
      machineFingerprint,
      activatedAt: '2026-04-01T00:00:00.000Z',
      expiresAt: '2026-06-01T00:00:00.000Z',
    });

    const result = validator.validate(licenseKey, token, 'b'.repeat(64));

    expect(result.valid).toBe(false);
    expect(result.reason).toContain('another machine');
  });

  it('fails when token is expired', () => {
    const licenseManager = new LicenseManager({ signingSecret: 'license-secret-123456' });
    const validator = new OfflineValidator({
      activationSecret: 'activation-secret-123456',
      licenseManager,
      now: () => new Date('2026-07-01T00:00:00.000Z'),
    });

    const licenseKey = licenseManager.sign(licensePayload);
    const token = validator.signToken({
      licenseId: licensePayload.id,
      machineFingerprint,
      activatedAt: '2026-04-01T00:00:00.000Z',
      expiresAt: '2026-06-01T00:00:00.000Z',
    });

    const result = validator.validate(licenseKey, token, machineFingerprint);

    expect(result.valid).toBe(false);
    expect(result.reason).toContain('expired');
  });
});
