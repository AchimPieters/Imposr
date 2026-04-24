#include "aimp/ImpositionInfo.h"
#include "EscapeUtils.h"

#include <algorithm>
#include <sstream>

namespace aimp {

namespace {
using internal::EscapeJson;
} // namespace

ImpositionInfoResult BuildImpositionInfo(const ImpositionPlan& plan) {
    ImpositionInfoResult result {};

    std::uint32_t maxSheet = 0;
    for (const auto& p : plan.placements) {
        if (p.sheetIndex > maxSheet) maxSheet = p.sheetIndex;
    }
    result.totalSheets = plan.placements.empty() ? 0 : maxSheet + 1;
    result.totalSlots  = static_cast<std::uint32_t>(plan.placements.size());

    for (const auto& p : plan.placements) {
        ImpositionInfoRecord rec {};
        rec.sheetIndex      = p.sheetIndex;
        rec.slotIndex       = p.slotIndex;
        rec.isBlank         = (p.sourcePage.pageIndex == kBlankPageIndex);
        rec.sourceDocumentId = p.sourcePage.sourceDocumentId;
        rec.sourcePageIndex = p.sourcePage.pageIndex;
        rec.targetRect      = p.targetRect;
        rec.rotationDegrees = p.rotationDegrees;
        rec.scale           = p.scale;

        if (rec.isBlank) {
            ++result.blankSlots;
        } else {
            ++result.nonBlankSlots;
        }
        result.records.push_back(rec);
    }

    return result;
}

std::vector<PageRef> ExtractPageRefsForSheet(const ImpositionPlan& plan,
                                              std::uint32_t sheetIndex) {
    std::vector<PageRef> refs;
    for (const auto& p : plan.placements) {
        if (p.sheetIndex == sheetIndex && p.sourcePage.pageIndex != kBlankPageIndex) {
            refs.push_back(p.sourcePage);
        }
    }
    return refs;
}

std::string ImpositionInfoToJson(const ImpositionInfoResult& result) {
    std::ostringstream out;
    out << "{\n";
    out << "  \"kind\": \"imposition-info\",\n";
    out << "  \"totalSheets\": "   << result.totalSheets   << ",\n";
    out << "  \"totalSlots\": "    << result.totalSlots    << ",\n";
    out << "  \"blankSlots\": "    << result.blankSlots    << ",\n";
    out << "  \"nonBlankSlots\": " << result.nonBlankSlots << ",\n";
    out << "  \"records\": [\n";
    for (std::size_t i = 0; i < result.records.size(); ++i) {
        const auto& r = result.records[i];
        out << "    {"
            << "\"sheetIndex\": "      << r.sheetIndex
            << ", \"slotIndex\": "     << r.slotIndex
            << ", \"isBlank\": "       << (r.isBlank ? "true" : "false")
            << ", \"sourceDoc\": \""   << EscapeJson(r.sourceDocumentId) << "\""
            << ", \"sourcePage\": "    << (r.isBlank ? -1 : static_cast<int>(r.sourcePageIndex))
            << ", \"rect\": {\"x\": "  << r.targetRect.x
            << ", \"y\": "             << r.targetRect.y
            << ", \"width\": "         << r.targetRect.width
            << ", \"height\": "        << r.targetRect.height << "}"
            << ", \"rotation\": "      << r.rotationDegrees
            << ", \"scale\": "         << r.scale
            << "}";
        if (i + 1 < result.records.size()) out << ",";
        out << "\n";
    }
    out << "  ]\n}\n";
    return out.str();
}

} // namespace aimp
