import { FeatureNotAvailableError } from '../../../src/utils/errors';
import { FeatureGate } from '../../../src/licensing/FeatureGate';
import { LicenseInfo } from '../../../src/licensing/LicenseManager';

describe('FeatureGate', () => {
  const gate = new FeatureGate({
    imposition: 'starter',
    batch: 'pro',
    api: 'enterprise',
  });

  const validProLicense: LicenseInfo = {
    status: 'valid',
    payload: {
      id: 'lic_1',
      tier: 'pro',
      issuedAt: '2026-01-01T00:00:00.000Z',
      expiresAt: '2026-12-31T00:00:00.000Z',
      customerEmail: 'pro@example.com',
      maxActivations: 5,
      features: ['imposition', 'batch'],
    },
  };

  it('permits features at or below current tier', () => {
    expect(gate.canAccess('imposition', validProLicense)).toBe(true);
    expect(gate.canAccess('batch', validProLicense)).toBe(true);
  });

  it('denies features above current tier', () => {
    expect(gate.canAccess('api', validProLicense)).toBe(false);
  });

  it('denies invalid license states', () => {
    expect(gate.canAccess('imposition', { status: 'expired', payload: validProLicense.payload })).toBe(
      false
    );
  });

  it('throws when asserting denied access', () => {
    expect(() => gate.assertAccess('api', validProLicense)).toThrow(FeatureNotAvailableError);
  });

  it('lists only available features', () => {
    expect(gate.listAvailableFeatures(validProLicense)).toEqual(['imposition', 'batch']);
  });
});
