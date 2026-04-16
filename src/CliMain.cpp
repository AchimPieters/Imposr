#include "aimp/ImpositionPlan.h"
#include "aimp/PdfComposer.h"
#include "aimp/Preset.h"

#include <cstdint>
#include <cstdlib>
#include <charconv>
#include <fstream>
#include <iostream>
#include <string>

namespace {

void PrintUsage() {
    std::cout
        << "Usage:\n"
        << "  imposr_cli two-up --pages <N> --sheet-width <pt> --sheet-height <pt> [--out <file>]\n"
        << "  imposr_cli n-up --pages <N> --sheet-width <pt> --sheet-height <pt> --columns <N> --rows <N> [--out <file>]\n"
        << "  imposr_cli booklet --pages <N> --sheet-width <pt> --sheet-height <pt> [--signature-size <N>] [--out <file>]\n"
        << "  imposr_cli step-repeat --pages <N> --sheet-width <pt> --sheet-height <pt> --repeat-x <N> --repeat-y <N> --step-x <pt> --step-y <pt> --slot-width <pt> --slot-height <pt> [--out <file>]\n"
        << "Common options for all modes: [--load-preset <file>] [--save-preset <file>] [--reverse 0|1] [--filter all|even|odd] [--pad-multiple N] [--signature-size N] [--fit-to-slot 0|1] [--rotate-to-fit 0|1] [--source-page-width <pt>] [--source-page-height <pt>] [--audit-out <file.xml>] [--pdf-out <file.pdf>] [--pdf-header <text>] [--pdf-footer <text>] [--pdf-sheet-number 0|1] [--pdf-bates-enable 0|1] [--pdf-bates-prefix <text>] [--pdf-bates-start N] [--inspect-source-page <N>] [--inspect-sheet <N> --inspect-slot <N>]\n";
}

bool ParseUInt(const std::string& value, std::uint32_t& output) {
    const char* begin = value.data();
    const char* end = value.data() + value.size();
    const auto result = std::from_chars(begin, end, output);
    return result.ec == std::errc {} && result.ptr == end;
}

bool ParseDouble(const std::string& value, double& output) {
    char* parseEnd = nullptr;
    output = std::strtod(value.c_str(), &parseEnd);
    return parseEnd != value.c_str() && *parseEnd == '\0';
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        PrintUsage();
        return 1;
    }

    const std::string mode = argv[1];
    std::uint32_t pages = 0;
    std::uint32_t columns = 0;
    std::uint32_t rows = 0;
    std::uint32_t repeatX = 0;
    std::uint32_t repeatY = 0;
    double sheetWidth = 0.0;
    double sheetHeight = 0.0;
    double stepX = 0.0;
    double stepY = 0.0;
    double slotWidth = 0.0;
    double slotHeight = 0.0;
    std::string outPath;
    std::string auditOutPath;
    std::string pdfOutPath;
    aimp::PdfComposeOptions pdfOptions {};
    std::uint32_t inspectSourcePage = aimp::kBlankPageIndex;
    std::uint32_t inspectSheet = aimp::kBlankPageIndex;
    std::uint32_t inspectSlot = aimp::kBlankPageIndex;
    aimp::BuildOptions buildOptions {};
    std::string savePresetPath;
    std::string loadPresetPath;

    for (int i = 2; i + 1 < argc; ++i) {
        if (std::string(argv[i]) == "--load-preset") {
            loadPresetPath = argv[i + 1];
        }
    }

    if (!loadPresetPath.empty()) {
        aimp::PlannerPreset preset {};
        std::string error;
        if (!aimp::LoadPreset(loadPresetPath, preset, error)) {
            std::cerr << "Could not load preset: " << error << '\n';
            return 1;
        }
        sheetWidth = preset.sheetSize.widthPoints;
        sheetHeight = preset.sheetSize.heightPoints;
        columns = preset.columns;
        rows = preset.rows;
        repeatX = preset.repeatX;
        repeatY = preset.repeatY;
        stepX = preset.stepX;
        stepY = preset.stepY;
        slotWidth = preset.slotWidth;
        slotHeight = preset.slotHeight;
        buildOptions = preset.buildOptions;
        pdfOptions = preset.pdfOptions;
    }

