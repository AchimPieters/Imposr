import { WorkerPool } from '../../../src/core/batch/WorkerPool';

describe('WorkerPool', () => {
  it('executes all items and preserves result ordering', async () => {
    const pool = new WorkerPool(2);
    const results = await pool.run([1, 2, 3], async (value) => value * 10);

    expect(results[0]).toMatchObject({ status: 'fulfilled', value: 10 });
    expect(results[1]).toMatchObject({ status: 'fulfilled', value: 20 });
    expect(results[2]).toMatchObject({ status: 'fulfilled', value: 30 });
  });

  it('captures handler failures as rejected results', async () => {
    const pool = new WorkerPool(2);
    const results = await pool.run([1, 2], async (value) => {
      if (value === 2) {
        throw new Error('boom');
      }
      return value;
    });

    expect(results[0].status).toBe('fulfilled');
    expect(results[1].status).toBe('rejected');
  });

  it('throws on invalid concurrency', () => {
    expect(() => new WorkerPool(0)).toThrow();
  });
});
