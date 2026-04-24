#include "aimp/ImpositionPlan.h"
#include "aimp/PdfComposer.h"
#include "aimp/Preset.h"
#include "aimp/AdjustPages.h"
#include "aimp/Annotations.h"
#include "aimp/Bleed.h"
#include "aimp/ImpositionInfo.h"
#include "aimp/PageTools.h"
#include "aimp/PdfX.h"
#include "aimp/PrinterMarks.h"
#include "aimp/SampleDocument.h"
#include "aimp/Shuffle.h"
#include "aimp/SplitMerge.h"
#include "aimp/StickOn.h"
#include "aimp/TilePages.h"
#include "aimp/TrimShift.h"
#include "aimp/VariableData.h"
#include "EscapeUtils.h"

#include <cstdint>
#include <cstdlib>
#include <charconv>
#include <chrono>
#include <ctime>
#include <set>
#include <thread>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>
#if defined(_WIN32)
#include <process.h>
#else
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace {

using aimp::internal::EscapeJson;

// Alias kept for all existing call sites in this file.
inline std::string EscapeJsonString(const std::string& s) { return EscapeJson(s); }

std::string BuildJobReportJson(const std::string& mode,
                               std::uint32_t pages,
                               const std::string& outputPlanPath,
                               const std::string& outputAuditPath,
                               const std::string& outputManifestPath,
                               const std::string& outputAcrobatJsPath,
                               const std::string& outputSdkOpsPath,
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
    out << "    \"acrobatSdkOpsJson\": \"" << EscapeJsonString(outputSdkOpsPath) << "\",\n";
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
        << "  imposr_cli two-up --pages <N> --sheet-width <pt> --sheet-height <pt>\n"
        << "  imposr_cli n-up --pages <N> --sheet-width <pt> --sheet-height <pt> --columns <N> --rows <N>\n"
        << "  imposr_cli booklet --pages <N> --sheet-width <pt> --sheet-height <pt> [--signature-size <N>]\n"
        << "  imposr_cli step-repeat --pages <N> --sheet-width <pt> --sheet-height <pt> --repeat-x <N> --repeat-y <N> --step-x <pt> --step-y <pt> --slot-width <pt> --slot-height <pt> [--repeat-rotation <degrees>]\n"
        << "  imposr_cli tile --pages <N> --sheet-width <pt> --sheet-height <pt> --columns <N> --rows <N> [--tile-overlap <pt>]\n"
        << "  imposr_cli manual --pages <N> --sheet-width <pt> --sheet-height <pt> --columns <N> --rows <N> --manual-sequence <csv>\n"
        << "  imposr_cli trim-shift --pages <N> --sheet-width <pt> --sheet-height <pt> --creep <pt> [--signature-size <N>]\n"
        << "  imposr_cli adjust-pages --pages <N> --sheet-width <pt> --sheet-height <pt> --adjust-mode scale|crop|extend|scale-to-fit|scale-to-fill [--scale-x <f>] [--scale-y <f>] [--target-width <pt>] [--target-height <pt>]\n"
        << "  imposr_cli insert-blank --pages <N> --sheet-width <pt> --sheet-height <pt> --insert-at <N> [--insert-count <N>]\n"
        << "  imposr_cli insert-file --pages <N> --sheet-width <pt> --sheet-height <pt> --insert-at <N> --insert-count <N> --insert-doc <id>\n"
        << "  imposr_cli insert-conditional --pages <N> --sheet-width <pt> --sheet-height <pt> [--pad-multiple N] [--insert-at <N>] [--filter even|odd]\n"
        << "  imposr_cli watch-dir --watch-dir <path> --pages <N> --sheet-width <pt> --sheet-height <pt> [--watch-interval <sec>] [--columns <N> --rows <N>]\n"
        << "  imposr_cli sample-doc --pages <N> --pdf-out <file> [--sample-page-width <pt>] [--sample-page-height <pt>] [--sample-diagonals 0|1]\n"
        << "  imposr_cli imposition-info --pages <N> --sheet-width <pt> --sheet-height <pt> [--module-out <file>]\n"
        << "  imposr_cli shuffle-assistant --pages <N> [--assistant-sig-size <N>] [--module-out <file>]\n"
        << "  imposr_cli peel-off-remove --pages <N> --sheet-width <pt> --sheet-height <pt> [--peel-all 1] [--peel-text 1] [--peel-bates 1] [--peel-page-number 1] [--peel-tape 1]\n"
        << "  imposr_cli stick-text --pages <N> --sheet-width <pt> --sheet-height <pt> --text <str> [--anchor topleft|topcenter|topright|middleleft|middlecenter|middleright|bottomleft|bottomcenter|bottomright] [--font-size <pt>] [--opacity <0-1>]\n"
        << "  imposr_cli stick-fields --pages <N> --sheet-width <pt> --sheet-height <pt> --variable-csv <file> --overlay-template <str>\n"
        << "  imposr_cli stick-bates --pages <N> --sheet-width <pt> --sheet-height <pt> [--bates-prefix <str>] [--bates-start <N>] [--bates-pad <N>]\n"
        << "  imposr_cli stick-pdf --pages <N> --sheet-width <pt> --sheet-height <pt> --source-pdf <path> [--source-pdf-page <N>]\n"
        << "  imposr_cli stick-tape --pages <N> --sheet-width <pt> --sheet-height <pt> [--stick-rect-x <pt>] [--stick-rect-y <pt>] [--stick-rect-w <pt>] [--stick-rect-h <pt>]\n"
        << "  imposr_cli peel-off --pages <N> --sheet-width <pt> --sheet-height <pt> [--stick-rect-x <pt>] [--stick-rect-y <pt>] [--stick-rect-w <pt>] [--stick-rect-h <pt>]\n"
        << "  imposr_cli variable-data --pages <N> --sheet-width <pt> --sheet-height <pt> --variable-csv <file> [--overlay-template <str>]\n"
        << "  imposr_cli batch --batch-csv <jobs.csv> --output-dir <dir> [--batch-report-out <file.json>] [--batch-stop-on-error 0|1]\n"
        << "  imposr_cli preset list [--preset-dir <dir>]\n"
        << "  imposr_cli preset apply --load-preset <file> --pages <N> --sheet-width <pt> --sheet-height <pt>\n"
        << "  imposr_cli sequence list [--sequence-dir <dir>]\n"
        << "  imposr_cli sequence run --sequence-file <file> [--output-dir <dir>]\n"
        << "Common options: [--load-preset <file>] [--save-preset <file>] [--page-sequence <csv>] [--reverse 0|1] [--filter all|even|odd] [--pad-multiple N] [--signature-size N] [--creep <pt>] [--fit-to-slot 0|1] [--rotate-to-fit 0|1] [--source-page-width <pt>] [--source-page-height <pt>] [--audit-out <file.xml>] [--manifest-out <file.json>] [--pdf-out <file.pdf>] [--output-dir <dir>] [--output-stem <name>] [--pdfx-profile none|pdfx-1a|pdfx-4] [--preflight 0|1] [--fail-on-quality-gate 0|1] [--summary 0|1]\n";
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

aimp::StickOnContext BuildStickOnContext(const aimp::ImpositionPlan& plan,
                                         const std::string& filename = "") {
    const auto now = std::chrono::system_clock::now();
    const std::time_t nowTime = std::chrono::system_clock::to_time_t(now);
    std::tm local {};
#if defined(_WIN32)
    localtime_s(&local, &nowTime);
#else
    localtime_r(&nowTime, &local);
#endif
    std::ostringstream dateBuf, dtBuf;
    dateBuf << std::put_time(&local, "%Y-%m-%d");
    dtBuf   << std::put_time(&local, "%Y-%m-%d %H:%M:%S");

    const auto stats = aimp::ComputePlanStatistics(plan);
    aimp::StickOnContext ctx {};
    ctx.filename          = filename;
    ctx.date              = dateBuf.str();
    ctx.datetime          = dtBuf.str();
    ctx.totalPageCount    = plan.sourcePageCount;
    ctx.totalSheetCount   = stats.sheetCount;
    return ctx;
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
                                 std::uint32_t failedJobs,
                                 const std::vector<std::string>& jobEntries) {
    std::ostringstream out;
    out << "{\n";
    out << "  \"type\": \"imposr-batch-report\",\n";
    out << "  \"totalJobs\": " << totalJobs << ",\n";
    out << "  \"successJobs\": " << successJobs << ",\n";
    out << "  \"failedJobs\": " << failedJobs << ",\n";
    out << "  \"status\": \"" << (failedJobs == 0 ? "ok" : "failed") << "\",\n";
    out << "  \"jobs\": [\n";
    for (std::size_t i = 0; i < jobEntries.size(); ++i) {
        out << jobEntries[i];
        if (i + 1 < jobEntries.size()) {
            out << ",";
        }
        out << "\n";
    }
    out << "  ]\n";
    out << "}\n";
    return out.str();
}

std::string QuoteShellArg(const std::string& value) {
    std::string out = "\"";
    for (char ch : value) {
        if (ch == '\\' || ch == '"') {
            out.push_back('\\');
        }
        out.push_back(ch);
    }
    out.push_back('"');
    return out;
}

std::string RenderCommandForLogs(const std::vector<std::string>& args) {
    std::ostringstream out;
    for (std::size_t i = 0; i < args.size(); ++i) {
        if (i > 0) {
            out << ' ';
        }
        out << QuoteShellArg(args[i]);
    }
    return out.str();
}

int RunProcess(const std::vector<std::string>& args, std::string& renderedCommand) {
    renderedCommand = RenderCommandForLogs(args);
    if (args.empty()) {
        return 1;
    }

#if defined(_WIN32)
    std::vector<const char*> argv;
    argv.reserve(args.size() + 1);
    for (const auto& arg : args) {
        argv.push_back(arg.c_str());
    }
    argv.push_back(nullptr);

    const int rc = _spawnv(_P_WAIT, args[0].c_str(), argv.data());
    if (rc < 0) {
        return 1;
    }
    return rc;
#else
    std::vector<char*> argv;
    argv.reserve(args.size() + 1);
    for (const auto& arg : args) {
        argv.push_back(const_cast<char*>(arg.c_str()));
    }
    argv.push_back(nullptr);

    const pid_t pid = fork();
    if (pid < 0) {
        return 1;
    }
    if (pid == 0) {
        execvp(argv[0], argv.data());
        _exit(127);
    }

    int status = 0;
    if (waitpid(pid, &status, 0) < 0) {
        return 1;
    }
    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }
    if (WIFSIGNALED(status)) {
        return 128 + WTERMSIG(status);
    }
    return 1;
#endif
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
    std::vector<std::string> jobReportEntries;
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
        const std::string signatureSize = get("signature_size");
        const std::string manualSequenceCsv = get("manual_sequence");
        const std::string pageSequenceCsv = get("page_sequence");
        const std::string reverseOrder = get("reverse");
        const std::string pageFilter = get("filter");
        const std::string padMultiple = get("pad_multiple");
        const std::string creep = get("creep");
        const std::string fitToSlot = get("fit_to_slot");
        const std::string rotateToFit = get("rotate_to_fit");
        const std::string sourcePageWidth = get("source_page_width");
        const std::string sourcePageHeight = get("source_page_height");
        const std::string overlayTemplate = get("pdf_overlay_template");
        const std::string variableCsv = get("pdf_variable_csv");
        const std::string preflight = get("preflight", "1");
        const std::string summary = get("summary");
        const std::string validate = get("validate");
        const std::string failOnValidation = get("fail_on_validation");
        const std::string failOnPreflight = get("fail_on_preflight");
        const std::filesystem::path outputDirPath(outputDir);
        const std::string jobOutPath = (outputDirPath / (outputStem + ".job.json")).string();

        std::vector<std::string> cmdArgs = {
            executablePath,
            mode,
            "--pages", pages,
            "--sheet-width", sheetWidth,
            "--sheet-height", sheetHeight,
            "--output-dir", outputDir,
            "--output-stem", outputStem,
            "--pdfx-profile", pdfxProfile,
            "--pdf-trim-marks", trimMarks,
            "--pdf-bleed-box", bleedBox,
            "--pdf-bleed", bleed,
            "--preflight", preflight,
            "--job-out", jobOutPath
        };

        if (!columns.empty()) {
            cmdArgs.push_back("--columns");
            cmdArgs.push_back(columns);
        }
        if (!rows.empty()) {
            cmdArgs.push_back("--rows");
            cmdArgs.push_back(rows);
        }
        if (!signatureSize.empty()) {
            cmdArgs.push_back("--signature-size");
            cmdArgs.push_back(signatureSize);
        }
        if (!manualSequenceCsv.empty()) {
            cmdArgs.push_back("--manual-sequence");
            cmdArgs.push_back(manualSequenceCsv);
        }
        if (!pageSequenceCsv.empty()) {
            cmdArgs.push_back("--page-sequence");
            cmdArgs.push_back(pageSequenceCsv);
        }
        if (!reverseOrder.empty()) {
            cmdArgs.push_back("--reverse");
            cmdArgs.push_back(reverseOrder);
        }
        if (!pageFilter.empty()) {
            cmdArgs.push_back("--filter");
            cmdArgs.push_back(pageFilter);
        }
        if (!padMultiple.empty()) {
            cmdArgs.push_back("--pad-multiple");
            cmdArgs.push_back(padMultiple);
        }
        if (!creep.empty()) {
            cmdArgs.push_back("--creep");
            cmdArgs.push_back(creep);
        }
        if (!fitToSlot.empty()) {
            cmdArgs.push_back("--fit-to-slot");
            cmdArgs.push_back(fitToSlot);
        }
        if (!rotateToFit.empty()) {
            cmdArgs.push_back("--rotate-to-fit");
            cmdArgs.push_back(rotateToFit);
        }
        if (!sourcePageWidth.empty()) {
            cmdArgs.push_back("--source-page-width");
            cmdArgs.push_back(sourcePageWidth);
        }
        if (!sourcePageHeight.empty()) {
            cmdArgs.push_back("--source-page-height");
            cmdArgs.push_back(sourcePageHeight);
        }
        if (!overlayTemplate.empty()) {
            cmdArgs.push_back("--pdf-overlay-template");
            cmdArgs.push_back(overlayTemplate);
        }
        if (!variableCsv.empty()) {
            cmdArgs.push_back("--pdf-variable-csv");
            cmdArgs.push_back(variableCsv);
        }
        if (!summary.empty()) {
            cmdArgs.push_back("--summary");
            cmdArgs.push_back(summary);
        }
        if (!validate.empty()) {
            cmdArgs.push_back("--validate");
            cmdArgs.push_back(validate);
        }
        if (!failOnValidation.empty()) {
            cmdArgs.push_back("--fail-on-validation");
            cmdArgs.push_back(failOnValidation);
        }
        if (!failOnPreflight.empty()) {
            cmdArgs.push_back("--fail-on-preflight");
            cmdArgs.push_back(failOnPreflight);
        }
        if (failOnQualityGate) {
            cmdArgs.push_back("--fail-on-quality-gate");
            cmdArgs.push_back("1");
        }

        ++totalJobs;
        std::string commandLine;
        const int rc = RunProcess(cmdArgs, commandLine);
        if (rc == 0) {
            ++successJobs;
        } else {
            ++failedJobs;
            std::cerr << "Batch job failed for output_stem=" << outputStem << " (exit=" << rc << ")\n";
            if (stopOnError) {
                break;
            }
        }
        std::ostringstream jobEntry;
        jobEntry << "    {\n";
        jobEntry << "      \"line\": " << totalJobs << ",\n";
        jobEntry << "      \"mode\": \"" << EscapeJsonString(mode) << "\",\n";
        jobEntry << "      \"outputStem\": \"" << EscapeJsonString(outputStem) << "\",\n";
        jobEntry << "      \"jobReport\": \"" << EscapeJsonString(jobOutPath) << "\",\n";
        jobEntry << "      \"exitCode\": " << rc << ",\n";
        jobEntry << "      \"status\": \"" << (rc == 0 ? "ok" : "failed") << "\",\n";
        jobEntry << "      \"command\": \"" << EscapeJsonString(commandLine) << "\"\n";
        jobEntry << "    }";
        jobReportEntries.push_back(jobEntry.str());
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
        out << BuildBatchReportJson(totalJobs, successJobs, failedJobs, jobReportEntries);
    }

    return failedJobs == 0 ? 0 : 5;
}

