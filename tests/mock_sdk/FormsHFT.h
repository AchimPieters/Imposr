// Mock FormsHFT.h — stubs AFExecuteThisScript for CI compilation.
#pragma once
#include "PIHeaders.h"

#define AcroFormHFT_NAME    "Forms"
#define AcroFormHFT_LATEST_VERSION 0x00010002u

// Init macro: returns nullptr (HFT not available in mock build).
#define Init_AcroFormHFT nullptr

// AFExecuteThisScript: stub always returns false / no output.
static inline ASBool AFExecuteThisScript(PDDoc, const char*, char** out) {
    if (out) *out = nullptr;
    return false;
}
