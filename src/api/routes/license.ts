import { Router } from 'express';
import { LicenseController } from '../controllers/LicenseController';
import { getLicensingRuntime } from '@licensing/LicensingFactory';
import { validateBodySchema } from '../middleware/validation';
import { offlineValidationBodySchema } from '../middleware/schemas';

const router = Router();

const runtime = getLicensingRuntime();
const controller = new LicenseController(
  runtime.licenseManager,
  runtime.activationService,
  runtime.offlineValidator,
  runtime.auditLogger
);

router.get('/verify', (req, res, next) => {
  void controller.verify(req, res, next);
});

router.get('/audit', (req, res, next) => {
  void controller.recentAudit(req, res, next);
});

router.post('/activate', (req, res, next) => {
  void controller.activate(req, res, next);
});

router.post('/deactivate', (req, res, next) => {
  void controller.deactivate(req, res, next);
});

router.post('/offline/validate', validateBodySchema(offlineValidationBodySchema), (req, res, next) => {
  void controller.validateOffline(req, res, next);
});

export default router;
