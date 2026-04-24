#include "aimp/AdjustPages.h"
#include "aimp/Annotations.h"
#include "aimp/Bleed.h"
#include "aimp/ImpositionPlan.h"
#include "aimp/PageTools.h"
#include "aimp/PdfX.h"
#include "aimp/PrinterMarks.h"
#include "aimp/Shuffle.h"
#include "aimp/SplitMerge.h"
#include "aimp/StickOn.h"
#include "aimp/TilePages.h"
#include "aimp/TrimShift.h"
#include "aimp/VariableData.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace {

int Fail(const char* msg) {
    std::cerr << "FAIL: " << msg << '\n';
    return EXIT_FAILURE;
}

std::string TempPath(const std::string& name) {
    std::error_code ec;
    auto d = std::filesystem::temp_directory_path(ec);
    return ec ? name : (d / name).string();
}

// ─────────────────────────────────────────────────────────────
// TrimShift
// ─────────────────────────────────────────────────────────────
int TestTrimShift() {
    // ComputeCreepOffset: sheet 0 → 0, sheet 1 → 2pt, sheet 2 → 4pt.
    if (aimp::ComputeCreepOffset(0, 4, 2.0) != 0.0)
        return Fail("TrimShift: outermost sheet must have zero creep");
    if (aimp::ComputeCreepOffset(1, 4, 2.0) != 2.0)
        return Fail("TrimShift: second sheet should have 2pt creep");
    if (aimp::ComputeCreepOffset(4, 4, 2.0) != 6.0)   // clamped to sigSize-1=3
        return Fail("TrimShift: out-of-range sheet should be clamped");

    // ApplyCreepShiftToPlan on an 8-page booklet (2 physical sheets) with 3pt/sheet creep.
    // A 4-page booklet has only 1 physical sheet (the outermost), which always gets 0 creep.
    // We need ≥ 2 physical sheets so the inner sheet receives a nonzero offset.
    aimp::BuildOptions opts;
    opts.bookletCreepPerSheetPoints = 0.0; // we apply TrimShift separately
    auto plan = aimp::BookletPlanner::Build("doc", 8, {1000.0, 700.0}, opts);
    if (plan.placements.size() != 8)
        return Fail("TrimShift: 8-page booklet plan should have 8 placements");

    aimp::TrimShiftConfig cfg;
    cfg.creepPerSheetPoints = 3.0;
    const auto result = aimp::ApplyCreepShiftToPlan(plan, cfg);
    if (result.sheetsAdjusted == 0)
        return Fail("TrimShift: should adjust at least one sheet");

    // JSON serialisation.
    const auto j = aimp::TrimShiftConfigToJson(cfg, result);
    if (j.find("\"kind\": \"trim-shift\"") == std::string::npos)
        return Fail("TrimShift: JSON must contain kind key");

    // Zero-creep plan → nothing adjusted.
    aimp::TrimShiftConfig zeroCfg;
    zeroCfg.creepPerSheetPoints = 0.0;
    auto plan2 = aimp::BookletPlanner::Build("doc", 4, {1000.0, 700.0}, opts);
    const auto r2 = aimp::ApplyCreepShiftToPlan(plan2, zeroCfg);
    if (r2.sheetsAdjusted != 0)
        return Fail("TrimShift: zero creep should adjust nothing");

    return EXIT_SUCCESS;
}

// ─────────────────────────────────────────────────────────────
// VariableData
// ─────────────────────────────────────────────────────────────
int TestVariableData() {
    const std::string csvPath = TempPath("aimp_nm_vardata.csv");
    {
        std::ofstream f(csvPath);
        f << "page,name,photo\n";
        f << "1,Alice,alice.jpg\n";
        f << "2,Bob,\n";
        f << "3,Carol,carol.png\n";
    }

    aimp::VariableDataSet ds;
    std::string err;
    if (!aimp::LoadVariableDataSet(csvPath, ds, err))
        return Fail("VariableData: LoadVariableDataSet should succeed");
    if (ds.records.size() != 3)
        return Fail("VariableData: should have 3 records");
    if (ds.records.at(0).at("name").value != "Alice")
        return Fail("VariableData: page 1 name should be Alice");
    if (ds.records.at(0).at("photo").type != aimp::VarFieldType::Image)
        return Fail("VariableData: photo column should be Image type");
    if (ds.records.at(1).at("photo").type != aimp::VarFieldType::Image)
        return Fail("VariableData: empty photo cell still Image type");

    // ExpandTemplate.
    const auto& rec = ds.records.at(2);
    const std::string expanded = aimp::ExpandTemplate("Hello {{name}}!", rec);
    if (expanded != "Hello Carol!")
        return Fail("VariableData: ExpandTemplate result mismatch");

    // MergeVariableData.
    const auto merged = aimp::MergeVariableData(ds, "{{name}}", 3);
    if (merged.size() != 3)
        return Fail("VariableData: MergeVariableData should return 3 entries");
    if (merged[0].resolvedText != "Alice")
        return Fail("VariableData: merged page 0 text should be Alice");
    if (merged[0].imageFields.empty() || merged[0].imageFields[0] != "alice.jpg")
        return Fail("VariableData: merged page 0 should have image alice.jpg");

    // JSON roundtrip check.
    const auto j = aimp::VariableDataSetToJson(ds);
    if (j.find("\"kind\": \"variable-data-set\"") == std::string::npos)
        return Fail("VariableData: JSON must contain kind key");
    if (j.find("Alice") == std::string::npos)
        return Fail("VariableData: JSON must contain field values");

    // Missing page column → error.
    const std::string badCsv = TempPath("aimp_nm_vardata_bad.csv");
    {
        std::ofstream f(badCsv);
        f << "nopage,name\n1,Alice\n";
    }
    aimp::VariableDataSet bad;
    if (aimp::LoadVariableDataSet(badCsv, bad, err))
        return Fail("VariableData: CSV without 'page' column should fail");

    return EXIT_SUCCESS;
}

