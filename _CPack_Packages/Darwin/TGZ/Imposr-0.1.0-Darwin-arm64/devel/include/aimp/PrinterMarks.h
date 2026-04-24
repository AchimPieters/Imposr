#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "aimp/ImpositionPlan.h"

namespace aimp {

struct PrinterMarksConfig {
    bool registrationMarks {true};
    bool cropMarks {true};
    bool bleedMarks {false};
    bool colorBar {false};
    bool jobInfoStrip {false};

    double markLengthPoints {18.0};
    double markOffsetPoints {6.0};
    double markLineWidthPoints {0.35};
    double bleedPoints {0.0};

    std::string jobName;
    std::string jobDate;
    std::string jobInfo;

    double colorBarHeightPoints {10.0};
    double jobStripHeightPoints {12.0};
};

struct PrinterMarksSheet {
    std::uint32_t sheetIndex {0};
    std::string pdfContentStream;
};

std::vector<PrinterMarksSheet> GeneratePrinterMarks(const ImpositionPlan& plan,
                                                     const PrinterMarksConfig& config);

std::string PrinterMarksPdfContent(const ImpositionPlan& plan,
                                    std::uint32_t sheetIndex,
                                    const PrinterMarksConfig& config);

std::string PrinterMarksConfigToJson(const PrinterMarksConfig& config);

} // namespace aimp
