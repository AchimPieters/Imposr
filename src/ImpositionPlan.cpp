#include "aimp/ImpositionPlan.h"

#include <sstream>

namespace aimp {

namespace {

bool IsInvalidSheet(const SheetSize& outputSheet) {
    return outputSheet.widthPoints <= 0.0 || outputSheet.heightPoints <= 0.0;
}

bool IsRectOutsideSheet(const Rect& rect, const SheetSize& sheet) {
    return rect.x < 0.0 || rect.y < 0.0 ||
           rect.x + rect.width > sheet.widthPoints ||
           rect.y + rect.height > sheet.heightPoints;
}

std::string EscapeJson(const std::string& input) {
    std::string out;
    out.reserve(input.size());

    for (char ch : input) {
        switch (ch) {
            case '\\': out += "\\\\"; break;
            case '\"': out += "\\\""; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default: out += ch; break;
        }
    }
    return out;
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

ImpositionPlan StepAndRepeatPlanner::Build(const std::string& sourceDocumentId,
                                           std::uint32_t pageCount,
                                           const SheetSize& outputSheet,
                                           const StepRepeatConfig& config) {
    ImpositionPlan plan {};
    plan.mode = LayoutMode::StepAndRepeat;
    plan.outputSheet = outputSheet;
    plan.sourcePageCount = pageCount;
    plan.paddedPageCount = pageCount;

    if (pageCount == 0 || IsInvalidSheet(outputSheet) ||
        config.repeatX == 0 || config.repeatY == 0 ||
        config.seedRect.width <= 0.0 || config.seedRect.height <= 0.0) {
        return plan;
    }

    std::uint32_t sourcePage = 0;
    for (std::uint32_t sheetIndex = 0; sourcePage < pageCount; ++sheetIndex) {
        bool anyPlacementOnSheet = false;
        for (std::uint32_t y = 0; y < config.repeatY && sourcePage < pageCount; ++y) {
            for (std::uint32_t x = 0; x < config.repeatX && sourcePage < pageCount; ++x) {
                Rect target {
                    config.seedRect.x + static_cast<double>(x) * config.stepXPoints,
                    config.seedRect.y + static_cast<double>(y) * config.stepYPoints,
                    config.seedRect.width,
                    config.seedRect.height
                };

                if (IsRectOutsideSheet(target, outputSheet)) {
                    continue;
                }

                SlotPlacement placement {};
                placement.sheetIndex = sheetIndex;
                placement.slotIndex = y * config.repeatX + x;
                placement.sourcePage = PageRef {sourceDocumentId, sourcePage};
                placement.targetRect = target;
                plan.placements.push_back(placement);
                anyPlacementOnSheet = true;
                ++sourcePage;
            }
        }
        if (!anyPlacementOnSheet) {
            break;
        }
    }

    return plan;
}

const char* LayoutModeName(LayoutMode mode) {
    switch (mode) {
        case LayoutMode::TwoUp: return "two-up";
        case LayoutMode::Booklet: return "booklet";
        case LayoutMode::NUp: return "n-up";
        case LayoutMode::StepAndRepeat: return "step-and-repeat";
        case LayoutMode::Manual: return "manual";
        case LayoutMode::Tile: return "tile";
        default: return "unknown";
    }
}

std::string ToJson(const ImpositionPlan& plan) {
    std::ostringstream out;
    out << "{\n";
    out << "  \"mode\": \"" << LayoutModeName(plan.mode) << "\",\n";
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
            << ", \"source\": {\"documentId\": \"" << EscapeJson(p.sourcePage.sourceDocumentId)
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
