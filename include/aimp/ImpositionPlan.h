#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace aimp {

enum class LayoutMode {
    TwoUp,
    Booklet,
    NUp,
    StepAndRepeat,
    Manual,
    Tile
};

struct PageRef {
    std::string sourceDocumentId;
    std::uint32_t pageIndex {0};
};

struct SheetSize {
    double widthPoints {0.0};
    double heightPoints {0.0};
};

struct Rect {
    double x {0.0};
    double y {0.0};
    double width {0.0};
    double height {0.0};
};

struct SlotPlacement {
    std::uint32_t sheetIndex {0};
    std::uint32_t slotIndex {0};
    PageRef sourcePage;
    Rect targetRect;
    double rotationDegrees {0.0};
    double scale {1.0};
};

struct ImpositionPlan {
    LayoutMode mode {LayoutMode::TwoUp};
    SheetSize outputSheet;
    std::vector<SlotPlacement> placements;
};

class TwoUpPlanner {
public:
    static ImpositionPlan Build(const std::string& sourceDocumentId,
                                std::uint32_t pageCount,
                                const SheetSize& outputSheet);
};

} // namespace aimp
