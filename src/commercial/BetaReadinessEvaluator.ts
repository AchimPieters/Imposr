import fs from 'node:fs/promises';
import path from 'node:path';

/** Phases required to ship an externally testable beta. */
export type BetaPhase = 1 | 2 | 3 | 4 | 5 | 6;
export interface HostEvidenceRow {
  os: string;
  cpu: string;
  execution_mode: string;
  [key: string]: string;
}

/** Root evidence payload model. */
export interface HostEvidencePayload {
  rows: HostEvidenceRow[];
}

/** One phase outcome including validation errors. */
export interface PhaseResult {
  phase: BetaPhase;
  name: string;
  passed: boolean;
  checks: string[];
  errors: string[];
}

/** Aggregate report returned to CLI and tests. */
export interface BetaReadinessReport {
  generatedAtUtc: string;
  releaseRoot: string;
  evidencePath: string;
  overallPassed: boolean;
  phases: PhaseResult[];
}

/** Runtime options for the beta readiness evaluator. */
export interface BetaReadinessOptions {
  /** Repository root used to resolve all relative paths. */
  releaseRoot: string;
  /** Relative or absolute evidence JSON path. */
  evidencePath: string;
  /** If true, simulated runtime rows are accepted for beta builds. */
  allowSimulatedRuntime: boolean;
}

const REQUIRED_CHECKS = [
  'plugin_load',
  'menu_actions',
  'preset_validate',
  'preset_preview',
  'preset_run_bundle',
  'runtime_quality_gate',
  'imposed_output_open',
  'panel_quick_actions',
] as const;

const REQUIRED_HOSTS = [
  { os: 'Windows 11 23H2', cpu: 'x64' },
  { os: 'macOS 14 Sonoma', cpu: 'arm64' },
  { os: 'macOS 14 Sonoma', cpu: 'x64 (Rosetta/Intel)' },
] as const;

const REQUIRED_COMMERCIAL_DOCS = [
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
] as const;

/** Load and parse a JSON file with strict error context. */
export async function loadJsonFile<T>(filePath: string): Promise<T> {
  try {
    const raw = await fs.readFile(filePath, 'utf8');
    return JSON.parse(raw) as T;
  } catch (error) {
    const message = error instanceof Error ? error.message : 'Unknown JSON parse/read error';
    throw new Error(`Cannot read JSON file at "${filePath}": ${message}`);
  }
}

/** Return true if a markdown file still includes unchecked checklist items. */
export async function hasUncheckedChecklist(filePath: string): Promise<boolean> {
  try {
    const text = await fs.readFile(filePath, 'utf8');
    return text.includes('- [ ]');
  } catch (error) {
    const message = error instanceof Error ? error.message : 'Unknown markdown read error';
    throw new Error(`Cannot inspect checklist file at "${filePath}": ${message}`);
  }
}

function normalize(value: string): string {
  return value.trim().toLowerCase();
}

function isPass(value: string): boolean {
  return ['pass', 'ok', 'true', '1'].includes(normalize(value));
}

function findHostRow(rows: HostEvidenceRow[], osName: string, cpu: string): HostEvidenceRow | undefined {
  return rows.find((row) => normalize(row.os) === normalize(osName) && normalize(row.cpu) === normalize(cpu));
}

