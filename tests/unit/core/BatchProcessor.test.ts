import { BatchProcessor } from '../../../src/core/batch/BatchProcessor';

describe('BatchProcessor', () => {
  it('processes queued jobs successfully', async () => {
    const processed: string[] = [];
    const processor = new BatchProcessor(async (job) => {
      processed.push(job.id);
    });

    processor.addJob({ id: 'job-1', inputFile: 'a.pdf', outputFile: 'a-out.pdf' });
    processor.addJob({ id: 'job-2', inputFile: 'b.pdf', outputFile: 'b-out.pdf' });

    const results = await processor.processAll();
    expect(results).toEqual([
      { jobId: 'job-1', status: 'completed' },
      { jobId: 'job-2', status: 'completed' },
    ]);
    expect(processed).toEqual(['job-1', 'job-2']);
  });

  it('returns failed result when executor throws', async () => {
    const processor = new BatchProcessor(async (job) => {
      if (job.id === 'job-2') {
        throw new Error('cannot process');
      }
    });

    processor.addJob({ id: 'job-1', inputFile: 'a.pdf', outputFile: 'a-out.pdf' });
    processor.addJob({ id: 'job-2', inputFile: 'b.pdf', outputFile: 'b-out.pdf' });

    const results = await processor.processAll();
    expect(results).toContainEqual({ jobId: 'job-1', status: 'completed' });
    expect(results).toContainEqual({
      jobId: 'job-2',
      status: 'failed',
      error: 'cannot process',
    });
  });

  it('returns empty results when queue is empty', async () => {
    const processor = new BatchProcessor(async () => undefined);
    await expect(processor.processAll()).resolves.toEqual([]);
  });
});
