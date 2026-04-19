import { ImpositionEngine } from '../../../src/core/imposition/ImpositionEngine';

describe('ImpositionEngine', () => {
  const engine = new ImpositionEngine();

  it('builds sequential plan', () => {
    const plan = engine.buildPlan({
      pageCount: 4,
      mode: 'sequential',
      layout: {
        sheet: { x: 0, y: 0, width: 400, height: 200 },
        columns: 2,
        rows: 1,
        gap: 0,
        margin: 0,
      },
    });

    expect(plan).toHaveLength(2);
    expect(plan[0].placements[0].assignment.pageNumber).toBe(1);
  });

  it('throws when booklet not 2-up', () => {
    expect(() =>
      engine.buildPlan({
        pageCount: 8,
        mode: 'booklet',
        layout: {
          sheet: { x: 0, y: 0, width: 400, height: 400 },
          columns: 2,
          rows: 2,
          gap: 0,
          margin: 0,
        },
      })
    ).toThrow();
  });
});