/** Evaluate all six commercialization phases and return a machine-readable report. */
export async function evaluateBetaReadiness(options: BetaReadinessOptions): Promise<BetaReadinessReport> {
  const releaseRoot = path.resolve(options.releaseRoot);
  const evidencePath = path.resolve(releaseRoot, options.evidencePath);
  const evidence = await loadJsonFile<HostEvidencePayload>(evidencePath);

  if (!Array.isArray(evidence.rows)) {
    throw new Error(`Invalid evidence format in "${evidencePath}": rows must be an array`);
  }

  const phase1: PhaseResult = {
    phase: 1,
    name: 'Core Foundation',
    passed: true,
    checks: ['Host evidence file exists and is parseable JSON'],
    errors: [],
  };

  const phase2Errors: string[] = [];
  for (const host of REQUIRED_HOSTS) {
    const row = findHostRow(evidence.rows, host.os, host.cpu);
    if (!row) {
      phase2Errors.push(`Missing host row for ${host.os} / ${host.cpu}`);
      continue;
    }
    const mode = normalize(row.execution_mode ?? '');
    if (options.allowSimulatedRuntime) {
      if (!['host-runtime', 'simulated-runtime'].includes(mode)) {
        phase2Errors.push(`${host.os}/${host.cpu}: invalid execution_mode "${row.execution_mode}"`);
      }
    } else if (mode !== 'host-runtime') {
      phase2Errors.push(`${host.os}/${host.cpu}: execution_mode must be host-runtime`);
    }
  }
  const phase2: PhaseResult = {
    phase: 2,
    name: 'Licensing Core',
    passed: phase2Errors.length === 0,
    checks: ['All required Windows/macOS host matrix rows are present with allowed runtime mode'],
    errors: phase2Errors,
  };

  const phase3Errors: string[] = [];
  for (const host of REQUIRED_HOSTS) {
    const row = findHostRow(evidence.rows, host.os, host.cpu);
    if (!row) {
      continue;
    }
    for (const check of REQUIRED_CHECKS) {
      if (!isPass(row[check] ?? '')) {
        phase3Errors.push(`${host.os}/${host.cpu}: check ${check} must be PASS`);
      }
    }
  }
  const phase3: PhaseResult = {
    phase: 3,
    name: 'API Enforcement',
    passed: phase3Errors.length === 0,
    checks: ['All required runtime quality checks are passing per host row'],
    errors: phase3Errors,
  };

  const phase4Errors: string[] = [];
  for (const relativePath of REQUIRED_COMMERCIAL_DOCS) {
    const absolutePath = path.resolve(releaseRoot, relativePath);
    try {
      const stat = await fs.stat(absolutePath);
      if (!stat.isFile() || stat.size === 0) {
        phase4Errors.push(`Commercial document invalid or empty: ${relativePath}`);
      }
    } catch {
      phase4Errors.push(`Commercial document missing: ${relativePath}`);
    }
  }
  const phase4: PhaseResult = {
    phase: 4,
    name: 'Hardening & Persistence',
    passed: phase4Errors.length === 0,
    checks: ['Commercial governance documents are present and non-empty'],
    errors: phase4Errors,
  };

  const phase5Errors: string[] = [];
  for (const relativePath of REQUIRED_COMMERCIAL_DOCS) {
    const absolutePath = path.resolve(releaseRoot, relativePath);
    try {
      if (await hasUncheckedChecklist(absolutePath)) {
        phase5Errors.push(`Open checklist items remain in ${relativePath}`);
      }
    } catch {
      // Missing files are already reported in phase 4.
    }
  }
  const phase5: PhaseResult = {
    phase: 5,
    name: 'Billing Integration',
    passed: phase5Errors.length === 0,
    checks: ['Commercial checklists are resolved (no unchecked markdown checkboxes)'],
    errors: phase5Errors,
  };

  const phases = [phase1, phase2, phase3, phase4, phase5];
  const phase6: PhaseResult = {
    phase: 6,
    name: 'Compliance & Ops',
    passed: phases.every((phase) => phase.passed),
    checks: ['All previous phases pass as precondition for beta sign-off'],
    errors: phases.filter((phase) => !phase.passed).map((phase) => `Blocked by phase ${phase.phase}: ${phase.name}`),
  };

  const allPhases = [...phases, phase6];

  return {
    generatedAtUtc: new Date().toISOString(),
    releaseRoot,
    evidencePath,
    overallPassed: allPhases.every((phase) => phase.passed),
    phases: allPhases,
  };
}
