import { Request, Response, NextFunction } from 'express';
import {
  runTemplateListCommand,
  runTemplateLoadCommand,
  runTemplateSaveCommand,
} from '../../cli/commands/templates';
import { ImpositionTemplate } from '@core/templates/TemplateValidator';

/** HTTP controller for template management routes. */
export class TemplateController {
  /**
   * List template filenames.
   */
  async list(req: Request, res: Response, next: NextFunction): Promise<void> {
    try {
      const directory = String(req.query.directory ?? 'templates/custom');
      const files = await runTemplateListCommand({ directory });
      res.status(200).json({ ok: true, files });
    } catch (error) {
      next(error);
    }
  }

  /**
   * Load one template by file name.
   */
  async load(
    req: Request<{ fileName: string }, unknown, unknown, { directory?: string }>,
    res: Response,
    next: NextFunction
  ): Promise<void> {
    try {
      const directory = String(req.query.directory ?? 'templates/custom');
      const template = await runTemplateLoadCommand({
        directory,
        fileName: req.params.fileName,
      });
      res.status(200).json({ ok: true, template });
    } catch (error) {
      next(error);
    }
  }

  /**
   * Save one template payload.
   */
  async save(
    req: Request<unknown, unknown, { directory?: string; template: ImpositionTemplate }>,
    res: Response,
    next: NextFunction
  ): Promise<void> {
    try {
      const directory = req.body.directory ?? 'templates/custom';
      const fileName = await runTemplateSaveCommand({
        directory,
        template: req.body.template,
      });
      res.status(201).json({ ok: true, fileName });
    } catch (error) {
      next(error);
    }
  }
}
