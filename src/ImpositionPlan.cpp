#include "aimp/ImpositionPlan.h"

#include <algorithm>
#include <sstream>
#include <vector>

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

std::vector<std::uint32_t> BuildSourcePages(std::uint32_t pageCount, const BuildOptions& options) {
    std::vector<std::uint32_t> pages;
    pages.reserve(pageCount);

    for (std::uint32_t idx = 0; idx < pageCount; ++idx) {
        const std::uint32_t humanPage = idx + 1;
        if (options.filter == PageFilter::EvenOnly && (humanPage % 2u) != 0u) {
            continue;
        }
        if (options.filter == PageFilter::OddOnly && (humanPage % 2u) == 0u) {
            continue;
        }
        pages.push_back(idx);
    }

    if (options.reverseOrder) {
        std::reverse(pages.begin(), pages.end());
    }

    if (options.padToMultiple > 0) {
        const std::size_t remainder = pages.size() % options.padToMultiple;
        if (remainder != 0) {
            const std::size_t padNeeded = options.padToMultiple - remainder;
            for (std::size_t i = 0; i < padNeeded; ++i) {
                pages.push_back(UINT32_MAX);
            }
        }
    }

    return pages;
}

}

ImpositionPlan TwoUpPlanner::Build(const std::string& sourceDocumentId,
                                   std::uint32_t pageCount,
                                   const SheetSize& outputSheet,
                                   const BuildOptions& options) {
    ImpositionPlan plan {};
    plan.mode = LayoutMode::TwoUp;
    plan.outputSheet = outputSheet;
    plan.sourcePageCount = pageCount;

    if (pageCount == 0 || IsInvalidSheet(outputSheet)) {
        return plan;
    }
    const auto sourcePages = BuildSourcePages(pageCount, options);
    plan.paddedPageCount = static_cast<std::uint32_t>(sourcePages.size());

    const double halfWidth = outputSheet.widthPoints / 2.0;
    const double fullHeight = outputSheet.heightPoints;

    std::uint32_t sheetIndex = 0;
    std::uint32_t slotIndex = 0;

    for (std::uint32_t sourcePageIndex : sourcePages) {
        SlotPlacement placement {};
        placement.sheetIndex = sheetIndex;
        placement.slotIndex = slotIndex;
        if (sourcePageIndex == UINT32_MAX) {
            placement.sourcePage = PageRef {"", UINT32_MAX};
        } else {
            placement.sourcePage = PageRef {sourceDocumentId, sourcePageIndex};
        }
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
                                 std::uint32_t rows,
                                 const BuildOptions& options) {
    ImpositionPlan plan {};
    plan.mode = LayoutMode::NUp;
    plan.outputSheet = outputSheet;
    plan.sourcePageCount = pageCount;

    if (pageCount == 0 || IsInvalidSheet(outputSheet) || columns == 0 || rows == 0) {
        return plan;
    }

    const double slotWidth = outputSheet.widthPoints / static_cast<double>(columns);
    const double slotHeight = outputSheet.heightPoints / static_cast<double>(rows);
    const std::uint32_t slotsPerSheet = columns * rows;

    const auto sourcePages = BuildSourcePages(pageCount, options);
    plan.paddedPageCount = static_cast<std::uint32_t>(sourcePages.size());

    for (std::uint32_t i = 0; i < sourcePages.size(); ++i) {
        const std::uint32_t sourcePageIndex = sourcePages[i];
        const std::uint32_t sheetIndex = i / slotsPerSheet;
        const std::uint32_t slotIndex = i % slotsPerSheet;
        const std::uint32_t col = slotIndex % columns;
        const std::uint32_t row = slotIndex / columns;

        SlotPlacement placement {};
        placement.sheetIndex = sheetIndex;
        placement.slotIndex = slotIndex;
        if (sourcePageIndex == UINT32_MAX) {
            placement.sourcePage = PageRef {"", UINT32_MAX};
        } else {
            placement.sourcePage = PageRef {sourceDocumentId, sourcePageIndex};
        }
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
                                     const SheetSize& outputSheet,
                                     const BuildOptions& options) {
    ImpositionPlan plan {};
    plan.mode = LayoutMode::Booklet;
    plan.outputSheet = outputSheet;
    plan.sourcePageCount = pageCount;

    if (pageCount == 0 || IsInvalidSheet(outputSheet)) {
        return plan;
    }

    auto sourcePages = BuildSourcePages(pageCount, options);
    if (sourcePages.empty()) {
        return plan;
    }

    const std::uint32_t padded = static_cast<std::uint32_t>(((sourcePages.size() + 3u) / 4u) * 4u);
    while (sourcePages.size() < padded) {
        sourcePages.push_back(UINT32_MAX);
    }
    plan.paddedPageCount = padded;

    const double halfWidth = outputSheet.widthPoints / 2.0;
    const double fullHeight = outputSheet.heightPoints;

    auto appendPlacement = [&](std::uint32_t sheetIndex, std::uint32_t slotIndex, std::uint32_t sequenceIndex) {
        const std::uint32_t sourcePageIndex = sourcePages[sequenceIndex];

        SlotPlacement placement {};
        placement.sheetIndex = sheetIndex;
        placement.slotIndex = slotIndex;
        if (sourcePageIndex == UINT32_MAX) {
            placement.sourcePage = PageRef {"", UINT32_MAX};
        } else {
            placement.sourcePage = PageRef {sourceDocumentId, sourcePageIndex};
        }
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
                                           const StepRepeatConfig& config,
                                           const BuildOptions& options) {
    ImpositionPlan plan {};
    plan.mode = LayoutMode::StepAndRepeat;
    plan.outputSheet = outputSheet;
    plan.sourcePageCount = pageCount;

    if (pageCount == 0 || IsInvalidSheet(outputSheet) ||
        config.repeatX == 0 || config.repeatY == 0 ||
        config.seedRect.width <= 0.0 || config.seedRect.height <= 0.0) {
        return plan;
    }

    const auto sourcePages = BuildSourcePages(pageCount, options);
    plan.paddedPageCount = static_cast<std::uint32_t>(sourcePages.size());

    std::size_t sourcePagePos = 0;
    for (std::uint32_t sheetIndex = 0; sourcePagePos < sourcePages.size(); ++sheetIndex) {
        bool anyPlacementOnSheet = false;
        for (std::uint32_t y = 0; y < config.repeatY && sourcePagePos < sourcePages.size(); ++y) {
            for (std::uint32_t x = 0; x < config.repeatX && sourcePagePos < sourcePages.size(); ++x) {
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
                const std::uint32_t sourcePageIndex = sourcePages[sourcePagePos];
                if (sourcePageIndex == UINT32_MAX) {
                    placement.sourcePage = PageRef {"", UINT32_MAX};
                } else {
                    placement.sourcePage = PageRef {sourceDocumentId, sourcePageIndex};
                }
                placement.targetRect = target;
                plan.placements.push_back(placement);
                anyPlacementOnSheet = true;
                ++sourcePagePos;
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
