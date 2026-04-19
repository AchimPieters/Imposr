import fs from 'node:fs/promises';
import os from 'node:os';
import path from 'node:path';
import {
  runTemplateListCommand,
  runTemplateLoadCommand,
  runTemplateSaveCommand,
} from '../../../src/cli/commands/templates';

describe('cli/templates', () => {
  it('saves, lists and loads templates', async () => {
    const dir = await fs.mkdtemp(path.join(os.tmpdir(), 'imposr-cli-templates-'));
    const fileName = await runTemplateSaveCommand({
      directory: dir,
      template: {
        id: 'cli-template-1',
        name: 'CLI Template',
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
    });

    expect(fileName).toBe('cli-template-1.json');
    const list = await runTemplateListCommand({ directory: dir });
    expect(list).toContain(fileName);

    const loaded = await runTemplateLoadCommand({ directory: dir, fileName });
    expect(loaded.id).toBe('cli-template-1');
  });
});
