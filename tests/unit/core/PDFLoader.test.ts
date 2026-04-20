import fs from 'node:fs/promises';
import os from 'node:os';
import path from 'node:path';
import { PDFDocument } from 'pdf-lib';
import { PDFLoader } from '../../../src/core/pdf/PDFLoader';
import { PDFLoadError } from '../../../src/utils/errors';

describe('PDFLoader', () => {
  const loader = new PDFLoader();

  async function createPdf(filePath: string, pageCount = 1): Promise<void> {
    const doc = await PDFDocument.create();
    for (let index = 0; index < pageCount; index += 1) {
      doc.addPage([300, 400]);
    }
    await fs.writeFile(filePath, await doc.save());
  }

  it('loads pdf from file and bytes', async () => {
    const dir = await fs.mkdtemp(path.join(os.tmpdir(), 'imposr-loader-'));
    const file = path.join(dir, 'a.pdf');
    await createPdf(file, 2);

    const loaded = await loader.loadFromFile(file);
    expect(loaded.getPageCount()).toBe(2);

    const bytes = await fs.readFile(file);
    const byBytes = await loader.loadFromBytes(bytes, 'bytes-source');
    expect(byBytes.getPageCount()).toBe(2);
  });

  it('throws on missing or invalid file', async () => {
    await expect(loader.loadFromFile('/tmp/not-found.pdf')).rejects.toBeInstanceOf(PDFLoadError);

    const dir = await fs.mkdtemp(path.join(os.tmpdir(), 'imposr-loader-'));
    const file = path.join(dir, 'bad.pdf');
    await fs.writeFile(file, Buffer.from('broken'));
    await expect(loader.loadFromFile(file)).rejects.toBeInstanceOf(PDFLoadError);
  });

  it('maps non-Error parse exceptions to generic message', async () => {
    const spy = jest.spyOn(PDFDocument, 'load').mockRejectedValueOnce('broken');
    await expect(loader.loadFromBytes(new Uint8Array([1, 2, 3]), 'bad-bytes')).rejects.toMatchObject({
      message: 'Unknown error while parsing PDF bytes',
    });
    spy.mockRestore();
  });
});
