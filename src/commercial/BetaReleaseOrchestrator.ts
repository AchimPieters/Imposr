import fs from 'node:fs/promises';
import path from 'node:path';
import { spawn } from 'node:child_process';

/** Supported phase IDs in the beta release orchestrator. */
export type BetaOrchestrationPhaseId = 1 | 2 | 3 | 4 | 5 | 6;

/** One command execution outcome. */
export interface CommandResult {
  command: string;
  exitCode: number;
  stdout: string;
  stderr: string;
}

/** Command runner abstraction for testability. */
export type CommandRunner = (command: string, args: string[], cwd: string) => Promise<CommandResult>;

/** One orchestrated phase result. */
export interface BetaOrchestrationPhaseResult {
  phase: BetaOrchestrationPhaseId;
  name: string;
  passed: boolean;
  message: string;
  command?: CommandResult;
}

/** Final orchestration report. */
export interface BetaOrchestrationReport {
  generatedAtUtc: string;
  releaseRoot: string;
  phases: BetaOrchestrationPhaseResult[];
  overallPassed: boolean;
}

/** Runtime options for beta orchestration. */
export interface BetaOrchestrationOptions {
  releaseRoot: string;
  evidencePath: string;
  reportPath: string;
  runCoverage: boolean;
}

/**
 * Execute a command with stdout/stderr capture.
 */
export const defaultCommandRunner: CommandRunner = async (command, args, cwd) => {
  return new Promise<CommandResult>((resolve, reject) => {
    const child = spawn(command, args, {
      cwd,
      shell: false,
      stdio: ['ignore', 'pipe', 'pipe'],
    });

    let stdout = '';
    let stderr = '';

    child.stdout.on('data', (chunk) => {
      stdout += chunk.toString();
    });

    child.stderr.on('data', (chunk) => {
      stderr += chunk.toString();
    });

    child.on('error', (error) => {
      reject(new Error(`Cannot start command ${command}: ${error.message}`));
    });

    child.on('close', (code) => {
      resolve({
        command: [command, ...args].join(' '),
        exitCode: code ?? 1,
        stdout,
        stderr,
      });
    });
  });
};

async function fileExists(filePath: string): Promise<boolean> {
  try {
    const stat = await fs.stat(filePath);
    return stat.isFile();
  } catch {
    return false;
  }
}

/**
 * Run six strict phases to produce a beta-ready release report.
 */
export async function runBetaReleaseOrchestration(
  options: BetaOrchestrationOptions,
  commandRunner: CommandRunner = defaultCommandRunner,
): Promise<BetaOrchestrationReport> {
  const releaseRoot = path.resolve(options.releaseRoot);
  const reportPath = path.resolve(releaseRoot, options.reportPath);
  const evidencePath = path.resolve(releaseRoot, options.evidencePath);

  const phases: BetaOrchestrationPhaseResult[] = [];

  const phase1Passed = await fileExists(evidencePath);
  phases.push({
    phase: 1,
    name: 'Preflight',
    passed: phase1Passed,
    message: phase1Passed
      ? `Evidence bestand gevonden: ${evidencePath}`
      : `Evidence bestand ontbreekt: ${evidencePath}`,
  });
  if (!phase1Passed) {
    return {
      generatedAtUtc: new Date().toISOString(),
      releaseRoot,
      phases,
      overallPassed: false,
    };
  }

  const phase2Command = await commandRunner('npm', ['run', 'typecheck'], releaseRoot);
  phases.push({
    phase: 2,
    name: 'Type Safety',
    passed: phase2Command.exitCode === 0,
    message: phase2Command.exitCode === 0 ? 'Typecheck geslaagd' : 'Typecheck gefaald',
    command: phase2Command,
  });
  if (phase2Command.exitCode !== 0) {
    return { generatedAtUtc: new Date().toISOString(), releaseRoot, phases, overallPassed: false };
  }

  const testArgs = options.runCoverage
    ? ['run', 'test:coverage', '--', '--runInBand', 'tests/unit/commercial', 'tests/unit/cli']
    : ['run', 'test', '--', '--runInBand', 'tests/unit/commercial', 'tests/unit/cli'];
  const phase3Command = await commandRunner('npm', testArgs, releaseRoot);
  phases.push({
    phase: 3,
    name: 'Quality Tests',
    passed: phase3Command.exitCode === 0,
    message: phase3Command.exitCode === 0 ? 'Tests geslaagd' : 'Tests gefaald',
    command: phase3Command,
  });
  if (phase3Command.exitCode !== 0) {
    return { generatedAtUtc: new Date().toISOString(), releaseRoot, phases, overallPassed: false };
  }

  const phase4Command = await commandRunner('npm', ['run', 'build:main'], releaseRoot);
  phases.push({
    phase: 4,
    name: 'Build',
    passed: phase4Command.exitCode === 0,
    message: phase4Command.exitCode === 0 ? 'Build geslaagd' : 'Build gefaald',
    command: phase4Command,
  });
  if (phase4Command.exitCode !== 0) {
    return { generatedAtUtc: new Date().toISOString(), releaseRoot, phases, overallPassed: false };
  }

  const phase5Command = await commandRunner(
    'node',
    ['dist/cli/beta-ready.js', '--release-root', '.', '--evidence', options.evidencePath, '--output', options.reportPath],
    releaseRoot,
  );
  phases.push({
    phase: 5,
    name: 'Beta Readiness Gate',
    passed: phase5Command.exitCode === 0,
    message: phase5Command.exitCode === 0 ? 'Beta gate geslaagd' : 'Beta gate gefaald',
    command: phase5Command,
  });
  if (phase5Command.exitCode !== 0) {
    return { generatedAtUtc: new Date().toISOString(), releaseRoot, phases, overallPassed: false };
  }

  const phase6Passed = await fileExists(reportPath);
  phases.push({
    phase: 6,
    name: 'Beta Artifact Verification',
    passed: phase6Passed,
    message: phase6Passed
      ? `Beta rapport gereed: ${reportPath}`
      : `Beta rapport ontbreekt na gate: ${reportPath}`,
  });

  return {
    generatedAtUtc: new Date().toISOString(),
    releaseRoot,
    phases,
    overallPassed: phases.every((phase) => phase.passed),
  };
}
