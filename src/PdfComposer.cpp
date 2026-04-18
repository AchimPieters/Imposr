#include "aimp/PdfComposer.h"

#include <algorithm>
#include <charconv>
#include <cctype>
#include <cstdio>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <unordered_map>
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

void DrawTrimMarks(std::ostringstream& content,
                   const Rect& rect,
                   double markLength,
                   double markOffset) {
    const double left = rect.x;
    const double right = rect.x + rect.width;
    const double bottom = rect.y;
    const double top = rect.y + rect.height;

    // Left-bottom corner.
    content << left - markOffset - markLength << ' ' << bottom << " m "
            << left - markOffset << ' ' << bottom << " l S\n";
    content << left << ' ' << bottom - markOffset - markLength << " m "
            << left << ' ' << bottom - markOffset << " l S\n";

    // Left-top corner.
    content << left - markOffset - markLength << ' ' << top << " m "
            << left - markOffset << ' ' << top << " l S\n";
    content << left << ' ' << top + markOffset << " m "
            << left << ' ' << top + markOffset + markLength << " l S\n";

    // Right-bottom corner.
    content << right + markOffset << ' ' << bottom << " m "
            << right + markOffset + markLength << ' ' << bottom << " l S\n";
    content << right << ' ' << bottom - markOffset - markLength << " m "
            << right << ' ' << bottom - markOffset << " l S\n";

    // Right-top corner.
    content << right + markOffset << ' ' << top << " m "
            << right + markOffset + markLength << ' ' << top << " l S\n";
    content << right << ' ' << top + markOffset << " m "
            << right << ' ' << top + markOffset + markLength << " l S\n";
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

std::vector<std::string> ParseCsvLine(const std::string& line) {
    std::vector<std::string> fields;
    std::string current;
    bool inQuotes = false;
    for (std::size_t i = 0; i < line.size(); ++i) {
        const char ch = line[i];
        if (ch == '"') {
            if (inQuotes && i + 1 < line.size() && line[i + 1] == '"') {
                current.push_back('"');
                ++i;
            } else {
                inQuotes = !inQuotes;
            }
            continue;
        }
        if (ch == ',' && !inQuotes) {
            fields.push_back(current);
            current.clear();
            continue;
        }
        current.push_back(ch);
    }
    fields.push_back(current);
    return fields;
}

std::string TrimAscii(std::string value) {
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front())) != 0) {
        value.erase(value.begin());
    }
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back())) != 0) {
        value.pop_back();
    }
    return value;
}

using VariableMap = std::unordered_map<std::string, std::string>;
using VariableByPageMap = std::unordered_map<std::uint32_t, VariableMap>;

bool LoadVariableDataCsv(const std::string& path, VariableByPageMap& out, std::string& errorMessage) {
    out.clear();
    if (path.empty()) {
        return true;
    }

    std::ifstream in(path);
    if (!in) {
        errorMessage = "Could not open variable data CSV file";
        return false;
    }

    std::string headerLine;
    if (!std::getline(in, headerLine)) {
        errorMessage = "Variable data CSV is empty";
        return false;
    }
    const auto headers = ParseCsvLine(headerLine);
    if (headers.empty()) {
        errorMessage = "Variable data CSV header is invalid";
        return false;
    }
    if (TrimAscii(headers[0]) != "page") {
        errorMessage = "Variable data CSV must start with 'page' column";
        return false;
    }

    std::string line;
    std::size_t lineNumber = 1;
    while (std::getline(in, line)) {
        ++lineNumber;
        if (TrimAscii(line).empty()) {
            continue;
        }
        const auto fields = ParseCsvLine(line);
        if (fields.empty()) {
            continue;
        }

        std::uint32_t humanPage = 0;
        const std::string pageField = TrimAscii(fields[0]);
        const char* begin = pageField.data();
        const char* end = pageField.data() + pageField.size();
        const auto parseResult = std::from_chars(begin, end, humanPage);
        if (parseResult.ec != std::errc {} || parseResult.ptr != end || humanPage == 0) {
            errorMessage = "Invalid page value in variable CSV at line " + std::to_string(lineNumber);
            return false;
        }

        VariableMap row;
        for (std::size_t i = 1; i < headers.size(); ++i) {
            const std::string key = TrimAscii(headers[i]);
            if (key.empty()) {
                continue;
            }
            const std::string value = i < fields.size() ? TrimAscii(fields[i]) : "";
            row[key] = value;
        }
        out[humanPage - 1u] = row;
    }

    return true;
}