    for (int i = 2; i < argc; ++i) {
        const std::string key = argv[i];
        if (i + 1 >= argc) {
            std::cerr << "Missing value for argument: " << key << '\n';
            return 1;
        }

        const std::string value = argv[++i];
        if (key == "--pages") {
            if (!ParseUInt(value, pages)) {
                std::cerr << "Invalid value for --pages\n";
                return 1;
            }
        } else if (key == "--sheet-width") {
            if (!ParseDouble(value, sheetWidth)) {
                std::cerr << "Invalid value for --sheet-width\n";
                return 1;
            }
        } else if (key == "--sheet-height") {
            if (!ParseDouble(value, sheetHeight)) {
                std::cerr << "Invalid value for --sheet-height\n";
                return 1;
            }
        } else if (key == "--columns") {
            if (!ParseUInt(value, columns)) {
                std::cerr << "Invalid value for --columns\n";
                return 1;
            }
        } else if (key == "--rows") {
            if (!ParseUInt(value, rows)) {
                std::cerr << "Invalid value for --rows\n";
                return 1;
            }
        } else if (key == "--out") {
            outPath = value;
        } else if (key == "--audit-out") {
            auditOutPath = value;
        } else if (key == "--load-preset") {
            // Already handled in first pass.
            continue;
        } else if (key == "--save-preset") {
            savePresetPath = value;
        } else if (key == "--pdf-out") {
            pdfOutPath = value;
        } else if (key == "--pdf-header") {
            pdfOptions.headerText = value;
        } else if (key == "--pdf-footer") {
            pdfOptions.footerText = value;
        } else if (key == "--pdf-sheet-number") {
            std::uint32_t raw = 1;
            if (!ParseUInt(value, raw) || raw > 1) {
                std::cerr << "Invalid value for --pdf-sheet-number (expected 0 or 1)\n";
                return 1;
            }
            pdfOptions.includeSheetNumber = (raw == 1);
        } else if (key == "--pdf-bates-enable") {
            std::uint32_t raw = 0;
            if (!ParseUInt(value, raw) || raw > 1) {
                std::cerr << "Invalid value for --pdf-bates-enable (expected 0 or 1)\n";
                return 1;
            }
            pdfOptions.includeBates = (raw == 1);
        } else if (key == "--pdf-bates-prefix") {
            pdfOptions.batesPrefix = value;
            pdfOptions.includeBates = true;
        } else if (key == "--pdf-bates-start") {
            if (!ParseUInt(value, pdfOptions.batesStart)) {
                std::cerr << "Invalid value for --pdf-bates-start\n";
                return 1;
            }
        } else if (key == "--inspect-source-page") {
            if (!ParseUInt(value, inspectSourcePage)) {
                std::cerr << "Invalid value for --inspect-source-page\n";
                return 1;
            }
        } else if (key == "--inspect-sheet") {
            if (!ParseUInt(value, inspectSheet)) {
                std::cerr << "Invalid value for --inspect-sheet\n";
                return 1;
            }
        } else if (key == "--inspect-slot") {
            if (!ParseUInt(value, inspectSlot)) {
                std::cerr << "Invalid value for --inspect-slot\n";
                return 1;
            }
        } else if (key == "--repeat-x") {
            if (!ParseUInt(value, repeatX)) {
                std::cerr << "Invalid value for --repeat-x\n";
                return 1;
            }
        } else if (key == "--repeat-y") {
            if (!ParseUInt(value, repeatY)) {
                std::cerr << "Invalid value for --repeat-y\n";
                return 1;
            }
        } else if (key == "--step-x") {
            if (!ParseDouble(value, stepX)) {
                std::cerr << "Invalid value for --step-x\n";
                return 1;
            }
        } else if (key == "--step-y") {
            if (!ParseDouble(value, stepY)) {
                std::cerr << "Invalid value for --step-y\n";
                return 1;
            }
        } else if (key == "--slot-width") {
            if (!ParseDouble(value, slotWidth)) {
                std::cerr << "Invalid value for --slot-width\n";
                return 1;
            }
        } else if (key == "--slot-height") {
            if (!ParseDouble(value, slotHeight)) {
                std::cerr << "Invalid value for --slot-height\n";
                return 1;
            }
        } else if (key == "--reverse") {
            std::uint32_t raw = 0;
            if (!ParseUInt(value, raw) || raw > 1) {
                std::cerr << "Invalid value for --reverse (expected 0 or 1)\n";
                return 1;
            }
            buildOptions.reverseOrder = (raw == 1);
        } else if (key == "--pad-multiple") {
            if (!ParseUInt(value, buildOptions.padToMultiple)) {
                std::cerr << "Invalid value for --pad-multiple\n";
                return 1;
            }
        } else if (key == "--signature-size") {
            if (!ParseUInt(value, buildOptions.bookletSignatureSize)) {
                std::cerr << "Invalid value for --signature-size\n";
                return 1;
            }
        } else if (key == "--fit-to-slot") {
            std::uint32_t raw = 0;
            if (!ParseUInt(value, raw) || raw > 1) {
                std::cerr << "Invalid value for --fit-to-slot (expected 0 or 1)\n";
                return 1;
            }
            buildOptions.scaleToFit = (raw == 1);
        } else if (key == "--rotate-to-fit") {
            std::uint32_t raw = 0;
            if (!ParseUInt(value, raw) || raw > 1) {
                std::cerr << "Invalid value for --rotate-to-fit (expected 0 or 1)\n";
                return 1;
            }
            buildOptions.autoRotateToFit = (raw == 1);
        } else if (key == "--source-page-width") {
            if (!ParseDouble(value, buildOptions.sourcePageWidthPoints)) {
                std::cerr << "Invalid value for --source-page-width\n";
                return 1;
            }
        } else if (key == "--source-page-height") {
            if (!ParseDouble(value, buildOptions.sourcePageHeightPoints)) {
                std::cerr << "Invalid value for --source-page-height\n";
                return 1;
            }
        } else if (key == "--filter") {
            if (value == "all") {
                buildOptions.filter = aimp::PageFilter::All;
            } else if (value == "even") {
                buildOptions.filter = aimp::PageFilter::EvenOnly;
            } else if (value == "odd") {
                buildOptions.filter = aimp::PageFilter::OddOnly;
            } else {
                std::cerr << "Invalid value for --filter (all|even|odd)\n";
                return 1;
            }
        } else {
            std::cerr << "Unknown argument: " << key << '\n';
            return 1;
        }
    }

