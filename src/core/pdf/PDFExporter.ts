import fs from 'node:fs/promises';
import { PDFDocument } from 'pdf-lib';
import { PDFProcessingError } from '@utils/errors';
import { logger } from '@utils/logger';
import { PDFLoader } from './PDFLoader';

/**
 * Exports and merges PDF documents.
 */
export class PDFExporter {
  constructor(private readonly loader: PDFLoader = new PDFLoader()) {}

  /**
   * Merges multiple PDF files into one output PDF file.
   * @throws PDFProcessingError If any source fails to load or write fails.
   */
  async mergeFiles(inputFiles: string[], outputFile: string): Promise<void> {
    if (inputFiles.length === 0) {
      throw new PDFProcessingError('No input files provided for merge', { outputFile });
    }

    try {
      const target = await PDFDocument.create();

      for (const inputFile of inputFiles) {
        const source = await this.loader.loadFromFile(inputFile);
        const copiedPages = await target.copyPages(source, source.getPageIndices());
        copiedPages.forEach((page) => target.addPage(page));
      }

      const bytes = await target.save();
      await fs.writeFile(outputFile, bytes);
      logger.info('PDF merge completed', { outputFile, pages: target.getPageCount() });
    } catch (error) {
      throw new PDFProcessingError(error instanceof Error ? error.message : 'Unknown merge error', {
        inputFiles,
        outputFile,
      });
    }
  }

  /**
   * Writes a PDFDocument instance to disk.
   */
  async writeDocument(document: PDFDocument, outputFile: string): Promise<void> {
    try {
      const bytes = await document.save();
      await fs.writeFile(outputFile, bytes);
    } catch (error) {
      throw new PDFProcessingError(
        error instanceof Error ? error.message : 'Unknown PDF write error',
        { outputFile }
      );
    }
  }
}
