#include "aimp/Shuffle.h"

#include <algorithm>
#include <sstream>

#include "aimp/ImpositionPlan.h"

namespace aimp {

ShuffleResult ShufflePages(const std::vector<std::uint32_t>& inputPages,
                            const ShuffleConfig& config) {
    ShuffleResult result {};

    if (inputPages.empty()) {
        return result;
    }

    if (config.mode == ShuffleMode::EvenOdd) {
        auto [evens, odds] = SplitEvenOdd(inputPages);
        result.orderedPages = MergeInterlace(evens, odds);
        result.signatureCount = 1;
        return result;
    }

    if (config.mode == ShuffleMode::Interleave) {
        result.orderedPages = inputPages;
        result.signatureCount = 1;
        return result;
    }

    const std::uint32_t sigSize = std::max(4u, (config.signatureSize / 4u) * 4u);

    std::vector<std::uint32_t> pages = inputPages;

    // Pad to signature boundary.
    const std::uint32_t remainder = static_cast<std::uint32_t>(pages.size()) % sigSize;
    if (remainder != 0) {
        const std::uint32_t padCount = sigSize - remainder;
        result.blankPagesAdded = padCount;
        for (std::uint32_t i = 0; i < padCount; ++i) {
            pages.push_back(kBlankPageIndex);
        }
    }

    const std::uint32_t totalPages = static_cast<std::uint32_t>(pages.size());
    result.signatureCount = totalPages / sigSize;

    if (config.mode == ShuffleMode::PerfectBound) {
        // Perfect binding: each signature is sequential pages in reading order.
        result.orderedPages = pages;
        return result;
    }

    // Signature (saddle-stitch) shuffle:
    // For each signature, reorder pages so they print correctly on press.
    // Signature of size S: pairs are (S-1, 0), (1, S-2), (2, S-3), ...
    result.orderedPages.reserve(totalPages);

    for (std::uint32_t sig = 0; sig < result.signatureCount; ++sig) {
        const std::uint32_t base = sig * sigSize;
        // Collect this signature's pages.
        std::vector<std::uint32_t> sigPages(pages.begin() + base, pages.begin() + base + sigSize);

        if (config.reverseSignatures) {
            std::reverse(sigPages.begin(), sigPages.end());
        }

        // Reorder: each sheet has [back-right, back-left, front-left, front-right].
        // For printing: outer sheet first (index 0 and sigSize-1), then next sheet, etc.
        std::vector<std::uint32_t> shuffled;
        shuffled.reserve(sigSize);
        for (std::uint32_t sheet = 0; sheet < sigSize / 4u; ++sheet) {
            const std::uint32_t outerRight = sigSize - 1u - 2u * sheet;
            const std::uint32_t outerLeft = 2u * sheet;
            const std::uint32_t innerLeft = 2u * sheet + 1u;
            const std::uint32_t innerRight = sigSize - 2u - 2u * sheet;
            shuffled.push_back(sigPages[outerLeft]);
            shuffled.push_back(sigPages[outerRight]);
            shuffled.push_back(sigPages[innerLeft]);
            shuffled.push_back(sigPages[innerRight]);
        }

        for (const auto p : shuffled) {
            result.orderedPages.push_back(p);
        }
    }

    return result;
}

std::pair<std::vector<std::uint32_t>, std::vector<std::uint32_t>>
SplitEvenOdd(const std::vector<std::uint32_t>& pages) {
    std::vector<std::uint32_t> evens;
    std::vector<std::uint32_t> odds;
    for (std::size_t i = 0; i < pages.size(); ++i) {
        if (i % 2u == 0u) {
            evens.push_back(pages[i]);
        } else {
            odds.push_back(pages[i]);
        }
    }
    return {evens, odds};
}

std::vector<std::uint32_t> MergeInterlace(const std::vector<std::uint32_t>& evens,
                                           const std::vector<std::uint32_t>& odds) {
    std::vector<std::uint32_t> result;
    result.reserve(evens.size() + odds.size());
    const std::size_t maxSize = std::max(evens.size(), odds.size());
    for (std::size_t i = 0; i < maxSize; ++i) {
        if (i < evens.size()) result.push_back(evens[i]);
        if (i < odds.size()) result.push_back(odds[i]);
    }
    return result;
}

std::string ShuffleResultToJson(const ShuffleResult& result) {
    std::ostringstream out;
    out << "{\n";
    out << "  \"kind\": \"shuffle-result\",\n";
    out << "  \"signatureCount\": " << result.signatureCount << ",\n";
    out << "  \"blankPagesAdded\": " << result.blankPagesAdded << ",\n";
    out << "  \"totalPages\": " << result.orderedPages.size() << ",\n";
    out << "  \"orderedPages\": [";
    for (std::size_t i = 0; i < result.orderedPages.size(); ++i) {
        if (i > 0) out << ", ";
        if (result.orderedPages[i] == kBlankPageIndex) {
            out << "null";
        } else {
            out << result.orderedPages[i];
        }
    }
    out << "]\n";
    out << "}\n";
    return out.str();
}

ShuffleAssistantResult RunShuffleAssistant(std::uint32_t pageCount,
                                            std::uint32_t signatureSize) {
    ShuffleAssistantResult result {};
    result.inputPageCount = pageCount;

    const std::uint32_t sigSize = std::max(4u, (signatureSize / 4u) * 4u);
    result.signatureSize = sigSize;

    // Pad to signature boundary.
    const std::uint32_t remainder = pageCount % sigSize;
    const std::uint32_t padded    = (remainder == 0) ? pageCount : pageCount + (sigSize - remainder);
    result.paddedPageCount = padded;
    result.blankPagesAdded = padded - pageCount;
    result.signatureCount  = padded / sigSize;

    // Build 1-based page labels (0 = blank).
    std::vector<std::uint32_t> labels(padded, 0u);
    for (std::uint32_t i = 0; i < pageCount; ++i) {
        labels[i] = i + 1u;
    }

    for (std::uint32_t sig = 0; sig < result.signatureCount; ++sig) {
        const std::uint32_t base = sig * sigSize;
        const std::uint32_t sheetsInSig = sigSize / 4u;

        for (std::uint32_t sh = 0; sh < sheetsInSig; ++sh) {
            ShuffleAssistantSheet sheet {};
            sheet.signatureIndex   = sig;
            sheet.sheetInSignature = sh;

            // Outer sheet: indices 0 and sigSize-1 form the outermost sheet front/back.
            // Pattern: front = [outerLeft, outerRight], back = [innerLeft, innerRight]
            // where outerLeft=base+2*sh, outerRight=base+sigSize-1-2*sh,
            //       innerLeft=base+2*sh+1, innerRight=base+sigSize-2-2*sh.
            const std::uint32_t iFL = base + 2u * sh;
            const std::uint32_t iFR = base + sigSize - 1u - 2u * sh;
            const std::uint32_t iBL = base + 2u * sh + 1u;
            const std::uint32_t iBR = base + sigSize - 2u - 2u * sh;

            sheet.frontLeft      = labels[iFL];
            sheet.frontRight     = labels[iFR];
            sheet.backLeft       = labels[iBL];
            sheet.backRight      = labels[iBR];
            sheet.frontLeftBlank  = (labels[iFL] == 0u);
            sheet.frontRightBlank = (labels[iFR] == 0u);
            sheet.backLeftBlank   = (labels[iBL] == 0u);
            sheet.backRightBlank  = (labels[iBR] == 0u);

            result.sheets.push_back(sheet);
        }
    }

    return result;
}

std::string ShuffleAssistantToJson(const ShuffleAssistantResult& result) {
    std::ostringstream out;
    out << "{\n";
    out << "  \"kind\": \"shuffle-assistant\",\n";
    out << "  \"inputPageCount\": "  << result.inputPageCount  << ",\n";
    out << "  \"signatureSize\": "   << result.signatureSize   << ",\n";
    out << "  \"signatureCount\": "  << result.signatureCount  << ",\n";
    out << "  \"paddedPageCount\": " << result.paddedPageCount << ",\n";
    out << "  \"blankPagesAdded\": " << result.blankPagesAdded << ",\n";
    out << "  \"sheets\": [\n";
    for (std::size_t i = 0; i < result.sheets.size(); ++i) {
        const auto& s = result.sheets[i];
        auto lbl = [](std::uint32_t v, bool blank) -> std::string {
            return blank ? "null" : std::to_string(v);
        };
        out << "    {\"signatureIndex\": "    << s.signatureIndex
            << ", \"sheetInSignature\": "     << s.sheetInSignature
            << ", \"frontLeft\": "            << lbl(s.frontLeft,  s.frontLeftBlank)
            << ", \"frontRight\": "           << lbl(s.frontRight, s.frontRightBlank)
            << ", \"backLeft\": "             << lbl(s.backLeft,   s.backLeftBlank)
            << ", \"backRight\": "            << lbl(s.backRight,  s.backRightBlank)
            << "}";
        if (i + 1 < result.sheets.size()) out << ",";
        out << "\n";
    }
    out << "  ]\n}\n";
    return out.str();
}

} // namespace aimp
