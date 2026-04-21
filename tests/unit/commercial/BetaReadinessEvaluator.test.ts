import fs from 'node:fs/promises';
import os from 'node:os';
import path from 'node:path';
import { evaluateBetaReadiness, hasUncheckedChecklist, loadJsonFile } from '../../../src/commercial/BetaReadinessEvaluator';

const DOCS = [
  'docs/COMMERCIAL_RELEASE_CHECKLIST.md',
  'docs/commercial/THIRD_PARTY_NOTICES.md',
  'docs/commercial/EULA.md',
  'docs/commercial/PRIVACY_STATEMENT.md',
  'docs/commercial/TELEMETRY_DISCLOSURE.md',
  'docs/release/VERSIONING_POLICY.md',
  'docs/release/ROLLBACK_POLICY.md',
  'docs/release/UPGRADE_COMPATIBILITY_MATRIX.md',
  'docs/support/INCIDENT_SLA.md',
  'docs/security/SECURITY_RELEASE_CHECKLIST.md',
  'docs/security/SBOM_POLICY.md',
  'docs/customer-ops/ENTERPRISE_DEPLOYMENT.md',
  'docs/customer-ops/TROUBLESHOOTING_PLAYBOOK.md',
];

async function createFixtureRoot(): Promise<string> {
  const root = await fs.mkdtemp(path.join(os.tmpdir(), 'imposr-beta-'));
  for (const rel of DOCS) {
    const full = path.join(root, rel);
    await fs.mkdir(path.dirname(full), { recursive: true });
    await fs.writeFile(full, '# ok\n- [x] done\n', 'utf8');
  }

  const evidence = {
    rows: [
      {
        os: 'Windows 11 23H2',
        cpu: 'x64',
        execution_mode: 'host-runtime',
        plugin_load: 'PASS',
        menu_actions: 'PASS',
        preset_validate: 'PASS',
        preset_preview: 'PASS',
        preset_run_bundle: 'PASS',
        runtime_quality_gate: 'PASS',
        imposed_output_open: 'PASS',
        panel_quick_actions: 'PASS',
      },
      {
        os: 'macOS 14 Sonoma',
        cpu: 'arm64',
        execution_mode: 'host-runtime',
        plugin_load: 'PASS',
        menu_actions: 'PASS',
        preset_validate: 'PASS',
        preset_preview: 'PASS',
        preset_run_bundle: 'PASS',
        runtime_quality_gate: 'PASS',
        imposed_output_open: 'PASS',
        panel_quick_actions: 'PASS',
      },
      {
        os: 'macOS 14 Sonoma',
        cpu: 'x64 (Rosetta/Intel)',
        execution_mode: 'host-runtime',
        plugin_load: 'PASS',
        menu_actions: 'PASS',
        preset_validate: 'PASS',
        preset_preview: 'PASS',
        preset_run_bundle: 'PASS',
        runtime_quality_gate: 'PASS',
        imposed_output_open: 'PASS',
        panel_quick_actions: 'PASS',
      },
    ],
  };

  await fs.mkdir(path.join(root, 'docs'), { recursive: true });
  await fs.writeFile(path.join(root, 'docs/sdk_smoke_evidence.json'), JSON.stringify(evidence), 'utf8');

  return root;
}

describe('BetaReadinessEvaluator', () => {
  it('loadJsonFile parses JSON and hasUncheckedChecklist detects open items', async () => {
    const root = await createFixtureRoot();
    const jsonPath = path.join(root, 'sample.json');
    const checklistPath = path.join(root, 'checklist.md');
    await fs.writeFile(jsonPath, '{"ok":true}', 'utf8');
    await fs.writeFile(checklistPath, '- [ ] pending', 'utf8');

    const loaded = await loadJsonFile<{ ok: boolean }>(jsonPath);
    const hasOpen = await hasUncheckedChecklist(checklistPath);

    expect(loaded.ok).toBe(true);
    expect(hasOpen).toBe(true);
  });

  it('returns pass report when all beta preconditions are satisfied', async () => {
    const root = await createFixtureRoot();

    const report = await evaluateBetaReadiness({
      releaseRoot: root,
      evidencePath: 'docs/sdk_smoke_evidence.json',
      allowSimulatedRuntime: false,
    });

    expect(report.overallPassed).toBe(true);
    expect(report.phases).toHaveLength(6);
    expect(report.phases.every((phase) => phase.passed)).toBe(true);
  });

  it('fails when runtime mode is simulated and not allowed', async () => {
    const root = await createFixtureRoot();
    const evidencePath = path.join(root, 'docs/sdk_smoke_evidence.json');
    const evidence = JSON.parse(await fs.readFile(evidencePath, 'utf8')) as {
      rows: Array<Record<string, string>>;
    };
    evidence.rows[0].execution_mode = 'simulated-runtime';
    await fs.writeFile(evidencePath, JSON.stringify(evidence), 'utf8');

    const report = await evaluateBetaReadiness({
      releaseRoot: root,
      evidencePath: 'docs/sdk_smoke_evidence.json',
      allowSimulatedRuntime: false,
    });

    expect(report.overallPassed).toBe(false);
    expect(report.phases[1].passed).toBe(false);
  });
});
