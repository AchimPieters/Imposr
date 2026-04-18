#include "aimp/ImpositionPlan.h"
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

AVMenuItem gPluginMenuItem = nullptr;
AVMenuItem gPluginReportMenuItem = nullptr;
AVMenuItem gPluginPresetSaveMenuItem = nullptr;
AVMenuItem gPluginPresetPreviewMenuItem = nullptr;
AVMenuItem gPluginPresetRunMenuItem = nullptr;
AVMenuItem gPluginPresetValidateMenuItem = nullptr;
AVMenuItem gPluginPresetQuickConfigMenuItem = nullptr;
AVMenu gPluginSubMenu = nullptr;
ASCallback gMenuExecuteProc = nullptr;
ASCallback gReportExecuteProc = nullptr;
ASCallback gPresetSaveProc = nullptr;
ASCallback gPresetPreviewProc = nullptr;
ASCallback gPresetRunProc = nullptr;
ASCallback gPresetValidateProc = nullptr;
ASCallback gPresetQuickConfigProc = nullptr;

constexpr const char* kExtensionName = "AcrobatImpositionPlugin";
constexpr const char* kPluginMenuTitle = "Acrobat Imposition Plugin";
constexpr const char* kMenuItemTitle = "2-Up Demo";
constexpr const char* kMenuItemReportTitle = "2-Up Report PDF";
constexpr const char* kMenuItemPresetSaveTitle = "Preset: Save default";
constexpr const char* kMenuItemPresetPreviewTitle = "Preset: Preview proof";
constexpr const char* kMenuItemPresetRunTitle = "Preset: Run bundle";
constexpr const char* kMenuItemPresetValidateTitle = "Preset: Validate active job";
constexpr const char* kMenuItemPresetQuickConfigTitle = "Preset: Quick configure";

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
                                const std::string& bundlePath) {
    std::ostringstream out;
    out << "{\n";
    out << "  \"kind\": \"acrobat-imposition-panel-state\",\n";
    out << "  \"mode\": \"" << modeLabel << "\",\n";
    out << "  \"sheet\": {\"widthPoints\": " << preset.sheetSize.widthPoints
        << ", \"heightPoints\": " << preset.sheetSize.heightPoints << "},\n";
    out << "  \"preset\": {\n";
    out << "    \"outputDirectory\": \"" << preset.outputDirectory << "\",\n";
    out << "    \"outputStem\": \"" << preset.outputStem << "\",\n";
    out << "    \"failOnValidationIssues\": " << (preset.pdfOptions.failOnValidationIssues ? "true" : "false") << ",\n";
    out << "    \"failOnPreflightErrors\": " << (preset.pdfOptions.failOnPreflightErrors ? "true" : "false") << "\n";
    out << "  },\n";
    out << "  \"validation\": {\n";
    out << "    \"validationIssueCount\": " << validationIssueCount << ",\n";
    out << "    \"preflightIssueCount\": " << preflightIssueCount << ",\n";
    out << "    \"preflightErrorCount\": " << preflightErrorCount << ",\n";
    out << "    \"status\": \"" << ((validationIssueCount == 0 && preflightErrorCount == 0) ? "ready" : "blocked") << "\"\n";
    out << "  },\n";
    out << "  \"bundlePath\": \"" << bundlePath << "\"\n";
    out << "}\n";
    return out.str();
}

struct SdkPlacementOp {
    std::uint32_t sheetIndex {0};
    std::uint32_t slotIndex {0};
    std::uint32_t sourcePageIndex {aimp::kBlankPageIndex};
    bool isBlank {true};
    double rotationDegrees {0.0};
    double ctmA {1.0};
    double ctmB {0.0};
    double ctmC {0.0};
    double ctmD {1.0};
    double ctmE {0.0};
    double ctmF {0.0};
    double targetX {0.0};
    double targetY {0.0};
    double targetWidth {0.0};
    double targetHeight {0.0};
};

bool TryExtractUIntAfter(const std::string& line,
                        const std::string& token,
                        std::uint32_t& outValue) {
    const std::size_t pos = line.find(token);
    if (pos == std::string::npos) {
        return false;
    }
    const std::size_t begin = pos + token.size();
    std::size_t end = begin;
    while (end < line.size() && line[end] >= '0' && line[end] <= '9') {
        ++end;
    }
    if (end == begin) {
        return false;
    }
    outValue = static_cast<std::uint32_t>(std::stoul(line.substr(begin, end - begin)));
    return true;
}

