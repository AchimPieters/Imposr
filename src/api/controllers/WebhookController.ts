import { NextFunction, Request, Response } from 'express';
import { PaymentHandler } from '@licensing/PaymentHandler';

/** API controller for payment/licensing webhooks. */
export class WebhookController {
  /**
   * @param paymentHandler Payment webhook processor.
   */
  constructor(private readonly paymentHandler: PaymentHandler) {}

  /**
   * Verifies webhook signature and processes event side effects.
   */
  async handleLicenseWebhook(req: Request, res: Response, next: NextFunction): Promise<void> {
    try {
      const signature = req.header('x-webhook-signature') ?? '';
      const rawBody = JSON.stringify(req.body ?? {});
      const event = this.paymentHandler.verifyWebhook(rawBody, signature);
      await this.paymentHandler.processWebhookEvent(event);
      res.status(202).json({ ok: true, eventId: event.id, type: event.type });
    } catch (error) {
      next(error);
    }
  }

  /**
   * Creates signed trial license for self-serve onboarding.
   */
  async issueTrial(req: Request, res: Response, next: NextFunction): Promise<void> {
    try {
      const body = req.body as { email?: string; validDays?: number };
      const licenseKey = await this.paymentHandler.issueTrialLicense(body.email ?? '', body.validDays ?? 14);
      res.status(201).json({ ok: true, licenseKey });
    } catch (error) {
      next(error);
    }
  }

  /**
   * Creates signed paid license key from back-office/admin API.
   */
  async issuePaid(req: Request, res: Response, next: NextFunction): Promise<void> {
    try {
      const body = req.body as {
        email?: string;
        tier?: 'starter' | 'pro' | 'enterprise';
        validDays?: number;
      };

      const licenseKey = await this.paymentHandler.issuePaidLicense(
        body.email ?? '',
        body.tier ?? 'starter',
        body.validDays ?? 365
      );

      res.status(201).json({ ok: true, licenseKey });
    } catch (error) {
      next(error);
    }
  }
}
