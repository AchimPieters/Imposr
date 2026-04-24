#include "aimp/StickOn.h"
#include "EscapeUtils.h"

#include <algorithm>
#include <cstring>
#include <iomanip>
#include <set>
#include <sstream>

namespace aimp {

namespace {

using internal::EscapeJson;
using internal::EscapePdf;

std::string ExpandTemplateVars(const std::string& text, const StickOnContext* ctx) {
    if (ctx == nullptr || text.find("{{") == std::string::npos) {
        return text;
    }
    std::string out = text;
    auto replaceAll = [](std::string& s, const char* from, const std::string& to) {
        const std::size_t fromLen = std::strlen(from);
        std::size_t pos = 0;
        while ((pos = s.find(from, pos)) != std::string::npos) {
            s.replace(pos, fromLen, to);
            pos += to.size();
        }
    };
    replaceAll(out, "{{filename}}",   ctx->filename);
    replaceAll(out, "{{date}}",       ctx->date);
    replaceAll(out, "{{datetime}}",   ctx->datetime);
    replaceAll(out, "{{pagecount}}",  std::to_string(ctx->totalPageCount));
    replaceAll(out, "{{sheetcount}}", std::to_string(ctx->totalSheetCount));
    return out;
}

// Alias for PDF string escaping (parentheses syntax).
inline std::string EscapePdfStr(const std::string& s) { return internal::EscapePdf(s); }

std::string FormatBates(std::uint32_t number,
                        const std::string& prefix,
                        const std::string& suffix,
                        std::uint32_t padWidth) {
    std::ostringstream out;
    out << prefix;
    out << std::setw(static_cast<int>(padWidth)) << std::setfill('0') << number;
    out << suffix;
    return out.str();
}

Rect ComputeAnchoredRect(const Rect& slotRect,
                          const Rect& itemRect,
                          StickOnAnchor anchor) {
    double x = slotRect.x;
    double y = slotRect.y;

    switch (anchor) {
        case StickOnAnchor::TopLeft:
            x = slotRect.x;
            y = slotRect.y + slotRect.height - itemRect.height;
            break;
        case StickOnAnchor::TopCenter:
            x = slotRect.x + (slotRect.width - itemRect.width) / 2.0;
            y = slotRect.y + slotRect.height - itemRect.height;
            break;
        case StickOnAnchor::TopRight:
            x = slotRect.x + slotRect.width - itemRect.width;
            y = slotRect.y + slotRect.height - itemRect.height;
            break;
        case StickOnAnchor::MiddleLeft:
            x = slotRect.x;
            y = slotRect.y + (slotRect.height - itemRect.height) / 2.0;
            break;
        case StickOnAnchor::MiddleCenter:
            x = slotRect.x + (slotRect.width - itemRect.width) / 2.0;
            y = slotRect.y + (slotRect.height - itemRect.height) / 2.0;
            break;
        case StickOnAnchor::MiddleRight:
            x = slotRect.x + slotRect.width - itemRect.width;
            y = slotRect.y + (slotRect.height - itemRect.height) / 2.0;
            break;
        case StickOnAnchor::BottomLeft:
            x = slotRect.x;
            y = slotRect.y;
            break;
        case StickOnAnchor::BottomCenter:
            x = slotRect.x + (slotRect.width - itemRect.width) / 2.0;
            y = slotRect.y;
            break;
        case StickOnAnchor::BottomRight:
            x = slotRect.x + slotRect.width - itemRect.width;
            y = slotRect.y;
            break;
    }

    return Rect {x, y, itemRect.width, itemRect.height};
}

const char* StickOnTypeName(StickOnType t) {
    switch (t) {
        case StickOnType::Text: return "text";
        case StickOnType::BatesNumber: return "bates-number";
        case StickOnType::PageNumber: return "page-number";
        case StickOnType::PdfPage: return "pdf-page";
        case StickOnType::MaskingTape: return "masking-tape";
        case StickOnType::PeelOff: return "peel-off";
        default: return "unknown";
    }
}

} // namespace

StickOnResult ResolveStickOnItems(const ImpositionPlan& plan,
                                   const std::vector<StickOnItem>& items,
                                   const StickOnContext* context) {
    StickOnResult result {};

    // Track Bates counters per item index.
    std::vector<std::uint32_t> batesCounters(items.size(), 0);
    for (std::size_t itemIdx = 0; itemIdx < items.size(); ++itemIdx) {
        batesCounters[itemIdx] = items[itemIdx].batesStart;
    }

    for (const auto& placement : plan.placements) {
        for (std::size_t itemIdx = 0; itemIdx < items.size(); ++itemIdx) {
            const auto& item = items[itemIdx];

            // applyToPageIndices filters by source-document page index (0-based),
            // NOT by visual/placement order. For booklets, the mapping from
            // "page 1" in the source document to a specific sheet/slot is
            // non-trivial. Users specifying --apply-to-pages should be aware
            // they are addressing source page indices, not imposed output positions.
            if (!item.applyToAllPages && !item.applyToPageIndices.empty()) {
                const auto& indices = item.applyToPageIndices;
                const bool targeted = std::find(indices.begin(), indices.end(),
                    placement.sourcePage.pageIndex) != indices.end();
                if (!targeted) continue;
            }

            ResolvedStickOnOp op {};
            op.sheetIndex = placement.sheetIndex;
            op.slotIndex = placement.slotIndex;
            op.type = item.type;
            op.fontSize = item.fontSize;
            op.opacity = item.opacity;
            op.colorR = item.colorR;
            op.colorG = item.colorG;
            op.colorB = item.colorB;
            op.sourcePdfPath = item.sourcePdfPath;
            op.sourcePdfPage = item.sourcePdfPage;

            // Compute anchored rect.
            op.rect = ComputeAnchoredRect(placement.targetRect, item.rect, item.anchor);

            switch (item.type) {
                case StickOnType::Text:
                    op.resolvedText = ExpandTemplateVars(item.text, context);
                    break;
                case StickOnType::BatesNumber:
                    op.resolvedText = FormatBates(batesCounters[itemIdx],
                                                  item.batesPrefix,
                                                  item.batesSuffix,
                                                  item.batesPadWidth);
                    ++batesCounters[itemIdx];
                    break;
                case StickOnType::PageNumber:
                    if (placement.sourcePage.pageIndex != kBlankPageIndex) {
                        op.resolvedText = std::to_string(placement.sourcePage.pageIndex + 1u);
                    } else {
                        op.resolvedText = "-";
                    }
                    break;
                case StickOnType::PdfPage:
                case StickOnType::MaskingTape:
                case StickOnType::PeelOff:
                    break;
            }

            result.ops.push_back(std::move(op));
            ++result.opsGenerated;
        }
    }

    return result;
}

std::string RenderStickOnPdfContent(const std::vector<ResolvedStickOnOp>& ops,
                                     std::uint32_t sheetIndex) {
    std::ostringstream content;
    content << "q\n";

    for (const auto& op : ops) {
        if (op.sheetIndex != sheetIndex) continue;

        if (op.opacity < 1.0) {
            content << "/GS1 gs\n";
        }

        content << op.colorR << " " << op.colorG << " " << op.colorB << " rg\n";
        content << op.colorR << " " << op.colorG << " " << op.colorB << " RG\n";

        switch (op.type) {
            case StickOnType::Text:
            case StickOnType::BatesNumber:
            case StickOnType::PageNumber:
                content << "BT /F1 " << op.fontSize << " Tf "
                        << op.rect.x << " " << op.rect.y << " Td "
                        << "(" << EscapePdfStr(op.resolvedText) << ") Tj ET\n";
                break;
            case StickOnType::MaskingTape:
                content << "1 1 1 rg\n";
                content << op.rect.x << " " << op.rect.y << " "
                        << op.rect.width << " " << op.rect.height << " re f\n";
                break;
            case StickOnType::PeelOff:
                content << "0.95 0.95 0.85 rg\n";
                content << "0.5 w\n";
                content << op.rect.x << " " << op.rect.y << " "
                        << op.rect.width << " " << op.rect.height << " re B\n";
                content << "BT /F1 8 Tf "
                        << (op.rect.x + 2.0) << " " << (op.rect.y + 2.0)
                        << " Td (peel-off) Tj ET\n";
                break;
            case StickOnType::PdfPage:
                // Recorded in the ops list; the Acrobat SDK adapter places the actual PDF page.
                content << "% pdf-page-stamp: src=" << EscapePdfStr(op.sourcePdfPath)
                        << " page=" << op.sourcePdfPage << "\n";
                break;
        }
    }

    content << "Q\n";
    return content.str();
}

std::string StickOnResultToJson(const StickOnResult& result) {
    std::ostringstream out;
    out << "{\n";
    out << "  \"kind\": \"stick-on-result\",\n";
    out << "  \"opsGenerated\": " << result.opsGenerated << ",\n";
    out << "  \"ops\": [\n";
    for (std::size_t i = 0; i < result.ops.size(); ++i) {
        const auto& op = result.ops[i];
        out << "    {\"sheetIndex\": " << op.sheetIndex
            << ", \"slotIndex\": " << op.slotIndex
            << ", \"type\": \"" << StickOnTypeName(op.type) << "\""
            << ", \"resolvedText\": \"" << EscapeJson(op.resolvedText) << "\""
            << ", \"rect\": {\"x\": " << op.rect.x
            << ", \"y\": " << op.rect.y
            << ", \"width\": " << op.rect.width
            << ", \"height\": " << op.rect.height << "}"
            << ", \"fontSize\": " << op.fontSize
            << ", \"opacity\": " << op.opacity << "}";
        if (i + 1 < result.ops.size()) out << ",";
        out << "\n";
    }
    out << "  ]\n";
    out << "}\n";
    return out.str();
}

std::vector<ResolvedStickOnOp> PeelOffOps(const std::vector<ResolvedStickOnOp>& ops,
                                           const PeelOffSpec& spec) {
    std::vector<ResolvedStickOnOp> remaining;
    remaining.reserve(ops.size());

    for (const auto& op : ops) {
        // Sheet filter.
        if (spec.sheetIndex != PeelOffSpec::kNoSheetFilter &&
            op.sheetIndex != spec.sheetIndex) {
            remaining.push_back(op);
            continue;
        }

        bool remove = spec.removeAll;
        if (!remove) {
            switch (op.type) {
                case StickOnType::Text:        remove = spec.removeText;        break;
                case StickOnType::BatesNumber: remove = spec.removeBatesNumber; break;
                case StickOnType::PageNumber:  remove = spec.removePageNumber;  break;
                case StickOnType::PdfPage:     remove = spec.removePdfPage;     break;
                case StickOnType::MaskingTape: remove = spec.removeMaskingTape; break;
                case StickOnType::PeelOff:     remove = spec.removePeelOff;     break;
            }
        }

        if (!remove) {
            remaining.push_back(op);
        }
    }

    return remaining;
}

} // namespace aimp
