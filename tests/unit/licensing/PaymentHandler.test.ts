import { createHmac } from 'node:crypto';
import { InMemoryActivationStore, ActivationService } from '../../../src/licensing/ActivationService';
import { LicenseManager, LicensePayload } from '../../../src/licensing/LicenseManager';
import { MachineFingerprintService, MachineIdProvider } from '../../../src/licensing/MachineId';
import { PaymentHandler } from '../../../src/licensing/PaymentHandler';

class StaticMachineIdProvider implements MachineIdProvider {
  constructor(private readonly id: string) {}

  async getRawId(): Promise<string> {
    return this.id;
  }
}

describe('PaymentHandler', () => {
  const licenseManager = new LicenseManager({ signingSecret: 'license-secret-123456' });
  const activationService = new ActivationService(
    licenseManager,
    new MachineFingerprintService(new StaticMachineIdProvider('machine-a')),
    new InMemoryActivationStore()
  );

  const handler = new PaymentHandler({
    webhookSecret: 'payment-webhook-secret-12345',
    licenseManager,
    activationService,
  });

  const payload: LicensePayload = {
    id: 'lic_pay_1',
    tier: 'pro',
    issuedAt: '2026-01-01T00:00:00.000Z',
    expiresAt: '2026-12-31T00:00:00.000Z',
    customerEmail: 'pay@example.com',
    maxActivations: 5,
    features: ['imposition', 'templates', 'batch'],
  };

  it('verifies and processes issued event', async () => {
    const licenseKey = licenseManager.sign(payload);
    const event = {
      id: 'evt_1',
      type: 'license.issued',
      createdAt: '2026-04-19T00:00:00.000Z',
      data: { licenseKey },
    };
    const raw = JSON.stringify(event);
    const signature = createHmac('sha256', 'payment-webhook-secret-12345').update(raw).digest('hex');

    const parsed = handler.verifyWebhook(raw, signature);
    await expect(handler.processWebhookEvent(parsed)).resolves.toBeUndefined();
  });

  it('issues trial and paid licenses', async () => {
    const trial = await handler.issueTrialLicense('trial@example.com', 14);
    const paid = await handler.issuePaidLicense('paid@example.com', 'enterprise', 365);

    expect(licenseManager.verify(trial).status).toBe('valid');
    expect(licenseManager.verify(paid).status).toBe('valid');
  });
});
