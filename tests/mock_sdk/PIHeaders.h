// Mock Acrobat SDK headers for CI compilation.
// Provides type stubs and no-op function definitions so ImposePlugin.cpp compiles
// without the real SDK. No functionality is provided — this is compile-only.
#pragma once
#include <cstdint>
#include <cstdlib>
#include <cstring>

// ─── Calling convention macros ────────────────────────────────────────────────
#define ACCB1
#define ACCB2
#define DURING          {
#define HANDLER         } if (false) {
#define END_HANDLER     }
#define E_RTRN_VOID     return;
#define E_RETURN(v)     return (v);
#define APPEND_MENU     0
#define APPEND_MENUITEM 0
#define NO_SHORTCUT     0
#define HANDSHAKE_V0200 0x0200u

// ─── Primitive types ──────────────────────────────────────────────────────────
typedef int            ASInt32;
typedef unsigned int   ASUns32;
typedef unsigned char  ASBool;
typedef float          ASReal;
typedef int            ASFixed;
typedef unsigned int   ASFlags;
typedef int            ASEnum8;
typedef unsigned short ASEnum16;
typedef unsigned int   AVFlagBits32;
typedef const char*    ASAtom;
typedef void*          HFT;

#define fixedZero  0
#define fixedOne   65536   // 1.0 in 16.16 fixed-point

#define ASFixed_ONE  fixedOne
#define kASFixedZero fixedZero

static inline ASFixed   ASFloatToFixed(float v)     { return (ASFixed)(v * 65536.0f); }
static inline float     ASFixedToFloat(ASFixed v)   { return (float)v / 65536.0f; }
#define ASFixed_ONE fixedOne

// ─── Opaque handle types ──────────────────────────────────────────────────────
struct _PDDoc;   typedef struct _PDDoc*   PDDoc;
struct _PDPage;  typedef struct _PDPage*  PDPage;
struct _AVDoc;   typedef struct _AVDoc*   AVDoc;
struct _AVMenu;  typedef struct _AVMenu*  AVMenu;
struct _AVMenuItem; typedef struct _AVMenuItem* AVMenuItem;
struct _AVMenubar;  typedef struct _AVMenubar*  AVMenubar;
struct _ASFileSys;  typedef struct _ASFileSys*  ASFileSys;
struct _ASPathName; typedef struct _ASPathName* ASPathName;
struct _PDEContent; typedef struct _PDEContent* PDEContent;
struct _PDEForm;    typedef struct _PDEForm*    PDEForm;
struct _PDEObject;  typedef struct _PDEObject*  PDEObject;
struct _PDEElement; typedef struct _PDEElement* PDEElement;
struct _AVPlatformWindowRef; typedef struct _AVPlatformWindowRef* AVPlatformWindowRef;
struct _AVWindow; typedef struct _AVWindow* AVWindow;

typedef void (*AVExecuteProc)(void*);

struct CosObj { int type; };
static inline int CosObjGetType(CosObj o) { return o.type; }
#define CosNull  0
#define CosArray 1

struct _CosDoc; typedef struct _CosDoc* CosDoc;

static inline CosDoc  PDDocGetCosDoc(PDDoc)                              { return nullptr; }
static inline CosObj  CosDocGetRoot(CosDoc)                              { CosObj o{}; return o; }
static inline CosObj  CosDictGet(CosObj, ASAtom)                         { CosObj o{}; return o; }
static inline ASInt32 CosArrayLength(CosObj)                             { return 0; }
static inline CosObj  CosArrayGet(CosObj, ASInt32)                       { CosObj o{}; return o; }
static inline void    CosArrayPut(CosObj, ASInt32, CosObj)               {}
static inline void    CosDictPut(CosObj, ASAtom, CosObj)                 {}
static inline CosObj  CosObjCopy(CosObj, CosDoc, ASBool)                 { CosObj o{}; return o; }
static inline CosObj  CosNewArray(CosDoc, ASBool, ASInt32)               { CosObj o{}; return o; }

struct ASFixedRect  { ASFixed left, top, right, bottom; };
struct ASFixedMatrix { ASFixed a, b, c, d, h, v; };

struct PDEContentAttrs {
    ASUns32 flags;
    int     formType;
    ASFixedRect bbox;
    ASFixedMatrix matrix;
};

#define kPDEContentToForm 0x0002u
#define kPDEAfterLast     ((ASInt32)-1)
#define PDSaveFull              0x0001
#define PDSaveCollectGarbage    0x0040

struct PIHandshakeData_V0200 {
    void* extensionName;
    void* exportHFTsCallback;
    void* importReplaceAndRegisterCallback;
    void* initCallback;
    void* unloadCallback;
};