// ─────────────────────────────────────────────────────────────
// Shuffle
// ─────────────────────────────────────────────────────────────
int TestShuffle() {
    const std::vector<std::uint32_t> pages {0,1,2,3,4,5,6,7};

    // Signature shuffle: pads and reorders.
    aimp::ShuffleConfig sigCfg;
    sigCfg.mode = aimp::ShuffleMode::Signature;
    sigCfg.signatureSize = 8;
    const auto sigResult = aimp::ShufflePages(pages, sigCfg);
    if (sigResult.orderedPages.size() != 8)
        return Fail("Shuffle: signature mode should produce 8 ordered pages");
    if (sigResult.signatureCount != 1)
        return Fail("Shuffle: 8 pages with sig=8 → 1 signature");
    if (sigResult.blankPagesAdded != 0)
        return Fail("Shuffle: exact fit should add no blanks");

    // Padding: 6 pages with sig=8 → 2 blanks added.
    const std::vector<std::uint32_t> six {0,1,2,3,4,5};
    const auto padResult = aimp::ShufflePages(six, sigCfg);
    if (padResult.blankPagesAdded != 2)
        return Fail("Shuffle: 6 pages with sig=8 should add 2 blanks");

    // PerfectBound: preserves order.
    aimp::ShuffleConfig pbCfg;
    pbCfg.mode = aimp::ShuffleMode::PerfectBound;
    const auto pbResult = aimp::ShufflePages(pages, pbCfg);
    if (pbResult.orderedPages != pages)
        return Fail("Shuffle: PerfectBound should preserve order");

    // SplitEvenOdd.
    auto [evens, odds] = aimp::SplitEvenOdd(pages);
    if (evens.size() != 4 || odds.size() != 4)
        return Fail("Shuffle: SplitEvenOdd should split 8 pages 4/4");
    if (evens[0] != 0 || evens[1] != 2)
        return Fail("Shuffle: evens should be pages at even indices");
    if (odds[0] != 1 || odds[1] != 3)
        return Fail("Shuffle: odds should be pages at odd indices");

    // MergeInterlace.
    const auto merged = aimp::MergeInterlace(evens, odds);
    if (merged.size() != 8)
        return Fail("Shuffle: MergeInterlace should produce 8 pages");
    if (merged[0] != 0 || merged[1] != 1 || merged[2] != 2)
        return Fail("Shuffle: MergeInterlace order is wrong");

    // EvenOdd mode.
    aimp::ShuffleConfig eoCfg;
    eoCfg.mode = aimp::ShuffleMode::EvenOdd;
    const auto eoResult = aimp::ShufflePages(pages, eoCfg);
    if (eoResult.orderedPages.size() != 8)
        return Fail("Shuffle: EvenOdd mode should produce 8 pages");

    // JSON.
    const auto j = aimp::ShuffleResultToJson(sigResult);
    if (j.find("\"kind\": \"shuffle-result\"") == std::string::npos)
        return Fail("Shuffle: JSON must contain kind key");

    // Empty input.
    const auto empty = aimp::ShufflePages({}, sigCfg);
    if (!empty.orderedPages.empty())
        return Fail("Shuffle: empty input should produce empty output");

    return EXIT_SUCCESS;
}

