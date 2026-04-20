import fs from 'node:fs/promises';
import { PDFDocument } from 'pdf-lib';
import { PDFValidationError } from '@utils/errors';

/** Supported validation constraints. */
export interface PDFValidationOptions {
  minPages?: number;
  maxPages?: number;
  maxFileSizeBytes?: number;
  requireEncrypted?: boolean;
  pdfxProfile?: 'none' | 'pdfx-1a' | 'pdfx-4';
  requireConsistentPageSizes?: boolean;
}

/** Result returned by validation. */
export interface PDFValidationResult {
  valid: boolean;
  pageCount: number;
  fileSizeBytes: number;
  encrypted: boolean;
  pageSizes: Array<{ width: number; height: number }>;
  errors: string[];
}

/**
 * Validates PDF files against preflight-like constraints.
 */
export class PDFValidator {
  /**
   * Validates a PDF file path against configured options.
   */
  async validate(filePath: string, options: PDFValidationOptions = {}): Promise<PDFValidationResult> {
    try {
      const stats = await fs.stat(filePath);
      const fileSizeBytes = stats.size;
      const bytes = await fs.readFile(filePath);
      const encrypted = await this.isEncrypted(bytes);
      const document = await PDFDocument.load(bytes, { ignoreEncryption: true });
      const pageCount = document.getPageCount();
      const pageSizes = document.getPages().map((page) => page.getSize());
      const errors: string[] = [];

      if (options.minPages !== undefined && pageCount < options.minPages) {
        errors.push(`Page count ${pageCount} is below minimum ${options.minPages}`);
      }

      if (options.maxPages !== undefined && pageCount > options.maxPages) {
        errors.push(`Page count ${pageCount} exceeds maximum ${options.maxPages}`);
      }

      if (options.maxFileSizeBytes !== undefined && fileSizeBytes > options.maxFileSizeBytes) {
        errors.push(`File size ${fileSizeBytes} exceeds maximum ${options.maxFileSizeBytes} bytes`);
      }

      if (options.requireEncrypted === true) {
        if (!encrypted) {
          errors.push('PDF must be encrypted');
        }
      } else if (encrypted) {
        errors.push('Encrypted PDFs are not supported for processing');
      }

      if (options.requireConsistentPageSizes === true && !this.hasConsistentPageSizes(pageSizes)) {
        errors.push('PDF pages must all have identical dimensions');
      }

      const profile = options.pdfxProfile ?? 'none';
      if (profile !== 'none') {
        if (encrypted) {
          errors.push(`PDF/X profile ${profile} does not allow encrypted input`);
        }
        if (profile === 'pdfx-1a' && !this.hasConsistentPageSizes(pageSizes)) {
          errors.push('PDF/X-1a preflight requires consistent page dimensions');
        }
      }

      return {
        valid: errors.length === 0,
        pageCount,
        fileSizeBytes,
        encrypted,
        pageSizes,
        errors,
      };
    } catch (error) {
      throw new PDFValidationError(error instanceof Error ? error.message : 'Unknown validation error', {
        filePath,
        options,
      });
    }
  }

  private async isEncrypted(bytes: Uint8Array): Promise<boolean> {
    try {
      await PDFDocument.load(bytes);
      return false;
    } catch (error) {
      const message = error instanceof Error ? error.message.toLowerCase() : '';
      if (message.includes('encrypted')) {
        return true;
      }
      throw error;
    }
  }

  private hasConsistentPageSizes(pageSizes: Array<{ width: number; height: number }>): boolean {
    if (pageSizes.length <= 1) {
      return true;
    }

    const first = pageSizes[0];
    return pageSizes.every(
      (size) => Math.abs(size.width - first.width) < 0.01 && Math.abs(size.height - first.height) < 0.01
    );
  }
}