bool TryExtractDoubleAfter(const std::string& line,
                           const std::string& token,
                           double& outValue) {
    const std::size_t pos = line.find(token);
    if (pos == std::string::npos) {
        return false;
    }
    const std::size_t begin = pos + token.size();
    std::size_t end = begin;
    if (end < line.size() && (line[end] == '-' || line[end] == '+')) {
        ++end;
    }
    bool seenDigit = false;
    while (end < line.size()) {
        const char ch = line[end];
        if ((ch >= '0' && ch <= '9') || ch == '.') {
            seenDigit = true;
            ++end;
            continue;
        }
        break;
    }
    if (!seenDigit) {
        return false;
    }
    outValue = std::stod(line.substr(begin, end - begin));
    return true;
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
                         std::vector<SdkPlacementOp>& outOps,
                         std::string& errorMessage) {
    outOps.clear();
    std::ifstream in(path);
    if (!in) {
        errorMessage = "Could not open sdk-ops file";
        return false;
    }

    std::string line;
    while (std::getline(in, line)) {
        if (line.find("\"op\": \"place-page\"") == std::string::npos) {
            continue;
        }
        SdkPlacementOp op {};
        if (!TryExtractUIntAfter(line, "\"sheetIndex\": ", op.sheetIndex) ||
            !TryExtractUIntAfter(line, "\"slotIndex\": ", op.slotIndex) ||
            !TryExtractUIntAfter(line, "\"pageIndex\": ", op.sourcePageIndex)) {
            errorMessage = "Invalid sdk-ops place-page row";
            return false;
        }
        TryExtractDoubleAfter(line, "\"rotationDegrees\": ", op.rotationDegrees);
        TryExtractDoubleAfter(line, "\"a\": ", op.ctmA);
        TryExtractDoubleAfter(line, "\"b\": ", op.ctmB);
        TryExtractDoubleAfter(line, "\"c\": ", op.ctmC);
        TryExtractDoubleAfter(line, "\"d\": ", op.ctmD);
        TryExtractDoubleAfter(line, "\"e\": ", op.ctmE);
        TryExtractDoubleAfter(line, "\"f\": ", op.ctmF);
        TryExtractDoubleAfter(line, "\"x\": ", op.targetX);
        TryExtractDoubleAfter(line, "\"y\": ", op.targetY);
        TryExtractDoubleAfter(line, "\"width\": ", op.targetWidth);
        TryExtractDoubleAfter(line, "\"height\": ", op.targetHeight);
        op.isBlank = line.find("\"isBlank\": true") != std::string::npos ||
                     op.sourcePageIndex == aimp::kBlankPageIndex;
        outOps.push_back(op);
    }

    if (outOps.empty()) {
        errorMessage = "sdk-ops does not contain place-page operations";
        return false;
    }
    return true;
}

