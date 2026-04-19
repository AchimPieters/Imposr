import fs from 'node:fs/promises';
import os from 'node:os';
import path from 'node:path';
import { FileLicenseAuditLogger } from '../../../src/licensing/LicenseAuditLogger';

describe('FileLicenseAuditLogger', () => {
  it('appends and reads recent events', async () => {
    const dir = await fs.mkdtemp(path.join(os.tmpdir(), 'imposr-audit-log-'));
    const filePath = path.join(dir, 'audit.ndjson');
    const logger = new FileLicenseAuditLogger(filePath);

    await logger.log({
      eventType: 'license.verify',
      timestamp: '2026-04-19T00:00:00.000Z',
      success: true,
      details: { status: 'valid' },
    });

    await logger.log({
      eventType: 'license.activate',
      timestamp: '2026-04-19T01:00:00.000Z',
      success: true,
      details: { licenseId: 'lic_1' },
    });

    const recent = await logger.readRecent(2);

    expect(recent).toHaveLength(2);
    expect(recent[0].eventType).toBe('license.activate');
    expect(recent[1].eventType).toBe('license.verify');
  });
});
