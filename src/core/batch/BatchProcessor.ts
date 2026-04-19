import { BatchProcessingError } from '@utils/errors';
import { logger } from '@utils/logger';
import { BatchJob, JobQueue } from './JobQueue';
import { WorkerPool } from './WorkerPool';

/** Final status payload for one processed job. */
export interface BatchJobResult {
  jobId: string;
  status: 'completed' | 'failed';
  error?: string;
}

/** Function contract for executing one batch job. */
export type BatchJobExecutor = (job: BatchJob) => Promise<void>;

/**
 * Orchestrates queued jobs with bounded concurrency.
 */
export class BatchProcessor {
  private readonly queue = new JobQueue();
  private readonly pool: WorkerPool;

  constructor(
    private readonly executor: BatchJobExecutor,
    concurrency = 2
  ) {
    this.pool = new WorkerPool(concurrency);
  }

  /**
   * Queue one batch job for later execution.
   */
  addJob(job: Omit<BatchJob, 'status'>): void {
    this.queue.enqueue({ ...job, status: 'pending' });
  }

  /**
   * Process all currently queued jobs.
   */
  async processAll(): Promise<BatchJobResult[]> {
    const jobs: BatchJob[] = [];
    while (this.queue.size() > 0) {
      const next = this.queue.dequeue();
      if (next) {
        jobs.push(next);
      }
    }

    if (jobs.length === 0) {
      return [];
    }

    const settled = await this.pool.run(jobs, async (job): Promise<BatchJobResult> => {
      try {
        await this.executor({ ...job, status: 'processing' });
        logger.info('Batch job completed', { jobId: job.id });
        return { jobId: job.id, status: 'completed' };
      } catch (error) {
        const message = error instanceof Error ? error.message : 'Unknown batch execution error';
        logger.error('Batch job failed', error instanceof Error ? error : undefined, { jobId: job.id });
        throw new BatchProcessingError(message, job.id);
      }
    });

    return settled.map((result, index) => {
      if (result.status === 'fulfilled') {
        return result.value;
      }
      const reason = result.reason;
      const message = reason instanceof Error ? reason.message : 'Unknown error';
      return { jobId: jobs[index].id, status: 'failed', error: message };
    });
  }
}