aimp::StickOnAnchor ParseAnchor(const std::string& s) {
    if (s == "topleft")      return aimp::StickOnAnchor::TopLeft;
    if (s == "topcenter")    return aimp::StickOnAnchor::TopCenter;
    if (s == "topright")     return aimp::StickOnAnchor::TopRight;
    if (s == "middleleft")   return aimp::StickOnAnchor::MiddleLeft;
    if (s == "middlecenter") return aimp::StickOnAnchor::MiddleCenter;
    if (s == "middleright")  return aimp::StickOnAnchor::MiddleRight;
    if (s == "bottomleft")   return aimp::StickOnAnchor::BottomLeft;
    if (s == "bottomcenter") return aimp::StickOnAnchor::BottomCenter;
    if (s == "bottomright")  return aimp::StickOnAnchor::BottomRight;
    return aimp::StickOnAnchor::BottomLeft;
}

bool ParseApplyToPages(const std::string& spec,
                       std::uint32_t pageCount,
                       bool& applyToAll,
                       std::vector<std::uint32_t>& indices) {
    if (spec.empty() || spec == "all") {
        applyToAll = true;
        return true;
    }
    applyToAll = false;
    std::stringstream ss(spec);
    std::string token;
    while (std::getline(ss, token, ',')) {
        std::uint32_t idx = 0;
        if (!ParseUInt(token, idx) || idx >= pageCount) {
            return false;
        }
        indices.push_back(idx);
    }
    return !indices.empty();
}