// ─────────────────────────────────────────────────────────────
// SplitMerge
// ─────────────────────────────────────────────────────────────
int TestSplitMerge() {
    // ParseRangeSpec.
    std::vector<aimp::PageRange> ranges;
    std::string err;
    if (!aimp::ParseRangeSpec("1-4,5-8", 8, ranges, err))
        return Fail("SplitMerge: ParseRangeSpec should accept valid spec");
    if (ranges.size() != 2)
        return Fail("SplitMerge: should parse 2 ranges");
    if (ranges[0].startPage != 0 || ranges[0].endPage != 3)
        return Fail("SplitMerge: range 1-4 should be pages 0-3");
    if (ranges[1].startPage != 4 || ranges[1].endPage != 7)
        return Fail("SplitMerge: range 5-8 should be pages 4-7");

    // Single page range.
    std::vector<aimp::PageRange> single;
    if (!aimp::ParseRangeSpec("3", 8, single, err))
        return Fail("SplitMerge: single page spec should parse");
    if (single[0].startPage != 2 || single[0].endPage != 2)
        return Fail("SplitMerge: single page 3 → index 2");

    // Invalid range.
    std::vector<aimp::PageRange> bad;
    if (aimp::ParseRangeSpec("0-5", 8, bad, err))
        return Fail("SplitMerge: range starting at 0 (human) should fail");
    if (aimp::ParseRangeSpec("5-3", 8, bad, err))
        return Fail("SplitMerge: inverted range should fail");

    // SplitPlan.
    const auto plan = aimp::TwoUpPlanner::Build("doc", 8, {1000.0, 700.0});
    const auto splitResult = aimp::SplitPlan(plan, ranges);
    if (!splitResult.success)
        return Fail("SplitMerge: SplitPlan should succeed");
    if (splitResult.segments.size() != 2)
        return Fail("SplitMerge: should produce 2 segments");

    // MergePlans.
    const auto merged = aimp::MergePlans(splitResult.segments);
    if (merged.placements.empty())
        return Fail("SplitMerge: merged plan should have placements");

    // Empty input.
    const auto empty = aimp::MergePlans({});
    if (!empty.placements.empty())
        return Fail("SplitMerge: merging empty list should return empty plan");

    // JSON.
    const auto j = aimp::SplitMergeResultToJson(splitResult);
    if (j.find("\"success\": true") == std::string::npos)
        return Fail("SplitMerge: JSON must report success=true");

    return EXIT_SUCCESS;
}

// ─────────────────────────────────────────────────────────────
// PageTools
// ─────────────────────────────────────────────────────────────
int TestPageTools() {
    const std::vector<std::uint32_t> pages {0,1,2,3,4};

    // DuplicatePages.
    const auto dup = aimp::DuplicatePages(pages, 2, 2);
    if (dup.size() != 7)
        return Fail("PageTools: DuplicatePages should add 2 copies");
    if (dup[2] != 2 || dup[3] != 2 || dup[4] != 2)
        return Fail("PageTools: DuplicatePages copies should be at correct positions");

    // DuplicatePages out-of-range → unchanged.
    const auto dupOob = aimp::DuplicatePages(pages, 99, 1);
    if (dupOob != pages)
        return Fail("PageTools: DuplicatePages out-of-range should return original");

    // DeletePages.
    const auto del = aimp::DeletePages(pages, {1, 3});
    if (del.size() != 3)
        return Fail("PageTools: DeletePages should remove 2 pages");
    if (del[0] != 0 || del[1] != 2 || del[2] != 4)
        return Fail("PageTools: DeletePages wrong remaining pages");

    // MovePages.
    const auto moved = aimp::MovePages(pages, 0, 4);
    if (moved.size() != 5)
        return Fail("PageTools: MovePages should keep same count");
    if (moved[3] != 0)
        return Fail("PageTools: moved page should be at new position");
    if (moved[0] != 1)
        return Fail("PageTools: first page after move should shift left");

    // MovePages same position → unchanged.
    const auto movedSame = aimp::MovePages(pages, 2, 2);
    if (movedSame != pages)
        return Fail("PageTools: moving to same position should be a no-op");

    // InsertBlankPages.
    const auto inserted = aimp::InsertBlankPages(pages, 2, 3);
    if (inserted.size() != 8)
        return Fail("PageTools: InsertBlankPages should add 3 blanks");
    if (inserted[2] != aimp::kBlankPageIndex ||
        inserted[3] != aimp::kBlankPageIndex ||
        inserted[4] != aimp::kBlankPageIndex)
        return Fail("PageTools: inserted pages should be blank markers");
    if (inserted[5] != 2)
        return Fail("PageTools: original page after insertion should shift right");

    // InsertBlankPages beyond end → append.
    const auto appended = aimp::InsertBlankPages(pages, 999, 1);
    if (appended.size() != 6)
        return Fail("PageTools: InsertBlankPages beyond end should append");
    if (appended.back() != aimp::kBlankPageIndex)
        return Fail("PageTools: appended page should be blank marker");

    // RotatePlacementsForPage.
    auto plan = aimp::TwoUpPlanner::Build("doc", 4, {1000.0, 700.0});
    aimp::RotatePlacementsForPage(plan, 0, 90.0);
    bool found = false;
    for (const auto& p : plan.placements) {
        if (p.sourcePage.pageIndex == 0 && p.rotationDegrees == 90.0) {
            found = true;
        }
    }
    if (!found)
        return Fail("PageTools: RotatePlacementsForPage should set rotation");

    // Additional rotation wraps.
    aimp::RotatePlacementsForPage(plan, 0, 270.0);
    bool wrapped = false;
    for (const auto& p : plan.placements) {
        if (p.sourcePage.pageIndex == 0 && p.rotationDegrees == 0.0) {
            wrapped = true;
        }
    }
    if (!wrapped)
        return Fail("PageTools: RotatePlacementsForPage 90+270 should wrap to 0");

    return EXIT_SUCCESS;
}

