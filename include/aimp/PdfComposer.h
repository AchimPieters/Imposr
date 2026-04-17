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
};

bool ComposePlanPdf(const ImpositionPlan& plan, const std::string& outputPath, std::string& errorMessage);
bool ComposePlanPdf(const ImpositionPlan& plan,
                    const std::string& outputPath,
                    const PdfComposeOptions& options,
                    std::string& errorMessage);

}
