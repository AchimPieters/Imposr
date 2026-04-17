#pragma once

#include <string>

#include "aimp/ImpositionPlan.h"
#include "aimp/PdfComposer.h"

namespace aimp {

struct ArtifactBundleOptions {
    std::string outputDirectory;
    std::string baseName {"imposition-plan"};
    bool includePdf {true};
    PdfComposeOptions pdfOptions;
};

struct ArtifactBundlePaths {
    std::string jsonPath;
    std::string auditXmlPath;
    std::string pdfPath;
};

bool WritePlanArtifactBundle(const ImpositionPlan& plan,
                             const ArtifactBundleOptions& options,
                             ArtifactBundlePaths& outPaths,
                             std::string& errorMessage);

} // namespace aimp

