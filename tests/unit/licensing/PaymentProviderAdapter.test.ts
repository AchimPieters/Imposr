import { createHmac } from 'node:crypto';
import {
  GenericHmacAdapter,
  PaddleAdapter,
  StripeAdapter,
} from '../../../src/licensing/providers/PaymentProviderAdapter';

describe('PaymentProviderAdapter', () => {
  const body = JSON.stringify({
    id: 'evt_1',
    type: 'license.issued',
    createdAt: '2026-04-20T12:00:00.000Z',
    data: { licenseKey: 'key' },
  });
  const secret = 'payment-provider-secret-123456';
  const now = () => new Date('2026-04-20T12:00:30.000Z');

  it('verifies generic hmac signature', () => {
    const adapter = new GenericHmacAdapter();
    const signature = createHmac('sha256', secret).update(body, 'utf8').digest('hex');
    expect(adapter.verifyAndParse(body, signature, secret, now, 60_000).id).toBe('evt_1');
  });

  it('verifies stripe-style signatures with timestamp tolerance', () => {
    const adapter = new StripeAdapter();
    const timestamp = Math.floor(now().getTime() / 1000);
    const signature = createHmac('sha256', secret)
      .update(`${timestamp}.${body}`, 'utf8')
      .digest('hex');
    const header = `t=${timestamp},v1=${signature}`;
    expect(adapter.verifyAndParse(body, header, secret, now, 60_000).id).toBe('evt_1');
  });

  it('verifies paddle-style signatures with timestamp tolerance', () => {
    const adapter = new PaddleAdapter();
    const timestamp = Math.floor(now().getTime() / 1000);
    const signature = createHmac('sha256', secret)
      .update(`${timestamp}:${body}`, 'utf8')
      .digest('hex');
    const header = `ts=${timestamp};h1=${signature}`;
    expect(adapter.verifyAndParse(body, header, secret, now, 60_000).id).toBe('evt_1');
  });
});
