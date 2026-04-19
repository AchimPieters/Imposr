import fs from 'node:fs/promises';
import os from 'node:os';
import path from 'node:path';
import { PDFDocument } from 'pdf-lib';
import { runValidateCommand } from '../../../src/cli/commands/validate';

describe('cli/validate', () => {
  it('validates a generated pdf', async () => {
    const dir = await fs.mkdtemp(path.join(os.tmpdir(), 'imposr-cli-validate-'));
    const file = path.join(dir, 'sample.pdf');
    const doc = await PDFDocument.create();
    doc.addPage([300, 400]);
    await fs.writeFile(file, await doc.save());

    const result = await runValidateCommand({
      file,
      minPages: 1,
      maxPages: 10,
      profile: 'none',
    });

    expect(result.valid).toBe(true);
    expect(result.pageCount).toBe(1);
  });
});
