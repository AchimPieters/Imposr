import fs from 'node:fs/promises';
import path from 'node:path';
import { BetaReadinessReport, evaluateBetaReadiness } from '../../commercial/BetaReadinessEvaluator';

/** CLI options for generating a beta readiness report. */
export interface BetaCommandOptions {
  releaseRoot: string;
  evidencePath: string;
  outputPath: string;
  allowSimulatedRuntime: boolean;
}

/**
 * Run beta readiness evaluation and persist the report as JSON.
 */
export async function runBetaCommand(options: BetaCommandOptions): Promise<BetaReadinessReport> {
  const report = await evaluateBetaReadiness({
    releaseRoot: options.releaseRoot,
    evidencePath: options.evidencePath,
    allowSimulatedRuntime: options.allowSimulatedRuntime,
  });

  const resolvedOutput = path.resolve(options.releaseRoot, options.outputPath);
  await fs.mkdir(path.dirname(resolvedOutput), { recursive: true });
  await fs.writeFile(resolvedOutput, `${JSON.stringify(report, null, 2)}\n`, 'utf8');

  return report;
}