std::string ExpandOverlayTemplate(const std::string& templ, const VariableMap& values) {
    std::string out;
    out.reserve(templ.size() + 32);
    for (std::size_t i = 0; i < templ.size();) {
        if (i + 1 < templ.size() && templ[i] == '{' && templ[i + 1] == '{') {
            const std::size_t end = templ.find("}}", i + 2);
            if (end != std::string::npos) {
                const std::string key = templ.substr(i + 2, end - (i + 2));
                const auto it = values.find(key);
                if (it != values.end()) {
                    out += it->second;
                }
                i = end + 2;
                continue;
            }
        }
        out.push_back(templ[i]);
        ++i;
    }
    return out;
}

} // namespace

const char* PdfxProfileName(PdfxProfile profile) {
    switch (profile) {
        case PdfxProfile::None: return "none";
        case PdfxProfile::Pdfx1a2001: return "pdfx-1a";
        case PdfxProfile::Pdfx4: return "pdfx-4";
        default: return "none";
    }
}

bool TryParsePdfxProfile(const std::string& value, PdfxProfile& outProfile) {
    if (value == "none") {
        outProfile = PdfxProfile::None;
        return true;
    }
    if (value == "pdfx-1a" || value == "pdfx-1a:2001") {
        outProfile = PdfxProfile::Pdfx1a2001;
        return true;
    }
    if (value == "pdfx-4" || value == "pdfx-4:2010") {
        outProfile = PdfxProfile::Pdfx4;
        return true;
    }
    return false;
}

std::vector<PreflightIssue> ValidatePrepressReadiness(const ImpositionPlan& plan,
                                                      const PdfComposeOptions& options) {
    std::vector<PreflightIssue> issues;

    if (plan.outputSheet.widthPoints <= 0.0 || plan.outputSheet.heightPoints <= 0.0) {
        issues.push_back({"sheet.invalid-size", "Output sheet size must be greater than zero.", true});
    }

    if (options.drawTrimMarks && options.trimMarkLengthPoints <= 0.0) {
        issues.push_back({"trim.invalid-length", "Trim mark length must be > 0 when trim marks are enabled.", true});
    }
    if (options.trimMarkOffsetPoints < 0.0) {
        issues.push_back({"trim.invalid-offset", "Trim mark offset must be >= 0.", true});
    }
    if (options.drawBleedBox && options.bleedPoints <= 0.0) {
        issues.push_back({"bleed.invalid-size", "Bleed points must be > 0 when bleed box drawing is enabled.", true});
    }
    if (!options.overlayTemplate.empty() && options.variableDataCsvPath.empty()) {
        issues.push_back({"overlay.csv-missing", "Overlay template is set without variable CSV; only built-in variables will be available.", false});
    }
    if (options.overlayTemplate.empty() && !options.variableDataCsvPath.empty()) {
        issues.push_back({"overlay.template-missing", "Variable CSV is set without an overlay template.", false});
    }

    if (options.targetPdfxProfile != PdfxProfile::None) {
        if (!options.drawTrimMarks) {
            issues.push_back({"pdfx.trim-required", "Selected PDF/X profile requires trim marks to be enabled for proof parity.", true});
        }
        if (!options.drawBleedBox || options.bleedPoints <= 0.0) {
            issues.push_back({"pdfx.bleed-required", "Selected PDF/X profile requires bleed box visualization with bleed > 0.", true});
        }
        for (const auto& placement : plan.placements) {
            if (placement.sourcePage.pageIndex == kBlankPageIndex) {
                issues.push_back({"pdfx.blank-placement", "Plan contains blank placements; verify this is allowed for final PDF/X delivery.", false});
                break;
            }
        }
    }

    return issues;
}

