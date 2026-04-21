#!/usr/bin/env node
import { Command } from 'commander';
import { runBetaCommand } from './commands/beta';

/** Parsed CLI options for beta-ready standalone entrypoint. */
export interface BetaReadyCliOptions {
  releaseRoot: string;
  evidence: string;
  output: string;
  allowSimulatedRuntime: boolean;
}

/**
 * Parse beta-ready command arguments.
 */
export function buildBetaReadyOptions(argv: string[]): BetaReadyCliOptions {
  const program = new Command();
  program
    .name('imposr-beta-ready')
    .option('--release-root <path>', 'Repository root path', process.cwd())
    .option('--evidence <path>', 'Path to sdk smoke evidence JSON', 'docs/sdk_smoke_evidence.json')
    .option('--output <path>', 'Output report path', 'docs/BETA_READINESS_REPORT.json')
    .option('--allow-simulated-runtime', 'Allow simulated runtime rows for beta dry-runs', false);

  program.parse(argv);
  const options = program.opts<Record<string, string | boolean>>();

  return {
    releaseRoot: String(options.releaseRoot),
    evidence: String(options.evidence),
    output: String(options.output),
    allowSimulatedRuntime: Boolean(options.allowSimulatedRuntime),
  };
}

/**
 * Execute standalone beta-ready command.
 */
export async function runBetaReadyCli(argv: string[]): Promise<number> {
  try {
    const options = buildBetaReadyOptions(argv);
    const report = await runBetaCommand({
      releaseRoot: options.releaseRoot,
      evidencePath: options.evidence,
      outputPath: options.output,
      allowSimulatedRuntime: options.allowSimulatedRuntime,
    });
    process.stdout.write(`${JSON.stringify(report, null, 2)}\n`);
    return report.overallPassed ? 0 : 2;
  } catch (error) {
    const message = error instanceof Error ? error.message : 'Unknown beta-ready CLI error';
    process.stderr.write(`${message}\n`);
    return 1;
  }
}

if (require.main === module) {
  void runBetaReadyCli(process.argv).then((exitCode) => {
    process.exitCode = exitCode;
  });
}
