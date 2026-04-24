#include "aimp/PageTools.h"

#include <algorithm>
#include <set>

namespace aimp {

std::vector<std::uint32_t> DuplicatePages(const std::vector<std::uint32_t>& pages,
                                           std::uint32_t pageIndex,
                                           std::uint32_t count) {
    std::vector<std::uint32_t> result = pages;
    if (pageIndex >= pages.size() || count == 0) {
        return result;
    }
    const std::uint32_t val = pages[pageIndex];
    result.insert(result.begin() + static_cast<std::ptrdiff_t>(pageIndex) + 1,
                  count, val);
    return result;
}

std::vector<std::uint32_t> DeletePages(const std::vector<std::uint32_t>& pages,
                                        const std::vector<std::uint32_t>& indicesToDelete) {
    const std::set<std::uint32_t> deleteSet(indicesToDelete.begin(), indicesToDelete.end());
    std::vector<std::uint32_t> result;
    result.reserve(pages.size());
    for (std::size_t i = 0; i < pages.size(); ++i) {
        if (deleteSet.find(static_cast<std::uint32_t>(i)) == deleteSet.end()) {
            result.push_back(pages[i]);
        }
    }
    return result;
}

std::vector<std::uint32_t> MovePages(const std::vector<std::uint32_t>& pages,
                                      std::uint32_t fromIndex,
                                      std::uint32_t toIndex) {
    if (fromIndex >= pages.size() || toIndex >= pages.size() || fromIndex == toIndex) {
        return pages;
    }

    std::vector<std::uint32_t> result = pages;
    const std::uint32_t val = result[fromIndex];
    result.erase(result.begin() + static_cast<std::ptrdiff_t>(fromIndex));

    const std::uint32_t insertAt = toIndex > fromIndex ? toIndex - 1u : toIndex;
    result.insert(result.begin() + static_cast<std::ptrdiff_t>(insertAt), val);
    return result;
}

void RotatePlacementsForPage(ImpositionPlan& plan,
                              std::uint32_t sourcePageIndex,
                              double additionalDegrees) {
    for (auto& placement : plan.placements) {
        if (placement.sourcePage.pageIndex == sourcePageIndex) {
            placement.rotationDegrees = std::fmod(placement.rotationDegrees + additionalDegrees, 360.0);
            if (placement.rotationDegrees < 0.0) {
                placement.rotationDegrees += 360.0;
            }
        }
    }
}

std::vector<std::uint32_t> InsertBlankPages(const std::vector<std::uint32_t>& pages,
                                             std::uint32_t atIndex,
                                             std::uint32_t count) {
    std::vector<std::uint32_t> result = pages;
    if (atIndex > static_cast<std::uint32_t>(result.size())) {
        atIndex = static_cast<std::uint32_t>(result.size());
    }
    result.insert(result.begin() + static_cast<std::ptrdiff_t>(atIndex),
                  count, kBlankPageIndex);
    return result;
}

ImpositionPlan ApplyPageSequenceOps(const ImpositionPlan& plan,
                                     const std::vector<PageSequenceOp>& ops,
                                     std::string& errorMessage) {
    errorMessage.clear();

    // Build a mutable page list from the plan's placements.
    std::vector<std::uint32_t> pages;
    pages.reserve(plan.sourcePageCount);
    for (std::uint32_t i = 0; i < plan.sourcePageCount; ++i) {
        pages.push_back(i);
    }

    ImpositionPlan result = plan;

    for (const auto& op : ops) {
        switch (op.type) {
            case PageSequenceOp::Type::Duplicate:
                pages = DuplicatePages(pages, op.sourceIndex, op.repeatCount);
                break;
            case PageSequenceOp::Type::Delete: {
                std::vector<std::uint32_t> toDelete {op.sourceIndex};
                pages = DeletePages(pages, toDelete);
                break;
            }
            case PageSequenceOp::Type::Move:
                pages = MovePages(pages, op.sourceIndex, op.targetIndex);
                break;
            case PageSequenceOp::Type::Rotate:
                RotatePlacementsForPage(result, op.sourceIndex, op.rotateDegrees);
                break;
            case PageSequenceOp::Type::Insert:
                pages = InsertBlankPages(pages, op.targetIndex, op.repeatCount);
                break;
            default:
                errorMessage = "Unknown page sequence op type";
                return result;
        }
    }

    result.sourcePageCount = static_cast<std::uint32_t>(pages.size());
    result.paddedPageCount = result.sourcePageCount;

    return result;
}

} // namespace aimp