// ─────────────────────────────────────────────────────────────
// StickOn
// ─────────────────────────────────────────────────────────────
int TestStickOn() {
    const auto plan = aimp::TwoUpPlanner::Build("doc", 4, {1000.0, 700.0});

    // Stick text on all pages.
    aimp::StickOnItem textItem;
    textItem.type = aimp::StickOnType::Text;
    textItem.text = "Hello";
    textItem.anchor = aimp::StickOnAnchor::BottomLeft;
    textItem.rect = aimp::Rect{0,0,100,20};
    textItem.applyToAllPages = true;

    const auto result = aimp::ResolveStickOnItems(plan, {textItem});
    if (result.opsGenerated != 4)
        return Fail("StickOn: text on all pages should produce 4 ops");
    if (result.ops[0].resolvedText != "Hello")
        return Fail("StickOn: text item should resolve to 'Hello'");

    // Bates stamp: counter increments per op.
    aimp::StickOnItem bates;
    bates.type = aimp::StickOnType::BatesNumber;
    bates.batesPrefix = "DOC-";
    bates.batesSuffix = "";
    bates.batesStart = 1;
    bates.batesPadWidth = 4;
    bates.applyToAllPages = true;
    bates.rect = aimp::Rect{0,0,80,15};
    const auto br = aimp::ResolveStickOnItems(plan, {bates});
    if (br.ops.size() != 4)
        return Fail("StickOn: Bates should produce 4 ops");
    if (br.ops[0].resolvedText != "DOC-0001")
        return Fail("StickOn: first Bates number should be DOC-0001");
    if (br.ops[3].resolvedText != "DOC-0004")
        return Fail("StickOn: last Bates number should be DOC-0004");

    // Page number item.
    aimp::StickOnItem pageNum;
    pageNum.type = aimp::StickOnType::PageNumber;
    pageNum.applyToAllPages = true;
    pageNum.rect = aimp::Rect{0,0,40,12};
    const auto pnr = aimp::ResolveStickOnItems(plan, {pageNum});
    if (pnr.ops[0].resolvedText != "1")
        return Fail("StickOn: page number should start at 1");

    // MaskingTape: PDF content should mention fill.
    aimp::StickOnItem tape;
    tape.type = aimp::StickOnType::MaskingTape;
    tape.applyToAllPages = true;
    tape.rect = aimp::Rect{10,10,200,30};
    const auto tr = aimp::ResolveStickOnItems(plan, {tape});
    const auto tapeContent = aimp::RenderStickOnPdfContent(tr.ops, 0);
    if (tapeContent.find("re f") == std::string::npos)
        return Fail("StickOn: masking tape should render a filled rect");

    // PeelOff.
    aimp::StickOnItem peelOff;
    peelOff.type = aimp::StickOnType::PeelOff;
    peelOff.applyToAllPages = true;
    peelOff.rect = aimp::Rect{0,0,80,30};
    const auto por = aimp::ResolveStickOnItems(plan, {peelOff});
    const auto peelContent = aimp::RenderStickOnPdfContent(por.ops, 0);
    if (peelContent.find("peel-off") == std::string::npos)
        return Fail("StickOn: peel-off should render label text");

    // JSON serialisation.
    const auto j = aimp::StickOnResultToJson(result);
    if (j.find("\"kind\": \"stick-on-result\"") == std::string::npos)
        return Fail("StickOn: JSON must contain kind key");
    if (j.find("\"type\": \"text\"") == std::string::npos)
        return Fail("StickOn: JSON must include type field");

    // Targeted pages only.
    aimp::StickOnItem targeted;
    targeted.type = aimp::StickOnType::Text;
    targeted.text = "Selected";
    targeted.applyToAllPages = false;
    targeted.applyToPageIndices = {0, 2};
    targeted.rect = aimp::Rect{0,0,60,15};
    const auto tgr = aimp::ResolveStickOnItems(plan, {targeted});
    if (tgr.opsGenerated != 2)
        return Fail("StickOn: targeted pages should produce 2 ops");

    return EXIT_SUCCESS;
}

// ─────────────────────────────────────────────────────────────
// PrinterMarks
// ─────────────────────────────────────────────────────────────
int TestPrinterMarks() {
    const auto plan = aimp::TwoUpPlanner::Build("doc", 4, {1000.0, 700.0});
    aimp::PrinterMarksConfig cfg;
    cfg.registrationMarks = true;
    cfg.cropMarks = true;
    cfg.bleedMarks = true;
    cfg.bleedPoints = 6.0;
    cfg.colorBar = true;
    cfg.jobInfoStrip = true;
    cfg.jobName = "TestJob";
    cfg.jobDate = "2026-04-24";

    const auto sheets = aimp::GeneratePrinterMarks(plan, cfg);
    if (sheets.size() != 2)
        return Fail("PrinterMarks: 4-page two-up plan has 2 sheets");

    const auto& content = sheets[0].pdfContentStream;
    if (content.find("re S") == std::string::npos && content.find("l S") == std::string::npos)
        return Fail("PrinterMarks: content should contain line drawing ops");
    if (content.find("TestJob") == std::string::npos)
        return Fail("PrinterMarks: content should include job name");

    // Direct content generation matches GeneratePrinterMarks.
    const auto direct = aimp::PrinterMarksPdfContent(plan, 0, cfg);
    if (direct != content)
        return Fail("PrinterMarks: direct content gen should match GeneratePrinterMarks");

    // Bleed marks annotation.
    if (content.find("bleed") == std::string::npos)
        return Fail("PrinterMarks: bleed marks should appear in content");

    // JSON config.
    const auto j = aimp::PrinterMarksConfigToJson(cfg);
    if (j.find("\"kind\": \"printer-marks-config\"") == std::string::npos)
        return Fail("PrinterMarks: JSON must contain kind key");
    if (j.find("true") == std::string::npos)
        return Fail("PrinterMarks: JSON must contain boolean true values");

    // Disabled marks → short content.
    aimp::PrinterMarksConfig none;
    none.registrationMarks = false;
    none.cropMarks = false;
    none.bleedMarks = false;
    none.colorBar = false;
    none.jobInfoStrip = false;
    const auto empty = aimp::PrinterMarksPdfContent(plan, 0, none);
    if (empty.find("re S") != std::string::npos || empty.find("l S") != std::string::npos)
        return Fail("PrinterMarks: disabled marks should produce no stroke ops");

    return EXIT_SUCCESS;
}