typedef ASBool (*PIExportHFTsProcType)(void);
typedef ASBool (*PIImportReplaceAndRegisterProcType)(void);
typedef ASBool (*PIInitProcType)(void);
typedef ASBool (*PIUnloadProcType)(void);

// ─── Macros for callback creation ────────────────────────────────────────────
#define ASCallbackCreateProto(type, fn)  ((type)(fn))

// ─── Atom / string ───────────────────────────────────────────────────────────
static inline ASAtom  ASAtomFromString(const char* s) { return s; }

// ─── ASFileSys ───────────────────────────────────────────────────────────────
static inline ASFileSys  ASGetDefaultFileSys()                           { return nullptr; }
static inline ASPathName ASFileSysCreatePathName(ASFileSys, ASAtom, const void*, void*) { return nullptr; }
static inline void       ASFileSysReleasePath(ASFileSys, ASPathName)     {}

// ─── AVApp ───────────────────────────────────────────────────────────────────
static inline AVDoc      AVAppGetActiveDoc()                             { return nullptr; }
static inline AVMenubar  AVAppGetMenubar()                               { return nullptr; }

// ─── AVDoc ───────────────────────────────────────────────────────────────────
static inline PDDoc  AVDocGetPDDoc(AVDoc)                                { return nullptr; }
static inline AVDoc  AVDocOpenFromFile(ASPathName, ASFileSys, void*)     { return nullptr; }

// ─── AVMenu ──────────────────────────────────────────────────────────────────
static inline AVMenu     AVMenuNew(const char*, const char*, void*)      { return nullptr; }
static inline void       AVMenubarAddMenu(AVMenubar, AVMenu, ASInt32)    {}
static inline void       AVMenuAddMenuItem(AVMenu, AVMenuItem, ASInt32)  {}
static inline void       AVMenuRelease(AVMenu)                           {}

// ─── AVMenuItem ──────────────────────────────────────────────────────────────
static inline AVMenuItem AVMenuItemNew(const char*, const char*, void*, ASBool, int, int, void*, void*) { return nullptr; }
static inline void       AVMenuItemSetExecuteProc(AVMenuItem, AVExecuteProc, void*) {}
static inline void       AVMenuItemRemove(AVMenuItem)                    {}

// ─── AVAlert ─────────────────────────────────────────────────────────────────
static inline void   AVAlertNote(const char*)                            {}
static inline ASBool AVAlertConfirm(const char*)                         { return false; }

// ─── PDDoc ───────────────────────────────────────────────────────────────────
static inline PDDoc    PDDocCreate()                                     { return nullptr; }
static inline void     PDDocClose(PDDoc)                                 {}
static inline ASInt32  PDDocGetNumPages(PDDoc)                           { return 0; }
static inline PDPage   PDDocAcquirePage(PDDoc, ASInt32)                  { return nullptr; }
static inline PDPage   PDDocCreatePage(PDDoc, ASInt32, ASFixedRect)      { return nullptr; }
static inline void     PDDocSave(PDDoc, int, ASPathName, void*, void*, void*) {}
// PDDocGetCosDoc is declared earlier with CosDoc return type.

// ─── PDPage ──────────────────────────────────────────────────────────────────
static inline void        PDPageRelease(PDPage)                          {}
static inline void        PDPageGetCropBox(PDPage, ASFixedRect*)         {}
static inline ASInt32     PDPageGetNumAnnots(PDPage)                     { return 0; }
static inline void        PDPageRemoveAnnot(PDPage, ASInt32)             {}
static inline PDEContent  PDPageAcquirePDEContent(PDPage, ASInt32)       { return nullptr; }
static inline void        PDPageReleasePDEContent(PDPage, ASInt32)       {}
static inline void        PDPageSetPDEContent(PDPage, ASInt32)           {}

// ─── PDEContent ──────────────────────────────────────────────────────────────
static inline void  PDEContentToCosObj(PDEContent, ASUns32, PDEContentAttrs*, ASUns32, void*, void*, CosObj*, CosObj*) {}
static inline void  PDEContentAddElem(PDEContent, ASInt32, PDEElement)   {}

// ─── PDEForm ─────────────────────────────────────────────────────────────────
static inline PDEForm PDEFormCreateFromCosObj(CosObj*, CosObj*, ASFixedMatrix*) { return nullptr; }
static inline void    PDERelease(PDEObject)                              {}

// ─── Extension manager ───────────────────────────────────────────────────────
static inline HFT ASExtensionMgrGetHFT(ASAtom, ASUns32) { return nullptr; }
static inline void ASfree(void* p) { free(p); }

// ─── PIMain glue ─────────────────────────────────────────────────────────────
// PIMain.c normally provides these; the mock provides empty stubs.
static inline void* PIGetSDKVersion() { return nullptr; }
