import { LayoutCalculator } from '../../../src/core/imposition/LayoutCalculator';

describe('LayoutCalculator', () => {
  const calculator = new LayoutCalculator();

  it('calculates slots for 2x2 grid', () => {
    const slots = calculator.calculateSlots({
      sheet: { x: 0, y: 0, width: 400, height: 400 },
      columns: 2,
      rows: 2,
      gap: 0,
      margin: 0,
    });
    expect(slots).toHaveLength(4);
    expect(slots[0].rect.width).toBe(200);
    expect(slots[0].rect.height).toBe(200);
  });

  it('throws on invalid grid', () => {
    expect(() =>
      calculator.calculateSlots({
        sheet: { x: 0, y: 0, width: 400, height: 400 },
        columns: 0,
        rows: 2,
        gap: 0,
        margin: 0,
      })
    ).toThrow();
  });
});
