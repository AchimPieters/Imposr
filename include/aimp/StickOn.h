#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "aimp/ImpositionPlan.h"

namespace aimp {

enum class StickOnType {
    Text,
    BatesNumber,
    PageNumber,
    PdfPage,
    MaskingTape,
    PeelOff
};

enum class StickOnAnchor {
    TopLeft,
    TopCenter,
    TopRight,
    MiddleLeft,
    MiddleCenter,
    MiddleRight,
    BottomLeft,
    BottomCenter,
    BottomRight
};

struct StickOnItem {
    StickOnType type {StickOnType::Text};
    StickOnAnchor anchor {StickOnAnchor::BottomLeft};
    std::string text;
    std::string sourcePdfPath;
    std::uint32_t sourcePdfPage {0};
    Rect rect;
    double fontSize {10.0};
    double opacity {1.0};
    bool applyToAllPages {true};
    std::vector<std::uint32_t> applyToPageIndices;

    // Bates number config.
    std::string batesPrefix;
    std::string batesSuffix;
    std::uint32_t batesStart {1};
    std::uint32_t batesPadWidth {6};

    // Color (RGB 0-1).
    double colorR {0.0};
    double colorG {0.0};
    double colorB {0.0};
};

struct ResolvedStickOnOp {
    std::uint32_t sheetIndex {0};
    std::uint32_t slotIndex {0};
    StickOnType type {StickOnType::Text};
    std::string resolvedText;
    Rect rect;
    double fontSize {10.0};
    double opacity {1.0};
    std::string sourcePdfPath;
    std::uint32_t sourcePdfPage {0};
    double colorR {0.0};
    double colorG {0.0};
    double colorB {0.0};
};

struct StickOnResult {
    std::vector<ResolvedStickOnOp> ops;
    std::uint32_t opsGenerated {0};
};

// Optional context passed to ResolveStickOnItems for expanding built-in template variables.
// Variables supported in StickOnItem::text: {{filename}}, {{date}}, {{datetime}},
// {{pagecount}}, {{sheetcount}}.
struct StickOnContext {
    std::string filename;
    std::string date;
    std::string datetime;
    std::uint32_t totalPageCount {0};
    std::uint32_t totalSheetCount {0};
};

StickOnResult ResolveStickOnItems(const ImpositionPlan& plan,
                                   const std::vector<StickOnItem>& items,
                                   const StickOnContext* context = nullptr);

std::string StickOnResultToJson(const StickOnResult& result);

std::string RenderStickOnPdfContent(const std::vector<ResolvedStickOnOp>& ops,
                                     std::uint32_t sheetIndex);

// ── Peel-off / mark removal ───────────────────────────────────────────────────

// Identifies which previously-applied ops to remove.
struct PeelOffSpec {
    bool removeAll         {false};   // if true, all other fields are ignored
    bool removeText        {false};
    bool removeBatesNumber {false};
    bool removePageNumber  {false};
    bool removePdfPage     {false};
    bool removeMaskingTape {false};
    bool removePeelOff     {false};
    // When set, only remove ops on this sheet (ignored if removeAll is true and sheetIndex == kNoSheetFilter).
    static constexpr std::uint32_t kNoSheetFilter = 0xFFFFFFFFu;
    std::uint32_t sheetIndex {kNoSheetFilter};
};

// Returns a copy of ops with matching entries removed according to spec.
std::vector<ResolvedStickOnOp> PeelOffOps(const std::vector<ResolvedStickOnOp>& ops,
                                           const PeelOffSpec& spec);

} // namespace aimp
