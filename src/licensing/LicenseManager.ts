import { createHmac, timingSafeEqual } from 'node:crypto';
import { LicenseError } from '@utils/errors';

/** Supported commercial license tiers. */
export type LicenseTier = 'trial' | 'starter' | 'pro' | 'enterprise';

/** Status of a loaded license. */
export type LicenseStatus = 'valid' | 'expired' | 'invalid' | 'missing';

/** Canonical payload contained in a signed license key. */
export interface LicensePayload {
  id: string;
  tier: LicenseTier;
  issuedAt: string;
  expiresAt: string;
  customerEmail: string;
  maxActivations: number;
  features: string[];
}

/** Runtime license object returned by verification. */
export interface LicenseInfo {
  status: LicenseStatus;
  payload: LicensePayload | null;
  reason?: string;
}

/** Dependencies for cryptographic verification and date handling. */
export interface LicenseManagerOptions {
  signingSecret: string;
  now?: () => Date;
}

/**
 * Manages parsing and verification of signed commercial license keys.
 *
 * Format: base64url(JSON payload).hex(HMAC-SHA256(payload))
 */
export class LicenseManager {
  private readonly signingSecret: string;
  private readonly now: () => Date;

  /**
   * @param options Runtime options used for license validation.
   * @throws LicenseError If required options are missing.
   */
  constructor(options: LicenseManagerOptions) {
    if (!options.signingSecret || options.signingSecret.trim().length < 16) {
      throw new LicenseError('Signing secret must be provided and at least 16 characters long');
    }

    this.signingSecret = options.signingSecret;
    this.now = options.now ?? (() => new Date());
  }

  /**
   * Validates and decodes a serialized license key.
   * @param licenseKey Signed license key string.
   * @returns Structured license validation result.
   */
  verify(licenseKey: string | null | undefined): LicenseInfo {
    if (!licenseKey || licenseKey.trim().length === 0) {
      return { status: 'missing', payload: null, reason: 'No license key provided' };
    }

    try {
      const [payloadSegment, signatureSegment] = this.splitLicenseKey(licenseKey);
      this.assertValidSignature(payloadSegment, signatureSegment);

      const payload = this.parsePayload(payloadSegment);
      this.validatePayload(payload);

      const expiresAt = new Date(payload.expiresAt);
      if (this.now().getTime() > expiresAt.getTime()) {
        return {
          status: 'expired',
          payload,
          reason: `License expired at ${payload.expiresAt}`,
        };
      }

      return { status: 'valid', payload };
    } catch (error) {
      const reason = error instanceof Error ? error.message : 'Unknown license parsing error';
      return { status: 'invalid', payload: null, reason };
    }
  }

  /**
   * Creates a signed license key from a payload.
   * @param payload License payload.
   * @returns Serialized signed key.
   */
  sign(payload: LicensePayload): string {
    this.validatePayload(payload);

    const encodedPayload = Buffer.from(JSON.stringify(payload), 'utf8').toString('base64url');
    const signature = this.computeSignature(encodedPayload);

    return `${encodedPayload}.${signature}`;
  }

  /**
   * Throws when the provided license key is not valid.
   * @param licenseKey Signed key.
   */
  assertValid(licenseKey: string): LicensePayload {
    const result = this.verify(licenseKey);
    if (result.status !== 'valid' || !result.payload) {
      throw new LicenseError(result.reason ?? 'Invalid license key', { status: result.status });
    }

    return result.payload;
  }

  private splitLicenseKey(licenseKey: string): [string, string] {
    const parts = licenseKey.split('.');
    if (parts.length !== 2 || !parts[0] || !parts[1]) {
      throw new LicenseError('License key format is invalid');
    }

    return [parts[0], parts[1]];
  }

  private assertValidSignature(encodedPayload: string, receivedSignature: string): void {
    if (!/^[a-f0-9]{64}$/i.test(receivedSignature)) {
      throw new LicenseError('License signature format is invalid');
    }

    const expectedSignature = this.computeSignature(encodedPayload);
    const expectedBuffer = Buffer.from(expectedSignature, 'hex');
    const receivedBuffer = Buffer.from(receivedSignature, 'hex');

    if (!timingSafeEqual(expectedBuffer, receivedBuffer)) {
      throw new LicenseError('License signature is invalid');
    }
  }

  private computeSignature(encodedPayload: string): string {
    return createHmac('sha256', this.signingSecret).update(encodedPayload, 'utf8').digest('hex');
  }

  private parsePayload(encodedPayload: string): LicensePayload {
    try {
      const json = Buffer.from(encodedPayload, 'base64url').toString('utf8');
      return JSON.parse(json) as LicensePayload;
    } catch (error) {
      throw new LicenseError('License payload cannot be decoded', {
        cause: error instanceof Error ? error.message : 'unknown',
      });
    }
  }

  private validatePayload(payload: LicensePayload): void {
    if (!payload.id || payload.id.trim().length === 0) {
      throw new LicenseError('License payload id is required');
    }

    if (!['trial', 'starter', 'pro', 'enterprise'].includes(payload.tier)) {
      throw new LicenseError('License payload tier is invalid', { tier: payload.tier });
    }

    if (!payload.customerEmail.includes('@')) {
      throw new LicenseError('License payload customerEmail is invalid');
    }

    if (!Number.isInteger(payload.maxActivations) || payload.maxActivations <= 0) {
      throw new LicenseError('License payload maxActivations must be a positive integer');
    }

    const issuedAt = new Date(payload.issuedAt);
    const expiresAt = new Date(payload.expiresAt);

    if (Number.isNaN(issuedAt.getTime()) || Number.isNaN(expiresAt.getTime())) {
      throw new LicenseError('License payload contains invalid date values');
    }

    if (issuedAt.getTime() >= expiresAt.getTime()) {
      throw new LicenseError('License payload expiresAt must be after issuedAt');
    }

    if (!Array.isArray(payload.features) || payload.features.some((value) => value.trim().length === 0)) {
      throw new LicenseError('License payload features must contain non-empty strings');
    }
  }
}
