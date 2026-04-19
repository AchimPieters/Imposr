import fs from 'node:fs/promises';
import os from 'node:os';
import path from 'node:path';
import { TemplateLibrary } from '../../../src/core/templates/TemplateLibrary';

describe('TemplateLibrary', () => {
  it('saves, lists and loads templates', async () => {
    const dir = await fs.mkdtemp(path.join(os.tmpdir(), 'imposr-templates-'));
    const library = new TemplateLibrary(dir);

    const fileName = await library.save({
      id: 't-1',
      name: 'Template 1',
      mode: 'sequential',
      layout: {
        columns: 2,
        rows: 1,
        gap: 0,
        margin: 0,
        sheetWidth: 500,
        sheetHeight: 700,
      },
    });

    expect(fileName).toBe('t-1.json');
    const list = await library.list();
    expect(list).toContain('t-1.json');

    const loaded = await library.load('t-1.json');
    expect(loaded.name).toBe('Template 1');
  });

  it('throws on non-json load request', async () => {
    const dir = await fs.mkdtemp(path.join(os.tmpdir(), 'imposr-templates-'));
    const library = new TemplateLibrary(dir);
    await expect(library.load('bad.txt')).rejects.toBeTruthy();
  });

  it('throws on path traversal in load request', async () => {
    const dir = await fs.mkdtemp(path.join(os.tmpdir(), 'imposr-templates-'));
    const library = new TemplateLibrary(dir);
    await expect(library.load('../bad.json')).rejects.toBeTruthy();
  });

  it('throws when listing templates from a non-directory path', async () => {
    const dir = await fs.mkdtemp(path.join(os.tmpdir(), 'imposr-templates-'));
    const filePath = path.join(dir, 'file-as-root');
    await fs.writeFile(filePath, 'x', 'utf8');
    const library = new TemplateLibrary(filePath);
    await expect(library.list()).rejects.toBeTruthy();
  });

  it('throws when loading invalid JSON template', async () => {
    const dir = await fs.mkdtemp(path.join(os.tmpdir(), 'imposr-templates-'));
    await fs.writeFile(path.join(dir, 'invalid.json'), '{ not-valid-json', 'utf8');
    const library = new TemplateLibrary(dir);
    await expect(library.load('invalid.json')).rejects.toBeTruthy();
  });

  it('throws when loading missing template file', async () => {
    const dir = await fs.mkdtemp(path.join(os.tmpdir(), 'imposr-templates-'));
    const library = new TemplateLibrary(dir);
    await expect(library.load('missing.json')).rejects.toBeTruthy();
  });

  it('throws when saving to invalid root path', async () => {
    const dir = await fs.mkdtemp(path.join(os.tmpdir(), 'imposr-templates-'));
    const filePath = path.join(dir, 'as-file');
    await fs.writeFile(filePath, 'x', 'utf8');
    const library = new TemplateLibrary(filePath);
    await expect(
      library.save({
        id: 't-err',
        name: 'Template Err',
        mode: 'sequential',
        layout: {
          columns: 1,
          rows: 1,
          gap: 0,
          margin: 0,
          sheetWidth: 100,
          sheetHeight: 100,
        },
      })
    ).rejects.toBeTruthy();
  });
});
