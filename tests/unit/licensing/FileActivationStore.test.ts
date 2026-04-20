import fs from 'node:fs/promises';
import os from 'node:os';
import path from 'node:path';
import { FileActivationStore } from '../../../src/licensing/FileActivationStore';

describe('FileActivationStore', () => {
  it('persists activation records across instances', async () => {
    const dir = await fs.mkdtemp(path.join(os.tmpdir(), 'imposr-activation-store-'));
    const filePath = path.join(dir, 'activations.json');

    const storeA = new FileActivationStore(filePath);
    await storeA.save({
      activationId: 'act_1',
      licenseId: 'lic_1',
      machineFingerprint: 'a'.repeat(64),
      activatedAt: '2026-04-19T00:00:00.000Z',
    });

    const storeB = new FileActivationStore(filePath);
    const found = await storeB.getByLicenseId('lic_1');

    expect(found?.activationId).toBe('act_1');

    await storeB.deleteByLicenseId('lic_1');
    await expect(storeB.getByLicenseId('lic_1')).resolves.toBeNull();
  });

  it('handles concurrent writes safely with file locking', async () => {
    const dir = await fs.mkdtemp(path.join(os.tmpdir(), 'imposr-activation-lock-'));
    const filePath = path.join(dir, 'activations.json');
    const store = new FileActivationStore(filePath);

    await Promise.all(
      Array.from({ length: 20 }).map((_, index) =>
        store.save({
          activationId: `act_${index}`,
          licenseId: `lic_${index}`,
          machineFingerprint: `machine_${index}`,
          activatedAt: '2026-04-20T00:00:00.000Z',
        })
      )
    );

    for (let index = 0; index < 20; index += 1) {
      await expect(store.getByLicenseId(`lic_${index}`)).resolves.toMatchObject({
        activationId: `act_${index}`,
      });
    }
  });

  it('supports encrypted-at-rest store content', async () => {
    const dir = await fs.mkdtemp(path.join(os.tmpdir(), 'imposr-activation-encrypted-'));
    const filePath = path.join(dir, 'activations.secure');
    const store = new FileActivationStore(filePath, { encryptionSecret: 'super-secret-storage-key' });

    await store.save({
      activationId: 'act_secure',
      licenseId: 'lic_secure',
      machineFingerprint: 'machine-secure',
      activatedAt: '2026-04-20T00:00:00.000Z',
    });

    const raw = await fs.readFile(filePath, 'utf8');
    expect(raw).not.toContain('lic_secure');

    await expect(store.getByLicenseId('lic_secure')).resolves.toMatchObject({
      activationId: 'act_secure',
    });
  });

  it('migrates legacy activation store shape automatically', async () => {
    const dir = await fs.mkdtemp(path.join(os.tmpdir(), 'imposr-activation-legacy-'));
    const filePath = path.join(dir, 'activations.json');
    await fs.writeFile(
      filePath,
      JSON.stringify(
        {
          lic_legacy: {
            activationId: 'act_legacy',
            machineId: 'machine-legacy',
            activatedAt: '2026-04-20T00:00:00.000Z',
          },
        },
        null,
        2
      ),
      'utf8'
    );

    const store = new FileActivationStore(filePath);
    await expect(store.getByLicenseId('lic_legacy')).resolves.toMatchObject({
      activationId: 'act_legacy',
      machineFingerprint: 'machine-legacy',
    });

    const normalized = JSON.parse(await fs.readFile(filePath, 'utf8')) as { records: unknown };
    expect(normalized).toHaveProperty('records');
  });

  it('migrates plaintext store when encryption is enabled later', async () => {
    const dir = await fs.mkdtemp(path.join(os.tmpdir(), 'imposr-activation-crypto-migrate-'));
    const filePath = path.join(dir, 'activations.json');

    const plainStore = new FileActivationStore(filePath);
    await plainStore.save({
      activationId: 'act_plain',
      licenseId: 'lic_plain',
      machineFingerprint: 'machine-plain',
      activatedAt: '2026-04-20T00:00:00.000Z',
    });

    const encryptedStore = new FileActivationStore(filePath, {
      encryptionSecret: 'encryption-secret-activated',
    });
    await expect(encryptedStore.getByLicenseId('lic_plain')).resolves.toMatchObject({
      activationId: 'act_plain',
    });

    const raw = await fs.readFile(filePath, 'utf8');
    expect(raw).not.toContain('"records"');
  });
});
