import { ImposrError } from '@utils/errors';

/**
 * Rectangle in PDF points.
 */
export interface Rect {
  x: number;
  y: number;
  width: number;
  height: number;
}

/**
 * Input configuration for N-up slot calculations.
 */
export interface LayoutCalculationInput {
  sheet: Rect;
  columns: number;
  rows: number;
  gap: number;
  margin: number;
}

/**
 * Slot metadata for placing source pages.
 */
export interface SlotLayout {
  slotIndex: number;
  rect: Rect;
}

/**
 * Computes N-up slot rectangles for one sheet.
 */
export class LayoutCalculator {
  /**
   * Calculate slot positions for a single output sheet.
   * @throws ImposrError if input is invalid.
   */
  calculateSlots(input: LayoutCalculationInput): SlotLayout[] {
    if (input.columns <= 0 || input.rows <= 0) {
      throw new ImposrError('Columns and rows must be > 0', 'LAYOUT_INVALID_GRID');
    }
    if (input.sheet.width <= 0 || input.sheet.height <= 0) {
      throw new ImposrError('Sheet width/height must be > 0', 'LAYOUT_INVALID_SHEET');
    }

    const usableWidth = input.sheet.width - input.margin * 2 - (input.columns - 1) * input.gap;
    const usableHeight = input.sheet.height - input.margin * 2 - (input.rows - 1) * input.gap;

    if (usableWidth <= 0 || usableHeight <= 0) {
      throw new ImposrError('Margins and gaps leave no usable area', 'LAYOUT_NO_USABLE_AREA', {
        usableWidth,
        usableHeight,
      });
    }

    const slotWidth = usableWidth / input.columns;
    const slotHeight = usableHeight / input.rows;

    const slots: SlotLayout[] = [];
    let slotIndex = 0;
    for (let row = 0; row < input.rows; row += 1) {
      for (let col = 0; col < input.columns; col += 1) {
        const x = input.sheet.x + input.margin + col * (slotWidth + input.gap);
        const y = input.sheet.y + input.margin + row * (slotHeight + input.gap);
        slots.push({
          slotIndex,
          rect: { x, y, width: slotWidth, height: slotHeight },
        });
        slotIndex += 1;
      }
    }

    return slots;
  }
}
