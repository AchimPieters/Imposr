import { PDFProcessor, PDFValidationResult } from '@core/pdf/PDFProcessor';

/** CLI options for PDF validation. */
export interface ValidateCommandOptions {
  file: string;
  minPages?: number;
  maxPages?: number;
  maxFileSizeBytes?: number;
  profile?: 'none' | 'pdfx-1a' | 'pdfx-4';
}

/**
 * Validate a PDF file with optional constraints.
 */
export async function runValidateCommand(options: ValidateCommandOptions): Promise<PDFValidationResult> {
  const processor = new PDFProcessor();
  return processor.validate(options.file, {
    minPages: options.minPages,
    maxPages: options.maxPages,
    maxFileSizeBytes: options.maxFileSizeBytes,
    pdfxProfile: options.profile ?? 'none',
  });
}
