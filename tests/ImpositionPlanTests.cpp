#include "aimp/ArtifactBundle.h"
#include "aimp/ImpositionPlan.h"
#include "aimp/PdfComposer.h"
#include "aimp/Preset.h"

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>

namespace {

int Fail(const char* message) {
    std::cerr << "FAIL: " << message << '\n';
    return EXIT_FAILURE;
}

} // namespace

int main() {
    {
        const auto plan = aimp::TwoUpPlanner::Build("doc", 5, {1000.0, 700.0});
        if (plan.placements.size() != 5) {
            return Fail("TwoUp should create one placement per source page.");
        }
        if (plan.sourcePageCount != 5 || plan.paddedPageCount != 5) {
            return Fail("TwoUp page count metadata mismatch.");
        }
    }

    {
        const auto plan = aimp::NUpPlanner::Build("doc", 10, {1200.0, 800.0}, 2, 2);
        if (plan.placements.size() != 10) {
            return Fail("NUp should create one placement per source page.");
        }
        if (plan.placements[3].sheetIndex != 0 || plan.placements[4].sheetIndex != 1) {
            return Fail("NUp sheet stepping is incorrect.");
        }
    }

    {
        aimp::BuildOptions options {};
        options.scaleToFit = true;
        options.autoRotateToFit = true;
        options.sourcePageWidthPoints = 800.0;
        options.sourcePageHeightPoints = 400.0;
        const auto plan = aimp::TwoUpPlanner::Build("doc", 1, {600.0, 800.0}, options);
        if (plan.placements.size() != 1) {
            return Fail("TwoUp fit test should create one placement.");
        }
        const auto& p = plan.placements.front();
        if (p.rotationDegrees != 90.0) {
            return Fail("TwoUp fit should rotate to maximize slot usage when enabled.");
        }
        if (p.scale <= 0.0 || p.targetRect.height != 600.0) {
            return Fail("TwoUp fit should compute scaled, centered target rect.");
        }
        if (p.targetRect.y != 100.0) {
            return Fail("TwoUp fit should center content in slot.");
        }
    }

    {
        const auto plan = aimp::BookletPlanner::Build("doc", 6, {1000.0, 700.0});
        if (plan.paddedPageCount != 8) {
            return Fail("Booklet should pad to multiples of 4.");
        }
        if (plan.placements.size() != 8) {
            return Fail("Booklet should keep padded blank slots for deterministic composition.");
        }
        if (plan.placements.front().sourcePage.pageIndex != aimp::kBlankPageIndex) {
            return Fail("Booklet front-left slot should be blank for 6-page signature.");
        }
    }

    {
        aimp::BuildOptions options {};
        options.bookletSignatureSize = 8;
        const auto plan = aimp::BookletPlanner::Build("doc", 10, {1000.0, 700.0}, options);
        if (plan.paddedPageCount != 16) {
            return Fail("Booklet signature sizing should pad each signature to configured size.");
        }
        if (plan.placements.size() != 16) {
            return Fail("Booklet signature sizing should keep deterministic blank placements.");
        }
        if (plan.placements[8].sourcePage.pageIndex != aimp::kBlankPageIndex) {
            return Fail("Second signature front-left should be blank when only 2 pages remain.");
        }
    }

    {
        const auto json = aimp::ToJson(aimp::TwoUpPlanner::Build("doc", 2, {1000.0, 700.0}));
        if (json.find("\"placements\"") == std::string::npos) {
            return Fail("JSON output should include placements.");
        }
        if (json.find("\"sourcePageCount\": 2") == std::string::npos) {
            return Fail("JSON output should include sourcePageCount.");
        }
        if (json.find("\"mode\": \"two-up\"") == std::string::npos) {
            return Fail("JSON mode should use stable text values.");
        }
    }

    {
        aimp::BuildOptions options {};
        options.reverseOrder = true;
        options.filter = aimp::PageFilter::EvenOnly;
        const auto plan = aimp::TwoUpPlanner::Build("doc", 6, {1000.0, 700.0}, options);
        if (plan.placements.size() != 3) {
            return Fail("TwoUp reverse/even filter should leave 3 pages for 6-page input.");
        }
        if (plan.placements.front().sourcePage.pageIndex != 5) {
            return Fail("TwoUp reverse/even filter ordering mismatch.");
        }
    }

    {
        aimp::BuildOptions options {};
        options.padToMultiple = 4;
        const auto plan = aimp::NUpPlanner::Build("doc", 6, {1200.0, 800.0}, 2, 2, options);
        if (plan.paddedPageCount != 8) {
            return Fail("NUp with padToMultiple=4 should pad to 8.");
        }
        if (plan.placements.back().sourcePage.pageIndex != aimp::kBlankPageIndex) {
            return Fail("NUp padded slots should carry blank page marker.");
        }
    }

    {
        const aimp::StepRepeatConfig config {
            2,
            2,
            500.0,
            350.0,
            aimp::Rect {0.0, 0.0, 500.0, 350.0}
        };
        const auto plan = aimp::StepAndRepeatPlanner::Build("doc", 5, {1000.0, 700.0}, config);
        if (plan.mode != aimp::LayoutMode::StepAndRepeat) {
            return Fail("Step-and-repeat should return step-and-repeat mode.");
        }
        if (plan.placements.size() != 5) {
            return Fail("Step-and-repeat should place all source pages when grid has capacity.");
        }
        if (plan.sourcePageCount != 5 || plan.paddedPageCount != 5) {
            return Fail("Step-and-repeat source/padded metadata mismatch.");
        }
    }

    {
        const aimp::StepRepeatConfig config {
            2,
            2,
            500.0,
            350.0,
            aimp::Rect {2000.0, 2000.0, 500.0, 350.0}
        };
        const auto plan = aimp::StepAndRepeatPlanner::Build("doc", 5, {1000.0, 700.0}, config);
        if (!plan.placements.empty()) {
            return Fail("Step-and-repeat should not produce placements when all slots are outside the sheet.");
        }
    }

    {
        const auto json = aimp::ToJson(aimp::TwoUpPlanner::Build("a\"b\\c", 1, {1000.0, 700.0}));
        if (json.find("a\\\"b\\\\c") == std::string::npos) {
            return Fail("JSON output should escape document IDs.");
        }
    }

    {
        const auto xml = aimp::ToAuditXml(aimp::TwoUpPlanner::Build("a&b", 1, {1000.0, 700.0}));
        if (xml.find("<imposition-plan") == std::string::npos) {
            return Fail("Audit XML should include root element.");
        }
        if (xml.find("a&amp;b") == std::string::npos) {
            return Fail("Audit XML should escape source document IDs.");
        }
    }

    {
        const auto plan = aimp::TwoUpPlanner::Build("doc", 4, {1000.0, 700.0});
        const auto matches = aimp::FindPlacementsForSourcePage(plan, "doc", 2);
        if (matches.size() != 1 || matches.front().sheetIndex != 1 || matches.front().slotIndex != 0) {
            return Fail("Inspector source->placement mapping mismatch.");
        }

        aimp::PageRef source {};
        if (!aimp::TryGetSourceForPlacement(plan, 0, 1, source)) {
            return Fail("Inspector placement->source lookup should succeed.");
        }
        if (source.pageIndex != 1 || source.sourceDocumentId != "doc") {
            return Fail("Inspector placement->source lookup mismatch.");
        }
    }

    {
        const auto plan = aimp::TwoUpPlanner::Build("doc", 2, {595.0, 842.0});
        std::string error;
        const std::string outputPath = "/tmp/aimp_test_output.pdf";
        aimp::PdfComposeOptions options {};
        options.headerText = "Header Demo";
        options.footerText = "Footer Demo";
        options.includeSheetNumber = false;
        options.includeBates = true;
        options.batesPrefix = "B-";
        options.batesStart = 10;
        if (!aimp::ComposePlanPdf(plan, outputPath, options, error)) {
            return Fail("ComposePlanPdf should generate a PDF file.");
        }

        std::ifstream file(outputPath, std::ios::binary);
        std::string header(8, '\0');
        file.read(header.data(), static_cast<std::streamsize>(header.size()));
        if (header.rfind("%PDF-1.4", 0) != 0) {
            return Fail("ComposePlanPdf should write a valid PDF header.");
        }

        file.seekg(0, std::ios::end);
        const std::streamsize size = static_cast<std::streamsize>(file.tellg());
        file.seekg(0, std::ios::beg);
        std::string body(static_cast<std::size_t>(size), '\0');
        file.read(body.data(), size);
        if (body.find("Header Demo") == std::string::npos || body.find("Footer Demo") == std::string::npos) {
            return Fail("ComposePlanPdf should include configured header/footer text.");
        }
        if (body.find("B-10") == std::string::npos) {
            return Fail("ComposePlanPdf should include Bates numbering when enabled.");
        }
    }

    {
        const auto plan = aimp::TwoUpPlanner::Build("doc", 4, {595.0, 842.0});
        aimp::ArtifactBundleOptions options {};
        options.outputDirectory = "/tmp/aimp_bundle_output";
        options.baseName = "bundle-test";
        options.includePdf = true;
        options.pdfOptions.headerText = "Bundle Header";
        aimp::ArtifactBundlePaths paths {};
        std::string error;
        if (!aimp::WritePlanArtifactBundle(plan, options, paths, error)) {
            return Fail("WritePlanArtifactBundle should succeed.");
        }
        if (paths.jsonPath.empty() || paths.auditXmlPath.empty() || paths.pdfPath.empty()) {
            return Fail("WritePlanArtifactBundle should return all output paths.");
        }

        std::ifstream jsonFile(paths.jsonPath);
        std::string jsonBody((std::istreambuf_iterator<char>(jsonFile)), std::istreambuf_iterator<char>());
        if (jsonBody.find("\"mode\": \"two-up\"") == std::string::npos) {
            return Fail("Bundle JSON output should contain plan JSON.");
        }
        std::ifstream xmlFile(paths.auditXmlPath);
        std::string xmlBody((std::istreambuf_iterator<char>(xmlFile)), std::istreambuf_iterator<char>());
        if (xmlBody.find("<imposition-plan") == std::string::npos) {
            return Fail("Bundle XML output should contain audit XML.");
        }
    }

    {
        aimp::PlannerPreset preset {};
        preset.sheetSize = {1000.0, 700.0};
        preset.columns = 2;
        preset.rows = 2;
        preset.repeatX = 3;
        preset.repeatY = 1;
        preset.stepX = 200.0;
        preset.stepY = 0.0;
        preset.slotWidth = 200.0;
        preset.slotHeight = 350.0;
        preset.buildOptions.reverseOrder = true;
        preset.buildOptions.filter = aimp::PageFilter::OddOnly;
        preset.buildOptions.padToMultiple = 4;
        preset.buildOptions.bookletSignatureSize = 16;
        preset.buildOptions.scaleToFit = true;
        preset.buildOptions.autoRotateToFit = true;
        preset.buildOptions.sourcePageWidthPoints = 612.0;
        preset.buildOptions.sourcePageHeightPoints = 792.0;
        preset.pdfOptions.headerText = "Preset Header";
        preset.pdfOptions.footerText = "Preset Footer";
        preset.pdfOptions.includeSheetNumber = false;
        preset.pdfOptions.includeBates = true;
        preset.pdfOptions.batesPrefix = "PR-";
        preset.pdfOptions.batesStart = 100;

        std::string error;
        const std::string presetPath = "/tmp/aimp_test_preset.txt";
        if (!aimp::SavePreset(preset, presetPath, error)) {
            return Fail("SavePreset should succeed.");
        }

        aimp::PlannerPreset loaded {};
        if (!aimp::LoadPreset(presetPath, loaded, error)) {
            return Fail("LoadPreset should succeed.");
        }

        if (loaded.sheetSize.widthPoints != 1000.0 || loaded.columns != 2 || loaded.buildOptions.padToMultiple != 4) {
            return Fail("Loaded preset scalar values mismatch.");
        }
        if (loaded.buildOptions.filter != aimp::PageFilter::OddOnly || !loaded.buildOptions.reverseOrder) {
            return Fail("Loaded preset build options mismatch.");
        }
        if (loaded.buildOptions.bookletSignatureSize != 16) {
            return Fail("Loaded preset booklet signature size mismatch.");
        }
        if (!loaded.buildOptions.scaleToFit || !loaded.buildOptions.autoRotateToFit) {
            return Fail("Loaded preset fit options mismatch.");
        }
        if (loaded.buildOptions.sourcePageWidthPoints != 612.0 || loaded.buildOptions.sourcePageHeightPoints != 792.0) {
            return Fail("Loaded preset source page dimensions mismatch.");
        }
        if (loaded.pdfOptions.headerText != "Preset Header" || loaded.pdfOptions.includeSheetNumber) {
            return Fail("Loaded preset PDF options mismatch.");
        }
        if (!loaded.pdfOptions.includeBates || loaded.pdfOptions.batesPrefix != "PR-" || loaded.pdfOptions.batesStart != 100) {
            return Fail("Loaded preset Bates options mismatch.");
        }
    }

    {
        const std::string invalidPresetPath = "/tmp/aimp_invalid_preset.txt";
        {
            std::ofstream out(invalidPresetPath);
            out << "sheetWidth=abc\n"; // invalid
        }
        aimp::PlannerPreset preset {};
        std::string error;
        if (aimp::LoadPreset(invalidPresetPath, preset, error)) {
            return Fail("LoadPreset should fail on invalid preset content.");
        }
        if (error.empty()) {
            return Fail("LoadPreset should report an explicit error for invalid presets.");
        }
    }

    std::cout << "All planner checks passed.\n";
    return EXIT_SUCCESS;
}
