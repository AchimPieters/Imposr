import { createHmac } from 'node:crypto';
import fs from 'node:fs/promises';
import path from 'node:path';
import { InMemoryActivationStore, ActivationService } from '../../../src/licensing/ActivationService';
import { LicenseManager, LicensePayload } from '../../../src/licensing/LicenseManager';
import { MachineFingerprintService, MachineIdProvider } from '../../../src/licensing/MachineId';
import { PaymentHandler } from '../../../src/licensing/PaymentHandler';
import { PaddleAdapter, StripeAdapter } from '../../../src/licensing/providers/PaymentProviderAdapter';

describe('PaymentHandler', () => {
  class StaticMachineIdProvider implements MachineIdProvider {
    constructor(private readonly id: string) {}

    async getRawId(): Promise<string> {
      return this.id;
    }
  }

  const now = new Date('2026-04-19T12:00:00.000Z');
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
    now: () => now,
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
      createdAt: '2026-04-19T11:59:00.000Z',
      data: { licenseKey },
    };
    const raw = JSON.stringify(event);
    const signature = createHmac('sha256', 'payment-webhook-secret-12345').update(raw).digest('hex');

    const parsed = handler.verifyWebhook(raw, signature);
    await expect(handler.processWebhookEvent(parsed)).resolves.toBeUndefined();

    await expect(handler.processWebhookEvent(parsed)).rejects.toThrow('Webhook replay detected');
  });

  it('rejects stale webhook events', () => {
    const licenseKey = licenseManager.sign(payload);
    const event = {
      id: 'evt_old',
      type: 'license.issued',
      createdAt: '2026-04-19T10:00:00.000Z',
      data: { licenseKey },
    };

    const raw = JSON.stringify(event);
    const signature = createHmac('sha256', 'payment-webhook-secret-12345').update(raw).digest('hex');

    expect(() => handler.verifyWebhook(raw, signature)).toThrow('Webhook event is too old');
  });

  it('issues trial and paid licenses', async () => {
    const trial = await handler.issueTrialLicense('trial@example.com', 14);
    const paid = await handler.issuePaidLicense('paid@example.com', 'enterprise', 365);

    expect(licenseManager.verify(trial).status).toBe('valid');
    expect(licenseManager.verify(paid).status).toBe('valid');
  });

  it('processes stripe fixture and rejects replay', async () => {
    const licenseKey = licenseManager.sign(payload);
    const fixturePath = path.join(process.cwd(), 'tests/fixtures/webhooks/stripe-license-issued.json');
    const fixtureRaw = await fs.readFile(fixturePath, 'utf8');
    const raw = fixtureRaw.replace('__LICENSE_KEY__', licenseKey);
    const timestamp = Math.floor(now.getTime() / 1000);
    const signature = createHmac('sha256', 'payment-webhook-secret-12345')
      .update(`${timestamp}.${raw}`, 'utf8')
      .digest('hex');

    const stripeHandler = new PaymentHandler({
      webhookSecret: 'payment-webhook-secret-12345',
      licenseManager,
      activationService,
      now: () => now,
      providerAdapter: new StripeAdapter(),
      signatureToleranceMs: 60_000,
    });

    const parsed = stripeHandler.verifyWebhook(raw, `t=${timestamp},v1=${signature}`);
    await expect(stripeHandler.processWebhookEvent(parsed)).resolves.toBeUndefined();
    await expect(stripeHandler.processWebhookEvent(parsed)).rejects.toThrow('Webhook replay detected');
  });

  it('processes paddle revocation fixture', async () => {
    const licenseKey = licenseManager.sign(payload);
    await activationService.activate(licenseKey);

    const fixturePath = path.join(process.cwd(), 'tests/fixtures/webhooks/paddle-license-revoked.json');
    const fixtureRaw = await fs.readFile(fixturePath, 'utf8');
    const raw = fixtureRaw.replace('__LICENSE_KEY__', licenseKey);
    const timestamp = Math.floor(now.getTime() / 1000);
    const signature = createHmac('sha256', 'payment-webhook-secret-12345')
      .update(`${timestamp}:${raw}`, 'utf8')
      .digest('hex');

    const paddleHandler = new PaymentHandler({
      webhookSecret: 'payment-webhook-secret-12345',
      licenseManager,
      activationService,
      now: () => now,
      providerAdapter: new PaddleAdapter(),
      signatureToleranceMs: 60_000,
    });

    const parsed = paddleHandler.verifyWebhook(raw, `ts=${timestamp};h1=${signature}`);
    await expect(paddleHandler.processWebhookEvent(parsed)).resolves.toBeUndefined();
    await expect(activationService.getActivation(licenseKey)).resolves.toBeNull();
  });
});
