#include "aimp/ImpositionPlan.h"

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace {

int Fail(const char* msg) {
    std::cerr << "FAIL: " << msg << '\n';
    return EXIT_FAILURE;
}

// ─── PlanToSdkOps golden fixtures ────────────────────────────────────────────

int TestTwoUpGolden() {
    // 4 source pages, A3-landscape sheet → 2 output sheets, 4 slots.
    const aimp::SheetSize sheet { 1190.55, 841.89 };
    const auto plan = aimp::TwoUpPlanner::Build("doc", 4u, sheet);

    const auto ops = aimp::PlanToSdkOps(plan);
    if (ops.size() != 4u)
        return Fail("TwoUp golden: expected 4 ops");

    // Sheet 0 has slots 0 and 1; sheet 1 has slots 0 and 1.
    for (const auto& op : ops) {
        if (op.isBlank)
            return Fail("TwoUp golden: no blank pages expected for 4-page doc");
        if (op.sheetIndex > 1u)
            return Fail("TwoUp golden: sheetIndex out of range");
        if (op.slotIndex > 1u)
            return Fail("TwoUp golden: slotIndex out of range");
        if (op.targetRect.width <= 0.0 || op.targetRect.height <= 0.0)
            return Fail("TwoUp golden: targetRect must have positive dimensions");
    }

    // PlanToSdkOps must produce identical results to the JSON round-trip.
    const std::string json = aimp::ToAcrobatSdkOpsJson(plan);
    std::vector<aimp::AcrobatSdkPlacementOp> jsonOps;
    std::string parseErr;
    if (!aimp::ParseAcrobatSdkOpsJson(json, jsonOps, parseErr))
        return Fail(("TwoUp golden: JSON parse failed: " + parseErr).c_str());

    if (jsonOps.size() != ops.size())
        return Fail("TwoUp golden: PlanToSdkOps and JSON round-trip produce different op counts");

    for (std::size_t i = 0; i < ops.size(); ++i) {
        if (ops[i].sheetIndex     != jsonOps[i].sheetIndex)
            return Fail("TwoUp golden: sheetIndex mismatch");
        if (ops[i].slotIndex      != jsonOps[i].slotIndex)
            return Fail("TwoUp golden: slotIndex mismatch");
        if (ops[i].isBlank        != jsonOps[i].isBlank)
            return Fail("TwoUp golden: isBlank mismatch");
        if (ops[i].sourcePageIndex != jsonOps[i].sourcePageIndex)
            return Fail("TwoUp golden: sourcePageIndex mismatch");
        if (ops[i].rotationDegrees != jsonOps[i].rotationDegrees)
            return Fail("TwoUp golden: rotationDegrees mismatch");
    }
    return EXIT_SUCCESS;
}

int TestBookletGolden() {
    // 8-page booklet, A4 sheet.
    const aimp::SheetSize sheet { 595.276 * 2.0, 841.89 };
    aimp::BuildOptions opts;
    opts.padToMultiple = 4;
    const auto plan = aimp::BookletPlanner::Build("doc", 8u, sheet, opts);

    const auto ops = aimp::PlanToSdkOps(plan);
    // 8 pages → 4 spreads → 4 sheets × 2 pages = 8 ops.
    if (ops.size() != 8u)
        return Fail("Booklet golden: expected 8 ops for 8-page booklet");

    // Verify sheet buckets can be built from PlanToSdkOps.
    std::vector<std::vector<aimp::AcrobatSdkPlacementOp>> buckets;
    std::string err;
    if (!aimp::BuildSheetComposeBuckets(plan, ops, buckets, err))
        return Fail(("Booklet golden: BuildSheetComposeBuckets failed: " + err).c_str());
    if (buckets.size() != 4u)
        return Fail("Booklet golden: expected 4 sheet buckets");

    return EXIT_SUCCESS;
}

int TestNUpWithBlanks() {
    // 3-source-page 2×2 n-up: needs 4 slots per sheet, 3 real + 1 blank.
    const aimp::SheetSize sheet { 595.276 * 2.0, 841.89 * 2.0 };
    aimp::BuildOptions opts;
    opts.padToMultiple = 4;
    const auto plan = aimp::NUpPlanner::Build("doc", 3u, sheet, 2u, 2u, opts);

    const auto ops = aimp::PlanToSdkOps(plan);
    if (ops.empty())
        return Fail("NUp blanks: ops must not be empty");

    std::size_t blankCount = 0;
    for (const auto& op : ops) {
        if (op.isBlank) ++blankCount;
    }
    if (blankCount == 0u)
        return Fail("NUp blanks: expected at least one blank slot for 3-page 2×2");

    // Blank ops must not have a valid source page.
    for (const auto& op : ops) {
        if (op.isBlank && op.sourcePageIndex != aimp::kBlankPageIndex)
            return Fail("NUp blanks: blank op should have kBlankPageIndex");
    }

    return EXIT_SUCCESS;
}

int TestStepRepeat() {
    const aimp::SheetSize sheet { 595.276 * 2.0, 841.89 * 2.0 };
    aimp::StepRepeatConfig cfg;
    cfg.repeatX     = 2u;
    cfg.repeatY     = 2u;
    cfg.stepXPoints = 595.276;
    cfg.stepYPoints = 841.89;
    cfg.seedRect    = { 0.0, 0.0, 595.276, 841.89 };

    // 4 source pages fill the 2×2 grid (one page per slot).
    const auto plan = aimp::StepAndRepeatPlanner::Build("doc", 4u, sheet, cfg);
    const auto ops  = aimp::PlanToSdkOps(plan);

    if (ops.size() != 4u)
        return Fail("StepRepeat: 4 pages into 2×2 grid should produce 4 ops");
    for (const auto& op : ops) {
        if (op.isBlank)
            return Fail("StepRepeat: no blank ops expected");
        if (op.sheetIndex != 0u)
            return Fail("StepRepeat: all ops should land on sheet 0");
        if (op.targetRect.width <= 0.0 || op.targetRect.height <= 0.0)
            return Fail("StepRepeat: targetRect must have positive dimensions");
    }
    return EXIT_SUCCESS;
}

// ─── Runner ──────────────────────────────────────────────────────────────────

struct Test { const char* name; int(*fn)(); };
const Test kTests[] = {
    { "PlanToSdkOps TwoUp golden",  TestTwoUpGolden  },
    { "PlanToSdkOps Booklet golden", TestBookletGolden },
    { "PlanToSdkOps NUp with blanks", TestNUpWithBlanks },
    { "PlanToSdkOps StepRepeat",    TestStepRepeat    },
};

} // namespace

int main() {
    int failures = 0;
    for (const auto& t : kTests) {
        const int rc = t.fn();
        std::cout << (rc == EXIT_SUCCESS ? "PASS" : "FAIL") << "  " << t.name << '\n';
        if (rc != EXIT_SUCCESS) ++failures;
    }
    if (failures == 0) {
        std::cout << "All " << (sizeof(kTests)/sizeof(kTests[0])) << " tests passed.\n";
    }
    return failures > 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}
