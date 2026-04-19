import { NextFunction, Request, Response } from 'express';
import { ActivationService } from '@licensing/ActivationService';
import { OfflineValidator } from '@licensing/OfflineValidator';
import { LicenseManager } from '@licensing/LicenseManager';
import { LicenseAuditLogger } from '@licensing/LicenseAuditLogger';

/** API controller for commercial licensing endpoints. */
export class LicenseController {
  /**
   * @param licenseManager License parser/validator.
   * @param activationService Activation lifecycle service.
   * @param offlineValidator Offline token validator.
   * @param auditLogger Audit logger/read model.
   */
  constructor(
    private readonly licenseManager: LicenseManager,
    private readonly activationService: ActivationService,
    private readonly offlineValidator: OfflineValidator,
    private readonly auditLogger: LicenseAuditLogger
  ) {}

  /** Returns parsed license information. */
  async verify(req: Request, res: Response, next: NextFunction): Promise<void> {
    try {
      const licenseKey = req.header('x-license-key') ?? '';
      const result = this.licenseManager.verify(licenseKey);
      await this.auditLogger.log({
        eventType: 'license.verify',
        timestamp: new Date().toISOString(),
        success: result.status === 'valid',
        details: { status: result.status },
      });
      res.status(result.status === 'valid' ? 200 : 422).json({ ok: result.status === 'valid', result });
    } catch (error) {
      next(error);
    }
  }

  /** Activates current machine for provided license key. */
  async activate(req: Request, res: Response, next: NextFunction): Promise<void> {
    try {
      const licenseKey = req.header('x-license-key') ?? '';
      const activation = await this.activationService.activate(licenseKey);
      res.status(201).json({ ok: true, activation });
    } catch (error) {
      next(error);
    }
  }

  /** Deactivates current machine for provided license key. */
  async deactivate(req: Request, res: Response, next: NextFunction): Promise<void> {
    try {
      const licenseKey = req.header('x-license-key') ?? '';
      await this.activationService.deactivate(licenseKey);
      res.status(204).send();
    } catch (error) {
      next(error);
    }
  }

  /** Validates offline activation token for current machine fingerprint. */
  async validateOffline(req: Request, res: Response, next: NextFunction): Promise<void> {
    try {
      const licenseKey = req.header('x-license-key') ?? '';
      const body = req.body as { offlineToken?: string; machineFingerprint?: string };
      const result = this.offlineValidator.validate(
        licenseKey,
        body.offlineToken ?? '',
        body.machineFingerprint ?? ''
      );

      await this.auditLogger.log({
        eventType: 'offline.validate',
        timestamp: new Date().toISOString(),
        success: result.valid,
        details: { reason: result.reason ?? null },
      });

      res.status(result.valid ? 200 : 422).json({ ok: result.valid, result });
    } catch (error) {
      next(error);
    }
  }

  /** Returns recent licensing audit events for support/compliance. */
  async recentAudit(req: Request, res: Response, next: NextFunction): Promise<void> {
    try {
      const limit = Number(req.query.limit ?? 50);
      const events = await this.auditLogger.readRecent(limit);
      res.status(200).json({ ok: true, events });
    } catch (error) {
      next(error);
    }
  }
}
