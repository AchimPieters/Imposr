#include "aimp/ImpositionPlan.h"

#include <sstream>

namespace aimp {

namespace {

bool IsInvalidSheet(const SheetSize& outputSheet) {
    return outputSheet.widthPoints <= 0.0 || outputSheet.heightPoints <= 0.0;
}

}

ImpositionPlan TwoUpPlanner::Build(const std::string& sourceDocumentId,
                                   std::uint32_t pageCount,
                                   const SheetSize& outputSheet) {
    ImpositionPlan plan {};
    plan.mode = LayoutMode::TwoUp;
    plan.outputSheet = outputSheet;
    plan.sourcePageCount = pageCount;
    plan.paddedPageCount = pageCount;

    if (pageCount == 0 || IsInvalidSheet(outputSheet)) {
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

ImpositionPlan NUpPlanner::Build(const std::string& sourceDocumentId,
                                 std::uint32_t pageCount,
                                 const SheetSize& outputSheet,
                                 std::uint32_t columns,
                                 std::uint32_t rows) {
    ImpositionPlan plan {};
    plan.mode = LayoutMode::NUp;
    plan.outputSheet = outputSheet;
    plan.sourcePageCount = pageCount;
    plan.paddedPageCount = pageCount;

    if (pageCount == 0 || IsInvalidSheet(outputSheet) || columns == 0 || rows == 0) {
        return plan;
    }

    const double slotWidth = outputSheet.widthPoints / static_cast<double>(columns);
    const double slotHeight = outputSheet.heightPoints / static_cast<double>(rows);
    const std::uint32_t slotsPerSheet = columns * rows;

    for (std::uint32_t page = 0; page < pageCount; ++page) {
        const std::uint32_t sheetIndex = page / slotsPerSheet;
        const std::uint32_t slotIndex = page % slotsPerSheet;
        const std::uint32_t col = slotIndex % columns;
        const std::uint32_t row = slotIndex / columns;

        SlotPlacement placement {};
        placement.sheetIndex = sheetIndex;
        placement.slotIndex = slotIndex;
        placement.sourcePage = PageRef {sourceDocumentId, page};
        placement.targetRect = Rect {
            slotWidth * static_cast<double>(col),
            slotHeight * static_cast<double>(row),
            slotWidth,
            slotHeight
        };

        plan.placements.push_back(placement);
    }

    return plan;
}

ImpositionPlan BookletPlanner::Build(const std::string& sourceDocumentId,
                                     std::uint32_t pageCount,
                                     const SheetSize& outputSheet) {
    ImpositionPlan plan {};
    plan.mode = LayoutMode::Booklet;
    plan.outputSheet = outputSheet;
    plan.sourcePageCount = pageCount;

    if (pageCount == 0 || IsInvalidSheet(outputSheet)) {
        return plan;
    }

    const std::uint32_t padded = ((pageCount + 3u) / 4u) * 4u;
    plan.paddedPageCount = padded;

    const double halfWidth = outputSheet.widthPoints / 2.0;
    const double fullHeight = outputSheet.heightPoints;

    auto appendPlacement = [&](std::uint32_t sheetIndex,
                               std::uint32_t slotIndex,
                               std::uint32_t pageNumber) {
        if (pageNumber >= pageCount) {
            return;
        }

        SlotPlacement placement {};
        placement.sheetIndex = sheetIndex;
        placement.slotIndex = slotIndex;
        placement.sourcePage = PageRef {sourceDocumentId, pageNumber};
        placement.targetRect = Rect {
            slotIndex == 0 ? 0.0 : halfWidth,
            0.0,
            halfWidth,
            fullHeight
        };
        plan.placements.push_back(placement);
    };

    const std::uint32_t sheetCount = padded / 4u;

    for (std::uint32_t sheet = 0; sheet < sheetCount; ++sheet) {
        const std::uint32_t frontLeft = padded - 1u - (2u * sheet);
        const std::uint32_t frontRight = 2u * sheet;
        const std::uint32_t backLeft = 2u * sheet + 1u;
        const std::uint32_t backRight = padded - 2u - (2u * sheet);

        appendPlacement(sheet * 2u, 0u, frontLeft);
        appendPlacement(sheet * 2u, 1u, frontRight);
        appendPlacement(sheet * 2u + 1u, 0u, backLeft);
        appendPlacement(sheet * 2u + 1u, 1u, backRight);
    }

    return plan;
}

std::string ToJson(const ImpositionPlan& plan) {
    std::ostringstream out;
    out << "{\n";
    out << "  \"mode\": " << static_cast<int>(plan.mode) << ",\n";
    out << "  \"sourcePageCount\": " << plan.sourcePageCount << ",\n";
    out << "  \"paddedPageCount\": " << plan.paddedPageCount << ",\n";
    out << "  \"outputSheet\": {\"widthPoints\": " << plan.outputSheet.widthPoints
        << ", \"heightPoints\": " << plan.outputSheet.heightPoints << "},\n";
    out << "  \"placements\": [\n";

    for (std::size_t i = 0; i < plan.placements.size(); ++i) {
        const auto& p = plan.placements[i];
        out << "    {"
            << "\"sheetIndex\": " << p.sheetIndex
            << ", \"slotIndex\": " << p.slotIndex
            << ", \"source\": {\"documentId\": \"" << p.sourcePage.sourceDocumentId
            << "\", \"pageIndex\": " << p.sourcePage.pageIndex << "}"
            << ", \"targetRect\": {\"x\": " << p.targetRect.x
            << ", \"y\": " << p.targetRect.y
            << ", \"width\": " << p.targetRect.width
            << ", \"height\": " << p.targetRect.height << "}"
            << ", \"rotationDegrees\": " << p.rotationDegrees
            << ", \"scale\": " << p.scale
            << "}";

        if (i + 1 != plan.placements.size()) {
            out << ",";
        }
        out << "\n";
    }

    out << "  ]\n";
    out << "}\n";
    return out.str();
}

} // namespace aimp
