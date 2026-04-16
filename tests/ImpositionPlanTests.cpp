#include "aimp/ImpositionPlan.h"
#include "aimp/PdfComposer.h"

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
        if (!aimp::ComposePlanPdf(plan, outputPath, error)) {
            return Fail("ComposePlanPdf should generate a PDF file.");
        }

        std::ifstream file(outputPath, std::ios::binary);
        std::string header(8, '\0');
        file.read(header.data(), static_cast<std::streamsize>(header.size()));
        if (header.rfind("%PDF-1.4", 0) != 0) {
            return Fail("ComposePlanPdf should write a valid PDF header.");
        }
    }

    std::cout << "All planner checks passed.\n";
    return EXIT_SUCCESS;
}