int RunPresetList(const std::string& presetDir) {
    const std::filesystem::path dir = presetDir.empty() ? "." : presetDir;
    std::error_code ec;
    std::ostringstream out;
    out << "{\n  \"kind\": \"preset-list\",\n  \"directory\": \"" << EscapeJsonString(dir.string()) << "\",\n  \"presets\": [\n";
    bool first = true;
    for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
        if (ec) break;
        if (entry.path().extension() == ".json") {
            if (!first) out << ",\n";
            out << "    \"" << EscapeJsonString(entry.path().string()) << "\"";
            first = false;
        }
    }
    out << "\n  ]\n}\n";
    std::cout << out.str();
    return 0;
}

int RunSequenceList(const std::string& sequenceDir) {
    const std::filesystem::path dir = sequenceDir.empty() ? "." : sequenceDir;
    std::error_code ec;
    std::ostringstream out;
    out << "{\n  \"kind\": \"sequence-list\",\n  \"directory\": \"" << EscapeJsonString(dir.string()) << "\",\n  \"sequences\": [\n";
    bool first = true;
    for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
        if (ec) break;
        const auto ext = entry.path().extension();
        if (ext == ".json" || ext == ".csv") {
            if (!first) out << ",\n";
            out << "    \"" << EscapeJsonString(entry.path().string()) << "\"";
            first = false;
        }
    }
    out << "\n  ]\n}\n";
    std::cout << out.str();
    return 0;
}

// Runs a sequence file (JSON array of arg-arrays, or one CSV row per job).
// Format: each line is a comma-separated list of CLI arguments excluding the
// executable name. Empty lines and lines beginning with '#' are skipped.
int RunSequenceFile(const std::string& cliPath,
                    const std::string& sequenceFile,
                    const std::string& outputDir) {
    if (sequenceFile.empty()) {
        std::cerr << "sequence run requires --sequence-file\n";
        return 1;
    }
    std::ifstream in(sequenceFile);
    if (!in) {
        std::cerr << "Could not open --sequence-file: " << sequenceFile << '\n';
        return 1;
    }

    std::string executablePath = TrimAscii(cliPath);
    if (executablePath.size() >= 2 && executablePath.front() == '"' && executablePath.back() == '"') {
        executablePath = executablePath.substr(1, executablePath.size() - 2);
    }

    std::uint32_t total = 0, succeeded = 0, failed = 0;
    std::vector<std::string> entries;
    std::string line;
    while (std::getline(in, line)) {
        line = TrimAscii(line);
        if (line.empty() || line[0] == '#') continue;
        const auto fields = ParseCsvLine(line);
        std::vector<std::string> cmdArgs = {executablePath};
        for (const auto& f : fields) {
            const std::string t = TrimAscii(f);
            if (!t.empty()) cmdArgs.push_back(t);
        }
        if (!outputDir.empty()) {
            cmdArgs.push_back("--output-dir");
            cmdArgs.push_back(outputDir);
        }
        ++total;
        std::string cmd;
        const int rc = RunProcess(cmdArgs, cmd);
        if (rc == 0) {
            ++succeeded;
        } else {
            ++failed;
            std::cerr << "Sequence step " << total << " failed (exit=" << rc << "): " << cmd << '\n';
        }
        std::ostringstream e;
        e << "    {\"step\": " << total << ", \"exitCode\": " << rc << ", \"status\": \""
          << (rc == 0 ? "ok" : "failed") << "\", \"command\": \"" << EscapeJsonString(cmd) << "\"}";
        entries.push_back(e.str());
    }

    std::cout << BuildBatchReportJson(total, succeeded, failed, entries);
    return failed == 0 ? 0 : 5;
}

