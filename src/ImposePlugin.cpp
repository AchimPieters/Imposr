#include "aimp/ImpositionPlan.h"
#include "aimp/PdfComposer.h"
#include "aimp/Preset.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>

// Acrobat SDK headers.
// These must resolve through ACROBAT_SDK_DIR include paths.
#include "PIHeaders.h"

namespace {

ACCB1 void ACCB2 ExecuteTwoUpDemo(void* clientData);
ACCB1 void ACCB2 ExecuteTwoUpReportExport(void* clientData);
ACCB1 void ACCB2 ExecutePresetSave(void* clientData);
ACCB1 void ACCB2 ExecutePresetPreview(void* clientData);
ACCB1 void ACCB2 ExecutePresetRunBundle(void* clientData);

AVMenuItem gPluginMenuItem = nullptr;
AVMenuItem gPluginReportMenuItem = nullptr;
AVMenuItem gPluginPresetSaveMenuItem = nullptr;
AVMenuItem gPluginPresetPreviewMenuItem = nullptr;
AVMenuItem gPluginPresetRunMenuItem = nullptr;
AVMenu gPluginSubMenu = nullptr;
ASCallback gMenuExecuteProc = nullptr;
ASCallback gReportExecuteProc = nullptr;
ASCallback gPresetSaveProc = nullptr;
ASCallback gPresetPreviewProc = nullptr;
ASCallback gPresetRunProc = nullptr;

constexpr const char* kExtensionName = "AcrobatImpositionPlugin";
constexpr const char* kPluginMenuTitle = "Acrobat Imposition Plugin";
constexpr const char* kMenuItemTitle = "2-Up Demo";
constexpr const char* kMenuItemReportTitle = "2-Up Report PDF";
constexpr const char* kMenuItemPresetSaveTitle = "Preset: Save default";
constexpr const char* kMenuItemPresetPreviewTitle = "Preset: Preview proof";
constexpr const char* kMenuItemPresetRunTitle = "Preset: Run bundle";

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
    return true;
}

void ShowInfoDialog(const std::string& message) {
    AVAlertNote(message.c_str());
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

        std::error_code fsError;
        const auto tempDir = std::filesystem::temp_directory_path(fsError);
        if (fsError) {
            ShowInfoDialog("Kan temp map niet bepalen.");
            E_RETURN_VOID;
        }
        const std::string stamp = BuildUtcTimestamp();
        const auto base = tempDir / ("acrobat-imposition-run-" + stamp);
        std::filesystem::create_directories(base, fsError);
        if (fsError) {
            ShowInfoDialog("Kan output map niet maken.");
            E_RETURN_VOID;
        }

        const auto planPath = (base / "plan.json").string();
        const auto manifestPath = (base / "manifest.json").string();
        const auto auditPath = (base / "audit.xml").string();
        const auto acrobatJsPath = (base / "placement.js").string();
        const auto compositionPath = (base / "production-composition.json").string();
        const auto proofPath = (base / "proof.pdf").string();
        const auto preflightPath = (base / "preflight.json").string();

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
            std::ofstream out(compositionPath);
            out << aimp::ToProductionCompositionJson(plan, preset.buildOptions, preset.pdfOptions);
        }
        if (!aimp::ComposePlanPdf(plan, proofPath, preset.pdfOptions, error)) {
            ShowInfoDialog("Kon proof PDF niet maken: " + error);
            E_RETURN_VOID;
        }
        {
            const auto preflight = aimp::ValidatePrepressReadiness(plan, preset.pdfOptions);
            std::ofstream out(preflightPath);
            out << "{\n  \"issues\": " << preflight.size() << "\n}\n";
        }

        ShowInfoDialog("Preset-run bundle klaar (" + modeLabel + "):\n" + base.string());
    HANDLER
        ShowInfoDialog("Er trad een fout op bij preset-run.");
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
