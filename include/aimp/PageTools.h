#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "aimp/ImpositionPlan.h"

namespace aimp {

struct PageSequenceOp {
    enum class Type { Duplicate, Delete, Move, Rotate, Insert };
    Type type {Type::Duplicate};
    std::uint32_t sourceIndex {0};
    std::uint32_t targetIndex {0};
    double rotateDegrees {0.0};
    std::uint32_t repeatCount {1};
};

std::vector<std::uint32_t> DuplicatePages(const std::vector<std::uint32_t>& pages,
                                           std::uint32_t pageIndex,
                                           std::uint32_t count = 1);

std::vector<std::uint32_t> DeletePages(const std::vector<std::uint32_t>& pages,
                                        const std::vector<std::uint32_t>& indicesToDelete);

std::vector<std::uint32_t> MovePages(const std::vector<std::uint32_t>& pages,
                                      std::uint32_t fromIndex,
                                      std::uint32_t toIndex);

void RotatePlacementsForPage(ImpositionPlan& plan,
                              std::uint32_t sourcePageIndex,
                              double additionalDegrees);

std::vector<std::uint32_t> InsertBlankPages(const std::vector<std::uint32_t>& pages,
                                             std::uint32_t atIndex,
                                             std::uint32_t count = 1);

ImpositionPlan ApplyPageSequenceOps(const ImpositionPlan& plan,
                                     const std::vector<PageSequenceOp>& ops,
                                     std::string& errorMessage);

} // namespace aimp
