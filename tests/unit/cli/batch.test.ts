import fs from 'node:fs/promises';
import os from 'node:os';
import path from 'node:path';
import { runBatchCommand } from '../../../src/cli/commands/batch';

describe('cli/batch', () => {
  it('processes jobs from batch json file', async () => {
    const dir = await fs.mkdtemp(path.join(os.tmpdir(), 'imposr-cli-batch-'));
    const jobsFile = path.join(dir, 'jobs.json');
    const out1 = path.join(dir, 'plan-1.json');
    const out2 = path.join(dir, 'plan-2.json');

    await fs.writeFile(
      jobsFile,
      JSON.stringify(
        [
          {
            id: 'job-1',
            pages: 4,
            mode: 'sequential',
            columns: 2,
            rows: 1,
            sheetWidth: 600,
            sheetHeight: 800,
            output: out1,
          },
          {
            id: 'job-2',
            pages: 8,
            mode: 'sequential',
            columns: 2,
            rows: 2,
            sheetWidth: 600,
            sheetHeight: 800,
            output: out2,
          },
        ],
        null,
        2
      ),
      'utf8'
    );

    const result = await runBatchCommand({ jobsFile, concurrency: 2 });
    expect(result).toEqual({ total: 2, completed: 2, failed: 0 });
  });
});
