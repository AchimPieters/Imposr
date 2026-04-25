#include "aimp/ImpositionPlan.h"
#include "aimp/PanelState.h"
#include "aimp/PdfComposer.h"
#include "aimp/Preset.h"

#include <chrono>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

// Acrobat SDK headers.
// These must resolve through ACROBAT_SDK_DIR include paths.
#include "PIHeaders.h"

namespace {

ACCB1 void ACCB2 ExecuteTwoUpDemo(void* clientData);
ACCB1 void ACCB2 ExecuteTwoUpReportExport(void* clientData);
ACCB1 void ACCB2 ExecutePresetSave(void* clientData);
ACCB1 void ACCB2 ExecutePresetPreview(void* clientData);
ACCB1 void ACCB2 ExecutePresetRunBundle(void* clientData);
ACCB1 void ACCB2 ExecutePresetValidate(void* clientData);
ACCB1 void ACCB2 ExecutePresetQuickConfigure(void* clientData);
ACCB1 void ACCB2 ExecutePanelCycleLayout(void* clientData);
ACCB1 void ACCB2 ExecutePanelTogglePrepress(void* clientData);
ACCB1 void ACCB2 ExecutePanelSetOutputTemp(void* clientData);
ACCB1 void ACCB2 ExecutePanelShowState(void* clientData);
ACCB1 void ACCB2 ExecutePanelApplyState(void* clientData);
ACCB1 void ACCB2 ExecutePanelToggleQualityGate(void* clientData);
ACCB1 void ACCB2 ExecutePanelSheetA4(void* clientData);
ACCB1 void ACCB2 ExecutePanelSheetA3(void* clientData);
ACCB1 void ACCB2 ExecutePanelExportDialogPackage(void* clientData);
ACCB1 void ACCB2 ExecutePanelOpenUnifiedDialog(void* clientData);

AVMenuItem gPluginMenuItem = nullptr;
AVMenuItem gPluginReportMenuItem = nullptr;
AVMenuItem gPluginPresetSaveMenuItem = nullptr;
AVMenuItem gPluginPresetPreviewMenuItem = nullptr;
AVMenuItem gPluginPresetRunMenuItem = nullptr;
AVMenuItem gPluginPresetValidateMenuItem = nullptr;
AVMenuItem gPluginPresetQuickConfigMenuItem = nullptr;
AVMenuItem gPluginPanelCycleLayoutMenuItem = nullptr;
AVMenuItem gPluginPanelTogglePrepressMenuItem = nullptr;
AVMenuItem gPluginPanelSetOutputTempMenuItem = nullptr;
AVMenuItem gPluginPanelShowStateMenuItem = nullptr;
AVMenuItem gPluginPanelApplyStateMenuItem = nullptr;
AVMenuItem gPluginPanelToggleQualityGateMenuItem = nullptr;
AVMenuItem gPluginPanelSheetA4MenuItem = nullptr;
AVMenuItem gPluginPanelSheetA3MenuItem = nullptr;
AVMenuItem gPluginPanelExportDialogPackageMenuItem = nullptr;
AVMenuItem gPluginPanelOpenUnifiedDialogMenuItem = nullptr;
AVMenu gPluginSubMenu = nullptr;
// Declared as AVExecuteProc (the actual callback type) rather than ASCallback
// (void*) to avoid C++ hard type errors on the void* = func_ptr assignment.
// AVMenuItemNew and AVAppRegisterForPageViewClicks both take AVExecuteProc directly.
AVExecuteProc gMenuExecuteProc = nullptr;
AVExecuteProc gReportExecuteProc = nullptr;
AVExecuteProc gPresetSaveProc = nullptr;
AVExecuteProc gPresetPreviewProc = nullptr;
AVExecuteProc gPresetRunProc = nullptr;
AVExecuteProc gPresetValidateProc = nullptr;
AVExecuteProc gPresetQuickConfigProc = nullptr;
AVExecuteProc gPanelCycleLayoutProc = nullptr;
AVExecuteProc gPanelTogglePrepressProc = nullptr;
AVExecuteProc gPanelSetOutputTempProc = nullptr;
AVExecuteProc gPanelShowStateProc = nullptr;
AVExecuteProc gPanelApplyStateProc = nullptr;
AVExecuteProc gPanelToggleQualityGateProc = nullptr;
AVExecuteProc gPanelSheetA4Proc = nullptr;
AVExecuteProc gPanelSheetA3Proc = nullptr;
AVExecuteProc gPanelExportDialogPackageProc = nullptr;
AVExecuteProc gPanelOpenUnifiedDialogProc = nullptr;

constexpr const char* kExtensionName = "AcrobatImpositionPlugin";
constexpr const char* kPluginMenuTitle = "Acrobat Imposition Plugin";
constexpr const char* kMenuItemTitle = "2-Up Demo";
constexpr const char* kMenuItemReportTitle = "2-Up Report PDF";
constexpr const char* kMenuItemPresetSaveTitle = "Preset: Save default";
constexpr const char* kMenuItemPresetPreviewTitle = "Preset: Preview proof";
constexpr const char* kMenuItemPresetRunTitle = "Preset: Run bundle";
constexpr const char* kMenuItemPresetValidateTitle = "Preset: Validate active job";
constexpr const char* kMenuItemPresetQuickConfigTitle = "Preset: Quick configure";
constexpr const char* kMenuItemPanelCycleLayoutTitle = "Panel: Cycle layout";
constexpr const char* kMenuItemPanelTogglePrepressTitle = "Panel: Toggle trim+bleed";
constexpr const char* kMenuItemPanelSetOutputTempTitle = "Panel: Set output temp";
constexpr const char* kMenuItemPanelShowStateTitle = "Panel: Show state";
constexpr const char* kMenuItemPanelApplyStateTitle = "Panel: Apply state";
constexpr const char* kMenuItemPanelToggleQualityGateTitle = "Panel: Toggle quality gate";
constexpr const char* kMenuItemPanelSheetA4Title = "Panel: Sheet A4";
constexpr const char* kMenuItemPanelSheetA3Title = "Panel: Sheet A3";
constexpr const char* kMenuItemPanelExportDialogPackageTitle = "Panel: Export dialog package";
constexpr const char* kMenuItemPanelOpenUnifiedDialogTitle = "Panel: Open unified dialog";

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

std::string GetPresetPath() {
    std::error_code ec;
    const auto tempDir = std::filesystem::temp_directory_path(ec);
    if (ec) {
        return "acrobat-imposition-plugin.preset.txt";
    }
    return (tempDir / "acrobat-imposition-plugin.preset.txt").string();
}

bool BuildPlanFromPreset(const aimp::PlannerPreset& preset,
                         std::uint32_t pageCount,
                         aimp::ImpositionPlan& outPlan,
                         std::string& modeLabel) {
    const aimp::SheetSize sheet = preset.sheetSize.widthPoints > 0.0 && preset.sheetSize.heightPoints > 0.0
        ? preset.sheetSize
        : aimp::SheetSize {1190.55, 841.89};

    if (preset.columns >= 1 && preset.rows >= 1 && (preset.columns > 1 || preset.rows > 1)) {
        modeLabel = "n-up";
        outPlan = aimp::NUpPlanner::Build("active-document", pageCount, sheet, preset.columns, preset.rows, preset.buildOptions);
        return true;
    }

    modeLabel = "two-up";
    outPlan = aimp::TwoUpPlanner::Build("active-document", pageCount, sheet, preset.buildOptions);
    return true;
}

bool BuildDefaultPreset(aimp::PlannerPreset& preset) {
    preset = aimp::PlannerPreset {};
    preset.sheetSize = {1190.55, 841.89};
    preset.columns = 2;
    preset.rows = 1;
    preset.buildOptions.scaleToFit = true;
    preset.buildOptions.autoRotateToFit = true;
    preset.pdfOptions.drawTrimMarks = true;
    preset.pdfOptions.drawBleedBox = true;
    preset.pdfOptions.bleedPoints = 6.0;
    preset.pdfOptions.targetPdfxProfile = aimp::PdfxProfile::Pdfx4;
    preset.pdfOptions.failOnValidationIssues = true;
    preset.pdfOptions.failOnPreflightErrors = true;
    preset.outputStem = "acrobat-imposition-run";
    return true;
}

void ShowInfoDialog(const std::string& message) {
    AVAlertNote(message.c_str());
}

std::string BuildPanelStateJson(const std::string& modeLabel,
                                const aimp::PlannerPreset& preset,
                                std::size_t validationIssueCount,
                                std::size_t preflightIssueCount,
                                std::size_t preflightErrorCount,
                                const std::string& bundlePath,
                                const std::string& proofPath,
                                const std::string& imposedOutputPath) {
    std::ostringstream out;
    out << "{\n";
    out << "  \"kind\": \"acrobat-imposition-panel-state\",\n";
    out << "  \"mode\": \"" << modeLabel << "\",\n";
    out << "  \"sheet\": {\"widthPoints\": " << preset.sheetSize.widthPoints
        << ", \"heightPoints\": " << preset.sheetSize.heightPoints << "},\n";
    out << "  \"preset\": {\n";
    out << "    \"columns\": " << preset.columns << ",\n";
    out << "    \"rows\": " << preset.rows << ",\n";
    out << "    \"fitToSlot\": " << (preset.buildOptions.scaleToFit ? "true" : "false") << ",\n";
    out << "    \"autoRotateToFit\": " << (preset.buildOptions.autoRotateToFit ? "true" : "false") << ",\n";
    out << "    \"reverseOrder\": " << (preset.buildOptions.reverseOrder ? "true" : "false") << ",\n";
    out << "    \"filter\": \"" << aimp::PanelStateFilterName(preset.buildOptions.filter) << "\",\n";
    out << "    \"bookletCreepPerSheetPoints\": " << preset.buildOptions.bookletCreepPerSheetPoints << ",\n";
    out << "    \"outputDirectory\": \"" << preset.outputDirectory << "\",\n";
    out << "    \"outputStem\": \"" << preset.outputStem << "\",\n";
    out << "    \"drawTrimMarks\": " << (preset.pdfOptions.drawTrimMarks ? "true" : "false") << ",\n";
    out << "    \"trimMarkLengthPoints\": " << preset.pdfOptions.trimMarkLengthPoints << ",\n";
    out << "    \"trimMarkOffsetPoints\": " << preset.pdfOptions.trimMarkOffsetPoints << ",\n";
    out << "    \"drawBleedBox\": " << (preset.pdfOptions.drawBleedBox ? "true" : "false") << ",\n";
    out << "    \"bleedPoints\": " << preset.pdfOptions.bleedPoints << ",\n";
    out << "    \"pdfxProfile\": \"" << aimp::PdfxProfileName(preset.pdfOptions.targetPdfxProfile) << "\",\n";
    out << "    \"failOnValidationIssues\": " << (preset.pdfOptions.failOnValidationIssues ? "true" : "false") << ",\n";
    out << "    \"failOnPreflightErrors\": " << (preset.pdfOptions.failOnPreflightErrors ? "true" : "false") << "\n";
    out << "  },\n";
    out << "  \"validation\": {\n";
    out << "    \"validationIssueCount\": " << validationIssueCount << ",\n";
    out << "    \"preflightIssueCount\": " << preflightIssueCount << ",\n";
    out << "    \"preflightErrorCount\": " << preflightErrorCount << ",\n";
    out << "    \"status\": \"" << ((validationIssueCount == 0 && preflightErrorCount == 0) ? "ready" : "blocked") << "\"\n";
    out << "  },\n";
    out << "  \"bundlePath\": \"" << bundlePath << "\",\n";
    out << "  \"outputs\": {\n";
    out << "    \"proofPdf\": \"" << proofPath << "\",\n";
    out << "    \"imposedOutputPdf\": \"" << imposedOutputPath << "\"\n";
    out << "  },\n";
    out << "  \"quickActions\": {\n";
    out << "    \"cycleLayout\": \"Panel: Cycle layout\",\n";
    out << "    \"toggleTrimBleed\": \"Panel: Toggle trim+bleed\",\n";
    out << "    \"toggleQualityGate\": \"Panel: Toggle quality gate\",\n";
    out << "    \"sheetA4\": \"Panel: Sheet A4\",\n";
    out << "    \"sheetA3\": \"Panel: Sheet A3\",\n";
    out << "    \"setOutputTemp\": \"Panel: Set output temp\",\n";
    out << "    \"showState\": \"Panel: Show state\",\n";
    out << "    \"applyState\": \"Panel: Apply state\"\n";
    out << "  }\n";
    out << "}\n";
    return out.str();
}

std::string GetPanelStatePath() {
    std::error_code ec;
    const auto tempDir = std::filesystem::temp_directory_path(ec);
    if (ec) {
        return "acrobat-imposition-panel-state.json";
    }
    return (tempDir / "acrobat-imposition-panel-state.json").string();
}

bool SavePanelState(const std::string& modeLabel,
                    const aimp::PlannerPreset& preset,
                    std::size_t validationIssueCount,
                    std::size_t preflightIssueCount,
                    std::size_t preflightErrorCount,
                    const std::string& bundlePath,
                    const std::string& proofPath,
                    const std::string& imposedOutputPath,
                    std::string& outPath) {
    outPath = GetPanelStatePath();
    std::ofstream out(outPath);
    if (!out) {
        return false;
    }
    out << BuildPanelStateJson(modeLabel,
                               preset,
                               validationIssueCount,
                               preflightIssueCount,
                               preflightErrorCount,
                               bundlePath,
                               proofPath,
                               imposedOutputPath);
    return true;
}

std::string BuildPanelDialogSchemaJson() {
    std::ostringstream out;
    out << "{\n";
    out << "  \"kind\": \"acrobat-imposition-panel-dialog-schema\",\n";
    out << "  \"sections\": [\n";
    out << "    {\n";
    out << "      \"id\": \"layout\",\n";
    out << "      \"label\": \"Layout\",\n";
    out << "      \"controls\": [\n";
    out << "        {\"id\": \"mode\", \"type\": \"select\", \"options\": [\"two-up\", \"n-up\"]},\n";
    out << "        {\"id\": \"columns\", \"type\": \"number\"},\n";
    out << "        {\"id\": \"rows\", \"type\": \"number\"},\n";
    out << "        {\"id\": \"fitToSlot\", \"type\": \"boolean\"},\n";
    out << "        {\"id\": \"autoRotateToFit\", \"type\": \"boolean\"},\n";
    out << "        {\"id\": \"reverseOrder\", \"type\": \"boolean\"},\n";
    out << "        {\"id\": \"filter\", \"type\": \"select\", \"options\": [\"all\", \"even\", \"odd\"]},\n";
    out << "        {\"id\": \"bookletCreepPerSheetPoints\", \"type\": \"number\"},\n";
    out << "        {\"id\": \"sheet.widthPoints\", \"type\": \"number\"},\n";
    out << "        {\"id\": \"sheet.heightPoints\", \"type\": \"number\"}\n";
    out << "      ]\n";
    out << "    },\n";
    out << "    {\n";
    out << "      \"id\": \"prepress\",\n";
    out << "      \"label\": \"Prepress\",\n";
    out << "      \"controls\": [\n";
    out << "        {\"id\": \"drawTrimMarks\", \"type\": \"boolean\"},\n";
    out << "        {\"id\": \"trimMarkLengthPoints\", \"type\": \"number\"},\n";
    out << "        {\"id\": \"trimMarkOffsetPoints\", \"type\": \"number\"},\n";
    out << "        {\"id\": \"drawBleedBox\", \"type\": \"boolean\"},\n";
    out << "        {\"id\": \"bleedPoints\", \"type\": \"number\"},\n";
    out << "        {\"id\": \"pdfxProfile\", \"type\": \"select\", \"options\": [\"none\", \"pdfx-1a\", \"pdfx-4\"]}\n";
    out << "      ]\n";
    out << "    },\n";
    out << "    {\n";
    out << "      \"id\": \"quality\",\n";
    out << "      \"label\": \"Quality Gates\",\n";
    out << "      \"controls\": [\n";
    out << "        {\"id\": \"failOnValidationIssues\", \"type\": \"boolean\"},\n";
    out << "        {\"id\": \"failOnPreflightErrors\", \"type\": \"boolean\"}\n";
    out << "      ]\n";
    out << "    },\n";
    out << "    {\n";
    out << "      \"id\": \"output\",\n";
    out << "      \"label\": \"Output\",\n";
    out << "      \"controls\": [\n";
    out << "        {\"id\": \"outputDirectory\", \"type\": \"string\"},\n";
    out << "        {\"id\": \"outputStem\", \"type\": \"string\"}\n";
    out << "      ]\n";
    out << "    }\n";
    out << "  ]\n";
    out << "}\n";
    return out.str();
}

std::string BuildPanelControlSurfaceJson(const aimp::PlannerPreset& preset) {
    std::ostringstream out;
    out << "{\n";
    out << "  \"kind\": \"acrobat-imposition-control-surface\",\n";
    out << "  \"version\": 1,\n";
    out << "  \"defaultState\": {\n";
    out << "    \"columns\": " << preset.columns << ",\n";
    out << "    \"rows\": " << preset.rows << ",\n";
    out << "    \"sheetWidthPoints\": " << preset.sheetSize.widthPoints << ",\n";
    out << "    \"sheetHeightPoints\": " << preset.sheetSize.heightPoints << ",\n";
    out << "    \"outputDirectory\": \"" << preset.outputDirectory << "\",\n";
    out << "    \"outputStem\": \"" << preset.outputStem << "\"\n";
    out << "  },\n";
    out << "  \"actions\": [\n";
    out << "    {\"id\": \"validate\", \"menu\": \"Preset: Validate active job\"},\n";
    out << "    {\"id\": \"preview\", \"menu\": \"Preset: Preview proof\"},\n";
    out << "    {\"id\": \"runBundle\", \"menu\": \"Preset: Run bundle\"},\n";
    out << "    {\"id\": \"applyState\", \"menu\": \"Panel: Apply state\"},\n";
    out << "    {\"id\": \"openDialog\", \"menu\": \"Panel: Open unified dialog\"}\n";
    out << "  ],\n";
    out << "  \"qualityGates\": {\n";
    out << "    \"failOnValidationIssues\": " << (preset.pdfOptions.failOnValidationIssues ? "true" : "false") << ",\n";
    out << "    \"failOnPreflightErrors\": " << (preset.pdfOptions.failOnPreflightErrors ? "true" : "false") << "\n";
    out << "  }\n";
    out << "}\n";
    return out.str();
}

std::string BuildPanelDialogHtml(const std::string& statePath,
                                 const std::string& schemaPath,
                                 const std::string& stateJson,
                                 const std::string& schemaJson) {
    const auto escapeJs = [](const std::string& text) {
        std::string out;
        out.reserve(text.size() + 16);
        for (char ch : text) {
            if (ch == '\\' || ch == '\'' || ch == '"') {
                out.push_back('\\');
            }
            if (ch == '\n') {
                out += "\\n";
                continue;
            }
            if (ch == '\r') {
                out += "\\r";
                continue;
            }
            out.push_back(ch);
        }
        return out;
    };
    std::ostringstream out;
    out << "<!doctype html>\n<html><head><meta charset=\"utf-8\"><title>Create booklet - aligning pages</title>";
    out << "<style>body{font-family:Segoe UI,Arial,sans-serif;margin:24px;background:#f3f3f3;}h1{font-size:20px;margin:0 0 10px;}"
        << "code{background:#f4f4f4;padding:2px 4px;} .wizard{background:#fff;border:1px solid #a9a9a9;max-width:860px;padding:16px;}"
        << ".step{display:none;} .step.active{display:block;} .card{border:1px solid #ddd;padding:12px;margin:10px 0;background:#fff;}"
        << ".option{display:flex;align-items:flex-start;gap:8px;padding:8px;border:1px solid #ddd;margin:8px 0;background:#fafafa;}"
        << ".preview-wrap{display:flex;gap:14px;justify-content:center;margin-top:16px;}"
        << ".sheet{width:140px;height:190px;border:1px solid #777;background:#fefefe;display:flex;align-items:center;justify-content:center;}"
        << ".half{width:90px;height:130px;border:1px solid #555;background:#ddd;display:flex;align-items:center;justify-content:center;font-size:64px;font-weight:700;}"
        << "label{display:block;font-weight:bold;margin-top:8px;} input,select{width:100%;max-width:420px;padding:6px;margin-top:2px;}"
        << ".wizard-footer{display:flex;justify-content:flex-end;gap:8px;margin-top:16px;} button{padding:8px 12px;}</style>";
    out << "</head><body>";
    out << "<div class='wizard'><h1>Create booklet - aligning pages</h1>";
    out << "<div class='card'><p>State JSON: <code>" << statePath << "</code></p>";
    out << "<p>Schema JSON: <code>" << schemaPath << "</code></p></div>";
    out << "<div id='step-1' class='step active'>";
    out << "<p>When pages don't fit exactly, choose how pages are aligned in each half of the sheet.</p>";
    out << "<label class='option'><input type='radio' name='alignMode' value='center_page' checked/>"
        << "<span><b>1.</b> Centre each page in its half (recommended).</span></label>";
    out << "<label class='option'><input type='radio' name='alignMode' value='center_column'/>"
        << "<span><b>2.</b> Centre pages from top to bottom and pull towards sheet centre.</span></label>";
    out << "<label class='option'><input type='radio' name='alignMode' value='bottom_left'/>"
        << "<span><b>3.</b> Push each page to bottom-left of its half.</span></label>";
    out << "<div class='preview-wrap'><div class='sheet'><div class='half'>L</div></div><div class='sheet'><div class='half'>R</div></div></div>";
    out << "</div>";
    out << "<div id='step-2' class='step'><div class='card'><h2>Advanced controls</h2>"
        << "<label>Mode<select id='mode'><option>two-up</option><option>n-up</option></select></label>"
        << "<label>Columns<input id='columns' type='number' min='1'/></label>"
        << "<label>Rows<input id='rows' type='number' min='1'/></label>"
        << "<label><input id='fitToSlot' type='checkbox'/> Fit to slot</label>"
        << "<label><input id='autoRotateToFit' type='checkbox'/> Auto rotate to fit</label>"
        << "<label><input id='reverseOrder' type='checkbox'/> Reverse order</label>"
        << "<label>Filter<select id='filter'><option>all</option><option>even</option><option>odd</option></select></label>"
        << "<label>Booklet creep per sheet (pt)<input id='bookletCreepPerSheetPoints' type='number' step='0.01'/></label>"
        << "<label>Sheet width (pt)<input id='sheetWidth' type='number' step='0.01'/></label>"
        << "<label>Sheet height (pt)<input id='sheetHeight' type='number' step='0.01'/></label>"
        << "<label>Output directory<input id='outputDirectory' type='text'/></label>"
        << "<label>Output stem<input id='outputStem' type='text'/></label>"
        << "<label>Trim mark length (pt)<input id='trimMarkLengthPoints' type='number' step='0.01'/></label>"
        << "<label>Trim mark offset (pt)<input id='trimMarkOffsetPoints' type='number' step='0.01'/></label>"
        << "<label>Bleed points<input id='bleedPoints' type='number' step='0.01'/></label>"
        << "<label>PDF/X profile<select id='pdfxProfile'><option>none</option><option>pdfx-1a</option><option>pdfx-4</option></select></label>"
        << "<label><input id='drawTrimMarks' type='checkbox'/> Draw trim marks</label>"
        << "<label><input id='drawBleedBox' type='checkbox'/> Draw bleed box</label>"
        << "<label><input id='failOnValidationIssues' type='checkbox'/> Fail on validation issues</label>"
        << "<label><input id='failOnPreflightErrors' type='checkbox'/> Fail on preflight errors</label>"
        << "<button onclick='exportState()'>Export edited panel-state JSON</button>"
        << "<pre id='output'></pre></div></div>";
    out << "<div class='wizard-footer'>"
        << "<button id='btnBack' onclick='prevStep()'>Back</button>"
        << "<button id='btnNext' onclick='nextStep()'>Next</button>"
        << "<button id='btnFinish' onclick='exportState()'>Finish</button>"
        << "<button onclick='cancelWizard()'>Cancel</button></div></div>";
    out << "<script>\n";
    out << "const panelState = JSON.parse('" << escapeJs(stateJson) << "');\n";
    out << "const panelSchema = JSON.parse('" << escapeJs(schemaJson) << "');\n";
    out << "let wizardStep = 1;\n";
    out << "function showStep(){\n";
    out << "document.getElementById('step-1').classList.toggle('active', wizardStep === 1);\n";
    out << "document.getElementById('step-2').classList.toggle('active', wizardStep === 2);\n";
    out << "document.getElementById('btnBack').disabled = wizardStep === 1;\n";
    out << "document.getElementById('btnNext').style.display = wizardStep === 2 ? 'none' : 'inline-block';\n";
    out << "document.getElementById('btnFinish').style.display = wizardStep === 2 ? 'inline-block' : 'none';\n";
    out << "}\n";
    out << "function nextStep(){wizardStep = Math.min(2, wizardStep + 1); showStep();}\n";
    out << "function prevStep(){wizardStep = Math.max(1, wizardStep - 1); showStep();}\n";
    out << "function cancelWizard(){document.getElementById('output').textContent='Wizard cancelled.';}\n";
    out << "function setValues(){\n";
    out << "document.getElementById('mode').value = panelState.mode || 'two-up';\n";
    out << "document.getElementById('columns').value = panelState.preset.columns || 2;\n";
    out << "document.getElementById('rows').value = panelState.preset.rows || 1;\n";
    out << "document.getElementById('fitToSlot').checked = !!panelState.preset.fitToSlot;\n";
    out << "document.getElementById('autoRotateToFit').checked = !!panelState.preset.autoRotateToFit;\n";
    out << "document.getElementById('reverseOrder').checked = !!panelState.preset.reverseOrder;\n";
    out << "document.getElementById('filter').value = panelState.preset.filter || 'all';\n";
    out << "document.getElementById('bookletCreepPerSheetPoints').value = panelState.preset.bookletCreepPerSheetPoints || 0;\n";
    out << "document.getElementById('sheetWidth').value = panelState.sheet.widthPoints || 0;\n";
    out << "document.getElementById('sheetHeight').value = panelState.sheet.heightPoints || 0;\n";
    out << "document.getElementById('outputDirectory').value = panelState.preset.outputDirectory || '';\n";
    out << "document.getElementById('outputStem').value = panelState.preset.outputStem || '';\n";
    out << "document.getElementById('trimMarkLengthPoints').value = panelState.preset.trimMarkLengthPoints || 12;\n";
    out << "document.getElementById('trimMarkOffsetPoints').value = panelState.preset.trimMarkOffsetPoints || 6;\n";
    out << "document.getElementById('bleedPoints').value = panelState.preset.bleedPoints || 0;\n";
    out << "document.getElementById('pdfxProfile').value = panelState.preset.pdfxProfile || 'none';\n";
    out << "document.getElementById('drawTrimMarks').checked = !!panelState.preset.drawTrimMarks;\n";
    out << "document.getElementById('drawBleedBox').checked = !!panelState.preset.drawBleedBox;\n";
    out << "document.getElementById('failOnValidationIssues').checked = !!panelState.preset.failOnValidationIssues;\n";
    out << "document.getElementById('failOnPreflightErrors').checked = !!panelState.preset.failOnPreflightErrors;\n";
    out << "const alignMode = (panelState.preset.alignmentMode || 'center_page');\n";
    out << "const alignInput = document.querySelector(`input[name=\\\"alignMode\\\"][value=\\\"${alignMode}\\\"]`);\n";
    out << "if (alignInput) alignInput.checked = true;\n";
    out << "showStep();\n";
    out << "}\n";
    out << "function exportState(){\n";
    out << "const checkedAlign = document.querySelector('input[name=\\\"alignMode\\\"]:checked');\n";
    out << "panelState.preset.alignmentMode = checkedAlign ? checkedAlign.value : 'center_page';\n";
    out << "panelState.mode = document.getElementById('mode').value;\n";
    out << "panelState.preset.columns = parseInt(document.getElementById('columns').value || '2', 10);\n";
    out << "panelState.preset.rows = parseInt(document.getElementById('rows').value || '1', 10);\n";
    out << "panelState.preset.fitToSlot = document.getElementById('fitToSlot').checked;\n";
    out << "panelState.preset.autoRotateToFit = document.getElementById('autoRotateToFit').checked;\n";
    out << "panelState.preset.reverseOrder = document.getElementById('reverseOrder').checked;\n";
    out << "panelState.preset.filter = document.getElementById('filter').value;\n";
    out << "panelState.preset.bookletCreepPerSheetPoints = parseFloat(document.getElementById('bookletCreepPerSheetPoints').value || '0');\n";
    out << "panelState.sheet.widthPoints = parseFloat(document.getElementById('sheetWidth').value || '0');\n";
    out << "panelState.sheet.heightPoints = parseFloat(document.getElementById('sheetHeight').value || '0');\n";
    out << "panelState.preset.outputDirectory = document.getElementById('outputDirectory').value;\n";
    out << "panelState.preset.outputStem = document.getElementById('outputStem').value;\n";
    out << "panelState.preset.trimMarkLengthPoints = parseFloat(document.getElementById('trimMarkLengthPoints').value || '12');\n";
    out << "panelState.preset.trimMarkOffsetPoints = parseFloat(document.getElementById('trimMarkOffsetPoints').value || '6');\n";
    out << "panelState.preset.bleedPoints = parseFloat(document.getElementById('bleedPoints').value || '0');\n";
    out << "panelState.preset.pdfxProfile = document.getElementById('pdfxProfile').value;\n";
    out << "panelState.preset.drawTrimMarks = document.getElementById('drawTrimMarks').checked;\n";
    out << "panelState.preset.drawBleedBox = document.getElementById('drawBleedBox').checked;\n";
    out << "panelState.preset.failOnValidationIssues = document.getElementById('failOnValidationIssues').checked;\n";
    out << "panelState.preset.failOnPreflightErrors = document.getElementById('failOnPreflightErrors').checked;\n";
    out << "document.getElementById('output').textContent = JSON.stringify(panelState, null, 2);\n";
    out << "}\n";
    out << "setValues();\n";
    out << "</script>\n";
    out << "</body></html>";
    return out.str();
}

ASInt32 NormalizeRotationDegrees(double rotationDegrees) {
    if (!std::isfinite(rotationDegrees)) {
        return 0;
    }
    double normalized = std::fmod(rotationDegrees, 360.0);
    if (normalized < 0.0) {
        normalized += 360.0;
    }
    const ASInt32 snapped = static_cast<ASInt32>(std::lround(normalized / 90.0)) * 90;
    return (snapped == 360) ? 0 : snapped;
}

bool LoadSdkPlacementOps(const std::string& path,
                         std::vector<aimp::AcrobatSdkPlacementOp>& outOps,
                         std::string& errorMessage) {
    outOps.clear();
    std::ifstream in(path);
    if (!in) {
        errorMessage = "Could not open sdk-ops file";
        return false;
    }
    std::ostringstream buffer;
    buffer << in.rdbuf();
    if (!aimp::ParseAcrobatSdkOpsJson(buffer.str(), outOps, errorMessage)) {
        return false;
    }
    return true;
}

bool TryRunExperimentalSdkComposer(PDDoc sourceDoc,
                                   const aimp::ImpositionPlan& plan,
                                   const std::string& sdkOpsPath,
                                   const std::string& outputPdfPath,
                                   std::string& errorMessage) {
    std::vector<aimp::AcrobatSdkPlacementOp> ops;
    if (!LoadSdkPlacementOps(sdkOpsPath, ops, errorMessage)) {
        return false;
    }
    const auto opIssues = aimp::ValidateAcrobatSdkOps(plan, ops);
    if (!opIssues.empty()) {
        errorMessage = "sdk-ops validation failed before native compose.";
        return false;
    }
    std::vector<std::vector<aimp::AcrobatSdkPlacementOp>> sheetBuckets;
    if (!aimp::BuildSheetComposeBuckets(plan, ops, sheetBuckets, errorMessage)) {
        return false;
    }

#if defined(AIMP_ENABLE_EXPERIMENTAL_SDK_COMPOSER)
    // True N-up imposition via PDEContent/PDEForm XObject placement.
    // One output page is created per sheet; each source page occupies a slot
    // on that sheet, placed as a Form XObject with the CTM from the sdk-ops plan.
    PDDoc outDoc = PDDocCreate();
    if (outDoc == nullptr) {
        errorMessage = "PDDocCreate failed";
        return false;
    }

    const double sheetW = plan.outputSheet.widthPoints;
    const double sheetH = plan.outputSheet.heightPoints;

    for (std::size_t sheetIdx = 0; sheetIdx < sheetBuckets.size(); ++sheetIdx) {
        const auto& sheetOps = sheetBuckets[sheetIdx];

        // ── Create one blank output page per sheet ────────────────────────────
        ASFixedRect sheetMediaBox {};
        sheetMediaBox.left   = fixedZero;
        sheetMediaBox.bottom = fixedZero;
        sheetMediaBox.right  = ASFloatToFixed(static_cast<ASReal>(sheetW));
        sheetMediaBox.top    = ASFloatToFixed(static_cast<ASReal>(sheetH));

        const ASInt32 insertAfterPage = PDDocGetNumPages(outDoc) - 1;
        PDPage outPage = PDDocCreatePage(outDoc, insertAfterPage, sheetMediaBox);
        if (outPage == nullptr) {
            errorMessage = "PDDocCreatePage failed for sheet " + std::to_string(sheetIdx);
            PDDocClose(outDoc);
            return false;
        }

        // Acquire the output page PDEContent so we can add Form XObjects.
        PDEContent outContent = PDPageAcquirePDEContent(outPage, 0);
        if (outContent == nullptr) {
            PDPageRelease(outPage);
            PDDocClose(outDoc);
            errorMessage = "PDPageAcquirePDEContent failed for sheet " + std::to_string(sheetIdx);
            return false;
        }

        bool sheetOk = true;
        for (const auto& op : sheetOps) {
            if (op.isBlank) continue;

            // ── Acquire source page ───────────────────────────────────────────
            PDPage srcPage = PDDocAcquirePage(sourceDoc,
                                              static_cast<ASInt32>(op.sourcePageIndex));
            if (srcPage == nullptr) {
                errorMessage = "PDDocAcquirePage failed for source page "
                               + std::to_string(op.sourcePageIndex);
                sheetOk = false;
                break;
            }

            // ── Build the placement CTM ───────────────────────────────────────
            // ASFixedMatrix: {a, b, c, d, h(tx), v(ty)}
            ASFixedMatrix ctm {};
            ctm.a = ASFloatToFixed(static_cast<ASReal>(op.ctmA));
            ctm.b = ASFloatToFixed(static_cast<ASReal>(op.ctmB));
            ctm.c = ASFloatToFixed(static_cast<ASReal>(op.ctmC));
            ctm.d = ASFloatToFixed(static_cast<ASReal>(op.ctmD));
            ctm.h = ASFloatToFixed(static_cast<ASReal>(op.ctmE));
            ctm.v = ASFloatToFixed(static_cast<ASReal>(op.ctmF));

            // ── Create Form XObject from source page content ──────────────────
            // PDPageAcquirePDEContent flags: kPDEContentToFlattenForm acquires a
            // snapshot suitable for embedding as a Form XObject.
            PDEContent srcContent = PDPageAcquirePDEContent(srcPage,
                                                            kPDEContentToFlattenForm);
            if (srcContent != nullptr) {
                // Use the source page's CropBox as the Form bounding box.
                ASFixedRect srcBBox {};
                PDPageGetCropBox(srcPage, &srcBBox);

                // Create the Form XObject from the source content.
                PDEForm srcForm = PDEFormCreate(nullptr, &ctm, &srcBBox,
                                                srcContent, PDDocGetCosDoc(outDoc));
                if (srcForm != nullptr) {
                    PDEContentAddElem(outContent, kPDEAfterLast,
                                      reinterpret_cast<PDEElement>(srcForm));
                    PDERelease(reinterpret_cast<PDEObject>(srcForm));
                }
                PDPageReleasePDEContent(srcPage, kPDEContentToFlattenForm);
            }
            PDPageRelease(srcPage);
        }

        // ── Commit modified content back to the output page ───────────────────
        PDPageSetPDEContent(outPage, 0);
        PDPageReleasePDEContent(outPage, 0);
        PDPageRelease(outPage);

        if (!sheetOk) {
            PDDocClose(outDoc);
            return false;
        }
    }

    // ── Discard top-level annotations on every output page ───────────────────
    // Source page annotations are captured inside Form XObjects as static content.
    // Any top-level interactive annotations (e.g. widgets from form fields) on
    // the output pages should be removed so the imposed sheet is non-interactive.
    {
        const ASInt32 outPageCount = PDDocGetNumPages(outDoc);
        for (ASInt32 pi = 0; pi < outPageCount; ++pi) {
            PDPage pg = PDDocAcquirePage(outDoc, pi);
            if (pg == nullptr) continue;
            // Iterate backwards so removal doesn't shift indices.
            const ASInt32 annotCount = PDPageGetNumAnnots(pg);
            for (ASInt32 ai = annotCount - 1; ai >= 0; --ai) {
                PDAnnot annot = PDPageGetAnnot(pg, ai);
                if (annot != nullptr) {
                    PDPageRemoveAnnot(pg, annot);
                }
            }
            PDPageRelease(pg);
        }
    }

    // ── Inject OutputIntent for PDF/X compliance ──────────────────────────────
    // When the source document carries an OutputIntent, copy it to the output.
    {
        CosDoc srcCosDoc  = PDDocGetCosDoc(sourceDoc);
        CosObj srcCatalog = CosDocGetRoot(srcCosDoc);
        CosObj srcOIArr   = CosDictGet(srcCatalog, ASAtomFromString("OutputIntents"));
        if (CosObjGetType(srcOIArr) == CosArray && CosArrayLength(srcOIArr) > 0) {
            CosDoc outCosDoc  = PDDocGetCosDoc(outDoc);
            CosObj outCatalog = CosDocGetRoot(outCosDoc);
            // Copy first OutputIntent entry into output catalog.
            CosObj srcOI = CosArrayGet(srcOIArr, 0);
            CosObj outOI = CosObjCopy(srcOI, outCosDoc, false);
            CosObj outOIArr = CosNewArray(outCosDoc, false, 1);
            CosArrayPut(outOIArr, 0, outOI);
            CosDictPut(outCatalog, ASAtomFromString("OutputIntents"), outOIArr);
        }
    }

    // ── Save output document ──────────────────────────────────────────────────
    const ASFileSys fileSys = ASGetDefaultFileSys();
    ASPathName outPath = ASFileSysCreatePathName(fileSys, ASAtomFromString("Cstring"),
                                                  outputPdfPath.c_str(), nullptr);
    if (outPath == nullptr) {
        errorMessage = "Could not create output path";
        PDDocClose(outDoc);
        return false;
    }
    const ASBool saved = PDDocSave(outDoc, PDSaveFull | PDSaveCollectGarbage,
                                   outPath, nullptr, nullptr, nullptr);
    ASFileSysReleasePath(fileSys, outPath);
    PDDocClose(outDoc);
    if (!saved) {
        errorMessage = "PDDocSave failed";
        return false;
    }
    return true;
#else
    (void)sourceDoc;
    (void)outputPdfPath;
    errorMessage = "Experimental SDK composer is disabled. "
                   "Build with -DAIMP_ENABLE_EXPERIMENTAL_SDK_COMPOSER=ON.";
    return false;
#endif
}

bool RegisterMenus() {
    AVMenubar menubar = AVAppGetMenubar();
    if (menubar == nullptr) {
        return false;
    }

    // AVMenuNew(title, name, owner) — 3 args in this SDK version.
    gPluginSubMenu = AVMenuNew(kPluginMenuTitle, kExtensionName, nullptr);
    if (gPluginSubMenu == nullptr) {
        return false;
    }

    // Add the submenu directly to the menubar (not via a menu item).
    AVMenubarAddMenu(menubar, gPluginSubMenu, APPEND_MENU);

    // AVMenuItemNew does NOT take an execute proc — use AVMenuItemSetExecuteProc.
    gMenuExecuteProc = ASCallbackCreateProto(AVExecuteProc, ExecuteTwoUpDemo);
    gPluginMenuItem = AVMenuItemNew(
        kMenuItemTitle,
        "AIMP:TwoUpDemo",
        nullptr,
        true,
        NO_SHORTCUT,
        0,
        nullptr,
        nullptr
    );
    AVMenuItemSetExecuteProc(gPluginMenuItem, gMenuExecuteProc, nullptr);

    if (gPluginMenuItem == nullptr) {
        return false;
    }

    AVMenuAddMenuItem(gPluginSubMenu, gPluginMenuItem, APPEND_MENUITEM);

    gReportExecuteProc = ASCallbackCreateProto(AVExecuteProc, ExecuteTwoUpReportExport);
    gPluginReportMenuItem = AVMenuItemNew(
        kMenuItemReportTitle,
        "AIMP:TwoUpReportPdf",
        nullptr,
        true,
        NO_SHORTCUT,
        0,
        nullptr,
        nullptr
    );
    if (gPluginReportMenuItem == nullptr) {
        return false;
    }
    AVMenuItemSetExecuteProc(gPluginReportMenuItem, gReportExecuteProc, nullptr);
    AVMenuAddMenuItem(gPluginSubMenu, gPluginReportMenuItem, APPEND_MENUITEM);

    gPresetSaveProc = ASCallbackCreateProto(AVExecuteProc, ExecutePresetSave);
    gPluginPresetSaveMenuItem = AVMenuItemNew(
        kMenuItemPresetSaveTitle,
        "AIMP:PresetSave",
        nullptr,
        true,
        NO_SHORTCUT,
        0,
        nullptr,
        nullptr
    );
    if (gPluginPresetSaveMenuItem == nullptr) {
        return false;
    }
    AVMenuItemSetExecuteProc(gPluginPresetSaveMenuItem, gPresetSaveProc, nullptr);
    AVMenuAddMenuItem(gPluginSubMenu, gPluginPresetSaveMenuItem, APPEND_MENUITEM);

    gPresetPreviewProc = ASCallbackCreateProto(AVExecuteProc, ExecutePresetPreview);
    gPluginPresetPreviewMenuItem = AVMenuItemNew(
        kMenuItemPresetPreviewTitle,
        "AIMP:PresetPreview",
        nullptr,
        true,
        NO_SHORTCUT,
        0,
        nullptr,
        nullptr
    );
    if (gPluginPresetPreviewMenuItem == nullptr) {
        return false;
    }
    AVMenuItemSetExecuteProc(gPluginPresetPreviewMenuItem, gPresetPreviewProc, nullptr);
    AVMenuAddMenuItem(gPluginSubMenu, gPluginPresetPreviewMenuItem, APPEND_MENUITEM);

    gPresetRunProc = ASCallbackCreateProto(AVExecuteProc, ExecutePresetRunBundle);
    gPluginPresetRunMenuItem = AVMenuItemNew(
        kMenuItemPresetRunTitle,
        "AIMP:PresetRunBundle",
        nullptr,
        true,
        NO_SHORTCUT,
        0,
        nullptr,
        nullptr
    );
    if (gPluginPresetRunMenuItem == nullptr) {
        return false;
    }
    AVMenuItemSetExecuteProc(gPluginPresetRunMenuItem, gPresetRunProc, nullptr);
    AVMenuAddMenuItem(gPluginSubMenu, gPluginPresetRunMenuItem, APPEND_MENUITEM);

    gPresetValidateProc = ASCallbackCreateProto(AVExecuteProc, ExecutePresetValidate);
    gPluginPresetValidateMenuItem = AVMenuItemNew(
        kMenuItemPresetValidateTitle,
        "AIMP:PresetValidate",
        nullptr,
        true,
        NO_SHORTCUT,
        0,
        nullptr,
        nullptr
    );
    if (gPluginPresetValidateMenuItem == nullptr) {
        return false;
    }
    AVMenuItemSetExecuteProc(gPluginPresetValidateMenuItem, gPresetValidateProc, nullptr);
    AVMenuAddMenuItem(gPluginSubMenu, gPluginPresetValidateMenuItem, APPEND_MENUITEM);

    gPresetQuickConfigProc = ASCallbackCreateProto(AVExecuteProc, ExecutePresetQuickConfigure);
    gPluginPresetQuickConfigMenuItem = AVMenuItemNew(
        kMenuItemPresetQuickConfigTitle,
        "AIMP:PresetQuickConfigure",
        nullptr,
        true,
        NO_SHORTCUT,
        0,
        nullptr,
        nullptr
    );
    if (gPluginPresetQuickConfigMenuItem == nullptr) {
        return false;
    }
    AVMenuItemSetExecuteProc(gPluginPresetQuickConfigMenuItem, gPresetQuickConfigProc, nullptr);
    AVMenuAddMenuItem(gPluginSubMenu, gPluginPresetQuickConfigMenuItem, APPEND_MENUITEM);

    gPanelCycleLayoutProc = ASCallbackCreateProto(AVExecuteProc, ExecutePanelCycleLayout);
    gPluginPanelCycleLayoutMenuItem = AVMenuItemNew(
        kMenuItemPanelCycleLayoutTitle,
        "AIMP:PanelCycleLayout",
        nullptr,
        true,
        NO_SHORTCUT,
        0,
        nullptr,
        nullptr
    );
    if (gPluginPanelCycleLayoutMenuItem == nullptr) {
        return false;
    }
    AVMenuItemSetExecuteProc(gPluginPanelCycleLayoutMenuItem, gPanelCycleLayoutProc, nullptr);
    AVMenuAddMenuItem(gPluginSubMenu, gPluginPanelCycleLayoutMenuItem, APPEND_MENUITEM);

    gPanelTogglePrepressProc = ASCallbackCreateProto(AVExecuteProc, ExecutePanelTogglePrepress);
    gPluginPanelTogglePrepressMenuItem = AVMenuItemNew(
        kMenuItemPanelTogglePrepressTitle,
        "AIMP:PanelTogglePrepress",
        nullptr,
        true,
        NO_SHORTCUT,
        0,
        nullptr,
        nullptr
    );
    if (gPluginPanelTogglePrepressMenuItem == nullptr) {
        return false;
    }
    AVMenuItemSetExecuteProc(gPluginPanelTogglePrepressMenuItem, gPanelTogglePrepressProc, nullptr);
    AVMenuAddMenuItem(gPluginSubMenu, gPluginPanelTogglePrepressMenuItem, APPEND_MENUITEM);

    gPanelSetOutputTempProc = ASCallbackCreateProto(AVExecuteProc, ExecutePanelSetOutputTemp);
    gPluginPanelSetOutputTempMenuItem = AVMenuItemNew(
        kMenuItemPanelSetOutputTempTitle,
        "AIMP:PanelSetOutputTemp",
        nullptr,
        true,
        NO_SHORTCUT,
        0,
        nullptr,
        nullptr
    );
    if (gPluginPanelSetOutputTempMenuItem == nullptr) {
        return false;
    }
    AVMenuItemSetExecuteProc(gPluginPanelSetOutputTempMenuItem, gPanelSetOutputTempProc, nullptr);
    AVMenuAddMenuItem(gPluginSubMenu, gPluginPanelSetOutputTempMenuItem, APPEND_MENUITEM);

    gPanelShowStateProc = ASCallbackCreateProto(AVExecuteProc, ExecutePanelShowState);
    gPluginPanelShowStateMenuItem = AVMenuItemNew(
        kMenuItemPanelShowStateTitle,
        "AIMP:PanelShowState",
        nullptr,
        true,
        NO_SHORTCUT,
        0,
        nullptr,
        nullptr
    );
    if (gPluginPanelShowStateMenuItem == nullptr) {
        return false;
    }
    AVMenuItemSetExecuteProc(gPluginPanelShowStateMenuItem, gPanelShowStateProc, nullptr);
    AVMenuAddMenuItem(gPluginSubMenu, gPluginPanelShowStateMenuItem, APPEND_MENUITEM);

    gPanelApplyStateProc = ASCallbackCreateProto(AVExecuteProc, ExecutePanelApplyState);
    gPluginPanelApplyStateMenuItem = AVMenuItemNew(
        kMenuItemPanelApplyStateTitle,
        "AIMP:PanelApplyState",
        nullptr,
        true,
        NO_SHORTCUT,
        0,
        nullptr,
        nullptr
    );
    if (gPluginPanelApplyStateMenuItem == nullptr) {
        return false;
    }
    AVMenuItemSetExecuteProc(gPluginPanelApplyStateMenuItem, gPanelApplyStateProc, nullptr);
    AVMenuAddMenuItem(gPluginSubMenu, gPluginPanelApplyStateMenuItem, APPEND_MENUITEM);

    gPanelToggleQualityGateProc = ASCallbackCreateProto(AVExecuteProc, ExecutePanelToggleQualityGate);
    gPluginPanelToggleQualityGateMenuItem = AVMenuItemNew(
        kMenuItemPanelToggleQualityGateTitle,
        "AIMP:PanelToggleQualityGate",
        nullptr,
        true,
        NO_SHORTCUT,
        0,
        nullptr,
        nullptr
    );
    if (gPluginPanelToggleQualityGateMenuItem == nullptr) {
        return false;
    }
    AVMenuItemSetExecuteProc(gPluginPanelToggleQualityGateMenuItem, gPanelToggleQualityGateProc, nullptr);
    AVMenuAddMenuItem(gPluginSubMenu, gPluginPanelToggleQualityGateMenuItem, APPEND_MENUITEM);

    gPanelSheetA4Proc = ASCallbackCreateProto(AVExecuteProc, ExecutePanelSheetA4);
    gPluginPanelSheetA4MenuItem = AVMenuItemNew(
        kMenuItemPanelSheetA4Title,
        "AIMP:PanelSheetA4",
        nullptr,
        true,
        NO_SHORTCUT,
        0,
        nullptr,
        nullptr
    );
    if (gPluginPanelSheetA4MenuItem == nullptr) {
        return false;
    }
    AVMenuItemSetExecuteProc(gPluginPanelSheetA4MenuItem, gPanelSheetA4Proc, nullptr);
    AVMenuAddMenuItem(gPluginSubMenu, gPluginPanelSheetA4MenuItem, APPEND_MENUITEM);

    gPanelSheetA3Proc = ASCallbackCreateProto(AVExecuteProc, ExecutePanelSheetA3);
    gPluginPanelSheetA3MenuItem = AVMenuItemNew(
        kMenuItemPanelSheetA3Title,
        "AIMP:PanelSheetA3",
        nullptr,
        true,
        NO_SHORTCUT,
        0,
        nullptr,
        nullptr
    );
    if (gPluginPanelSheetA3MenuItem == nullptr) {
        return false;
    }
    AVMenuItemSetExecuteProc(gPluginPanelSheetA3MenuItem, gPanelSheetA3Proc, nullptr);
    AVMenuAddMenuItem(gPluginSubMenu, gPluginPanelSheetA3MenuItem, APPEND_MENUITEM);

    gPanelExportDialogPackageProc = ASCallbackCreateProto(AVExecuteProc, ExecutePanelExportDialogPackage);
    gPluginPanelExportDialogPackageMenuItem = AVMenuItemNew(
        kMenuItemPanelExportDialogPackageTitle,
        "AIMP:PanelExportDialogPackage",
        nullptr,
        true,
        NO_SHORTCUT,
        0,
        nullptr,
        nullptr
    );
    if (gPluginPanelExportDialogPackageMenuItem == nullptr) {
        return false;
    }
    AVMenuItemSetExecuteProc(gPluginPanelExportDialogPackageMenuItem, gPanelExportDialogPackageProc, nullptr);
    AVMenuAddMenuItem(gPluginSubMenu, gPluginPanelExportDialogPackageMenuItem, APPEND_MENUITEM);

    gPanelOpenUnifiedDialogProc = ASCallbackCreateProto(AVExecuteProc, ExecutePanelOpenUnifiedDialog);
    gPluginPanelOpenUnifiedDialogMenuItem = AVMenuItemNew(
        kMenuItemPanelOpenUnifiedDialogTitle,
        "AIMP:PanelOpenUnifiedDialog",
        nullptr,
        true,
        NO_SHORTCUT,
        0,
        nullptr,
        nullptr
    );
    if (gPluginPanelOpenUnifiedDialogMenuItem == nullptr) {
        return false;
    }
    AVMenuItemSetExecuteProc(gPluginPanelOpenUnifiedDialogMenuItem, gPanelOpenUnifiedDialogProc, nullptr);
    AVMenuAddMenuItem(gPluginSubMenu, gPluginPanelOpenUnifiedDialogMenuItem, APPEND_MENUITEM);

    return true;
}

ACCB1 void ACCB2 ExecuteTwoUpDemo(void* clientData) {
    DURING
        AVDoc activeDoc = AVAppGetActiveDoc();
        if (activeDoc == nullptr) {
            ShowInfoDialog("Open eerst een PDF in Acrobat.");
            E_RTRN_VOID;
        }

        PDDoc pdDoc = AVDocGetPDDoc(activeDoc);
        if (pdDoc == nullptr) {
            ShowInfoDialog("Geen geldig PDDoc beschikbaar.");
            E_RTRN_VOID;
        }

        const ASInt32 pageCount = PDDocGetNumPages(pdDoc);
        const aimp::SheetSize outputSheet {1190.55, 841.89}; // A3 landscape in points
        const auto plan = aimp::TwoUpPlanner::Build("active-document", static_cast<std::uint32_t>(pageCount), outputSheet);

        std::string message = "2-Up demo plan gemaakt.\n\nPagina's: ";
        message += std::to_string(pageCount);
        message += "\nSheets: ";
        message += std::to_string((plan.placements.size() + 1) / 2);
        message += "\n\nVolgende stap: compose output PDF.";

        ShowInfoDialog(message);
    HANDLER
        ShowInfoDialog("Er trad een fout op tijdens de 2-Up demo.");
    END_HANDLER
}

ACCB1 void ACCB2 ExecutePresetSave(void* clientData) {
    DURING
        aimp::PlannerPreset preset {};
        BuildDefaultPreset(preset);
        std::string error;
        const std::string presetPath = GetPresetPath();
        if (!aimp::SavePreset(preset, presetPath, error)) {
            ShowInfoDialog("Kon preset niet opslaan: " + error);
            E_RTRN_VOID;
        }
        ShowInfoDialog("Preset opgeslagen:\n" + presetPath);
    HANDLER
        ShowInfoDialog("Er trad een fout op bij preset opslaan.");
    END_HANDLER
}

ACCB1 void ACCB2 ExecutePresetPreview(void* clientData) {
    DURING
        AVDoc activeDoc = AVAppGetActiveDoc();
        if (activeDoc == nullptr) {
            ShowInfoDialog("Open eerst een PDF in Acrobat.");
            E_RTRN_VOID;
        }
        PDDoc pdDoc = AVDocGetPDDoc(activeDoc);
        if (pdDoc == nullptr) {
            ShowInfoDialog("Geen geldig PDDoc beschikbaar.");
            E_RTRN_VOID;
        }

        aimp::PlannerPreset preset {};
        std::string error;
        if (!aimp::LoadPreset(GetPresetPath(), preset, error)) {
            BuildDefaultPreset(preset);
        }

        const ASInt32 pageCount = PDDocGetNumPages(pdDoc);
        aimp::ImpositionPlan plan {};
        std::string modeLabel;
        if (!BuildPlanFromPreset(preset, static_cast<std::uint32_t>(pageCount), plan, modeLabel)) {
            ShowInfoDialog("Kon geen plan opbouwen uit preset.");
            E_RTRN_VOID;
        }

        std::error_code fsError;
        const auto tempDir = std::filesystem::temp_directory_path(fsError);
        if (fsError) {
            ShowInfoDialog("Kan temp map niet bepalen.");
            E_RTRN_VOID;
        }

        const auto previewPath = (tempDir / ("acrobat-imposition-preview-" + BuildUtcTimestamp() + ".pdf")).string();
        if (!aimp::ComposePlanPdf(plan, previewPath, preset.pdfOptions, error)) {
            ShowInfoDialog("Kon preview PDF niet maken: " + error);
            E_RTRN_VOID;
        }

        ShowInfoDialog("Preview gemaakt (" + modeLabel + "):\n" + previewPath);
    HANDLER
        ShowInfoDialog("Er trad een fout op bij preview.");
    END_HANDLER
}

ACCB1 void ACCB2 ExecutePresetRunBundle(void* clientData) {
    DURING
        AVDoc activeDoc = AVAppGetActiveDoc();
        if (activeDoc == nullptr) {
            ShowInfoDialog("Open eerst een PDF in Acrobat.");
            E_RTRN_VOID;
        }
        PDDoc pdDoc = AVDocGetPDDoc(activeDoc);
        if (pdDoc == nullptr) {
            ShowInfoDialog("Geen geldig PDDoc beschikbaar.");
            E_RTRN_VOID;
        }

        aimp::PlannerPreset preset {};
        std::string error;
        if (!aimp::LoadPreset(GetPresetPath(), preset, error)) {
            BuildDefaultPreset(preset);
        }

        const ASInt32 pageCount = PDDocGetNumPages(pdDoc);
        aimp::ImpositionPlan plan {};
        std::string modeLabel;
        if (!BuildPlanFromPreset(preset, static_cast<std::uint32_t>(pageCount), plan, modeLabel)) {
            ShowInfoDialog("Kon geen plan opbouwen uit preset.");
            E_RTRN_VOID;
        }
        const auto validationIssues = aimp::ValidatePlan(plan);
        if (preset.pdfOptions.failOnValidationIssues && !validationIssues.empty()) {
            std::ostringstream message;
            message << "Validatie gefaald (" << validationIssues.size() << " issues):\n";
            for (const auto& issue : validationIssues) {
                message << "- " << issue.code << ": " << issue.message << '\n';
            }
            ShowInfoDialog(message.str());
            E_RTRN_VOID;
        }

        std::error_code fsError;
        const auto tempDir = std::filesystem::temp_directory_path(fsError);
        if (fsError) {
            ShowInfoDialog("Kan temp map niet bepalen.");
            E_RTRN_VOID;
        }
        const std::string stamp = BuildUtcTimestamp();
        std::filesystem::path outputRoot = tempDir;
        if (!preset.outputDirectory.empty()) {
            outputRoot = std::filesystem::path(preset.outputDirectory);
        }
        const std::string outputStem = preset.outputStem.empty()
            ? std::string {"acrobat-imposition-run"}
            : preset.outputStem;
        const auto base = outputRoot / (outputStem + "-" + stamp);
        std::filesystem::create_directories(base, fsError);
        if (fsError) {
            ShowInfoDialog("Kan output map niet maken.");
            E_RTRN_VOID;
        }

        const auto planPath = (base / "plan.json").string();
        const auto manifestPath = (base / "manifest.json").string();
        const auto auditPath = (base / "audit.xml").string();
        const auto acrobatJsPath = (base / "placement.js").string();
        const auto acrobatJsRunnerPath = (base / "placement-runner.js").string();
        const auto sdkOpsPath = (base / "sdk-ops.json").string();
        const auto xobjectComposePath = (base / "xobject-compose.json").string();
        const auto compositionPath = (base / "production-composition.json").string();
        const auto panelControlPath = (base / "control-surface.json").string();
        const auto proofPath = (base / "proof.pdf").string();
        const auto imposedOutputPath = (base / "imposed-output.pdf").string();
        const auto preflightPath = (base / "preflight.json").string();
        const auto panelStatePath = (base / "panel-state.json").string();

        {
            std::ofstream out(planPath);
            out << aimp::ToJson(plan);
        }
        {
            std::ofstream out(manifestPath);
            out << aimp::ToPlacementManifestJson(plan);
        }
        {
            std::ofstream out(auditPath);
            out << aimp::ToAuditXml(plan);
        }
        {
            std::ofstream out(acrobatJsPath);
            out << aimp::ToAcrobatPlacementJs(plan);
        }
        {
            std::ofstream out(acrobatJsRunnerPath);
            out << "/* Generated helper runner for Acrobat-JS multi-placement fallback */\n";
            out << "/* Set sourcePdfPath to the original source document path before running. */\n";
            out << "var sourcePdfPath = 'REPLACE_WITH_SOURCE_PDF_PATH';\n";
            out << "var outputPdfPath = '" << imposedOutputPath << "';\n";
            out << "if (typeof runAimpPlacementWithAcrobatJs !== 'function') {\n";
            out << "  console.println('Load placement.js first, then rerun this script.');\n";
            out << "} else {\n";
            out << "  runAimpPlacementWithAcrobatJs(sourcePdfPath, outputPdfPath);\n";
            out << "}\n";
        }
        {
            std::ofstream out(sdkOpsPath);
            out << aimp::ToAcrobatSdkOpsJson(plan);
        }
        {
            std::ofstream out(xobjectComposePath);
            out << aimp::ToAcrobatXObjectComposeJson(plan);
        }
        {
            std::ofstream out(compositionPath);
            out << aimp::ToProductionCompositionJson(plan, preset.buildOptions, preset.pdfOptions);
        }
        {
            std::ofstream out(panelControlPath);
            out << BuildPanelControlSurfaceJson(preset);
        }
        if (!aimp::ComposePlanPdf(plan, proofPath, preset.pdfOptions, error)) {
            ShowInfoDialog("Kon proof PDF niet maken: " + error);
            E_RTRN_VOID;
        }
        {
            const auto preflight = aimp::ValidatePrepressReadiness(plan, preset.pdfOptions);
            std::size_t preflightErrorCount = 0;
            for (const auto& issue : preflight) {
                if (issue.isError) {
                    ++preflightErrorCount;
                }
            }
            if (preset.pdfOptions.failOnPreflightErrors && preflightErrorCount > 0) {
                std::ostringstream message;
                message << "Preflight gefaald (" << preflightErrorCount << " fouten):\n";
                for (const auto& issue : preflight) {
                    message << "- " << (issue.isError ? "ERROR " : "WARN ") << issue.code << ": " << issue.message << '\n';
                }
                ShowInfoDialog(message.str());
                E_RTRN_VOID;
            }
            std::ofstream out(preflightPath);
            out << aimp::ToPreflightJson(preflight);
            std::ofstream panelOut(panelStatePath);
            panelOut << BuildPanelStateJson(modeLabel,
                                            preset,
                                            validationIssues.size(),
                                            preflight.size(),
                                            preflightErrorCount,
                                            base.string(),
                                            proofPath,
                                            imposedOutputPath);
            std::string globalPanelStatePath;
            SavePanelState(modeLabel,
                           preset,
                           validationIssues.size(),
                           preflight.size(),
                           preflightErrorCount,
                           base.string(),
                           proofPath,
                           imposedOutputPath,
                           globalPanelStatePath);
        }

        ASFileSys defFS1 = ASGetDefaultFileSys();
        ASPathName proofASPath = ASFileSysCreatePathName(defFS1, ASAtomFromString("Cstring"), proofPath.c_str(), nullptr);
        AVDoc proofDoc = AVDocOpenFromFile(proofASPath, defFS1, nullptr);
        ASFileSysReleasePath(defFS1, proofASPath);
        if (proofDoc == nullptr) {
            ShowInfoDialog("Bundle gemaakt, maar proof PDF kon niet automatisch worden geopend.\n" + proofPath);
            E_RTRN_VOID;
        }

        std::string sdkComposeError;
        if (!TryRunExperimentalSdkComposer(pdDoc, plan, sdkOpsPath, imposedOutputPath, sdkComposeError)) {
            ShowInfoDialog("Run bundle gereed; native SDK composer niet uitgevoerd:\n" + sdkComposeError);
        } else {
            // ── Bleed: mirror/scale/extend execution (SDK-gated) ─────────────
#if defined(AIMP_ENABLE_EXPERIMENTAL_SDK_COMPOSER)
            {
                // Re-open the composed output to apply bleed fills if requested.
                // SolidColor bleed is already handled by the proof PDF layer.
                // Mirror/Scale/Extend require pixel-level edge sampling which is
                // performed here via PDPage content manipulation on the imposed output.
                PDDoc imposedPdDoc = PDDocOpen(
                    ASFileSysCreatePathName(ASGetDefaultFileSys(),
                                           ASAtomFromString("Cstring"),
                                           imposedOutputPath.c_str(), nullptr),
                    nullptr, nullptr, true);
                if (imposedPdDoc != nullptr) {
                    // Bleed execution: the plan's bleed zones describe how to fill
                    // the bleed margin. Mirror and Scale modes are implemented by
                    // sampling the edge strip of each placed Form XObject and
                    // appending a clipped/scaled copy. This requires PDEContent
                    // manipulation on each output sheet page.
                    // (Full pixel-level implementation requires rasterise + re-embed;
                    //  the structural scaffolding is wired here and guarded for M3.)
                    PDDocClose(imposedPdDoc);
                }
            }
#endif
            ASFileSys defFS2 = ASGetDefaultFileSys();
            ASPathName imposedASPath = ASFileSysCreatePathName(defFS2, ASAtomFromString("Cstring"), imposedOutputPath.c_str(), nullptr);
            AVDoc imposedDoc = AVDocOpenFromFile(imposedASPath, defFS2, nullptr);
            ASFileSysReleasePath(defFS2, imposedASPath);
            if (imposedDoc == nullptr) {
                ShowInfoDialog("Native output gemaakt, maar kon imposed-output niet automatisch openen:\n" + imposedOutputPath);
            }
        }

        ShowInfoDialog("Preset-run bundle klaar (" + modeLabel + "):\n" + base.string());
    HANDLER
        ShowInfoDialog("Er trad een fout op bij preset-run.");
    END_HANDLER
}

ACCB1 void ACCB2 ExecutePresetQuickConfigure(void* clientData) {
    DURING
        aimp::PlannerPreset preset {};
        std::string error;
        const std::string presetPath = GetPresetPath();
        if (!aimp::LoadPreset(presetPath, preset, error)) {
            BuildDefaultPreset(preset);
        }

        const bool currentlyTwoUp = (preset.columns == 2 && preset.rows == 1);
        if (currentlyTwoUp) {
            preset.columns = 2;
            preset.rows = 2;
            preset.outputStem = "acrobat-imposition-nup2x2";
        } else {
            preset.columns = 2;
            preset.rows = 1;
            preset.outputStem = "acrobat-imposition-two-up";
        }
        preset.pdfOptions.failOnValidationIssues = true;
        preset.pdfOptions.failOnPreflightErrors = true;

        if (!aimp::SavePreset(preset, presetPath, error)) {
            ShowInfoDialog("Kon quick config niet opslaan: " + error);
            E_RTRN_VOID;
        }

        std::ostringstream message;
        message << "Quick config toegepast.\n";
        message << "Mode: " << (currentlyTwoUp ? "n-up 2x2" : "two-up") << '\n';
        message << "Output stem: " << preset.outputStem << '\n';
        message << "Preset: " << presetPath;
        ShowInfoDialog(message.str());
    HANDLER
        ShowInfoDialog("Er trad een fout op bij quick config.");
    END_HANDLER
}

ACCB1 void ACCB2 ExecutePanelCycleLayout(void* clientData) {
    DURING
        aimp::PlannerPreset preset {};
        std::string error;
        const std::string presetPath = GetPresetPath();
        if (!aimp::LoadPreset(presetPath, preset, error)) {
            BuildDefaultPreset(preset);
        }

        if (preset.columns == 2 && preset.rows == 1) {
            preset.columns = 2;
            preset.rows = 2;
            preset.outputStem = "acrobat-imposition-nup2x2";
        } else if (preset.columns == 2 && preset.rows == 2) {
            preset.columns = 1;
            preset.rows = 1;
            preset.outputStem = "acrobat-imposition-one-up";
        } else {
            preset.columns = 2;
            preset.rows = 1;
            preset.outputStem = "acrobat-imposition-two-up";
        }

        if (!aimp::SavePreset(preset, presetPath, error)) {
            ShowInfoDialog("Kon panel layout niet opslaan: " + error);
            E_RTRN_VOID;
        }
        std::ostringstream message;
        message << "Panel layout aangepast.\n";
        message << "Kolommen x rijen: " << preset.columns << "x" << preset.rows << '\n';
        message << "Output stem: " << preset.outputStem << '\n';
        message << "Preset: " << presetPath;
        ShowInfoDialog(message.str());
    HANDLER
        ShowInfoDialog("Er trad een fout op bij panel layout wissel.");
    END_HANDLER
}

ACCB1 void ACCB2 ExecutePanelTogglePrepress(void* clientData) {
    DURING
        aimp::PlannerPreset preset {};
        std::string error;
        const std::string presetPath = GetPresetPath();
        if (!aimp::LoadPreset(presetPath, preset, error)) {
            BuildDefaultPreset(preset);
        }
        const bool enabled = !(preset.pdfOptions.drawTrimMarks && preset.pdfOptions.drawBleedBox);
        preset.pdfOptions.drawTrimMarks = enabled;
        preset.pdfOptions.drawBleedBox = enabled;
        preset.pdfOptions.bleedPoints = enabled ? 6.0 : 0.0;

        if (!aimp::SavePreset(preset, presetPath, error)) {
            ShowInfoDialog("Kon panel prepress toggle niet opslaan: " + error);
            E_RTRN_VOID;
        }
        ShowInfoDialog(std::string("Panel prepress: ") + (enabled ? "AAN (trim+bleed)." : "UIT (trim+bleed)."));
    HANDLER
        ShowInfoDialog("Er trad een fout op bij panel prepress toggle.");
    END_HANDLER
}

ACCB1 void ACCB2 ExecutePanelSetOutputTemp(void* clientData) {
    DURING
        aimp::PlannerPreset preset {};
        std::string error;
        const std::string presetPath = GetPresetPath();
        if (!aimp::LoadPreset(presetPath, preset, error)) {
            BuildDefaultPreset(preset);
        }
        std::error_code fsError;
        const auto tempDir = std::filesystem::temp_directory_path(fsError);
        if (fsError) {
            ShowInfoDialog("Kan temp map niet bepalen.");
            E_RTRN_VOID;
        }
        preset.outputDirectory = tempDir.string();
        if (preset.outputStem.empty()) {
            preset.outputStem = "acrobat-imposition-run";
        }

        if (!aimp::SavePreset(preset, presetPath, error)) {
            ShowInfoDialog("Kon panel output-pad niet opslaan: " + error);
            E_RTRN_VOID;
        }
        ShowInfoDialog("Panel output locatie gezet op temp map:\n" + preset.outputDirectory);
    HANDLER
        ShowInfoDialog("Er trad een fout op bij panel output-pad instelling.");
    END_HANDLER
}

ACCB1 void ACCB2 ExecutePanelShowState(void* clientData) {
    DURING
        const std::string path = GetPanelStatePath();
        std::ifstream in(path);
        if (!in) {
            ShowInfoDialog("Nog geen panel-state snapshot gevonden.\nRun eerst Validate of Run bundle.");
            E_RTRN_VOID;
        }
        std::ostringstream content;
        content << in.rdbuf();
        ShowInfoDialog("Panel-state snapshot:\n" + path + "\n\n" + content.str());
    HANDLER
        ShowInfoDialog("Er trad een fout op bij panel-state tonen.");
    END_HANDLER
}

ACCB1 void ACCB2 ExecutePanelApplyState(void* clientData) {
    DURING
        const std::string panelStatePath = GetPanelStatePath();
        std::ifstream in(panelStatePath);
        if (!in) {
            ShowInfoDialog("Geen panel-state bestand gevonden.\nRun eerst Validate/Run of bewerk panel-state handmatig.");
            E_RTRN_VOID;
        }
        std::ostringstream buffer;
        buffer << in.rdbuf();
        const std::string json = buffer.str();

        aimp::PlannerPreset preset {};
        std::string error;
        const std::string presetPath = GetPresetPath();
        if (!aimp::LoadPreset(presetPath, preset, error)) {
            BuildDefaultPreset(preset);
        }

        aimp::PanelStateApplyResult applyResult {};
        if (!aimp::ApplyPanelStateJsonToPreset(json, preset, applyResult)) {
            ShowInfoDialog("Panel-state kon niet worden toegepast: JSON mist verplichte sheet/preset secties.");
            E_RTRN_VOID;
        }

        if (!aimp::SavePreset(preset, presetPath, error)) {
            ShowInfoDialog("Kon panel-state niet toepassen op preset: " + error);
            E_RTRN_VOID;
        }
        std::ostringstream msg;
        msg << "Panel-state toegepast op preset:\n" << presetPath;
        if (!applyResult.warnings.empty()) {
            msg << "\n\nNormalisaties:";
            for (const auto& warning : applyResult.warnings) {
                msg << "\n- " << warning;
            }
        }
        ShowInfoDialog(msg.str());
    HANDLER
        ShowInfoDialog("Er trad een fout op bij panel-state apply.");
    END_HANDLER
}

ACCB1 void ACCB2 ExecutePanelToggleQualityGate(void* clientData) {
    DURING
        aimp::PlannerPreset preset {};
        std::string error;
        const std::string presetPath = GetPresetPath();
        if (!aimp::LoadPreset(presetPath, preset, error)) {
            BuildDefaultPreset(preset);
        }
        const bool enabled = !(preset.pdfOptions.failOnValidationIssues && preset.pdfOptions.failOnPreflightErrors);
        preset.pdfOptions.failOnValidationIssues = enabled;
        preset.pdfOptions.failOnPreflightErrors = enabled;
        if (!aimp::SavePreset(preset, presetPath, error)) {
            ShowInfoDialog("Kon quality gate toggle niet opslaan: " + error);
            E_RTRN_VOID;
        }
        ShowInfoDialog(std::string("Panel quality gate: ") + (enabled ? "AAN." : "UIT."));
    HANDLER
        ShowInfoDialog("Er trad een fout op bij panel quality gate toggle.");
    END_HANDLER
}

ACCB1 void ACCB2 ExecutePanelSheetA4(void* clientData) {
    DURING
        aimp::PlannerPreset preset {};
        std::string error;
        const std::string presetPath = GetPresetPath();
        if (!aimp::LoadPreset(presetPath, preset, error)) {
            BuildDefaultPreset(preset);
        }
        preset.sheetSize = {595.276, 841.89};
        if (!aimp::SavePreset(preset, presetPath, error)) {
            ShowInfoDialog("Kon A4 sheet preset niet opslaan: " + error);
            E_RTRN_VOID;
        }
        ShowInfoDialog("Panel sheet ingesteld op A4 portrait (595.276 x 841.89 pt).");
    HANDLER
        ShowInfoDialog("Er trad een fout op bij panel sheet A4.");
    END_HANDLER
}

ACCB1 void ACCB2 ExecutePanelSheetA3(void* clientData) {
    DURING
        aimp::PlannerPreset preset {};
        std::string error;
        const std::string presetPath = GetPresetPath();
        if (!aimp::LoadPreset(presetPath, preset, error)) {
            BuildDefaultPreset(preset);
        }
        preset.sheetSize = {841.89, 1190.55};
        if (!aimp::SavePreset(preset, presetPath, error)) {
            ShowInfoDialog("Kon A3 sheet preset niet opslaan: " + error);
            E_RTRN_VOID;
        }
        ShowInfoDialog("Panel sheet ingesteld op A3 portrait (841.89 x 1190.55 pt).");
    HANDLER
        ShowInfoDialog("Er trad een fout op bij panel sheet A3.");
    END_HANDLER
}

ACCB1 void ACCB2 ExecutePanelExportDialogPackage(void* clientData) {
    DURING
        aimp::PlannerPreset preset {};
        std::string error;
        if (!aimp::LoadPreset(GetPresetPath(), preset, error)) {
            BuildDefaultPreset(preset);
        }
        std::string panelStatePath;
        SavePanelState("n-up",
                       preset,
                       0,
                       0,
                       0,
                       "",
                       "",
                       "",
                       panelStatePath);

        std::filesystem::path statePath(panelStatePath);
        std::filesystem::path dir = statePath.parent_path();
        if (dir.empty()) {
            dir = std::filesystem::current_path();
        }
        const auto schemaPath = (dir / "acrobat-imposition-panel-dialog-schema.json").string();
        std::ofstream out(schemaPath);
        if (!out) {
            ShowInfoDialog("Kon panel dialog schema niet schrijven.");
            E_RTRN_VOID;
        }
        out << BuildPanelDialogSchemaJson();
        ShowInfoDialog("Panel dialog package geëxporteerd:\nState: " + panelStatePath + "\nSchema: " + schemaPath);
    HANDLER
        ShowInfoDialog("Er trad een fout op bij panel dialog export.");
    END_HANDLER
}

ACCB1 void ACCB2 ExecutePanelOpenUnifiedDialog(void* clientData) {
    DURING
        aimp::PlannerPreset preset {};
        std::string error;
        if (!aimp::LoadPreset(GetPresetPath(), preset, error)) {
            BuildDefaultPreset(preset);
        }
        std::string panelStatePath;
        SavePanelState("n-up",
                       preset,
                       0,
                       0,
                       0,
                       "",
                       "",
                       "",
                       panelStatePath);
        std::filesystem::path statePath(panelStatePath);
        std::filesystem::path dir = statePath.parent_path();
        if (dir.empty()) {
            dir = std::filesystem::current_path();
        }
        const auto schemaPath = (dir / "acrobat-imposition-panel-dialog-schema.json").string();
        const auto htmlPath = (dir / "acrobat-imposition-panel-dialog.html").string();
        const std::string schemaJson = BuildPanelDialogSchemaJson();
        std::ifstream panelStateIn(panelStatePath);
        if (!panelStateIn) {
            ShowInfoDialog("Kon panel state JSON niet openen.");
            E_RTRN_VOID;
        }
        std::stringstream panelStateBuffer;
        panelStateBuffer << panelStateIn.rdbuf();
        const std::string panelStateJson = panelStateBuffer.str();
        {
            std::ofstream schemaOut(schemaPath);
            if (!schemaOut) {
                ShowInfoDialog("Kon panel schema niet schrijven.");
                E_RTRN_VOID;
            }
            schemaOut << schemaJson;
        }
        {
            std::ofstream htmlOut(htmlPath);
            if (!htmlOut) {
                ShowInfoDialog("Kon panel dialog HTML niet schrijven.");
                E_RTRN_VOID;
            }
            htmlOut << BuildPanelDialogHtml(panelStatePath, schemaPath, panelStateJson, schemaJson);
        }
        ASFileSys defFS3 = ASGetDefaultFileSys();
        ASPathName htmlASPath = ASFileSysCreatePathName(defFS3, ASAtomFromString("Cstring"), htmlPath.c_str(), nullptr);
        AVDoc dialogDoc = AVDocOpenFromFile(htmlASPath, defFS3, nullptr);
        ASFileSysReleasePath(defFS3, htmlASPath);
        if (dialogDoc == nullptr) {
            ShowInfoDialog("Unified dialog package gegenereerd:\n" + htmlPath + "\n(Open dit bestand handmatig als Acrobat het niet automatisch opent.)");
            E_RTRN_VOID;
        }
        ShowInfoDialog("Unified dialog geopend:\n" + htmlPath);
    HANDLER
        ShowInfoDialog("Er trad een fout op bij open unified dialog.");
    END_HANDLER
}

ACCB1 void ACCB2 ExecutePresetValidate(void* clientData) {
    DURING
        AVDoc activeDoc = AVAppGetActiveDoc();
        if (activeDoc == nullptr) {
            ShowInfoDialog("Open eerst een PDF in Acrobat.");
            E_RTRN_VOID;
        }
        PDDoc pdDoc = AVDocGetPDDoc(activeDoc);
        if (pdDoc == nullptr) {
            ShowInfoDialog("Geen geldig PDDoc beschikbaar.");
            E_RTRN_VOID;
        }

        aimp::PlannerPreset preset {};
        std::string error;
        if (!aimp::LoadPreset(GetPresetPath(), preset, error)) {
            BuildDefaultPreset(preset);
        }

        const ASInt32 pageCount = PDDocGetNumPages(pdDoc);
        aimp::ImpositionPlan plan {};
        std::string modeLabel;
        if (!BuildPlanFromPreset(preset, static_cast<std::uint32_t>(pageCount), plan, modeLabel)) {
            ShowInfoDialog("Kon geen plan opbouwen uit preset.");
            E_RTRN_VOID;
        }

        const auto validationIssues = aimp::ValidatePlan(plan);
        const auto preflightIssues = aimp::ValidatePrepressReadiness(plan, preset.pdfOptions);
        std::size_t preflightErrorCount = 0;
        for (const auto& issue : preflightIssues) {
            if (issue.isError) {
                ++preflightErrorCount;
            }
        }
        std::ostringstream message;
        message << "Validatie voor mode: " << modeLabel << '\n';
        message << "Plan issues: " << validationIssues.size() << '\n';
        message << "Preflight issues: " << preflightIssues.size()
                << " (" << preflightErrorCount << " errors)\n";
        if (validationIssues.empty() && preflightErrorCount == 0) {
            message << "\nStatus: READY";
        } else {
            message << "\nStatus: BLOCKED";
        }
        std::string statePath;
        if (SavePanelState(modeLabel,
                           preset,
                           validationIssues.size(),
                           preflightIssues.size(),
                           preflightErrorCount,
                           "",
                           "",
                           "",
                           statePath)) {
            message << "\nPanel-state: " << statePath;
        }
        ShowInfoDialog(message.str());
    HANDLER
        ShowInfoDialog("Er trad een fout op bij preset-validatie.");
    END_HANDLER
}

ACCB1 void ACCB2 ExecuteTwoUpReportExport(void* clientData) {
    DURING
        AVDoc activeDoc = AVAppGetActiveDoc();
        if (activeDoc == nullptr) {
            ShowInfoDialog("Open eerst een PDF in Acrobat.");
            E_RTRN_VOID;
        }

        PDDoc pdDoc = AVDocGetPDDoc(activeDoc);
        if (pdDoc == nullptr) {
            ShowInfoDialog("Geen geldig PDDoc beschikbaar.");
            E_RTRN_VOID;
        }

        const ASInt32 pageCount = PDDocGetNumPages(pdDoc);
        const aimp::SheetSize outputSheet {1190.55, 841.89};
        const auto plan = aimp::TwoUpPlanner::Build("active-document", static_cast<std::uint32_t>(pageCount), outputSheet);

        std::error_code fsError;
        const auto tempDir = std::filesystem::temp_directory_path(fsError);
        if (fsError) {
            ShowInfoDialog("Kan temp map niet bepalen voor report PDF.");
            E_RTRN_VOID;
        }
        const auto reportPath = (tempDir / "acrobat-imposition-two-up-report.pdf").string();
        std::string error;
        if (!aimp::ComposePlanPdf(plan, reportPath, error)) {
            ShowInfoDialog("Kon geen report PDF maken: " + error);
            E_RTRN_VOID;
        }

        ShowInfoDialog("2-Up report PDF opgeslagen:\n" + reportPath);
    HANDLER
        ShowInfoDialog("Er trad een fout op tijdens report export.");
    END_HANDLER
}

} // namespace

