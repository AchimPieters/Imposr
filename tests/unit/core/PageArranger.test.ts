import { PageArranger } from '../../../src/core/imposition/PageArranger';

describe('PageArranger', () => {
  const arranger = new PageArranger();

  it('arranges sequential pages with null padding', () => {
    const sheets = arranger.arrange(5, 4, 'sequential');
    expect(sheets).toHaveLength(2);
    expect(sheets[1][1].pageNumber).toBe(null);
  });

  it('arranges booklet with 4-page padding', () => {
    const sheets = arranger.arrange(6, 2, 'booklet');
    expect(sheets[0]).toEqual([
      { slotIndex: 0, pageNumber: null },
      { slotIndex: 1, pageNumber: 1 },
    ]);
  });

  it('throws for invalid input', () => {
    expect(() => arranger.arrange(0, 2, 'sequential')).toThrow();
    expect(() => arranger.arrange(2, 0, 'sequential')).toThrow();
  });
});
