#include "aimp/ImpositionPlan.h"
#include "aimp/PdfComposer.h"
#include "aimp/Preset.h"

#include <cstdint>
#include <cstdlib>
#include <charconv>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace {

std::string EscapeJsonString(const std::string& input) {
    std::string out;
    out.reserve(input.size() + 8);
    for (char ch : input) {
        switch (ch) {
            case '\\': out += "\\\\"; break;
            case '\"': out += "\\\""; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default: out.push_back(ch); break;
        }
    }
    return out;
}

std::string BuildPreflightJson(const std::vector<aimp::PreflightIssue>& issues) {
    std::ostringstream out;
    out << "{\n";
    out << "  \"status\": \"" << (issues.empty() ? "ok" : "issues") << "\",\n";
    out << "  \"issues\": [\n";
    for (std::size_t i = 0; i < issues.size(); ++i) {
        const auto& issue = issues[i];
        out << "    {\"severity\": \"" << (issue.isError ? "error" : "warning")
            << "\", \"code\": \"" << EscapeJsonString(issue.code)
            << "\", \"message\": \"" << EscapeJsonString(issue.message) << "\"}";
        if (i + 1 < issues.size()) {
            out << ',';
        }
        out << '\n';
    }
    out << "  ]\n";
    out << "}\n";
    return out.str();
}

std::string BuildJobReportJson(const std::string& mode,
                               std::uint32_t pages,
                               const std::string& outputPlanPath,
                               const std::string& outputAuditPath,
                               const std::string& outputManifestPath,
                               const std::string& outputAcrobatJsPath,
                               const std::string& outputCompositionPath,
                               const std::string& outputPdfPath,
                               const std::string& outputPreflightPath,
                               const aimp::ImpositionPlan& plan,
                               const aimp::PdfComposeOptions& pdfOptions,
                               const std::vector<aimp::ValidationIssue>& validationIssues,
                               const std::vector<aimp::PreflightIssue>& preflightIssues) {
    std::ostringstream out;
    out << "{\n";
    out << "  \"job\": {\n";
    out << "    \"mode\": \"" << EscapeJsonString(mode) << "\",\n";
    out << "    \"inputPages\": " << pages << ",\n";
    out << "    \"summary\": \"" << EscapeJsonString(aimp::BuildHumanSummary(plan)) << "\",\n";
    out << "    \"pdfxProfile\": \"" << EscapeJsonString(aimp::PdfxProfileName(pdfOptions.targetPdfxProfile)) << "\"\n";
    out << "  },\n";
    out << "  \"outputs\": {\n";
    out << "    \"planJson\": \"" << EscapeJsonString(outputPlanPath) << "\",\n";
    out << "    \"auditXml\": \"" << EscapeJsonString(outputAuditPath) << "\",\n";
    out << "    \"manifestJson\": \"" << EscapeJsonString(outputManifestPath) << "\",\n";
    out << "    \"acrobatPlacementJs\": \"" << EscapeJsonString(outputAcrobatJsPath) << "\",\n";
    out << "    \"productionCompositionJson\": \"" << EscapeJsonString(outputCompositionPath) << "\",\n";
    out << "    \"proofPdf\": \"" << EscapeJsonString(outputPdfPath) << "\",\n";
    out << "    \"preflightJson\": \"" << EscapeJsonString(outputPreflightPath) << "\"\n";
    out << "  },\n";
    out << "  \"qualityGate\": {\n";
    out << "    \"validationIssueCount\": " << validationIssues.size() << ",\n";
    out << "    \"preflightIssueCount\": " << preflightIssues.size() << ",\n";
    std::size_t preflightErrorCount = 0;
    for (const auto& issue : preflightIssues) {
        if (issue.isError) {
            ++preflightErrorCount;
        }
    }
    out << "    \"preflightErrorCount\": " << preflightErrorCount << ",\n";
    out << "    \"status\": \"" << ((validationIssues.empty() && preflightErrorCount == 0) ? "pass" : "fail") << "\"\n";
    out << "  }\n";
    out << "}\n";
    return out.str();
}

