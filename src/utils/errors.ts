/**
 * Base error class for Imposr application.
 */
export class ImposrError extends Error {
  public code: string;
  public details: Record<string, unknown>;
  public isRecoverable: boolean;

  /**
   * @param message Human-readable error message.
   * @param code Stable machine-readable error code.
   * @param details Additional structured context.
   * @param isRecoverable Whether the user can retry the operation.
   */
  constructor(
    message: string,
    code: string,
    details: Record<string, unknown> = {},
    isRecoverable = false
  ) {
    super(message);
    this.name = 'ImposrError';
    this.code = code;
    this.details = details;
    this.isRecoverable = isRecoverable;
    Error.captureStackTrace?.(this, this.constructor);
  }

  /**
   * Serializes error to JSON-safe object.
   */
  toJSON(): Record<string, unknown> {
    return {
      name: this.name,
      message: this.message,
      code: this.code,
      details: this.details,
      isRecoverable: this.isRecoverable,
      stack: this.stack,
    };
  }
}

/** Error related to PDF operations. */
export class PDFError extends ImposrError {
  constructor(message: string, details: Record<string, unknown> = {}) {
    super(message, 'PDF_ERROR', details);
    this.name = 'PDFError';
  }
}

/** Error thrown when a PDF cannot be loaded. */
export class PDFLoadError extends PDFError {
  constructor(message: string, filePath: string, details: Record<string, unknown> = {}) {
    super(message, { filePath, ...details });
    this.code = 'PDF_LOAD_ERROR';
    this.name = 'PDFLoadError';
  }
}

/** Error thrown when PDF validation fails. */
export class PDFValidationError extends PDFError {
  constructor(message: string, details: Record<string, unknown> = {}) {
    super(message, details);
    this.code = 'PDF_VALIDATION_ERROR';
    this.name = 'PDFValidationError';
  }
}

/** Error thrown when PDF processing fails mid-workflow. */
export class PDFProcessingError extends PDFError {
  constructor(message: string, details: Record<string, unknown> = {}) {
    super(message, details);
    this.code = 'PDF_PROCESSING_ERROR';
    this.name = 'PDFProcessingError';
    this.isRecoverable = true;
  }
}

/** Error thrown for template parsing/validation failures. */
export class TemplateError extends ImposrError {
  constructor(message: string, details: Record<string, unknown> = {}) {
    super(message, 'TEMPLATE_ERROR', details);
    this.name = 'TemplateError';
  }
}

/** Error thrown for license validation/access failures. */
export class LicenseError extends ImposrError {
  constructor(message: string, details: Record<string, unknown> = {}) {
    super(message, 'LICENSE_ERROR', details, false);
    this.name = 'LicenseError';
  }
}

/** Error thrown when a feature is blocked by license tier. */
export class FeatureNotAvailableError extends LicenseError {
  constructor(feature: string, requiredTier: string) {
    super(`Feature "${feature}" requires ${requiredTier} license`, {
      feature,
      requiredTier,
    });
    this.code = 'FEATURE_NOT_AVAILABLE';
    this.name = 'FeatureNotAvailableError';
  }
}

/** Error thrown for batch processing failures. */
export class BatchProcessingError extends ImposrError {
  constructor(message: string, jobId: string, details: Record<string, unknown> = {}) {
    super(message, 'BATCH_PROCESSING_ERROR', { jobId, ...details }, true);
    this.name = 'BatchProcessingError';
  }
}

/** Error thrown for network-bound operation failures. */
export class NetworkError extends ImposrError {
  constructor(message: string, details: Record<string, unknown> = {}) {
    super(message, 'NETWORK_ERROR', details, true);
    this.name = 'NetworkError';
  }
}
