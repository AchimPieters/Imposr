import { ImposrError } from '@utils/errors';

/**
 * Arrangement mode.
 */
export type ArrangeMode = 'sequential' | 'booklet';

/**
 * Page assignment for one side/slot.
 */
export interface PageAssignment {
  slotIndex: number;
  pageNumber: number | null;
}

/**
 * Handles page ordering strategies.
 */
export class PageArranger {
  /**
   * Arrange pages for slot-based placement.
   * @throws ImposrError if mode/input invalid.
   */
  arrange(pageCount: number, slotsPerSheet: number, mode: ArrangeMode): PageAssignment[][] {
    if (pageCount <= 0) {
      throw new ImposrError('Page count must be > 0', 'ARRANGE_INVALID_PAGE_COUNT');
    }
    if (slotsPerSheet <= 0) {
      throw new ImposrError('Slots per sheet must be > 0', 'ARRANGE_INVALID_SLOTS');
    }

    if (mode === 'sequential') {
      return this.arrangeSequential(pageCount, slotsPerSheet);
    }
    if (mode === 'booklet') {
      return this.arrangeBooklet(pageCount);
    }

    throw new ImposrError('Unsupported arrange mode', 'ARRANGE_MODE_UNSUPPORTED', { mode });
  }

  private arrangeSequential(pageCount: number, slotsPerSheet: number): PageAssignment[][] {
    const sheets: PageAssignment[][] = [];
    let page = 1;

    while (page <= pageCount) {
      const sheet: PageAssignment[] = [];
      for (let slot = 0; slot < slotsPerSheet; slot += 1) {
        sheet.push({ slotIndex: slot, pageNumber: page <= pageCount ? page : null });
        page += 1;
      }
      sheets.push(sheet);
    }

    return sheets;
  }

  /**
   * 2-up booklet spread order: [last, first], [second, second-last], ...
   */
  private arrangeBooklet(pageCount: number): PageAssignment[][] {
    const padded = pageCount % 4 === 0 ? pageCount : pageCount + (4 - (pageCount % 4));
    const sheets: PageAssignment[][] = [];

    let left = padded;
    let right = 1;

    while (right < left) {
      sheets.push([
        { slotIndex: 0, pageNumber: left > pageCount ? null : left },
        { slotIndex: 1, pageNumber: right > pageCount ? null : right },
      ]);
      right += 1;
      left -= 1;

      sheets.push([
        { slotIndex: 0, pageNumber: right > pageCount ? null : right },
        { slotIndex: 1, pageNumber: left > pageCount ? null : left },
      ]);
      right += 1;
      left -= 1;
    }

    return sheets;
  }
}
