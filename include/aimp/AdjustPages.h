#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "aimp/ImpositionPlan.h"

namespace aimp {

enum class AdjustMode {
    Scale,
    Crop,
    Extend,
    ScaleToFit,
    ScaleToFill
};

struct AdjustSpec {
    AdjustMode mode {AdjustMode::Scale};
    double scaleX {1.0};
    double scaleY {1.0};
    Rect cropRect;
    double extendTop {0.0};
    double extendBottom {0.0};
    double extendLeft {0.0};
    double extendRight {0.0};
    double targetWidth {0.0};
    double targetHeight {0.0};
    double rotationDegrees {0.0};
    bool applyToAllPlacements {true};
    std::vector<std::uint32_t> targetSourcePageIndices;
};

struct AdjustResult {
    std::uint32_t placementsModified {0};
    std::vector<std::string> warnings;
};

AdjustResult ApplyAdjustSpec(ImpositionPlan& plan, const AdjustSpec& spec);

std::string AdjustResultToJson(const AdjustResult& result);

} // namespace aimp
