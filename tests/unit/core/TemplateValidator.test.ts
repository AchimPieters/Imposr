import { TemplateValidator } from '../../../src/core/templates/TemplateValidator';

describe('TemplateValidator', () => {
  const validator = new TemplateValidator();

  it('validates a correct template', () => {
    const template = validator.validate({
      id: 'booklet-8',
      name: 'Booklet 8',
      mode: 'booklet',
      layout: {
        columns: 2,
        rows: 1,
        gap: 0,
        margin: 10,
        sheetWidth: 595,
        sheetHeight: 842,
      },
    });

    expect(template.id).toBe('booklet-8');
  });

  it('throws on invalid template', () => {
    expect(() =>
      validator.validate({
        id: '',
        mode: 'x',
      })
    ).toThrow();
  });

  it('throws when template id contains path characters', () => {
    expect(() =>
      validator.validate({
        id: '../escape',
        name: 'Escape',
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
    ).toThrow();
  });
});
