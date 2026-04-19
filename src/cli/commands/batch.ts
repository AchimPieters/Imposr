import fs from 'node:fs/promises';
import { BatchProcessor } from '@core/batch/BatchProcessor';
import { runImposeCommand } from './impose';

/** One job item in batch command file. */
export interface BatchImposeJob {
  id: string;
  pages: number;
  mode: 'sequential' | 'booklet';
  columns: number;
  rows: number;
  sheetWidth: number;
  sheetHeight: number;
  output: string;
}

/** Options for running batch command. */
export interface BatchCommandOptions {
  jobsFile: string;
  concurrency?: number;
}

/** Batch summary. */
export interface BatchCommandResult {
  total: number;
  completed: number;
  failed: number;
}

/** Execute impose jobs from JSON file. */
export async function runBatchCommand(options: BatchCommandOptions): Promise<BatchCommandResult> {
  const raw = await fs.readFile(options.jobsFile, 'utf8');
  const payload = JSON.parse(raw) as unknown;
  if (!Array.isArray(payload)) {
    throw new Error('Batch jobs file must contain a JSON array');
  }

  const jobs = payload as BatchImposeJob[];
  const processor = new BatchProcessor(
    async (job) => {
      const source = jobs.find((candidate) => candidate.id === job.id);
      if (!source) {
        throw new Error(`Missing job definition for ${job.id}`);
      }
      await runImposeCommand(source);
    },
    options.concurrency ?? 2
  );

  for (const job of jobs) {
    processor.addJob({
      id: job.id,
      inputFile: `virtual://${job.id}`,
      outputFile: job.output,
    });
  }

  const results = await processor.processAll();
  const completed = results.filter((result) => result.status === 'completed').length;
  const failed = results.length - completed;
  return { total: results.length, completed, failed };
}
