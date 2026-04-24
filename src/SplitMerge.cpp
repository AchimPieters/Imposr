#include "aimp/SplitMerge.h"
#include "EscapeUtils.h"

#include <algorithm>
#include <charconv>
#include <sstream>

namespace aimp {

namespace {

using internal::EscapeJson;

bool ParseUInt32(const std::string& s, std::uint32_t& out) {
    const char* begin = s.data();
    const char* end = s.data() + s.size();
    const auto r = std::from_chars(begin, end, out);
    return r.ec == std::errc{} && r.ptr == end;
}

std::string Trim(std::string s) {
    while (!s.empty() && (s.front() == ' ' || s.front() == '\t' || s.front() == '\r')) s.erase(s.begin());
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r')) s.pop_back();
    return s;
}

} // namespace

bool ParseRangeSpec(const std::string& spec,
                    std::uint32_t totalPages,
                    std::vector<PageRange>& outRanges,
                    std::string& errorMessage) {
    outRanges.clear();
    errorMessage.clear();

    if (spec.empty()) {
        errorMessage = "Range specification is empty";
        return false;
    }

    // Spec format: "1-4,5-8,9-12" or "1-4 5-8" (comma or space separated).
    // Each token is either "N" (single page) or "N-M" (inclusive range).
    std::string token;
    std::istringstream stream(spec);
    char sep = ',';

    while (std::getline(stream, token, sep)) {
        token = Trim(token);
        if (token.empty()) continue;

        PageRange range;
        const std::size_t dashPos = token.find('-');
        if (dashPos == std::string::npos) {
            std::uint32_t p = 0;
            if (!ParseUInt32(token, p) || p == 0 || p > totalPages) {
                errorMessage = "Invalid page number: " + token;
                return false;
            }
            range.startPage = p - 1u;
            range.endPage = p - 1u;
        } else {
            const std::string startStr = Trim(token.substr(0, dashPos));
            const std::string endStr = Trim(token.substr(dashPos + 1));
            std::uint32_t startH = 0;
            std::uint32_t endH = 0;
            if (!ParseUInt32(startStr, startH) || !ParseUInt32(endStr, endH)) {
                errorMessage = "Invalid range token: " + token;
                return false;
            }
            if (startH == 0 || endH == 0 || startH > totalPages || endH > totalPages || startH > endH) {
                errorMessage = "Invalid range bounds: " + token;
                return false;
            }
            range.startPage = startH - 1u;
            range.endPage = endH - 1u;
        }

        outRanges.push_back(range);
    }

    if (outRanges.empty()) {
        errorMessage = "No valid ranges parsed from spec: " + spec;
        return false;
    }

    return true;
}

SplitMergeResult SplitPlan(const ImpositionPlan& plan,
                            const std::vector<PageRange>& ranges) {
    SplitMergeResult result {};

    if (ranges.empty()) {
        result.errorMessage = "No ranges specified for split";
        return result;
    }

    result.ranges = ranges;

    for (const auto& range : ranges) {
        ImpositionPlan segment {};
        segment.mode = plan.mode;
        segment.outputSheet = plan.outputSheet;

        std::uint32_t newSheetIndex = 0;
        std::uint32_t prevOrigSheet = 0xFFFFFFFFu;

        for (const auto& placement : plan.placements) {
            const auto& src = placement.sourcePage;
            if (src.pageIndex == kBlankPageIndex) continue;
            if (src.pageIndex < range.startPage || src.pageIndex > range.endPage) continue;

            SlotPlacement sp = placement;
            if (placement.sheetIndex != prevOrigSheet) {
                if (prevOrigSheet != 0xFFFFFFFFu) ++newSheetIndex;
                prevOrigSheet = placement.sheetIndex;
            }
            sp.sheetIndex = newSheetIndex;
            segment.placements.push_back(sp);
            segment.sourcePageCount = std::max(segment.sourcePageCount, src.pageIndex + 1u);
        }

        segment.paddedPageCount = range.endPage - range.startPage + 1u;
        result.segments.push_back(std::move(segment));
    }

    result.success = true;
    return result;
}

ImpositionPlan MergePlans(const std::vector<ImpositionPlan>& plans) {
    ImpositionPlan merged {};

    if (plans.empty()) {
        return merged;
    }

    merged.mode = plans[0].mode;
    merged.outputSheet = plans[0].outputSheet;

    std::uint32_t sheetOffset = 0;
    for (const auto& plan : plans) {
        std::uint32_t maxSheet = 0;
        for (const auto& placement : plan.placements) {
            SlotPlacement sp = placement;
            sp.sheetIndex += sheetOffset;
            merged.placements.push_back(sp);
            maxSheet = std::max(maxSheet, placement.sheetIndex + 1u);
            merged.sourcePageCount = std::max(merged.sourcePageCount, placement.sourcePage.pageIndex != kBlankPageIndex
                ? placement.sourcePage.pageIndex + 1u : 0u);
        }
        sheetOffset += maxSheet;
        merged.paddedPageCount += plan.paddedPageCount;
    }

    return merged;
}

std::string SplitMergeResultToJson(const SplitMergeResult& result) {
    std::ostringstream out;
    out << "{\n";
    out << "  \"kind\": \"split-merge-result\",\n";
    out << "  \"success\": " << (result.success ? "true" : "false") << ",\n";
    out << "  \"errorMessage\": \"" << EscapeJson(result.errorMessage) << "\",\n";
    out << "  \"segmentCount\": " << result.segments.size() << ",\n";
    out << "  \"segments\": [\n";
    for (std::size_t i = 0; i < result.segments.size(); ++i) {
        const auto& seg = result.segments[i];
        out << "    {\"placementCount\": " << seg.placements.size()
            << ", \"sourcePageCount\": " << seg.sourcePageCount
            << ", \"paddedPageCount\": " << seg.paddedPageCount;
        if (i < result.ranges.size()) {
            const auto& r = result.ranges[i];
            out << ", \"rangeStart\": " << r.startPage
                << ", \"rangeEnd\": " << r.endPage;
            if (!r.label.empty()) {
                out << ", \"label\": \"" << EscapeJson(r.label) << "\"";
            }
        }
        out << "}";
        if (i + 1 < result.segments.size()) out << ",";
        out << "\n";
    }
    out << "  ]\n";
    out << "}\n";
    return out.str();
}

} // namespace aimp
