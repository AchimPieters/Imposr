#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "aimp/ImpositionPlan.h"

namespace aimp {

struct TilePagesConfig {
    std::uint32_t columns {2};
    std::uint32_t rows {2};
    double tileWidthPoints {0.0};
    double tileHeightPoints {0.0};
    double overlapPoints {0.0};
    double bleedPoints {0.0};
    bool addAlignmentMarks {false};
};

struct TileSlice {
    std::uint32_t column {0};
    std::uint32_t row {0};
    Rect sourceRegion;
    Rect targetRect;
    double bleedExpansion {0.0};
};

struct TilePagesResult {
    std::vector<TileSlice> slices;
    std::uint32_t columns {0};
    std::uint32_t rows {0};
    double tileWidth {0.0};
    double tileHeight {0.0};
    bool success {false};
    std::string errorMessage;
};

TilePagesResult ComputeTileGrid(double sourcePageWidth,
                                 double sourcePageHeight,
                                 const TilePagesConfig& config);

ImpositionPlan BuildTilePlan(const std::string& sourceDocumentId,
                              std::uint32_t pageCount,
                              double sourcePageWidth,
                              double sourcePageHeight,
                              const TilePagesConfig& config);

std::string TilePagesResultToJson(const TilePagesResult& result);

} // namespace aimp
