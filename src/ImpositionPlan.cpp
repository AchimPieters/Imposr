#include "aimp/ImpositionPlan.h"

namespace aimp {

ImpositionPlan TwoUpPlanner::Build(const std::string& sourceDocumentId,
                                   std::uint32_t pageCount,
                                   const SheetSize& outputSheet) {
    ImpositionPlan plan {};
    plan.mode = LayoutMode::TwoUp;
    plan.outputSheet = outputSheet;

    if (pageCount == 0 || outputSheet.widthPoints <= 0.0 || outputSheet.heightPoints <= 0.0) {
        return plan;
    }

    const double halfWidth = outputSheet.widthPoints / 2.0;
    const double fullHeight = outputSheet.heightPoints;

    std::uint32_t sheetIndex = 0;
    std::uint32_t slotIndex = 0;

    for (std::uint32_t page = 0; page < pageCount; ++page) {
        SlotPlacement placement {};
        placement.sheetIndex = sheetIndex;
        placement.slotIndex = slotIndex;
        placement.sourcePage = PageRef {sourceDocumentId, page};
        placement.targetRect = Rect {
            slotIndex == 0 ? 0.0 : halfWidth,
            0.0,
            halfWidth,
            fullHeight
        };
        placement.rotationDegrees = 0.0;
        placement.scale = 1.0;

        plan.placements.push_back(placement);

        ++slotIndex;
        if (slotIndex >= 2) {
            slotIndex = 0;
            ++sheetIndex;
        }
    }

    return plan;
}

} // namespace aimp
