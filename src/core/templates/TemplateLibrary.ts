import fs from 'node:fs/promises';
import path from 'node:path';
import { logger } from '@utils/logger';
import { TemplateError } from '@utils/errors';
import { ImpositionTemplate, TemplateValidator } from './TemplateValidator';

/**
 * Repository for loading/saving template JSON files.
 */
export class TemplateLibrary {
  private readonly validator = new TemplateValidator();

  constructor(private readonly rootDir: string) {}

  /**
   * List template filenames in repository root.
   */
  async list(): Promise<string[]> {
    try {
      const entries = await fs.readdir(this.rootDir);
      return entries.filter((entry) => entry.endsWith('.json')).sort();
    } catch (error) {
      throw new TemplateError('Failed to list templates', {
        rootDir: this.rootDir,
        reason: error instanceof Error ? error.message : 'unknown',
      });
    }
  }

  /**
   * Load a template by file name.
   */
  async load(fileName: string): Promise<ImpositionTemplate> {
    if (!fileName.endsWith('.json')) {
      throw new TemplateError('Template file must end with .json', { fileName });
    }
    this.assertSafeFileName(fileName);

    const fullPath = path.join(this.rootDir, fileName);
    try {
      const content = await fs.readFile(fullPath, 'utf8');
      const parsed = JSON.parse(content) as unknown;
      const validated = this.validator.validate(parsed);
      return validated;
    } catch (error) {
      throw new TemplateError('Failed to load template', {
        fileName,
        reason: error instanceof Error ? error.message : 'unknown',
      });
    }
  }

  /**
   * Save template to disk after validation.
   */
  async save(template: ImpositionTemplate): Promise<string> {
    const validated = this.validator.validate(template);
    const fileName = `${validated.id}.json`;
    this.assertSafeFileName(fileName);
    const fullPath = path.join(this.rootDir, fileName);

    try {
      await fs.mkdir(this.rootDir, { recursive: true });
      await fs.writeFile(fullPath, `${JSON.stringify(validated, null, 2)}\n`, 'utf8');
      logger.info('Template saved', { fileName, rootDir: this.rootDir });
      return fileName;
    } catch (error) {
      throw new TemplateError('Failed to save template', {
        fileName,
        reason: error instanceof Error ? error.message : 'unknown',
      });
    }
  }

  private assertSafeFileName(fileName: string): void {
    if (path.basename(fileName) !== fileName || /[\\/]/.test(fileName)) {
      throw new TemplateError('Template file name must not contain path segments', { fileName });
    }
  }
}
