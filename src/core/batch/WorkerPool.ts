/**
 * Parallel worker executor with deterministic max concurrency.
 */
export class WorkerPool {
  constructor(private readonly concurrency: number) {
    if (!Number.isInteger(concurrency) || concurrency <= 0) {
      throw new Error('Concurrency must be a positive integer');
    }
  }

  /**
   * Execute jobs with bounded parallelism.
   */
  async run<TItem, TResult>(
    items: TItem[],
    handler: (item: TItem, index: number) => Promise<TResult>
  ): Promise<Array<PromiseSettledResult<TResult>>> {
    const results: Array<PromiseSettledResult<TResult>> = new Array(items.length);
    let nextIndex = 0;

    const workers = Array.from({ length: Math.min(this.concurrency, items.length) }, async () => {
      while (nextIndex < items.length) {
        const currentIndex = nextIndex;
        nextIndex += 1;
        try {
          const value = await handler(items[currentIndex], currentIndex);
          results[currentIndex] = { status: 'fulfilled', value };
        } catch (reason) {
          results[currentIndex] = { status: 'rejected', reason };
        }
      }
    });

    await Promise.all(workers);
    return results;
  }
}
