import { ImpositionEngine, SheetPlan } from '@core/imposition/ImpositionEngine';
import { TemplateError } from '@utils/errors';
import { ImpositionTemplate } from './TemplateValidator';

/**
 * Compiles templates into concrete imposition plans.
 */
export class TemplateEngine {
  private readonly engine = new ImpositionEngine();

  /**
   * Apply template with page count and return sheet plans.
   * @throws TemplateError when execution fails.
   */
  applyTemplate(template: ImpositionTemplate, pageCount: number): SheetPlan[] {
    if (pageCount <= 0) {
      throw new TemplateError('Page count must be > 0', { pageCount });
    }

    try {
      return this.engine.buildPlan({
        pageCount,
        mode: template.mode,
        layout: {
          sheet: {
            x: 0,
            y: 0,
            width: template.layout.sheetWidth,
            height: template.layout.sheetHeight,
          },
          columns: template.layout.columns,
          rows: template.layout.rows,
          gap: template.layout.gap,
          margin: template.layout.margin,
        },
      });
    } catch (error) {
      throw new TemplateError('Template execution failed', {
        templateId: template.id,
        reason: error instanceof Error ? error.message : 'unknown',
      });
    }
  }
}
