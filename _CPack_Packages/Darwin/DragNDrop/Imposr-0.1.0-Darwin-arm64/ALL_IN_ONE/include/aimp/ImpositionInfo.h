#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "aimp/ImpositionPlan.h"

namespace aimp {

// One record per slot in the imposed output.
struct ImpositionInfoRecord {
    std::uint32_t sheetIndex {0};
    std::uint32_t slotIndex {0};
    bool isBlank {true};
    std::string sourceDocumentId;
    std::uint32_t sourcePageIndex {kBlankPageIndex};
    Rect targetRect;
    double rotationDegrees {0.0};
    double scale {1.0};
};

struct ImpositionInfoResult {
    std::vector<ImpositionInfoRecord> records;
    std::uint32_t totalSheets {0};
    std::uint32_t totalSlots {0};
    std::uint32_t blankSlots {0};
    std::uint32_t nonBlankSlots {0};
};

// Build a full forward mapping: imposed position → source page.
ImpositionInfoResult BuildImpositionInfo(const ImpositionPlan& plan);

// Return all source page refs placed on a given sheet (blank slots omitted).
std::vector<PageRef> ExtractPageRefsForSheet(const ImpositionPlan& plan,
                                              std::uint32_t sheetIndex);

std::string ImpositionInfoToJson(const ImpositionInfoResult& result);

} // namespace aimp
