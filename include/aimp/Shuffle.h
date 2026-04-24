#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace aimp {

enum class ShuffleMode {
    Signature,
    PerfectBound,
    EvenOdd,
    Interleave
};

struct ShuffleConfig {
    ShuffleMode mode {ShuffleMode::Signature};
    std::uint32_t signatureSize {4};
    bool reverseSignatures {false};
};

struct ShuffleResult {
    std::vector<std::uint32_t> orderedPages;
    std::uint32_t signatureCount {0};
    std::uint32_t blankPagesAdded {0};
};

ShuffleResult ShufflePages(const std::vector<std::uint32_t>& inputPages,
                            const ShuffleConfig& config);

std::pair<std::vector<std::uint32_t>, std::vector<std::uint32_t>>
SplitEvenOdd(const std::vector<std::uint32_t>& pages);

std::vector<std::uint32_t> MergeInterlace(const std::vector<std::uint32_t>& evens,
                                           const std::vector<std::uint32_t>& odds);

std::string ShuffleResultToJson(const ShuffleResult& result);

// ── Shuffle Assistant ─────────────────────────────────────────────────────────
// Analyses a given page count + signature size and describes how pages are
// grouped, which pages face each other, and how many blanks are needed.
// Equivalent to QI+'s "Shuffle Assistant" wizard.

struct ShuffleAssistantSheet {
    std::uint32_t signatureIndex {0};
    std::uint32_t sheetInSignature {0};
    // Four positions: [front-left, front-right, back-left, back-right]
    std::uint32_t frontLeft  {0};
    std::uint32_t frontRight {0};
    std::uint32_t backLeft   {0};
    std::uint32_t backRight  {0};
    bool frontLeftBlank  {false};
    bool frontRightBlank {false};
    bool backLeftBlank   {false};
    bool backRightBlank  {false};
};

struct ShuffleAssistantResult {
    std::uint32_t inputPageCount    {0};
    std::uint32_t signatureSize     {0};
    std::uint32_t signatureCount    {0};
    std::uint32_t paddedPageCount   {0};
    std::uint32_t blankPagesAdded   {0};
    std::vector<ShuffleAssistantSheet> sheets;
};

ShuffleAssistantResult RunShuffleAssistant(std::uint32_t pageCount,
                                            std::uint32_t signatureSize);

std::string ShuffleAssistantToJson(const ShuffleAssistantResult& result);

} // namespace aimp
