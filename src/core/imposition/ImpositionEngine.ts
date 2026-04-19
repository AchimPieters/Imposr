import { ImposrError } from '@utils/errors';
import { LayoutCalculator, LayoutCalculationInput, SlotLayout } from './LayoutCalculator';
import { ArrangeMode, PageArranger, PageAssignment } from './PageArranger';

/**
 * Input for full imposition planning.
 */
export interface ImpositionInput {
  pageCount: number;
  layout: LayoutCalculationInput;
  mode: ArrangeMode;
}

/**
 * Final placement for one sheet.
 */
export interface SheetPlan {
  sheetIndex: number;
  placements: Array<{
    slot: SlotLayout;
    assignment: PageAssignment;
  }>;
}

/**
 * High-level imposition planner that combines slot math + page arrangement.
 */
export class ImpositionEngine {
  private readonly layoutCalculator = new LayoutCalculator();
  private readonly arranger = new PageArranger();

  /**
   * Build full sheet plans for the requested mode/layout.
   */
  buildPlan(input: ImpositionInput): SheetPlan[] {
    const slots = this.layoutCalculator.calculateSlots(input.layout);

    if (input.mode === 'booklet' && slots.length !== 2) {
      throw new ImposrError('Booklet mode requires exactly 2 slots per sheet', 'BOOKLET_REQUIRES_2UP', {
        slots: slots.length,
      });
    }

    const assignments = this.arranger.arrange(input.pageCount, slots.length, input.mode);

    return assignments.map((sheetAssignments, sheetIndex) => ({
      sheetIndex,
      placements: sheetAssignments.map((assignment) => {
        const slot = slots.find((s) => s.slotIndex === assignment.slotIndex);
        if (!slot) {
          throw new ImposrError('Missing slot mapping', 'SLOT_MAPPING_MISSING', {
            slotIndex: assignment.slotIndex,
          });
        }
        return { slot, assignment };
      }),
    }));
  }
}
