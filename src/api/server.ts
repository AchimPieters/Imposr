import express, { Express } from 'express';
import cors from 'cors';
import helmet from 'helmet';
import rateLimit from 'express-rate-limit';
import imposeRoutes from './routes/impose';
import templateRoutes from './routes/templates';
import jobRoutes from './routes/jobs';
import licenseRoutes from './routes/license';
import webhookRoutes from './routes/webhooks';
import { errorHandler } from './middleware/errorHandler';
import { requireBodyKeys } from './middleware/validation';
import { createApiTokenAuth } from './middleware/auth';
import { createLicenseGuard } from './middleware/licenseGuard';
import { getLicensingRuntime } from '@licensing/LicensingFactory';

/** Build configured Express app instance. */
export function createApiApp(): Express {
  const app = express();
  app.use(helmet());
  app.use(cors());
  app.use(express.json({ limit: '2mb' }));
  app.use(
    rateLimit({
      windowMs: 60_000,
      max: 120,
    })
  );

  const apiToken = process.env.API_AUTH_TOKEN;
  if (apiToken) {
    app.use(createApiTokenAuth({ token: apiToken, exemptPaths: ['/api/webhooks'] }));
  }

  const runtime = getLicensingRuntime();
  const licenseGuard = createLicenseGuard(runtime.licenseManager, runtime.featureGate);

  app.get('/health', (_req, res) => {
    res.status(200).json({ ok: true });
  });

  app.use(
    '/api/impose',
    licenseGuard('imposition'),
    requireBodyKeys(['pages', 'mode', 'columns', 'rows', 'sheetWidth', 'sheetHeight', 'output']),
    imposeRoutes
  );
  app.use('/api/templates', licenseGuard('templates'), templateRoutes);
  app.use('/api/jobs', licenseGuard('batch'), jobRoutes);
  app.use('/api/license', licenseRoutes);
  app.use('/api/webhooks', webhookRoutes);
  app.use(errorHandler);

  return app;
}

if (require.main === module) {
  const port = Number(process.env.PORT ?? 4000);
  createApiApp().listen(port, () => {
    // eslint-disable-next-line no-console
    console.log(`Imposr API listening on :${port}`);
  });
}
