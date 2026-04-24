#include "aimp/ImpositionPlan.h"
#include "EscapeUtils.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <limits>
#include <map>
#include <sstream>
#include <set>
#include <utility>
#include <vector>

namespace aimp {

namespace {

using internal::EscapeJson;

bool IsInvalidSheet(const SheetSize& outputSheet) {
    return outputSheet.widthPoints <= 0.0 || outputSheet.heightPoints <= 0.0;
}

bool IsRectOutsideSheet(const Rect& rect, const SheetSize& sheet) {
    return rect.x < 0.0 || rect.y < 0.0 ||
           rect.x + rect.width > sheet.widthPoints ||
           rect.y + rect.height > sheet.heightPoints;
}

struct PlacementGeometry {
    Rect rect;
    double rotationDegrees {0.0};
    double scale {1.0};
};

struct PlacementCtm {
    double a {1.0};
    double b {0.0};
    double c {0.0};
    double d {1.0};
    double e {0.0};
    double f {0.0};
};

PlacementGeometry BuildPlacementGeometry(const Rect& slotRect, const BuildOptions& options) {
    PlacementGeometry geometry {};
    geometry.rect = slotRect;

    if (!options.scaleToFit ||
        options.sourcePageWidthPoints <= 0.0 ||
        options.sourcePageHeightPoints <= 0.0 ||
        slotRect.width <= 0.0 ||
        slotRect.height <= 0.0) {
        return geometry;
    }

    const auto computeContainScale = [&](double sourceWidth, double sourceHeight) -> double {
        const double sx = slotRect.width / sourceWidth;
        const double sy = slotRect.height / sourceHeight;
        return std::min(sx, sy);
    };

    const double normalScale = computeContainScale(options.sourcePageWidthPoints, options.sourcePageHeightPoints);
    const double rotatedScale = computeContainScale(options.sourcePageHeightPoints, options.sourcePageWidthPoints);

    bool useRotated = false;
    if (options.autoRotateToFit && rotatedScale > normalScale) {
        useRotated = true;
    }

    geometry.rotationDegrees = useRotated ? 90.0 : 0.0;
    geometry.scale = useRotated ? rotatedScale : normalScale;

    const double baseWidth = useRotated ? options.sourcePageHeightPoints : options.sourcePageWidthPoints;
    const double baseHeight = useRotated ? options.sourcePageWidthPoints : options.sourcePageHeightPoints;
    const double fittedWidth = baseWidth * geometry.scale;
    const double fittedHeight = baseHeight * geometry.scale;

    geometry.rect = Rect {
        slotRect.x + (slotRect.width - fittedWidth) / 2.0,
        slotRect.y + (slotRect.height - fittedHeight) / 2.0,
        fittedWidth,
        fittedHeight
    };

    return geometry;
}

PlacementCtm BuildPlacementCtm(const SlotPlacement& p) {
    PlacementCtm ctm {};
    ctm.a = p.targetRect.width;
    ctm.d = p.targetRect.height;
    ctm.e = p.targetRect.x;
    ctm.f = p.targetRect.y;
    if (p.rotationDegrees == 90.0) {
        ctm.a = 0.0;
        ctm.b = p.targetRect.height;
        ctm.c = -p.targetRect.width;
        ctm.d = 0.0;
        ctm.e = p.targetRect.x + p.targetRect.width;
        ctm.f = p.targetRect.y;
    } else if (p.rotationDegrees == 180.0) {
        ctm.a = -p.targetRect.width;
        ctm.b = 0.0;
        ctm.c = 0.0;
        ctm.d = -p.targetRect.height;
        ctm.e = p.targetRect.x + p.targetRect.width;
        ctm.f = p.targetRect.y + p.targetRect.height;
    } else if (p.rotationDegrees == 270.0) {
        ctm.a = 0.0;
        ctm.b = -p.targetRect.height;
        ctm.c = p.targetRect.width;
        ctm.d = 0.0;
        ctm.e = p.targetRect.x;
        ctm.f = p.targetRect.y + p.targetRect.height;
    }
    return ctm;
}


std::vector<std::uint32_t> BuildSourcePages(std::uint32_t pageCount, const BuildOptions& options) {
    std::vector<std::uint32_t> pages;
    if (!options.explicitPageSequence.empty()) {
        pages.reserve(options.explicitPageSequence.size());
        for (const auto pageIndex : options.explicitPageSequence) {
            if (pageIndex == kBlankPageIndex || pageIndex < pageCount) {
                pages.push_back(pageIndex);
            } else {
                pages.push_back(kBlankPageIndex);
            }
        }
    } else {
        pages.reserve(pageCount);
        for (std::uint32_t idx = 0; idx < pageCount; ++idx) {
            pages.push_back(idx);
        }
    }

    if (options.filter != PageFilter::All) {
        std::vector<std::uint32_t> filtered;
        filtered.reserve(pages.size());
        for (const auto pageIndex : pages) {
            if (pageIndex == kBlankPageIndex) {
                filtered.push_back(pageIndex);
                continue;
            }
            const std::uint32_t humanPage = pageIndex + 1u;
            if (options.filter == PageFilter::EvenOnly && (humanPage % 2u) == 0u) {
                filtered.push_back(pageIndex);
            }
            if (options.filter == PageFilter::OddOnly && (humanPage % 2u) != 0u) {
                filtered.push_back(pageIndex);
            }
        }
        pages = std::move(filtered);
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
        const Rect slotRect {
            slotIndex == 0 ? 0.0 : halfWidth,
            0.0,
            halfWidth,
            fullHeight
        };
        const auto geometry = BuildPlacementGeometry(slotRect, options);
        placement.targetRect = geometry.rect;
        placement.rotationDegrees = geometry.rotationDegrees;
        placement.scale = geometry.scale;

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
        const Rect slotRect {
            slotWidth * static_cast<double>(col),
            slotHeight * static_cast<double>(row),
            slotWidth,
            slotHeight
        };
        const auto geometry = BuildPlacementGeometry(slotRect, options);
        placement.targetRect = geometry.rect;
        placement.rotationDegrees = geometry.rotationDegrees;
        placement.scale = geometry.scale;

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
                               std::uint32_t sourcePageIndex,
                               double creepOffsetXPoints) {

        SlotPlacement placement {};
        placement.sheetIndex = sheetIndex;
        placement.slotIndex = slotIndex;
        if (sourcePageIndex == kBlankPageIndex) {
            placement.sourcePage = PageRef {"", kBlankPageIndex};
        } else {
            placement.sourcePage = PageRef {sourceDocumentId, sourcePageIndex};
        }
        const Rect slotRect {
            (slotIndex == 0 ? 0.0 : halfWidth) + creepOffsetXPoints,
            0.0,
            halfWidth,
            fullHeight
        };
        const auto geometry = BuildPlacementGeometry(slotRect, options);
        placement.targetRect = geometry.rect;
        placement.rotationDegrees = geometry.rotationDegrees;
        placement.scale = geometry.scale;
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
            const double creepMagnitude = options.bookletCreepPerSheetPoints > 0.0
                ? static_cast<double>(signatureSheet) * options.bookletCreepPerSheetPoints
                : 0.0;
            const std::uint32_t frontLeft = signaturePaddedPageCount - 1u - (2u * signatureSheet);
            const std::uint32_t frontRight = 2u * signatureSheet;
            const std::uint32_t backLeft = 2u * signatureSheet + 1u;
            const std::uint32_t backRight = signaturePaddedPageCount - 2u - (2u * signatureSheet);

            appendPlacement(sheetIndexOffset + signatureSheet * 2u, 0u, signaturePages[frontLeft], creepMagnitude);
            appendPlacement(sheetIndexOffset + signatureSheet * 2u, 1u, signaturePages[frontRight], -creepMagnitude);
            appendPlacement(sheetIndexOffset + signatureSheet * 2u + 1u, 0u, signaturePages[backLeft], creepMagnitude);
            appendPlacement(sheetIndexOffset + signatureSheet * 2u + 1u, 1u, signaturePages[backRight], -creepMagnitude);
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
                const auto geometry = BuildPlacementGeometry(target, options);
                placement.targetRect = geometry.rect;
                placement.rotationDegrees = std::fmod(geometry.rotationDegrees + config.rotationDegrees, 360.0);
                placement.scale = geometry.scale;
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

ImpositionPlan ManualPlanner::Build(const std::string& sourceDocumentId,
                                    std::uint32_t pageCount,
                                    const SheetSize& outputSheet,
                                    std::uint32_t columns,
                                    std::uint32_t rows,
                                    const std::vector<std::uint32_t>& orderedPages,
                                    const BuildOptions& options) {
    ImpositionPlan plan {};
    plan.mode = LayoutMode::Manual;
    plan.outputSheet = outputSheet;
    plan.sourcePageCount = pageCount;

    if (pageCount == 0 || IsInvalidSheet(outputSheet) || columns == 0 || rows == 0 || orderedPages.empty()) {
        return plan;
    }

    plan.paddedPageCount = static_cast<std::uint32_t>(orderedPages.size());

    const double slotWidth = outputSheet.widthPoints / static_cast<double>(columns);
    const double slotHeight = outputSheet.heightPoints / static_cast<double>(rows);
    const std::uint32_t slotsPerSheet = columns * rows;

    for (std::size_t i = 0; i < orderedPages.size(); ++i) {
        const std::uint32_t sheetIndex = static_cast<std::uint32_t>(i) / slotsPerSheet;
        const std::uint32_t slotIndex = static_cast<std::uint32_t>(i) % slotsPerSheet;
        const std::uint32_t col = slotIndex % columns;
        const std::uint32_t row = slotIndex / columns;
        const std::uint32_t sourcePageIndex = orderedPages[i];

        SlotPlacement placement {};
        placement.sheetIndex = sheetIndex;
        placement.slotIndex = slotIndex;
        if (sourcePageIndex == kBlankPageIndex || sourcePageIndex >= pageCount) {
            placement.sourcePage = PageRef {"", kBlankPageIndex};
        } else {
            placement.sourcePage = PageRef {sourceDocumentId, sourcePageIndex};
        }
        const Rect slotRect {
            slotWidth * static_cast<double>(col),
            slotHeight * static_cast<double>(row),
            slotWidth,
            slotHeight
        };
        const auto geometry = BuildPlacementGeometry(slotRect, options);
        placement.targetRect = geometry.rect;
        placement.rotationDegrees = geometry.rotationDegrees;
        placement.scale = geometry.scale;
        plan.placements.push_back(placement);
    }

    return plan;
}

ImpositionPlan TilePlanner::Build(const std::string& sourceDocumentId,
                                  std::uint32_t pageCount,
                                  const SheetSize& outputSheet,
                                  const TileConfig& config,
                                  const BuildOptions& options) {
    ImpositionPlan plan {};
    plan.mode = LayoutMode::Tile;
    plan.outputSheet = outputSheet;
    plan.sourcePageCount = pageCount;

    if (pageCount == 0 || IsInvalidSheet(outputSheet) || config.columns == 0 || config.rows == 0 || config.overlapPoints < 0.0) {
        return plan;
    }

    const auto sourcePages = BuildSourcePages(pageCount, options);
    if (sourcePages.empty()) {
        return plan;
    }
    plan.paddedPageCount = static_cast<std::uint32_t>(sourcePages.size());

    const double denomX = static_cast<double>(config.columns);
    const double denomY = static_cast<double>(config.rows);
    const double slotWidth = (outputSheet.widthPoints + config.overlapPoints * static_cast<double>(config.columns - 1u)) / denomX;
    const double slotHeight = (outputSheet.heightPoints + config.overlapPoints * static_cast<double>(config.rows - 1u)) / denomY;
    if (slotWidth <= 0.0 || slotHeight <= 0.0 || slotWidth < config.overlapPoints || slotHeight < config.overlapPoints) {
        return plan;
    }

    for (std::size_t sourcePos = 0; sourcePos < sourcePages.size(); ++sourcePos) {
        const std::uint32_t sourcePageIndex = sourcePages[sourcePos];
        for (std::uint32_t row = 0; row < config.rows; ++row) {
            for (std::uint32_t col = 0; col < config.columns; ++col) {
                SlotPlacement placement {};
                placement.sheetIndex = static_cast<std::uint32_t>(sourcePos);
                placement.slotIndex = row * config.columns + col;
                if (sourcePageIndex == kBlankPageIndex) {
                    placement.sourcePage = PageRef {"", kBlankPageIndex};
                } else {
                    placement.sourcePage = PageRef {sourceDocumentId, sourcePageIndex};
                }
                const Rect slotRect {
                    static_cast<double>(col) * (slotWidth - config.overlapPoints),
                    static_cast<double>(row) * (slotHeight - config.overlapPoints),
                    slotWidth,
                    slotHeight
                };
                const auto geometry = BuildPlacementGeometry(slotRect, options);
                placement.targetRect = geometry.rect;
                placement.rotationDegrees = geometry.rotationDegrees;
                placement.scale = geometry.scale;
                plan.placements.push_back(placement);
            }
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
            << ", \"rotationDegrees\": " << p.rotationDegrees
            << ", \"scale\": " << p.scale
            << ", \"targetRect\": {\"x\": " << p.targetRect.x
            << ", \"y\": " << p.targetRect.y
            << ", \"width\": " << p.targetRect.width
            << ", \"height\": " << p.targetRect.height << "}"
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

std::string ToPlacementManifestJson(const ImpositionPlan& plan) {
    std::ostringstream out;
    out << "{\n";
    out << "  \"mode\": \"" << LayoutModeName(plan.mode) << "\",\n";
    out << "  \"sheet\": {\"widthPoints\": " << plan.outputSheet.widthPoints
        << ", \"heightPoints\": " << plan.outputSheet.heightPoints << "},\n";
    out << "  \"placements\": [\n";

    for (std::size_t i = 0; i < plan.placements.size(); ++i) {
        const auto& p = plan.placements[i];
        // Affine transform for Acrobat-like placement handoff.
        // Maps normalized source page space (0..1) into destination targetRect.
        const PlacementCtm ctm = BuildPlacementCtm(p);

        out << "    {"
            << "\"sheetIndex\": " << p.sheetIndex
            << ", \"slotIndex\": " << p.slotIndex
            << ", \"source\": {\"documentId\": \"" << EscapeJson(p.sourcePage.sourceDocumentId)
            << "\", \"pageIndex\": " << p.sourcePage.pageIndex << "}"
            << ", \"rotationDegrees\": " << p.rotationDegrees
            << ", \"scale\": " << p.scale
            << ", \"targetRect\": {\"x\": " << p.targetRect.x
            << ", \"y\": " << p.targetRect.y
            << ", \"width\": " << p.targetRect.width
            << ", \"height\": " << p.targetRect.height << "}"
            << ", \"ctm\": {\"a\": " << ctm.a
            << ", \"b\": " << ctm.b
            << ", \"c\": " << ctm.c
            << ", \"d\": " << ctm.d
            << ", \"e\": " << ctm.e
            << ", \"f\": " << ctm.f << "}"
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

std::string ToAcrobatPlacementJs(const ImpositionPlan& plan) {
    std::ostringstream out;
    out << "// Generated by imposr_cli --acrobat-js-out\n";
    out << "// Acrobat composition execution scaffold (adapter-backed).\n";
    out << "// Provide an adapter with the required methods, then call runAimpPlacement(adapter).\n";
    out << "var aimpPlan = {\n";
    out << "  mode: \"" << LayoutModeName(plan.mode) << "\",\n";
    out << "  sheetWidth: " << plan.outputSheet.widthPoints << ",\n";
    out << "  sheetHeight: " << plan.outputSheet.heightPoints << ",\n";
    out << "  placements: [\n";
    for (std::size_t i = 0; i < plan.placements.size(); ++i) {
        const auto& p = plan.placements[i];
        const PlacementCtm ctm = BuildPlacementCtm(p);
        out << "    {sheet: " << p.sheetIndex
            << ", slot: " << p.slotIndex
            << ", sourceDocId: \"" << EscapeJson(p.sourcePage.sourceDocumentId)
            << "\", sourcePageIndex: " << p.sourcePage.pageIndex
            << ", rotationDegrees: " << p.rotationDegrees
            << ", ctm: [" << ctm.a << ", " << ctm.b << ", " << ctm.c
            << ", " << ctm.d << ", " << ctm.e << ", " << ctm.f << "]}";
        if (i + 1 != plan.placements.size()) {
            out << ",";
        }
        out << "\n";
    }
    out << "  ]\n";
    out << "};\n";
    out << "function runAimpPlacement(adapter) {\n";
    out << "  if (!adapter) throw new Error(\"Adapter is required.\");\n";
    out << "  if (typeof adapter.ensureSheet !== \"function\") throw new Error(\"adapter.ensureSheet(sheet, width, height) is required.\");\n";
    out << "  if (typeof adapter.placeSourcePage !== \"function\") throw new Error(\"adapter.placeSourcePage(sourceDocId, sourcePageIndex, sheet, ctm) is required.\");\n";
    out << "  if (typeof adapter.finalize !== \"function\") throw new Error(\"adapter.finalize(plan) is required.\");\n";
    out << "\n";
    out << "  for (var i = 0; i < aimpPlan.placements.length; ++i) {\n";
    out << "    var p = aimpPlan.placements[i];\n";
    out << "    adapter.ensureSheet(p.sheet, aimpPlan.sheetWidth, aimpPlan.sheetHeight);\n";
    out << "    if (p.sourcePageIndex === " << kBlankPageIndex << ") {\n";
    out << "      if (typeof adapter.markBlankSlot === \"function\") {\n";
    out << "        adapter.markBlankSlot(p.sheet, p.slot);\n";
    out << "      }\n";
    out << "      continue;\n";
    out << "    }\n";
    out << "    adapter.placeSourcePage(p.sourceDocId, p.sourcePageIndex, p.sheet, p.ctm);\n";
    out << "  }\n";
    out << "  adapter.finalize(aimpPlan);\n";
    out << "}\n";
    out << "\n";
    out << "function runAimpPlacementWithAcrobatJs(sourcePdfPath, outputPdfPath) {\n";
    out << "  if (!sourcePdfPath) throw new Error(\"sourcePdfPath is required\");\n";
    out << "  if (!outputPdfPath) throw new Error(\"outputPdfPath is required\");\n";
    out << "  var outDoc = app.newDoc({nWidth: aimpPlan.sheetWidth, nHeight: aimpPlan.sheetHeight});\n";
    out << "  var ensureSheet = function(sheetIndex) {\n";
    out << "    while (outDoc.numPages <= sheetIndex) {\n";
    out << "      outDoc.newPage(outDoc.numPages - 1, aimpPlan.sheetWidth, aimpPlan.sheetHeight);\n";
    out << "    }\n";
    out << "  };\n";
    out << "  var placeSourcePage = function(sourcePageIndex, sheetIndex, ctm, rotationDegrees) {\n";
    out << "    var widthScale = Math.abs(ctm[0]) > 0 ? Math.abs(ctm[0]) : Math.abs(ctm[2]);\n";
    out << "    var percentScale = (widthScale * 100.0);\n";
    out << "    outDoc.addWatermarkFromFile({\n";
    out << "      cDIPath: sourcePdfPath,\n";
    out << "      nSourcePage: sourcePageIndex,\n";
    out << "      nStart: sheetIndex,\n";
    out << "      nEnd: sheetIndex,\n";
    out << "      nHorizAlign: app.constants.align.left,\n";
    out << "      nVertAlign: app.constants.align.bottom,\n";
    out << "      nHorizValue: ctm[4],\n";
    out << "      nVertValue: ctm[5],\n";
    out << "      nRotation: rotationDegrees,\n";
    out << "      nScale: percentScale,\n";
    out << "      bOnTop: true\n";
    out << "    });\n";
    out << "  };\n";
    out << "  for (var i = 0; i < aimpPlan.placements.length; ++i) {\n";
    out << "    var p = aimpPlan.placements[i];\n";
    out << "    ensureSheet(p.sheet);\n";
    out << "    if (p.sourcePageIndex === " << kBlankPageIndex << ") continue;\n";
    out << "    placeSourcePage(p.sourcePageIndex, p.sheet, p.ctm, p.rotationDegrees || 0);\n";
    out << "  }\n";
    out << "  outDoc.saveAs(outputPdfPath);\n";
    out << "}\n";
    return out.str();
}

std::string ToAcrobatSdkOpsJson(const ImpositionPlan& plan) {
    std::ostringstream out;
    out << "{\n";
    out << "  \"kind\": \"acrobat-sdk-placement-ops\",\n";
    out << "  \"mode\": \"" << LayoutModeName(plan.mode) << "\",\n";
    out << "  \"sheet\": {\"widthPoints\": " << plan.outputSheet.widthPoints
        << ", \"heightPoints\": " << plan.outputSheet.heightPoints << "},\n";
    out << "  \"operations\": [\n";
    for (std::size_t i = 0; i < plan.placements.size(); ++i) {
        const auto& p = plan.placements[i];
        const PlacementCtm ctm = BuildPlacementCtm(p);
        out << "    {\"op\": \"place-page\""
            << ", \"sheetIndex\": " << p.sheetIndex
            << ", \"slotIndex\": " << p.slotIndex
            << ", \"isBlank\": " << (p.sourcePage.pageIndex == kBlankPageIndex ? "true" : "false")
            << ", \"source\": {\"documentId\": \"" << EscapeJson(p.sourcePage.sourceDocumentId)
            << "\", \"pageIndex\": " << p.sourcePage.pageIndex << "}"
            << ", \"rotationDegrees\": " << p.rotationDegrees
            << ", \"scale\": " << p.scale
            << ", \"targetRect\": {\"x\": " << p.targetRect.x
            << ", \"y\": " << p.targetRect.y
            << ", \"width\": " << p.targetRect.width
            << ", \"height\": " << p.targetRect.height << "}"
            << ", \"ctm\": {\"a\": " << ctm.a
            << ", \"b\": " << ctm.b
            << ", \"c\": " << ctm.c
            << ", \"d\": " << ctm.d
            << ", \"e\": " << ctm.e
            << ", \"f\": " << ctm.f << "}}";
        if (i + 1 < plan.placements.size()) {
            out << ",";
        }
        out << '\n';
    }
    out << "  ]\n";
    out << "}\n";
    return out.str();
}

std::string ToAcrobatXObjectComposeJson(const ImpositionPlan& plan) {
    std::ostringstream out;
    out << "{\n";
    out << "  \"kind\": \"acrobat-xobject-compose-plan\",\n";
    out << "  \"sheet\": {\"widthPoints\": " << plan.outputSheet.widthPoints
        << ", \"heightPoints\": " << plan.outputSheet.heightPoints << "},\n";
    out << "  \"sheets\": [\n";

    const std::uint32_t sheetCount = plan.placements.empty()
        ? 0u
        : (std::max_element(plan.placements.begin(), plan.placements.end(),
                            [](const SlotPlacement& a, const SlotPlacement& b) {
                                return a.sheetIndex < b.sheetIndex;
                            })->sheetIndex + 1u);
    for (std::uint32_t sheetIndex = 0; sheetIndex < sheetCount; ++sheetIndex) {
        out << "    {\n";
        out << "      \"sheetIndex\": " << sheetIndex << ",\n";
        out << "      \"placements\": [\n";
        bool firstPlacement = true;
        for (const auto& p : plan.placements) {
            if (p.sheetIndex != sheetIndex) {
                continue;
            }
            if (!firstPlacement) {
                out << ",\n";
            }
            firstPlacement = false;
            const PlacementCtm ctm = BuildPlacementCtm(p);
            out << "        {\"slotIndex\": " << p.slotIndex
                << ", \"isBlank\": " << (p.sourcePage.pageIndex == kBlankPageIndex ? "true" : "false")
                << ", \"sourcePageIndex\": " << p.sourcePage.pageIndex
                << ", \"rotationDegrees\": " << p.rotationDegrees
                << ", \"targetRect\": {\"x\": " << p.targetRect.x
                << ", \"y\": " << p.targetRect.y
                << ", \"width\": " << p.targetRect.width
                << ", \"height\": " << p.targetRect.height << "}"
                << ", \"ctm\": {\"a\": " << ctm.a
                << ", \"b\": " << ctm.b
                << ", \"c\": " << ctm.c
                << ", \"d\": " << ctm.d
                << ", \"e\": " << ctm.e
                << ", \"f\": " << ctm.f << "}}";
        }
        out << "\n      ]\n";
        out << "    }";
        if (sheetIndex + 1u < sheetCount) {
            out << ",";
        }
        out << "\n";
    }
    out << "  ]\n";
    out << "}\n";
    return out.str();
}

// ---------------------------------------------------------------------------
// Minimal recursive-descent JSON parser used by ParseAcrobatSdkOpsJson.
// Supports strings (with escape sequences), numbers, booleans, null,
// arrays, and objects. No external dependencies.
// ---------------------------------------------------------------------------
namespace {

struct JVal {
    enum class K { Null, Bool, Num, Str, Arr, Obj } k = K::Null;
    bool  b   = false;
    double n  = 0.0;
    std::string s;
    std::vector<JVal> a;
    std::vector<std::pair<std::string, JVal>> o;

    const JVal* get(const char* key) const {
        if (k != K::Obj) return nullptr;
        for (const auto& kv : o) if (kv.first == key) return &kv.second;
        return nullptr;
    }
    double asNum(double def = 0.0)       const { return k == K::Num  ? n : def; }
    bool   asBool(bool def = false)      const { return k == K::Bool ? b : def; }
    const std::string& asStr()           const { static const std::string e; return k == K::Str ? s : e; }
    std::uint32_t asUInt(std::uint32_t def = 0) const {
        return k == K::Num ? static_cast<std::uint32_t>(n) : def;
    }
};

struct JP {
    const std::string& src;
    std::size_t p = 0;

    void ws() { while (p < src.size() && std::isspace((unsigned char)src[p])) ++p; }

    bool parseStr(std::string& out) {
        if (p >= src.size() || src[p] != '"') return false;
        ++p;
        out.clear();
        while (p < src.size() && src[p] != '"') {
            if (src[p] == '\\') {
                if (++p >= src.size()) return false;
                switch (src[p]) {
                    case '"':  out += '"';  break;
                    case '\\': out += '\\'; break;
                    case '/':  out += '/';  break;
                    case 'n':  out += '\n'; break;
                    case 'r':  out += '\r'; break;
                    case 't':  out += '\t'; break;
                    case 'b':  out += '\b'; break;
                    case 'f':  out += '\f'; break;
                    case 'u':
                        // Skip 4-hex digits; replace with '?'
                        if (p + 4 >= src.size()) return false;
                        p += 4;
                        out += '?';
                        break;
                    default: out += src[p]; break;
                }
            } else {
                out += src[p];
            }
            ++p;
        }
        if (p >= src.size()) return false;
        ++p;
        return true;
    }

    bool parseArr(JVal& v) {
        if (p >= src.size() || src[p] != '[') return false;
        ++p; ws(); v.k = JVal::K::Arr;
        if (p < src.size() && src[p] == ']') { ++p; return true; }
        while (true) {
            JVal elem; ws();
            if (!parseVal(elem)) return false;
            v.a.push_back(std::move(elem));
            ws();
            if (p < src.size() && src[p] == ',') { ++p; ws(); continue; }
            if (p < src.size() && src[p] == ']') { ++p; return true; }
            return false;
        }
    }

    bool parseObj(JVal& v) {
        if (p >= src.size() || src[p] != '{') return false;
        ++p; ws(); v.k = JVal::K::Obj;
        if (p < src.size() && src[p] == '}') { ++p; return true; }
        while (true) {
            std::string key; ws();
            if (!parseStr(key)) return false;
            ws();
            if (p >= src.size() || src[p] != ':') return false;
            ++p; ws();
            JVal val;
            if (!parseVal(val)) return false;
            v.o.emplace_back(std::move(key), std::move(val));
            ws();
            if (p < src.size() && src[p] == ',') { ++p; ws(); continue; }
            if (p < src.size() && src[p] == '}') { ++p; return true; }
            return false;
        }
    }

    bool parseVal(JVal& v) {
        ws();
        if (p >= src.size()) return false;
        const char c = src[p];
        if (c == '"') { v.k = JVal::K::Str; return parseStr(v.s); }
        if (c == '[') return parseArr(v);
        if (c == '{') return parseObj(v);
        if (c == 't') {
            if (src.compare(p, 4, "true") != 0) return false;
            v.k = JVal::K::Bool; v.b = true; p += 4; return true;
        }
        if (c == 'f') {
            if (src.compare(p, 5, "false") != 0) return false;
            v.k = JVal::K::Bool; v.b = false; p += 5; return true;
        }
        if (c == 'n') {
            if (src.compare(p, 4, "null") != 0) return false;
            v.k = JVal::K::Null; p += 4; return true;
        }
        if (c == '-' || std::isdigit((unsigned char)c)) {
            char* end = nullptr;
            v.n = std::strtod(src.c_str() + p, &end);
            if (end == src.c_str() + p) return false;
            v.k = JVal::K::Num;
            p = static_cast<std::size_t>(end - src.c_str());
            return true;
        }
        return false;
    }

    bool parse(JVal& v) { ws(); return parseVal(v) && (ws(), p == src.size()); }
};

} // anonymous namespace

bool ParseAcrobatSdkOpsJson(const std::string& sdkOpsJson,
                            std::vector<AcrobatSdkPlacementOp>& outOps,
                            std::string& errorMessage) {
    outOps.clear();
    errorMessage.clear();

    JP jp{sdkOpsJson};
    JVal root;
    if (!jp.parse(root) || root.k != JVal::K::Obj) {
        errorMessage = "sdk-ops: invalid JSON";
        return false;
    }

    const JVal* ops = root.get("operations");
    if (!ops || ops->k != JVal::K::Arr) {
        errorMessage = "sdk-ops: missing \"operations\" array";
        return false;
    }

    for (const JVal& item : ops->a) {
        if (item.k != JVal::K::Obj) continue;

        const JVal* opField = item.get("op");
        if (!opField || opField->asStr() != "place-page") continue;

        const JVal* sheetF = item.get("sheetIndex");
        const JVal* slotF  = item.get("slotIndex");
        if (!sheetF || !slotF) {
            errorMessage = "sdk-ops: operation missing sheetIndex or slotIndex";
            outOps.clear();
            return false;
        }

        AcrobatSdkPlacementOp op {};
        op.sheetIndex = sheetF->asUInt();
        op.slotIndex  = slotF->asUInt();

        if (const JVal* f = item.get("isBlank"))        op.isBlank       = f->asBool();
        if (const JVal* f = item.get("rotationDegrees")) op.rotationDegrees = f->asNum();
        if (const JVal* f = item.get("scale"))           op.scale          = f->asNum(1.0);

        if (const JVal* src = item.get("source")) {
            if (const JVal* f = src->get("documentId"))  op.sourceDocumentId = f->asStr();
            if (const JVal* f = src->get("pageIndex"))   op.sourcePageIndex  = f->asUInt(kBlankPageIndex);
        }
        if (const JVal* r = item.get("targetRect")) {
            if (const JVal* f = r->get("x"))      op.targetRect.x      = f->asNum();
            if (const JVal* f = r->get("y"))      op.targetRect.y      = f->asNum();
            if (const JVal* f = r->get("width"))  op.targetRect.width  = f->asNum();
            if (const JVal* f = r->get("height")) op.targetRect.height = f->asNum();
        }
        if (const JVal* ctm = item.get("ctm")) {
            if (const JVal* f = ctm->get("a")) op.ctmA = f->asNum();
            if (const JVal* f = ctm->get("b")) op.ctmB = f->asNum();
            if (const JVal* f = ctm->get("c")) op.ctmC = f->asNum();
            if (const JVal* f = ctm->get("d")) op.ctmD = f->asNum();
            if (const JVal* f = ctm->get("e")) op.ctmE = f->asNum();
            if (const JVal* f = ctm->get("f")) op.ctmF = f->asNum();
        }

        op.isBlank = op.isBlank || (op.sourcePageIndex == kBlankPageIndex);
        outOps.push_back(op);
    }

    if (outOps.empty()) {
        errorMessage = "sdk-ops: no place-page operations found";
        return false;
    }
    return true;
}


std::vector<ValidationIssue> ValidateAcrobatSdkOps(const ImpositionPlan& plan,
                                                   const std::vector<AcrobatSdkPlacementOp>& ops) {
    const auto nearlyEqual = [](double a, double b, double eps = 0.01) {
        return std::abs(a - b) <= eps;
    };
    const auto expectedFromRectAndRotation = [](const AcrobatSdkPlacementOp& op) {
        PlacementCtm ctm {};
        ctm.a = op.targetRect.width;
        ctm.d = op.targetRect.height;
        ctm.e = op.targetRect.x;
        ctm.f = op.targetRect.y;
        const double rot = std::fmod(op.rotationDegrees, 360.0);
        const auto rotNear = [&rot](double deg) { return std::abs(rot - deg) < 0.5; };
        if (rotNear(90.0) || rotNear(-270.0)) {
            ctm.a = 0.0;
            ctm.b = op.targetRect.height;
            ctm.c = -op.targetRect.width;
            ctm.d = 0.0;
            ctm.e = op.targetRect.x + op.targetRect.width;
            ctm.f = op.targetRect.y;
        } else if (rotNear(180.0) || rotNear(-180.0)) {
            ctm.a = -op.targetRect.width;
            ctm.d = -op.targetRect.height;
            ctm.e = op.targetRect.x + op.targetRect.width;
            ctm.f = op.targetRect.y + op.targetRect.height;
        } else if (rotNear(270.0) || rotNear(-90.0)) {
            ctm.a = 0.0;
            ctm.b = -op.targetRect.height;
            ctm.c = op.targetRect.width;
            ctm.d = 0.0;
            ctm.e = op.targetRect.x;
            ctm.f = op.targetRect.y + op.targetRect.height;
        }
        return ctm;
    };

    std::vector<ValidationIssue> issues;
    if (ops.size() != plan.placements.size()) {
        issues.push_back({"sdk-ops-count-mismatch", "Number of sdk-ops placements differs from planner placements."});
    }

    std::set<std::pair<std::uint32_t, std::uint32_t>> expectedSheetSlots;
    for (const auto& placement : plan.placements) {
        expectedSheetSlots.insert({placement.sheetIndex, placement.slotIndex});
    }
    std::set<std::pair<std::uint32_t, std::uint32_t>> seenSheetSlots;
    for (const auto& op : ops) {
        const auto key = std::make_pair(op.sheetIndex, op.slotIndex);
        if (!seenSheetSlots.insert(key).second) {
            issues.push_back({"sdk-ops-duplicate-slot", "Duplicate sheetIndex/slotIndex pair in sdk-ops."});
        }
        if (!expectedSheetSlots.empty() && expectedSheetSlots.find(key) == expectedSheetSlots.end()) {
            issues.push_back({"sdk-ops-unexpected-slot", "sdk-ops contains a sheetIndex/slotIndex pair not present in planner output."});
        }
        if (!op.isBlank && op.sourcePageIndex >= plan.sourcePageCount) {
            issues.push_back({"sdk-ops-invalid-source-page", "sdk-ops references source page outside sourcePageCount."});
        }
        if (op.targetRect.width <= 0.0 || op.targetRect.height <= 0.0) {
            issues.push_back({"sdk-ops-invalid-target-rect", "sdk-ops targetRect must have positive width/height."});
        }
        const PlacementCtm expected = expectedFromRectAndRotation(op);
        if (!nearlyEqual(op.ctmA, expected.a) ||
            !nearlyEqual(op.ctmB, expected.b) ||
            !nearlyEqual(op.ctmC, expected.c) ||
            !nearlyEqual(op.ctmD, expected.d) ||
            !nearlyEqual(op.ctmE, expected.e) ||
            !nearlyEqual(op.ctmF, expected.f)) {
            issues.push_back({"sdk-ops-ctm-parity-mismatch", "sdk-ops CTM does not match planner targetRect/rotation parity expectations."});
        }
    }

    return issues;
}

bool BuildSheetComposeBuckets(const ImpositionPlan& plan,
                              const std::vector<AcrobatSdkPlacementOp>& ops,
                              std::vector<std::vector<AcrobatSdkPlacementOp>>& outBuckets,
                              std::string& errorMessage) {
    outBuckets.clear();
    errorMessage.clear();

    const auto issues = ValidateAcrobatSdkOps(plan, ops);
    if (!issues.empty()) {
        errorMessage = "Cannot build sheet compose buckets because sdk-ops validation failed.";
        return false;
    }

    std::uint32_t sheetCount = 0;
    for (const auto& placement : plan.placements) {
        sheetCount = std::max(sheetCount, placement.sheetIndex + 1u);
    }
    outBuckets.resize(sheetCount);

    for (const auto& op : ops) {
        if (op.sheetIndex >= outBuckets.size()) {
            errorMessage = "sdk-ops sheet index is out of planner sheet range.";
            outBuckets.clear();
            return false;
        }
        outBuckets[op.sheetIndex].push_back(op);
    }

    for (auto& sheetOps : outBuckets) {
        std::sort(sheetOps.begin(), sheetOps.end(), [](const AcrobatSdkPlacementOp& a, const AcrobatSdkPlacementOp& b) {
            return a.slotIndex < b.slotIndex;
        });
    }

    return true;
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


PlanStatistics ComputePlanStatistics(const ImpositionPlan& plan) {
    PlanStatistics stats {};
    stats.placementCount = static_cast<std::uint32_t>(plan.placements.size());
    stats.sheetCount = plan.placements.empty() ? 0u : 1u;
    double minX = std::numeric_limits<double>::max();
    double minY = std::numeric_limits<double>::max();
    double maxX = std::numeric_limits<double>::lowest();
    double maxY = std::numeric_limits<double>::lowest();

    for (const auto& placement : plan.placements) {
        stats.sheetCount = std::max(stats.sheetCount, placement.sheetIndex + 1u);
        const bool isBlank = placement.sourcePage.pageIndex == kBlankPageIndex;
        if (isBlank) {
            ++stats.blankPlacementCount;
        } else {
            ++stats.nonBlankPlacementCount;
        }
        minX = std::min(minX, placement.targetRect.x);
        minY = std::min(minY, placement.targetRect.y);
        maxX = std::max(maxX, placement.targetRect.x + placement.targetRect.width);
        maxY = std::max(maxY, placement.targetRect.y + placement.targetRect.height);
    }

    if (!plan.placements.empty()) {
        stats.minX = minX;
        stats.minY = minY;
        stats.maxX = maxX;
        stats.maxY = maxY;
    }
    return stats;
}

std::vector<ValidationIssue> ValidatePlan(const ImpositionPlan& plan) {
    std::vector<ValidationIssue> issues;
    if (plan.outputSheet.widthPoints <= 0.0 || plan.outputSheet.heightPoints <= 0.0) {
        issues.push_back({"invalid-sheet", "Output sheet dimensions must be positive."});
    }

    for (const auto& placement : plan.placements) {
        if (placement.targetRect.width <= 0.0 || placement.targetRect.height <= 0.0) {
            issues.push_back({"invalid-rect", "Placement has a non-positive target rectangle."});
        }
        if (placement.targetRect.x < 0.0 || placement.targetRect.y < 0.0 ||
            placement.targetRect.x + placement.targetRect.width > plan.outputSheet.widthPoints + 0.01 ||
            placement.targetRect.y + placement.targetRect.height > plan.outputSheet.heightPoints + 0.01) {
            issues.push_back({"out-of-bounds", "Placement falls outside the output sheet."});
        }
        if (placement.sourcePage.pageIndex != kBlankPageIndex &&
            placement.sourcePage.pageIndex >= plan.sourcePageCount) {
            issues.push_back({"invalid-source-page", "Placement references a source page index beyond the document size."});
        }
    }
    return issues;
}

std::string BuildHumanSummary(const ImpositionPlan& plan) {
    const auto stats = ComputePlanStatistics(plan);
    const auto issues = ValidatePlan(plan);

    std::ostringstream out;
    out << "mode=" << LayoutModeName(plan.mode)
        << ", sourcePages=" << plan.sourcePageCount
        << ", paddedPages=" << plan.paddedPageCount
        << ", placements=" << stats.placementCount
        << ", sheets=" << stats.sheetCount
        << ", blanks=" << stats.blankPlacementCount;

    if (!plan.placements.empty()) {
        out << ", bounds=(" << stats.minX << "," << stats.minY
            << ")-(" << stats.maxX << "," << stats.maxY << ")";
    }
    if (!issues.empty()) {
        out << ", validationIssues=" << issues.size();
    }
    return out.str();
}

} // namespace aimp