// ─────────────────────────────────────────────────────────────
// AdjustPages
// ─────────────────────────────────────────────────────────────
int TestAdjustPages() {
    // Scale mode.
    {
        auto plan = aimp::TwoUpPlanner::Build("doc", 2, {1000.0, 700.0});
        const double origW = plan.placements[0].targetRect.width;
        aimp::AdjustSpec spec;
        spec.mode = aimp::AdjustMode::Scale;
        spec.scaleX = 0.8;
        spec.scaleY = 0.8;
        const auto r = aimp::ApplyAdjustSpec(plan, spec);
        if (r.placementsModified != 2)
            return Fail("AdjustPages: scale should modify all placements");
        if (std::abs(plan.placements[0].targetRect.width - origW * 0.8) > 0.01)
            return Fail("AdjustPages: scale should resize width");
    }

    // Crop mode.
    {
        auto plan = aimp::TwoUpPlanner::Build("doc", 2, {1000.0, 700.0});
        aimp::AdjustSpec spec;
        spec.mode = aimp::AdjustMode::Crop;
        spec.cropRect = {10.0, 10.0, 400.0, 300.0};
        const auto r = aimp::ApplyAdjustSpec(plan, spec);
        if (r.placementsModified != 2)
            return Fail("AdjustPages: crop should modify all placements");
        if (plan.placements[0].targetRect.width != 400.0)
            return Fail("AdjustPages: crop should set target width to cropRect width");
    }

    // Extend mode.
    {
        auto plan = aimp::TwoUpPlanner::Build("doc", 2, {1000.0, 700.0});
        const double origH = plan.placements[0].targetRect.height;
        aimp::AdjustSpec spec;
        spec.mode = aimp::AdjustMode::Extend;
        spec.extendTop = 10.0;
        spec.extendBottom = 5.0;
        const auto r = aimp::ApplyAdjustSpec(plan, spec);
        if (std::abs(plan.placements[0].targetRect.height - (origH + 15.0)) > 0.01)
            return Fail("AdjustPages: extend should increase height by top+bottom");
    }

    // ScaleToFit mode.
    {
        auto plan = aimp::TwoUpPlanner::Build("doc", 1, {1000.0, 700.0});
        aimp::AdjustSpec spec;
        spec.mode = aimp::AdjustMode::ScaleToFit;
        spec.targetWidth = 200.0;
        spec.targetHeight = 200.0;
        const auto r = aimp::ApplyAdjustSpec(plan, spec);
        if (r.placementsModified != 1)
            return Fail("AdjustPages: ScaleToFit should modify 1 placement");
    }

    // Targeted pages only.
    {
        auto plan = aimp::TwoUpPlanner::Build("doc", 4, {1000.0, 700.0});
        aimp::AdjustSpec spec;
        spec.mode = aimp::AdjustMode::Scale;
        spec.scaleX = 0.5;
        spec.scaleY = 0.5;
        spec.applyToAllPlacements = false;
        spec.targetSourcePageIndices = {0};
        const auto r = aimp::ApplyAdjustSpec(plan, spec);
        if (r.placementsModified != 1)
            return Fail("AdjustPages: targeted mode should modify only 1 placement");
    }

    // Rotation applied.
    {
        auto plan = aimp::TwoUpPlanner::Build("doc", 1, {1000.0, 700.0});
        aimp::AdjustSpec spec;
        spec.mode = aimp::AdjustMode::Scale;
        spec.scaleX = 1.0;
        spec.scaleY = 1.0;
        spec.rotationDegrees = 90.0;
        aimp::ApplyAdjustSpec(plan, spec);
        if (plan.placements[0].rotationDegrees != 90.0)
            return Fail("AdjustPages: rotation should be applied to placement");
    }

    // JSON output.
    {
        auto plan = aimp::TwoUpPlanner::Build("doc", 2, {1000.0, 700.0});
        aimp::AdjustSpec spec;
        spec.mode = aimp::AdjustMode::Scale;
        spec.scaleX = 1.0;
        spec.scaleY = 1.0;
        const auto r = aimp::ApplyAdjustSpec(plan, spec);
        const auto j = aimp::AdjustResultToJson(r);
        if (j.find("\"kind\": \"adjust-result\"") == std::string::npos)
            return Fail("AdjustPages: JSON must contain kind key");
    }

    return EXIT_SUCCESS;
}

