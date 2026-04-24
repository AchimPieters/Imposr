#include "aimp/TilePages.h"
#include "EscapeUtils.h"

#include <cmath>
#include <sstream>

namespace aimp {

namespace {

using internal::EscapeJson;

} // namespace

TilePagesResult ComputeTileGrid(double sourcePageWidth,
                                 double sourcePageHeight,
                                 const TilePagesConfig& config) {
    TilePagesResult result {};

    if (config.columns == 0 || config.rows == 0) {
        result.errorMessage = "Columns and rows must be > 0";
        return result;
    }
    if (sourcePageWidth <= 0.0 || sourcePageHeight <= 0.0) {
        result.errorMessage = "Source page dimensions must be > 0";
        return result;
    }

    const double overlap = std::max(0.0, config.overlapPoints);

    double tileW = config.tileWidthPoints;
    double tileH = config.tileHeightPoints;

    if (tileW <= 0.0) {
        tileW = (sourcePageWidth + overlap * static_cast<double>(config.columns - 1u))
                / static_cast<double>(config.columns);
    }
    if (tileH <= 0.0) {
        tileH = (sourcePageHeight + overlap * static_cast<double>(config.rows - 1u))
                / static_cast<double>(config.rows);
    }

    if (tileW <= 0.0 || tileH <= 0.0) {
        result.errorMessage = "Computed tile dimensions are invalid";
        return result;
    }

    result.columns = config.columns;
    result.rows = config.rows;
    result.tileWidth = tileW;
    result.tileHeight = tileH;

    const double bleed = std::max(0.0, config.bleedPoints);

    for (std::uint32_t row = 0; row < config.rows; ++row) {
        for (std::uint32_t col = 0; col < config.columns; ++col) {
            TileSlice slice {};
            slice.column = col;
            slice.row = row;
            slice.bleedExpansion = bleed;

            // Source region within the original page (in source page coordinates).
            const double srcX = static_cast<double>(col) * (tileW - overlap);
            const double srcY = static_cast<double>(row) * (tileH - overlap);
            slice.sourceRegion = Rect {srcX, srcY, tileW, tileH};

            // Target rect on the output tile sheet (with optional bleed padding).
            slice.targetRect = Rect {bleed, bleed, tileW, tileH};

            result.slices.push_back(std::move(slice));
        }
    }

    result.success = true;
    return result;
}

ImpositionPlan BuildTilePlan(const std::string& sourceDocumentId,
                              std::uint32_t pageCount,
                              double sourcePageWidth,
                              double sourcePageHeight,
                              const TilePagesConfig& config) {
    ImpositionPlan plan {};
    plan.mode = LayoutMode::Tile;
    plan.sourcePageCount = pageCount;

    if (pageCount == 0 || config.columns == 0 || config.rows == 0) {
        return plan;
    }

    const auto grid = ComputeTileGrid(sourcePageWidth, sourcePageHeight, config);
    if (!grid.success) {
        return plan;
    }

    const double tileSheetW = grid.tileWidth + config.bleedPoints * 2.0;
    const double tileSheetH = grid.tileHeight + config.bleedPoints * 2.0;
    plan.outputSheet = SheetSize {tileSheetW, tileSheetH};
    plan.paddedPageCount = pageCount;

    for (std::uint32_t pageIdx = 0; pageIdx < pageCount; ++pageIdx) {
        for (const auto& slice : grid.slices) {
            SlotPlacement placement {};
            placement.sheetIndex = pageIdx;
            placement.slotIndex = slice.row * config.columns + slice.column;
            placement.sourcePage = PageRef {sourceDocumentId, pageIdx};
            placement.targetRect = slice.targetRect;
            placement.scale = 1.0;
            plan.placements.push_back(std::move(placement));
        }
    }

    return plan;
}

std::string TilePagesResultToJson(const TilePagesResult& result) {
    std::ostringstream out;
    out << "{\n";
    out << "  \"kind\": \"tile-pages-result\",\n";
    out << "  \"success\": " << (result.success ? "true" : "false") << ",\n";
    out << "  \"errorMessage\": \"" << EscapeJson(result.errorMessage) << "\",\n";
    out << "  \"columns\": " << result.columns << ",\n";
    out << "  \"rows\": " << result.rows << ",\n";
    out << "  \"tileWidth\": " << result.tileWidth << ",\n";
    out << "  \"tileHeight\": " << result.tileHeight << ",\n";
    out << "  \"sliceCount\": " << result.slices.size() << ",\n";
    out << "  \"slices\": [\n";
    for (std::size_t i = 0; i < result.slices.size(); ++i) {
        const auto& s = result.slices[i];
        out << "    {\"column\": " << s.column << ", \"row\": " << s.row
            << ", \"bleedExpansion\": " << s.bleedExpansion
            << ", \"sourceRegion\": {\"x\": " << s.sourceRegion.x
            << ", \"y\": " << s.sourceRegion.y
            << ", \"width\": " << s.sourceRegion.width
            << ", \"height\": " << s.sourceRegion.height << "}"
            << ", \"targetRect\": {\"x\": " << s.targetRect.x
            << ", \"y\": " << s.targetRect.y
            << ", \"width\": " << s.targetRect.width
            << ", \"height\": " << s.targetRect.height << "}}";
        if (i + 1 < result.slices.size()) out << ",";
        out << "\n";
    }
    out << "  ]\n";
    out << "}\n";
    return out.str();
}

} // namespace aimp
