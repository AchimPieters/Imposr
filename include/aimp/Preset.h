#pragma once

#include <string>

#include "aimp/ImpositionPlan.h"
#include "aimp/PdfComposer.h"

namespace aimp {

struct PlannerPreset {
    SheetSize sheetSize;
    std::uint32_t columns {0};
    std::uint32_t rows {0};
    std::uint32_t repeatX {0};
    std::uint32_t repeatY {0};
    double stepX {0.0};
    double stepY {0.0};
    double slotWidth {0.0};
    double slotHeight {0.0};
    BuildOptions buildOptions;
    PdfComposeOptions pdfOptions;
};

bool SavePreset(const PlannerPreset& preset, const std::string& path, std::string& errorMessage);
bool LoadPreset(const std::string& path, PlannerPreset& preset, std::string& errorMessage);

}
