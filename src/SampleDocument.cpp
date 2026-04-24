#include "aimp/SampleDocument.h"

#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace aimp {

namespace {

// Approximate Helvetica character width at 1pt — used to centre page labels.
// Helvetica digit width is roughly 0.556 em.
constexpr double kHelvDigitWidthFactor = 0.556;

double ApproxTextWidth(const std::string& text, double fontSize) {
    return text.size() * kHelvDigitWidthFactor * fontSize;
}

} // namespace

bool CreateSampleDocument(const SampleDocumentOptions& options,
                           const std::string& outputPath,
                           std::string& errorMessage) {
    if (options.pageCount == 0) {
        errorMessage = "pageCount must be > 0";
        return false;
    }
    if (options.pageWidthPoints <= 0.0 || options.pageHeightPoints <= 0.0) {
        errorMessage = "page dimensions must be > 0";
        return false;
    }

    const double W = options.pageWidthPoints;
    const double H = options.pageHeightPoints;
    const double margin = 18.0;

    // Font size: 30% of the smaller dimension, clamped to [24, 200].
    double labelFontSize = std::min(W, H) * 0.30;
    if (labelFontSize < 24.0)  labelFontSize = 24.0;
    if (labelFontSize > 200.0) labelFontSize = 200.0;

    // PDF object layout:
    //   1 = Catalog
    //   2 = Pages
    //   3 = Font (Helvetica)
    //   Then for each page: contentObj, pageObj (2 objects each)
    const int kBaseObjects = 3;
    const int objectCount  = kBaseObjects + static_cast<int>(options.pageCount) * 2;

    std::vector<std::string> objects(static_cast<std::size_t>(objectCount + 1));
    std::vector<int> pageObjectIds;

    for (std::uint32_t pg = 0; pg < options.pageCount; ++pg) {
        const int contentObj = kBaseObjects + 1 + static_cast<int>(pg) * 2;
        const int pageObj    = contentObj + 1;
        pageObjectIds.push_back(pageObj);

        const std::uint32_t label = options.numberFromOne ? (pg + 1u) : pg;
        const std::string labelStr = std::to_string(label);

        const double textW = ApproxTextWidth(labelStr, labelFontSize);
        const double textX = (W - textW) / 2.0;
        const double textY = (H - labelFontSize) / 2.0;

        // Small caption font size for the secondary label.
        const double captionSize = std::max(8.0, labelFontSize * 0.18);
        const std::string caption = "Page " + labelStr + " of " + std::to_string(options.pageCount);
        const double captionW = ApproxTextWidth(caption, captionSize);
        const double captionX = (W - captionW) / 2.0;
        const double captionY = margin;

        std::ostringstream c;
        c << "q\n";

        // Light grey background fill.
        c << "0.96 0.96 0.96 rg\n";
        c << "0 0 " << W << ' ' << H << " re f\n";

        // White content area.
        c << "1 1 1 rg\n";
        c << margin << ' ' << margin << ' '
          << (W - 2 * margin) << ' ' << (H - 2 * margin) << " re f\n";

        // Diagonals (optional guide lines).
        if (options.drawDiagonals) {
            c << "0 0 0 RG 0.5 w\n";
            c << margin << ' ' << margin << " m "
              << (W - margin) << ' ' << (H - margin) << " l S\n";
            c << (W - margin) << ' ' << margin << " m "
              << margin << ' ' << (H - margin) << " l S\n";
        }

        // Border.
        if (options.drawBorder) {
            c << "0.4 0.4 0.4 RG 1 w\n";
            c << margin << ' ' << margin << ' '
              << (W - 2 * margin) << ' ' << (H - 2 * margin) << " re S\n";
        }

        // Large centred page number.
        c << "0.15 0.15 0.15 rg\n";
        c << "BT /F1 " << labelFontSize << " Tf "
          << textX << ' ' << textY << " Td ("
          << labelStr << ") Tj ET\n";

        // Caption below.
        c << "0.5 0.5 0.5 rg\n";
        c << "BT /F1 " << captionSize << " Tf "
          << captionX << ' ' << captionY << " Td ("
          << caption << ") Tj ET\n";

        c << "Q\n";

        const std::string contentData = c.str();
        std::ostringstream contentObjData;
        contentObjData << "<< /Length " << contentData.size()
                       << " >>\nstream\n" << contentData << "endstream\n";
        objects[static_cast<std::size_t>(contentObj)] = contentObjData.str();

        std::ostringstream pageObjData;
        pageObjData << "<< /Type /Page /Parent 2 0 R"
                    << " /MediaBox [0 0 " << W << ' ' << H << "]"
                    << " /Resources << /Font << /F1 3 0 R >> >>"
                    << " /Contents " << contentObj << " 0 R >>\n";
        objects[static_cast<std::size_t>(pageObj)] = pageObjData.str();
    }

    std::ostringstream kids;
    for (int id : pageObjectIds) kids << id << " 0 R ";

    objects[1] = "<< /Type /Catalog /Pages 2 0 R >>\n";
    {
        std::ostringstream pagesObj;
        pagesObj << "<< /Type /Pages /Count " << options.pageCount
                 << " /Kids [ " << kids.str() << "] >>\n";
        objects[2] = pagesObj.str();
    }
    objects[3] = "<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>\n";

    std::ofstream out(outputPath, std::ios::binary);
    if (!out) {
        errorMessage = "Could not open output file for writing: " + outputPath;
        return false;
    }

    out << "%PDF-1.4\n";
    std::vector<long> offsets(static_cast<std::size_t>(objectCount + 1), 0);
    for (int i = 1; i <= objectCount; ++i) {
        offsets[static_cast<std::size_t>(i)] = static_cast<long>(out.tellp());
        out << i << " 0 obj\n" << objects[static_cast<std::size_t>(i)] << "endobj\n";
    }

    const long xrefOffset = static_cast<long>(out.tellp());
    out << "xref\n0 " << (objectCount + 1) << "\n";
    out << "0000000000 65535 f \n";
    for (int i = 1; i <= objectCount; ++i) {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%010ld 00000 n \n",
                      offsets[static_cast<std::size_t>(i)]);
        out << buf;
    }
    out << "trailer\n<< /Size " << (objectCount + 1) << " /Root 1 0 R >>\n";
    out << "startxref\n" << xrefOffset << "\n%%EOF\n";

    if (!out.good()) {
        errorMessage = "Failed while writing sample document";
        return false;
    }
    return true;
}

} // namespace aimp