// ─────────────────────────────────────────────────────────────
// TilePages
// ─────────────────────────────────────────────────────────────
int TestTilePages() {
    aimp::TilePagesConfig cfg;
    cfg.columns = 3;
    cfg.rows = 2;
    cfg.overlapPoints = 10.0;
    cfg.bleedPoints = 5.0;

    // ComputeTileGrid.
    const auto grid = aimp::ComputeTileGrid(900.0, 600.0, cfg);
    if (!grid.success)
        return Fail("TilePages: ComputeTileGrid should succeed");
    if (grid.slices.size() != 6)
        return Fail("TilePages: 3×2 grid should have 6 slices");
    if (grid.columns != 3 || grid.rows != 2)
        return Fail("TilePages: grid metadata mismatch");

    // Slice geometry: overlap factored into tile size.
    // Expected tileW = (900 + 10*(3-1)) / 3 = (920)/3 ≈ 306.67
    const double expectedTileW = (900.0 + 10.0 * 2.0) / 3.0;
    if (std::abs(grid.tileWidth - expectedTileW) > 0.1)
        return Fail("TilePages: tileWidth should account for overlap");

    // Target rect includes bleed padding.
    if (grid.slices[0].targetRect.x != cfg.bleedPoints)
        return Fail("TilePages: target rect x should start at bleed offset");
    if (grid.slices[0].targetRect.y != cfg.bleedPoints)
        return Fail("TilePages: target rect y should start at bleed offset");

    // BuildTilePlan.
    const auto plan = aimp::BuildTilePlan("doc", 2, 900.0, 600.0, cfg);
    if (plan.mode != aimp::LayoutMode::Tile)
        return Fail("TilePages: BuildTilePlan should return Tile mode");
    // 2 source pages × 6 tiles each = 12 sheets.
    if (plan.placements.size() != 12)
        return Fail("TilePages: 2 pages × 6 tiles = 12 placements");

    // Invalid config: zero columns.
    aimp::TilePagesConfig bad;
    bad.columns = 0;
    bad.rows = 2;
    const auto badGrid = aimp::ComputeTileGrid(900.0, 600.0, bad);
    if (badGrid.success)
        return Fail("TilePages: zero columns should fail");

    // JSON.
    const auto j = aimp::TilePagesResultToJson(grid);
    if (j.find("\"kind\": \"tile-pages-result\"") == std::string::npos)
        return Fail("TilePages: JSON must contain kind key");
    if (j.find("\"success\": true") == std::string::npos)
        return Fail("TilePages: JSON must report success=true");

    return EXIT_SUCCESS;
}

// ─────────────────────────────────────────────────────────────
// Bleed
// ─────────────────────────────────────────────────────────────
int TestBleed() {
    aimp::BleedSpec spec;
    spec.top = 6.0;
    spec.bottom = 6.0;
    spec.left = 6.0;
    spec.right = 6.0;
    spec.method = aimp::BleedMethod::Mirror;

    const aimp::Rect trimBox {100.0, 100.0, 400.0, 300.0};
    const auto bleedBox = aimp::ComputeBleedBox(trimBox, spec);
    if (bleedBox.x != 94.0)
        return Fail("Bleed: bleedBox x should be trimBox.x - left");
    if (bleedBox.y != 94.0)
        return Fail("Bleed: bleedBox y should be trimBox.y - bottom");
    if (bleedBox.width != 412.0)
        return Fail("Bleed: bleedBox width should include left + right bleed");
    if (bleedBox.height != 312.0)
        return Fail("Bleed: bleedBox height should include top + bottom bleed");

    // GenerateBleedZones.
    const auto plan = aimp::TwoUpPlanner::Build("doc", 4, {1000.0, 700.0});
    const auto result = aimp::GenerateBleedZones(plan, spec);
    if (result.zonesGenerated != 4)
        return Fail("Bleed: 4-placement plan should generate 4 zones");

    // Zero bleed → no zones.
    aimp::BleedSpec zero;
    const auto zeroResult = aimp::GenerateBleedZones(plan, zero);
    if (zeroResult.zonesGenerated != 0)
        return Fail("Bleed: zero bleed spec should produce no zones");

    // Solid color content.
    aimp::BleedSpec solid;
    solid.top = solid.bottom = solid.left = solid.right = 4.0;
    solid.method = aimp::BleedMethod::SolidColor;
    solid.solidColorR = 1.0;
    solid.solidColorG = 0.0;
    solid.solidColorB = 0.0;
    const auto solidResult = aimp::GenerateBleedZones(plan, solid);
    const auto solidContent = aimp::RenderBleedPdfContent(solidResult, 0, solid);
    if (solidContent.find("re f") == std::string::npos)
        return Fail("Bleed: solid color should render filled rects");

    // Mirror method → comment annotation.
    const auto mirrorResult = aimp::GenerateBleedZones(plan, spec);
    const auto mirrorContent = aimp::RenderBleedPdfContent(mirrorResult, 0, spec);
    if (mirrorContent.find("bleed-mirror") == std::string::npos)
        return Fail("Bleed: mirror method should annotate content stream");

    // JSON.
    const auto j = aimp::BleedZonesToJson(result);
    if (j.find("\"kind\": \"bleed-zones\"") == std::string::npos)
        return Fail("Bleed: JSON must contain kind key");
    if (j.find("\"method\": \"mirror\"") == std::string::npos)
        return Fail("Bleed: JSON must include method name");

    return EXIT_SUCCESS;
}

