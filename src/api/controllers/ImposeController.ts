import { Request, Response, NextFunction } from 'express';
import { runImposeCommand } from '../../cli/commands/impose';

/** Request body for impose endpoint. */
export interface ImposeRequestBody {
  pages: number;
  mode: 'sequential' | 'booklet';
  columns: number;
  rows: number;
  sheetWidth: number;
  sheetHeight: number;
  output: string;
}

/** HTTP controller for imposition requests. */
export class ImposeController {
  /**
   * Build imposition plan and persist output JSON.
   */
  async impose(req: Request<unknown, unknown, ImposeRequestBody>, res: Response, next: NextFunction): Promise<void> {
    try {
      const plan = await runImposeCommand(req.body);
      res.status(200).json({
        ok: true,
        sheets: plan.length,
        output: req.body.output,
      });
    } catch (error) {
      next(error);
    }
  }
}
