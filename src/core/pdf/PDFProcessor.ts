import { PDFDocument } from 'pdf-lib';
import { PDFLoader } from './PDFLoader';
import { PDFExporter } from './PDFExporter';
import { PDFValidationOptions, PDFValidationResult, PDFValidator } from './PDFValidator';

/**
 * Main PDF processing facade for loading, validating and exporting PDF content.
 */
export class PDFProcessor {
  constructor(
    private readonly loader: PDFLoader = new PDFLoader(),
    private readonly validator: PDFValidator = new PDFValidator(),
    private readonly exporter: PDFExporter = new PDFExporter(loader)
  ) {}

  /**
   * Loads a PDF document from disk.
   */
  async load(filePath: string): Promise<PDFDocument> {
    return this.loader.loadFromFile(filePath);
  }

  /**
   * Validates a PDF file against optional constraints.
   */
  async validate(filePath: string, options: PDFValidationOptions = {}): Promise<PDFValidationResult> {
    return this.validator.validate(filePath, options);
  }

  /**
   * Merges multiple PDF files into one output file.
   */
  async merge(inputFiles: string[], outputFile: string): Promise<void> {
    await this.exporter.mergeFiles(inputFiles, outputFile);
  }
}

export type { PDFValidationOptions, PDFValidationResult } from './PDFValidator';
