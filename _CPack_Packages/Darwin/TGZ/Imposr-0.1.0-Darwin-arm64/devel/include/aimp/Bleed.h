#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "aimp/ImpositionPlan.h"

namespace aimp {

enum class BleedMethod {
    Mirror,
    Scale,
    Extend,
    SolidColor
};

struct BleedSpec {
    double top {0.0};
    double bottom {0.0};
    double left {0.0};
    double right {0.0};
    BleedMethod method {BleedMethod::Mirror};
    double solidColorR {1.0};
    double solidColorG {1.0};
    double solidColorB {1.0};
};

struct BleedZone {
    std::uint32_t sheetIndex {0};
    std::uint32_t slotIndex {0};
    Rect trimBox;
    Rect bleedBox;
    BleedMethod method {BleedMethod::Mirror};
};

struct BleedResult {
    std::vector<BleedZone> zones;
    std::uint32_t zonesGenerated {0};
};

Rect ComputeBleedBox(const Rect& trimBox, const BleedSpec& spec);

BleedResult GenerateBleedZones(const ImpositionPlan& plan, const BleedSpec& spec);

std::string BleedZonesToJson(const BleedResult& result);

std::string RenderBleedPdfContent(const BleedResult& result,
                                   std::uint32_t sheetIndex,
                                   const BleedSpec& spec);

} // namespace aimp