    const aimp::SheetSize sheet {sheetWidth, sheetHeight};
    aimp::ImpositionPlan plan {};
    if (mode == "two-up") {
        plan = aimp::TwoUpPlanner::Build("cli-input", pages, sheet, buildOptions);
    } else if (mode == "n-up") {
        if (columns == 0 || rows == 0) {
            std::cerr << "n-up mode requires --columns and --rows\n";
            return 1;
        }
        plan = aimp::NUpPlanner::Build("cli-input", pages, sheet, columns, rows, buildOptions);
    } else if (mode == "booklet") {
        plan = aimp::BookletPlanner::Build("cli-input", pages, sheet, buildOptions);
    } else if (mode == "step-repeat") {
        if (repeatX == 0 || repeatY == 0 || slotWidth <= 0.0 || slotHeight <= 0.0) {
            std::cerr << "step-repeat mode requires repeat/step/slot arguments\n";
            return 1;
        }
        const aimp::StepRepeatConfig config {
            repeatX,
            repeatY,
            stepX,
            stepY,
            aimp::Rect {0.0, 0.0, slotWidth, slotHeight}
        };
        plan = aimp::StepAndRepeatPlanner::Build("cli-input", pages, sheet, config, buildOptions);
    } else {
        std::cerr << "Unknown mode: " << mode << '\n';
        PrintUsage();
        return 1;
    }

    const std::string json = aimp::ToJson(plan);
    if (!outPath.empty()) {
        std::ofstream file(outPath);
        if (!file) {
            std::cerr << "Could not open output file: " << outPath << '\n';
            return 1;
        }
        file << json;
    } else {
        std::cout << json;
    }

    if (!auditOutPath.empty()) {
        std::ofstream file(auditOutPath);
        if (!file) {
            std::cerr << "Could not open audit output file: " << auditOutPath << '\n';
            return 1;
        }
        file << aimp::ToAuditXml(plan);
    }

    if (!pdfOutPath.empty()) {
        std::string errorMessage;
        if (!aimp::ComposePlanPdf(plan, pdfOutPath, pdfOptions, errorMessage)) {
            std::cerr << "Could not write PDF output: " << errorMessage << '\n';
            return 1;
        }
    }

    if (!savePresetPath.empty()) {
        aimp::PlannerPreset preset {};
        preset.sheetSize = sheet;
        preset.columns = columns;
        preset.rows = rows;
        preset.repeatX = repeatX;
        preset.repeatY = repeatY;
        preset.stepX = stepX;
        preset.stepY = stepY;
        preset.slotWidth = slotWidth;
        preset.slotHeight = slotHeight;
        preset.buildOptions = buildOptions;
        preset.pdfOptions = pdfOptions;
        std::string error;
        if (!aimp::SavePreset(preset, savePresetPath, error)) {
            std::cerr << "Could not save preset: " << error << '\n';
            return 1;
        }
    }

    if (inspectSourcePage != aimp::kBlankPageIndex) {
        const auto matches = aimp::FindPlacementsForSourcePage(plan, "cli-input", inspectSourcePage);
        std::cout << "\n# Inspector: source page " << inspectSourcePage << '\n';
        for (const auto& match : matches) {
            std::cout << "sheet=" << match.sheetIndex << ", slot=" << match.slotIndex << '\n';
        }
        if (matches.empty()) {
            std::cout << "(no placements found)\n";
        }
    }

    if (inspectSheet != aimp::kBlankPageIndex && inspectSlot != aimp::kBlankPageIndex) {
        aimp::PageRef source {};
        std::cout << "\n# Inspector: sheet=" << inspectSheet << ", slot=" << inspectSlot << '\n';
        if (aimp::TryGetSourceForPlacement(plan, inspectSheet, inspectSlot, source)) {
            std::cout << "sourceDocumentId=" << source.sourceDocumentId << ", pageIndex=" << source.pageIndex << '\n';
        } else {
            std::cout << "(no source found)\n";
        }
    }

    return 0;
}
