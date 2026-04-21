import fs from 'node:fs/promises';
import os from 'node:os';
import path from 'node:path';
import { buildBetaReadyOptions, runBetaReadyCli } from '../../../src/cli/beta-ready';

async function createBetaFixtureRoot(): Promise<string> {
  const root = await fs.mkdtemp(path.join(os.tmpdir(), 'imposr-beta-ready-'));
  const docs = [
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

  for (const rel of docs) {
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

describe('cli/beta-ready', () => {
  it('parses explicit options', () => {
    const options = buildBetaReadyOptions([
      'node',
      'beta-ready',
      '--release-root',
      '/repo',
      '--evidence',
      'docs/custom-evidence.json',
      '--output',
      'reports/beta.json',
      '--allow-simulated-runtime',
    ]);

    expect(options.releaseRoot).toBe('/repo');
    expect(options.evidence).toBe('docs/custom-evidence.json');
    expect(options.output).toBe('reports/beta.json');
    expect(options.allowSimulatedRuntime).toBe(true);
  });

  it('executes end-to-end and returns zero on pass', async () => {
    const root = await createBetaFixtureRoot();
    const code = await runBetaReadyCli([
      'node',
      'beta-ready',
      '--release-root',
      root,
      '--evidence',
      'docs/sdk_smoke_evidence.json',
      '--output',
      'docs/BETA_READY_OUT.json',
    ]);

    expect(code).toBe(0);

    const reportRaw = await fs.readFile(path.join(root, 'docs/BETA_READY_OUT.json'), 'utf8');
    expect(JSON.parse(reportRaw).overallPassed).toBe(true);
  });
});
