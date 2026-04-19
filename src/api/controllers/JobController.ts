import { Request, Response, NextFunction } from 'express';
import { runBatchCommand } from '../../cli/commands/batch';

/** Request body for batch execution endpoint. */
interface RunBatchBody {
  jobsFile: string;
  concurrency?: number;
}

/** HTTP controller for batch job routes. */
export class JobController {
  /**
   * Start batch processing for a JSON jobs file.
   */
  async runBatch(req: Request<unknown, unknown, RunBatchBody>, res: Response, next: NextFunction): Promise<void> {
    try {
      const result = await runBatchCommand(req.body);
      res.status(200).json({ ok: true, ...result });
    } catch (error) {
      next(error);
    }
  }
}