// ─────────────────────────────────────────────────────────────
// PdfX
// ─────────────────────────────────────────────────────────────
int TestPdfX() {
    // PdfXSubtypeName round-trips through ParsePdfXSubtype.
    const struct { aimp::PdfXSubtype sub; const char* str; } cases[] = {
        {aimp::PdfXSubtype::PdfX1a_2001, "PDF/X-1a:2001"},
        {aimp::PdfXSubtype::PdfX3_2002,  "PDF/X-3:2002"},
        {aimp::PdfXSubtype::PdfX4_2010,  "PDF/X-4:2010"},
        {aimp::PdfXSubtype::PdfX5g_2010, "PDF/X-5g:2010"},
        {aimp::PdfXSubtype::None,         "none"},
    };
    for (const auto& c : cases) {
        if (aimp::ParsePdfXSubtype(c.str) != c.sub)
            return Fail("PdfX: ParsePdfXSubtype round-trip failed");
    }

    // pdfx-4 shorthand.
    if (aimp::ParsePdfXSubtype("pdfx-4") != aimp::PdfXSubtype::PdfX4_2010)
        return Fail("PdfX: pdfx-4 shorthand should map to PdfX4_2010");

    // DetectPdfXMetadata: header containing "PDF/X-4".
    const std::string header = "...some xmp metadata PDF/X-4 OutputIntent ...";
    const auto meta = aimp::DetectPdfXMetadata(header);
    if (meta.subtype != aimp::PdfXSubtype::PdfX4_2010)
        return Fail("PdfX: should detect PDF/X-4 from header sample");
    if (!meta.hasOutputIntent)
        return Fail("PdfX: should detect OutputIntent in header");

    // ValidatePdfXCompliance: blank pages are a warning.
    const auto plan = aimp::BookletPlanner::Build("doc", 6, {1000.0, 700.0});
    aimp::PdfXPreservationConfig cfg;
    cfg.requiredSubtype = aimp::PdfXSubtype::None;
    aimp::PdfXMetadata pdfxMeta;
    pdfxMeta.subtype = aimp::PdfXSubtype::PdfX4_2010;
    pdfxMeta.hasOutputIntent = true;
    const auto issues = aimp::ValidatePdfXCompliance(plan, pdfxMeta, cfg);
    bool foundBlank = false;
    for (const auto& i : issues) {
        if (i.code == "pdfx.blank-page-in-pdfx") foundBlank = true;
    }
    if (!foundBlank)
        return Fail("PdfX: blank placements in plan should raise pdfx.blank-page-in-pdfx");

    // Subtype mismatch triggers error.
    aimp::PdfXPreservationConfig strictCfg;
    strictCfg.requiredSubtype = aimp::PdfXSubtype::PdfX1a_2001;
    const auto mismatch = aimp::ValidatePdfXCompliance(plan, pdfxMeta, strictCfg);
    bool foundMismatch = false;
    for (const auto& i : mismatch) {
        if (i.code == "pdfx.subtype-mismatch" && i.isError) foundMismatch = true;
    }
    if (!foundMismatch)
        return Fail("PdfX: mismatched required subtype should raise error");

    // OutputIntent stream is non-empty for PDF/X.
    const auto stream = aimp::PdfXOutputIntentStream(pdfxMeta);
    if (stream.find("/Type /OutputIntent") == std::string::npos)
        return Fail("PdfX: OutputIntent stream must contain /Type key");

    // None subtype → empty stream.
    aimp::PdfXMetadata noneMeta;
    if (!aimp::PdfXOutputIntentStream(noneMeta).empty())
        return Fail("PdfX: None subtype should produce empty stream");

    // JSON.
    const auto j = aimp::PdfXMetadataToJson(pdfxMeta);
    if (j.find("\"kind\": \"pdfx-metadata\"") == std::string::npos)
        return Fail("PdfX: JSON must contain kind key");
    if (j.find("PDF/X-4") == std::string::npos)
        return Fail("PdfX: JSON must include subtype string");

    return EXIT_SUCCESS;
}

