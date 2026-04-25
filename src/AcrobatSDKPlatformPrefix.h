/* Force-included before PIMain.c and ImposePlugin.cpp to satisfy
   Environ.h's PLATFORM and PRODUCT string-macro requirements.
   Mirrors what the SDK's own Xcode project does via GCC_PREFIX_HEADER = PIHeaders++.pch */
#if defined(_WIN32) || defined(WIN_PLATFORM)
#  define WIN_PLATFORM 1
#  define PLUGIN 1
#  define PLATFORM "WinPlatform.h"
#  ifndef PRODUCT
#    define PRODUCT "Plugin.h"
#  endif
#elif defined(__APPLE__) || defined(MAC_PLATFORM)
#  ifndef MAC_PLATFORM
#    define MAC_PLATFORM 1
#  endif
#  define PLUGIN 1
#  define PLATFORM "MacPlatform.h"
#  ifndef PRODUCT
#    define PRODUCT "Plugin.h"
#  endif
/* PIMain.h uses CFBundleRef (CoreFoundation) and ResFileRefNum (CarbonCore).
   Include the frameworks before the SDK headers process them. */
#  ifdef __OBJC__
#    import <Foundation/Foundation.h>
#  endif
#  include <CoreFoundation/CoreFoundation.h>
#  include <CoreServices/CoreServices.h>
#endif