bool TryRunExperimentalSdkComposer(PDDoc sourceDoc,
                                   const std::string& sdkOpsPath,
                                   const std::string& outputPdfPath,
                                   std::string& errorMessage) {
    std::vector<SdkPlacementOp> ops;
    if (!LoadSdkPlacementOps(sdkOpsPath, ops, errorMessage)) {
        return false;
    }

#if defined(AIMP_ENABLE_EXPERIMENTAL_SDK_COMPOSER)
    std::sort(ops.begin(), ops.end(), [](const SdkPlacementOp& a, const SdkPlacementOp& b) {
        if (a.sheetIndex != b.sheetIndex) {
            return a.sheetIndex < b.sheetIndex;
        }
        return a.slotIndex < b.slotIndex;
    });

    // Experimental first-pass native SDK adapter:
    // consumes sdk-ops.json and applies placement order + transform metadata.
    // Rotation is currently applied directly; CTM fields are parsed and attached
    // to the operation loop for upcoming matrix/XObject placement parity work.
    PDDoc outDoc = PDDocCreate();
    if (outDoc == nullptr) {
        errorMessage = "PDDocCreate failed";
        return false;
    }

    for (const auto& op : ops) {
        if (op.isBlank) {
            continue;
        }
        const ASInt32 insertAfter = PDDocGetNumPages(outDoc) - 1;
        const ASBool inserted = PDDocInsertPages(outDoc,
                                                 insertAfter,
                                                 sourceDoc,
                                                 static_cast<ASInt32>(op.sourcePageIndex),
                                                 1,
                                                 0,
                                                 nullptr,
                                                 nullptr,
                                                 nullptr,
                                                 nullptr);
        if (!inserted) {
            errorMessage = "PDDocInsertPages failed while consuming sdk-ops";
            PDDocClose(outDoc);
            return false;
        }
        const ASInt32 newPageIndex = PDDocGetNumPages(outDoc) - 1;
        PDPage outPage = PDDocAcquirePage(outDoc, newPageIndex);
        if (outPage != nullptr) {
            const ASInt32 rotation = NormalizeRotationDegrees(op.rotationDegrees);
            PDPageSetRotate(outPage, rotation);

            // Experimental CTM parity step:
            // prefer explicit targetRect from sdk-ops (planner-authoritative geometry),
            // fallback to CTM-derived width/height for compatibility with older manifests.
            double width = op.targetWidth;
            double height = op.targetHeight;
            double left = op.targetX;
            double bottom = op.targetY;
            if (!(std::isfinite(width) && std::isfinite(height) && width > 0.0 && height > 0.0)) {
                width = std::sqrt((op.ctmA * op.ctmA) + (op.ctmC * op.ctmC));
                height = std::sqrt((op.ctmB * op.ctmB) + (op.ctmD * op.ctmD));
                left = op.ctmE;
                bottom = op.ctmF;
            }
            ASFixedRect destRect {};
            destRect.left = ASFloatToFixed(static_cast<ASReal>(left));
            destRect.bottom = ASFloatToFixed(static_cast<ASReal>(bottom));
            destRect.right = ASFloatToFixed(static_cast<ASReal>(left + width));
            destRect.top = ASFloatToFixed(static_cast<ASReal>(bottom + height));
            PDPageSetCropBox(outPage, &destRect);
            PDPageSetMediaBox(outPage, &destRect);
            PDPageRelease(outPage);
        }
    }

    const ASFileSys fileSys = ASGetDefaultFileSys();
    ASPathName outPath = ASFileSysCreatePathName(fileSys, ASAtomFromString("Cstring"), outputPdfPath.c_str(), nullptr);
    if (outPath == nullptr) {
        errorMessage = "Could not create output path";
        PDDocClose(outDoc);
        return false;
    }
    const ASBool saved = PDDocSave(outDoc, PDSaveFull | PDSaveCollectGarbage, outPath, nullptr, nullptr, nullptr);
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
    errorMessage = "Experimental SDK composer is disabled. Enable -DAIMP_ENABLE_EXPERIMENTAL_SDK_COMPOSER=ON.";
    return false;
#endif
}

