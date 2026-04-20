import fs from 'node:fs/promises';
import path from 'node:path';

describe('OpenAPI contract', () => {
  it('documents all commercial licensing and webhook endpoints', async () => {
    const filePath = path.join(process.cwd(), 'docs/api-reference/openapi.yaml');
    const content = await fs.readFile(filePath, 'utf8');

    expect(content).toContain('openapi: 3.0.3');
    expect(content).toContain('/api/license/verify:');
    expect(content).toContain('/api/license/offline/validate:');
    expect(content).toContain('/api/license/audit:');
    expect(content).toContain('/api/webhooks/licensing:');
    expect(content).toContain('/api/webhooks/trial:');
    expect(content).toContain('/api/webhooks/paid:');
  });

  it('documents required request schemas', async () => {
    const filePath = path.join(process.cwd(), 'docs/api-reference/openapi.yaml');
    const content = await fs.readFile(filePath, 'utf8');

    expect(content).toContain('OfflineValidationBody');
    expect(content).toContain('TrialIssueBody');
    expect(content).toContain('PaidIssueBody');
    expect(content).toContain('LicensingWebhookBody');
  });
});
