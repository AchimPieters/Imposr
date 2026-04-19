import { existsSync } from 'node:fs';
import fs from 'node:fs/promises';
import { PDFDocument } from 'pdf-lib';
import { logger } from '@utils/logger';
import {
  PDFLoadError,
  PDFProcessingError,
  PDFValidationError,
} from '@utils/errors';

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
 * Main PDF processing service for loading, validating and exporting PDF content.
 */
export class PDFProcessor {
  /**
   * Loads a PDF document from disk.
   * @throws PDFLoadError When file is missing, unreadable or invalid.
   */
  async load(filePath: string): Promise<PDFDocument> {
    try {
      if (!existsSync(filePath)) {
        throw new PDFLoadError('PDF file does not exist', filePath);
      }

      const bytes = await fs.readFile(filePath);
      const doc = await PDFDocument.load(bytes);
      logger.info('PDF loaded', { filePath, pages: doc.getPageCount() });
      return doc;
    } catch (error) {
      if (error instanceof PDFLoadError) {
        throw error;
      }

      const message = error instanceof Error ? error.message : 'Unknown error while loading PDF';
      logger.error('Failed to load PDF', error instanceof Error ? error : undefined, { filePath });
      throw new PDFLoadError(message, filePath);
    }
  }

  /**
   * Validates a PDF file against optional constraints.
   * @throws PDFValidationError If document cannot be inspected.
   */
  async validate(filePath: string, options: PDFValidationOptions = {}): Promise<PDFValidationResult> {
    try {
      const stats = await fs.stat(filePath);
      const fileSizeBytes = stats.size;
      const bytes = await fs.readFile(filePath);
      const encrypted = await this.isEncrypted(bytes);
      const doc = await PDFDocument.load(bytes, { ignoreEncryption: true });
      const pageCount = doc.getPageCount();
      const pageSizes = doc.getPages().map((page) => page.getSize());
      const errors: string[] = [];

      if (options.minPages !== undefined && pageCount < options.minPages) {
        errors.push(`Page count ${pageCount} is below minimum ${options.minPages}`);
      }
      if (options.maxPages !== undefined && pageCount > options.maxPages) {
        errors.push(`Page count ${pageCount} exceeds maximum ${options.maxPages}`);
      }
      if (options.maxFileSizeBytes !== undefined && fileSizeBytes > options.maxFileSizeBytes) {
        errors.push(
          `File size ${fileSizeBytes} exceeds maximum ${options.maxFileSizeBytes} bytes`
        );
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
      const message = error instanceof Error ? error.message : 'Unknown validation error';
      throw new PDFValidationError(message, { filePath, options });
    }
  }

  /**
   * Merges multiple PDF files into one output file.
   * @throws PDFProcessingError If merge fails.
   */
  async merge(inputFiles: string[], outputFile: string): Promise<void> {
    if (inputFiles.length === 0) {
      throw new PDFProcessingError('No input files provided for merge', { outputFile });
    }

    try {
      const target = await PDFDocument.create();

      for (const inputFile of inputFiles) {
        const source = await this.load(inputFile);
        const sourcePages = await target.copyPages(source, source.getPageIndices());
        sourcePages.forEach((page) => target.addPage(page));
      }

      const bytes = await target.save();
      await fs.writeFile(outputFile, bytes);
      logger.info('PDF merge completed', { inputFiles, outputFile, pages: target.getPageCount() });
    } catch (error) {
      const message = error instanceof Error ? error.message : 'Unknown merge error';
      logger.error('PDF merge failed', error instanceof Error ? error : undefined, {
        inputFiles,
        outputFile,
      });
      throw new PDFProcessingError(message, { inputFiles, outputFile });
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
