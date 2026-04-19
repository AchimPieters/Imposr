import { NextFunction, Request, Response } from 'express';

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
