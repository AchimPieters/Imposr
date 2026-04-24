#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "aimp/ImpositionPlan.h"

namespace aimp {

struct PageRange {
    std::uint32_t startPage {0};
    std::uint32_t endPage {0};
    std::string label;
};

struct SplitMergeResult {
    std::vector<ImpositionPlan> segments;
    std::vector<PageRange> ranges;
    bool success {false};
    std::string errorMessage;
};

bool ParseRangeSpec(const std::string& spec,
                    std::uint32_t totalPages,
                    std::vector<PageRange>& outRanges,
                    std::string& errorMessage);

SplitMergeResult SplitPlan(const ImpositionPlan& plan,
                            const std::vector<PageRange>& ranges);

ImpositionPlan MergePlans(const std::vector<ImpositionPlan>& plans);

std::string SplitMergeResultToJson(const SplitMergeResult& result);

} // namespace aimp
