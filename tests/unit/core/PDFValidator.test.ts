import fs from 'node:fs/promises';
import os from 'node:os';
import path from 'node:path';
import { PDFDocument } from 'pdf-lib';
import { PDFValidator } from '../../../src/core/pdf/PDFValidator';
import { PDFValidationError } from '../../../src/utils/errors';

describe('PDFValidator', () => {
  const validator = new PDFValidator();

  async function createPdf(filePath: string, sizes: Array<[number, number]>): Promise<void> {
    const doc = await PDFDocument.create();
    sizes.forEach((size) => doc.addPage(size));
    await fs.writeFile(filePath, await doc.save());
  }

  it('validates healthy PDF and PDF/X profile constraints', async () => {
    const dir = await fs.mkdtemp(path.join(os.tmpdir(), 'imposr-validator-'));
    const file = path.join(dir, 'ok.pdf');
    await createPdf(file, [
      [300, 400],
      [300, 400],
    ]);

    const result = await validator.validate(file, { minPages: 1, pdfxProfile: 'pdfx-1a' });
    expect(result.valid).toBe(true);
    expect(result.errors).toHaveLength(0);
  });

  it('returns validation errors for violated constraints', async () => {
    const dir = await fs.mkdtemp(path.join(os.tmpdir(), 'imposr-validator-'));
    const file = path.join(dir, 'bad-rules.pdf');
    await createPdf(file, [[300, 400]]);

    const result = await validator.validate(file, {
      minPages: 2,
      maxPages: 0,
      maxFileSizeBytes: 10,
      requireEncrypted: true,
    });

    expect(result.valid).toBe(false);
    expect(result.errors.length).toBeGreaterThanOrEqual(3);
  });

  it('adds encrypted and consistency errors when preflight requires them', async () => {
    const dir = await fs.mkdtemp(path.join(os.tmpdir(), 'imposr-validator-'));
    const file = path.join(dir, 'mixed.pdf');
    await createPdf(file, [
      [300, 400],
      [320, 400],
    ]);

    const encryptedSpy = jest.spyOn(validator as any, 'isEncrypted').mockResolvedValue(true);
    const result = await validator.validate(file, {
      pdfxProfile: 'pdfx-4',
      requireConsistentPageSizes: true,
    });
    encryptedSpy.mockRestore();

    expect(result.valid).toBe(false);
    expect(result.errors).toContain('Encrypted PDFs are not supported for processing');
    expect(result.errors).toContain('PDF pages must all have identical dimensions');
    expect(result.errors).toContain('PDF/X profile pdfx-4 does not allow encrypted input');
  });

  it('fails PDF/X-1a with mixed page sizes', async () => {
    const dir = await fs.mkdtemp(path.join(os.tmpdir(), 'imposr-validator-'));
    const file = path.join(dir, 'mixed.pdf');
    await createPdf(file, [
      [300, 400],
      [320, 400],
    ]);

    const result = await validator.validate(file, { pdfxProfile: 'pdfx-1a' });
    expect(result.valid).toBe(false);
    expect(result.errors).toContain('PDF/X-1a preflight requires consistent page dimensions');
  });

  it('throws when file cannot be read', async () => {
    await expect(validator.validate('/tmp/not-here.pdf')).rejects.toBeInstanceOf(PDFValidationError);
  });

  it('rethrows non-encryption parse errors from isEncrypted', async () => {
    const loadSpy = jest.spyOn(PDFDocument, 'load').mockRejectedValueOnce(new Error('parse failed'));
    await expect((validator as any).isEncrypted(new Uint8Array([1, 2, 3]))).rejects.toThrow('parse failed');
    loadSpy.mockRestore();
  });
});
