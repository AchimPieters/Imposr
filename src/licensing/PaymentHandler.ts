import { LicenseError, NetworkError } from '@utils/errors';
import { ActivationService } from './ActivationService';
import { LicenseManager, LicensePayload, LicenseTier } from './LicenseManager';
import { LicenseAuditLogger, NoopLicenseAuditLogger } from './LicenseAuditLogger';
import { InMemoryWebhookReplayStore, WebhookReplayStore } from './WebhookReplayStore';
import { GenericHmacAdapter, PaymentProviderAdapter } from './providers/PaymentProviderAdapter';

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
  replayStore?: WebhookReplayStore;
  now?: () => Date;
  maxEventAgeMs?: number;
  signatureToleranceMs?: number;
  providerAdapter?: PaymentProviderAdapter;
}

/**
 * Handles payment-provider webhook verification and billing event processing.
 */
export class PaymentHandler {
  private readonly webhookSecret: string;
  private readonly licenseManager: LicenseManager;
  private readonly activationService: ActivationService;
  private readonly auditLogger: LicenseAuditLogger;
  private readonly replayStore: WebhookReplayStore;
  private readonly now: () => Date;
  private readonly maxEventAgeMs: number;
  private readonly signatureToleranceMs: number;
  private readonly providerAdapter: PaymentProviderAdapter;

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
    this.replayStore = options.replayStore ?? new InMemoryWebhookReplayStore();
    this.now = options.now ?? (() => new Date());
    this.maxEventAgeMs = options.maxEventAgeMs ?? 10 * 60 * 1000;
    this.signatureToleranceMs = options.signatureToleranceMs ?? 5 * 60 * 1000;
    this.providerAdapter = options.providerAdapter ?? new GenericHmacAdapter();
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

    const now = this.now();
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
    const event = this.providerAdapter.verifyAndParse(
      rawBody,
      signature,
      this.webhookSecret,
      this.now,
      this.signatureToleranceMs
    );

    this.validateEvent(event);
    return event;
  }

  /**
   * Applies webhook side effects to local licensing state.
   * @param event Verified webhook event.
   */
  async processWebhookEvent(event: PaymentWebhookEvent): Promise<void> {
    this.validateEvent(event);

    if (await this.replayStore.has(event.id)) {
      throw new NetworkError('Webhook replay detected', { eventId: event.id });
    }

    if (event.type === 'license.revoked' || event.type === 'subscription.canceled') {
      await this.activationService.deactivate(event.data.licenseKey);
    } else {
      this.licenseManager.assertValid(event.data.licenseKey);
    }

    await this.replayStore.mark(event.id);
    await this.auditLogger.log({
      eventType: 'payment.webhook',
      timestamp: this.now().toISOString(),
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

    const now = this.now();
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

    const ageMs = this.now().getTime() - createdAt.getTime();
    if (ageMs > this.maxEventAgeMs) {
      throw new NetworkError('Webhook event is too old', { ageMs, maxEventAgeMs: this.maxEventAgeMs });
    }
  }
}
