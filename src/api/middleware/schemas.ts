import { z } from 'zod';

/** Schema for offline validation endpoint body. */
export const offlineValidationBodySchema = z.object({
  offlineToken: z.string().min(10),
  machineFingerprint: z.string().regex(/^[a-f0-9]{64}$/i),
});

/** Schema for trial issuance body. */
export const trialIssueBodySchema = z.object({
  email: z.string().email(),
  validDays: z.number().int().min(1).max(90).optional(),
});

/** Schema for paid issuance body. */
export const paidIssueBodySchema = z.object({
  email: z.string().email(),
  tier: z.enum(['starter', 'pro', 'enterprise']),
  validDays: z.number().int().min(30).max(3650),
});

/** Schema for licensing webhook event body. */
export const licensingWebhookBodySchema = z.object({
  id: z.string().min(1),
  type: z.enum(['license.issued', 'license.renewed', 'license.revoked', 'subscription.canceled']),
  createdAt: z.string().datetime(),
  data: z.object({
    licenseKey: z.string().min(10),
  }),
});
