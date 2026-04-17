#include "aimp/ImpositionPlan.h"
#include "aimp/PdfComposer.h"
#include "aimp/Preset.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace {

int Fail(const char* message) {
    std::cerr << "FAIL: " << message << '\n';
    return EXIT_FAILURE;
}

std::string BuildTempPath(const std::string& fileName) {
    std::error_code ec;
    const auto tempDir = std::filesystem::temp_directory_path(ec);
    if (!ec) {
        return (tempDir / fileName).string();
    }
    return (std::filesystem::current_path() / fileName).string();
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
        options.bookletCreepPerSheetPoints = 6.0;
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
        // Inner sheet (sheetIndex 2) should have creep adjustment versus outer sheet.
        if (plan.placements[4].sheetIndex != 2 || plan.placements[4].targetRect.x != 6.0) {
            return Fail("Booklet creep should shift left slots on inner sheets.");
        }
        if (plan.placements[5].sheetIndex != 2 || plan.placements[5].targetRect.x != 494.0) {
            return Fail("Booklet creep should shift right slots on inner sheets.");
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
        const auto manifest = aimp::ToPlacementManifestJson(aimp::TwoUpPlanner::Build("doc", 1, {1000.0, 700.0}));
        if (manifest.find("\"ctm\"") == std::string::npos ||
            manifest.find("\"a\": 500") == std::string::npos ||
            manifest.find("\"d\": 700") == std::string::npos) {
            return Fail("Placement manifest should include CTM entries for Acrobat composition handoff.");
        }
    }

    {
        aimp::BuildOptions options {};
        options.scaleToFit = true;
        options.autoRotateToFit = true;
        options.sourcePageWidthPoints = 800.0;
        options.sourcePageHeightPoints = 400.0;
        const auto manifest = aimp::ToPlacementManifestJson(aimp::TwoUpPlanner::Build("doc", 1, {600.0, 800.0}, options));
        if (manifest.find("\"rotationDegrees\": 90") == std::string::npos ||
            manifest.find("\"b\": 600") == std::string::npos ||
            manifest.find("\"c\": -300") == std::string::npos) {
            return Fail("Placement manifest should encode rotated CTM for 90-degree placement.");
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
        aimp::BuildOptions options {};
        options.explicitPageSequence = {
            3,
            1,
            3,
            aimp::kBlankPageIndex,
            0
        };
        const auto plan = aimp::TwoUpPlanner::Build("doc", 4, {1000.0, 700.0}, options);
        if (plan.placements.size() != 5 || plan.paddedPageCount != 5) {
            return Fail("Explicit page sequence should drive placement count deterministically.");
        }
        if (plan.placements[0].sourcePage.pageIndex != 3 ||
            plan.placements[1].sourcePage.pageIndex != 1 ||
            plan.placements[2].sourcePage.pageIndex != 3 ||
            plan.placements[3].sourcePage.pageIndex != aimp::kBlankPageIndex ||
            plan.placements[4].sourcePage.pageIndex != 0) {
            return Fail("Explicit page sequence should preserve order, duplicates, and blanks.");
        }
    }

    {
        aimp::BuildOptions options {};
        options.explicitPageSequence = {
            99,
            1
        };
        const auto plan = aimp::TwoUpPlanner::Build("doc", 4, {1000.0, 700.0}, options);
        if (plan.placements.size() != 2) {
            return Fail("Explicit page sequence should emit one placement per sequence item.");
        }
        if (plan.placements[0].sourcePage.pageIndex != aimp::kBlankPageIndex ||
            plan.placements[1].sourcePage.pageIndex != 1) {
            return Fail("Out-of-range explicit sequence entries should be converted to blanks.");
        }
        const auto issues = aimp::ValidatePlan(plan);
        if (!issues.empty()) {
            return Fail("Explicit sequence subsets should be valid and not raise padding issues.");
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
        const std::vector<std::uint32_t> manualOrder {
            7,
            0,
            1,
            6,
            2,
            5,
            3,
            4
        };
        const auto plan = aimp::ManualPlanner::Build("doc", 8, {1000.0, 700.0}, 2, 2, manualOrder);
        if (plan.mode != aimp::LayoutMode::Manual) {
            return Fail("Manual planner should report manual mode.");
        }
        if (plan.placements.size() != 8 || plan.paddedPageCount != 8) {
            return Fail("Manual planner should preserve the requested sequence length.");
        }
        if (plan.placements[0].sourcePage.pageIndex != 7) {
            return Fail("Manual planner should keep explicit first mapping.");
        }
        if (plan.placements[1].sourcePage.pageIndex != 0) {
            return Fail("Manual planner should keep explicit second mapping.");
        }
    }

    {
        const std::vector<std::uint32_t> manualOrder {
            0,
            99
        };
        const auto plan = aimp::ManualPlanner::Build("doc", 4, {1000.0, 700.0}, 2, 1, manualOrder);
        if (plan.placements.size() != 2) {
            return Fail("Manual planner should emit one placement per sequence item.");
        }
        if (plan.placements[0].sourcePage.pageIndex != 0) {
            return Fail("Manual planner should preserve valid first page.");
        }
        if (plan.placements[1].sourcePage.pageIndex != aimp::kBlankPageIndex) {
            return Fail("Manual planner should convert out-of-range pages to blank.");
        }
    }

    {
        const aimp::TileConfig config {
            2,
            2,
            10.0
        };
        const auto plan = aimp::TilePlanner::Build("doc", 2, {1000.0, 700.0}, config);
        if (plan.mode != aimp::LayoutMode::Tile) {
            return Fail("Tile planner should report tile mode.");
        }
        if (plan.placements.size() != 8) {
            return Fail("Tile planner should generate columns*rows placements per source page.");
        }
        const auto& first = plan.placements[0];
        const auto& right = plan.placements[1];
        const auto& bottom = plan.placements[2];
        if (first.targetRect.width != 505.0 || first.targetRect.height != 355.0) {
            return Fail("Tile planner should account for overlap in slot dimensions.");
        }
        if (right.targetRect.x != 495.0 || bottom.targetRect.y != 345.0) {
            return Fail("Tile planner should offset neighboring tiles by overlap stride.");
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
        const auto plan = aimp::BookletPlanner::Build("doc", 7, {1000.0, 700.0});
        const auto stats = aimp::ComputePlanStatistics(plan);
        if (stats.sheetCount != 4 || stats.blankPlacementCount != 1 || stats.nonBlankPlacementCount != 7) {
            return Fail("ComputePlanStatistics should report expected sheet and blank counts.");
        }
        const auto issues = aimp::ValidatePlan(plan);
        if (!issues.empty()) {
            return Fail("ValidatePlan should accept a valid planner output.");
        }
        const auto summary = aimp::BuildHumanSummary(plan);
        if (summary.find("mode=booklet") == std::string::npos || summary.find("sheets=4") == std::string::npos) {
            return Fail("BuildHumanSummary should include mode and sheet count.");
        }
    }

    {
        const auto plan = aimp::TwoUpPlanner::Build("doc", 2, {595.0, 842.0});
        std::string error;
        const std::string outputPath = BuildTempPath("aimp_test_output.pdf");
        aimp::PdfComposeOptions options {};
        options.headerText = "Header Demo";
        options.footerText = "Footer Demo";
        options.includeSheetNumber = false;
        options.includeBates = true;
        options.batesPrefix = "B-";
        options.batesStart = 10;
        options.drawTrimMarks = true;
        options.trimMarkLengthPoints = 9.0;
        options.trimMarkOffsetPoints = 4.0;
        options.drawBleedBox = true;
        options.bleedPoints = 6.0;
        options.overlayTemplate = "sheet={{sheet}} slot={{slot}} page={{page}} name={{name}}";
        const std::string csvPath = BuildTempPath("aimp_test_variables.csv");
        {
            std::ofstream csv(csvPath);
            csv << "page,name\n";
            csv << "1,Alice\n";
            csv << "2,Bob\n";
        }
        options.variableDataCsvPath = csvPath;
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
        if (body.find("trim-marks") == std::string::npos || body.find("bleed-box") == std::string::npos) {
            return Fail("ComposePlanPdf should include trim marks and bleed box drawing when enabled.");
        }
        if (body.find("name=Alice") == std::string::npos || body.find("name=Bob") == std::string::npos) {
            return Fail("ComposePlanPdf should expand overlay template with CSV variables.");
        }
    }

    {
        aimp::PlannerPreset preset {};
        preset.sheetSize = {1000.0, 700.0};
        preset.columns = 2;
        preset.rows = 2;
        preset.tileOverlap = 12.5;
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
        preset.buildOptions.explicitPageSequence = {2, 1, aimp::kBlankPageIndex, 0};
        preset.buildOptions.scaleToFit = true;
        preset.buildOptions.autoRotateToFit = true;
        preset.buildOptions.sourcePageWidthPoints = 612.0;
        preset.buildOptions.sourcePageHeightPoints = 792.0;
        preset.buildOptions.bookletCreepPerSheetPoints = 2.5;
        preset.manualSequence = {4, 0, 3, 2, 1};
        preset.pdfOptions.headerText = "Preset Header";
        preset.pdfOptions.footerText = "Preset Footer";
        preset.pdfOptions.includeSheetNumber = false;
        preset.pdfOptions.includeBates = true;
        preset.pdfOptions.batesPrefix = "PR-";
        preset.pdfOptions.batesStart = 100;
        preset.pdfOptions.drawTrimMarks = true;
        preset.pdfOptions.trimMarkLengthPoints = 8.0;
        preset.pdfOptions.trimMarkOffsetPoints = 3.0;
        preset.pdfOptions.drawBleedBox = true;
        preset.pdfOptions.bleedPoints = 5.0;
        preset.pdfOptions.overlayTemplate = "job={{sheet}}/{{slot}}";
        preset.pdfOptions.variableDataCsvPath = "vars.csv";

        std::string error;
        const std::string presetPath = BuildTempPath("aimp_test_preset.txt");
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
        if (loaded.tileOverlap != 12.5) {
            return Fail("Loaded preset tile overlap mismatch.");
        }
        if (loaded.buildOptions.filter != aimp::PageFilter::OddOnly || !loaded.buildOptions.reverseOrder) {
            return Fail("Loaded preset build options mismatch.");
        }
        if (loaded.buildOptions.explicitPageSequence.size() != 4 ||
            loaded.buildOptions.explicitPageSequence[0] != 2 ||
            loaded.buildOptions.explicitPageSequence[1] != 1 ||
            loaded.buildOptions.explicitPageSequence[2] != aimp::kBlankPageIndex ||
            loaded.buildOptions.explicitPageSequence[3] != 0) {
            return Fail("Loaded preset explicit sequence mismatch.");
        }
        if (loaded.manualSequence.size() != 5 ||
            loaded.manualSequence[0] != 4 ||
            loaded.manualSequence[1] != 0 ||
            loaded.manualSequence[2] != 3 ||
            loaded.manualSequence[3] != 2 ||
            loaded.manualSequence[4] != 1) {
            return Fail("Loaded preset manual sequence mismatch.");
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
        if (loaded.buildOptions.bookletCreepPerSheetPoints != 2.5) {
            return Fail("Loaded preset booklet creep mismatch.");
        }
        if (loaded.pdfOptions.headerText != "Preset Header" || loaded.pdfOptions.includeSheetNumber) {
            return Fail("Loaded preset PDF options mismatch.");
        }
        if (!loaded.pdfOptions.includeBates || loaded.pdfOptions.batesPrefix != "PR-" || loaded.pdfOptions.batesStart != 100) {
            return Fail("Loaded preset Bates options mismatch.");
        }
        if (!loaded.pdfOptions.drawTrimMarks ||
            loaded.pdfOptions.trimMarkLengthPoints != 8.0 ||
            loaded.pdfOptions.trimMarkOffsetPoints != 3.0 ||
            !loaded.pdfOptions.drawBleedBox ||
            loaded.pdfOptions.bleedPoints != 5.0) {
            return Fail("Loaded preset trim/bleed options mismatch.");
        }
        if (loaded.pdfOptions.overlayTemplate != "job={{sheet}}/{{slot}}" ||
            loaded.pdfOptions.variableDataCsvPath != "vars.csv") {
            return Fail("Loaded preset overlay/csv options mismatch.");
        }
    }

    {
        const std::string invalidPresetPath = BuildTempPath("aimp_invalid_preset.txt");
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