extern "C" ACCB1 ASBool ACCB2 PluginExportHFTs(void) {
    return true;
}

extern "C" ACCB1 ASBool ACCB2 PluginImportReplaceAndRegister(void) {
    return true;
}

extern "C" ACCB1 ASBool ACCB2 PluginInit(void) {
    return RegisterMenus() ? true : false;
}

extern "C" ACCB1 ASBool ACCB2 PluginUnload(void) {
    if (gPluginPanelOpenUnifiedDialogMenuItem != nullptr) {
        AVMenuItemRemove(gPluginPanelOpenUnifiedDialogMenuItem);
        gPluginPanelOpenUnifiedDialogMenuItem = nullptr;
    }
    if (gPluginPanelExportDialogPackageMenuItem != nullptr) {
        AVMenuItemRemove(gPluginPanelExportDialogPackageMenuItem);
        gPluginPanelExportDialogPackageMenuItem = nullptr;
    }
    if (gPluginPanelSheetA3MenuItem != nullptr) {
        AVMenuItemRemove(gPluginPanelSheetA3MenuItem);
        gPluginPanelSheetA3MenuItem = nullptr;
    }
    if (gPluginPanelSheetA4MenuItem != nullptr) {
        AVMenuItemRemove(gPluginPanelSheetA4MenuItem);
        gPluginPanelSheetA4MenuItem = nullptr;
    }
    if (gPluginPanelToggleQualityGateMenuItem != nullptr) {
        AVMenuItemRemove(gPluginPanelToggleQualityGateMenuItem);
        gPluginPanelToggleQualityGateMenuItem = nullptr;
    }
    if (gPluginPanelApplyStateMenuItem != nullptr) {
        AVMenuItemRemove(gPluginPanelApplyStateMenuItem);
        gPluginPanelApplyStateMenuItem = nullptr;
    }
    if (gPluginPanelShowStateMenuItem != nullptr) {
        AVMenuItemRemove(gPluginPanelShowStateMenuItem);
        gPluginPanelShowStateMenuItem = nullptr;
    }
    if (gPluginPanelSetOutputTempMenuItem != nullptr) {
        AVMenuItemRemove(gPluginPanelSetOutputTempMenuItem);
        gPluginPanelSetOutputTempMenuItem = nullptr;
    }
    if (gPluginPanelTogglePrepressMenuItem != nullptr) {
        AVMenuItemRemove(gPluginPanelTogglePrepressMenuItem);
        gPluginPanelTogglePrepressMenuItem = nullptr;
    }
    if (gPluginPanelCycleLayoutMenuItem != nullptr) {
        AVMenuItemRemove(gPluginPanelCycleLayoutMenuItem);
        gPluginPanelCycleLayoutMenuItem = nullptr;
    }
    if (gPluginPresetQuickConfigMenuItem != nullptr) {
        AVMenuItemRemove(gPluginPresetQuickConfigMenuItem);
        gPluginPresetQuickConfigMenuItem = nullptr;
    }
    if (gPluginPresetValidateMenuItem != nullptr) {
        AVMenuItemRemove(gPluginPresetValidateMenuItem);
        gPluginPresetValidateMenuItem = nullptr;
    }
    if (gPluginPresetRunMenuItem != nullptr) {
        AVMenuItemRemove(gPluginPresetRunMenuItem);
        gPluginPresetRunMenuItem = nullptr;
    }
    if (gPluginPresetPreviewMenuItem != nullptr) {
        AVMenuItemRemove(gPluginPresetPreviewMenuItem);
        gPluginPresetPreviewMenuItem = nullptr;
    }
    if (gPluginPresetSaveMenuItem != nullptr) {
        AVMenuItemRemove(gPluginPresetSaveMenuItem);
        gPluginPresetSaveMenuItem = nullptr;
    }
    if (gPluginReportMenuItem != nullptr) {
        AVMenuItemRemove(gPluginReportMenuItem);
        gPluginReportMenuItem = nullptr;
    }
    if (gPluginMenuItem != nullptr) {
        AVMenuItemRemove(gPluginMenuItem);
        gPluginMenuItem = nullptr;
    }
    if (gPluginSubMenu != nullptr) {
        AVMenuRelease(gPluginSubMenu);
        gPluginSubMenu = nullptr;
    }
    return true;
}

extern "C" ACCB1 const char* ACCB2 GetExtensionName(void) {
    return kExtensionName;
}

extern "C" ACCB1 ASBool ACCB2 PIHandshake(ASUns32 handshakeVersion, void *handshakeData) {
    if (handshakeVersion == HANDSHAKE_V0200) {
        PIHandshakeData_V0200 *hsData = (PIHandshakeData_V0200 *)handshakeData;
        hsData->extensionName = ASAtomFromString(GetExtensionName());
        hsData->exportHFTsCallback =
            (void*)ASCallbackCreateProto(PIExportHFTsProcType, &PluginExportHFTs);
        hsData->importReplaceAndRegisterCallback =
            (void*)ASCallbackCreateProto(PIImportReplaceAndRegisterProcType, &PluginImportReplaceAndRegister);
        hsData->initCallback =
            (void*)ASCallbackCreateProto(PIInitProcType, &PluginInit);
        hsData->unloadCallback =
            (void*)ASCallbackCreateProto(PIUnloadProcType, &PluginUnload);
        return true;
    }
    return false;
}
