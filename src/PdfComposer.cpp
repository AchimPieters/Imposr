#include "aimp/PdfComposer.h"

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <vector>

namespace aimp {

namespace {

std::string EscapePdfText(const std::string& input) {
    std::string out;
    out.reserve(input.size());
    for (char ch : input) {
        if (ch == '\\' || ch == '(' || ch == ')') {
            out.push_back('\\');
        }
        out.push_back(ch);
    }
    return out;
}

std::size_t SheetCount(const ImpositionPlan& plan) {
    std::size_t maxSheet = 0;
    for (const auto& placement : plan.placements) {
        maxSheet = std::max(maxSheet, static_cast<std::size_t>(placement.sheetIndex));
    }
    return plan.placements.empty() ? 1 : maxSheet + 1;
}

void StrokeRect(std::ostringstream& content, const Rect& rect) {
    content << rect.x << ' ' << rect.y << ' ' << rect.width << ' ' << rect.height << " re S\n";
}

void FillRect(std::ostringstream& content, const Rect& rect) {
    content << rect.x << ' ' << rect.y << ' ' << rect.width << ' ' << rect.height << " re f\n";
}

void DrawCrosshair(std::ostringstream& content, double x, double y, double size) {
    content << x - size << ' ' << y << " m " << x + size << ' ' << y << " l S\n";
    content << x << ' ' << y - size << " m " << x << ' ' << y + size << " l S\n";
}

std::string FormatPlacementLabel(const SlotPlacement& placement,
                                 const PdfComposeOptions& options,
                                 std::uint32_t& batesCounter) {
    std::ostringstream line;
    line << "slot " << placement.slotIndex << " • ";
    if (placement.sourcePage.pageIndex == kBlankPageIndex) {
        line << "blank";
    } else {
        line << placement.sourcePage.sourceDocumentId << " p" << (placement.sourcePage.pageIndex + 1u);
    }
    if (placement.rotationDegrees != 0.0) {
        line << " • rot=" << placement.rotationDegrees;
    }
    if (placement.scale != 1.0) {
        line << " • scale=" << std::fixed << std::setprecision(3) << placement.scale;
    }
    if (options.includeBates) {
        line << " • " << options.batesPrefix << batesCounter++;
    }
    return line.str();
}

} // namespace

bool ComposePlanPdf(const ImpositionPlan& plan,
                    const std::string& outputPath,
                    const PdfComposeOptions& options,
                    std::string& errorMessage) {
    std::ofstream out(outputPath, std::ios::binary);
    if (!out) {
        errorMessage = "Could not open output file";
        return false;
    }

    const double pageWidth = plan.outputSheet.widthPoints > 0.0 ? plan.outputSheet.widthPoints : 595.0;
    const double pageHeight = plan.outputSheet.heightPoints > 0.0 ? plan.outputSheet.heightPoints : 842.0;
    const std::size_t sheetCount = SheetCount(plan);

    const int fontObj = 3;
    const int firstPageObj = 4;
    const int objectCount = 3 + static_cast<int>(sheetCount) * 2;

    std::vector<std::string> objects(objectCount + 1);
    std::vector<int> pageObjectIds;
    pageObjectIds.reserve(sheetCount);

    std::uint32_t batesCounter = options.batesStart;
    for (std::size_t sheet = 0; sheet < sheetCount; ++sheet) {
        const int pageObj = firstPageObj + static_cast<int>(sheet) * 2;
        const int contentObj = pageObj + 1;
        pageObjectIds.push_back(pageObj);

        std::ostringstream content;
        content << "q\n";
        if (options.drawSheetBorder) {
            content << "0.2 w\n0 0 0 RG\n";
            StrokeRect(content, Rect {18.0, 18.0, pageWidth - 36.0, pageHeight - 36.0});
        }

        if (!options.headerText.empty()) {
            content << "BT /F1 12 Tf 36 " << (pageHeight - 24.0) << " Td (" << EscapePdfText(options.headerText) << ") Tj ET\n";
        }
        if (options.includeSheetNumber) {
            content << "BT /F1 14 Tf 36 " << (pageHeight - 42.0) << " Td (Imposition sheet " << (sheet + 1u) << ") Tj ET\n";
        }
        content << "BT /F1 10 Tf 36 " << (pageHeight - 58.0) << " Td (" << EscapePdfText(BuildHumanSummary(plan)) << ") Tj ET\n";

        for (const auto& placement : plan.placements) {
            if (placement.sheetIndex != sheet) {
                continue;
            }

            if (options.drawSlotOutlines) {
                if (placement.sourcePage.pageIndex == kBlankPageIndex) {
                    content << "0.70 0.70 0.70 RG 0.95 0.95 0.95 rg\n";
                    FillRect(content, placement.targetRect);
                    content << "0.55 0.55 0.55 RG\n";
                } else {
                    content << "0.12 0.12 0.12 RG\n";
                }
                content << "0.75 w\n";
                StrokeRect(content, placement.targetRect);
            }

            if (options.drawCenterMarks) {
                content << "0.35 w\n";
                DrawCrosshair(content,
                              placement.targetRect.x + placement.targetRect.width / 2.0,
                              placement.targetRect.y + placement.targetRect.height / 2.0,
                              8.0);
            }

            if (options.drawSlotLabels) {
                const auto label = FormatPlacementLabel(placement, options, batesCounter);
                const double labelY = std::max(placement.targetRect.y + placement.targetRect.height - 14.0, 24.0);
                content << "BT /F1 9 Tf " << (placement.targetRect.x + 6.0) << ' ' << labelY
                        << " Td (" << EscapePdfText(label) << ") Tj ET\n";
                std::ostringstream rectLine;
                rectLine << "rect=(" << std::fixed << std::setprecision(1)
                         << placement.targetRect.x << ',' << placement.targetRect.y << ','
                         << placement.targetRect.width << ',' << placement.targetRect.height << ')';
                content << "BT /F1 8 Tf " << (placement.targetRect.x + 6.0) << ' ' << std::max(labelY - 11.0, 16.0)
                        << " Td (" << EscapePdfText(rectLine.str()) << ") Tj ET\n";
            }
        }

        if (!options.footerText.empty()) {
            content << "BT /F1 10 Tf 36 24 Td (" << EscapePdfText(options.footerText) << ") Tj ET\n";
        }
        content << "Q\n";

        const std::string contentData = content.str();
        std::ostringstream contentObjData;
        contentObjData << "<< /Length " << contentData.size() << " >>\nstream\n"
                       << contentData << "endstream\n";
        objects[contentObj] = contentObjData.str();

        std::ostringstream pageObjData;
        pageObjData << "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 " << pageWidth << ' ' << pageHeight << "] "
                    << "/Resources << /Font << /F1 " << fontObj << " 0 R >> >> "
                    << "/Contents " << contentObj << " 0 R >>\n";
        objects[pageObj] = pageObjData.str();
    }

    std::ostringstream kids;
    for (int pageObj : pageObjectIds) {
        kids << pageObj << " 0 R ";
    }

    objects[1] = "<< /Type /Catalog /Pages 2 0 R >>\n";
    {
        std::ostringstream pagesObj;
        pagesObj << "<< /Type /Pages /Count " << sheetCount << " /Kids [ " << kids.str() << "] >>\n";
        objects[2] = pagesObj.str();
    }
    objects[3] = "<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>\n";

    out << "%PDF-1.4\n";
    std::vector<long> offsets(objectCount + 1, 0);
    for (int i = 1; i <= objectCount; ++i) {
        offsets[i] = static_cast<long>(out.tellp());
        out << i << " 0 obj\n" << objects[i] << "endobj\n";
    }

    const long xrefOffset = static_cast<long>(out.tellp());
    out << "xref\n0 " << (objectCount + 1) << "\n";
    out << "0000000000 65535 f \n";
    for (int i = 1; i <= objectCount; ++i) {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%010ld 00000 n \n", offsets[i]);
        out << buf;
    }
    out << "trailer\n<< /Size " << (objectCount + 1) << " /Root 1 0 R >>\n";
    out << "startxref\n" << xrefOffset << "\n%%EOF\n";

    if (!out.good()) {
        errorMessage = "Failed while writing PDF output";
        return false;
    }
    return true;
}

bool ComposePlanPdf(const ImpositionPlan& plan, const std::string& outputPath, std::string& errorMessage) {
    return ComposePlanPdf(plan, outputPath, PdfComposeOptions {}, errorMessage);
}

} // namespace aimp