// ─────────────────────────────────────────────────────────────
// Annotations
// ─────────────────────────────────────────────────────────────
int TestAnnotations() {
    // ParseAnnotationType.
    if (aimp::ParseAnnotationType("Text") != aimp::AnnotationType::Text)
        return Fail("Annotations: ParseAnnotationType Text");
    if (aimp::ParseAnnotationType("Widget") != aimp::AnnotationType::Widget)
        return Fail("Annotations: ParseAnnotationType Widget");
    if (aimp::ParseAnnotationType("Link") != aimp::AnnotationType::Link)
        return Fail("Annotations: ParseAnnotationType Link");
    if (aimp::ParseAnnotationType("UnknownXXX") != aimp::AnnotationType::Unknown)
        return Fail("Annotations: unknown type should return Unknown");

    // Default config discards annotations.
    aimp::AnnotationConfig cfg;
    cfg.defaultHandling = aimp::AnnotationHandling::Discard;
    cfg.discardFormFields = true;
    cfg.preserveLinks = false;
    cfg.preservePrinterMarks = true;

    aimp::AnnotationRecord textAnn;
    textAnn.type = aimp::AnnotationType::Text;
    textAnn.pageIndex = 0;
    textAnn.rect = {10,10,100,50};

    aimp::AnnotationRecord linkAnn;
    linkAnn.type = aimp::AnnotationType::Link;
    linkAnn.pageIndex = 1;

    aimp::AnnotationRecord printerMarkAnn;
    printerMarkAnn.type = aimp::AnnotationType::PrinterMark;
    printerMarkAnn.pageIndex = 2;

    aimp::AnnotationRecord widgetAnn;
    widgetAnn.type = aimp::AnnotationType::Widget;
    widgetAnn.pageIndex = 3;

    const auto result = aimp::ProcessAnnotations(
        {textAnn, linkAnn, printerMarkAnn, widgetAnn}, cfg);

    if (result.processed.size() != 4)
        return Fail("Annotations: should process all 4 annotations");
    if (result.discarded == 0)
        return Fail("Annotations: default handling should discard some");

    // PrinterMark should be preserved.
    bool pmPreserved = false;
    for (const auto& p : result.processed) {
        if (p.type == aimp::AnnotationType::PrinterMark &&
            p.resolvedHandling == aimp::AnnotationHandling::Preserve) {
            pmPreserved = true;
        }
    }
    if (!pmPreserved)
        return Fail("Annotations: PrinterMark should be preserved by default");

    // PreserveLinks = true.
    cfg.preserveLinks = true;
    const auto r2 = aimp::ProcessAnnotations({linkAnn}, cfg);
    if (r2.preserved != 1)
        return Fail("Annotations: Link should be preserved when preserveLinks=true");

    // Filter override: flatten Stamp.
    aimp::AnnotationRecord stampAnn;
    stampAnn.type = aimp::AnnotationType::Stamp;
    aimp::AnnotationFilter stampFilter;
    stampFilter.type = aimp::AnnotationType::Stamp;
    stampFilter.handleAll = false;
    stampFilter.handling = aimp::AnnotationHandling::Flatten;
    cfg.filters.push_back(stampFilter);
    const auto r3 = aimp::ProcessAnnotations({stampAnn}, cfg);
    if (r3.flattened != 1)
        return Fail("Annotations: Stamp with Flatten filter should be flattened");

    // DefaultAnnotationConfigForPdfX.
    const auto pdfxCfg = aimp::DefaultAnnotationConfigForPdfX();
    if (pdfxCfg.defaultHandling != aimp::AnnotationHandling::Discard)
        return Fail("Annotations: PDF/X config should discard by default");
    if (!pdfxCfg.discardFormFields)
        return Fail("Annotations: PDF/X config should discard form fields");
    if (pdfxCfg.filters.empty())
        return Fail("Annotations: PDF/X config should have stamp/watermark filters");

    // JSON.
    const auto j = aimp::AnnotationProcessResultToJson(result);
    if (j.find("\"kind\": \"annotation-process-result\"") == std::string::npos)
        return Fail("Annotations: JSON must contain kind key");

    return EXIT_SUCCESS;
}

} // namespace

int main() {
    struct { const char* name; int(*fn)(); } tests[] = {
        {"TrimShift",    TestTrimShift},
        {"VariableData", TestVariableData},
        {"Shuffle",      TestShuffle},
        {"SplitMerge",   TestSplitMerge},
        {"PageTools",    TestPageTools},
        {"StickOn",      TestStickOn},
        {"PrinterMarks", TestPrinterMarks},
        {"AdjustPages",  TestAdjustPages},
        {"TilePages",    TestTilePages},
        {"Bleed",        TestBleed},
        {"PdfX",         TestPdfX},
        {"Annotations",  TestAnnotations},
    };

    int failed = 0;
    for (const auto& t : tests) {
        std::cerr << "[ RUN ] " << t.name << '\n';
        const int rc = t.fn();
        if (rc != EXIT_SUCCESS) {
            ++failed;
            std::cerr << "[FAIL ] " << t.name << '\n';
        } else {
            std::cerr << "[ OK  ] " << t.name << '\n';
        }
    }

    if (failed == 0) {
        std::cout << "All new-module tests passed.\n";
        return EXIT_SUCCESS;
    }
    std::cerr << failed << " test(s) failed.\n";
    return EXIT_FAILURE;
}