bool RegisterMenus() {
    AVMenubar menubar = AVAppGetMenubar();
    if (menubar == nullptr) {
        return false;
    }

    gPluginSubMenu = AVMenuNew(kPluginMenuTitle, kExtensionName, nullptr, true);
    if (gPluginSubMenu == nullptr) {
        return false;
    }

    AVMenuItem submenuItem = AVMenuItemNew(kPluginMenuTitle, kExtensionName, gPluginSubMenu, false, NO_SHORTCUT, 0, nullptr, nullptr);
    if (submenuItem == nullptr) {
        return false;
    }

    AVMenuAddMenuItem(menubar, submenuItem, APPEND_MENUITEM);

    gMenuExecuteProc = ASCallbackCreateProto(AVExecuteProc, ExecuteTwoUpDemo);
    gPluginMenuItem = AVMenuItemNew(
        kMenuItemTitle,
        "AIMP:TwoUpDemo",
        nullptr,
        true,
        NO_SHORTCUT,
        0,
        gMenuExecuteProc,
        nullptr
    );

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
        gReportExecuteProc,
        nullptr
    );

    if (gPluginReportMenuItem == nullptr) {
        return false;
    }
    AVMenuAddMenuItem(gPluginSubMenu, gPluginReportMenuItem, APPEND_MENUITEM);

    gPresetSaveProc = ASCallbackCreateProto(AVExecuteProc, ExecutePresetSave);
    gPluginPresetSaveMenuItem = AVMenuItemNew(
        kMenuItemPresetSaveTitle,
        "AIMP:PresetSave",
        nullptr,
        true,
        NO_SHORTCUT,
        0,
        gPresetSaveProc,
        nullptr
    );
    if (gPluginPresetSaveMenuItem == nullptr) {
        return false;
    }
    AVMenuAddMenuItem(gPluginSubMenu, gPluginPresetSaveMenuItem, APPEND_MENUITEM);

    gPresetPreviewProc = ASCallbackCreateProto(AVExecuteProc, ExecutePresetPreview);
    gPluginPresetPreviewMenuItem = AVMenuItemNew(
        kMenuItemPresetPreviewTitle,
        "AIMP:PresetPreview",
        nullptr,
        true,
        NO_SHORTCUT,
        0,
        gPresetPreviewProc,
        nullptr
    );
    if (gPluginPresetPreviewMenuItem == nullptr) {
        return false;
    }
    AVMenuAddMenuItem(gPluginSubMenu, gPluginPresetPreviewMenuItem, APPEND_MENUITEM);

    gPresetRunProc = ASCallbackCreateProto(AVExecuteProc, ExecutePresetRunBundle);
    gPluginPresetRunMenuItem = AVMenuItemNew(
        kMenuItemPresetRunTitle,
        "AIMP:PresetRunBundle",
        nullptr,
        true,
        NO_SHORTCUT,
        0,
        gPresetRunProc,
        nullptr
    );
    if (gPluginPresetRunMenuItem == nullptr) {
        return false;
    }
    AVMenuAddMenuItem(gPluginSubMenu, gPluginPresetRunMenuItem, APPEND_MENUITEM);

    gPresetValidateProc = ASCallbackCreateProto(AVExecuteProc, ExecutePresetValidate);
    gPluginPresetValidateMenuItem = AVMenuItemNew(
        kMenuItemPresetValidateTitle,
        "AIMP:PresetValidate",
        nullptr,
        true,
        NO_SHORTCUT,
        0,
        gPresetValidateProc,
        nullptr
    );
    if (gPluginPresetValidateMenuItem == nullptr) {
        return false;
    }
    AVMenuAddMenuItem(gPluginSubMenu, gPluginPresetValidateMenuItem, APPEND_MENUITEM);

    gPresetQuickConfigProc = ASCallbackCreateProto(AVExecuteProc, ExecutePresetQuickConfigure);
    gPluginPresetQuickConfigMenuItem = AVMenuItemNew(
        kMenuItemPresetQuickConfigTitle,
        "AIMP:PresetQuickConfigure",
        nullptr,
        true,
        NO_SHORTCUT,
        0,
        gPresetQuickConfigProc,
        nullptr
    );
    if (gPluginPresetQuickConfigMenuItem == nullptr) {
        return false;
    }
    AVMenuAddMenuItem(gPluginSubMenu, gPluginPresetQuickConfigMenuItem, APPEND_MENUITEM);

    return true;
}

