import fs from 'node:fs/promises';
import { createHmac } from 'node:crypto';
import os from 'node:os';
import path from 'node:path';
import type { AddressInfo } from 'node:net';
import { createApiApp } from '../../../src/api/server';
import { LicenseManager, LicensePayload } from '../../../src/licensing/LicenseManager';
import { OfflineValidator } from '../../../src/licensing/OfflineValidator';

describe('api/server', () => {
  const licenseManager = new LicenseManager({
    signingSecret: process.env.LICENSE_SIGNING_SECRET ?? 'dev-license-signing-secret-12345',
  });

  const validLicensePayload: LicensePayload = {
    id: 'api-license-1',
    tier: 'enterprise',
    issuedAt: '2026-01-01T00:00:00.000Z',
    expiresAt: '2026-12-31T00:00:00.000Z',
    customerEmail: 'api@example.com',
    maxActivations: 10,
    features: ['imposition', 'templates', 'batch', 'api'],
  };

  const validLicenseKey = licenseManager.sign(validLicensePayload);

  it('returns health response', async () => {
    const app = createApiApp();
    const server = app.listen(0);
    const port = (server.address() as AddressInfo).port;

    try {
      const response = await fetch(`http://127.0.0.1:${port}/health`);
      expect(response.status).toBe(200);
      expect(await response.json()).toEqual({ ok: true });
    } finally {
      await new Promise<void>((resolve) => server.close(() => resolve()));
    }
  });

  it('rejects impose request without license', async () => {
    const app = createApiApp();
    const server = app.listen(0);
    const port = (server.address() as AddressInfo).port;

    try {
      const response = await fetch(`http://127.0.0.1:${port}/api/impose`, {
        method: 'POST',
        headers: { 'content-type': 'application/json' },
        body: JSON.stringify({}),
      });
      expect(response.status).toBe(403);
    } finally {
      await new Promise<void>((resolve) => server.close(() => resolve()));
    }
  });

  it('validates impose payload with license', async () => {
    const app = createApiApp();
    const server = app.listen(0);
    const port = (server.address() as AddressInfo).port;

    try {
      const response = await fetch(`http://127.0.0.1:${port}/api/impose`, {
        method: 'POST',
        headers: {
          'content-type': 'application/json',
          'x-license-key': validLicenseKey,
        },
        body: JSON.stringify({}),
      });
      expect(response.status).toBe(400);
    } finally {
      await new Promise<void>((resolve) => server.close(() => resolve()));
    }
  });

  it('saves and lists templates via API with license', async () => {
    const app = createApiApp();
    const server = app.listen(0);
    const port = (server.address() as AddressInfo).port;
    const dir = await fs.mkdtemp(path.join(os.tmpdir(), 'imposr-api-templates-'));

    try {
      const saveResponse = await fetch(`http://127.0.0.1:${port}/api/templates`, {
        method: 'POST',
        headers: {
          'content-type': 'application/json',
          'x-license-key': validLicenseKey,
        },
        body: JSON.stringify({
          directory: dir,
          template: {
            id: 'api-template-1',
            name: 'API Template',
            mode: 'sequential',
            layout: {
              columns: 2,
              rows: 1,
              gap: 0,
              margin: 0,
              sheetWidth: 500,
              sheetHeight: 700,
            },
          },
        }),
      });
      expect(saveResponse.status).toBe(201);

      const listResponse = await fetch(
        `http://127.0.0.1:${port}/api/templates?directory=${encodeURIComponent(dir)}`,
        {
          headers: { 'x-license-key': validLicenseKey },
        }
      );
      expect(listResponse.status).toBe(200);
      const listPayload = (await listResponse.json()) as { files: string[] };
      expect(listPayload.files).toContain('api-template-1.json');
    } finally {
      await new Promise<void>((resolve) => server.close(() => resolve()));
    }
  });

  it('validates offline token via license endpoint', async () => {
    const app = createApiApp();
    const server = app.listen(0);
    const port = (server.address() as AddressInfo).port;

    try {
      const validator = new OfflineValidator({
        activationSecret: process.env.OFFLINE_ACTIVATION_SECRET ?? 'dev-offline-secret-123456',
        licenseManager,
      });

      const machineFingerprint = 'a'.repeat(64);
      const token = validator.signToken({
        licenseId: validLicensePayload.id,
        machineFingerprint,
        activatedAt: '2026-04-01T00:00:00.000Z',
        expiresAt: '2026-10-01T00:00:00.000Z',
      });

      const response = await fetch(`http://127.0.0.1:${port}/api/license/offline/validate`, {
        method: 'POST',
        headers: {
          'content-type': 'application/json',
          'x-license-key': validLicenseKey,
        },
        body: JSON.stringify({
          offlineToken: token,
          machineFingerprint,
        }),
      });

      expect(response.status).toBe(200);
      const payload = (await response.json()) as { ok: boolean };
      expect(payload.ok).toBe(true);
    } finally {
      await new Promise<void>((resolve) => server.close(() => resolve()));
    }
  });

  it('accepts signed licensing webhook', async () => {
    const app = createApiApp();
    const server = app.listen(0);
    const port = (server.address() as AddressInfo).port;

    try {
      const event = {
        id: 'evt_api_1',
        type: 'license.issued',
        createdAt: new Date().toISOString(),
        data: {
          licenseKey: validLicenseKey,
        },
      };

      const rawBody = JSON.stringify(event);
      const signature = createHmac('sha256', process.env.PAYMENT_WEBHOOK_SECRET ?? 'dev-payment-webhook-secret-1234').update(rawBody, 'utf8').digest('hex');

      const response = await fetch(`http://127.0.0.1:${port}/api/webhooks/licensing`, {
        method: 'POST',
        headers: {
          'content-type': 'application/json',
          'x-webhook-signature': signature,
        },
        body: rawBody,
      });

      expect(response.status).toBe(202);
      const payload = (await response.json()) as { ok: boolean };
      expect(payload.ok).toBe(true);
    } finally {
      await new Promise<void>((resolve) => server.close(() => resolve()));
    }
  });

  it('issues trial license via webhook controller', async () => {
    const app = createApiApp();
    const server = app.listen(0);
    const port = (server.address() as AddressInfo).port;

    try {
      const response = await fetch(`http://127.0.0.1:${port}/api/webhooks/trial`, {
        method: 'POST',
        headers: { 'content-type': 'application/json' },
        body: JSON.stringify({ email: 'trial-api@example.com', validDays: 10 }),
      });

      expect(response.status).toBe(201);
      const payload = (await response.json()) as { ok: boolean; licenseKey: string };
      expect(payload.ok).toBe(true);
      expect(licenseManager.verify(payload.licenseKey).status).toBe('valid');
    } finally {
      await new Promise<void>((resolve) => server.close(() => resolve()));
    }
  });

  it('returns recent license audit events', async () => {
    const app = createApiApp();
    const server = app.listen(0);
    const port = (server.address() as AddressInfo).port;

    try {
      const response = await fetch(`http://127.0.0.1:${port}/api/license/audit?limit=10`, {
        headers: { 'x-license-key': validLicenseKey },
      });

      expect(response.status).toBe(200);
      const payload = (await response.json()) as { ok: boolean; events: unknown[] };
      expect(payload.ok).toBe(true);
      expect(Array.isArray(payload.events)).toBe(true);
    } finally {
      await new Promise<void>((resolve) => server.close(() => resolve()));
    }
  });


  it('rejects invalid paid issuance payload', async () => {
    const app = createApiApp();
    const server = app.listen(0);
    const port = (server.address() as AddressInfo).port;

    try {
      const response = await fetch(`http://127.0.0.1:${port}/api/webhooks/paid`, {
        method: 'POST',
        headers: { 'content-type': 'application/json' },
        body: JSON.stringify({ email: 'invalid-email', tier: 'pro', validDays: 10 }),
      });

      expect(response.status).toBe(400);
      const payload = (await response.json()) as { ok: boolean; code: string };
      expect(payload.ok).toBe(false);
      expect(payload.code).toBe('VALIDATION_ERROR');
    } finally {
      await new Promise<void>((resolve) => server.close(() => resolve()));
    }
  });

});