std::string ToProductionCompositionJson(const ImpositionPlan& plan,
                                        const BuildOptions& buildOptions,
                                        const PdfComposeOptions& options) {
    std::ostringstream out;
    out << "{\n";
    out << "  \"kind\": \"acrobat-production-composition\",\n";
    out << "  \"mode\": \"" << LayoutModeName(plan.mode) << "\",\n";
    out << "  \"sheet\": {\"widthPoints\": " << plan.outputSheet.widthPoints
        << ", \"heightPoints\": " << plan.outputSheet.heightPoints << "},\n";
    out << "  \"prepress\": {\n";
    out << "    \"pdfxProfile\": \"" << PdfxProfileName(options.targetPdfxProfile) << "\",\n";
    out << "    \"trimMarks\": " << (options.drawTrimMarks ? "true" : "false") << ",\n";
    out << "    \"trimMarkLengthPoints\": " << options.trimMarkLengthPoints << ",\n";
    out << "    \"trimMarkOffsetPoints\": " << options.trimMarkOffsetPoints << ",\n";
    out << "    \"bleedBox\": " << (options.drawBleedBox ? "true" : "false") << ",\n";
    out << "    \"bleedPoints\": " << options.bleedPoints << ",\n";
    out << "    \"bookletCreepPerSheetPoints\": " << buildOptions.bookletCreepPerSheetPoints << ",\n";
    out << "    \"overlayTemplate\": \"" << EscapePdfText(options.overlayTemplate) << "\",\n";
    out << "    \"variableDataCsvPath\": \"" << EscapePdfText(options.variableDataCsvPath) << "\"\n";
    out << "  },\n";
    out << "  \"placements\": [\n";
    for (std::size_t i = 0; i < plan.placements.size(); ++i) {
        const auto& p = plan.placements[i];
        double a = p.targetRect.width;
        double b = 0.0;
        double c = 0.0;
        double d = p.targetRect.height;
        double e = p.targetRect.x;
        double f = p.targetRect.y;
        if (p.rotationDegrees == 90.0) {
            a = 0.0;
            b = p.targetRect.height;
            c = -p.targetRect.width;
            d = 0.0;
            e = p.targetRect.x + p.targetRect.width;
            f = p.targetRect.y;
        } else if (p.rotationDegrees == 180.0) {
            a = -p.targetRect.width;
            d = -p.targetRect.height;
            e = p.targetRect.x + p.targetRect.width;
            f = p.targetRect.y + p.targetRect.height;
        } else if (p.rotationDegrees == 270.0) {
            a = 0.0;
            b = -p.targetRect.height;
            c = p.targetRect.width;
            d = 0.0;
            e = p.targetRect.x;
            f = p.targetRect.y + p.targetRect.height;
        }

        out << "    {\"sheetIndex\": " << p.sheetIndex
            << ", \"slotIndex\": " << p.slotIndex
            << ", \"sourcePageIndex\": " << p.sourcePage.pageIndex
            << ", \"rotationDegrees\": " << p.rotationDegrees
            << ", \"ctm\": {\"a\": " << a
            << ", \"b\": " << b
            << ", \"c\": " << c
            << ", \"d\": " << d
            << ", \"e\": " << e
            << ", \"f\": " << f << "}}";
        if (i + 1 < plan.placements.size()) {
            out << ',';
        }
        out << '\n';
    }
    out << "  ]\n";
    out << "}\n";
    return out.str();
}

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
    VariableByPageMap csvVariables;
    if (!LoadVariableDataCsv(options.variableDataCsvPath, csvVariables, errorMessage)) {
        return false;
    }

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

            if (options.drawBleedBox && options.bleedPoints > 0.0) {
                const Rect bleedRect {
                    placement.targetRect.x - options.bleedPoints,
                    placement.targetRect.y - options.bleedPoints,
                    placement.targetRect.width + options.bleedPoints * 2.0,
                    placement.targetRect.height + options.bleedPoints * 2.0
                };
                content << "% bleed-box\n";
                content << "0.85 0.05 0.05 RG\n";
                content << "0.35 w\n";
                StrokeRect(content, bleedRect);
            }

            if (options.drawTrimMarks) {
                const double length = std::max(options.trimMarkLengthPoints, 1.0);
                const double offset = std::max(options.trimMarkOffsetPoints, 0.0);
                content << "% trim-marks\n";
                content << "0.0 0.0 0.0 RG\n";
                content << "0.45 w\n";
                DrawTrimMarks(content, placement.targetRect, length, offset);
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

            if (!options.overlayTemplate.empty()) {
                VariableMap variables;
                variables["sheet"] = std::to_string(placement.sheetIndex + 1u);
                variables["slot"] = std::to_string(placement.slotIndex);
                if (placement.sourcePage.pageIndex == kBlankPageIndex) {
                    variables["page"] = "blank";
                    variables["doc"] = "";
                } else {
                    variables["page"] = std::to_string(placement.sourcePage.pageIndex + 1u);
                    variables["doc"] = placement.sourcePage.sourceDocumentId;
                }
                const auto csvIt = csvVariables.find(placement.sourcePage.pageIndex);
                if (csvIt != csvVariables.end()) {
                    for (const auto& pair : csvIt->second) {
                        variables[pair.first] = pair.second;
                    }
                }
                const std::string overlay = ExpandOverlayTemplate(options.overlayTemplate, variables);
                const double overlayY = std::max(placement.targetRect.y + 6.0, 14.0);
                content << "BT /F1 8 Tf " << (placement.targetRect.x + 6.0) << ' ' << overlayY
                        << " Td (" << EscapePdfText(overlay) << ") Tj ET\n";
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
