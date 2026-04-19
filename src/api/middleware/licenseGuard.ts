import { NextFunction, Request, Response } from 'express';
import { FeatureGate } from '@licensing/FeatureGate';
import { LicenseManager } from '@licensing/LicenseManager';

/** Builds middleware that validates license key and optional feature access. */
export function createLicenseGuard(licenseManager: LicenseManager, featureGate: FeatureGate) {
  return (requiredFeature?: string) => {
    return (req: Request, res: Response, next: NextFunction): void => {
      const licenseKey = req.header('x-license-key');
      const result = licenseManager.verify(licenseKey);

      if (result.status !== 'valid' || !result.payload) {
        res.status(403).json({
          ok: false,
          code: 'LICENSE_INVALID',
          message: result.reason ?? `License status: ${result.status}`,
          status: result.status,
        });
        return;
      }

      if (requiredFeature && !featureGate.canAccess(requiredFeature, result)) {
        res.status(403).json({
          ok: false,
          code: 'FEATURE_FORBIDDEN',
          message: `Feature "${requiredFeature}" is not available for this license`,
        });
        return;
      }

      req.licenseInfo = result;
      next();
    };
  };
}

declare global {
  namespace Express {
    interface Request {
      licenseInfo?: ReturnType<LicenseManager['verify']>;
    }
  }
}
