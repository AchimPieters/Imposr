import { LicenseError } from '../../../src/utils/errors';
import {
  ActivationService,
  InMemoryActivationStore,
} from '../../../src/licensing/ActivationService';
import { LicenseManager, LicensePayload } from '../../../src/licensing/LicenseManager';
import { MachineFingerprintService, MachineIdProvider } from '../../../src/licensing/MachineId';

class StaticMachineIdProvider implements MachineIdProvider {
  constructor(private readonly id: string) {}

  async getRawId(): Promise<string> {
    return this.id;
  }
}

describe('ActivationService', () => {
  const payload: LicensePayload = {
    id: 'lic_abc',
    tier: 'pro',
    issuedAt: '2026-01-01T00:00:00.000Z',
    expiresAt: '2026-12-31T00:00:00.000Z',
    customerEmail: 'pro@example.com',
    maxActivations: 5,
    features: ['imposition', 'batch'],
  };

  const manager = new LicenseManager({ signingSecret: 'license-secret-123456' });
  const licenseKey = manager.sign(payload);

  it('activates and reads activation on same machine', async () => {
    const service = new ActivationService(
      manager,
      new MachineFingerprintService(new StaticMachineIdProvider('machine-a')),
      new InMemoryActivationStore(),
      () => new Date('2026-04-19T00:00:00.000Z')
    );

    const activation = await service.activate(licenseKey);
    const readBack = await service.getActivation(licenseKey);

    expect(readBack).toEqual(activation);
  });

  it('throws when license is already activated on another machine', async () => {
    const sharedStore = new InMemoryActivationStore();

    const serviceA = new ActivationService(
      manager,
      new MachineFingerprintService(new StaticMachineIdProvider('machine-a')),
      sharedStore
    );
    const serviceB = new ActivationService(
      manager,
      new MachineFingerprintService(new StaticMachineIdProvider('machine-b')),
      sharedStore
    );

    await serviceA.activate(licenseKey);

    await expect(serviceB.activate(licenseKey)).rejects.toThrow(LicenseError);
  });

  it('deactivates successfully', async () => {
    const service = new ActivationService(
      manager,
      new MachineFingerprintService(new StaticMachineIdProvider('machine-a')),
      new InMemoryActivationStore()
    );

    await service.activate(licenseKey);
    await service.deactivate(licenseKey);

    await expect(service.getActivation(licenseKey)).resolves.toBeNull();
  });
});
