#include "aimp/ImpositionPlan.h"

#include <cstdlib>
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
        if (plan.placements.size() != 6) {
            return Fail("Booklet should skip blank source placements.");
        }
        if (plan.placements.front().sourcePage.pageIndex != 0) {
            return Fail("Booklet first source page in sparse placement should be page 1 (index 0).");
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
    }

    std::cout << "All planner checks passed.\n";
    return EXIT_SUCCESS;
}
