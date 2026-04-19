import { BatchProcessingError } from '@utils/errors';

/** Current lifecycle state of a batch job. */
export type BatchJobStatus = 'pending' | 'processing' | 'completed' | 'failed';

/** Input contract for one batch job. */
export interface BatchJob {
  id: string;
  inputFile: string;
  outputFile: string;
  templateId?: string;
  status: BatchJobStatus;
}

/**
 * In-memory FIFO queue for batch jobs.
 */
export class JobQueue {
  private readonly jobs: BatchJob[] = [];

  /**
   * Add a job to the queue.
   * @throws BatchProcessingError when the job is invalid or duplicated.
   */
  enqueue(job: BatchJob): void {
    if (!job.id.trim()) {
      throw new BatchProcessingError('Job id is required', job.id || 'unknown');
    }
    if (!job.inputFile.trim() || !job.outputFile.trim()) {
      throw new BatchProcessingError('Job input/output paths are required', job.id);
    }
    if (this.jobs.some((candidate) => candidate.id === job.id)) {
      throw new BatchProcessingError('Duplicate job id in queue', job.id);
    }
    this.jobs.push({ ...job, status: 'pending' });
  }

  /**
   * Remove and return the next job from the queue.
   */
  dequeue(): BatchJob | undefined {
    return this.jobs.shift();
  }

  /**
   * Read the next job without removing it.
   */
  peek(): BatchJob | undefined {
    return this.jobs[0];
  }

  /**
   * Remove a specific queued job by id.
   * @returns true when a job was removed.
   */
  remove(jobId: string): boolean {
    const index = this.jobs.findIndex((job) => job.id === jobId);
    if (index < 0) {
      return false;
    }
    this.jobs.splice(index, 1);
    return true;
  }

  /**
   * Total number of queued jobs.
   */
  size(): number {
    return this.jobs.length;
  }

  /**
   * Remove all queued jobs.
   */
  clear(): void {
    this.jobs.splice(0, this.jobs.length);
  }
}
