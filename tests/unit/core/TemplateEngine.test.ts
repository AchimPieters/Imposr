import { TemplateEngine } from '../../../src/core/templates/TemplateEngine';

describe('TemplateEngine', () => {
  const engine = new TemplateEngine();

  const template = {
    id: 'seq-2up',
    name: 'Sequential 2-up',
    mode: 'sequential' as const,
    layout: {
      columns: 2,
      rows: 1,
      gap: 0,
      margin: 0,
      sheetWidth: 400,
      sheetHeight: 200,
    },
  };

  it('builds plan from template', () => {
    const plan = engine.applyTemplate(template, 4);
    expect(plan).toHaveLength(2);
    expect(plan[0].placements[0].assignment.pageNumber).toBe(1);
  });

  it('throws on invalid page count', () => {
    expect(() => engine.applyTemplate(template, 0)).toThrow();
  });

  it('wraps internal engine errors as TemplateError', () => {
    expect(() =>
      engine.applyTemplate(
        {
          id: 'bad-booklet',
          name: 'Bad Booklet',
          mode: 'booklet',
          layout: {
            columns: 2,
            rows: 2,
            gap: 0,
            margin: 0,
            sheetWidth: 400,
            sheetHeight: 400,
          },
        },
        8
      )
    ).toThrow();
  });
});