void PrintUsage() {
    std::cout
        << "Usage:\n"
        << "  imposr_cli two-up --pages <N> --sheet-width <pt> --sheet-height <pt> [--out <file>]\n"
        << "  imposr_cli n-up --pages <N> --sheet-width <pt> --sheet-height <pt> --columns <N> --rows <N> [--out <file>]\n"
        << "  imposr_cli booklet --pages <N> --sheet-width <pt> --sheet-height <pt> [--signature-size <N>] [--out <file>]\n"
        << "  imposr_cli step-repeat --pages <N> --sheet-width <pt> --sheet-height <pt> --repeat-x <N> --repeat-y <N> --step-x <pt> --step-y <pt> --slot-width <pt> --slot-height <pt> [--out <file>]\n"
        << "  imposr_cli tile --pages <N> --sheet-width <pt> --sheet-height <pt> --columns <N> --rows <N> [--tile-overlap <pt>] [--out <file>]\n"
        << "  imposr_cli manual --pages <N> --sheet-width <pt> --sheet-height <pt> --columns <N> --rows <N> --manual-sequence <csv>\n"
        << "  imposr_cli batch --batch-csv <jobs.csv> --output-dir <dir> [--batch-report-out <file.json>] [--batch-stop-on-error 0|1]\n"
        << "Common options for all modes: [--load-preset <file>] [--save-preset <file>] [--page-sequence <csv>] [--reverse 0|1] [--filter all|even|odd] [--pad-multiple N] [--signature-size N] [--creep <pt>] [--fit-to-slot 0|1] [--rotate-to-fit 0|1] [--source-page-width <pt>] [--source-page-height <pt>] [--audit-out <file.xml>] [--manifest-out <file.json>] [--acrobat-js-out <file.js>] [--composition-out <file.json>] [--pdf-out <file.pdf>] [--job-out <file.json>] [--output-dir <dir>] [--output-stem <name>] [--stamp-output 0|1] [--batch-csv <jobs.csv>] [--batch-report-out <file.json>] [--batch-stop-on-error 0|1] [--pdf-header <text>] [--pdf-footer <text>] [--pdf-sheet-number 0|1] [--pdf-bates-enable 0|1] [--pdf-bates-prefix <text>] [--pdf-bates-start N] [--pdf-trim-marks 0|1] [--pdf-trim-length <pt>] [--pdf-trim-offset <pt>] [--pdf-bleed-box 0|1] [--pdf-bleed <pt>] [--pdf-overlay-template <text>] [--pdf-variable-csv <file.csv>] [--pdfx-profile none|pdfx-1a|pdfx-4] [--preflight 0|1] [--preflight-out <file.json>] [--fail-on-preflight 0|1] [--fail-on-quality-gate 0|1] [--summary 0|1] [--validate 0|1] [--fail-on-validation 0|1] [--inspect-source-page <N>] [--inspect-sheet <N> --inspect-slot <N>]\n";
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

bool ParsePageSequenceCsv(const std::string& csv, std::vector<std::uint32_t>& sequence, std::string& errorMessage) {
    sequence.clear();
    std::stringstream stream(csv);
    std::string token;
    while (std::getline(stream, token, ',')) {
        if (token.empty()) {
            errorMessage = "Empty item in page sequence CSV.";
            return false;
        }

        std::uint32_t humanPage = 0;
        if (!ParseUInt(token, humanPage)) {
            errorMessage = "Invalid page number in page sequence: " + token;
            return false;
        }

        if (humanPage == 0) {
            sequence.push_back(aimp::kBlankPageIndex);
            continue;
        }
        sequence.push_back(humanPage - 1u);
    }

    if (sequence.empty()) {
        errorMessage = "Page sequence cannot be empty.";
        return false;
    }
    return true;
}

std::string BuildUtcTimestamp() {
    const auto now = std::chrono::system_clock::now();
    const std::time_t nowTime = std::chrono::system_clock::to_time_t(now);
    std::tm utc {};
#if defined(_WIN32)
    gmtime_s(&utc, &nowTime);
#else
    gmtime_r(&nowTime, &utc);
#endif
    std::ostringstream out;
    out << std::put_time(&utc, "%Y%m%d-%H%M%SZ");
    return out.str();
}

std::string BuildDefaultOutputStem(const std::string& mode, std::uint32_t pages) {
    std::ostringstream out;
    out << mode << '_' << pages << "p";
    return out.str();
}

std::vector<std::string> ParseCsvLine(const std::string& line) {
    std::vector<std::string> fields;
    std::string current;
    bool inQuotes = false;
    for (std::size_t i = 0; i < line.size(); ++i) {
        const char ch = line[i];
        if (ch == '"') {
            if (inQuotes && i + 1 < line.size() && line[i + 1] == '"') {
                current.push_back('"');
                ++i;
            } else {
                inQuotes = !inQuotes;
            }
            continue;
        }
        if (ch == ',' && !inQuotes) {
            fields.push_back(current);
            current.clear();
            continue;
        }
        current.push_back(ch);
    }
    fields.push_back(current);
    return fields;
}

std::string TrimAscii(std::string value) {
    while (!value.empty() && (value.front() == ' ' || value.front() == '\t' || value.front() == '\r')) {
        value.erase(value.begin());
    }
    while (!value.empty() && (value.back() == ' ' || value.back() == '\t' || value.back() == '\r')) {
        value.pop_back();
    }
    return value;
}

std::string BuildBatchReportJson(std::uint32_t totalJobs,
                                 std::uint32_t successJobs,
                                 std::uint32_t failedJobs) {
    std::ostringstream out;
    out << "{\n";
    out << "  \"type\": \"imposr-batch-report\",\n";
    out << "  \"totalJobs\": " << totalJobs << ",\n";
    out << "  \"successJobs\": " << successJobs << ",\n";
    out << "  \"failedJobs\": " << failedJobs << ",\n";
    out << "  \"status\": \"" << (failedJobs == 0 ? "ok" : "failed") << "\"\n";
    out << "}\n";
    return out.str();
}

int RunBatchMode(const std::string& cliPath,
                 const std::string& csvPath,
                 const std::string& outputDir,
                 const std::string& batchReportOutPath,
                 bool stopOnError,
                 bool failOnQualityGate) {
    if (csvPath.empty()) {
        std::cerr << "batch mode requires --batch-csv\n";
        return 1;
    }
    if (outputDir.empty()) {
        std::cerr << "batch mode requires --output-dir\n";
        return 1;
    }

    std::ifstream in(csvPath);
    if (!in) {
        std::cerr << "Could not open --batch-csv: " << csvPath << '\n';
        return 1;
    }

    std::string headerLine;
    if (!std::getline(in, headerLine)) {
        std::cerr << "--batch-csv is empty\n";
        return 1;
    }
    const auto headers = ParseCsvLine(headerLine);
    std::map<std::string, std::size_t> columnIndex;
    for (std::size_t i = 0; i < headers.size(); ++i) {
        columnIndex[TrimAscii(headers[i])] = i;
    }
    if (columnIndex.find("mode") == columnIndex.end() ||
        columnIndex.find("pages") == columnIndex.end() ||
        columnIndex.find("sheet_width") == columnIndex.end() ||
        columnIndex.find("sheet_height") == columnIndex.end()) {
        std::cerr << "--batch-csv must include mode,pages,sheet_width,sheet_height columns\n";
        return 1;
    }

    std::uint32_t totalJobs = 0;
    std::uint32_t successJobs = 0;
    std::uint32_t failedJobs = 0;
    std::string executablePath = TrimAscii(cliPath);
    if (executablePath.size() >= 2 && executablePath.front() == '"' && executablePath.back() == '"') {
        executablePath = executablePath.substr(1, executablePath.size() - 2);
    }
    std::string line;
    while (std::getline(in, line)) {
        if (TrimAscii(line).empty()) {
            continue;
        }
        const auto fields = ParseCsvLine(line);
        const auto get = [&](const std::string& key, const std::string& fallback = "") -> std::string {
            const auto it = columnIndex.find(key);
            if (it == columnIndex.end() || it->second >= fields.size()) {
                return fallback;
            }
            return TrimAscii(fields[it->second]);
        };

        const std::string mode = get("mode");
        const std::string pages = get("pages");
        const std::string sheetWidth = get("sheet_width");
        const std::string sheetHeight = get("sheet_height");
        const std::string columns = get("columns");
        const std::string rows = get("rows");
        const std::string outputStem = get("output_stem", mode + "_" + pages + "p");
        const std::string pdfxProfile = get("pdfx_profile", "none");
        const std::string trimMarks = get("trim_marks", "0");
        const std::string bleedBox = get("bleed_box", "0");
        const std::string bleed = get("bleed", "0");

        std::ostringstream cmd;
        cmd << '"' << executablePath << "\" " << mode
            << " --pages " << pages
            << " --sheet-width " << sheetWidth
            << " --sheet-height " << sheetHeight
            << " --output-dir \"" << outputDir << '"'
            << " --output-stem \"" << outputStem << '"'
            << " --pdfx-profile " << pdfxProfile
            << " --pdf-trim-marks " << trimMarks
            << " --pdf-bleed-box " << bleedBox
            << " --pdf-bleed " << bleed
            << " --preflight 1";

        if (!columns.empty()) {
            cmd << " --columns " << columns;
        }
        if (!rows.empty()) {
            cmd << " --rows " << rows;
        }
        if (failOnQualityGate) {
            cmd << " --fail-on-quality-gate 1";
        }

        ++totalJobs;
        const int rc = std::system(cmd.str().c_str());
        if (rc == 0) {
            ++successJobs;
        } else {
            ++failedJobs;
            std::cerr << "Batch job failed for output_stem=" << outputStem << " (exit=" << rc << ")\n";
            if (stopOnError) {
                break;
            }
        }
    }

    if (!batchReportOutPath.empty()) {
        std::error_code ec;
        const auto reportParent = std::filesystem::path(batchReportOutPath).parent_path();
        if (!reportParent.empty()) {
            std::filesystem::create_directories(reportParent, ec);
            if (ec) {
                std::cerr << "Could not create parent directory for --batch-report-out: " << reportParent.string() << '\n';
                return 1;
            }
        }
        std::ofstream out(batchReportOutPath);
        if (!out) {
            std::cerr << "Could not open --batch-report-out: " << batchReportOutPath << '\n';
            return 1;
        }
        out << BuildBatchReportJson(totalJobs, successJobs, failedJobs);
    }

    return failedJobs == 0 ? 0 : 5;
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
    double tileOverlap = 0.0;
    std::string outPath;
    std::string auditOutPath;
    std::string manifestOutPath;
    std::string acrobatJsOutPath;
    std::string compositionOutPath;
    std::string preflightOutPath;
    std::string jobOutPath;
    std::string pdfOutPath;
    std::string outputDir;
    std::string outputStem;
    std::string batchCsvPath;
    std::string batchReportOutPath;
    bool stampOutput = false;
    aimp::PdfComposeOptions pdfOptions {};
    std::uint32_t inspectSourcePage = aimp::kBlankPageIndex;
    std::uint32_t inspectSheet = aimp::kBlankPageIndex;
    std::uint32_t inspectSlot = aimp::kBlankPageIndex;
    aimp::BuildOptions buildOptions {};
    std::string savePresetPath;
    std::string loadPresetPath;
    bool emitSummary = false;
    bool emitValidation = false;
    bool failOnValidation = false;
    bool emitPreflight = false;
    bool failOnPreflight = false;
    bool failOnQualityGate = false;
    bool batchStopOnError = false;
    std::string manualSequenceCsv;
    std::vector<std::uint32_t> manualSequence;
    std::string pageSequenceCsv;
    std::vector<std::uint32_t> pageSequence;

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
        tileOverlap = preset.tileOverlap;
        stepX = preset.stepX;
        stepY = preset.stepY;
        slotWidth = preset.slotWidth;
        slotHeight = preset.slotHeight;
        buildOptions = preset.buildOptions;
        pdfOptions = preset.pdfOptions;
        if (!preset.manualSequence.empty() && manualSequenceCsv.empty()) {
            manualSequence = preset.manualSequence;
        }
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
        } else if (key == "--manifest-out") {
            manifestOutPath = value;
        } else if (key == "--acrobat-js-out") {
            acrobatJsOutPath = value;
        } else if (key == "--composition-out") {
            compositionOutPath = value;
        } else if (key == "--job-out") {
            jobOutPath = value;
        } else if (key == "--load-preset") {
            // Already handled in first pass.
            continue;
        } else if (key == "--save-preset") {
            savePresetPath = value;
        } else if (key == "--pdf-out") {
            pdfOutPath = value;
        } else if (key == "--output-dir") {
            outputDir = value;
        } else if (key == "--output-stem") {
            outputStem = value;
        } else if (key == "--batch-csv") {
            batchCsvPath = value;
        } else if (key == "--batch-report-out") {
            batchReportOutPath = value;
        } else if (key == "--batch-stop-on-error") {
            std::uint32_t raw = 0;
            if (!ParseUInt(value, raw) || raw > 1) {
                std::cerr << "Invalid value for --batch-stop-on-error (expected 0 or 1)\n";
                return 1;
            }
            batchStopOnError = (raw == 1);
        } else if (key == "--stamp-output") {
            std::uint32_t raw = 0;
            if (!ParseUInt(value, raw) || raw > 1) {
                std::cerr << "Invalid value for --stamp-output (expected 0 or 1)\n";
                return 1;
            }
            stampOutput = (raw == 1);
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
        } else if (key == "--pdf-trim-marks") {
            std::uint32_t raw = 0;
            if (!ParseUInt(value, raw) || raw > 1) {
                std::cerr << "Invalid value for --pdf-trim-marks (expected 0 or 1)\n";
                return 1;
            }
            pdfOptions.drawTrimMarks = (raw == 1);
        } else if (key == "--pdf-trim-length") {
            if (!ParseDouble(value, pdfOptions.trimMarkLengthPoints)) {
                std::cerr << "Invalid value for --pdf-trim-length\n";
                return 1;
            }
        } else if (key == "--pdf-trim-offset") {
            if (!ParseDouble(value, pdfOptions.trimMarkOffsetPoints)) {
                std::cerr << "Invalid value for --pdf-trim-offset\n";
                return 1;
            }
        } else if (key == "--pdf-bleed-box") {
            std::uint32_t raw = 0;
            if (!ParseUInt(value, raw) || raw > 1) {
                std::cerr << "Invalid value for --pdf-bleed-box (expected 0 or 1)\n";
                return 1;
            }
            pdfOptions.drawBleedBox = (raw == 1);
        } else if (key == "--pdf-bleed") {
            if (!ParseDouble(value, pdfOptions.bleedPoints)) {
                std::cerr << "Invalid value for --pdf-bleed\n";
                return 1;
            }
        } else if (key == "--pdf-overlay-template") {
            pdfOptions.overlayTemplate = value;
        } else if (key == "--pdf-variable-csv") {
            pdfOptions.variableDataCsvPath = value;
        } else if (key == "--pdfx-profile") {
            if (!aimp::TryParsePdfxProfile(value, pdfOptions.targetPdfxProfile)) {
                std::cerr << "Invalid value for --pdfx-profile (none|pdfx-1a|pdfx-4)\n";
                return 1;
            }
        } else if (key == "--preflight") {
            std::uint32_t raw = 0;
            if (!ParseUInt(value, raw) || raw > 1) {
                std::cerr << "Invalid value for --preflight (expected 0 or 1)\n";
                return 1;
            }
            emitPreflight = (raw == 1);
        } else if (key == "--preflight-out") {
            preflightOutPath = value;
        } else if (key == "--fail-on-preflight") {
            std::uint32_t raw = 0;
            if (!ParseUInt(value, raw) || raw > 1) {
                std::cerr << "Invalid value for --fail-on-preflight (expected 0 or 1)\n";
                return 1;
            }
            failOnPreflight = (raw == 1);
        } else if (key == "--fail-on-quality-gate") {
            std::uint32_t raw = 0;
            if (!ParseUInt(value, raw) || raw > 1) {
                std::cerr << "Invalid value for --fail-on-quality-gate (expected 0 or 1)\n";
                return 1;
            }
            failOnQualityGate = (raw == 1);
        } else if (key == "--summary") {
            std::uint32_t raw = 0;
            if (!ParseUInt(value, raw) || raw > 1) {
                std::cerr << "Invalid value for --summary (expected 0 or 1)\n";
                return 1;
            }
            emitSummary = (raw == 1);
        } else if (key == "--validate") {
            std::uint32_t raw = 0;
            if (!ParseUInt(value, raw) || raw > 1) {
                std::cerr << "Invalid value for --validate (expected 0 or 1)\n";
                return 1;
            }
            emitValidation = (raw == 1);
        } else if (key == "--fail-on-validation") {
            std::uint32_t raw = 0;
            if (!ParseUInt(value, raw) || raw > 1) {
                std::cerr << "Invalid value for --fail-on-validation (expected 0 or 1)\n";
                return 1;
            }
            failOnValidation = (raw == 1);
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
        } else if (key == "--manual-sequence") {
            manualSequenceCsv = value;
        } else if (key == "--page-sequence") {
            pageSequenceCsv = value;
        } else if (key == "--tile-overlap") {
            if (!ParseDouble(value, tileOverlap)) {
                std::cerr << "Invalid value for --tile-overlap\n";
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
        } else if (key == "--creep") {
            if (!ParseDouble(value, buildOptions.bookletCreepPerSheetPoints) || buildOptions.bookletCreepPerSheetPoints < 0.0) {
                std::cerr << "Invalid value for --creep (expected >= 0)\n";
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

    if (mode == "batch") {
        return RunBatchMode(argv[0], batchCsvPath, outputDir, batchReportOutPath, batchStopOnError, failOnQualityGate);
    }

    if (!pageSequenceCsv.empty()) {
        std::string parseError;
        if (!ParsePageSequenceCsv(pageSequenceCsv, pageSequence, parseError)) {
            std::cerr << parseError << '\n';
            return 1;
        }
        buildOptions.explicitPageSequence = pageSequence;
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
    } else if (mode == "manual") {
        if (columns == 0 || rows == 0) {
            std::cerr << "manual mode requires --columns and --rows\n";
            return 1;
        }
        if (manualSequenceCsv.empty()) {
            if (manualSequence.empty()) {
                std::cerr << "manual mode requires --manual-sequence (or a preset containing manualSequence)\n";
                return 1;
            }
        }
        if (!manualSequenceCsv.empty()) {
            std::string parseError;
            if (!ParsePageSequenceCsv(manualSequenceCsv, manualSequence, parseError)) {
                std::cerr << parseError << '\n';
                return 1;
            }
        }
        plan = aimp::ManualPlanner::Build("cli-input", pages, sheet, columns, rows, manualSequence, buildOptions);
    } else if (mode == "tile") {
        if (columns == 0 || rows == 0) {
            std::cerr << "tile mode requires --columns and --rows\n";
            return 1;
        }
        if (tileOverlap < 0.0) {
            std::cerr << "--tile-overlap must be >= 0\n";
            return 1;
        }
        const aimp::TileConfig config {columns, rows, tileOverlap};
        plan = aimp::TilePlanner::Build("cli-input", pages, sheet, config, buildOptions);
    } else {
        std::cerr << "Unknown mode: " << mode << '\n';
        PrintUsage();
        return 1;
    }

    if (!outputDir.empty()) {
        std::error_code ec;
        std::filesystem::create_directories(outputDir, ec);
        if (ec) {
            std::cerr << "Could not create --output-dir: " << outputDir << '\n';
            return 1;
        }

        std::string resolvedStem = outputStem.empty() ? BuildDefaultOutputStem(mode, pages) : outputStem;
        if (stampOutput) {
            resolvedStem += "_" + BuildUtcTimestamp();
        }

        const std::filesystem::path base = std::filesystem::path(outputDir) / resolvedStem;
        if (outPath.empty()) {
            outPath = base.string() + ".plan.json";
        }
        if (auditOutPath.empty()) {
            auditOutPath = base.string() + ".audit.xml";
        }
        if (manifestOutPath.empty()) {
            manifestOutPath = base.string() + ".manifest.json";
        }
        if (acrobatJsOutPath.empty()) {
            acrobatJsOutPath = base.string() + ".acrobat-placement.js";
        }
        if (compositionOutPath.empty()) {
            compositionOutPath = base.string() + ".production-composition.json";
        }
        if (pdfOutPath.empty()) {
            pdfOutPath = base.string() + ".proof.pdf";
        }
        if (preflightOutPath.empty()) {
            preflightOutPath = base.string() + ".preflight.json";
        }
        if (jobOutPath.empty()) {
            jobOutPath = base.string() + ".job.json";
        }
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

    if (!manifestOutPath.empty()) {
        std::ofstream file(manifestOutPath);
        if (!file) {
            std::cerr << "Could not open manifest output file: " << manifestOutPath << '\n';
            return 1;
        }
        file << aimp::ToPlacementManifestJson(plan);
    }
    if (!acrobatJsOutPath.empty()) {
        std::ofstream file(acrobatJsOutPath);
        if (!file) {
            std::cerr << "Could not open Acrobat JS output file: " << acrobatJsOutPath << '\n';
            return 1;
        }
        file << aimp::ToAcrobatPlacementJs(plan);
    }
    if (!compositionOutPath.empty()) {
        std::ofstream file(compositionOutPath);
        if (!file) {
            std::cerr << "Could not open production composition output file: " << compositionOutPath << '\n';
            return 1;
        }
        file << aimp::ToProductionCompositionJson(plan, buildOptions, pdfOptions);
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
        preset.tileOverlap = tileOverlap;
        preset.repeatX = repeatX;
        preset.repeatY = repeatY;
        preset.stepX = stepX;
        preset.stepY = stepY;
        preset.slotWidth = slotWidth;
        preset.slotHeight = slotHeight;
        preset.manualSequence = manualSequence;
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

    if (emitSummary) {
        std::cout << "\n# Summary\n" << aimp::BuildHumanSummary(plan) << '\n';
    }

    std::vector<aimp::ValidationIssue> validationIssues;
    if (emitValidation || failOnValidation || failOnQualityGate) {
        validationIssues = aimp::ValidatePlan(plan);
    }

    if (emitValidation) {
        std::cout << "\n# Validation\n";
        if (validationIssues.empty()) {
            std::cout << "OK\n";
        } else {
            for (const auto& issue : validationIssues) {
                std::cout << issue.code << ": " << issue.message << '\n';
            }
        }
    }

    std::vector<aimp::PreflightIssue> preflightIssues;
    if (emitPreflight || failOnPreflight || failOnQualityGate || !preflightOutPath.empty()) {
        preflightIssues = aimp::ValidatePrepressReadiness(plan, pdfOptions);
    }

    if (!preflightOutPath.empty()) {
        std::ofstream file(preflightOutPath);
        if (!file) {
            std::cerr << "Could not open preflight output file: " << preflightOutPath << '\n';
            return 1;
        }
        file << BuildPreflightJson(preflightIssues);
    }

    if (emitPreflight) {
        std::cout << "\n# Preflight";
        if (pdfOptions.targetPdfxProfile != aimp::PdfxProfile::None) {
            std::cout << " (" << aimp::PdfxProfileName(pdfOptions.targetPdfxProfile) << ")";
        }
        std::cout << "\n";
        if (preflightIssues.empty()) {
            std::cout << "OK\n";
        } else {
            for (const auto& issue : preflightIssues) {
                std::cout << (issue.isError ? "ERROR " : "WARN ") << issue.code << ": " << issue.message << '\n';
            }
        }
    }

    if (!jobOutPath.empty()) {
        std::ofstream file(jobOutPath);
        if (!file) {
            std::cerr << "Could not open job report output file: " << jobOutPath << '\n';
            return 1;
        }
        file << BuildJobReportJson(mode,
                                   pages,
                                   outPath,
                                   auditOutPath,
                                   manifestOutPath,
                                   acrobatJsOutPath,
                                   compositionOutPath,
                                   pdfOutPath,
                                   preflightOutPath,
                                   plan,
                                   pdfOptions,
                                   validationIssues,
                                   preflightIssues);
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

    if (failOnValidation && !validationIssues.empty()) {
        std::cerr << "Validation failed with " << validationIssues.size() << " issue(s).\n";
        return 2;
    }
    if (failOnPreflight) {
        std::size_t errorCount = 0;
        for (const auto& issue : preflightIssues) {
            if (issue.isError) {
                ++errorCount;
            }
        }
        if (errorCount > 0) {
            std::cerr << "Preflight failed with " << errorCount << " error(s).\n";
            return 3;
        }
    }
    if (failOnQualityGate) {
        std::size_t preflightErrorCount = 0;
        for (const auto& issue : preflightIssues) {
            if (issue.isError) {
                ++preflightErrorCount;
            }
        }
        if (!validationIssues.empty() || preflightErrorCount > 0) {
            std::cerr << "Combined quality gate failed (validation/preflight).\n";
            return 4;
        }
    }

    return 0;
}
