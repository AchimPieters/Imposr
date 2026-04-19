import { LicenseError } from '../../../src/utils/errors';
import { LicenseManager, LicensePayload } from '../../../src/licensing/LicenseManager';

describe('LicenseManager', () => {
  const now = new Date('2026-04-19T00:00:00.000Z');

  const payload: LicensePayload = {
    id: 'lic_123',
    tier: 'pro',
    issuedAt: '2026-04-01T00:00:00.000Z',
    expiresAt: '2026-12-31T23:59:59.000Z',
    customerEmail: 'test@example.com',
    maxActivations: 3,
    features: ['imposition', 'batch', 'api'],
  };

  it('signs and verifies a valid license', () => {
    const manager = new LicenseManager({
      signingSecret: 'super-secret-value-12345',
      now: () => now,
    });

    const key = manager.sign(payload);
    const result = manager.verify(key);

    expect(result.status).toBe('valid');
    expect(result.payload).toMatchObject(payload);
  });

  it('returns missing for empty key', () => {
    const manager = new LicenseManager({ signingSecret: 'super-secret-value-12345' });
    expect(manager.verify('')).toMatchObject({ status: 'missing', payload: null });
  });

  it('returns invalid for tampered key', () => {
    const manager = new LicenseManager({ signingSecret: 'super-secret-value-12345' });
    const key = manager.sign(payload);
    const tampered = `${key.slice(0, -1)}0`;

    const result = manager.verify(tampered);

    expect(result.status).toBe('invalid');
    expect(result.payload).toBeNull();
  });

  it('returns expired for expired key', () => {
    const manager = new LicenseManager({
      signingSecret: 'super-secret-value-12345',
      now: () => new Date('2027-01-01T00:00:00.000Z'),
    });

    const key = manager.sign(payload);
    const result = manager.verify(key);

    expect(result.status).toBe('expired');
    expect(result.payload?.id).toBe(payload.id);
  });

  it('throws when assertValid fails', () => {
    const manager = new LicenseManager({ signingSecret: 'super-secret-value-12345' });

    expect(() => manager.assertValid('invalid')).toThrow(LicenseError);
  });

  it('throws when constructed with weak secret', () => {
    expect(() => new LicenseManager({ signingSecret: 'short' })).toThrow(LicenseError);
  });
});
