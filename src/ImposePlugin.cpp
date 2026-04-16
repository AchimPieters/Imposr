#include "aimp/ImpositionPlan.h"
#include "aimp/PdfComposer.h"

#include <filesystem>
#include <string>

// Acrobat SDK headers.
// These must resolve through ACROBAT_SDK_DIR include paths.
#include "PIHeaders.h"

namespace {

ACCB1 void ACCB2 ExecuteTwoUpDemo(void* clientData);
ACCB1 void ACCB2 ExecuteTwoUpReportExport(void* clientData);

AVMenuItem gPluginMenuItem = nullptr;
AVMenuItem gPluginReportMenuItem = nullptr;
AVMenu gPluginSubMenu = nullptr;
ASCallback gMenuExecuteProc = nullptr;
ASCallback gReportExecuteProc = nullptr;

constexpr const char* kExtensionName = "AcrobatImpositionPlugin";
constexpr const char* kPluginMenuTitle = "Acrobat Imposition Plugin";
constexpr const char* kMenuItemTitle = "2-Up Demo";
constexpr const char* kMenuItemReportTitle = "2-Up Report PDF";

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

        const auto reportPath = (std::filesystem::temp_directory_path() / "acrobat-imposition-two-up-report.pdf").string();
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
