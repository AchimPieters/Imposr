import {
  BatchProcessingError,
  FeatureNotAvailableError,
  ImposrError,
  LicenseError,
  NetworkError,
  PDFError,
  PDFLoadError,
  PDFProcessingError,
  PDFValidationError,
  TemplateError,
} from '../../../src/utils/errors';

describe('errors', () => {
  it('serializes ImposrError', () => {
    const err = new ImposrError('boom', 'E_TEST', { foo: 'bar' }, true);
    expect(err.toJSON()).toMatchObject({
      message: 'boom',
      code: 'E_TEST',
      details: { foo: 'bar' },
      isRecoverable: true,
    });
  });

  it('builds specialized error types', () => {
    expect(new PDFError('x').code).toBe('PDF_ERROR');
    expect(new PDFLoadError('x', '/tmp/a.pdf').code).toBe('PDF_LOAD_ERROR');
    expect(new PDFValidationError('x').code).toBe('PDF_VALIDATION_ERROR');
    expect(new PDFProcessingError('x').isRecoverable).toBe(true);
    expect(new TemplateError('x').code).toBe('TEMPLATE_ERROR');
    expect(new LicenseError('x').code).toBe('LICENSE_ERROR');
    expect(new FeatureNotAvailableError('booklet', 'pro').code).toBe('FEATURE_NOT_AVAILABLE');
    expect(new BatchProcessingError('x', 'job-1').isRecoverable).toBe(true);
    expect(new NetworkError('x').isRecoverable).toBe(true);
  });
});
