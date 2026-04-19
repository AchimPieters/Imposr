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
}

/** Result returned by validation. */
export interface PDFValidationResult {
  valid: boolean;
  pageCount: number;
  fileSizeBytes: number;
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
      const doc = await this.load(filePath);
      const pageCount = doc.getPageCount();
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
        errors.push('Encrypted PDF requirement is not implemented in current engine');
      }

      return {
        valid: errors.length === 0,
        pageCount,
        fileSizeBytes,
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
}
