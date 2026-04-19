import fs from 'node:fs/promises';
import os from 'node:os';
import path from 'node:path';
import { PDFDocument } from 'pdf-lib';
import { PDFProcessor } from '../../../src/core/pdf/PDFProcessor';
import { PDFLoadError, PDFProcessingError } from '../../../src/utils/errors';

describe('PDFProcessor', () => {
  const processor = new PDFProcessor();

  async function createPdf(
    filePath: string,
    pages = 1,
    sizes: Array<[number, number]> = []
  ): Promise<void> {
    const doc = await PDFDocument.create();
    for (let i = 0; i < pages; i += 1) {
      doc.addPage(sizes[i] ?? [300, 400]);
    }
    const bytes = await doc.save();
    await fs.writeFile(filePath, bytes);
  }

  it('loads and validates a pdf', async () => {
    const dir = await fs.mkdtemp(path.join(os.tmpdir(), 'imposr-pdf-'));
    const file = path.join(dir, 'a.pdf');
    await createPdf(file, 2);

    const doc = await processor.load(file);
    expect(doc.getPageCount()).toBe(2);

    const result = await processor.validate(file, { minPages: 1, maxPages: 10 });
    expect(result.valid).toBe(true);
    expect(result.pageCount).toBe(2);
    expect(result.encrypted).toBe(false);
    expect(result.pageSizes).toHaveLength(2);
  });

  it('merges files', async () => {
    const dir = await fs.mkdtemp(path.join(os.tmpdir(), 'imposr-pdf-'));
    const a = path.join(dir, 'a.pdf');
    const b = path.join(dir, 'b.pdf');
    const out = path.join(dir, 'out.pdf');
    await createPdf(a, 1);
    await createPdf(b, 2);

    await processor.merge([a, b], out);
    const merged = await processor.load(out);
    expect(merged.getPageCount()).toBe(3);
  });

  it('fails on missing file', async () => {
    await expect(processor.load('/tmp/does-not-exist.pdf')).rejects.toBeInstanceOf(PDFLoadError);
  });

  it('fails on invalid pdf bytes', async () => {
    const dir = await fs.mkdtemp(path.join(os.tmpdir(), 'imposr-pdf-'));
    const file = path.join(dir, 'broken.pdf');
    await fs.writeFile(file, Buffer.from('not-a-real-pdf'));
    await expect(processor.load(file)).rejects.toBeInstanceOf(PDFLoadError);
  });

  it('returns validation errors when constraints are violated', async () => {
    const dir = await fs.mkdtemp(path.join(os.tmpdir(), 'imposr-pdf-'));
    const file = path.join(dir, 'small.pdf');
    await createPdf(file, 1);
    const result = await processor.validate(file, {
      minPages: 2,
      maxPages: 0,
      maxFileSizeBytes: 10,
      requireEncrypted: true,
    });
    expect(result.valid).toBe(false);
    expect(result.errors.length).toBeGreaterThanOrEqual(3);
    expect(result.errors).toContain('PDF must be encrypted');
  });

  it('fails merge when no input files are provided', async () => {
    await expect(processor.merge([], '/tmp/out.pdf')).rejects.toBeInstanceOf(PDFProcessingError);
  });

  it('fails merge when one source is invalid', async () => {
    const dir = await fs.mkdtemp(path.join(os.tmpdir(), 'imposr-pdf-'));
    const bad = path.join(dir, 'bad.pdf');
    await fs.writeFile(bad, Buffer.from('broken'));
    await expect(processor.merge([bad], path.join(dir, 'out.pdf'))).rejects.toBeInstanceOf(
      PDFProcessingError
    );
  });

  it('fails validate when file does not exist', async () => {
    await expect(processor.validate('/tmp/not-here.pdf')).rejects.toBeTruthy();
  });

  it('fails PDF/X-1a preflight when page sizes differ', async () => {
    const dir = await fs.mkdtemp(path.join(os.tmpdir(), 'imposr-pdf-'));
    const file = path.join(dir, 'mixed.pdf');
    await createPdf(file, 2, [
      [300, 400],
      [320, 400],
    ]);

    const result = await processor.validate(file, { pdfxProfile: 'pdfx-1a' });
    expect(result.valid).toBe(false);
    expect(result.errors).toContain('PDF/X-1a preflight requires consistent page dimensions');
  });

  it('accepts PDF/X-4 preflight with mixed page sizes', async () => {
    const dir = await fs.mkdtemp(path.join(os.tmpdir(), 'imposr-pdf-'));
    const file = path.join(dir, 'mixed-x4.pdf');
    await createPdf(file, 2, [
      [300, 400],
      [320, 400],
    ]);

    const result = await processor.validate(file, { pdfxProfile: 'pdfx-4' });
    expect(result.valid).toBe(true);
  });
});
