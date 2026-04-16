#include "aimp/ArtifactBundle.h"

#include <filesystem>
#include <fstream>
#include <string>

namespace aimp {

namespace {

bool WriteTextFile(const std::filesystem::path& path,
                   const std::string& content,
                   std::string& errorMessage) {
    std::ofstream out(path);
    if (!out) {
        errorMessage = "Could not open file for writing: " + path.string();
        return false;
    }
    out << content;
    if (!out.good()) {
        errorMessage = "Failed while writing file: " + path.string();
        return false;
    }
    return true;
}

} // namespace

bool WritePlanArtifactBundle(const ImpositionPlan& plan,
                             const ArtifactBundleOptions& options,
                             ArtifactBundlePaths& outPaths,
                             std::string& errorMessage) {
    if (options.outputDirectory.empty()) {
        errorMessage = "Artifact output directory is empty.";
        return false;
    }

    std::error_code fsError;
    const std::filesystem::path outputDir(options.outputDirectory);
    std::filesystem::create_directories(outputDir, fsError);
    if (fsError) {
        errorMessage = "Could not create artifact directory: " + outputDir.string();
        return false;
    }

    const std::string baseName = options.baseName.empty() ? "imposition-plan" : options.baseName;
    const auto jsonPath = outputDir / (baseName + ".json");
    const auto xmlPath = outputDir / (baseName + "-audit.xml");

    if (!WriteTextFile(jsonPath, ToJson(plan), errorMessage)) {
        return false;
    }
    if (!WriteTextFile(xmlPath, ToAuditXml(plan), errorMessage)) {
        return false;
    }

    outPaths.jsonPath = jsonPath.string();
    outPaths.auditXmlPath = xmlPath.string();
    outPaths.pdfPath.clear();

    if (options.includePdf) {
        const auto pdfPath = outputDir / (baseName + "-report.pdf");
        if (!ComposePlanPdf(plan, pdfPath.string(), options.pdfOptions, errorMessage)) {
            return false;
        }
        outPaths.pdfPath = pdfPath.string();
    }

    return true;
}

} // namespace aimp

