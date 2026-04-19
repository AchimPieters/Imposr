import { NextFunction, Request, Response } from 'express';
import { ImposrError } from '@utils/errors';

/** Express error middleware for API errors. */
export function errorHandler(error: unknown, _req: Request, res: Response, _next: NextFunction): void {
  if (error instanceof ImposrError) {
    res.status(400).json({
      ok: false,
      code: error.code,
      message: error.message,
      details: error.details,
    });
    return;
  }

  const message = error instanceof Error ? error.message : 'Unexpected server error';
  res.status(500).json({
    ok: false,
    code: 'INTERNAL_ERROR',
    message,
  });
}
