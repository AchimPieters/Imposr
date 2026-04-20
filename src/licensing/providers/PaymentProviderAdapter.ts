import { createHmac, timingSafeEqual } from 'node:crypto';
import { NetworkError } from '@utils/errors';
import { PaymentWebhookEvent } from '../PaymentHandler';

/**
 * Adapter contract for provider-specific webhook signature parsing/verification.
 */
export interface PaymentProviderAdapter {
  readonly provider: 'generic' | 'stripe' | 'paddle';
  verifyAndParse(
    rawBody: string,
    signatureHeader: string,
    secret: string,
    now: () => Date,
    toleranceMs: number
  ): PaymentWebhookEvent;
}

export class GenericHmacAdapter implements PaymentProviderAdapter {
  readonly provider = 'generic' as const;

  verifyAndParse(
    rawBody: string,
    signatureHeader: string,
    secret: string,
    _now: () => Date,
    _toleranceMs: number
  ): PaymentWebhookEvent {
    if (!signatureHeader || !/^[a-f0-9]{64}$/i.test(signatureHeader)) {
      throw new NetworkError('Webhook signature format is invalid');
    }

    const expectedSignature = createHmac('sha256', secret).update(rawBody, 'utf8').digest('hex');
    const expectedBuffer = Buffer.from(expectedSignature, 'hex');
    const receivedBuffer = Buffer.from(signatureHeader, 'hex');

    if (!timingSafeEqual(expectedBuffer, receivedBuffer)) {
      throw new NetworkError('Webhook signature mismatch');
    }

    return parseEvent(rawBody);
  }
}

export class StripeAdapter implements PaymentProviderAdapter {
  readonly provider = 'stripe' as const;

  verifyAndParse(
    rawBody: string,
    signatureHeader: string,
    secret: string,
    now: () => Date,
    toleranceMs: number
  ): PaymentWebhookEvent {
    const parsed = parsePairs(signatureHeader, ',', '=');
    const timestamp = Number(parsed.t);
    const signature = parsed.v1;

    if (!Number.isFinite(timestamp) || !signature || !/^[a-f0-9]{64}$/i.test(signature)) {
      throw new NetworkError('Stripe signature header is invalid');
    }

    enforceTimestampTolerance(timestamp, now, toleranceMs, 'stripe');

    const signedPayload = `${timestamp}.${rawBody}`;
    const expectedSignature = createHmac('sha256', secret).update(signedPayload, 'utf8').digest('hex');
    compareSignature(expectedSignature, signature, 'Stripe signature mismatch');

    return parseEvent(rawBody);
  }
}

export class PaddleAdapter implements PaymentProviderAdapter {
  readonly provider = 'paddle' as const;

  verifyAndParse(
    rawBody: string,
    signatureHeader: string,
    secret: string,
    now: () => Date,
    toleranceMs: number
  ): PaymentWebhookEvent {
    const parsed = parsePairs(signatureHeader, ';', '=');
    const timestamp = Number(parsed.ts);
    const signature = parsed.h1;

    if (!Number.isFinite(timestamp) || !signature || !/^[a-f0-9]{64}$/i.test(signature)) {
      throw new NetworkError('Paddle signature header is invalid');
    }

    enforceTimestampTolerance(timestamp, now, toleranceMs, 'paddle');

    const signedPayload = `${timestamp}:${rawBody}`;
    const expectedSignature = createHmac('sha256', secret).update(signedPayload, 'utf8').digest('hex');
    compareSignature(expectedSignature, signature, 'Paddle signature mismatch');

    return parseEvent(rawBody);
  }
}

export function createProviderAdapter(provider: string | undefined): PaymentProviderAdapter {
  if (!provider || provider === 'generic') {
    return new GenericHmacAdapter();
  }
  if (provider === 'stripe') {
    return new StripeAdapter();
  }
  if (provider === 'paddle') {
    return new PaddleAdapter();
  }

  throw new NetworkError('Unsupported payment provider', { provider });
}

function parseEvent(rawBody: string): PaymentWebhookEvent {
  try {
    return JSON.parse(rawBody) as PaymentWebhookEvent;
  } catch (error) {
    throw new NetworkError('Webhook payload is not valid JSON', {
      cause: error instanceof Error ? error.message : 'unknown',
    });
  }
}

function parsePairs(value: string, fieldSeparator: string, pairSeparator: string): Record<string, string> {
  return value
    .split(fieldSeparator)
    .map((part) => part.trim())
    .filter(Boolean)
    .reduce<Record<string, string>>((acc, part) => {
      const [key, val] = part.split(pairSeparator);
      if (key && val) {
        acc[key.trim()] = val.trim();
      }
      return acc;
    }, {});
}

function enforceTimestampTolerance(
  timestampSeconds: number,
  now: () => Date,
  toleranceMs: number,
  provider: string
): void {
  const eventTimestampMs = timestampSeconds * 1000;
  const drift = Math.abs(now().getTime() - eventTimestampMs);
  if (drift > toleranceMs) {
    throw new NetworkError(`${provider} webhook signature timestamp outside tolerance`, {
      driftMs: drift,
      toleranceMs,
    });
  }
}

function compareSignature(expectedHex: string, receivedHex: string, message: string): void {
  const expected = Buffer.from(expectedHex, 'hex');
  const received = Buffer.from(receivedHex, 'hex');
  if (!timingSafeEqual(expected, received)) {
    throw new NetworkError(message);
  }
}
