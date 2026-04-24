#pragma once

#include <string>
#include <vector>

#include "aimp/Preset.h"

namespace aimp {

struct PanelStateApplyResult {
    bool applied {false};
    std::vector<std::string> warnings;
};

const char* PanelStateFilterName(PageFilter filter);
PageFilter ParsePanelStateFilter(const std::string& value);

bool ApplyPanelStateJsonToPreset(const std::string& json,
                                 PlannerPreset& preset,
                                 PanelStateApplyResult& outResult);

} // namespace aimp

