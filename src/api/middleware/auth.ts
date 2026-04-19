import { NextFunction, Request, Response } from 'express';

/** Configuration for API token auth middleware. */
export interface ApiTokenAuthOptions {
  token: string;
  exemptPaths?: string[];
}

/**
 * Creates middleware enforcing Bearer token authentication.
 * @param options Static token configuration.
 */
export function createApiTokenAuth(options: ApiTokenAuthOptions) {
  return (req: Request, res: Response, next: NextFunction): void => {
    const exemptPaths = options.exemptPaths ?? [];
    if (exemptPaths.some((pathPrefix) => req.path.startsWith(pathPrefix))) {
      next();
      return;
    }

    const authHeader = req.header('authorization');
    if (!authHeader || !authHeader.startsWith('Bearer ')) {
      res.status(401).json({ ok: false, code: 'UNAUTHORIZED', message: 'Missing Bearer token' });
      return;
    }

    const token = authHeader.slice('Bearer '.length).trim();
    if (token !== options.token) {
      res.status(401).json({ ok: false, code: 'UNAUTHORIZED', message: 'Invalid API token' });
      return;
    }

    next();
  };
}
