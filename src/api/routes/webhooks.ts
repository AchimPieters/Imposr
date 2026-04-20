import { Router } from 'express';
import { WebhookController } from '../controllers/WebhookController';
import { getLicensingRuntime } from '@licensing/LicensingFactory';
import { validateBodySchema } from '../middleware/validation';
import {
  licensingWebhookBodySchema,
  paidIssueBodySchema,
  trialIssueBodySchema,
} from '../middleware/schemas';

const router = Router();
const controller = new WebhookController(getLicensingRuntime().paymentHandler);

router.post('/licensing', validateBodySchema(licensingWebhookBodySchema), (req, res, next) => {
  void controller.handleLicenseWebhook(req, res, next);
});

router.post('/trial', validateBodySchema(trialIssueBodySchema), (req, res, next) => {
  void controller.issueTrial(req, res, next);
});

router.post('/paid', validateBodySchema(paidIssueBodySchema), (req, res, next) => {
  void controller.issuePaid(req, res, next);
});

export default router;
