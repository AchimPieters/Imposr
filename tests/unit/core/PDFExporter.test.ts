import fs from 'node:fs/promises';
import os from 'node:os';
import path from 'node:path';
import { PDFDocument } from 'pdf-lib';
import { PDFExporter } from '../../../src/core/pdf/PDFExporter';
import { PDFLoader } from '../../../src/core/pdf/PDFLoader';
import { PDFProcessingError } from '../../../src/utils/errors';

describe('PDFExporter', () => {
  const loader = new PDFLoader();
  const exporter = new PDFExporter(loader);

  async function createPdf(filePath: string, pages = 1): Promise<void> {
    const doc = await PDFDocument.create();
    for (let index = 0; index < pages; index += 1) {
      doc.addPage([300, 400]);
    }
    await fs.writeFile(filePath, await doc.save());
  }

  it('merges and writes documents', async () => {
    const dir = await fs.mkdtemp(path.join(os.tmpdir(), 'imposr-exporter-'));
    const first = path.join(dir, 'a.pdf');
    const second = path.join(dir, 'b.pdf');
    const mergedFile = path.join(dir, 'merged.pdf');
    const savedFile = path.join(dir, 'saved.pdf');

    await createPdf(first, 1);
    await createPdf(second, 2);

    await exporter.mergeFiles([first, second], mergedFile);
    const merged = await loader.loadFromFile(mergedFile);
    expect(merged.getPageCount()).toBe(3);

    const newDoc = await PDFDocument.create();
    newDoc.addPage([300, 400]);
    await exporter.writeDocument(newDoc, savedFile);
    const saved = await loader.loadFromFile(savedFile);
    expect(saved.getPageCount()).toBe(1);
  });

  it('throws for invalid merge inputs', async () => {
    await expect(exporter.mergeFiles([], '/tmp/out.pdf')).rejects.toBeInstanceOf(PDFProcessingError);

    const dir = await fs.mkdtemp(path.join(os.tmpdir(), 'imposr-exporter-'));
    const broken = path.join(dir, 'broken.pdf');
    await fs.writeFile(broken, Buffer.from('bad'));

    await expect(exporter.mergeFiles([broken], path.join(dir, 'out.pdf'))).rejects.toBeInstanceOf(
      PDFProcessingError
    );
  });

  it('maps non-Error write exceptions to generic message', async () => {
    const doc = await PDFDocument.create();
    doc.addPage([300, 400]);

    const saveSpy = jest.spyOn(doc, 'save').mockRejectedValueOnce('save failed');
    await expect(exporter.writeDocument(doc, '/tmp/out.pdf')).rejects.toMatchObject({
      message: 'Unknown PDF write error',
    });
    saveSpy.mockRestore();
  });

  it('supports default loader constructor path', async () => {
    const dir = await fs.mkdtemp(path.join(os.tmpdir(), 'imposr-exporter-default-'));
    const source = path.join(dir, 'source.pdf');
    const output = path.join(dir, 'out.pdf');
    await createPdf(source, 1);

    const defaultExporter = new PDFExporter();
    await defaultExporter.mergeFiles([source], output);

    const merged = await loader.loadFromFile(output);
    expect(merged.getPageCount()).toBe(1);
  });

  it('preserves Error message when writeDocument fails with Error', async () => {
    const doc = await PDFDocument.create();
    doc.addPage([300, 400]);

    const saveSpy = jest.spyOn(doc, 'save').mockRejectedValueOnce(new Error('disk error'));
    await expect(exporter.writeDocument(doc, '/tmp/out.pdf')).rejects.toMatchObject({
      message: 'disk error',
    });
    saveSpy.mockRestore();
  });

});
