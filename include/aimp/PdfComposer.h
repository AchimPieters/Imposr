#pragma once

#include <cstdint>
#include <string>

#include "aimp/ImpositionPlan.h"

namespace aimp {

struct PdfComposeOptions {
    bool includeSheetNumber {true};
    std::string headerText;
    std::string footerText;
    bool includeBates {false};
    std::string batesPrefix;
    std::uint32_t batesStart {1};
    bool drawSheetBorder {true};
    bool drawSlotOutlines {true};
    bool drawSlotLabels {true};
    bool drawCenterMarks {true};
    bool drawTrimMarks {false};
    double trimMarkLengthPoints {12.0};
    double trimMarkOffsetPoints {6.0};
    bool drawBleedBox {false};
    double bleedPoints {0.0};
    std::string overlayTemplate;
    std::string variableDataCsvPath;
};

bool ComposePlanPdf(const ImpositionPlan& plan, const std::string& outputPath, std::string& errorMessage);
bool ComposePlanPdf(const ImpositionPlan& plan,
                    const std::string& outputPath,
                    const PdfComposeOptions& options,
                    std::string& errorMessage);

}
