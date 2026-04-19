import { createHmac, timingSafeEqual } from 'node:crypto';
import { LicenseError } from '@utils/errors';
import { LicenseManager, LicensePayload } from './LicenseManager';

/** Payload in offline activation response token. */
export interface OfflineActivationPayload {
  licenseId: string;
  machineFingerprint: string;
  activatedAt: string;
  expiresAt: string;
}

/** Result of offline activation validation. */
export interface OfflineValidationResult {
  valid: boolean;
  reason?: string;
  payload?: OfflineActivationPayload;
  license?: LicensePayload;
}

/** Constructor options for offline validation. */
export interface OfflineValidatorOptions {
  activationSecret: string;
  licenseManager: LicenseManager;
  now?: () => Date;
}

/**
 * Validates signed offline activation tokens bound to a machine fingerprint.
 */
export class OfflineValidator {
  private readonly activationSecret: string;
  private readonly licenseManager: LicenseManager;
  private readonly now: () => Date;

  /**
   * @param options Runtime dependencies and secrets.
   */
  constructor(options: OfflineValidatorOptions) {
    if (!options.activationSecret || options.activationSecret.trim().length < 16) {
      throw new LicenseError('Activation secret must be provided and at least 16 characters long');
    }

    this.activationSecret = options.activationSecret;
    this.licenseManager = options.licenseManager;
    this.now = options.now ?? (() => new Date());
  }

  /**
   * Signs an offline activation payload for distribution.
   * @param payload Activation payload.
   */
  signToken(payload: OfflineActivationPayload): string {
    this.validateActivationPayload(payload);

    const encodedPayload = Buffer.from(JSON.stringify(payload), 'utf8').toString('base64url');
    const signature = this.computeSignature(encodedPayload);
    return `${encodedPayload}.${signature}`;
  }

  /**
   * Validates license key + offline token + machine binding.
   * @param licenseKey Serialized commercial license key.
   * @param offlineToken Signed offline activation token.
   * @param machineFingerprint Current machine fingerprint hash.
   */
  validate(licenseKey: string, offlineToken: string, machineFingerprint: string): OfflineValidationResult {
    try {
      const licenseInfo = this.licenseManager.verify(licenseKey);
      if (licenseInfo.status !== 'valid' || !licenseInfo.payload) {
        return { valid: false, reason: `License is ${licenseInfo.status}` };
      }

      const payload = this.verifyToken(offlineToken);
      this.validateActivationPayload(payload);

      if (payload.licenseId !== licenseInfo.payload.id) {
        return { valid: false, reason: 'Offline token does not belong to this license' };
      }

      if (payload.machineFingerprint !== machineFingerprint) {
        return { valid: false, reason: 'Offline token is bound to another machine' };
      }

      const now = this.now().getTime();
      const tokenExpiry = new Date(payload.expiresAt).getTime();
      if (now > tokenExpiry) {
        return { valid: false, reason: `Offline token expired at ${payload.expiresAt}` };
      }

      return { valid: true, payload, license: licenseInfo.payload };
    } catch (error) {
      return {
        valid: false,
        reason: error instanceof Error ? error.message : 'Unknown offline validation error',
      };
    }
  }

  private verifyToken(token: string): OfflineActivationPayload {
    const [encodedPayload, receivedSignature] = token.split('.');
    if (!encodedPayload || !receivedSignature) {
      throw new LicenseError('Offline token format is invalid');
    }

    if (!/^[a-f0-9]{64}$/i.test(receivedSignature)) {
      throw new LicenseError('Offline token signature format is invalid');
    }

    const expectedSignature = this.computeSignature(encodedPayload);
    const expectedBuffer = Buffer.from(expectedSignature, 'hex');
    const receivedBuffer = Buffer.from(receivedSignature, 'hex');

    if (!timingSafeEqual(expectedBuffer, receivedBuffer)) {
      throw new LicenseError('Offline token signature is invalid');
    }

    try {
      const json = Buffer.from(encodedPayload, 'base64url').toString('utf8');
      return JSON.parse(json) as OfflineActivationPayload;
    } catch (error) {
      throw new LicenseError('Offline token payload cannot be decoded', {
        cause: error instanceof Error ? error.message : 'unknown',
      });
    }
  }

  private computeSignature(encodedPayload: string): string {
    return createHmac('sha256', this.activationSecret).update(encodedPayload, 'utf8').digest('hex');
  }

  private validateActivationPayload(payload: OfflineActivationPayload): void {
    if (!payload.licenseId || payload.licenseId.trim().length === 0) {
      throw new LicenseError('Offline activation payload licenseId is required');
    }

    if (!/^[a-f0-9]{64}$/i.test(payload.machineFingerprint)) {
      throw new LicenseError('Offline activation payload machineFingerprint must be sha256 hex');
    }

    const activatedAt = new Date(payload.activatedAt);
    const expiresAt = new Date(payload.expiresAt);
    if (Number.isNaN(activatedAt.getTime()) || Number.isNaN(expiresAt.getTime())) {
      throw new LicenseError('Offline activation payload contains invalid date values');
    }

    if (activatedAt.getTime() >= expiresAt.getTime()) {
      throw new LicenseError('Offline activation payload expiresAt must be after activatedAt');
    }
  }
}
