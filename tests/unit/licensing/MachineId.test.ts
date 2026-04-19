import { createHash } from 'node:crypto';
import { LicenseError } from '../../../src/utils/errors';
import { MachineFingerprintService, MachineIdProvider } from '../../../src/licensing/MachineId';

class StaticProvider implements MachineIdProvider {
  constructor(private readonly rawValue: string) {}

  async getRawId(): Promise<string> {
    return this.rawValue;
  }
}

describe('MachineFingerprintService', () => {
  it('returns deterministic sha256 hash', async () => {
    const service = new MachineFingerprintService(new StaticProvider('machine-001'));

    const fingerprint = await service.getFingerprint();

    expect(fingerprint).toBe(createHash('sha256').update('machine-001', 'utf8').digest('hex'));
  });

  it('throws when provider returns empty id', async () => {
    const service = new MachineFingerprintService(new StaticProvider(''));
    await expect(service.getFingerprint()).rejects.toThrow(LicenseError);
  });
});