ACCB1 void ACCB2 ExecuteTwoUpDemo(void* clientData) {
    DURING
        AVDoc activeDoc = AVAppGetActiveDoc();
        if (activeDoc == nullptr) {
            ShowInfoDialog("Open eerst een PDF in Acrobat.");
            E_RETURN_VOID;
        }

        PDDoc pdDoc = AVDocGetPDDoc(activeDoc);
        if (pdDoc == nullptr) {
            ShowInfoDialog("Geen geldig PDDoc beschikbaar.");
            E_RETURN_VOID;
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
            E_RETURN_VOID;
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
            E_RETURN_VOID;
        }
        PDDoc pdDoc = AVDocGetPDDoc(activeDoc);
        if (pdDoc == nullptr) {
            ShowInfoDialog("Geen geldig PDDoc beschikbaar.");
            E_RETURN_VOID;
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
            E_RETURN_VOID;
        }

        std::error_code fsError;
        const auto tempDir = std::filesystem::temp_directory_path(fsError);
        if (fsError) {
            ShowInfoDialog("Kan temp map niet bepalen.");
            E_RETURN_VOID;
        }

        const auto previewPath = (tempDir / ("acrobat-imposition-preview-" + BuildUtcTimestamp() + ".pdf")).string();
        if (!aimp::ComposePlanPdf(plan, previewPath, preset.pdfOptions, error)) {
            ShowInfoDialog("Kon preview PDF niet maken: " + error);
            E_RETURN_VOID;
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
            E_RETURN_VOID;
        }
        PDDoc pdDoc = AVDocGetPDDoc(activeDoc);
        if (pdDoc == nullptr) {
            ShowInfoDialog("Geen geldig PDDoc beschikbaar.");
            E_RETURN_VOID;
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
            E_RETURN_VOID;
        }
        const auto validationIssues = aimp::ValidatePlan(plan);
        if (preset.pdfOptions.failOnValidationIssues && !validationIssues.empty()) {
            std::ostringstream message;
            message << "Validatie gefaald (" << validationIssues.size() << " issues):\n";
            for (const auto& issue : validationIssues) {
                message << "- " << issue.code << ": " << issue.message << '\n';
            }
            ShowInfoDialog(message.str());
            E_RETURN_VOID;
        }

        std::error_code fsError;
        const auto tempDir = std::filesystem::temp_directory_path(fsError);
        if (fsError) {
            ShowInfoDialog("Kan temp map niet bepalen.");
            E_RETURN_VOID;
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
            E_RETURN_VOID;
        }

        const auto planPath = (base / "plan.json").string();
        const auto manifestPath = (base / "manifest.json").string();
        const auto auditPath = (base / "audit.xml").string();
        const auto acrobatJsPath = (base / "placement.js").string();
        const auto sdkOpsPath = (base / "sdk-ops.json").string();
        const auto compositionPath = (base / "production-composition.json").string();
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
            std::ofstream out(sdkOpsPath);
            out << aimp::ToAcrobatSdkOpsJson(plan);
        }
        {
            std::ofstream out(compositionPath);
            out << aimp::ToProductionCompositionJson(plan, preset.buildOptions, preset.pdfOptions);
        }
        if (!aimp::ComposePlanPdf(plan, proofPath, preset.pdfOptions, error)) {
            ShowInfoDialog("Kon proof PDF niet maken: " + error);
            E_RETURN_VOID;
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
                E_RETURN_VOID;
            }
            std::ofstream out(preflightPath);
            out << aimp::ToPreflightJson(preflight);
            std::ofstream panelOut(panelStatePath);
            panelOut << BuildPanelStateJson(modeLabel,
                                            preset,
                                            validationIssues.size(),
                                            preflight.size(),
                                            preflightErrorCount,
                                            base.string());
        }

        AVDoc proofDoc = AVDocOpenFromFile(proofPath.c_str(), "Acrobat Imposition Proof");
        if (proofDoc == nullptr) {
            ShowInfoDialog("Bundle gemaakt, maar proof PDF kon niet automatisch worden geopend.\n" + proofPath);
            E_RETURN_VOID;
        }

        std::string sdkComposeError;
        if (!TryRunExperimentalSdkComposer(pdDoc, sdkOpsPath, imposedOutputPath, sdkComposeError)) {
            ShowInfoDialog("Run bundle gereed; native SDK composer niet uitgevoerd:\n" + sdkComposeError);
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
            E_RETURN_VOID;
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

ACCB1 void ACCB2 ExecutePresetValidate(void* clientData) {
    DURING
        AVDoc activeDoc = AVAppGetActiveDoc();
        if (activeDoc == nullptr) {
            ShowInfoDialog("Open eerst een PDF in Acrobat.");
            E_RETURN_VOID;
        }
        PDDoc pdDoc = AVDocGetPDDoc(activeDoc);
        if (pdDoc == nullptr) {
            ShowInfoDialog("Geen geldig PDDoc beschikbaar.");
            E_RETURN_VOID;
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
            E_RETURN_VOID;
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
        std::error_code fsError;
        const auto tempDir = std::filesystem::temp_directory_path(fsError);
        if (!fsError) {
            const auto statePath = (tempDir / "acrobat-imposition-panel-state.json").string();
            std::ofstream out(statePath);
            out << BuildPanelStateJson(modeLabel,
                                       preset,
                                       validationIssues.size(),
                                       preflightIssues.size(),
                                       preflightErrorCount,
                                       "");
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
            E_RETURN_VOID;
        }

        PDDoc pdDoc = AVDocGetPDDoc(activeDoc);
        if (pdDoc == nullptr) {
            ShowInfoDialog("Geen geldig PDDoc beschikbaar.");
            E_RETURN_VOID;
        }

        const ASInt32 pageCount = PDDocGetNumPages(pdDoc);
        const aimp::SheetSize outputSheet {1190.55, 841.89};
        const auto plan = aimp::TwoUpPlanner::Build("active-document", static_cast<std::uint32_t>(pageCount), outputSheet);

        std::error_code fsError;
        const auto tempDir = std::filesystem::temp_directory_path(fsError);
        if (fsError) {
            ShowInfoDialog("Kan temp map niet bepalen voor report PDF.");
            E_RETURN_VOID;
        }
        const auto reportPath = (tempDir / "acrobat-imposition-two-up-report.pdf").string();
        std::string error;
        if (!aimp::ComposePlanPdf(plan, reportPath, error)) {
            ShowInfoDialog("Kon geen report PDF maken: " + error);
            E_RETURN_VOID;
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
