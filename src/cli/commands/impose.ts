import fs from 'node:fs/promises';
import { ImpositionEngine, SheetPlan } from '@core/imposition/ImpositionEngine';
import { ArrangeMode } from '@core/imposition/PageArranger';

/** Typed options accepted by impose command. */
export interface ImposeCommandOptions {
  pages: number;
  mode: ArrangeMode;
  columns: number;
  rows: number;
  sheetWidth: number;
  sheetHeight: number;
  output: string;
}

/**
 * Execute imposition planning and write result JSON.
 */
export async function runImposeCommand(options: ImposeCommandOptions): Promise<SheetPlan[]> {
  const engine = new ImpositionEngine();
  const plan = engine.buildPlan({
    pageCount: options.pages,
    mode: options.mode,
    layout: {
      sheet: { x: 0, y: 0, width: options.sheetWidth, height: options.sheetHeight },
      columns: options.columns,
      rows: options.rows,
      gap: 0,
      margin: 0,
    },
  });

  await fs.writeFile(options.output, `${JSON.stringify(plan, null, 2)}\n`, 'utf8');
  return plan;
}