// Write a string to a file, creating parent directories as needed.
bool WriteFile(const std::string& path, const std::string& content) {
    if (path.empty()) return true;
    std::error_code ec;
    const auto parent = std::filesystem::path(path).parent_path();
    if (!parent.empty()) {
        std::filesystem::create_directories(parent, ec);
        if (ec) return false;
    }
    std::ofstream f(path);
    if (!f) return false;
    f << content;
    return true;
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        PrintUsage();
        return 1;
    }

    const std::string mode = argv[1];

    // Sub-command for compound modes (preset / sequence).
    std::string subMode;
    if ((mode == "preset" || mode == "sequence") && argc >= 3) {
        subMode = argv[2];
    }

    std::uint32_t pages = 0;
    std::uint32_t columns = 0;
    std::uint32_t rows = 0;
    std::uint32_t repeatX = 0;
    std::uint32_t repeatY = 0;
    double repeatRotation = 0.0;
    double sheetWidth = 0.0;
    double sheetHeight = 0.0;
    double stepX = 0.0;
    double stepY = 0.0;
    std::string watchDir;
    double watchIntervalSec = 2.0;
    double slotWidth = 0.0;
    double slotHeight = 0.0;
    double tileOverlap = 0.0;
    std::string outPath;
    std::string auditOutPath;
    std::string manifestOutPath;
    std::string acrobatJsOutPath;
    std::string sdkOpsOutPath;
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

    // New module flags.
    std::string adjustMode;
    double scaleX = 1.0, scaleY = 1.0;
    double targetWidth = 0.0, targetHeight = 0.0;
    double cropRectX = 0.0, cropRectY = 0.0, cropRectW = 0.0, cropRectH = 0.0;
    double extendTop = 0.0, extendBottom = 0.0, extendLeft = 0.0, extendRight = 0.0;
    std::uint32_t insertAt = 0;
    std::uint32_t insertCount = 1;
    std::string insertDoc;
    std::string stickText;
    std::string stickAnchorStr;
    double stickFontSize = 12.0;
    double stickOpacity = 1.0;
    double stickColorR = 0.0, stickColorG = 0.0, stickColorB = 0.0;
    double stickRectX = 0.0, stickRectY = 0.0, stickRectW = 200.0, stickRectH = 20.0;
    std::string stickApplyToPages;
    std::string batesPrefix;
    std::string batesSuffix;
    std::uint32_t batesStart = 1;
    std::uint32_t batesPadWidth = 4;
    std::string sourcePdf;
    std::uint32_t sourcePdfPage = 0;
    double trimShiftCreep = 0.0;
    std::string overlayTemplate;
    std::string variableCsvPath;
    std::string presetDir;
    std::string sequenceDir;
    std::string sequenceFile;
    // Sample document options.
    double samplePageWidth  = 595.28;
    double samplePageHeight = 841.89;
    bool   sampleDiagonals  = false;
    // Shuffle assistant option.
    std::uint32_t assistantSigSize = 4;
    // Peel-off removal flags.
    bool peelAll        = false;
    bool peelText       = false;
    bool peelBates      = false;
    bool peelPageNumber = false;
    bool peelPdfPage    = false;
    bool peelTape       = false;
    // Module-result output paths.
    std::string moduleOutPath;

    // First pass: collect --load-preset.
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
        repeatRotation = preset.repeatRotation;
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

    // Start arg index: skip subMode token for compound commands.
    const int argStart = (mode == "preset" || mode == "sequence") ? 3 : 2;

    for (int i = argStart; i < argc; ++i) {
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
            if (!ParseDouble(value, sheetWidth) || sheetWidth <= 0.0) {
                std::cerr << "Invalid value for --sheet-width (must be > 0)\n";
                return 1;
            }
        } else if (key == "--sheet-height") {
            if (!ParseDouble(value, sheetHeight) || sheetHeight <= 0.0) {
                std::cerr << "Invalid value for --sheet-height (must be > 0)\n";
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
        } else if (key == "--sdk-ops-out") {
            sdkOpsOutPath = value;
        } else if (key == "--composition-out") {
            compositionOutPath = value;
        } else if (key == "--job-out") {
            jobOutPath = value;
        } else if (key == "--load-preset") {
            continue; // already handled
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
        } else if (key == "--repeat-rotation") {
            if (!ParseDouble(value, repeatRotation)) {
                std::cerr << "Invalid value for --repeat-rotation\n";
                return 1;
            }
        } else if (key == "--watch-dir") {
            watchDir = value;
        } else if (key == "--watch-interval") {
            if (!ParseDouble(value, watchIntervalSec) || watchIntervalSec <= 0.0) {
                std::cerr << "Invalid value for --watch-interval (expected > 0)\n";
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
            trimShiftCreep = buildOptions.bookletCreepPerSheetPoints;
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
        // New module args.
        } else if (key == "--adjust-mode") {
            adjustMode = value;
        } else if (key == "--scale-x") {
            if (!ParseDouble(value, scaleX)) {
                std::cerr << "Invalid value for --scale-x\n";
                return 1;
            }
        } else if (key == "--scale-y") {
            if (!ParseDouble(value, scaleY)) {
                std::cerr << "Invalid value for --scale-y\n";
                return 1;
            }
        } else if (key == "--target-width") {
            if (!ParseDouble(value, targetWidth)) {
                std::cerr << "Invalid value for --target-width\n";
                return 1;
            }
        } else if (key == "--target-height") {
            if (!ParseDouble(value, targetHeight)) {
                std::cerr << "Invalid value for --target-height\n";
                return 1;
            }
        } else if (key == "--crop-x") {
            if (!ParseDouble(value, cropRectX)) {
                std::cerr << "Invalid value for --crop-x\n";
                return 1;
            }
        } else if (key == "--crop-y") {
            if (!ParseDouble(value, cropRectY)) {
                std::cerr << "Invalid value for --crop-y\n";
                return 1;
            }
        } else if (key == "--crop-w") {
            if (!ParseDouble(value, cropRectW)) {
                std::cerr << "Invalid value for --crop-w\n";
                return 1;
            }
        } else if (key == "--crop-h") {
            if (!ParseDouble(value, cropRectH)) {
                std::cerr << "Invalid value for --crop-h\n";
                return 1;
            }
        } else if (key == "--extend-top") {
            if (!ParseDouble(value, extendTop)) {
                std::cerr << "Invalid value for --extend-top\n";
                return 1;
            }
        } else if (key == "--extend-bottom") {
            if (!ParseDouble(value, extendBottom)) {
                std::cerr << "Invalid value for --extend-bottom\n";
                return 1;
            }
        } else if (key == "--extend-left") {
            if (!ParseDouble(value, extendLeft)) {
                std::cerr << "Invalid value for --extend-left\n";
                return 1;
            }
        } else if (key == "--extend-right") {
            if (!ParseDouble(value, extendRight)) {
                std::cerr << "Invalid value for --extend-right\n";
                return 1;
            }
        } else if (key == "--insert-at") {
            if (!ParseUInt(value, insertAt)) {
                std::cerr << "Invalid value for --insert-at\n";
                return 1;
            }
        } else if (key == "--insert-count") {
            if (!ParseUInt(value, insertCount) || insertCount == 0) {
                std::cerr << "Invalid value for --insert-count (must be >= 1)\n";
                return 1;
            }
        } else if (key == "--insert-doc") {
            insertDoc = value;
        } else if (key == "--text") {
            stickText = value;
        } else if (key == "--anchor") {
            stickAnchorStr = value;
        } else if (key == "--font-size") {
            if (!ParseDouble(value, stickFontSize) || stickFontSize <= 0.0) {
                std::cerr << "Invalid value for --font-size\n";
                return 1;
            }
        } else if (key == "--opacity") {
            if (!ParseDouble(value, stickOpacity) || stickOpacity < 0.0 || stickOpacity > 1.0) {
                std::cerr << "Invalid value for --opacity (expected 0.0 - 1.0)\n";
                return 1;
            }
        } else if (key == "--color-r") {
            if (!ParseDouble(value, stickColorR)) {
                std::cerr << "Invalid value for --color-r\n";
                return 1;
            }
        } else if (key == "--color-g") {
            if (!ParseDouble(value, stickColorG)) {
                std::cerr << "Invalid value for --color-g\n";
                return 1;
            }
        } else if (key == "--color-b") {
            if (!ParseDouble(value, stickColorB)) {
                std::cerr << "Invalid value for --color-b\n";
                return 1;
            }
        } else if (key == "--stick-rect-x") {
            if (!ParseDouble(value, stickRectX)) {
                std::cerr << "Invalid value for --stick-rect-x\n";
                return 1;
            }
        } else if (key == "--stick-rect-y") {
            if (!ParseDouble(value, stickRectY)) {
                std::cerr << "Invalid value for --stick-rect-y\n";
                return 1;
            }
        } else if (key == "--stick-rect-w") {
            if (!ParseDouble(value, stickRectW) || stickRectW <= 0.0) {
                std::cerr << "Invalid value for --stick-rect-w\n";
                return 1;
            }
        } else if (key == "--stick-rect-h") {
            if (!ParseDouble(value, stickRectH) || stickRectH <= 0.0) {
                std::cerr << "Invalid value for --stick-rect-h\n";
                return 1;
            }
        } else if (key == "--apply-to-pages") {
            stickApplyToPages = value;
        } else if (key == "--bates-prefix") {
            batesPrefix = value;
        } else if (key == "--bates-suffix") {
            batesSuffix = value;
        } else if (key == "--bates-start") {
            if (!ParseUInt(value, batesStart)) {
                std::cerr << "Invalid value for --bates-start\n";
                return 1;
            }
        } else if (key == "--bates-pad") {
            if (!ParseUInt(value, batesPadWidth)) {
                std::cerr << "Invalid value for --bates-pad\n";
                return 1;
            }
        } else if (key == "--source-pdf") {
            sourcePdf = value;
        } else if (key == "--source-pdf-page") {
            if (!ParseUInt(value, sourcePdfPage)) {
                std::cerr << "Invalid value for --source-pdf-page\n";
                return 1;
            }
        } else if (key == "--overlay-template") {
            overlayTemplate = value;
        } else if (key == "--variable-csv") {
            variableCsvPath = value;
        } else if (key == "--preset-dir") {
            presetDir = value;
        } else if (key == "--sequence-dir") {
            sequenceDir = value;
        } else if (key == "--sequence-file") {
            sequenceFile = value;
        } else if (key == "--module-out") {
            moduleOutPath = value;
        } else if (key == "--sample-page-width") {
            if (!ParseDouble(value, samplePageWidth) || samplePageWidth <= 0.0) {
                std::cerr << "Invalid value for --sample-page-width\n";
                return 1;
            }
        } else if (key == "--sample-page-height") {
            if (!ParseDouble(value, samplePageHeight) || samplePageHeight <= 0.0) {
                std::cerr << "Invalid value for --sample-page-height\n";
                return 1;
            }
        } else if (key == "--sample-diagonals") {
            std::uint32_t raw = 0;
            if (!ParseUInt(value, raw) || raw > 1) {
                std::cerr << "Invalid value for --sample-diagonals (0 or 1)\n";
                return 1;
            }
            sampleDiagonals = (raw == 1);
        } else if (key == "--assistant-sig-size") {
            if (!ParseUInt(value, assistantSigSize) || assistantSigSize < 4) {
                std::cerr << "Invalid value for --assistant-sig-size (min 4)\n";
                return 1;
            }
        } else if (key == "--peel-all") {
            std::uint32_t raw = 0;
            if (!ParseUInt(value, raw) || raw > 1) { std::cerr << "Invalid --peel-all\n"; return 1; }
            peelAll = (raw == 1);
        } else if (key == "--peel-text") {
            std::uint32_t raw = 0;
            if (!ParseUInt(value, raw) || raw > 1) { std::cerr << "Invalid --peel-text\n"; return 1; }
            peelText = (raw == 1);
        } else if (key == "--peel-bates") {
            std::uint32_t raw = 0;
            if (!ParseUInt(value, raw) || raw > 1) { std::cerr << "Invalid --peel-bates\n"; return 1; }
            peelBates = (raw == 1);
        } else if (key == "--peel-page-number") {
            std::uint32_t raw = 0;
            if (!ParseUInt(value, raw) || raw > 1) { std::cerr << "Invalid --peel-page-number\n"; return 1; }
            peelPageNumber = (raw == 1);
        } else if (key == "--peel-pdf-page") {
            std::uint32_t raw = 0;
            if (!ParseUInt(value, raw) || raw > 1) { std::cerr << "Invalid --peel-pdf-page\n"; return 1; }
            peelPdfPage = (raw == 1);
        } else if (key == "--peel-tape") {
            std::uint32_t raw = 0;
            if (!ParseUInt(value, raw) || raw > 1) { std::cerr << "Invalid --peel-tape\n"; return 1; }
            peelTape = (raw == 1);
        } else {
            std::cerr << "Unknown argument: " << key << '\n';
            return 1;
        }
    }

    // ─── Compound sub-commands ────────────────────────────────────────────────

    if (mode == "batch") {
        return RunBatchMode(argv[0], batchCsvPath, outputDir, batchReportOutPath, batchStopOnError, failOnQualityGate);
    }

    if (mode == "preset") {
        if (subMode == "list") {
            return RunPresetList(presetDir);
        }
        if (subMode == "apply") {
            // Fall through — handled by normal plan generation below with --load-preset.
            // Requires --pages / --sheet-width / --sheet-height from preset or flags.
        } else if (!subMode.empty()) {
            std::cerr << "Unknown preset sub-command: " << subMode << '\n';
            return 1;
        }
    }

    if (mode == "sequence") {
        if (subMode == "list") {
            return RunSequenceList(sequenceDir);
        }
        if (subMode == "run") {
            return RunSequenceFile(argv[0], sequenceFile, outputDir);
        }
        std::cerr << "Unknown sequence sub-command: " << subMode << '\n';
        return 1;
    }

    // ─── Page-sequence pre-processing ────────────────────────────────────────

    if (!pageSequenceCsv.empty()) {
        std::string parseError;
        if (!ParsePageSequenceCsv(pageSequenceCsv, pageSequence, parseError)) {
            std::cerr << parseError << '\n';
            return 1;
        }
        buildOptions.explicitPageSequence = pageSequence;
    }

    // ─── Output-dir scaffolding ───────────────────────────────────────────────

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
        if (outPath.empty())          outPath          = base.string() + ".plan.json";
        if (auditOutPath.empty())     auditOutPath     = base.string() + ".audit.xml";
        if (manifestOutPath.empty())  manifestOutPath  = base.string() + ".manifest.json";
        if (acrobatJsOutPath.empty()) acrobatJsOutPath = base.string() + ".acrobat-placement.js";
        if (sdkOpsOutPath.empty())    sdkOpsOutPath    = base.string() + ".sdk-ops.json";
        if (compositionOutPath.empty()) compositionOutPath = base.string() + ".production-composition.json";
        if (pdfOutPath.empty())       pdfOutPath       = base.string() + ".proof.pdf";
        if (preflightOutPath.empty()) preflightOutPath = base.string() + ".preflight.json";
        if (jobOutPath.empty())       jobOutPath       = base.string() + ".job.json";
        if (moduleOutPath.empty())    moduleOutPath    = base.string() + ".module.json";
    }

    // ─── Build the base plan ─────────────────────────────────────────────────

    const aimp::SheetSize sheet {sheetWidth, sheetHeight};
    aimp::ImpositionPlan plan {};

    // Helper: build a two-up plan as the default for all new module modes.
    const auto buildTwoUp = [&]() {
        plan = aimp::TwoUpPlanner::Build("cli-input", pages, sheet, buildOptions);
    };

    // Helper: build booklet.
    const auto buildBooklet = [&]() {
        plan = aimp::BookletPlanner::Build("cli-input", pages, sheet, buildOptions);
    };

    if (mode == "two-up" || mode == "preset") {
        plan = aimp::TwoUpPlanner::Build("cli-input", pages, sheet, buildOptions);
    } else if (mode == "n-up") {
        if (columns == 0 || rows == 0) {
            std::cerr << "n-up mode requires --columns and --rows\n";
            return 1;
        }
        plan = aimp::NUpPlanner::Build("cli-input", pages, sheet, columns, rows, buildOptions);
    } else if (mode == "booklet") {
        buildBooklet();
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
            aimp::Rect {0.0, 0.0, slotWidth, slotHeight},
            repeatRotation
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

    // ─── New module modes ─────────────────────────────────────────────────────

    } else if (mode == "trim-shift") {
        buildBooklet();
        aimp::TrimShiftConfig cfg {};
        cfg.creepPerSheetPoints = trimShiftCreep;
        cfg.totalSheetsInSignature = buildOptions.bookletSignatureSize;
        const auto result = aimp::ApplyCreepShiftToPlan(plan, cfg);
        const std::string json = aimp::TrimShiftConfigToJson(cfg, result);
        if (!WriteFile(moduleOutPath, json)) {
            std::cerr << "Could not write module output: " << moduleOutPath << '\n';
            return 1;
        }
        if (moduleOutPath.empty()) {
            std::cout << json;
        }

    } else if (mode == "adjust-pages") {
        buildTwoUp();
        aimp::AdjustSpec spec {};
        if (adjustMode.empty() || adjustMode == "scale") {
            spec.mode = aimp::AdjustMode::Scale;
            spec.scaleX = scaleX;
            spec.scaleY = scaleY;
        } else if (adjustMode == "crop") {
            spec.mode = aimp::AdjustMode::Crop;
            spec.cropRect = aimp::Rect {cropRectX, cropRectY, cropRectW, cropRectH};
        } else if (adjustMode == "extend") {
            spec.mode = aimp::AdjustMode::Extend;
            spec.extendTop = extendTop;
            spec.extendBottom = extendBottom;
            spec.extendLeft = extendLeft;
            spec.extendRight = extendRight;
        } else if (adjustMode == "scale-to-fit") {
            spec.mode = aimp::AdjustMode::ScaleToFit;
            spec.targetWidth = targetWidth;
            spec.targetHeight = targetHeight;
        } else if (adjustMode == "scale-to-fill") {
            spec.mode = aimp::AdjustMode::ScaleToFill;
            spec.targetWidth = targetWidth;
            spec.targetHeight = targetHeight;
        } else {
            std::cerr << "Unknown --adjust-mode: " << adjustMode << '\n';
            return 1;
        }
        spec.applyToAllPlacements = true;
        std::string adjustErr;
        const auto result = aimp::ApplyAdjustSpec(plan, spec);
        if (!WriteFile(moduleOutPath, aimp::AdjustResultToJson(result))) {
            std::cerr << "Could not write module output\n";
            return 1;
        }
        if (moduleOutPath.empty()) {
            std::cout << aimp::AdjustResultToJson(result);
        }

    } else if (mode == "insert-blank") {
        // Build a sequence with blank pages inserted, then build two-up.
        std::vector<std::uint32_t> seq;
        for (std::uint32_t i = 0; i < pages; ++i) seq.push_back(i);
        seq = aimp::InsertBlankPages(seq, insertAt, insertCount);
        buildOptions.explicitPageSequence = seq;
        plan = aimp::TwoUpPlanner::Build("cli-input",
                                          static_cast<std::uint32_t>(seq.size()),
                                          sheet, buildOptions);

    } else if (mode == "insert-file") {
        // Build a sequence; insert insertCount pages from insertDoc at insertAt.
        std::vector<std::uint32_t> seq;
        for (std::uint32_t i = 0; i < pages; ++i) seq.push_back(i);
        const std::string docId = insertDoc.empty() ? "inserted-file" : insertDoc;
        // Represent inserted pages as blank placeholders in the sequence so
        // the plan accounts for them; the actual content comes from docId at
        // render time (handled by the SDK plugin or PDF composer).
        for (std::uint32_t k = 0; k < insertCount; ++k) {
            seq.insert(seq.begin() + static_cast<std::ptrdiff_t>(insertAt + k),
                       aimp::kBlankPageIndex);
        }
        buildOptions.explicitPageSequence = seq;
        plan = aimp::TwoUpPlanner::Build("cli-input",
                                          static_cast<std::uint32_t>(seq.size()),
                                          sheet, buildOptions);
        // Emit a JSON note about the inserted document.
        std::ostringstream note;
        note << "{\"kind\":\"insert-file\",\"insertedDoc\":\"" << EscapeJsonString(docId)
             << "\",\"insertAt\":" << insertAt << ",\"insertCount\":" << insertCount << "}\n";
        if (!WriteFile(moduleOutPath, note.str())) {
            std::cerr << "Could not write module output\n";
            return 1;
        }
        if (moduleOutPath.empty()) {
            std::cout << note.str();
        }

    } else if (mode == "insert-conditional") {
        std::vector<std::uint32_t> result;
        if (buildOptions.padToMultiple > 0) {
            // Pad sequence to the nearest multiple of padToMultiple by appending blank pages.
            for (std::uint32_t i = 0; i < pages; ++i) result.push_back(i);
            const std::size_t target = ((result.size() + buildOptions.padToMultiple - 1)
                                        / buildOptions.padToMultiple) * buildOptions.padToMultiple;
            while (result.size() < target) result.push_back(aimp::kBlankPageIndex);
        } else {
            // Insert blank pages at the specified position when it matches the even/odd filter.
            for (std::uint32_t i = 0; i < pages; ++i) result.push_back(i);
            const bool insertAtEven = (buildOptions.filter != aimp::PageFilter::OddOnly);
            std::vector<std::uint32_t> padded;
            for (std::uint32_t i = 0; i < static_cast<std::uint32_t>(result.size()); ++i) {
                if ((i % 2 == 0) == insertAtEven && i == insertAt) {
                    for (std::uint32_t k = 0; k < insertCount; ++k) {
                        padded.push_back(aimp::kBlankPageIndex);
                    }
                }
                padded.push_back(result[i]);
            }
            result = std::move(padded);
        }
        buildOptions.explicitPageSequence = result;
        plan = aimp::TwoUpPlanner::Build("cli-input",
                                          static_cast<std::uint32_t>(result.size()),
                                          sheet, buildOptions);

    } else if (mode == "stick-text") {
        buildTwoUp();
        aimp::StickOnItem item {};
        item.type = aimp::StickOnType::Text;
        item.text = stickText;
        item.anchor = ParseAnchor(stickAnchorStr);
        item.fontSize = stickFontSize;
        item.opacity = stickOpacity;
        item.colorR = stickColorR;
        item.colorG = stickColorG;
        item.colorB = stickColorB;
        item.rect = aimp::Rect {stickRectX, stickRectY, stickRectW, stickRectH};
        bool applyAll = true;
        std::vector<std::uint32_t> targetPages;
        ParseApplyToPages(stickApplyToPages, pages, applyAll, targetPages);
        item.applyToAllPages = applyAll;
        item.applyToPageIndices = targetPages;
        {
            const auto ctx = BuildStickOnContext(plan, outputStem);
            const auto result = aimp::ResolveStickOnItems(plan, {item}, &ctx);
            const auto content = aimp::RenderStickOnPdfContent(result.ops, 0);
            const std::string json = aimp::StickOnResultToJson(result);
            if (!WriteFile(moduleOutPath, json)) {
                std::cerr << "Could not write module output\n";
                return 1;
            }
            if (moduleOutPath.empty()) {
                std::cout << json;
            }
        }

    } else if (mode == "stick-fields") {
        buildTwoUp();
        if (variableCsvPath.empty()) {
            std::cerr << "stick-fields requires --variable-csv\n";
            return 1;
        }
        aimp::VariableDataSet dataset {};
        std::string loadErr;
        if (!aimp::LoadVariableDataSet(variableCsvPath, dataset, loadErr)) {
            std::cerr << "Could not load --variable-csv: " << loadErr << '\n';
            return 1;
        }
        // Build one text stick-on item per CSV header using overlay template.
        std::vector<aimp::StickOnItem> items;
        for (const auto& header : dataset.headers) {
            aimp::StickOnItem item {};
            item.type = aimp::StickOnType::Text;
            item.text = overlayTemplate.empty() ? ("{{" + header + "}}") : overlayTemplate;
            item.anchor = ParseAnchor(stickAnchorStr);
            item.fontSize = stickFontSize;
            item.rect = aimp::Rect {stickRectX, stickRectY, stickRectW, stickRectH};
            item.applyToAllPages = true;
            items.push_back(item);
            break; // one item per plan is sufficient for the planning layer
        }
        const auto result = aimp::ResolveStickOnItems(plan, items);
        const std::string json = aimp::StickOnResultToJson(result);
        if (!WriteFile(moduleOutPath, json)) {
            std::cerr << "Could not write module output\n";
            return 1;
        }
        if (moduleOutPath.empty()) {
            std::cout << json;
        }

    } else if (mode == "stick-bates") {
        buildTwoUp();
        aimp::StickOnItem item {};
        item.type = aimp::StickOnType::BatesNumber;
        item.batesPrefix = batesPrefix;
        item.batesSuffix = batesSuffix;
        item.batesStart = batesStart;
        item.batesPadWidth = batesPadWidth;
        item.anchor = ParseAnchor(stickAnchorStr);
        item.rect = aimp::Rect {stickRectX, stickRectY, stickRectW, stickRectH};
        item.applyToAllPages = true;
        const auto result = aimp::ResolveStickOnItems(plan, {item});
        const std::string json = aimp::StickOnResultToJson(result);
        if (!WriteFile(moduleOutPath, json)) {
            std::cerr << "Could not write module output\n";
            return 1;
        }
        if (moduleOutPath.empty()) {
            std::cout << json;
        }

    } else if (mode == "stick-pdf") {
        buildTwoUp();
        if (sourcePdf.empty()) {
            std::cerr << "stick-pdf requires --source-pdf\n";
            return 1;
        }
        aimp::StickOnItem item {};
        item.type = aimp::StickOnType::PdfPage;
        item.sourcePdfPath = sourcePdf;
        item.sourcePdfPage = sourcePdfPage;
        item.anchor = ParseAnchor(stickAnchorStr);
        item.rect = aimp::Rect {stickRectX, stickRectY, stickRectW, stickRectH};
        item.applyToAllPages = true;
        const auto result = aimp::ResolveStickOnItems(plan, {item});
        const std::string json = aimp::StickOnResultToJson(result);
        if (!WriteFile(moduleOutPath, json)) {
            std::cerr << "Could not write module output\n";
            return 1;
        }
        if (moduleOutPath.empty()) {
            std::cout << json;
        }

    } else if (mode == "stick-tape") {
        buildTwoUp();
        aimp::StickOnItem item {};
        item.type = aimp::StickOnType::MaskingTape;
        item.anchor = ParseAnchor(stickAnchorStr);
        item.rect = aimp::Rect {stickRectX, stickRectY, stickRectW, stickRectH};
        item.applyToAllPages = true;
        const auto result = aimp::ResolveStickOnItems(plan, {item});
        const std::string json = aimp::StickOnResultToJson(result);
        if (!WriteFile(moduleOutPath, json)) {
            std::cerr << "Could not write module output\n";
            return 1;
        }
        if (moduleOutPath.empty()) {
            std::cout << json;
        }

    } else if (mode == "peel-off") {
        buildTwoUp();
        aimp::StickOnItem item {};
        item.type = aimp::StickOnType::PeelOff;
        item.anchor = ParseAnchor(stickAnchorStr);
        item.rect = aimp::Rect {stickRectX, stickRectY, stickRectW, stickRectH};
        item.applyToAllPages = true;
        const auto result = aimp::ResolveStickOnItems(plan, {item});
        const std::string json = aimp::StickOnResultToJson(result);
        if (!WriteFile(moduleOutPath, json)) {
            std::cerr << "Could not write module output\n";
            return 1;
        }
        if (moduleOutPath.empty()) {
            std::cout << json;
        }

    } else if (mode == "variable-data") {
        buildTwoUp();
        if (variableCsvPath.empty()) {
            std::cerr << "variable-data requires --variable-csv\n";
            return 1;
        }
        aimp::VariableDataSet dataset {};
        std::string loadErr;
        if (!aimp::LoadVariableDataSet(variableCsvPath, dataset, loadErr)) {
            std::cerr << "Could not load --variable-csv: " << loadErr << '\n';
            return 1;
        }
        const std::string tmpl = overlayTemplate.empty() ? "" : overlayTemplate;
        const auto mergeResults = aimp::MergeVariableData(dataset, tmpl, pages);
        const std::string datasetJson = aimp::VariableDataSetToJson(dataset);
        std::ostringstream mergeJson;
        mergeJson << "{\n  \"kind\": \"variable-data-merge\",\n";
        mergeJson << "  \"dataset\": " << datasetJson << ",\n";
        mergeJson << "  \"mergeResults\": [\n";
        for (std::size_t i = 0; i < mergeResults.size(); ++i) {
            const auto& r = mergeResults[i];
            mergeJson << "    {\"pageIndex\": " << r.pageIndex
                      << ", \"resolvedText\": \"" << EscapeJsonString(r.resolvedText) << "\"}";
            if (i + 1 < mergeResults.size()) mergeJson << ",";
            mergeJson << "\n";
        }
        mergeJson << "  ]\n}\n";
        if (!WriteFile(moduleOutPath, mergeJson.str())) {
            std::cerr << "Could not write module output\n";
            return 1;
        }
        if (moduleOutPath.empty()) {
            std::cout << mergeJson.str();
        }

    } else if (mode == "sample-doc") {
        if (pdfOutPath.empty()) {
            std::cerr << "sample-doc requires --pdf-out <file>\n";
            return 1;
        }
        if (pages == 0) pages = 8;
        aimp::SampleDocumentOptions sdOpts {};
        sdOpts.pageCount        = pages;
        sdOpts.pageWidthPoints  = (samplePageWidth  > 0.0) ? samplePageWidth  : sheetWidth;
        sdOpts.pageHeightPoints = (samplePageHeight > 0.0) ? samplePageHeight : sheetHeight;
        if (sdOpts.pageWidthPoints  <= 0.0) sdOpts.pageWidthPoints  = 595.28;
        if (sdOpts.pageHeightPoints <= 0.0) sdOpts.pageHeightPoints = 841.89;
        sdOpts.drawDiagonals = sampleDiagonals;
        std::string sdErr;
        if (!aimp::CreateSampleDocument(sdOpts, pdfOutPath, sdErr)) {
            std::cerr << "Could not create sample document: " << sdErr << '\n';
            return 1;
        }
        std::cout << "{\"kind\":\"sample-doc\",\"pages\":" << pages
                  << ",\"path\":\"" << EscapeJsonString(pdfOutPath) << "\"}\n";
        return 0;

    } else if (mode == "imposition-info") {
        // Build a plan from preset or two-up default, then emit imposition info.
        if (plan.placements.empty()) {
            buildTwoUp();
        }
        const auto info = aimp::BuildImpositionInfo(plan);
        const std::string json = aimp::ImpositionInfoToJson(info);
        if (!moduleOutPath.empty()) {
            if (!WriteFile(moduleOutPath, json)) {
                std::cerr << "Could not write imposition-info output\n";
                return 1;
            }
        } else {
            std::cout << json;
        }
        return 0;

    } else if (mode == "shuffle-assistant") {
        if (pages == 0) {
            std::cerr << "shuffle-assistant requires --pages N\n";
            return 1;
        }
        const auto result = aimp::RunShuffleAssistant(pages, assistantSigSize);
        const std::string json = aimp::ShuffleAssistantToJson(result);
        if (!moduleOutPath.empty()) {
            if (!WriteFile(moduleOutPath, json)) {
                std::cerr << "Could not write shuffle-assistant output\n";
                return 1;
            }
        } else {
            std::cout << json;
        }
        return 0;

    } else if (mode == "peel-off-remove") {
        // Remove previously-applied stick-on marks from a resolved ops JSON.
        // The ops JSON is read from --module-out (as source) or passed as stdin.
        // Here we demonstrate by building a plan, resolving all Text ops, then peeling text.
        buildTwoUp();
        aimp::StickOnItem item {};
        item.type = aimp::StickOnType::Text;
        item.text = "(applied-mark)";
        item.applyToAllPages = true;
        item.rect = aimp::Rect {stickRectX, stickRectY, stickRectW, stickRectH};
        const auto resolved = aimp::ResolveStickOnItems(plan, {item});

        aimp::PeelOffSpec spec {};
        spec.removeAll         = peelAll;
        spec.removeText        = peelText || peelAll;
        spec.removeBatesNumber = peelBates || peelAll;
        spec.removePageNumber  = peelPageNumber || peelAll;
        spec.removePdfPage     = peelPdfPage || peelAll;
        spec.removeMaskingTape = peelTape || peelAll;
        spec.removePeelOff     = peelAll;

        const auto remaining = aimp::PeelOffOps(resolved.ops, spec);
        // Emit summary JSON.
        std::ostringstream out;
        out << "{\"kind\":\"peel-off-remove\""
            << ",\"originalCount\":" << resolved.ops.size()
            << ",\"removedCount\":"  << (resolved.ops.size() - remaining.size())
            << ",\"remainingCount\":" << remaining.size()
            << "}\n";
        if (!moduleOutPath.empty()) {
            if (!WriteFile(moduleOutPath, out.str())) {
                std::cerr << "Could not write peel-off-remove output\n";
                return 1;
            }
        } else {
            std::cout << out.str();
        }
        return 0;

    } else if (mode == "watch-dir") {
        if (watchDir.empty()) {
            std::cerr << "watch-dir mode requires --watch-dir <path>\n";
            return 1;
        }
        if (pages == 0) {
            std::cerr << "watch-dir mode requires --pages N\n";
            return 1;
        }
        const std::filesystem::path watchPath(watchDir);
        std::error_code dirEc;
        if (!std::filesystem::is_directory(watchPath, dirEc)) {
            std::cerr << "watch-dir: not a directory: " << watchDir << '\n';
            return 1;
        }
        const int pollMs = static_cast<int>(watchIntervalSec * 1000.0);
        std::set<std::string> seen;
        std::cout << "Watching: " << watchDir
                  << " (interval " << watchIntervalSec << "s, --pages " << pages << ")\n";
        std::cout << "Press Ctrl+C to stop.\n" << std::flush;

        while (true) {
            std::error_code ec;
            for (const auto& entry : std::filesystem::directory_iterator(watchPath, ec)) {
                if (!entry.is_regular_file()) continue;
                const auto p = entry.path();
                const auto ext = p.extension().string();
                if (ext != ".pdf" && ext != ".PDF") continue;
                const std::string pathStr = p.string();
                if (seen.count(pathStr)) continue;
                seen.insert(pathStr);

                std::cout << "New file: " << pathStr << '\n' << std::flush;

                // Build plan using the configured mode and the provided --pages value.
                aimp::ImpositionPlan watchPlan;
                const aimp::SheetSize watchSheet {sheetWidth, sheetHeight};
                if (mode == "watch-dir") { // always true here; avoid unused-var warning
                    if (columns > 0 && rows > 0) {
                        watchPlan = aimp::NUpPlanner::Build("watch-input", pages,
                                                            watchSheet, columns, rows, buildOptions);
                    } else {
                        watchPlan = aimp::TwoUpPlanner::Build("watch-input", pages,
                                                              watchSheet, buildOptions);
                    }
                }
                const std::string planStr = aimp::ToJson(watchPlan);
                const auto outFile = p.parent_path() / (p.stem().string() + ".plan.json");
                if (!WriteFile(outFile.string(), planStr)) {
                    std::cerr << "  Could not write plan: " << outFile.string() << '\n';
                } else {
                    std::cout << "  Plan written: " << outFile.string() << '\n' << std::flush;
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(pollMs));
        }
        return 0; // unreachable; loop exits via signal

    } else {
        std::cerr << "Unknown mode: " << mode << '\n';
        PrintUsage();
        return 1;
    }

    // ─── Shared output pipeline ───────────────────────────────────────────────

    const std::string planJson = aimp::ToJson(plan);
    if (!outPath.empty()) {
        if (!WriteFile(outPath, planJson)) {
            std::cerr << "Could not write plan JSON: " << outPath << '\n';
            return 1;
        }
    } else if (mode == "two-up" || mode == "n-up" || mode == "booklet" ||
               mode == "step-repeat" || mode == "manual" || mode == "tile" ||
               mode == "preset" ||
               mode == "insert-blank" || mode == "insert-file" || mode == "insert-conditional") {
        std::cout << planJson;
    }

    if (!auditOutPath.empty()) {
        if (!WriteFile(auditOutPath, aimp::ToAuditXml(plan))) {
            std::cerr << "Could not write audit XML: " << auditOutPath << '\n';
            return 1;
        }
    }

    if (!manifestOutPath.empty()) {
        if (!WriteFile(manifestOutPath, aimp::ToPlacementManifestJson(plan))) {
            std::cerr << "Could not write manifest JSON: " << manifestOutPath << '\n';
            return 1;
        }
    }
    if (!acrobatJsOutPath.empty()) {
        if (!WriteFile(acrobatJsOutPath, aimp::ToAcrobatPlacementJs(plan))) {
            std::cerr << "Could not write Acrobat JS: " << acrobatJsOutPath << '\n';
            return 1;
        }
    }
    if (!sdkOpsOutPath.empty()) {
        if (!WriteFile(sdkOpsOutPath, aimp::ToAcrobatSdkOpsJson(plan))) {
            std::cerr << "Could not write SDK ops JSON: " << sdkOpsOutPath << '\n';
            return 1;
        }
    }
    if (!compositionOutPath.empty()) {
        if (!WriteFile(compositionOutPath, aimp::ToProductionCompositionJson(plan, buildOptions, pdfOptions))) {
            std::cerr << "Could not write production composition JSON: " << compositionOutPath << '\n';
            return 1;
        }
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
        preset.repeatRotation = repeatRotation;
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
        if (!WriteFile(preflightOutPath, aimp::ToPreflightJson(preflightIssues))) {
            std::cerr << "Could not write preflight JSON: " << preflightOutPath << '\n';
            return 1;
        }
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
        if (!WriteFile(jobOutPath, BuildJobReportJson(mode,
                                                      pages,
                                                      outPath,
                                                      auditOutPath,
                                                      manifestOutPath,
                                                      acrobatJsOutPath,
                                                      sdkOpsOutPath,
                                                      compositionOutPath,
                                                      pdfOutPath,
                                                      preflightOutPath,
                                                      plan,
                                                      pdfOptions,
                                                      validationIssues,
                                                      preflightIssues))) {
            std::cerr << "Could not write job report: " << jobOutPath << '\n';
            return 1;
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
