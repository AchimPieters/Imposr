import { LicenseError } from '../../../src/utils/errors';
import { getLicensingRuntime, resetLicensingRuntime } from '../../../src/licensing/LicensingFactory';

describe('LicensingFactory', () => {
  const originalNodeEnv = process.env.NODE_ENV;
  const originalSigningSecret = process.env.LICENSE_SIGNING_SECRET;
  const originalActivationSecret = process.env.OFFLINE_ACTIVATION_SECRET;

  afterEach(() => {
    process.env.NODE_ENV = originalNodeEnv;
    process.env.LICENSE_SIGNING_SECRET = originalSigningSecret;
    process.env.OFFLINE_ACTIVATION_SECRET = originalActivationSecret;
    resetLicensingRuntime();
  });

  it('returns cached runtime instance', () => {
    process.env.NODE_ENV = 'test';
    const first = getLicensingRuntime();
    const second = getLicensingRuntime();

    expect(first).toBe(second);
  });

  it('throws in production when secrets are missing', () => {
    process.env.NODE_ENV = 'production';
    delete process.env.LICENSE_SIGNING_SECRET;
    delete process.env.OFFLINE_ACTIVATION_SECRET;

    expect(() => getLicensingRuntime()).toThrow(LicenseError);
  });
});
