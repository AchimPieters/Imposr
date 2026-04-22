import { parseBetaPrepareOptions } from '../../../src/cli/beta-prepare';

describe('cli/beta-prepare', () => {
  it('parses defaults', () => {
    const options = parseBetaPrepareOptions(['node', 'beta-prepare']);
    expect(options.evidence).toBe('docs/sdk_smoke_evidence.json');
    expect(options.report).toBe('docs/BETA_READINESS_REPORT.json');
    expect(options.runCoverage).toBe(false);
  });

  it('parses explicit values', () => {
    const options = parseBetaPrepareOptions([
      'node',
      'beta-prepare',
      '--release-root',
      '/tmp/repo',
      '--evidence',
      'docs/custom.json',
      '--report',
      'reports/beta.json',
      '--coverage',
    ]);

    expect(options.releaseRoot).toBe('/tmp/repo');
    expect(options.evidence).toBe('docs/custom.json');
    expect(options.report).toBe('reports/beta.json');
    expect(options.runCoverage).toBe(true);
  });
});
