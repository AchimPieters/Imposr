#include "aimp/ImpositionPlan.h"

#include <algorithm>
#include <cstddef>
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
                pages.push_back(kBlankPageIndex);
            }
        }
    }

    return pages;
}

std::uint32_t NormalizeSignatureSize(std::uint32_t signatureSize) {
    if (signatureSize == 0) {
        return 0;
    }
    if (signatureSize < 4u) {
        return 4u;
    }
    const std::uint32_t remainder = signatureSize % 4u;
    if (remainder == 0u) {
        return signatureSize;
    }
    return signatureSize + (4u - remainder);
}

std::string EscapeXml(const std::string& input) {
    std::string out;
    out.reserve(input.size());
    for (char ch : input) {
        switch (ch) {
            case '&': out += "&amp;"; break;
            case '<': out += "&lt;"; break;
            case '>': out += "&gt;"; break;
            case '\'': out += "&apos;"; break;
            case '"': out += "&quot;"; break;
            default: out += ch; break;
        }
    }
    return out;
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
        if (sourcePageIndex == kBlankPageIndex) {
            placement.sourcePage = PageRef {"", kBlankPageIndex};
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
        if (sourcePageIndex == kBlankPageIndex) {
            placement.sourcePage = PageRef {"", kBlankPageIndex};
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

    const std::uint32_t normalizedSignatureSize = NormalizeSignatureSize(options.bookletSignatureSize);

    const double halfWidth = outputSheet.widthPoints / 2.0;
    const double fullHeight = outputSheet.heightPoints;

    auto appendPlacement = [&](std::uint32_t sheetIndex,
                               std::uint32_t slotIndex,
                               std::uint32_t sourcePageIndex) {

        SlotPlacement placement {};
        placement.sheetIndex = sheetIndex;
        placement.slotIndex = slotIndex;
        if (sourcePageIndex == kBlankPageIndex) {
            placement.sourcePage = PageRef {"", kBlankPageIndex};
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

    std::size_t sourcePos = 0;
    std::uint32_t sheetIndexOffset = 0;
    while (sourcePos < sourcePages.size()) {
        std::vector<std::uint32_t> signaturePages;
        std::uint32_t signaturePaddedPageCount = 0;

        if (normalizedSignatureSize == 0) {
            signaturePages.assign(sourcePages.begin() + static_cast<std::ptrdiff_t>(sourcePos), sourcePages.end());
            signaturePaddedPageCount = static_cast<std::uint32_t>(((signaturePages.size() + 3u) / 4u) * 4u);
            sourcePos = sourcePages.size();
        } else {
            const std::size_t signatureSourceCount = std::min(
                static_cast<std::size_t>(normalizedSignatureSize),
                sourcePages.size() - sourcePos);
            signaturePages.assign(sourcePages.begin() + static_cast<std::ptrdiff_t>(sourcePos),
                                  sourcePages.begin() + static_cast<std::ptrdiff_t>(sourcePos + signatureSourceCount));
            signaturePaddedPageCount = normalizedSignatureSize;
            sourcePos += signatureSourceCount;
        }

        while (signaturePages.size() < signaturePaddedPageCount) {
            signaturePages.push_back(kBlankPageIndex);
        }
        plan.paddedPageCount += signaturePaddedPageCount;

        const std::uint32_t signatureSheetCount = signaturePaddedPageCount / 4u;
        for (std::uint32_t signatureSheet = 0; signatureSheet < signatureSheetCount; ++signatureSheet) {
            const std::uint32_t frontLeft = signaturePaddedPageCount - 1u - (2u * signatureSheet);
            const std::uint32_t frontRight = 2u * signatureSheet;
            const std::uint32_t backLeft = 2u * signatureSheet + 1u;
            const std::uint32_t backRight = signaturePaddedPageCount - 2u - (2u * signatureSheet);

            appendPlacement(sheetIndexOffset + signatureSheet * 2u, 0u, signaturePages[frontLeft]);
            appendPlacement(sheetIndexOffset + signatureSheet * 2u, 1u, signaturePages[frontRight]);
            appendPlacement(sheetIndexOffset + signatureSheet * 2u + 1u, 0u, signaturePages[backLeft]);
            appendPlacement(sheetIndexOffset + signatureSheet * 2u + 1u, 1u, signaturePages[backRight]);
        }
        sheetIndexOffset += signatureSheetCount * 2u;
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
                if (sourcePageIndex == kBlankPageIndex) {
                    placement.sourcePage = PageRef {"", kBlankPageIndex};
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

std::string ToAuditXml(const ImpositionPlan& plan) {
    std::ostringstream out;
    out << "<imposition-plan mode=\"" << LayoutModeName(plan.mode) << "\" "
        << "sourcePageCount=\"" << plan.sourcePageCount << "\" "
        << "paddedPageCount=\"" << plan.paddedPageCount << "\">\n";
    out << "  <output-sheet widthPoints=\"" << plan.outputSheet.widthPoints
        << "\" heightPoints=\"" << plan.outputSheet.heightPoints << "\" />\n";
    out << "  <placements>\n";

    for (const auto& p : plan.placements) {
        out << "    <placement sheetIndex=\"" << p.sheetIndex
            << "\" slotIndex=\"" << p.slotIndex
            << "\" rotationDegrees=\"" << p.rotationDegrees
            << "\" scale=\"" << p.scale << "\">\n";
        out << "      <source documentId=\"" << EscapeXml(p.sourcePage.sourceDocumentId)
            << "\" pageIndex=\"" << p.sourcePage.pageIndex << "\" />\n";
        out << "      <targetRect x=\"" << p.targetRect.x
            << "\" y=\"" << p.targetRect.y
            << "\" width=\"" << p.targetRect.width
            << "\" height=\"" << p.targetRect.height << "\" />\n";
        out << "    </placement>\n";
    }

    out << "  </placements>\n";
    out << "</imposition-plan>\n";
    return out.str();
}

std::vector<PlacementRef> FindPlacementsForSourcePage(const ImpositionPlan& plan,
                                                      const std::string& sourceDocumentId,
                                                      std::uint32_t pageIndex) {
    std::vector<PlacementRef> matches;
    for (const auto& placement : plan.placements) {
        if (placement.sourcePage.sourceDocumentId == sourceDocumentId &&
            placement.sourcePage.pageIndex == pageIndex) {
            matches.push_back(PlacementRef {placement.sheetIndex, placement.slotIndex});
        }
    }
    return matches;
}

bool TryGetSourceForPlacement(const ImpositionPlan& plan,
                              std::uint32_t sheetIndex,
                              std::uint32_t slotIndex,
                              PageRef& outSourcePage) {
    for (const auto& placement : plan.placements) {
        if (placement.sheetIndex == sheetIndex && placement.slotIndex == slotIndex) {
            outSourcePage = placement.sourcePage;
            return true;
        }
    }
    return false;
}

} // namespace aimp
