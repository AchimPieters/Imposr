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

  it('handles concurrent log writes safely', async () => {
    const dir = await fs.mkdtemp(path.join(os.tmpdir(), 'imposr-audit-lock-'));
    const filePath = path.join(dir, 'audit.ndjson');
    const logger = new FileLicenseAuditLogger(filePath);

    await Promise.all(
      Array.from({ length: 25 }).map((_, index) =>
        logger.log({
          eventType: 'payment.webhook',
          timestamp: `2026-04-20T00:00:${index.toString().padStart(2, '0')}.000Z`,
          success: true,
          details: { eventId: `evt_${index}` },
        })
      )
    );

    const recent = await logger.readRecent(25);
    expect(recent).toHaveLength(25);
  });

  it('supports encrypted-at-rest audit logs', async () => {
    const dir = await fs.mkdtemp(path.join(os.tmpdir(), 'imposr-audit-encrypted-'));
    const filePath = path.join(dir, 'audit.secure');
    const logger = new FileLicenseAuditLogger(filePath, {
      encryptionSecret: 'super-secret-audit-key',
    });

    await logger.log({
      eventType: 'license.verify',
      timestamp: '2026-04-20T00:00:00.000Z',
      success: true,
      details: { licenseId: 'lic_secure' },
    });

    const raw = await fs.readFile(filePath, 'utf8');
    expect(raw).not.toContain('lic_secure');

    const recent = await logger.readRecent(1);
    expect(recent[0].eventType).toBe('license.verify');
  });

  it('migrates legacy JSON-array logs on read', async () => {
    const dir = await fs.mkdtemp(path.join(os.tmpdir(), 'imposr-audit-legacy-'));
    const filePath = path.join(dir, 'audit.ndjson');
    await fs.writeFile(
      filePath,
      JSON.stringify(
        [
          {
            eventType: 'license.activate',
            timestamp: '2026-04-20T00:00:00.000Z',
            success: true,
            details: { licenseId: 'lic_legacy' },
          },
        ],
        null,
        2
      ),
      'utf8'
    );

    const logger = new FileLicenseAuditLogger(filePath);
    const recent = await logger.readRecent(1);
    expect(recent[0].eventType).toBe('license.activate');

    const normalized = await fs.readFile(filePath, 'utf8');
    expect(normalized.trim().startsWith('{')).toBe(true);
  });

  it('migrates plaintext logs when encryption is enabled later', async () => {
    const dir = await fs.mkdtemp(path.join(os.tmpdir(), 'imposr-audit-crypto-migrate-'));
    const filePath = path.join(dir, 'audit.log');

    const plainLogger = new FileLicenseAuditLogger(filePath);
    await plainLogger.log({
      eventType: 'license.verify',
      timestamp: '2026-04-20T00:00:00.000Z',
      success: true,
      details: { licenseId: 'lic_plain' },
    });

    const encryptedLogger = new FileLicenseAuditLogger(filePath, {
      encryptionSecret: 'encryption-secret-audit',
    });
    const recent = await encryptedLogger.readRecent(1);
    expect(recent[0].eventType).toBe('license.verify');

    const raw = await fs.readFile(filePath, 'utf8');
    expect(raw).not.toContain('license.verify');
  });
});
