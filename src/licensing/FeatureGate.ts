import { FeatureNotAvailableError } from '@utils/errors';
import { LicenseInfo, LicenseTier } from './LicenseManager';

/** Declarative feature requirements keyed by feature identifier. */
export type FeatureMatrix = Record<string, LicenseTier>;

/**
 * Centralized access control for commercial features.
 */
export class FeatureGate {
  private static readonly tierOrder: LicenseTier[] = ['trial', 'starter', 'pro', 'enterprise'];

  private readonly matrix: FeatureMatrix;

  /**
   * @param matrix Required minimum tier per feature.
   */
  constructor(matrix: FeatureMatrix) {
    this.matrix = { ...matrix };
  }

  /**
   * Returns whether a feature may be used for the given license.
   * Checks both tier and explicit feature entitlement list on the license payload.
   * @param feature Feature key.
   * @param licenseInfo Current license info.
   */
  canAccess(feature: string, licenseInfo: LicenseInfo): boolean {
    if (!feature || feature.trim().length === 0) {
      return false;
    }

    if (licenseInfo.status !== 'valid' || !licenseInfo.payload) {
      return false;
    }

    const requiredTier = this.matrix[feature] ?? 'enterprise';
    const actualTier = licenseInfo.payload.tier;
    const tierAllowed = this.compareTiers(actualTier, requiredTier) >= 0;
    const explicitlyEntitled = licenseInfo.payload.features.includes(feature);

    return tierAllowed && explicitlyEntitled;
  }

  /**
   * Ensures access and throws when denied.
   * @param feature Feature key.
   * @param licenseInfo Current license info.
   */
  assertAccess(feature: string, licenseInfo: LicenseInfo): void {
    if (!this.canAccess(feature, licenseInfo)) {
      const requiredTier = this.matrix[feature] ?? 'enterprise';
      throw new FeatureNotAvailableError(feature, requiredTier);
    }
  }

  /**
   * Lists features available for a license.
   * @param licenseInfo Current license info.
   */
  listAvailableFeatures(licenseInfo: LicenseInfo): string[] {
    return Object.keys(this.matrix).filter((feature) => this.canAccess(feature, licenseInfo));
  }

  private compareTiers(actualTier: LicenseTier, requiredTier: LicenseTier): number {
    return FeatureGate.tierOrder.indexOf(actualTier) - FeatureGate.tierOrder.indexOf(requiredTier);
  }
}
