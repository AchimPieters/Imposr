import fs from 'node:fs/promises';
import os from 'node:os';
import path from 'node:path';
import { CommandResult, runBetaReleaseOrchestration } from '../../../src/commercial/BetaReleaseOrchestrator';

function createRunner(results: CommandResult[]): (command: string, args: string[], cwd: string) => Promise<CommandResult> {
  let index = 0;
  return async (command: string, args: string[], cwd: string) => {
    const result = results[index];
    index += 1;
    return {
      ...result,
      command: `${command} ${args.join(' ')}`.trim(),
      stdout: `${result.stdout}\nCWD=${cwd}`,
    };
  };
}

describe('BetaReleaseOrchestrator', () => {
  it('fails fast when evidence file is missing', async () => {
    const root = await fs.mkdtemp(path.join(os.tmpdir(), 'imposr-orch-'));
    const report = await runBetaReleaseOrchestration(
      {
        releaseRoot: root,
        evidencePath: 'docs/missing.json',
        reportPath: 'docs/BETA_READINESS_REPORT.json',
        runCoverage: false,
      },
      createRunner([]),
    );

    expect(report.overallPassed).toBe(false);
    expect(report.phases).toHaveLength(1);
    expect(report.phases[0].phase).toBe(1);
  });

  it('completes all phases when commands and artifacts pass', async () => {
    const root = await fs.mkdtemp(path.join(os.tmpdir(), 'imposr-orch-pass-'));
    await fs.mkdir(path.join(root, 'docs'), { recursive: true });
    await fs.writeFile(path.join(root, 'docs/sdk_smoke_evidence.json'), '{"rows":[]}', 'utf8');
    await fs.writeFile(path.join(root, 'docs/BETA_READINESS_REPORT.json'), '{"overallPassed":true}', 'utf8');

    const report = await runBetaReleaseOrchestration(
      {
        releaseRoot: root,
        evidencePath: 'docs/sdk_smoke_evidence.json',
        reportPath: 'docs/BETA_READINESS_REPORT.json',
        runCoverage: true,
      },
      createRunner([
        { command: '', exitCode: 0, stdout: 'ok', stderr: '' },
        { command: '', exitCode: 0, stdout: 'ok', stderr: '' },
        { command: '', exitCode: 0, stdout: 'ok', stderr: '' },
        { command: '', exitCode: 0, stdout: 'ok', stderr: '' },
      ]),
    );

    expect(report.overallPassed).toBe(true);
    expect(report.phases).toHaveLength(6);
    expect(report.phases.every((phase) => phase.passed)).toBe(true);
  });

  it('stops when a command phase fails', async () => {
    const root = await fs.mkdtemp(path.join(os.tmpdir(), 'imposr-orch-fail-'));
    await fs.mkdir(path.join(root, 'docs'), { recursive: true });
    await fs.writeFile(path.join(root, 'docs/sdk_smoke_evidence.json'), '{"rows":[]}', 'utf8');

    const report = await runBetaReleaseOrchestration(
      {
        releaseRoot: root,
        evidencePath: 'docs/sdk_smoke_evidence.json',
        reportPath: 'docs/BETA_READINESS_REPORT.json',
        runCoverage: false,
      },
      createRunner([
        { command: '', exitCode: 0, stdout: 'ok', stderr: '' },
        { command: '', exitCode: 1, stdout: 'fail', stderr: 'x' },
      ]),
    );

    expect(report.overallPassed).toBe(false);
    expect(report.phases).toHaveLength(3);
    expect(report.phases[2].passed).toBe(false);
  });
});
