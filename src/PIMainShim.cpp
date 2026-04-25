// Adobe requires the SDK support file PIMain.c to be part of every plug-in.
// This shim includes it from the SDK installation path.
//
// PIMain.c → Environ.h requires PLATFORM to be defined (e.g. "MacPlatform.h").
// Include the platform-specific prefix header first so that macro is in place.
#if defined(_WIN32) || defined(WIN_PLATFORM)
#  include "WinPIHeaders.h"
#elif defined(__APPLE__) || defined(MAC_PLATFORM)
#  include "MacPIHeaders.h"
#endif

extern "C" {
#include "PIMain.c"
}
