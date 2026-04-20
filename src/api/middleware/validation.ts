import { NextFunction, Request, Response } from 'express';
import { ZodSchema } from 'zod';

/** Build required-body-keys validator middleware. */
export function requireBodyKeys(keys: string[]) {
  return (req: Request, res: Response, next: NextFunction): void => {
    const body = req.body as Record<string, unknown> | undefined;
    if (!body) {
      res.status(400).json({ ok: false, code: 'VALIDATION_ERROR', message: 'Missing request body' });
      return;
    }

    const missing = keys.filter((key) => body[key] === undefined || body[key] === null);
    if (missing.length > 0) {
      res.status(400).json({
        ok: false,
        code: 'VALIDATION_ERROR',
        message: `Missing required fields: ${missing.join(', ')}`,
      });
      return;
    }

    next();
  };
}

/**
 * Builds a zod-powered body validator middleware.
 */
export function validateBodySchema<TBody>(schema: ZodSchema<TBody>) {
  return (req: Request, res: Response, next: NextFunction): void => {
    const parsed = schema.safeParse(req.body);
    if (!parsed.success) {
      res.status(400).json({
        ok: false,
        code: 'VALIDATION_ERROR',
        message: 'Request body validation failed',
        errors: parsed.error.issues.map((issue) => ({
          path: issue.path.join('.'),
          message: issue.message,
        })),
      });
      return;
    }

    req.body = parsed.data;
    next();
  };
}
