#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "aimp/ImpositionPlan.h"

namespace aimp {

enum class PdfxProfile {
    None,
    Pdfx1a2001,
    Pdfx4
};

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
    PdfxProfile targetPdfxProfile {PdfxProfile::None};
};

struct PreflightIssue {
    std::string code;
    std::string message;
    bool isError {true};
};

const char* PdfxProfileName(PdfxProfile profile);
bool TryParsePdfxProfile(const std::string& value, PdfxProfile& outProfile);
std::vector<PreflightIssue> ValidatePrepressReadiness(const ImpositionPlan& plan,
                                                      const PdfComposeOptions& options);
std::string ToProductionCompositionJson(const ImpositionPlan& plan,
                                        const BuildOptions& buildOptions,
                                        const PdfComposeOptions& options);

bool ComposePlanPdf(const ImpositionPlan& plan, const std::string& outputPath, std::string& errorMessage);
bool ComposePlanPdf(const ImpositionPlan& plan,
                    const std::string& outputPath,
                    const PdfComposeOptions& options,
                    std::string& errorMessage);

}
