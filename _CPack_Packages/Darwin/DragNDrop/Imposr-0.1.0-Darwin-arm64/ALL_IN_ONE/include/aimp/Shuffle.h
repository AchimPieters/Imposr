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

} // namespace aimp
