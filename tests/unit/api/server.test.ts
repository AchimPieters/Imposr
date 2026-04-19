import fs from 'node:fs/promises';
import os from 'node:os';
import path from 'node:path';
import type { AddressInfo } from 'node:net';
import { createApiApp } from '../../../src/api/server';

describe('api/server', () => {
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

  it('validates impose payload', async () => {
    const app = createApiApp();
    const server = app.listen(0);
    const port = (server.address() as AddressInfo).port;

    try {
      const response = await fetch(`http://127.0.0.1:${port}/api/impose`, {
        method: 'POST',
        headers: { 'content-type': 'application/json' },
        body: JSON.stringify({}),
      });
      expect(response.status).toBe(400);
    } finally {
      await new Promise<void>((resolve) => server.close(() => resolve()));
    }
  });

  it('saves and lists templates via API', async () => {
    const app = createApiApp();
    const server = app.listen(0);
    const port = (server.address() as AddressInfo).port;
    const dir = await fs.mkdtemp(path.join(os.tmpdir(), 'imposr-api-templates-'));

    try {
      const saveResponse = await fetch(`http://127.0.0.1:${port}/api/templates`, {
        method: 'POST',
        headers: { 'content-type': 'application/json' },
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
        `http://127.0.0.1:${port}/api/templates?directory=${encodeURIComponent(dir)}`
      );
      expect(listResponse.status).toBe(200);
      const listPayload = (await listResponse.json()) as { files: string[] };
      expect(listPayload.files).toContain('api-template-1.json');
    } finally {
      await new Promise<void>((resolve) => server.close(() => resolve()));
    }
  });
});
