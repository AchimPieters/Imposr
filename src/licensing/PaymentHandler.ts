import { createHmac, timingSafeEqual } from 'node:crypto';
import { LicenseError, NetworkError } from '@utils/errors';
import { ActivationService } from './ActivationService';
import { LicenseManager, LicensePayload, LicenseTier } from './LicenseManager';
import { LicenseAuditLogger, NoopLicenseAuditLogger } from './LicenseAuditLogger';

/** Supported webhook event names for licensing/billing sync. */
export type PaymentWebhookEventType =
  | 'license.issued'
  | 'license.renewed'
  | 'license.revoked'
  | 'subscription.canceled';

/** Payload structure accepted from payment provider webhooks. */
export interface PaymentWebhookEvent {
  id: string;
  type: PaymentWebhookEventType;
  createdAt: string;
  data: {
    licenseKey: string;
  };
}

/** Options for payment webhook verification/processing. */
export interface PaymentHandlerOptions {
  webhookSecret: string;
  licenseManager: LicenseManager;
  activationService: ActivationService;
  auditLogger?: LicenseAuditLogger;
}

/**
 * Handles payment-provider webhook verification and billing event processing.
 */
export class PaymentHandler {
  private readonly webhookSecret: string;
  private readonly licenseManager: LicenseManager;
  private readonly activationService: ActivationService;
  private readonly auditLogger: LicenseAuditLogger;

  /**
   * @param options Dependencies and webhook signing secret.
   */
  constructor(options: PaymentHandlerOptions) {
    if (!options.webhookSecret || options.webhookSecret.trim().length < 16) {
      throw new LicenseError('Webhook secret must be provided and at least 16 characters long');
    }

    this.webhookSecret = options.webhookSecret;
    this.licenseManager = options.licenseManager;
    this.activationService = options.activationService;
    this.auditLogger = options.auditLogger ?? new NoopLicenseAuditLogger();
  }

  /**
   * Generates a signed trial license key for onboarding.
   * @param email Customer email.
   * @param validDays Trial duration in days.
   */
  async issueTrialLicense(email: string, validDays = 14): Promise<string> {
    if (!email || !email.includes('@')) {
      throw new LicenseError('Valid customer email is required to issue trial license');
    }

    if (!Number.isInteger(validDays) || validDays <= 0 || validDays > 90) {
      throw new LicenseError('Trial duration must be an integer between 1 and 90 days');
    }

    const now = new Date();
    const expiresAt = new Date(now.getTime() + validDays * 24 * 60 * 60 * 1000);

    const payload: LicensePayload = {
      id: `trial_${now.getTime()}`,
      tier: 'trial',
      issuedAt: now.toISOString(),
      expiresAt: expiresAt.toISOString(),
      customerEmail: email,
      maxActivations: 1,
      features: ['imposition'],
    };

    const licenseKey = this.licenseManager.sign(payload);
    await this.auditLogger.log({
      eventType: 'license.issue.trial',
      timestamp: now.toISOString(),
      success: true,
      details: { licenseId: payload.id, email },
    });

    return licenseKey;
  }

  /**
   * Verifies raw webhook payload and returns parsed event.
   * @param rawBody Raw JSON payload string.
   * @param signature Hex signature from provider header.
   */
  verifyWebhook(rawBody: string, signature: string): PaymentWebhookEvent {
    if (!signature || !/^[a-f0-9]{64}$/i.test(signature)) {
      throw new NetworkError('Webhook signature format is invalid');
    }

    const expectedSignature = createHmac('sha256', this.webhookSecret).update(rawBody, 'utf8').digest('hex');
    const expectedBuffer = Buffer.from(expectedSignature, 'hex');
    const receivedBuffer = Buffer.from(signature, 'hex');

    if (!timingSafeEqual(expectedBuffer, receivedBuffer)) {
      throw new NetworkError('Webhook signature mismatch');
    }

    let event: PaymentWebhookEvent;
    try {
      event = JSON.parse(rawBody) as PaymentWebhookEvent;
    } catch (error) {
      throw new NetworkError('Webhook payload is not valid JSON', {
        cause: error instanceof Error ? error.message : 'unknown',
      });
    }

    this.validateEvent(event);
    return event;
  }

  /**
   * Applies webhook side effects to local licensing state.
   * @param event Verified webhook event.
   */
  async processWebhookEvent(event: PaymentWebhookEvent): Promise<void> {
    this.validateEvent(event);

    if (event.type === 'license.revoked' || event.type === 'subscription.canceled') {
      await this.activationService.deactivate(event.data.licenseKey);
    } else {
      // issued/renewed must at least be valid licenses; activation remains user-driven.
      this.licenseManager.assertValid(event.data.licenseKey);
    }

    await this.auditLogger.log({
      eventType: 'payment.webhook',
      timestamp: new Date().toISOString(),
      success: true,
      details: { eventId: event.id, type: event.type },
    });
  }

  /**
   * Creates signed license key for paid tiers from checkout response data.
   */
  async issuePaidLicense(email: string, tier: Exclude<LicenseTier, 'trial'>, validDays: number): Promise<string> {
    if (!email || !email.includes('@')) {
      throw new LicenseError('Valid customer email is required to issue paid license');
    }

    if (!Number.isInteger(validDays) || validDays < 30 || validDays > 3650) {
      throw new LicenseError('Paid license duration must be between 30 and 3650 days');
    }

    const featuresByTier: Record<Exclude<LicenseTier, 'trial'>, string[]> = {
      starter: ['imposition', 'templates'],
      pro: ['imposition', 'templates', 'batch'],
      enterprise: ['imposition', 'templates', 'batch', 'api'],
    };

    const now = new Date();
    const expiresAt = new Date(now.getTime() + validDays * 24 * 60 * 60 * 1000);

    const key = this.licenseManager.sign({
      id: `${tier}_${now.getTime()}`,
      tier,
      issuedAt: now.toISOString(),
      expiresAt: expiresAt.toISOString(),
      customerEmail: email,
      maxActivations: tier === 'starter' ? 1 : tier === 'pro' ? 3 : 25,
      features: featuresByTier[tier],
    });

    await this.auditLogger.log({
      eventType: 'license.issue.paid',
      timestamp: now.toISOString(),
      success: true,
      details: { email, tier },
    });

    return key;
  }

  private validateEvent(event: PaymentWebhookEvent): void {
    if (!event.id || event.id.trim().length === 0) {
      throw new NetworkError('Webhook event id is required');
    }

    if (
      !event.type ||
      !['license.issued', 'license.renewed', 'license.revoked', 'subscription.canceled'].includes(event.type)
    ) {
      throw new NetworkError('Webhook event type is invalid', { type: event.type });
    }

    if (!event.data?.licenseKey) {
      throw new NetworkError('Webhook event data.licenseKey is required');
    }

    const createdAt = new Date(event.createdAt);
    if (Number.isNaN(createdAt.getTime())) {
      throw new NetworkError('Webhook event createdAt is invalid');
    }
  }
}
