import { JobQueue } from '../../../src/core/batch/JobQueue';

describe('JobQueue', () => {
  it('enqueues and dequeues in FIFO order', () => {
    const queue = new JobQueue();
    queue.enqueue({ id: 'a', inputFile: 'in-a.pdf', outputFile: 'out-a.pdf', status: 'pending' });
    queue.enqueue({ id: 'b', inputFile: 'in-b.pdf', outputFile: 'out-b.pdf', status: 'pending' });

    expect(queue.size()).toBe(2);
    expect(queue.dequeue()?.id).toBe('a');
    expect(queue.dequeue()?.id).toBe('b');
    expect(queue.dequeue()).toBeUndefined();
  });

  it('throws on duplicate job ids', () => {
    const queue = new JobQueue();
    queue.enqueue({ id: 'dup', inputFile: 'in.pdf', outputFile: 'out.pdf', status: 'pending' });
    expect(() =>
      queue.enqueue({ id: 'dup', inputFile: 'in2.pdf', outputFile: 'out2.pdf', status: 'pending' })
    ).toThrow();
  });

  it('removes specific jobs by id', () => {
    const queue = new JobQueue();
    queue.enqueue({ id: 'a', inputFile: 'in-a.pdf', outputFile: 'out-a.pdf', status: 'pending' });
    queue.enqueue({ id: 'b', inputFile: 'in-b.pdf', outputFile: 'out-b.pdf', status: 'pending' });
    expect(queue.remove('a')).toBe(true);
    expect(queue.remove('missing')).toBe(false);
    expect(queue.peek()?.id).toBe('b');
  });
});
