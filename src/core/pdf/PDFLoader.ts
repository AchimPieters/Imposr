import { existsSync } from 'node:fs';
import fs from 'node:fs/promises';
import { PDFDocument } from 'pdf-lib';
import { PDFLoadError } from '@utils/errors';
import { logger } from '@utils/logger';

/**
 * Loads PDF content from disk or memory.
 */
export class PDFLoader {
  /**
   * Loads a PDF document from a file path.
   * @throws PDFLoadError When file does not exist or cannot be parsed.
   */
  async loadFromFile(filePath: string): Promise<PDFDocument> {
    if (!existsSync(filePath)) {
      throw new PDFLoadError('PDF file does not exist', filePath);
    }

    try {
      const bytes = await fs.readFile(filePath);
      return await this.loadFromBytes(bytes, filePath);
    } catch (error) {
      if (error instanceof PDFLoadError) {
        throw error;
      }

      throw new PDFLoadError(
        error instanceof Error ? error.message : 'Unknown error while reading PDF file',
        filePath
      );
    }
  }

  /**
   * Loads a PDF document from raw bytes.
   * @param bytes PDF bytes.
   * @param sourceLabel Optional source label used in logs and errors.
   */
  async loadFromBytes(bytes: Uint8Array, sourceLabel = 'memory'): Promise<PDFDocument> {
    try {
      const document = await PDFDocument.load(bytes);
      logger.info('PDF loaded', { sourceLabel, pages: document.getPageCount() });
      return document;
    } catch (error) {
      throw new PDFLoadError(
        error instanceof Error ? error.message : 'Unknown error while parsing PDF bytes',
        sourceLabel
      );
    }
  }
}
