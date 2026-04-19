import fs from 'node:fs/promises';
import os from 'node:os';
import path from 'node:path';
import { runImposeCommand } from '../../../src/cli/commands/impose';

describe('cli/impose', () => {
  it('writes plan output JSON file', async () => {
    const dir = await fs.mkdtemp(path.join(os.tmpdir(), 'imposr-cli-impose-'));
    const output = path.join(dir, 'plan.json');

    const plan = await runImposeCommand({
      pages: 4,
      mode: 'sequential',
      columns: 2,
      rows: 1,
      sheetWidth: 600,
      sheetHeight: 800,
      output,
    });

    expect(plan.length).toBeGreaterThan(0);
    const persisted = JSON.parse(await fs.readFile(output, 'utf8')) as unknown[];
    expect(Array.isArray(persisted)).toBe(true);
    expect(persisted.length).toBe(plan.length);
  });
});
