// ImposePlugin.cpp — Acrobat imposition plug-in (macOS + Windows)
// Compiled as Objective-C++ on macOS so AppKit/Foundation types resolve.

#include "aimp/ImpositionPlan.h"
#include "aimp/PageTools.h"
#include "aimp/PanelState.h"
#include "aimp/PdfComposer.h"
#include "aimp/Preset.h"
#include "aimp/SampleDocument.h"
#include "aimp/Shuffle.h"
#include "aimp/TilePages.h"
#include "aimp/TrimShift.h"
#include "aimp/PrinterMarks.h"
#include "aimp/StickOn.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

#include "PIHeaders.h"

#ifdef MAC_PLATFORM
#import <AppKit/AppKit.h>
#import <Foundation/Foundation.h>
#endif

// ── Forward declarations ──────────────────────────────────────────────────────

namespace {

ACCB1 void ACCB2 ExecuteControlPanel(void*);
ACCB1 void ACCB2 ExecuteBooklet(void*);
ACCB1 void ACCB2 ExecuteNUpPages(void*);
ACCB1 void ACCB2 ExecuteStepAndRepeat(void*);
ACCB1 void ACCB2 ExecuteJoin2Pages(void*);
ACCB1 void ACCB2 ExecuteShufflePages(void*);
ACCB1 void ACCB2 ExecuteShuffleEvenOdd(void*);
ACCB1 void ACCB2 ExecuteReversePages(void*);
ACCB1 void ACCB2 ExecuteInsertPages(void*);
ACCB1 void ACCB2 ExecuteTrimShift(void*);
ACCB1 void ACCB2 ExecuteTilePages(void*);
ACCB1 void ACCB2 ExecutePageSizes(void*);
ACCB1 void ACCB2 ExecutePageTools(void*);
ACCB1 void ACCB2 ExecuteCreep(void*);
ACCB1 void ACCB2 ExecuteSampleDocument(void*);
ACCB1 void ACCB2 ExecuteMonitor(void*);
ACCB1 void ACCB2 ExecuteStickOnText(void*);
ACCB1 void ACCB2 ExecuteStickOnPdf(void*);
ACCB1 void ACCB2 ExecuteStickOnMasking(void*);
ACCB1 void ACCB2 ExecutePeelOffText(void*);
ACCB1 void ACCB2 ExecutePeelOffMasking(void*);
ACCB1 void ACCB2 ExecuteRegistrationMarks(void*);
ACCB1 void ACCB2 ExecuteSequences(void*);
ACCB1 void ACCB2 ExecutePlayback(void*);
ACCB1 void ACCB2 ExecuteRememberLast(void*);
ACCB1 void ACCB2 ExecutePreferences(void*);
ACCB1 void ACCB2 ExecuteCustomizePanel(void*);
ACCB1 void ACCB2 ExecuteHelpAbout(void*);

} // forward namespace close

// ── macOS Control Panel delegate ──────────────────────────────────────────────

#ifdef MAC_PLATFORM
@interface ImposeControlPanelDelegate : NSObject
- (void)onBooklet:(id)sender;
- (void)onNUp:(id)sender;
- (void)onStepRepeat:(id)sender;
- (void)onJoin2Pages:(id)sender;
- (void)onShufflePages:(id)sender;
- (void)onReversePages:(id)sender;
- (void)onTrimShift:(id)sender;
- (void)onInsertPages:(id)sender;
- (void)onTilePages:(id)sender;
- (void)onPageTools:(id)sender;
- (void)onStickOnText:(id)sender;
- (void)onStickOnPdf:(id)sender;
- (void)onMaskingTape:(id)sender;
- (void)onRemoveMasking:(id)sender;
- (void)onRememberLast:(id)sender;
- (void)onPlayback:(id)sender;
- (void)onPreferences:(id)sender;
@end

@implementation ImposeControlPanelDelegate
- (void)onBooklet:(id)sender     { ExecuteBooklet(nil); }
- (void)onNUp:(id)sender         { ExecuteNUpPages(nil); }
- (void)onStepRepeat:(id)sender  { ExecuteStepAndRepeat(nil); }
- (void)onJoin2Pages:(id)sender  { ExecuteJoin2Pages(nil); }
- (void)onShufflePages:(id)sender{ ExecuteShufflePages(nil); }
- (void)onReversePages:(id)sender{ ExecuteReversePages(nil); }
- (void)onTrimShift:(id)sender   { ExecuteTrimShift(nil); }
- (void)onInsertPages:(id)sender { ExecuteInsertPages(nil); }
- (void)onTilePages:(id)sender   { ExecuteTilePages(nil); }
- (void)onPageTools:(id)sender   { ExecutePageTools(nil); }
- (void)onStickOnText:(id)sender { ExecuteStickOnText(nil); }
- (void)onStickOnPdf:(id)sender  { ExecuteStickOnPdf(nil); }
- (void)onMaskingTape:(id)sender { ExecuteStickOnMasking(nil); }
- (void)onRemoveMasking:(id)sender{ExecutePeelOffMasking(nil); }
- (void)onRememberLast:(id)sender{ ExecuteRememberLast(nil); }
- (void)onPlayback:(id)sender    { ExecutePlayback(nil); }
- (void)onPreferences:(id)sender { ExecutePreferences(nil); }
@end

static ImposeControlPanelDelegate* gPanelDelegate = nil;
static NSPanel* gControlPanel = nil;

static NSButton* MakePanelButton(NSString* title, id target, SEL action, NSRect frame) {
    NSButton* btn = [[NSButton alloc] initWithFrame:frame];
    [btn setTitle:title];
    [btn setBezelStyle:NSBezelStyleRounded];
    [btn setTarget:target];
    [btn setAction:action];
    return btn;
}

static NSBox* MakeGroup(NSString* title, NSRect frame) {
    NSBox* box = [[NSBox alloc] initWithFrame:frame];
    [box setTitle:title];
    [box setTitlePosition:NSAtTop];
    [box setBoxType:NSBoxPrimary];
    return box;
}

static void BuildControlPanelContent(NSView* tabView, ImposeControlPanelDelegate* del) {
    const CGFloat BW = 100, BH = 24, BG = 4;
    const CGFloat GW = 230, GM = 8;

    // Easy Imposition group
    NSBox* grpEasy = MakeGroup(@"Easy Imposition", NSMakeRect(GM, 470, GW, 100));
    [grpEasy addSubview:MakePanelButton(@"Booklet", del, @selector(onBooklet:),
        NSMakeRect(GM, 44, BW, BH))];
    [grpEasy addSubview:MakePanelButton(@"N-up pages", del, @selector(onNUp:),
        NSMakeRect(BW+GM+BG, 44, BW, BH))];
    [grpEasy addSubview:MakePanelButton(@"Step & repeat", del, @selector(onStepRepeat:),
        NSMakeRect(GM, 14, BW, BH))];
    [grpEasy addSubview:MakePanelButton(@"Join 2 pages", del, @selector(onJoin2Pages:),
        NSMakeRect(BW+GM+BG, 14, BW, BH))];

    // Page Management group
    NSBox* grpPages = MakeGroup(@"Page Management", NSMakeRect(GM, 310, GW, 150));
    [grpPages addSubview:MakePanelButton(@"Shuffle pages", del, @selector(onShufflePages:),
        NSMakeRect(GM, 104, BW, BH))];
    [grpPages addSubview:MakePanelButton(@"Reverse pages", del, @selector(onReversePages:),
        NSMakeRect(BW+GM+BG, 104, BW, BH))];
    [grpPages addSubview:MakePanelButton(@"Trim & shift", del, @selector(onTrimShift:),
        NSMakeRect(GM, 74, BW, BH))];
    [grpPages addSubview:MakePanelButton(@"Insert pages", del, @selector(onInsertPages:),
        NSMakeRect(BW+GM+BG, 74, BW, BH))];
    [grpPages addSubview:MakePanelButton(@"Tile pages", del, @selector(onTilePages:),
        NSMakeRect(GM, 44, BW, BH))];
    [grpPages addSubview:MakePanelButton(@"Page tools", del, @selector(onPageTools:),
        NSMakeRect(BW+GM+BG, 44, BW, BH))];

    // Stick On group
    NSBox* grpStick = MakeGroup(@"Stick On", NSMakeRect(GM, 200, GW, 100));
    [grpStick addSubview:MakePanelButton(@"Text & numbers", del, @selector(onStickOnText:),
        NSMakeRect(GM, 44, BW, BH))];
    [grpStick addSubview:MakePanelButton(@"PDF pages", del, @selector(onStickOnPdf:),
        NSMakeRect(BW+GM+BG, 44, BW, BH))];
    [grpStick addSubview:MakePanelButton(@"Masking tape", del, @selector(onMaskingTape:),
        NSMakeRect(GM, 14, BW, BH))];
    [grpStick addSubview:MakePanelButton(@"Remove overlays", del, @selector(onRemoveMasking:),
        NSMakeRect(BW+GM+BG, 14, BW, BH))];

    // Memory group
    NSBox* grpMem = MakeGroup(@"Memory", NSMakeRect(GM, 120, GW, 70));
    [grpMem addSubview:MakePanelButton(@"Remember last", del, @selector(onRememberLast:),
        NSMakeRect(GM, 14, BW, BH))];
    [grpMem addSubview:MakePanelButton(@"Playback", del, @selector(onPlayback:),
        NSMakeRect(BW+GM+BG, 14, BW, BH))];

    // Settings group
    NSBox* grpSet = MakeGroup(@"Settings / Help", NSMakeRect(GM, 50, GW, 60));
    [grpSet addSubview:MakePanelButton(@"Preferences", del, @selector(onPreferences:),
        NSMakeRect(GM, 14, BW, BH))];

    [tabView addSubview:grpEasy];
    [tabView addSubview:grpPages];
    [tabView addSubview:grpStick];
    [tabView addSubview:grpMem];
    [tabView addSubview:grpSet];
}

static void ShowControlPanel() {
    if (!gPanelDelegate) {
        gPanelDelegate = [[ImposeControlPanelDelegate alloc] init];
    }
    if (!gControlPanel) {
        const NSRect frame = NSMakeRect(100, 200, 260, 610);
        gControlPanel = [[NSPanel alloc]
            initWithContentRect:frame
                      styleMask:(NSWindowStyleMaskTitled |
                                 NSWindowStyleMaskClosable |
                                 NSWindowStyleMaskMiniaturizable |
                                 NSWindowStyleMaskUtilityWindow)
                        backing:NSBackingStoreBuffered
                          defer:NO];
        [gControlPanel setTitle:@"Imposition Control Panel"];
        [gControlPanel setFloatingPanel:YES];
        [gControlPanel setLevel:NSFloatingWindowLevel];
        [gControlPanel setBecomesKeyOnlyIfNeeded:YES];

        NSView* content = [gControlPanel contentView];
        NSTabView* tabs = [[NSTabView alloc]
            initWithFrame:NSMakeRect(0, 0, 260, 590)];

        NSTabViewItem* controlTab = [[NSTabViewItem alloc] initWithIdentifier:@"control"];
        [controlTab setLabel:@"Control"];
        NSView* controlView = [[NSView alloc] initWithFrame:NSMakeRect(0,0,250,570)];
        BuildControlPanelContent(controlView, gPanelDelegate);
        [controlTab setView:controlView];

        NSTabViewItem* seqTab = [[NSTabViewItem alloc] initWithIdentifier:@"sequences"];
        [seqTab setLabel:@"Sequences"];
        NSView* seqView = [[NSView alloc] initWithFrame:NSMakeRect(0,0,250,570)];
        NSTextField* seqLbl = [[NSTextField alloc] initWithFrame:NSMakeRect(20,260,210,40)];
        [seqLbl setStringValue:@"Sequences manager — use Imposition > Automation > Sequences…"];
        [seqLbl setBezeled:NO];
        [seqLbl setDrawsBackground:NO];
        [seqLbl setEditable:NO];
        [seqLbl setSelectable:NO];
        [seqLbl setLineBreakMode:NSLineBreakByWordWrapping];
        [seqView addSubview:seqLbl];
        [seqTab setView:seqView];

        NSTabViewItem* infoTab = [[NSTabViewItem alloc] initWithIdentifier:@"info"];
        [infoTab setLabel:@"Info"];
        NSView* infoView = [[NSView alloc] initWithFrame:NSMakeRect(0,0,250,570)];
        NSTextField* infoLbl = [[NSTextField alloc] initWithFrame:NSMakeRect(20,260,210,40)];
        [infoLbl setStringValue:@"Info tab — run an imposition to see layout details here."];
        [infoLbl setBezeled:NO];
        [infoLbl setDrawsBackground:NO];
        [infoLbl setEditable:NO];
        [infoLbl setSelectable:NO];
        [infoLbl setLineBreakMode:NSLineBreakByWordWrapping];
        [infoView addSubview:infoLbl];
        [infoTab setView:infoView];

        NSTabViewItem* manualTab = [[NSTabViewItem alloc] initWithIdentifier:@"manual"];
        [manualTab setLabel:@"Manual"];
        NSView* manualView = [[NSView alloc] initWithFrame:NSMakeRect(0,0,250,570)];
        NSTextField* manualLbl = [[NSTextField alloc] initWithFrame:NSMakeRect(20,260,210,40)];
        [manualLbl setStringValue:@"Manual imposition — drag pages onto sheets."];
        [manualLbl setBezeled:NO];
        [manualLbl setDrawsBackground:NO];
        [manualLbl setEditable:NO];
        [manualLbl setSelectable:NO];
        [manualLbl setLineBreakMode:NSLineBreakByWordWrapping];
        [manualView addSubview:manualLbl];
        [manualTab setView:manualView];

        [tabs addTabViewItem:controlTab];
        [tabs addTabViewItem:seqTab];
        [tabs addTabViewItem:infoTab];
        [tabs addTabViewItem:manualTab];
        [content addSubview:tabs];
    }
    [gControlPanel makeKeyAndOrderFront:nil];
    [gControlPanel orderFrontRegardless];
}
#endif // MAC_PLATFORM

