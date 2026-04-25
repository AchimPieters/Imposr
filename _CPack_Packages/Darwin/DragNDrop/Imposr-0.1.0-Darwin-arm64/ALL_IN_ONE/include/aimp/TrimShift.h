#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "aimp/ImpositionPlan.h"

namespace aimp {

enum class CreepDirection {
    InwardFromSpine,
    OutwardFromSpine
};

struct TrimShiftConfig {
    double creepPerSheetPoints {0.0};
    double innerMarginPoints {0.0};
    double outerMarginPoints {0.0};
    std::uint32_t totalSheetsInSignature {0};
    CreepDirection direction {CreepDirection::InwardFromSpine};
    bool applyToLeftSlot {true};
    bool applyToRightSlot {true};
};

struct TrimShiftEntry {
    std::uint32_t sheetIndex {0};
    std::uint32_t physicalSheetIndex {0};
    double creepOffsetPoints {0.0};
};

struct TrimShiftResult {
    std::uint32_t sheetsAdjusted {0};
    double maxCreepApplied {0.0};
    double minCreepApplied {0.0};
    std::vector<TrimShiftEntry> sheetTable;
};

double ComputeCreepOffset(std::uint32_t sheetFromOutside,
                          std::uint32_t totalSheetsInSignature,
                          double creepPerSheetPoints) noexcept;

TrimShiftResult ApplyCreepShiftToPlan(ImpositionPlan& plan, const TrimShiftConfig& config);

std::string TrimShiftConfigToJson(const TrimShiftConfig& config, const TrimShiftResult& result);

} // namespace aimp
