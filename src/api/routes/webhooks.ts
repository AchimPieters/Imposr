import { Router } from 'express';
import { WebhookController } from '../controllers/WebhookController';
import { getLicensingRuntime } from '@licensing/LicensingFactory';
import { requireBodyKeys } from '../middleware/validation';

const router = Router();
const controller = new WebhookController(getLicensingRuntime().paymentHandler);

router.post('/licensing', requireBodyKeys(['id', 'type', 'createdAt', 'data']), (req, res, next) => {
  void controller.handleLicenseWebhook(req, res, next);
});

router.post('/trial', requireBodyKeys(['email']), (req, res, next) => {
  void controller.issueTrial(req, res, next);
});

router.post('/paid', requireBodyKeys(['email', 'tier', 'validDays']), (req, res, next) => {
  void controller.issuePaid(req, res, next);
});

export default router;
