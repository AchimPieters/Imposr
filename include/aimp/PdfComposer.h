#pragma once

#include <string>

#include "aimp/ImpositionPlan.h"

namespace aimp {

bool ComposePlanPdf(const ImpositionPlan& plan, const std::string& outputPath, std::string& errorMessage);

}
