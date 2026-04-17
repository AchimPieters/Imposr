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

enum class PageFilter {
    All,
    EvenOnly,
    OddOnly
};

struct PageRef {
    std::string sourceDocumentId;
    std::uint32_t pageIndex {0};
};

constexpr std::uint32_t kBlankPageIndex = 0xFFFFFFFFu;

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

struct PlacementRef {
    std::uint32_t sheetIndex {0};
    std::uint32_t slotIndex {0};
};

struct ImpositionPlan {
    LayoutMode mode {LayoutMode::TwoUp};
    SheetSize outputSheet;
    std::vector<SlotPlacement> placements;
    std::uint32_t sourcePageCount {0};
    std::uint32_t paddedPageCount {0};
};

struct PlanStatistics {
    std::uint32_t sheetCount {0};
    std::uint32_t placementCount {0};
    std::uint32_t blankPlacementCount {0};
    std::uint32_t nonBlankPlacementCount {0};
    double minX {0.0};
    double minY {0.0};
    double maxX {0.0};
    double maxY {0.0};
};

struct ValidationIssue {
    std::string code;
    std::string message;
};

struct StepRepeatConfig {
    std::uint32_t repeatX {1};
    std::uint32_t repeatY {1};
    double stepXPoints {0.0};
    double stepYPoints {0.0};
    Rect seedRect;
};

struct TileConfig {
    std::uint32_t columns {1};
    std::uint32_t rows {1};
    double overlapPoints {0.0};
};

struct BuildOptions {
    bool reverseOrder {false};
    PageFilter filter {PageFilter::All};
    std::uint32_t padToMultiple {0};
    std::uint32_t bookletSignatureSize {0};
    std::vector<std::uint32_t> explicitPageSequence;
    bool scaleToFit {false};
    bool autoRotateToFit {false};
    double sourcePageWidthPoints {0.0};
    double sourcePageHeightPoints {0.0};
    double bookletCreepPerSheetPoints {0.0};
};

class TwoUpPlanner {
public:
    static ImpositionPlan Build(const std::string& sourceDocumentId,
                                std::uint32_t pageCount,
                                const SheetSize& outputSheet,
                                const BuildOptions& options = {});
};

class NUpPlanner {
public:
    static ImpositionPlan Build(const std::string& sourceDocumentId,
                                std::uint32_t pageCount,
                                const SheetSize& outputSheet,
                                std::uint32_t columns,
                                std::uint32_t rows,
                                const BuildOptions& options = {});
};

class BookletPlanner {
public:
    static ImpositionPlan Build(const std::string& sourceDocumentId,
                                std::uint32_t pageCount,
                                const SheetSize& outputSheet,
                                const BuildOptions& options = {});
};

class StepAndRepeatPlanner {
public:
    static ImpositionPlan Build(const std::string& sourceDocumentId,
                                std::uint32_t pageCount,
                                const SheetSize& outputSheet,
                                const StepRepeatConfig& config,
                                const BuildOptions& options = {});
};

class ManualPlanner {
public:
    static ImpositionPlan Build(const std::string& sourceDocumentId,
                                std::uint32_t pageCount,
                                const SheetSize& outputSheet,
                                std::uint32_t columns,
                                std::uint32_t rows,
                                const std::vector<std::uint32_t>& orderedPages,
                                const BuildOptions& options = {});
};

class TilePlanner {
public:
    static ImpositionPlan Build(const std::string& sourceDocumentId,
                                std::uint32_t pageCount,
                                const SheetSize& outputSheet,
                                const TileConfig& config,
                                const BuildOptions& options = {});
};

const char* LayoutModeName(LayoutMode mode);
std::string ToJson(const ImpositionPlan& plan);
std::string ToPlacementManifestJson(const ImpositionPlan& plan);
std::string ToAuditXml(const ImpositionPlan& plan);
std::vector<PlacementRef> FindPlacementsForSourcePage(const ImpositionPlan& plan,
                                                      const std::string& sourceDocumentId,
                                                      std::uint32_t pageIndex);
bool TryGetSourceForPlacement(const ImpositionPlan& plan,
                              std::uint32_t sheetIndex,
                              std::uint32_t slotIndex,
                              PageRef& outSourcePage);
PlanStatistics ComputePlanStatistics(const ImpositionPlan& plan);
std::vector<ValidationIssue> ValidatePlan(const ImpositionPlan& plan);
std::string BuildHumanSummary(const ImpositionPlan& plan);

} // namespace aimp
