#!/usr/bin/env node
import { Command } from 'commander';
import { runBetaReleaseOrchestration } from '../commercial/BetaReleaseOrchestrator';

/** Parsed options for beta preparation command. */
export interface BetaPrepareCliOptions {
  releaseRoot: string;
  evidence: string;
  report: string;
  runCoverage: boolean;
}

/**
 * Parse CLI arguments for beta preparation.
 */
export function parseBetaPrepareOptions(argv: string[]): BetaPrepareCliOptions {
  const program = new Command();
  program
    .name('imposr-beta-prepare')
    .option('--release-root <path>', 'Repository root path', process.cwd())
    .option('--evidence <path>', 'Path to sdk smoke evidence JSON', 'docs/sdk_smoke_evidence.json')
    .option('--report <path>', 'Path to write beta readiness report', 'docs/BETA_READINESS_REPORT.json')
    .option('--coverage', 'Run coverage instead of standard tests', false);

  program.parse(argv);
  const options = program.opts<Record<string, string | boolean>>();
  return {
    releaseRoot: String(options.releaseRoot),
    evidence: String(options.evidence),
    report: String(options.report),
    runCoverage: Boolean(options.coverage),
  };
}

/**
 * Execute six-phase beta preparation.
 */
export async function runBetaPrepareCli(argv: string[]): Promise<number> {
  try {
    const options = parseBetaPrepareOptions(argv);
    const report = await runBetaReleaseOrchestration({
      releaseRoot: options.releaseRoot,
      evidencePath: options.evidence,
      reportPath: options.report,
      runCoverage: options.runCoverage,
    });

    process.stdout.write(`${JSON.stringify(report, null, 2)}\n`);
    return report.overallPassed ? 0 : 2;
  } catch (error) {
    const message = error instanceof Error ? error.message : 'Unknown beta prepare CLI error';
    process.stderr.write(`${message}\n`);
    return 1;
  }
}

if (require.main === module) {
  void runBetaPrepareCli(process.argv).then((exitCode) => {
    process.exitCode = exitCode;
  });
}