// ── Re-open namespace for plugin body ─────────────────────────────────────────

namespace {

// ── Constants ─────────────────────────────────────────────────────────────────

constexpr const char* kExtensionName    = "AcrobatImpositionPlugin";
constexpr const char* kMenuName         = "Imposition";

// Section: Control
constexpr const char* kMI_ControlPanel      = "Imposition Control Panel\xE2\x80\xA6";
// Section: Easy Imposition
constexpr const char* kMI_Booklet           = "Booklet\xE2\x80\xA6";
constexpr const char* kMI_NUpPages          = "N-up pages\xE2\x80\xA6";
constexpr const char* kMI_StepRepeat        = "Step & repeat\xE2\x80\xA6";
constexpr const char* kMI_Join2Pages        = "Join 2 pages\xE2\x80\xA6";
// Section: Page Management
constexpr const char* kMI_ShufflePages      = "Shuffle pages\xE2\x80\xA6";
constexpr const char* kMI_ShuffleEvenOdd    = "Shuffle even/odd\xE2\x80\xA6";
constexpr const char* kMI_ReversePages      = "Reverse pages";
constexpr const char* kMI_InsertPages       = "Insert pages\xE2\x80\xA6";
constexpr const char* kMI_TrimShift         = "Trim & shift\xE2\x80\xA6";
constexpr const char* kMI_TilePages         = "Tile pages\xE2\x80\xA6";
constexpr const char* kMI_PageSizes         = "Page sizes\xE2\x80\xA6";
constexpr const char* kMI_PageTools         = "Page tools\xE2\x80\xA6";
// Section: Advanced
constexpr const char* kMI_Creep             = "Creep\xE2\x80\xA6";
constexpr const char* kMI_SampleDocument    = "Sample document\xE2\x80\xA6";
constexpr const char* kMI_Monitor           = "Monitor\xE2\x80\xA6";
// Section: Stick On
constexpr const char* kMI_StickText         = "Text & numbers\xE2\x80\xA6";
constexpr const char* kMI_StickPdf          = "PDF pages\xE2\x80\xA6";
constexpr const char* kMI_StickMasking      = "Masking tape\xE2\x80\xA6";
// Section: Peel Off
constexpr const char* kMI_PeelText          = "Text & numbers\xE2\x80\xA6";
constexpr const char* kMI_PeelMasking       = "Masking tape\xE2\x80\xA6";
constexpr const char* kMI_RegMarks          = "Registration marks\xE2\x80\xA6";
// Section: Automation
constexpr const char* kMI_Sequences         = "Sequences\xE2\x80\xA6";
constexpr const char* kMI_Playback          = "Playback";
constexpr const char* kMI_RememberLast      = "Remember last";
// Section: Settings
constexpr const char* kMI_Preferences       = "Preferences\xE2\x80\xA6";
constexpr const char* kMI_CustomizePanel    = "Customize panel\xE2\x80\xA6";
constexpr const char* kMI_HelpAbout         = "Help / About";

// ── Global menu handles ───────────────────────────────────────────────────────

AVMenu   gImpositionMenu        = nullptr;
AVMenuItem gMI_ControlPanel     = nullptr;
AVMenuItem gMI_Booklet          = nullptr;
AVMenuItem gMI_NUpPages         = nullptr;
AVMenuItem gMI_StepRepeat       = nullptr;
AVMenuItem gMI_Join2Pages       = nullptr;
AVMenuItem gMI_ShufflePages     = nullptr;
AVMenuItem gMI_ShuffleEvenOdd   = nullptr;
AVMenuItem gMI_ReversePages     = nullptr;
AVMenuItem gMI_InsertPages      = nullptr;
AVMenuItem gMI_TrimShift        = nullptr;
AVMenuItem gMI_TilePages        = nullptr;
AVMenuItem gMI_PageSizes        = nullptr;
AVMenuItem gMI_PageTools        = nullptr;
AVMenuItem gMI_Creep            = nullptr;
AVMenuItem gMI_SampleDocument   = nullptr;
AVMenuItem gMI_Monitor          = nullptr;
AVMenuItem gMI_StickText        = nullptr;
AVMenuItem gMI_StickPdf         = nullptr;
AVMenuItem gMI_StickMasking     = nullptr;
AVMenuItem gMI_PeelText         = nullptr;
AVMenuItem gMI_PeelMasking      = nullptr;
AVMenuItem gMI_RegMarks         = nullptr;
AVMenuItem gMI_Sequences        = nullptr;
AVMenuItem gMI_Playback         = nullptr;
AVMenuItem gMI_RememberLast     = nullptr;
AVMenuItem gMI_Preferences      = nullptr;
AVMenuItem gMI_CustomizePanel   = nullptr;
AVMenuItem gMI_HelpAbout        = nullptr;

// Procs
AVExecuteProc gProc_ControlPanel    = nullptr;
AVExecuteProc gProc_Booklet         = nullptr;
AVExecuteProc gProc_NUpPages        = nullptr;
AVExecuteProc gProc_StepRepeat      = nullptr;
AVExecuteProc gProc_Join2Pages      = nullptr;
AVExecuteProc gProc_ShufflePages    = nullptr;
AVExecuteProc gProc_ShuffleEvenOdd  = nullptr;
AVExecuteProc gProc_ReversePages    = nullptr;
AVExecuteProc gProc_InsertPages     = nullptr;
AVExecuteProc gProc_TrimShift       = nullptr;
AVExecuteProc gProc_TilePages       = nullptr;
AVExecuteProc gProc_PageSizes       = nullptr;
AVExecuteProc gProc_PageTools       = nullptr;
AVExecuteProc gProc_Creep           = nullptr;
AVExecuteProc gProc_SampleDocument  = nullptr;
AVExecuteProc gProc_Monitor         = nullptr;
AVExecuteProc gProc_StickText       = nullptr;
AVExecuteProc gProc_StickPdf        = nullptr;
AVExecuteProc gProc_StickMasking    = nullptr;
AVExecuteProc gProc_PeelText        = nullptr;
AVExecuteProc gProc_PeelMasking     = nullptr;
AVExecuteProc gProc_RegMarks        = nullptr;
AVExecuteProc gProc_Sequences       = nullptr;
AVExecuteProc gProc_Playback        = nullptr;
AVExecuteProc gProc_RememberLast    = nullptr;
AVExecuteProc gProc_Preferences     = nullptr;
AVExecuteProc gProc_CustomizePanel  = nullptr;
AVExecuteProc gProc_HelpAbout       = nullptr;

// ── Utilities ─────────────────────────────────────────────────────────────────

std::string BuildUtcTimestamp() {
    const auto now = std::chrono::system_clock::now();
    const std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm utc {};
#if defined(_WIN32)
    gmtime_s(&utc, &t);
#else
    gmtime_r(&t, &utc);
#endif
    std::ostringstream out;
    out << std::put_time(&utc, "%Y%m%d-%H%M%SZ");
    return out.str();
}

std::string GetPresetPath() {
    std::error_code ec;
    const auto tmp = std::filesystem::temp_directory_path(ec);
    if (ec) return "acrobat-imposition-plugin.preset.txt";
    return (tmp / "acrobat-imposition-plugin.preset.txt").string();
}

std::string GetLastActionPath() {
    std::error_code ec;
    const auto tmp = std::filesystem::temp_directory_path(ec);
    if (ec) return "aimp-last-action.json";
    return (tmp / "aimp-last-action.json").string();
}

void ShowInfoDialog(const std::string& msg) {
    AVAlertNote(msg.c_str());
}

bool BuildDefaultPreset(aimp::PlannerPreset& p) {
    p = {};
    p.sheetSize = {1190.55, 841.89};
    p.columns = 2;
    p.rows = 1;
    p.buildOptions.scaleToFit = true;
    p.buildOptions.autoRotateToFit = true;
    p.pdfOptions.drawTrimMarks = false;
    p.pdfOptions.drawBleedBox = false;
    p.pdfOptions.bleedPoints = 0.0;
    p.outputStem = "imposr-output";
    return true;
}

static bool GetFirstPageDimensions(PDDoc pdDoc, double& outW, double& outH) {
    if (PDDocGetNumPages(pdDoc) <= 0) return false;
    PDPage pg = PDDocAcquirePage(pdDoc, 0);
    if (!pg) return false;
    ASFixedRect cb {};
    PDPageGetCropBox(pg, &cb);
    PDPageRelease(pg);
    outW = ASFixedToFloat(cb.right)  - ASFixedToFloat(cb.left);
    outH = ASFixedToFloat(cb.top)    - ASFixedToFloat(cb.bottom);
    return outW > 0.0 && outH > 0.0;
}

static AVDoc OpenPdfInAcrobat(const std::string& path) {
    const ASFileSys fs = ASGetDefaultFileSys();
    ASPathName ap = ASFileSysCreatePathName(fs, ASAtomFromString("Cstring"),
                                             path.c_str(), nullptr);
    if (!ap) return nullptr;
    AVDoc doc = AVDocOpenFromFile(ap, fs, nullptr);
    ASFileSysReleasePath(fs, ap);
    return doc;
}

// ── PDF composition engine ────────────────────────────────────────────────────

static ASInt32 NormalizeRotation(double deg) {
    if (!std::isfinite(deg)) return 0;
    double n = std::fmod(deg, 360.0);
    if (n < 0.0) n += 360.0;
    const ASInt32 s = static_cast<ASInt32>(std::lround(n / 90.0)) * 90;
    return (s == 360) ? 0 : s;
}

static ASFixedMatrix BuildCropBoxCorrectCtm(const aimp::AcrobatSdkPlacementOp& op,
                                             const ASFixedRect& srcBBox) {
    const double cx1 = ASFixedToFloat(srcBBox.left);
    const double cy1 = ASFixedToFloat(srcBBox.bottom);
    const double cx2 = ASFixedToFloat(srcBBox.right);
    const double cy2 = ASFixedToFloat(srcBBox.top);
    const double cw  = cx2 - cx1;
    const double ch  = cy2 - cy1;
    const double tx  = op.targetRect.x;
    const double ty  = op.targetRect.y;
    const double tw  = op.targetRect.width;
    const double th  = op.targetRect.height;

    ASFixedMatrix ctm {};
    if (cw <= 0.0 || ch <= 0.0) {
        ctm.a = ASFloatToFixed((ASReal)op.ctmA);
        ctm.b = ASFloatToFixed((ASReal)op.ctmB);
        ctm.c = ASFloatToFixed((ASReal)op.ctmC);
        ctm.d = ASFloatToFixed((ASReal)op.ctmD);
        ctm.h = ASFloatToFixed((ASReal)op.ctmE);
        ctm.v = ASFloatToFixed((ASReal)op.ctmF);
        return ctm;
    }

    const int rot = NormalizeRotation(op.rotationDegrees);

    if (rot == 0) {
        const double sx = tw / cw, sy = th / ch;
        ctm.a = ASFloatToFixed((ASReal)sx);
        ctm.b = fixedZero; ctm.c = fixedZero;
        ctm.d = ASFloatToFixed((ASReal)sy);
        ctm.h = ASFloatToFixed((ASReal)(tx - sx * cx1));
        ctm.v = ASFloatToFixed((ASReal)(ty - sy * cy1));
    } else if (rot == 90) {
        const double bv =  th / cw, cv = -tw / ch;
        ctm.a = fixedZero;
        ctm.b = ASFloatToFixed((ASReal)bv);
        ctm.c = ASFloatToFixed((ASReal)cv);
        ctm.d = fixedZero;
        ctm.h = ASFloatToFixed((ASReal)(tx + tw + cv * cy1));
        ctm.v = ASFloatToFixed((ASReal)(ty - bv * cx1));
    } else if (rot == 180) {
        const double sx = tw / cw, sy = th / ch;
        ctm.a = ASFloatToFixed((ASReal)(-sx));
        ctm.b = fixedZero; ctm.c = fixedZero;
        ctm.d = ASFloatToFixed((ASReal)(-sy));
        ctm.h = ASFloatToFixed((ASReal)(tx + tw + sx * cx1));
        ctm.v = ASFloatToFixed((ASReal)(ty + th + sy * cy1));
    } else if (rot == 270) {
        const double bv = -th / cw, cv = tw / ch;
        ctm.a = fixedZero;
        ctm.b = ASFloatToFixed((ASReal)bv);
        ctm.c = ASFloatToFixed((ASReal)cv);
        ctm.d = fixedZero;
        ctm.h = ASFloatToFixed((ASReal)(tx - cv * cy1));
        ctm.v = ASFloatToFixed((ASReal)(ty + th - bv * cx1));
    } else {
        ctm.a = ASFloatToFixed((ASReal)op.ctmA);
        ctm.b = ASFloatToFixed((ASReal)op.ctmB);
        ctm.c = ASFloatToFixed((ASReal)op.ctmC);
        ctm.d = ASFloatToFixed((ASReal)op.ctmD);
        ctm.h = ASFloatToFixed((ASReal)op.ctmE);
        ctm.v = ASFloatToFixed((ASReal)op.ctmF);
    }
    return ctm;
}

static bool TryRunExperimentalSdkComposer(PDDoc sourceDoc,
                                          const aimp::ImpositionPlan& plan,
                                          const std::string& sdkOpsPath,
                                          const std::string& outputPdfPath,
                                          std::string& err) {
    std::vector<aimp::AcrobatSdkPlacementOp> ops;
    {
        std::ifstream in(sdkOpsPath);
        if (!in) { err = "Cannot open sdk-ops file"; return false; }
        std::ostringstream buf; buf << in.rdbuf();
        if (!aimp::ParseAcrobatSdkOpsJson(buf.str(), ops, err)) return false;
    }
    const auto opIssues = aimp::ValidateAcrobatSdkOps(plan, ops);
    if (!opIssues.empty()) { err = "sdk-ops validation failed"; return false; }

    std::vector<std::vector<aimp::AcrobatSdkPlacementOp>> buckets;
    if (!aimp::BuildSheetComposeBuckets(plan, ops, buckets, err)) return false;

    PDDoc outDoc = PDDocCreate();
    if (!outDoc) { err = "PDDocCreate failed"; return false; }

    const double shW = plan.outputSheet.widthPoints;
    const double shH = plan.outputSheet.heightPoints;

    for (std::size_t si = 0; si < buckets.size(); ++si) {
        ASFixedRect mb {};
        mb.left   = fixedZero;
        mb.bottom = fixedZero;
        mb.right  = ASFloatToFixed((ASReal)shW);
        mb.top    = ASFloatToFixed((ASReal)shH);

        const ASInt32 ins = PDDocGetNumPages(outDoc) - 1;
        PDPage outPg = PDDocCreatePage(outDoc, ins, mb);
        if (!outPg) {
            err = "PDDocCreatePage failed for sheet " + std::to_string(si);
            PDDocClose(outDoc); return false;
        }
        PDEContent outContent = PDPageAcquirePDEContent(outPg, 0);
        if (!outContent) {
            PDPageRelease(outPg); PDDocClose(outDoc);
            err = "PDPageAcquirePDEContent failed"; return false;
        }

        bool ok = true;
        for (const auto& op : buckets[si]) {
            if (op.isBlank) continue;
            PDPage srcPage = PDDocAcquirePage(sourceDoc, (ASInt32)op.sourcePageIndex);
            if (!srcPage) {
                err = "PDDocAcquirePage failed for page " + std::to_string(op.sourcePageIndex);
                ok = false; break;
            }
            ASFixedRect srcBBox {};
            PDPageGetCropBox(srcPage, &srcBBox);
            PDEContent srcContent = PDPageAcquirePDEContent(srcPage, 0);
            if (srcContent) {
                PDEContentAttrs fa {};
                fa.flags     = 0;
                fa.formType  = 1;
                fa.bbox      = srcBBox;
                fa.matrix.a  = fixedOne;  fa.matrix.b = fixedZero;
                fa.matrix.c  = fixedZero; fa.matrix.d = fixedOne;
                fa.matrix.h  = fixedZero; fa.matrix.v = fixedZero;
                CosObj formObj {}, resObj {};
                PDEContentToCosObj(srcContent, kPDEContentToForm, &fa,
                                   (ASUns32)sizeof(fa), PDDocGetCosDoc(outDoc),
                                   nullptr, &formObj, &resObj);
                if (CosObjGetType(formObj) != CosNull) {
                    ASFixedMatrix ctm = BuildCropBoxCorrectCtm(op, srcBBox);
                    PDEForm frm = PDEFormCreateFromCosObj(&formObj, nullptr, &ctm);
                    if (frm) {
                        PDEContentAddElem(outContent, kPDEAfterLast, (PDEElement)frm);
                        PDERelease((PDEObject)frm);
                    }
                }
                PDPageReleasePDEContent(srcPage, 0);
            }
            PDPageRelease(srcPage);
        }

        PDPageSetPDEContent(outPg, 0);
        PDPageReleasePDEContent(outPg, 0);
        PDPageRelease(outPg);
        if (!ok) { PDDocClose(outDoc); return false; }
    }

    // Remove interactive annotations from output pages
    const ASInt32 opCount = PDDocGetNumPages(outDoc);
    for (ASInt32 pi = 0; pi < opCount; ++pi) {
        PDPage pg = PDDocAcquirePage(outDoc, pi);
        if (!pg) continue;
        for (ASInt32 ai = PDPageGetNumAnnots(pg) - 1; ai >= 0; --ai)
            PDPageRemoveAnnot(pg, ai);
        PDPageRelease(pg);
    }

    // Copy OutputIntent from source
    {
        CosDoc srcCD  = PDDocGetCosDoc(sourceDoc);
        CosObj srcCat = CosDocGetRoot(srcCD);
        CosObj srcOIA = CosDictGet(srcCat, ASAtomFromString("OutputIntents"));
        if (CosObjGetType(srcOIA) == CosArray && CosArrayLength(srcOIA) > 0) {
            CosDoc outCD  = PDDocGetCosDoc(outDoc);
            CosObj outCat = CosDocGetRoot(outCD);
            CosObj outOI  = CosObjCopy(CosArrayGet(srcOIA, 0), outCD, false);
            CosObj outArr = CosNewArray(outCD, false, 1);
            CosArrayPut(outArr, 0, outOI);
            CosDictPut(outCat, ASAtomFromString("OutputIntents"), outArr);
        }
    }

    const ASFileSys fs = ASGetDefaultFileSys();
    ASPathName outAP = ASFileSysCreatePathName(fs, ASAtomFromString("Cstring"),
                                               outputPdfPath.c_str(), nullptr);
    if (!outAP) { err = "Cannot create output path"; PDDocClose(outDoc); return false; }
    PDDocSave(outDoc, PDSaveFull | PDSaveCollectGarbage, outAP, nullptr, nullptr, nullptr);
    ASFileSysReleasePath(fs, outAP);
    PDDocClose(outDoc);
    return true;
}

static bool NativeComposePlan(PDDoc sourceDoc,
                               const aimp::ImpositionPlan& plan,
                               const std::string& outputPath,
                               std::string& err) {
    std::error_code fse;
    const auto tmp = std::filesystem::temp_directory_path(fse);
    if (fse) { err = "Cannot determine temp dir"; return false; }
    const auto opsPath = (tmp / "imposr-sdk-ops-tmp.json").string();
    {
        std::ofstream out(opsPath);
        if (!out) { err = "Cannot write sdk-ops"; return false; }
        out << aimp::ToAcrobatSdkOpsJson(plan);
    }
    return TryRunExperimentalSdkComposer(sourceDoc, plan, opsPath, outputPath, err);
}

// Build a reorder plan manually (used by Shuffle Pages, Reverse Pages, etc.)
static aimp::ImpositionPlan BuildReorderPlan(const std::string& docId,
                                              const std::vector<std::uint32_t>& pageOrder,
                                              double pageW, double pageH) {
    aimp::ImpositionPlan plan;
    plan.mode = aimp::LayoutMode::Manual;
    plan.outputSheet = {pageW, pageH};
    plan.sourcePageCount = (std::uint32_t)pageOrder.size();
    plan.paddedPageCount = plan.sourcePageCount;

    for (std::uint32_t i = 0; i < (std::uint32_t)pageOrder.size(); ++i) {
        aimp::SlotPlacement p;
        p.sheetIndex = i;
        p.slotIndex  = 0;
        p.sourcePage.sourceDocumentId = docId;
        p.sourcePage.pageIndex        = pageOrder[i];
        p.targetRect = {0.0, 0.0, pageW, pageH};
        p.rotationDegrees = 0.0;
        p.scale = 1.0;
        plan.placements.push_back(p);
    }
    return plan;
}

// ── Dialog parameter structs ──────────────────────────────────────────────────

struct BookletParams {
    bool     cancelled       = true;
    double   sheetWidth      = 0;
    double   sheetHeight     = 0;
    bool     bindingLeft     = true;
    std::uint32_t sigSize    = 0;   // 0 = all
    double   creepPerSheet   = 0;
    bool     autoPad         = true;
};

struct NUpParams {
    bool     cancelled   = true;
    std::uint32_t cols   = 2;
    std::uint32_t rows   = 2;
    double   sheetWidth  = 0;
    double   sheetHeight = 0;
    bool     scaleToFit  = true;
    bool     autoRotate  = true;
};

struct StepRepeatParams {
    bool     cancelled   = true;
    std::uint32_t cols   = 2;
    std::uint32_t rows   = 2;
    double   sheetWidth  = 0;
    double   sheetHeight = 0;
    double   stepX       = 0;   // 0 = auto (= page size)
    double   stepY       = 0;
};

struct ShuffleParams {
    bool     cancelled   = true;
    aimp::ShuffleMode mode = aimp::ShuffleMode::Signature;
    std::uint32_t sigSize  = 4;
};

struct TilePagesParams {
    bool     cancelled     = true;
    std::uint32_t cols     = 2;
    std::uint32_t rows     = 2;
    double   overlapPoints = 18.0;
};

struct CreepParams {
    bool   cancelled        = true;
    double creepPerSheet    = 1.0;
    std::uint32_t sheetsInSig = 8;
};

// ── macOS Cocoa dialog helpers ─────────────────────────────────────────────────

#ifdef MAC_PLATFORM

struct SheetPreset { const char* label; double w; double h; };
static const SheetPreset kSheetPresets[] = {
    {"A4 Portrait (595×842pt)",     595.276,  841.890},
    {"A4 Landscape (842×595pt)",    841.890,  595.276},
    {"A3 Portrait (842×1191pt)",    841.890, 1190.551},
    {"A3 Landscape (1191×842pt)", 1190.551,   841.890},
    {"US Letter Portrait",          612.0,    792.0},
    {"US Letter Landscape",         792.0,    612.0},
    {"US Tabloid Portrait",         792.0,   1224.0},
    {"US Tabloid Landscape",       1224.0,    792.0},
    {"Match source pages",            0.0,      0.0},
};
static constexpr int kSheetPresetCount = 9;
static constexpr int kSheetMatchIdx    = 8;

static NSView* MakeAccessoryView(CGFloat w, CGFloat h) {
    return [[NSView alloc] initWithFrame:NSMakeRect(0, 0, w, h)];
}

static NSTextField* MakeLabel(NSString* text, NSRect frame) {
    NSTextField* f = [[NSTextField alloc] initWithFrame:frame];
    [f setStringValue:text];
    [f setBezeled:NO];
    [f setDrawsBackground:NO];
    [f setEditable:NO];
    [f setSelectable:NO];
    return f;
}

static NSTextField* MakeTextField(NSString* value, NSRect frame) {
    NSTextField* f = [[NSTextField alloc] initWithFrame:frame];
    [f setStringValue:value];
    [f setEditable:YES];
    [f setBezeled:YES];
    return f;
}

static NSPopUpButton* MakePopup(NSRect frame) {
    NSPopUpButton* p = [[NSPopUpButton alloc] initWithFrame:frame pullsDown:NO];
    return p;
}

static NSButton* MakeCheckbox(NSString* title, BOOL on, NSRect frame) {
    NSButton* b = [[NSButton alloc] initWithFrame:frame];
    [b setButtonType:NSButtonTypeSwitch];
    [b setTitle:title];
    [b setState:on ? NSControlStateValueOn : NSControlStateValueOff];
    return b;
}

static NSPopUpButton* AddSheetSizePopup(NSView* view, NSRect frame) {
    NSPopUpButton* p = MakePopup(frame);
    for (int i = 0; i < kSheetPresetCount; ++i)
        [p addItemWithTitle:[NSString stringWithUTF8String:kSheetPresets[i].label]];
    [p selectItemAtIndex:2]; // A3 Portrait default
    [view addSubview:p];
    return p;
}

// Returns {w,h} in points; w==0 means use source page size
static std::pair<double,double> SheetFromPopup(NSPopUpButton* popup, double fallbackW, double fallbackH) {
    const NSInteger idx = [popup indexOfSelectedItem];
    if (idx < 0 || idx >= kSheetPresetCount) return {fallbackW, fallbackH};
    if (idx == kSheetMatchIdx) return {fallbackW, fallbackH};
    return {kSheetPresets[idx].w, kSheetPresets[idx].h};
}

// Booklet dialog
static BookletParams ShowBookletDialog(double pageW, double pageH) {
    BookletParams p;
    @autoreleasepool {
        const CGFloat W = 360, ROW = 26, GAP = 6, LBL = 130, TOP = 8;
        // 5 rows
        NSView* acc = MakeAccessoryView(W, TOP + 5 * (ROW + GAP) + 10);

        auto rowY = [&](int row) -> CGFloat {
            return TOP + (CGFloat)row * (ROW + GAP);
        };

        // Row 0: binding
        [acc addSubview:MakeLabel(@"Binding:", NSMakeRect(0, rowY(4), LBL, ROW))];
        NSPopUpButton* bindPopup = MakePopup(NSMakeRect(LBL, rowY(4), W-LBL, ROW));
        [bindPopup addItemWithTitle:@"Left (book opens right)"];
        [bindPopup addItemWithTitle:@"Right (book opens left)"];
        [acc addSubview:bindPopup];

        // Row 1: sheet size
        [acc addSubview:MakeLabel(@"Sheet size:", NSMakeRect(0, rowY(3), LBL, ROW))];
        NSPopUpButton* sheetPopup = AddSheetSizePopup(acc, NSMakeRect(LBL, rowY(3), W-LBL, ROW));
        // Select A3 landscape as default for booklet (two A4 pages side by side)
        [sheetPopup selectItemAtIndex:3];

        // Row 2: signature size
        [acc addSubview:MakeLabel(@"Signature:", NSMakeRect(0, rowY(2), LBL, ROW))];
        NSPopUpButton* sigPopup = MakePopup(NSMakeRect(LBL, rowY(2), W-LBL, ROW));
        [sigPopup addItemWithTitle:@"All pages"];
        [sigPopup addItemWithTitle:@"4 pages"];
        [sigPopup addItemWithTitle:@"8 pages"];
        [sigPopup addItemWithTitle:@"12 pages"];
        [sigPopup addItemWithTitle:@"16 pages"];
        [acc addSubview:sigPopup];

        // Row 3: creep
        [acc addSubview:MakeLabel(@"Creep/sheet (pt):", NSMakeRect(0, rowY(1), LBL, ROW))];
        NSTextField* creepFld = MakeTextField(@"0", NSMakeRect(LBL, rowY(1)+2, 80, ROW-4));
        [acc addSubview:creepFld];

        // Row 4: auto-pad
        NSButton* padChk = MakeCheckbox(@"Pad to multiple of 4", YES, NSMakeRect(0, rowY(0), W, ROW));
        [acc addSubview:padChk];

        NSAlert* alert = [[NSAlert alloc] init];
        [alert setMessageText:@"Booklet"];
        [alert setInformativeText:@"Create a saddle-stitched booklet from the active document."];
        [alert addButtonWithTitle:@"Create Booklet"];
        [alert addButtonWithTitle:@"Cancel"];
        [alert setAccessoryView:acc];
        [alert layout];

        if ([alert runModal] == NSAlertFirstButtonReturn) {
            p.cancelled = false;
            p.bindingLeft = ([bindPopup indexOfSelectedItem] == 0);
            const auto [sw, sh] = SheetFromPopup(sheetPopup, pageW * 2.0, pageH);
            p.sheetWidth  = sw;
            p.sheetHeight = sh;
            const NSInteger si = [sigPopup indexOfSelectedItem];
            static const std::uint32_t sigSizes[] = {0, 4, 8, 12, 16};
            p.sigSize = sigSizes[si >= 0 && si <= 4 ? si : 0];
            p.creepPerSheet = [[creepFld stringValue] doubleValue];
            p.autoPad = ([padChk state] == NSControlStateValueOn);
        }
    }
    return p;
}

// N-up dialog
static NUpParams ShowNUpDialog(double pageW, double pageH) {
    NUpParams p;
    @autoreleasepool {
        const CGFloat W = 360, ROW = 26, GAP = 6, LBL = 130, TOP = 8;
        NSView* acc = MakeAccessoryView(W, TOP + 5 * (ROW + GAP) + 10);

        auto rowY = [&](int row) -> CGFloat { return TOP + (CGFloat)row * (ROW + GAP); };

        [acc addSubview:MakeLabel(@"Columns:", NSMakeRect(0, rowY(4), LBL, ROW))];
        NSTextField* colFld = MakeTextField(@"2", NSMakeRect(LBL, rowY(4)+2, 60, ROW-4));
        [acc addSubview:colFld];

        [acc addSubview:MakeLabel(@"Rows:", NSMakeRect(0, rowY(3), LBL, ROW))];
        NSTextField* rowFld = MakeTextField(@"2", NSMakeRect(LBL, rowY(3)+2, 60, ROW-4));
        [acc addSubview:rowFld];

        [acc addSubview:MakeLabel(@"Sheet size:", NSMakeRect(0, rowY(2), LBL, ROW))];
        NSPopUpButton* sheetPopup = AddSheetSizePopup(acc, NSMakeRect(LBL, rowY(2), W-LBL, ROW));

        NSButton* scaleChk = MakeCheckbox(@"Scale pages to fit slot", YES,
                                           NSMakeRect(0, rowY(1), W, ROW));
        [acc addSubview:scaleChk];

        NSButton* rotChk = MakeCheckbox(@"Auto-rotate to fit", YES,
                                         NSMakeRect(0, rowY(0), W, ROW));
        [acc addSubview:rotChk];

        NSAlert* alert = [[NSAlert alloc] init];
        [alert setMessageText:@"N-up Pages"];
        [alert setInformativeText:@"Place multiple source pages on each output sheet."];
        [alert addButtonWithTitle:@"Create N-up"];
        [alert addButtonWithTitle:@"Cancel"];
        [alert setAccessoryView:acc];
        [alert layout];

        if ([alert runModal] == NSAlertFirstButtonReturn) {
            p.cancelled = false;
            p.cols = (std::uint32_t)std::max((NSInteger)1, [[colFld stringValue] integerValue]);
            p.rows = (std::uint32_t)std::max((NSInteger)1, [[rowFld stringValue] integerValue]);
            const auto [sw, sh] = SheetFromPopup(sheetPopup,
                                                  pageW * (double)p.cols,
                                                  pageH * (double)p.rows);
            p.sheetWidth  = sw;
            p.sheetHeight = sh;
            p.scaleToFit = ([scaleChk state] == NSControlStateValueOn);
            p.autoRotate = ([rotChk state]   == NSControlStateValueOn);
        }
    }
    return p;
}

// Step & Repeat dialog
static StepRepeatParams ShowStepRepeatDialog(double pageW, double pageH) {
    StepRepeatParams p;
    @autoreleasepool {
        const CGFloat W = 360, ROW = 26, GAP = 6, LBL = 140, TOP = 8;
        NSView* acc = MakeAccessoryView(W, TOP + 5 * (ROW + GAP) + 10);

        auto rowY = [&](int row) -> CGFloat { return TOP + (CGFloat)row * (ROW + GAP); };

        [acc addSubview:MakeLabel(@"Repeat columns:", NSMakeRect(0, rowY(4), LBL, ROW))];
        NSTextField* colFld = MakeTextField(@"2", NSMakeRect(LBL, rowY(4)+2, 60, ROW-4));
        [acc addSubview:colFld];

        [acc addSubview:MakeLabel(@"Repeat rows:", NSMakeRect(0, rowY(3), LBL, ROW))];
        NSTextField* rowFld = MakeTextField(@"2", NSMakeRect(LBL, rowY(3)+2, 60, ROW-4));
        [acc addSubview:rowFld];

        [acc addSubview:MakeLabel(@"Sheet size:", NSMakeRect(0, rowY(2), LBL, ROW))];
        NSPopUpButton* sheetPopup = AddSheetSizePopup(acc, NSMakeRect(LBL, rowY(2), W-LBL, ROW));
        [sheetPopup selectItemAtIndex:kSheetMatchIdx]; // match source by default

        [acc addSubview:MakeLabel(@"Step X (pt, 0=auto):", NSMakeRect(0, rowY(1), LBL, ROW))];
        NSTextField* stepXFld = MakeTextField(@"0", NSMakeRect(LBL, rowY(1)+2, 60, ROW-4));
        [acc addSubview:stepXFld];

        [acc addSubview:MakeLabel(@"Step Y (pt, 0=auto):", NSMakeRect(0, rowY(0), LBL, ROW))];
        NSTextField* stepYFld = MakeTextField(@"0", NSMakeRect(LBL, rowY(0)+2, 60, ROW-4));
        [acc addSubview:stepYFld];

        NSAlert* alert = [[NSAlert alloc] init];
        [alert setMessageText:@"Step & Repeat"];
        [alert setInformativeText:@"Repeat one page in a regular grid."];
        [alert addButtonWithTitle:@"Create Step & Repeat"];
        [alert addButtonWithTitle:@"Cancel"];
        [alert setAccessoryView:acc];
        [alert layout];

        if ([alert runModal] == NSAlertFirstButtonReturn) {
            p.cancelled = false;
            p.cols = (std::uint32_t)std::max((NSInteger)1, [[colFld stringValue] integerValue]);
            p.rows = (std::uint32_t)std::max((NSInteger)1, [[rowFld stringValue] integerValue]);
            const auto [sw, sh] = SheetFromPopup(sheetPopup,
                                                  pageW * (double)p.cols,
                                                  pageH * (double)p.rows);
            p.sheetWidth  = sw;
            p.sheetHeight = sh;
            const double sx = [[stepXFld stringValue] doubleValue];
            const double sy = [[stepYFld stringValue] doubleValue];
            p.stepX = (sx > 0) ? sx : pageW;
            p.stepY = (sy > 0) ? sy : pageH;
        }
    }
    return p;
}

// Shuffle Pages dialog
static ShuffleParams ShowShuffleDialog() {
    ShuffleParams p;
    @autoreleasepool {
        const CGFloat W = 340, ROW = 26, GAP = 6, LBL = 140, TOP = 8;
        NSView* acc = MakeAccessoryView(W, TOP + 2 * (ROW + GAP) + 10);

        auto rowY = [&](int row) -> CGFloat { return TOP + (CGFloat)row * (ROW + GAP); };

        [acc addSubview:MakeLabel(@"Shuffle mode:", NSMakeRect(0, rowY(1), LBL, ROW))];
        NSPopUpButton* modePopup = MakePopup(NSMakeRect(LBL, rowY(1), W-LBL, ROW));
        [modePopup addItemWithTitle:@"Saddle Stitch (Signature)"];
        [modePopup addItemWithTitle:@"Perfect Bound"];
        [modePopup addItemWithTitle:@"Split Even/Odd"];
        [modePopup addItemWithTitle:@"Interleave"];
        [acc addSubview:modePopup];

        [acc addSubview:MakeLabel(@"Signature size:", NSMakeRect(0, rowY(0), LBL, ROW))];
        NSPopUpButton* sigPopup = MakePopup(NSMakeRect(LBL, rowY(0), W-LBL, ROW));
        [sigPopup addItemWithTitle:@"4 pages"];
        [sigPopup addItemWithTitle:@"8 pages"];
        [sigPopup addItemWithTitle:@"12 pages"];
        [sigPopup addItemWithTitle:@"16 pages"];
        [acc addSubview:sigPopup];

        NSAlert* alert = [[NSAlert alloc] init];
        [alert setMessageText:@"Shuffle Pages"];
        [alert setInformativeText:@"Reorder pages for booklet or duplex printing."];
        [alert addButtonWithTitle:@"Shuffle & Compose"];
        [alert addButtonWithTitle:@"Cancel"];
        [alert setAccessoryView:acc];
        [alert layout];

        if ([alert runModal] == NSAlertFirstButtonReturn) {
            p.cancelled = false;
            const NSInteger mi = [modePopup indexOfSelectedItem];
            static const aimp::ShuffleMode modes[] = {
                aimp::ShuffleMode::Signature,
                aimp::ShuffleMode::PerfectBound,
                aimp::ShuffleMode::EvenOdd,
                aimp::ShuffleMode::Interleave,
            };
            p.mode = modes[mi >= 0 && mi < 4 ? mi : 0];
            static const std::uint32_t sigSizes[] = {4, 8, 12, 16};
            const NSInteger si = [sigPopup indexOfSelectedItem];
            p.sigSize = sigSizes[si >= 0 && si < 4 ? si : 0];
        }
    }
    return p;
}

// Tile Pages dialog
static TilePagesParams ShowTilePagesDialog() {
    TilePagesParams p;
    @autoreleasepool {
        const CGFloat W = 320, ROW = 26, GAP = 6, LBL = 130, TOP = 8;
        NSView* acc = MakeAccessoryView(W, TOP + 3 * (ROW + GAP) + 10);

        auto rowY = [&](int row) -> CGFloat { return TOP + (CGFloat)row * (ROW + GAP); };

        [acc addSubview:MakeLabel(@"Tile columns:", NSMakeRect(0, rowY(2), LBL, ROW))];
        NSTextField* colFld = MakeTextField(@"2", NSMakeRect(LBL, rowY(2)+2, 60, ROW-4));
        [acc addSubview:colFld];

        [acc addSubview:MakeLabel(@"Tile rows:", NSMakeRect(0, rowY(1), LBL, ROW))];
        NSTextField* rowFld = MakeTextField(@"2", NSMakeRect(LBL, rowY(1)+2, 60, ROW-4));
        [acc addSubview:rowFld];

        [acc addSubview:MakeLabel(@"Overlap (pt):", NSMakeRect(0, rowY(0), LBL, ROW))];
        NSTextField* overlapFld = MakeTextField(@"18", NSMakeRect(LBL, rowY(0)+2, 60, ROW-4));
        [acc addSubview:overlapFld];

        NSAlert* alert = [[NSAlert alloc] init];
        [alert setMessageText:@"Tile Pages"];
        [alert setInformativeText:@"Split large pages into smaller tile sheets."];
        [alert addButtonWithTitle:@"Create Tiles"];
        [alert addButtonWithTitle:@"Cancel"];
        [alert setAccessoryView:acc];
        [alert layout];

        if ([alert runModal] == NSAlertFirstButtonReturn) {
            p.cancelled = false;
            p.cols = (std::uint32_t)std::max((NSInteger)1, [[colFld stringValue] integerValue]);
            p.rows = (std::uint32_t)std::max((NSInteger)1, [[rowFld stringValue] integerValue]);
            p.overlapPoints = [[overlapFld stringValue] doubleValue];
        }
    }
    return p;
}

// Creep dialog
static CreepParams ShowCreepDialog() {
    CreepParams p;
    @autoreleasepool {
        const CGFloat W = 320, ROW = 26, GAP = 6, LBL = 160, TOP = 8;
        NSView* acc = MakeAccessoryView(W, TOP + 2 * (ROW + GAP) + 10);

        auto rowY = [&](int row) -> CGFloat { return TOP + (CGFloat)row * (ROW + GAP); };

        [acc addSubview:MakeLabel(@"Creep per sheet (pt):", NSMakeRect(0, rowY(1), LBL, ROW))];
        NSTextField* creepFld = MakeTextField(@"1.0", NSMakeRect(LBL, rowY(1)+2, 60, ROW-4));
        [acc addSubview:creepFld];

        [acc addSubview:MakeLabel(@"Sheets in signature:", NSMakeRect(0, rowY(0), LBL, ROW))];
        NSTextField* sheetsFld = MakeTextField(@"8", NSMakeRect(LBL, rowY(0)+2, 60, ROW-4));
        [acc addSubview:sheetsFld];

        NSAlert* alert = [[NSAlert alloc] init];
        [alert setMessageText:@"Creep"];
        [alert setInformativeText:@"Apply creep correction for booklet binding."];
        [alert addButtonWithTitle:@"Apply Creep"];
        [alert addButtonWithTitle:@"Cancel"];
        [alert setAccessoryView:acc];
        [alert layout];

        if ([alert runModal] == NSAlertFirstButtonReturn) {
            p.cancelled = false;
            p.creepPerSheet = [[creepFld stringValue] doubleValue];
            p.sheetsInSig = (std::uint32_t)std::max((NSInteger)1, [[sheetsFld stringValue] integerValue]);
        }
    }
    return p;
}

#endif // MAC_PLATFORM

// ── Execute functions ─────────────────────────────────────────────────────────

// Helper: get active PDDoc or show error
static PDDoc GetActiveDocOrError(const char* ctx) {
    AVDoc av = AVAppGetActiveDoc();
    if (!av) { ShowInfoDialog(std::string("Open a PDF in Acrobat first (") + ctx + ")."); return nullptr; }
    PDDoc pd = AVDocGetPDDoc(av);
    if (!pd) { ShowInfoDialog("No valid PDDoc available."); return nullptr; }
    if (PDDocGetNumPages(pd) <= 0) { ShowInfoDialog("Document has no pages."); return nullptr; }
    return pd;
}

// Helper: save imposition params as "last action" JSON
static void SaveLastAction(const std::string& actionType, const std::string& paramsJson) {
    std::ofstream out(GetLastActionPath());
    if (out) {
        out << "{\n  \"action\": \"" << actionType << "\",\n"
            << "  \"params\": " << paramsJson << "\n}\n";
    }
}

// ── Control Panel ──────────────────────────────────────────────────────────────

ACCB1 void ACCB2 ExecuteControlPanel(void*) {
    DURING
#ifdef MAC_PLATFORM
    ShowControlPanel();
#else
    ShowInfoDialog("Imposition Control Panel — use the Imposition menu to access features.");
#endif
    HANDLER
        ShowInfoDialog("Error opening Control Panel.");
    END_HANDLER
}

// ── Booklet ────────────────────────────────────────────────────────────────────

ACCB1 void ACCB2 ExecuteBooklet(void*) {
    DURING
        PDDoc pdDoc = GetActiveDocOrError("Booklet");
        if (!pdDoc) E_RTRN_VOID;

        const ASInt32 pageCount = PDDocGetNumPages(pdDoc);
        double pageW = 595.276, pageH = 841.890;
        GetFirstPageDimensions(pdDoc, pageW, pageH);

#ifdef MAC_PLATFORM
        const BookletParams dlg = ShowBookletDialog(pageW, pageH);
        if (dlg.cancelled) E_RTRN_VOID;
        const double shW = (dlg.sheetWidth  > 0) ? dlg.sheetWidth  : pageW * 2.0;
        const double shH = (dlg.sheetHeight > 0) ? dlg.sheetHeight : pageH;
        const double creep = dlg.creepPerSheet;
        const bool   autoPad = dlg.autoPad;
#else
        const double shW = pageW * 2.0, shH = pageH;
        const double creep = 0.0;
        const bool autoPad = true;
#endif
        const aimp::SheetSize sheetSize {shW, shH};
        aimp::BuildOptions opts {};
        opts.scaleToFit      = false;
        opts.autoRotateToFit = false;
        opts.padToMultiple   = autoPad ? 4u : 0u;
        opts.bookletCreepPerSheetPoints = creep;

        const auto plan = aimp::BookletPlanner::Build(
            "active-document", (std::uint32_t)pageCount, sheetSize, opts);

        std::error_code fse;
        const auto tmp = std::filesystem::temp_directory_path(fse);
        if (fse) { ShowInfoDialog("Cannot determine temp dir."); E_RTRN_VOID; }
        const auto outPath = (tmp / ("imposr-booklet-" + BuildUtcTimestamp() + ".pdf")).string();
        std::string err;
        if (!NativeComposePlan(pdDoc, plan, outPath, err)) {
            ShowInfoDialog("Booklet failed:\n" + err); E_RTRN_VOID;
        }
        const std::size_t sheets = plan.placements.empty() ? 0u :
            (std::size_t)plan.placements.back().sheetIndex + 1u;
        SaveLastAction("booklet",
            "{\"pages\":" + std::to_string(pageCount) +
            ",\"sheetW\":" + std::to_string(shW) +
            ",\"sheetH\":" + std::to_string(shH) + "}");
        OpenPdfInAcrobat(outPath);
        std::ostringstream msg;
        msg << "Booklet created: " << pageCount << " pages, " << sheets << " sheets.\n" << outPath;
        ShowInfoDialog(msg.str());
    HANDLER
        ShowInfoDialog("Error in Booklet.");
    END_HANDLER
}

// ── N-up Pages ─────────────────────────────────────────────────────────────────

ACCB1 void ACCB2 ExecuteNUpPages(void*) {
    DURING
        PDDoc pdDoc = GetActiveDocOrError("N-up");
        if (!pdDoc) E_RTRN_VOID;

        const ASInt32 pageCount = PDDocGetNumPages(pdDoc);
        double pageW = 595.276, pageH = 841.890;
        GetFirstPageDimensions(pdDoc, pageW, pageH);

#ifdef MAC_PLATFORM
        const NUpParams dlg = ShowNUpDialog(pageW, pageH);
        if (dlg.cancelled) E_RTRN_VOID;
        const std::uint32_t cols = dlg.cols, rows = dlg.rows;
        const double shW = (dlg.sheetWidth  > 0) ? dlg.sheetWidth  : pageW * cols;
        const double shH = (dlg.sheetHeight > 0) ? dlg.sheetHeight : pageH * rows;
        const bool scale = dlg.scaleToFit, rot = dlg.autoRotate;
#else
        constexpr std::uint32_t cols = 2, rows = 2;
        const double shW = pageW * cols, shH = pageH * rows;
        const bool scale = true, rot = true;
#endif
        const aimp::SheetSize sheetSize {shW, shH};
        aimp::BuildOptions opts {};
        opts.scaleToFit             = scale;
        opts.autoRotateToFit        = rot;
        opts.sourcePageWidthPoints  = pageW;
        opts.sourcePageHeightPoints = pageH;

        const auto plan = aimp::NUpPlanner::Build(
            "active-document", (std::uint32_t)pageCount, sheetSize, cols, rows, opts);

        std::error_code fse;
        const auto tmp = std::filesystem::temp_directory_path(fse);
        if (fse) { ShowInfoDialog("Cannot determine temp dir."); E_RTRN_VOID; }
        const auto outPath = (tmp / ("imposr-nup-" + BuildUtcTimestamp() + ".pdf")).string();
        std::string err;
        if (!NativeComposePlan(pdDoc, plan, outPath, err)) {
            ShowInfoDialog("N-up failed:\n" + err); E_RTRN_VOID;
        }
        const std::size_t sheets = plan.placements.empty() ? 0u :
            (std::size_t)plan.placements.back().sheetIndex + 1u;
        SaveLastAction("nup",
            "{\"cols\":" + std::to_string(cols) +
            ",\"rows\":" + std::to_string(rows) +
            ",\"sheetW\":" + std::to_string(shW) +
            ",\"sheetH\":" + std::to_string(shH) + "}");
        OpenPdfInAcrobat(outPath);
        std::ostringstream msg;
        msg << cols << "×" << rows << " N-up: " << pageCount << " pages, " << sheets << " sheets.\n" << outPath;
        ShowInfoDialog(msg.str());
    HANDLER
        ShowInfoDialog("Error in N-up Pages.");
    END_HANDLER
}

// ── Step & Repeat ──────────────────────────────────────────────────────────────

ACCB1 void ACCB2 ExecuteStepAndRepeat(void*) {
    DURING
        PDDoc pdDoc = GetActiveDocOrError("Step & Repeat");
        if (!pdDoc) E_RTRN_VOID;

        const ASInt32 pageCount = PDDocGetNumPages(pdDoc);
        double pageW = 595.276, pageH = 841.890;
        GetFirstPageDimensions(pdDoc, pageW, pageH);

#ifdef MAC_PLATFORM
        const StepRepeatParams dlg = ShowStepRepeatDialog(pageW, pageH);
        if (dlg.cancelled) E_RTRN_VOID;
        const std::uint32_t cols = dlg.cols, rows = dlg.rows;
        const double shW  = (dlg.sheetWidth  > 0) ? dlg.sheetWidth  : pageW * cols;
        const double shH  = (dlg.sheetHeight > 0) ? dlg.sheetHeight : pageH * rows;
        const double stepX = dlg.stepX, stepY = dlg.stepY;
#else
        constexpr std::uint32_t cols = 2, rows = 2;
        const double shW = pageW * cols, shH = pageH * rows;
        const double stepX = pageW, stepY = pageH;
#endif
        const aimp::SheetSize sheetSize {shW, shH};
        aimp::StepRepeatConfig cfg {};
        cfg.repeatX     = cols;
        cfg.repeatY     = rows;
        cfg.stepXPoints = stepX;
        cfg.stepYPoints = stepY;
        cfg.seedRect    = {0.0, 0.0, pageW, pageH};

        aimp::BuildOptions opts {};
        opts.scaleToFit             = true;
        opts.autoRotateToFit        = false;
        opts.sourcePageWidthPoints  = pageW;
        opts.sourcePageHeightPoints = pageH;

        const auto plan = aimp::StepAndRepeatPlanner::Build(
            "active-document", (std::uint32_t)pageCount, sheetSize, cfg, opts);

        std::error_code fse;
        const auto tmp = std::filesystem::temp_directory_path(fse);
        if (fse) { ShowInfoDialog("Cannot determine temp dir."); E_RTRN_VOID; }
        const auto outPath = (tmp / ("imposr-steprepeat-" + BuildUtcTimestamp() + ".pdf")).string();
        std::string err;
        if (!NativeComposePlan(pdDoc, plan, outPath, err)) {
            ShowInfoDialog("Step & Repeat failed:\n" + err); E_RTRN_VOID;
        }
        const std::size_t sheets = plan.placements.empty() ? 0u :
            (std::size_t)plan.placements.back().sheetIndex + 1u;
        SaveLastAction("steprepeat",
            "{\"cols\":" + std::to_string(cols) +
            ",\"rows\":" + std::to_string(rows) + "}");
        OpenPdfInAcrobat(outPath);
        std::ostringstream msg;
        msg << cols << "×" << rows << " Step & Repeat: " << pageCount
            << " pages, " << sheets << " sheets.\n" << outPath;
        ShowInfoDialog(msg.str());
    HANDLER
        ShowInfoDialog("Error in Step & Repeat.");
    END_HANDLER
}

// ── Join 2 Pages ───────────────────────────────────────────────────────────────

ACCB1 void ACCB2 ExecuteJoin2Pages(void*) {
    DURING
        PDDoc pdDoc = GetActiveDocOrError("Join 2 Pages");
        if (!pdDoc) E_RTRN_VOID;

        const ASInt32 pageCount = PDDocGetNumPages(pdDoc);
        double pageW = 595.276, pageH = 841.890;
        GetFirstPageDimensions(pdDoc, pageW, pageH);

        // Two-up: side-by-side, preserve order
        const aimp::SheetSize sheetSize {pageW * 2.0, pageH};
        aimp::BuildOptions opts {};
        opts.scaleToFit      = false;
        opts.autoRotateToFit = false;
        opts.padToMultiple   = 2u;

        const auto plan = aimp::TwoUpPlanner::Build(
            "active-document", (std::uint32_t)pageCount, sheetSize, opts);

        std::error_code fse;
        const auto tmp = std::filesystem::temp_directory_path(fse);
        if (fse) { ShowInfoDialog("Cannot determine temp dir."); E_RTRN_VOID; }
        const auto outPath = (tmp / ("imposr-join2-" + BuildUtcTimestamp() + ".pdf")).string();
        std::string err;
        if (!NativeComposePlan(pdDoc, plan, outPath, err)) {
            ShowInfoDialog("Join 2 pages failed:\n" + err); E_RTRN_VOID;
        }
        const std::size_t sheets = plan.placements.empty() ? 0u :
            (std::size_t)plan.placements.back().sheetIndex + 1u;
        OpenPdfInAcrobat(outPath);
        std::ostringstream msg;
        msg << "Join 2 pages: " << sheets << " spreads.\n" << outPath;
        ShowInfoDialog(msg.str());
    HANDLER
        ShowInfoDialog("Error in Join 2 Pages.");
    END_HANDLER
}

// ── Shuffle Pages ──────────────────────────────────────────────────────────────

ACCB1 void ACCB2 ExecuteShufflePages(void*) {
    DURING
        PDDoc pdDoc = GetActiveDocOrError("Shuffle Pages");
        if (!pdDoc) E_RTRN_VOID;

        const ASInt32 pageCount = PDDocGetNumPages(pdDoc);
        double pageW = 595.276, pageH = 841.890;
        GetFirstPageDimensions(pdDoc, pageW, pageH);

#ifdef MAC_PLATFORM
        const ShuffleParams dlg = ShowShuffleDialog();
        if (dlg.cancelled) E_RTRN_VOID;
        const aimp::ShuffleMode mode = dlg.mode;
        const std::uint32_t sigSize  = dlg.sigSize;
#else
        const aimp::ShuffleMode mode = aimp::ShuffleMode::Signature;
        const std::uint32_t sigSize  = 4;
#endif
        std::vector<std::uint32_t> inputPages;
        inputPages.reserve((std::size_t)pageCount);
        for (std::uint32_t i = 0; i < (std::uint32_t)pageCount; ++i) inputPages.push_back(i);

        aimp::ShuffleConfig cfg {};
        cfg.mode          = mode;
        cfg.signatureSize = sigSize;
        const auto result = aimp::ShufflePages(inputPages, cfg);

        const auto plan = BuildReorderPlan("active-document",
                                            result.orderedPages, pageW, pageH);
        std::error_code fse;
        const auto tmp = std::filesystem::temp_directory_path(fse);
        if (fse) { ShowInfoDialog("Cannot determine temp dir."); E_RTRN_VOID; }
        const auto outPath = (tmp / ("imposr-shuffle-" + BuildUtcTimestamp() + ".pdf")).string();
        std::string err;
        if (!NativeComposePlan(pdDoc, plan, outPath, err)) {
            ShowInfoDialog("Shuffle Pages failed:\n" + err); E_RTRN_VOID;
        }
        SaveLastAction("shuffle", "{\"mode\":\"" + std::to_string((int)mode) + "\"}");
        OpenPdfInAcrobat(outPath);
        std::ostringstream msg;
        msg << "Shuffle Pages: " << pageCount << " → " << result.orderedPages.size()
            << " pages (" << result.blankPagesAdded << " blank added).\n" << outPath;
        ShowInfoDialog(msg.str());
    HANDLER
        ShowInfoDialog("Error in Shuffle Pages.");
    END_HANDLER
}

// ── Shuffle Even/Odd ───────────────────────────────────────────────────────────

ACCB1 void ACCB2 ExecuteShuffleEvenOdd(void*) {
    DURING
        PDDoc pdDoc = GetActiveDocOrError("Shuffle Even/Odd");
        if (!pdDoc) E_RTRN_VOID;

        const ASInt32 pageCount = PDDocGetNumPages(pdDoc);
        double pageW = 595.276, pageH = 841.890;
        GetFirstPageDimensions(pdDoc, pageW, pageH);

        std::vector<std::uint32_t> inputPages;
        for (std::uint32_t i = 0; i < (std::uint32_t)pageCount; ++i) inputPages.push_back(i);
        const auto [evens, odds] = aimp::SplitEvenOdd(inputPages);
        const auto merged = aimp::MergeInterlace(evens, odds);
        const auto plan = BuildReorderPlan("active-document", merged, pageW, pageH);

        std::error_code fse;
        const auto tmp = std::filesystem::temp_directory_path(fse);
        if (fse) { ShowInfoDialog("Cannot determine temp dir."); E_RTRN_VOID; }
        const auto outPath = (tmp / ("imposr-evenodd-" + BuildUtcTimestamp() + ".pdf")).string();
        std::string err;
        if (!NativeComposePlan(pdDoc, plan, outPath, err)) {
            ShowInfoDialog("Shuffle Even/Odd failed:\n" + err); E_RTRN_VOID;
        }
        OpenPdfInAcrobat(outPath);
        ShowInfoDialog("Shuffle Even/Odd complete.\n" + outPath);
    HANDLER
        ShowInfoDialog("Error in Shuffle Even/Odd.");
    END_HANDLER
}

// ── Reverse Pages ─────────────────────────────────────────────────────────────

ACCB1 void ACCB2 ExecuteReversePages(void*) {
    DURING
        PDDoc pdDoc = GetActiveDocOrError("Reverse Pages");
        if (!pdDoc) E_RTRN_VOID;

        const ASInt32 pageCount = PDDocGetNumPages(pdDoc);
        double pageW = 595.276, pageH = 841.890;
        GetFirstPageDimensions(pdDoc, pageW, pageH);

        std::vector<std::uint32_t> order;
        order.reserve((std::size_t)pageCount);
        for (ASInt32 i = pageCount - 1; i >= 0; --i) order.push_back((std::uint32_t)i);
        const auto plan = BuildReorderPlan("active-document", order, pageW, pageH);

        std::error_code fse;
        const auto tmp = std::filesystem::temp_directory_path(fse);
        if (fse) { ShowInfoDialog("Cannot determine temp dir."); E_RTRN_VOID; }
        const auto outPath = (tmp / ("imposr-reverse-" + BuildUtcTimestamp() + ".pdf")).string();
        std::string err;
        if (!NativeComposePlan(pdDoc, plan, outPath, err)) {
            ShowInfoDialog("Reverse Pages failed:\n" + err); E_RTRN_VOID;
        }
        OpenPdfInAcrobat(outPath);
        ShowInfoDialog("Pages reversed.\n" + outPath);
    HANDLER
        ShowInfoDialog("Error in Reverse Pages.");
    END_HANDLER
}

// ── Insert Pages ───────────────────────────────────────────────────────────────

ACCB1 void ACCB2 ExecuteInsertPages(void*) {
    DURING
        ShowInfoDialog("Insert Pages: coming in a future update.\n\nThis feature will insert blank pages at specified positions.");
    HANDLER
        ShowInfoDialog("Error in Insert Pages.");
    END_HANDLER
}

// ── Trim & Shift ───────────────────────────────────────────────────────────────

ACCB1 void ACCB2 ExecuteTrimShift(void*) {
    DURING
        PDDoc pdDoc = GetActiveDocOrError("Trim & Shift");
        if (!pdDoc) E_RTRN_VOID;

        const ASInt32 pageCount = PDDocGetNumPages(pdDoc);
        double pageW = 595.276, pageH = 841.890;
        GetFirstPageDimensions(pdDoc, pageW, pageH);

        // Build a simple booklet plan to apply creep to
        const aimp::SheetSize sheetSize {pageW * 2.0, pageH};
        aimp::BuildOptions opts {};
        auto plan = aimp::BookletPlanner::Build(
            "active-document", (std::uint32_t)pageCount, sheetSize, opts);

        // Apply creep: 1.5pt per sheet, 8 sheets
        aimp::TrimShiftConfig cfg {};
        cfg.creepPerSheetPoints     = 1.5;
        cfg.totalSheetsInSignature  = 8;
        cfg.applyToLeftSlot         = true;
        cfg.applyToRightSlot        = true;
        cfg.direction               = aimp::CreepDirection::InwardFromSpine;
        const auto result = aimp::ApplyCreepShiftToPlan(plan, cfg);

        std::error_code fse;
        const auto tmp = std::filesystem::temp_directory_path(fse);
        if (fse) { ShowInfoDialog("Cannot determine temp dir."); E_RTRN_VOID; }
        const auto outPath = (tmp / ("imposr-trimshift-" + BuildUtcTimestamp() + ".pdf")).string();
        std::string err;
        if (!NativeComposePlan(pdDoc, plan, outPath, err)) {
            ShowInfoDialog("Trim & Shift failed:\n" + err); E_RTRN_VOID;
        }
        OpenPdfInAcrobat(outPath);
        std::ostringstream msg;
        msg << "Trim & Shift applied (" << result.sheetsAdjusted << " sheets adjusted).\n" << outPath;
        ShowInfoDialog(msg.str());
    HANDLER
        ShowInfoDialog("Error in Trim & Shift.");
    END_HANDLER
}

// ── Tile Pages ─────────────────────────────────────────────────────────────────

ACCB1 void ACCB2 ExecuteTilePages(void*) {
    DURING
        PDDoc pdDoc = GetActiveDocOrError("Tile Pages");
        if (!pdDoc) E_RTRN_VOID;

        const ASInt32 pageCount = PDDocGetNumPages(pdDoc);
        double pageW = 595.276, pageH = 841.890;
        GetFirstPageDimensions(pdDoc, pageW, pageH);

#ifdef MAC_PLATFORM
        const TilePagesParams dlg = ShowTilePagesDialog();
        if (dlg.cancelled) E_RTRN_VOID;
        const std::uint32_t cols = dlg.cols, rows = dlg.rows;
        const double overlap = dlg.overlapPoints;
#else
        constexpr std::uint32_t cols = 2, rows = 2;
        const double overlap = 18.0;
#endif
        aimp::TilePagesConfig cfg {};
        cfg.columns          = cols;
        cfg.rows             = rows;
        cfg.tileWidthPoints  = pageW / cols;
        cfg.tileHeightPoints = pageH / rows;
        cfg.overlapPoints    = overlap;
        cfg.addAlignmentMarks = false;

        const auto plan = aimp::BuildTilePlan("active-document",
                                               (std::uint32_t)pageCount,
                                               pageW, pageH, cfg);
        std::error_code fse;
        const auto tmp = std::filesystem::temp_directory_path(fse);
        if (fse) { ShowInfoDialog("Cannot determine temp dir."); E_RTRN_VOID; }
        const auto outPath = (tmp / ("imposr-tile-" + BuildUtcTimestamp() + ".pdf")).string();
        std::string err;
        if (!NativeComposePlan(pdDoc, plan, outPath, err)) {
            ShowInfoDialog("Tile Pages failed:\n" + err); E_RTRN_VOID;
        }
        OpenPdfInAcrobat(outPath);
        std::ostringstream msg;
        msg << cols << "×" << rows << " tiles created.\n" << outPath;
        ShowInfoDialog(msg.str());
    HANDLER
        ShowInfoDialog("Error in Tile Pages.");
    END_HANDLER
}

// ── Page Sizes ─────────────────────────────────────────────────────────────────

ACCB1 void ACCB2 ExecutePageSizes(void*) {
    DURING
        ShowInfoDialog("Page Sizes: coming in a future update.\n\nThis feature adjusts the media/trim/bleed boxes of individual pages.");
    HANDLER
        ShowInfoDialog("Error in Page Sizes.");
    END_HANDLER
}

// ── Page Tools ─────────────────────────────────────────────────────────────────

ACCB1 void ACCB2 ExecutePageTools(void*) {
    DURING
        ShowInfoDialog("Page Tools: coming in a future update.\n\nDuplicate, delete, move, and rotate individual pages.");
    HANDLER
        ShowInfoDialog("Error in Page Tools.");
    END_HANDLER
}

// ── Creep ──────────────────────────────────────────────────────────────────────

ACCB1 void ACCB2 ExecuteCreep(void*) {
    DURING
        PDDoc pdDoc = GetActiveDocOrError("Creep");
        if (!pdDoc) E_RTRN_VOID;

        const ASInt32 pageCount = PDDocGetNumPages(pdDoc);
        double pageW = 595.276, pageH = 841.890;
        GetFirstPageDimensions(pdDoc, pageW, pageH);

#ifdef MAC_PLATFORM
        const CreepParams dlg = ShowCreepDialog();
        if (dlg.cancelled) E_RTRN_VOID;
        const double creepPt = dlg.creepPerSheet;
        const std::uint32_t sheetsInSig = dlg.sheetsInSig;
#else
        const double creepPt = 1.5;
        const std::uint32_t sheetsInSig = 8;
#endif
        const aimp::SheetSize sheetSize {pageW * 2.0, pageH};
        aimp::BuildOptions opts {};
        auto plan = aimp::BookletPlanner::Build(
            "active-document", (std::uint32_t)pageCount, sheetSize, opts);

        aimp::TrimShiftConfig cfg {};
        cfg.creepPerSheetPoints    = creepPt;
        cfg.totalSheetsInSignature = sheetsInSig;
        cfg.applyToLeftSlot        = true;
        cfg.applyToRightSlot       = true;
        const auto result = aimp::ApplyCreepShiftToPlan(plan, cfg);

        std::error_code fse;
        const auto tmp = std::filesystem::temp_directory_path(fse);
        if (fse) { ShowInfoDialog("Cannot determine temp dir."); E_RTRN_VOID; }
        const auto outPath = (tmp / ("imposr-creep-" + BuildUtcTimestamp() + ".pdf")).string();
        std::string err;
        if (!NativeComposePlan(pdDoc, plan, outPath, err)) {
            ShowInfoDialog("Creep failed:\n" + err); E_RTRN_VOID;
        }
        OpenPdfInAcrobat(outPath);
        std::ostringstream msg;
        msg << "Creep applied (" << result.sheetsAdjusted << " sheets, max "
            << result.maxCreepApplied << "pt).\n" << outPath;
        ShowInfoDialog(msg.str());
    HANDLER
        ShowInfoDialog("Error in Creep.");
    END_HANDLER
}

// ── Sample Document ────────────────────────────────────────────────────────────

ACCB1 void ACCB2 ExecuteSampleDocument(void*) {
    DURING
        std::error_code fse;
        const auto tmp = std::filesystem::temp_directory_path(fse);
        if (fse) { ShowInfoDialog("Cannot determine temp dir."); E_RTRN_VOID; }

        aimp::SampleDocumentOptions opts {};
        opts.pageCount      = 16;
        opts.pageWidthPoints  = 595.276;
        opts.pageHeightPoints = 841.890;
        opts.numberFromOne    = true;
        opts.drawBorder       = true;
        opts.drawDiagonals    = true;

        const auto outPath = (tmp / ("imposr-sample-" + BuildUtcTimestamp() + ".pdf")).string();
        std::string err;
        if (!aimp::CreateSampleDocument(opts, outPath, err)) {
            ShowInfoDialog("Sample Document failed:\n" + err); E_RTRN_VOID;
        }
        OpenPdfInAcrobat(outPath);
        ShowInfoDialog("Sample document created (16 pages, A4).\n" + outPath);
    HANDLER
        ShowInfoDialog("Error creating Sample Document.");
    END_HANDLER
}

// ── Monitor ────────────────────────────────────────────────────────────────────

ACCB1 void ACCB2 ExecuteMonitor(void*) {
    DURING
        AVDoc avDoc = AVAppGetActiveDoc();
        std::ostringstream msg;
        msg << "Imposition Monitor\n\n";
        if (!avDoc) {
            msg << "No document open.";
        } else {
            PDDoc pdDoc = AVDocGetPDDoc(avDoc);
            const ASInt32 pageCount = pdDoc ? PDDocGetNumPages(pdDoc) : 0;
            msg << "Active document: " << pageCount << " pages\n";
            double pageW = 0, pageH = 0;
            if (pdDoc) GetFirstPageDimensions(pdDoc, pageW, pageH);
            msg << "First page: " << pageW << " × " << pageH << " pt\n";
        }
        // Show last action
        std::ifstream last(GetLastActionPath());
        if (last) {
            std::ostringstream buf; buf << last.rdbuf();
            msg << "\nLast action:\n" << buf.str();
        } else {
            msg << "\nNo remembered action.";
        }
        ShowInfoDialog(msg.str());
    HANDLER
        ShowInfoDialog("Error in Monitor.");
    END_HANDLER
}

// ── Stick On: Text & Numbers ───────────────────────────────────────────────────

ACCB1 void ACCB2 ExecuteStickOnText(void*) {
    DURING
        ShowInfoDialog("Stick On: Text & Numbers — coming in a future update.\n\nAdd page numbers, Bates numbers, or custom text overlays to imposed sheets.");
    HANDLER
        ShowInfoDialog("Error in Stick On: Text & Numbers.");
    END_HANDLER
}

// ── Stick On: PDF Pages ────────────────────────────────────────────────────────

ACCB1 void ACCB2 ExecuteStickOnPdf(void*) {
    DURING
        ShowInfoDialog("Stick On: PDF Pages — coming in a future update.\n\nPlace a PDF page as an overlay on imposed sheets.");
    HANDLER
        ShowInfoDialog("Error in Stick On: PDF Pages.");
    END_HANDLER
}

// ── Stick On: Masking Tape ─────────────────────────────────────────────────────

ACCB1 void ACCB2 ExecuteStickOnMasking(void*) {
    DURING
        ShowInfoDialog("Stick On: Masking Tape — coming in a future update.\n\nCover areas of the output sheet.");
    HANDLER
        ShowInfoDialog("Error in Stick On: Masking Tape.");
    END_HANDLER
}

// ── Peel Off: Text & Numbers ───────────────────────────────────────────────────

ACCB1 void ACCB2 ExecutePeelOffText(void*) {
    DURING
        ShowInfoDialog("Peel Off: Text & Numbers — coming in a future update.\n\nRemove previously applied text/number overlays.");
    HANDLER
        ShowInfoDialog("Error in Peel Off: Text & Numbers.");
    END_HANDLER
}

// ── Peel Off: Masking Tape ─────────────────────────────────────────────────────

ACCB1 void ACCB2 ExecutePeelOffMasking(void*) {
    DURING
        ShowInfoDialog("Peel Off: Masking Tape — coming in a future update.");
    HANDLER
        ShowInfoDialog("Error in Peel Off: Masking Tape.");
    END_HANDLER
}

// ── Registration Marks ─────────────────────────────────────────────────────────

ACCB1 void ACCB2 ExecuteRegistrationMarks(void*) {
    DURING
        ShowInfoDialog("Registration Marks — coming in a future update.\n\nAdd crop marks, registration marks, and color bars to output sheets.");
    HANDLER
        ShowInfoDialog("Error in Registration Marks.");
    END_HANDLER
}

// ── Sequences ─────────────────────────────────────────────────────────────────

ACCB1 void ACCB2 ExecuteSequences(void*) {
    DURING
        ShowInfoDialog("Sequences Manager — coming in a future update.\n\nRecord, save, and replay imposition sequences.\n\nUse 'Remember last' and 'Playback' for quick replay.");
    HANDLER
        ShowInfoDialog("Error in Sequences.");
    END_HANDLER
}

// ── Playback ──────────────────────────────────────────────────────────────────

ACCB1 void ACCB2 ExecutePlayback(void*) {
    DURING
        const std::string lastPath = GetLastActionPath();
        std::ifstream in(lastPath);
        if (!in) {
            ShowInfoDialog("No remembered action. Use 'Remember last' after an operation.");
            E_RTRN_VOID;
        }
        std::ostringstream buf; buf << in.rdbuf();
        const std::string json = buf.str();

        // Parse action type from JSON: "action": "booklet"|"nup"|"steprepeat"|"shuffle"
        const auto findVal = [&](const std::string& key) -> std::string {
            const auto pos = json.find("\"" + key + "\":");
            if (pos == std::string::npos) return {};
            const auto vpos = json.find('"', pos + key.size() + 3);
            if (vpos == std::string::npos) return {};
            const auto vend = json.find('"', vpos + 1);
            if (vend == std::string::npos) return {};
            return json.substr(vpos + 1, vend - vpos - 1);
        };
        const std::string action = findVal("action");
        if (action == "booklet")    { ExecuteBooklet(nullptr); }
        else if (action == "nup")   { ExecuteNUpPages(nullptr); }
        else if (action == "steprepeat") { ExecuteStepAndRepeat(nullptr); }
        else if (action == "shuffle")    { ExecuteShufflePages(nullptr); }
        else {
            ShowInfoDialog("Last action: " + action + "\nNo playback defined for this action yet.");
        }
    HANDLER
        ShowInfoDialog("Error in Playback.");
    END_HANDLER
}

// ── Remember Last ─────────────────────────────────────────────────────────────

ACCB1 void ACCB2 ExecuteRememberLast(void*) {
    DURING
        const std::string path = GetLastActionPath();
        std::ifstream in(path);
        if (!in) {
            ShowInfoDialog("No action to remember yet. Run an imposition first.");
            E_RTRN_VOID;
        }
        std::ostringstream buf; buf << in.rdbuf();
        ShowInfoDialog("Remembered action saved:\n" + path + "\n\n" + buf.str());
    HANDLER
        ShowInfoDialog("Error in Remember Last.");
    END_HANDLER
}

// ── Preferences ───────────────────────────────────────────────────────────────

ACCB1 void ACCB2 ExecutePreferences(void*) {
    DURING
#ifdef MAC_PLATFORM
        @autoreleasepool {
            const CGFloat W = 360, ROW = 26, GAP = 6, LBL = 180, TOP = 8;
            NSView* acc = MakeAccessoryView(W, TOP + 4 * (ROW + GAP) + 10);
            auto rowY = [&](int row) -> CGFloat { return TOP + (CGFloat)row * (ROW + GAP); };

            [acc addSubview:MakeLabel(@"Default sheet size:", NSMakeRect(0, rowY(3), LBL, ROW))];
            NSPopUpButton* sheetPopup = AddSheetSizePopup(acc, NSMakeRect(LBL, rowY(3), W-LBL, ROW));

            NSButton* trimChk = MakeCheckbox(@"Draw trim marks by default", NO,
                                              NSMakeRect(0, rowY(2), W, ROW));
            [acc addSubview:trimChk];

            NSButton* bleedChk = MakeCheckbox(@"Draw bleed box by default", NO,
                                               NSMakeRect(0, rowY(1), W, ROW));
            [acc addSubview:bleedChk];

            [acc addSubview:MakeLabel(@"Bleed (pt):", NSMakeRect(0, rowY(0), LBL, ROW))];
            NSTextField* bleedFld = MakeTextField(@"3", NSMakeRect(LBL, rowY(0)+2, 60, ROW-4));
            [acc addSubview:bleedFld];

            NSAlert* alert = [[NSAlert alloc] init];
            [alert setMessageText:@"Preferences"];
            [alert setInformativeText:@"Imposition plug-in preferences."];
            [alert addButtonWithTitle:@"OK"];
            [alert addButtonWithTitle:@"Cancel"];
            [alert setAccessoryView:acc];
            [alert layout];

            if ([alert runModal] == NSAlertFirstButtonReturn) {
                aimp::PlannerPreset preset {};
                std::string err;
                if (!aimp::LoadPreset(GetPresetPath(), preset, err)) BuildDefaultPreset(preset);
                const auto [sw, sh] = SheetFromPopup(sheetPopup, 1190.551, 841.890);
                if (sw > 0) { preset.sheetSize.widthPoints = sw; preset.sheetSize.heightPoints = sh; }
                preset.pdfOptions.drawTrimMarks = ([trimChk state] == NSControlStateValueOn);
                preset.pdfOptions.drawBleedBox  = ([bleedChk state] == NSControlStateValueOn);
                preset.pdfOptions.bleedPoints   = [[bleedFld stringValue] doubleValue];
                (void)aimp::SavePreset(preset, GetPresetPath(), err);
                ShowInfoDialog("Preferences saved.");
            }
        }
#else
        ShowInfoDialog("Preferences: edit the preset file at:\n" + GetPresetPath());
#endif
    HANDLER
        ShowInfoDialog("Error in Preferences.");
    END_HANDLER
}

// ── Customize Panel ────────────────────────────────────────────────────────────

ACCB1 void ACCB2 ExecuteCustomizePanel(void*) {
    DURING
        ShowInfoDialog("Customize Panel — coming in a future update.\n\nReorder and show/hide buttons in the Control Panel.");
    HANDLER
        ShowInfoDialog("Error in Customize Panel.");
    END_HANDLER
}

// ── Help / About ───────────────────────────────────────────────────────────────

ACCB1 void ACCB2 ExecuteHelpAbout(void*) {
    DURING
        ShowInfoDialog(
            "Imposr — PDF Imposition Plug-in\n"
            "Version 0.1.0\n\n"
            "Features:\n"
            "  \xE2\x80\xA2 Booklet (saddle stitch)\n"
            "  \xE2\x80\xA2 N-up pages\n"
            "  \xE2\x80\xA2 Step & Repeat\n"
            "  \xE2\x80\xA2 Join 2 pages\n"
            "  \xE2\x80\xA2 Shuffle / Reverse pages\n"
            "  \xE2\x80\xA2 Tile pages\n"
            "  \xE2\x80\xA2 Creep / Trim & Shift\n"
            "  \xE2\x80\xA2 Sample document\n\n"
            "All output is created as a NEW PDF \xe2\x80\x94 original is never modified.\n\n"
            "https://github.com/AchimPieters/Imposr"
        );
    HANDLER
        ShowInfoDialog("Error in Help / About.");
    END_HANDLER
}

// ── Menu registration ──────────────────────────────────────────────────────────

static AVMenuItem AddItem(AVMenu menu, const char* title, const char* name,
                          AVExecuteProc* procSlot, ACCB1 void (ACCB2 *fn)(void*)) {
    *procSlot = ASCallbackCreateProto(AVExecuteProc, fn);
    AVMenuItem item = AVMenuItemNew(title, name, nullptr, true, NO_SHORTCUT, 0, nullptr, nullptr);
    if (!item) return nullptr;
    AVMenuItemSetExecuteProc(item, *procSlot, nullptr);
    AVMenuAddMenuItem(menu, item, APPEND_MENUITEM);
    return item;
}

static void AddSeparator(AVMenu menu, const char* name) {
    AVMenuItem sep = AVMenuItemNew("-", name, nullptr, true, NO_SHORTCUT, 0, nullptr, nullptr);
    if (sep) AVMenuAddMenuItem(menu, sep, APPEND_MENUITEM);
}

bool RegisterMenus() {
    AVMenubar mb = AVAppGetMenubar();
    if (!mb) return false;

    gImpositionMenu = AVMenuNew(kMenuName, kExtensionName, nullptr);
    if (!gImpositionMenu) return false;
    AVMenubarAddMenu(mb, gImpositionMenu, APPEND_MENU);

    // ── Control ──────────────────────────────────────────────────────────────
    gMI_ControlPanel = AddItem(gImpositionMenu, kMI_ControlPanel,
        "AIMP:ControlPanel", &gProc_ControlPanel, ExecuteControlPanel);

    AddSeparator(gImpositionMenu, "AIMP:Sep_Easy");

    // ── Easy Imposition ───────────────────────────────────────────────────────
    gMI_Booklet       = AddItem(gImpositionMenu, kMI_Booklet,    "AIMP:Booklet",    &gProc_Booklet,      ExecuteBooklet);
    gMI_NUpPages      = AddItem(gImpositionMenu, kMI_NUpPages,   "AIMP:NUpPages",   &gProc_NUpPages,     ExecuteNUpPages);
    gMI_StepRepeat    = AddItem(gImpositionMenu, kMI_StepRepeat, "AIMP:StepRepeat", &gProc_StepRepeat,   ExecuteStepAndRepeat);
    gMI_Join2Pages    = AddItem(gImpositionMenu, kMI_Join2Pages, "AIMP:Join2Pages", &gProc_Join2Pages,   ExecuteJoin2Pages);

    AddSeparator(gImpositionMenu, "AIMP:Sep_PageMgmt");

    // ── Page Management ───────────────────────────────────────────────────────
    gMI_ShufflePages  = AddItem(gImpositionMenu, kMI_ShufflePages,  "AIMP:ShufflePages",   &gProc_ShufflePages,  ExecuteShufflePages);
    gMI_ShuffleEvenOdd= AddItem(gImpositionMenu, kMI_ShuffleEvenOdd,"AIMP:ShuffleEvenOdd", &gProc_ShuffleEvenOdd,ExecuteShuffleEvenOdd);
    gMI_ReversePages  = AddItem(gImpositionMenu, kMI_ReversePages,  "AIMP:ReversePages",   &gProc_ReversePages,  ExecuteReversePages);
    gMI_InsertPages   = AddItem(gImpositionMenu, kMI_InsertPages,   "AIMP:InsertPages",    &gProc_InsertPages,   ExecuteInsertPages);
    gMI_TrimShift     = AddItem(gImpositionMenu, kMI_TrimShift,     "AIMP:TrimShift",      &gProc_TrimShift,     ExecuteTrimShift);
    gMI_TilePages     = AddItem(gImpositionMenu, kMI_TilePages,     "AIMP:TilePages",      &gProc_TilePages,     ExecuteTilePages);
    gMI_PageSizes     = AddItem(gImpositionMenu, kMI_PageSizes,     "AIMP:PageSizes",      &gProc_PageSizes,     ExecutePageSizes);
    gMI_PageTools     = AddItem(gImpositionMenu, kMI_PageTools,     "AIMP:PageTools",      &gProc_PageTools,     ExecutePageTools);

    AddSeparator(gImpositionMenu, "AIMP:Sep_Advanced");

    // ── Advanced ─────────────────────────────────────────────────────────────
    gMI_Creep          = AddItem(gImpositionMenu, kMI_Creep,         "AIMP:Creep",         &gProc_Creep,         ExecuteCreep);
    gMI_SampleDocument = AddItem(gImpositionMenu, kMI_SampleDocument,"AIMP:SampleDocument",&gProc_SampleDocument,ExecuteSampleDocument);
    gMI_Monitor        = AddItem(gImpositionMenu, kMI_Monitor,       "AIMP:Monitor",       &gProc_Monitor,       ExecuteMonitor);

    AddSeparator(gImpositionMenu, "AIMP:Sep_StickOn");

    // ── Stick On ─────────────────────────────────────────────────────────────
    gMI_StickText    = AddItem(gImpositionMenu, kMI_StickText,   "AIMP:StickText",    &gProc_StickText,    ExecuteStickOnText);
    gMI_StickPdf     = AddItem(gImpositionMenu, kMI_StickPdf,    "AIMP:StickPdf",     &gProc_StickPdf,     ExecuteStickOnPdf);
    gMI_StickMasking = AddItem(gImpositionMenu, kMI_StickMasking,"AIMP:StickMasking", &gProc_StickMasking, ExecuteStickOnMasking);

    AddSeparator(gImpositionMenu, "AIMP:Sep_PeelOff");

    // ── Peel Off ──────────────────────────────────────────────────────────────
    gMI_PeelText    = AddItem(gImpositionMenu, kMI_PeelText,    "AIMP:PeelText",    &gProc_PeelText,    ExecutePeelOffText);
    gMI_PeelMasking = AddItem(gImpositionMenu, kMI_PeelMasking, "AIMP:PeelMasking", &gProc_PeelMasking, ExecutePeelOffMasking);
    gMI_RegMarks    = AddItem(gImpositionMenu, kMI_RegMarks,    "AIMP:RegMarks",    &gProc_RegMarks,    ExecuteRegistrationMarks);

    AddSeparator(gImpositionMenu, "AIMP:Sep_Automation");

    // ── Automation ────────────────────────────────────────────────────────────
    gMI_Sequences    = AddItem(gImpositionMenu, kMI_Sequences,    "AIMP:Sequences",    &gProc_Sequences,    ExecuteSequences);
    gMI_Playback     = AddItem(gImpositionMenu, kMI_Playback,     "AIMP:Playback",     &gProc_Playback,     ExecutePlayback);
    gMI_RememberLast = AddItem(gImpositionMenu, kMI_RememberLast, "AIMP:RememberLast", &gProc_RememberLast, ExecuteRememberLast);

    AddSeparator(gImpositionMenu, "AIMP:Sep_Settings");

    // ── Settings ─────────────────────────────────────────────────────────────
    gMI_Preferences    = AddItem(gImpositionMenu, kMI_Preferences,    "AIMP:Preferences",   &gProc_Preferences,   ExecutePreferences);
    gMI_CustomizePanel = AddItem(gImpositionMenu, kMI_CustomizePanel, "AIMP:CustomizePanel",&gProc_CustomizePanel,ExecuteCustomizePanel);
    gMI_HelpAbout      = AddItem(gImpositionMenu, kMI_HelpAbout,      "AIMP:HelpAbout",     &gProc_HelpAbout,     ExecuteHelpAbout);

    return true;
}

// ── PluginUnload ──────────────────────────────────────────────────────────────

void UnloadMenus() {
    AVMenuItem* items[] = {
        &gMI_ControlPanel, &gMI_Booklet, &gMI_NUpPages, &gMI_StepRepeat, &gMI_Join2Pages,
        &gMI_ShufflePages, &gMI_ShuffleEvenOdd, &gMI_ReversePages, &gMI_InsertPages,
        &gMI_TrimShift, &gMI_TilePages, &gMI_PageSizes, &gMI_PageTools,
        &gMI_Creep, &gMI_SampleDocument, &gMI_Monitor,
        &gMI_StickText, &gMI_StickPdf, &gMI_StickMasking,
        &gMI_PeelText, &gMI_PeelMasking, &gMI_RegMarks,
        &gMI_Sequences, &gMI_Playback, &gMI_RememberLast,
        &gMI_Preferences, &gMI_CustomizePanel, &gMI_HelpAbout,
    };
    for (auto* slot : items) {
        if (*slot) { AVMenuItemRemove(*slot); *slot = nullptr; }
    }
    if (gImpositionMenu) { AVMenuRelease(gImpositionMenu); gImpositionMenu = nullptr; }
}

} // namespace

