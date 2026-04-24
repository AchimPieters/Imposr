#include "aimp/TrimShift.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <sstream>

namespace aimp {

double ComputeCreepOffset(std::uint32_t sheetFromOutside,
                          std::uint32_t totalSheetsInSignature,
                          double creepPerSheetPoints) noexcept {
    if (totalSheetsInSignature == 0 || creepPerSheetPoints <= 0.0) {
        return 0.0;
    }
    const std::uint32_t clamped = std::min(sheetFromOutside, totalSheetsInSignature - 1u);
    return static_cast<double>(clamped) * creepPerSheetPoints;
}

TrimShiftResult ApplyCreepShiftToPlan(ImpositionPlan& plan, const TrimShiftConfig& config) {
    TrimShiftResult result {};

    if (config.creepPerSheetPoints <= 0.0 || plan.placements.empty()) {
        return result;
    }

    // Bucket placements by sheetIndex.
    std::map<std::uint32_t, std::vector<std::size_t>> sheetMap;
    for (std::size_t i = 0; i < plan.placements.size(); ++i) {
        sheetMap[plan.placements[i].sheetIndex].push_back(i);
    }

    if (sheetMap.empty()) {
        return result;
    }

    // Each pair of consecutive sheetIndex values represents one physical sheet
    // (front side = even sheetIndex, back side = odd sheetIndex).
    const std::uint32_t totalLogicalSheets = static_cast<std::uint32_t>(sheetMap.size());
    const std::uint32_t physicalSheetCount = (totalLogicalSheets + 1u) / 2u;

    const std::uint32_t sigPhysicalSheets = config.totalSheetsInSignature > 0
        ? config.totalSheetsInSignature / 2u
        : physicalSheetCount;

    double maxCreep = std::numeric_limits<double>::lowest();
    double minCreep = std::numeric_limits<double>::max();

    std::uint32_t logicalIndex = 0;
    for (auto& [sheetIdx, placementIndices] : sheetMap) {
        const std::uint32_t physicalSheetIndex = logicalIndex / 2u;
        const double creep = ComputeCreepOffset(physicalSheetIndex, sigPhysicalSheets, config.creepPerSheetPoints);

        if (creep > 0.0) {
            for (const std::size_t idx : placementIndices) {
                auto& placement = plan.placements[idx];
                if (placement.slotIndex == 0 && config.applyToLeftSlot) {
                    const double sign = (config.direction == CreepDirection::InwardFromSpine) ? 1.0 : -1.0;
                    placement.targetRect.x += sign * creep;
                } else if (placement.slotIndex == 1 && config.applyToRightSlot) {
                    const double sign = (config.direction == CreepDirection::InwardFromSpine) ? -1.0 : 1.0;
                    placement.targetRect.x += sign * creep;
                }
            }
            maxCreep = std::max(maxCreep, creep);
            minCreep = std::min(minCreep, creep);
            ++result.sheetsAdjusted;
        }

        TrimShiftEntry entry {};
        entry.sheetIndex = sheetIdx;
        entry.physicalSheetIndex = physicalSheetIndex;
        entry.creepOffsetPoints = creep;
        result.sheetTable.push_back(entry);

        ++logicalIndex;
    }

    if (result.sheetsAdjusted > 0) {
        result.maxCreepApplied = maxCreep;
        result.minCreepApplied = minCreep;
    }

    return result;
}

std::string TrimShiftConfigToJson(const TrimShiftConfig& config, const TrimShiftResult& result) {
    std::ostringstream out;
    out << "{\n";
    out << "  \"kind\": \"trim-shift\",\n";
    out << "  \"config\": {\n";
    out << "    \"creepPerSheetPoints\": " << config.creepPerSheetPoints << ",\n";
    out << "    \"innerMarginPoints\": " << config.innerMarginPoints << ",\n";
    out << "    \"outerMarginPoints\": " << config.outerMarginPoints << ",\n";
    out << "    \"totalSheetsInSignature\": " << config.totalSheetsInSignature << ",\n";
    out << "    \"direction\": \""
        << (config.direction == CreepDirection::InwardFromSpine ? "inward" : "outward") << "\",\n";
    out << "    \"applyToLeftSlot\": " << (config.applyToLeftSlot ? "true" : "false") << ",\n";
    out << "    \"applyToRightSlot\": " << (config.applyToRightSlot ? "true" : "false") << "\n";
    out << "  },\n";
    out << "  \"result\": {\n";
    out << "    \"sheetsAdjusted\": " << result.sheetsAdjusted << ",\n";
    out << "    \"maxCreepApplied\": " << result.maxCreepApplied << ",\n";
    out << "    \"minCreepApplied\": " << result.minCreepApplied << ",\n";
    out << "    \"sheetTable\": [\n";
    for (std::size_t i = 0; i < result.sheetTable.size(); ++i) {
        const auto& e = result.sheetTable[i];
        out << "      {\"sheetIndex\": " << e.sheetIndex
            << ", \"physicalSheetIndex\": " << e.physicalSheetIndex
            << ", \"creepOffsetPoints\": " << e.creepOffsetPoints << "}";
        if (i + 1 < result.sheetTable.size()) {
            out << ",";
        }
        out << "\n";
    }
    out << "    ]\n";
    out << "  }\n";
    out << "}\n";
    return out.str();
}

} // namespace aimp
