#pragma once

#include <string>

#include "aimp/ImpositionPlan.h"

namespace aimp {

struct PdfComposeOptions {
    bool includeSheetNumber {true};
    std::string headerText;
    std::string footerText;
};

bool ComposePlanPdf(const ImpositionPlan& plan, const std::string& outputPath, std::string& errorMessage);
bool ComposePlanPdf(const ImpositionPlan& plan,
                    const std::string& outputPath,
                    const PdfComposeOptions& options,
                    std::string& errorMessage);

}
