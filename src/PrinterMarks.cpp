#include "aimp/PrinterMarks.h"
#include "EscapeUtils.h"

#include <algorithm>
#include <cmath>
#include <sstream>

namespace aimp {

namespace {

using internal::EscapeJson;

inline std::string EscapePdfStr(const std::string& s) { return internal::EscapePdf(s); }

void DrawLine(std::ostringstream& c, double x1, double y1, double x2, double y2) {
    c << x1 << " " << y1 << " m " << x2 << " " << y2 << " l S\n";
}

void DrawRegistrationMark(std::ostringstream& c, double cx, double cy, double radius) {
    // Circle approximated with Bezier curves.
    const double k = 0.5523 * radius;
    c << "0 w\n";
    c << (cx + radius) << " " << cy << " m "
      << (cx + radius) << " " << (cy + k) << " "
      << (cx + k) << " " << (cy + radius) << " "
      << cx << " " << (cy + radius) << " c "
      << (cx - k) << " " << (cy + radius) << " "
      << (cx - radius) << " " << (cy + k) << " "
      << (cx - radius) << " " << cy << " c "
      << (cx - radius) << " " << (cy - k) << " "
      << (cx - k) << " " << (cy - radius) << " "
      << cx << " " << (cy - radius) << " c "
      << (cx + k) << " " << (cy - radius) << " "
      << (cx + radius) << " " << (cy - k) << " "
      << (cx + radius) << " " << cy << " c S\n";
    // Crosshair.
    DrawLine(c, cx - radius, cy, cx + radius, cy);
    DrawLine(c, cx, cy - radius, cx, cy + radius);
}

void DrawColorBar(std::ostringstream& c,
                  double x, double y,
                  double width, double height) {
    const double swatchWidth = width / 8.0;
    const double colors[8][3] = {
        {0,0,0}, {1,1,1}, {1,0,0}, {0,1,0},
        {0,0,1}, {1,1,0}, {0,1,1}, {1,0,1}
    };
    for (int i = 0; i < 8; ++i) {
        c << colors[i][0] << " " << colors[i][1] << " " << colors[i][2] << " rg\n";
        c << (x + i * swatchWidth) << " " << y << " "
          << swatchWidth << " " << height << " re f\n";
    }
    c << "0 g\n";
    c << "0.35 w\n";
    c << x << " " << y << " " << width << " " << height << " re S\n";
}

void DrawCropMarks(std::ostringstream& c,
                   const Rect& rect,
                   double length,
                   double offset,
                   double lw) {
    c << lw << " w\n";
    const double L = rect.x;
    const double R = rect.x + rect.width;
    const double B = rect.y;
    const double T = rect.y + rect.height;

    // Bottom-left.
    DrawLine(c, L - offset - length, B, L - offset, B);
    DrawLine(c, L, B - offset - length, L, B - offset);
    // Top-left.
    DrawLine(c, L - offset - length, T, L - offset, T);
    DrawLine(c, L, T + offset, L, T + offset + length);
    // Bottom-right.
    DrawLine(c, R + offset, B, R + offset + length, B);
    DrawLine(c, R, B - offset - length, R, B - offset);
    // Top-right.
    DrawLine(c, R + offset, T, R + offset + length, T);
    DrawLine(c, R, T + offset, R, T + offset + length);
}

void DrawBleedMarks(std::ostringstream& c,
                    const Rect& rect,
                    double bleed,
                    double length,
                    double lw) {
    c << "% bleed marks\n";
    c << "0.85 0.05 0.05 RG\n";
    c << lw << " w\n";
    const Rect bleedRect {
        rect.x - bleed,
        rect.y - bleed,
        rect.width + bleed * 2.0,
        rect.height + bleed * 2.0
    };
    const double L = bleedRect.x;
    const double R = bleedRect.x + bleedRect.width;
    const double B = bleedRect.y;
    const double T = bleedRect.y + bleedRect.height;
    DrawLine(c, L - length, B, L, B);
    DrawLine(c, L, B - length, L, B);
    DrawLine(c, R, B - length, R, B);
    DrawLine(c, R, B, R + length, B);
    DrawLine(c, L - length, T, L, T);
    DrawLine(c, L, T, L, T + length);
    DrawLine(c, R, T, R + length, T);
    DrawLine(c, R, T, R, T + length);
}

} // namespace

std::string PrinterMarksPdfContent(const ImpositionPlan& plan,
                                    std::uint32_t sheetIndex,
                                    const PrinterMarksConfig& config) {
    std::ostringstream c;
    c << "q\n";
    c << "0 0 0 RG\n0 0 0 rg\n";

    for (const auto& placement : plan.placements) {
        if (placement.sheetIndex != sheetIndex) continue;

        const Rect& rect = placement.targetRect;
        const double len = std::max(config.markLengthPoints, 1.0);
        const double off = std::max(config.markOffsetPoints, 0.0);
        const double lw = std::max(config.markLineWidthPoints, 0.1);

        if (config.cropMarks) {
            DrawCropMarks(c, rect, len, off, lw);
        }

        if (config.bleedMarks && config.bleedPoints > 0.0) {
            DrawBleedMarks(c, rect, config.bleedPoints, len, lw);
        }

        if (config.registrationMarks) {
            const double regSize = 6.0;
            const double regOff = off + len * 0.5;
            // Registration mark centers near each corner.
            DrawRegistrationMark(c, rect.x - regOff, rect.y + rect.height / 2.0, regSize);
            DrawRegistrationMark(c, rect.x + rect.width + regOff, rect.y + rect.height / 2.0, regSize);
            DrawRegistrationMark(c, rect.x + rect.width / 2.0, rect.y - regOff, regSize);
            DrawRegistrationMark(c, rect.x + rect.width / 2.0, rect.y + rect.height + regOff, regSize);
        }

        if (config.colorBar && config.colorBarHeightPoints > 0.0) {
            const double barY = rect.y - off - len - config.colorBarHeightPoints - 2.0;
            DrawColorBar(c, rect.x, barY, rect.width, config.colorBarHeightPoints);
        }

        if (config.jobInfoStrip && !config.jobName.empty()) {
            const double stripY = rect.y + rect.height + off + len + 2.0;
            c << "BT /F1 7 Tf " << rect.x << " " << stripY << " Td ("
              << EscapePdfStr(config.jobName);
            if (!config.jobDate.empty()) {
                c << " | " << EscapePdfStr(config.jobDate);
            }
            if (!config.jobInfo.empty()) {
                c << " | " << EscapePdfStr(config.jobInfo);
            }
            c << ") Tj ET\n";
        }
    }

    c << "Q\n";
    return c.str();
}

std::vector<PrinterMarksSheet> GeneratePrinterMarks(const ImpositionPlan& plan,
                                                     const PrinterMarksConfig& config) {
    std::vector<PrinterMarksSheet> sheets;

    std::uint32_t sheetCount = 0;
    for (const auto& p : plan.placements) {
        sheetCount = std::max(sheetCount, p.sheetIndex + 1u);
    }

    for (std::uint32_t si = 0; si < sheetCount; ++si) {
        PrinterMarksSheet sheet {};
        sheet.sheetIndex = si;
        sheet.pdfContentStream = PrinterMarksPdfContent(plan, si, config);
        sheets.push_back(std::move(sheet));
    }

    return sheets;
}

std::string PrinterMarksConfigToJson(const PrinterMarksConfig& config) {
    std::ostringstream out;
    out << "{\n";
    out << "  \"kind\": \"printer-marks-config\",\n";
    out << "  \"registrationMarks\": " << (config.registrationMarks ? "true" : "false") << ",\n";
    out << "  \"cropMarks\": " << (config.cropMarks ? "true" : "false") << ",\n";
    out << "  \"bleedMarks\": " << (config.bleedMarks ? "true" : "false") << ",\n";
    out << "  \"colorBar\": " << (config.colorBar ? "true" : "false") << ",\n";
    out << "  \"jobInfoStrip\": " << (config.jobInfoStrip ? "true" : "false") << ",\n";
    out << "  \"markLengthPoints\": " << config.markLengthPoints << ",\n";
    out << "  \"markOffsetPoints\": " << config.markOffsetPoints << ",\n";
    out << "  \"bleedPoints\": " << config.bleedPoints << ",\n";
    out << "  \"jobName\": \"" << EscapePdfStr(config.jobName) << "\"\n";
    out << "}\n";
    return out.str();
}

} // namespace aimp
