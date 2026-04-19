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
});