// ── Plugin entrypoints ─────────────────────────────────────────────────────────

extern "C" ACCB1 ASBool ACCB2 PluginExportHFTs(void) { return true; }

extern "C" ACCB1 ASBool ACCB2 PluginImportReplaceAndRegister(void) { return true; }

extern "C" ACCB1 ASBool ACCB2 PluginInit(void) {
    return RegisterMenus() ? true : false;
}

extern "C" ACCB1 ASBool ACCB2 PluginUnload(void) {
#ifdef MAC_PLATFORM
    if (gControlPanel) { [gControlPanel close]; gControlPanel = nil; }
    if (gPanelDelegate) { gPanelDelegate = nil; }
#endif
    UnloadMenus();
    return true;
}

extern "C" ACCB1 const char* ACCB2 GetExtensionName(void) {
    return kExtensionName;
}

extern "C" ACCB1 ASBool ACCB2 PIHandshake(ASUns32 handshakeVersion, void* handshakeData) {
    if (handshakeVersion == HANDSHAKE_V0200) {
        PIHandshakeData_V0200* hsd = (PIHandshakeData_V0200*)handshakeData;
        hsd->extensionName = ASAtomFromString(GetExtensionName());
        hsd->exportHFTsCallback =
            (void*)ASCallbackCreateProto(PIExportHFTsProcType, &PluginExportHFTs);
        hsd->importReplaceAndRegisterCallback =
            (void*)ASCallbackCreateProto(PIImportReplaceAndRegisterProcType, &PluginImportReplaceAndRegister);
        hsd->initCallback =
            (void*)ASCallbackCreateProto(PIInitProcType, &PluginInit);
        hsd->unloadCallback =
            (void*)ASCallbackCreateProto(PIUnloadProcType, &PluginUnload);
        return true;
    }
    return false;
}
