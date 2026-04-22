#!/usr/bin/env node
import 'module-alias/register';
import { Command } from 'commander';
import { runBatchCommand } from './commands/batch';
import { runBetaCommand } from './commands/beta';
import { runBetaReleaseOrchestration } from '../commercial/BetaReleaseOrchestrator';
import { runImposeCommand } from './commands/impose';
import {
  runTemplateListCommand,
  runTemplateLoadCommand,
  runTemplateSaveCommand,
} from './commands/templates';
import { runValidateCommand } from './commands/validate';
import { cliLogger } from './utils/logger';

const program = new Command();

program.name('imposr').description('Imposr command line interface').version('1.0.0');

program
  .command('impose')
  .requiredOption('--pages <number>', 'Source page count')
  .requiredOption('--mode <mode>', 'sequential|booklet')
  .requiredOption('--columns <number>', 'Sheet columns')
  .requiredOption('--rows <number>', 'Sheet rows')
  .requiredOption('--sheet-width <number>', 'Sheet width in points')
  .requiredOption('--sheet-height <number>', 'Sheet height in points')
  .requiredOption('--output <file>', 'Output JSON file')
  .action(async (raw: Record<string, string>) => {
    try {
      const plan = await runImposeCommand({
        pages: Number(raw.pages),
        mode: raw.mode as 'sequential' | 'booklet',
        columns: Number(raw.columns),
        rows: Number(raw.rows),
        sheetWidth: Number(raw.sheetWidth),
        sheetHeight: Number(raw.sheetHeight),
        output: raw.output,
      });
      cliLogger.info(`Generated ${plan.length} sheet plans`);
    } catch (error) {
      const message = error instanceof Error ? error.message : 'Unknown impose command error';
      cliLogger.error(message);
      process.exitCode = 1;
    }
  });

program
  .command('batch')
  .requiredOption('--jobs-file <path>', 'JSON file with batch jobs')
  .option('--concurrency <number>', 'Number of parallel workers', '2')
  .action(async (raw: Record<string, string>) => {
    try {
      const result = await runBatchCommand({
        jobsFile: raw.jobsFile,
        concurrency: Number(raw.concurrency),
      });
      cliLogger.info(JSON.stringify(result, null, 2));
      process.exitCode = result.failed === 0 ? 0 : 2;
    } catch (error) {
      const message = error instanceof Error ? error.message : 'Unknown batch command error';
      cliLogger.error(message);
      process.exitCode = 1;
    }
  });

program
  .command('validate')
  .requiredOption('--file <path>', 'PDF input file')
  .option('--min-pages <number>', 'Minimum page count')
  .option('--max-pages <number>', 'Maximum page count')
  .option('--max-file-size-bytes <number>', 'Maximum file size')
  .option('--profile <profile>', 'none|pdfx-1a|pdfx-4', 'none')
  .action(async (raw: Record<string, string>) => {
    try {
      const result = await runValidateCommand({
        file: raw.file,
        minPages: raw.minPages ? Number(raw.minPages) : undefined,
        maxPages: raw.maxPages ? Number(raw.maxPages) : undefined,
        maxFileSizeBytes: raw.maxFileSizeBytes ? Number(raw.maxFileSizeBytes) : undefined,
        profile: raw.profile as 'none' | 'pdfx-1a' | 'pdfx-4',
      });
      cliLogger.info(JSON.stringify(result, null, 2));
      process.exitCode = result.valid ? 0 : 2;
    } catch (error) {
      const message = error instanceof Error ? error.message : 'Unknown validate command error';
      cliLogger.error(message);
      process.exitCode = 1;
    }
  });


program
  .command('beta:ready')
  .option('--release-root <path>', 'Repository root path', process.cwd())
  .option('--evidence <path>', 'Path to sdk smoke evidence JSON', 'docs/sdk_smoke_evidence.json')
  .option('--output <path>', 'Output report path', 'docs/BETA_READINESS_REPORT.json')
  .option('--allow-simulated-runtime', 'Allow simulated runtime rows for beta dry-runs', false)
  .action(async (raw: Record<string, string | boolean>) => {
    try {
      const report = await runBetaCommand({
        releaseRoot: String(raw.releaseRoot),
        evidencePath: String(raw.evidence),
        outputPath: String(raw.output),
        allowSimulatedRuntime: Boolean(raw.allowSimulatedRuntime),
      });
      cliLogger.info(JSON.stringify(report, null, 2));
      process.exitCode = report.overallPassed ? 0 : 2;
    } catch (error) {
      const message = error instanceof Error ? error.message : 'Unknown beta:ready command error';
      cliLogger.error(message);
      process.exitCode = 1;
    }
  });


program
  .command('beta:prepare')
  .option('--release-root <path>', 'Repository root path', process.cwd())
  .option('--evidence <path>', 'Path to sdk smoke evidence JSON', 'docs/sdk_smoke_evidence.json')
  .option('--report <path>', 'Path to write beta readiness report', 'docs/BETA_READINESS_REPORT.json')
  .option('--coverage', 'Run coverage instead of standard tests', false)
  .action(async (raw: Record<string, string | boolean>) => {
    try {
      const report = await runBetaReleaseOrchestration({
        releaseRoot: String(raw.releaseRoot),
        evidencePath: String(raw.evidence),
        reportPath: String(raw.report),
        runCoverage: Boolean(raw.coverage),
      });
      cliLogger.info(JSON.stringify(report, null, 2));
      process.exitCode = report.overallPassed ? 0 : 2;
    } catch (error) {
      const message = error instanceof Error ? error.message : 'Unknown beta:prepare command error';
      cliLogger.error(message);
      process.exitCode = 1;
    }
  });

program
  .command('templates:list')
  .requiredOption('--directory <path>', 'Templates directory')
  .action(async (raw: Record<string, string>) => {
    try {
      const result = await runTemplateListCommand({ directory: raw.directory });
      cliLogger.info(JSON.stringify(result, null, 2));
    } catch (error) {
      const message = error instanceof Error ? error.message : 'Unknown templates:list command error';
      cliLogger.error(message);
      process.exitCode = 1;
    }
  });

program
  .command('templates:load')
  .requiredOption('--directory <path>', 'Templates directory')
  .requiredOption('--file-name <name>', 'Template JSON file name')
  .action(async (raw: Record<string, string>) => {
    try {
      const result = await runTemplateLoadCommand({
        directory: raw.directory,
        fileName: raw.fileName,
      });
      cliLogger.info(JSON.stringify(result, null, 2));
    } catch (error) {
      const message = error instanceof Error ? error.message : 'Unknown templates:load command error';
      cliLogger.error(message);
      process.exitCode = 1;
    }
  });

program
  .command('templates:save')
  .requiredOption('--directory <path>', 'Templates directory')
  .requiredOption('--template-file <path>', 'Template JSON payload')
  .action(async (raw: Record<string, string>) => {
    try {
      const fs = await import('node:fs/promises');
      const source = await fs.readFile(raw.templateFile, 'utf8');
      const template = JSON.parse(source) as Parameters<typeof runTemplateSaveCommand>[0]['template'];
      const fileName = await runTemplateSaveCommand({ directory: raw.directory, template });
      cliLogger.info(fileName);
    } catch (error) {
      const message = error instanceof Error ? error.message : 'Unknown templates:save command error';
      cliLogger.error(message);
      process.exitCode = 1;
    }
  });

void program.parseAsync(process.argv);
