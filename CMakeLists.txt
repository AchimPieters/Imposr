cmake_minimum_required(VERSION 3.21)
project(AcrobatImpositionPlugin LANGUAGES C CXX)
set(AIMP_VERSION "0.1.0")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)

option(AIMP_BUILD_PLUGIN "Build the Acrobat plug-in target." ON)
option(AIMP_BUILD_TESTS "Build planner tests." ON)
option(AIMP_BUILD_CLI "Build the CLI app." ON)
option(AIMP_NATIVE_PDF_COMPOSER "Enable native Acrobat SDK PDF composer (PDEFormCreate/PDEContentAddElem imposition)." ON)
# Backward-compat alias kept so existing build scripts that set -DAIMP_ENABLE_EXPERIMENTAL_SDK_COMPOSER=ON still work.
if(AIMP_ENABLE_EXPERIMENTAL_SDK_COMPOSER AND NOT AIMP_NATIVE_PDF_COMPOSER)
  set(AIMP_NATIVE_PDF_COMPOSER ON CACHE BOOL "" FORCE)
endif()
option(AIMP_ENABLE_WARNINGS "Enable compiler warnings for all targets." ON)
option(AIMP_WARNINGS_AS_ERRORS "Treat compiler warnings as errors." OFF)
option(AIMP_ENABLE_SANITIZERS "Enable address/undefined sanitizers for supported compilers." OFF)
option(AIMP_ENABLE_PACKAGING "Enable install/CPack packaging targets." ON)
option(AIMP_DEPLOY_PLUGIN_TO_ACROBAT_ON_INSTALL "Deploy plugin into Acrobat plug-in directory during install/package install when possible." ON)

if(WIN32)
  set(AIMP_ACROBAT_PLUGIN_INSTALL_DIR_DEFAULT "C:/Program Files/Adobe/Acrobat DC/Acrobat/plug_ins")
elseif(APPLE)
  set(AIMP_ACROBAT_PLUGIN_INSTALL_DIR_DEFAULT "/Applications/Adobe Acrobat DC/Adobe Acrobat.app/Contents/Plug-ins")
else()
  set(AIMP_ACROBAT_PLUGIN_INSTALL_DIR_DEFAULT "")
endif()
set(AIMP_ACROBAT_PLUGIN_INSTALL_DIR "${AIMP_ACROBAT_PLUGIN_INSTALL_DIR_DEFAULT}" CACHE PATH "Destination folder for Acrobat plug-in deployment.")

if(NOT DEFINED ACROBAT_SDK_DIR)
  if(DEFINED ENV{ACROBAT_SDK_DIR})
    set(ACROBAT_SDK_DIR "$ENV{ACROBAT_SDK_DIR}")
  endif()
endif()

# Auto-detect SDK from well-known locations when not explicitly set.
# Expected layout (both Mac and Windows SDKs):
#   <SDK root>/PluginSupport/Headers/SDK/PIHeaders.h
#   <SDK root>/PluginSupport/Headers/API/PIMain.c
if(NOT ACROBAT_SDK_DIR)
  set(_sdk_candidates
    "$ENV{HOME}/Adobe/AcrobatSDK"
    "$ENV{HOME}/Downloads/AcrobatSDK"
    "$ENV{HOME}/Desktop/AcrobatSDK"
    "${CMAKE_SOURCE_DIR}/AcrobatSDK"
  )
  # Windows SDK bundled in repo
  if(WIN32)
    list(APPEND _sdk_candidates
      "${CMAKE_SOURCE_DIR}/Acrobat_DC_SDK_Windows_2021_v3/Adobe/Acrobat DC SDK/Version 1"
    )
  endif()
  foreach(_c IN LISTS _sdk_candidates)
    if(EXISTS "${_c}/PluginSupport/Headers/SDK/PIHeaders.h")
      set(ACROBAT_SDK_DIR "${_c}")
      message(STATUS "Auto-detected Acrobat SDK at: ${ACROBAT_SDK_DIR}")
      break()
    endif()
  endforeach()
endif()

if(AIMP_BUILD_PLUGIN AND NOT ACROBAT_SDK_DIR)
  message(WARNING "ACROBAT_SDK_DIR is not set. Disabling plug-in build.")
  set(AIMP_BUILD_PLUGIN OFF)
endif()

if(ACROBAT_SDK_DIR)
  message(STATUS "Using Acrobat SDK at: ${ACROBAT_SDK_DIR}")
  # Validate expected structure
  if(NOT EXISTS "${ACROBAT_SDK_DIR}/PluginSupport/Headers/SDK/PIHeaders.h")
    message(WARNING "PIHeaders.h not found under ${ACROBAT_SDK_DIR}/PluginSupport/Headers/SDK/. Check ACROBAT_SDK_DIR.")
  endif()
endif()

set(PLUGIN_NAME AcrobatImpositionPlugin)

function(aimp_apply_target_options target_name)
  if(AIMP_ENABLE_WARNINGS)
    if(MSVC)
      target_compile_options(${target_name} PRIVATE /W4)
      if(AIMP_WARNINGS_AS_ERRORS)
        target_compile_options(${target_name} PRIVATE /WX)
      endif()
    else()
      target_compile_options(${target_name} PRIVATE -Wall -Wextra -Wpedantic)
      if(AIMP_WARNINGS_AS_ERRORS)
        target_compile_options(${target_name} PRIVATE -Werror)
      endif()
    endif()
  endif()

  if(AIMP_ENABLE_SANITIZERS)
    if(MSVC)
      message(WARNING "AIMP_ENABLE_SANITIZERS is enabled, but sanitizers are not configured for MSVC in this project.")
    else()
      target_compile_options(${target_name} PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
      target_link_options(${target_name} PRIVATE -fsanitize=address,undefined)
    endif()
  endif()
endfunction()

include(GNUInstallDirs)

add_library(aimp_core STATIC
  src/ImpositionPlan.cpp
  src/PanelState.cpp
  src/PdfComposer.cpp
  src/Preset.cpp
  src/TrimShift.cpp
  src/VariableData.cpp
  src/Shuffle.cpp
  src/SplitMerge.cpp
  src/PageTools.cpp
  src/StickOn.cpp
  src/PrinterMarks.cpp
  src/AdjustPages.cpp
  src/TilePages.cpp
  src/Bleed.cpp
  src/PdfX.cpp
  src/Annotations.cpp
  src/ImpositionInfo.cpp
  src/SampleDocument.cpp
)

target_include_directories(aimp_core PUBLIC
  include
)
aimp_apply_target_options(aimp_core)

if(AIMP_BUILD_PLUGIN)
  # SDK layout (Acrobat DC SDK Mac 2023 / Windows 2021):
  #   PluginSupport/Headers/SDK/  — PIHeaders.h, MacPIHeaders.h, WinPIHeaders.h
  #   PluginSupport/Headers/API/  — PIMain.c + all 150+ API headers
  #
  # PIMain.c is compiled directly as C (not wrapped in a .cpp) to avoid
  # "templates must have C++ linkage" errors from ASExtraCalls.h, which
  # defines C++ templates that Clang rejects inside extern "C" blocks.
  #
  # ImposePlugin.cpp is compiled as Objective-C++ on macOS so that SDK
  # headers that reference NSURL (Foundation type) resolve correctly.

  set(PLUGIN_SOURCES
    src/ImposePlugin.cpp
    ${ACROBAT_SDK_DIR}/PluginSupport/Headers/API/PIMain.c
  )

  # MODULE produces MH_BUNDLE (not MH_DYLIB) which is required by Acrobat DC 26.x.
  add_library(${PLUGIN_NAME} MODULE ${PLUGIN_SOURCES})

  target_include_directories(${PLUGIN_NAME} PRIVATE
    include
    ${ACROBAT_SDK_DIR}/PluginSupport/Headers/SDK
    ${ACROBAT_SDK_DIR}/PluginSupport/Headers/API
    ${ACROBAT_SDK_DIR}/PluginSupport/Headers
  )

  target_link_libraries(${PLUGIN_NAME} PRIVATE aimp_core)
  aimp_apply_target_options(${PLUGIN_NAME})

  if(WIN32)
    target_compile_definitions(${PLUGIN_NAME} PRIVATE WIN_PLATFORM _WINDOWS PLUGIN=1)
  elseif(APPLE)
    target_compile_definitions(${PLUGIN_NAME} PRIVATE MAC_PLATFORM PLUGIN=1)
    # Compile ImposePlugin.cpp as Objective-C++ so NSURL (Foundation) resolves.
    # Also force-include the platform prefix so Environ.h's PLATFORM macro is defined.
    set_source_files_properties(src/ImposePlugin.cpp PROPERTIES
      COMPILE_FLAGS "-x objective-c++ -include ${CMAKE_SOURCE_DIR}/src/AcrobatSDKPlatformPrefix.h"
    )
    # Link required macOS frameworks.
    target_link_libraries(${PLUGIN_NAME} PRIVATE
      "-framework CoreFoundation"
      "-framework Foundation"
      "-framework CoreServices"
    )
  endif()

  if(AIMP_NATIVE_PDF_COMPOSER)
    target_compile_definitions(${PLUGIN_NAME} PRIVATE AIMP_NATIVE_PDF_COMPOSER=1)
    # Keep the old macro for code that still uses it.
    target_compile_definitions(${PLUGIN_NAME} PRIVATE AIMP_ENABLE_EXPERIMENTAL_SDK_COMPOSER=1)
  endif()

  # The Acrobat SDK uses (void*)func_ptr — suppress pedantic warnings.
  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(${PLUGIN_NAME} PRIVATE -Wno-pedantic -Wno-deprecated-declarations)
  elseif(MSVC)
    target_compile_options(${PLUGIN_NAME} PRIVATE /wd4152)
  endif()

  # PIMain.c needs the PLATFORM string macro before Environ.h is processed.
  # Use -include to force the prefix header in; COMPILE_DEFINITIONS only sets
  # integer macros and cannot define the string "MacPlatform.h" that Environ.h requires.
  if(APPLE)
    # PIMain.c uses C++ features (throw, _miExceptionInfo) and macOS types (CFBundleRef,
    # ResFileRefNum). Tell CMake this is a CXX file (so it applies C++ standards, not C),
    # then force Objective-C++ mode so macOS framework types resolve without extern "C".
    set_source_files_properties(
      ${ACROBAT_SDK_DIR}/PluginSupport/Headers/API/PIMain.c
      PROPERTIES
      LANGUAGE CXX
      COMPILE_FLAGS "-x objective-c++ -include ${CMAKE_SOURCE_DIR}/src/AcrobatSDKPlatformPrefix.h -Wno-deprecated-declarations -Wno-unused-parameter"
    )
  elseif(WIN32)
    set_source_files_properties(
      ${ACROBAT_SDK_DIR}/PluginSupport/Headers/API/PIMain.c
      PROPERTIES
      COMPILE_FLAGS "-include ${CMAKE_SOURCE_DIR}/src/AcrobatSDKPlatformPrefix.h"
      COMPILE_DEFINITIONS "WIN_PLATFORM=1;_WINDOWS=1;PLUGIN=1"
    )
  endif()

  if(APPLE)
    set_target_properties(${PLUGIN_NAME} PROPERTIES
      OUTPUT_NAME "AcrobatImpositionPlugin"
      PREFIX ""
      SUFFIX ""
    )
    # Assemble the .acroplugin bundle after the binary is built.
    # Acrobat DC 26.x uses Contents/Plugins/<name>.acroplugin/Contents/MacOS/<binary>
    set(ACROPLUGIN_BUNDLE "${CMAKE_BINARY_DIR}/AcrobatImpositionPlugin.acroplugin")
    add_custom_command(TARGET ${PLUGIN_NAME} POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E make_directory "${ACROPLUGIN_BUNDLE}/Contents/MacOS"
      COMMAND ${CMAKE_COMMAND} -E make_directory "${ACROPLUGIN_BUNDLE}/Contents/Resources"
      COMMAND ${CMAKE_COMMAND} -E copy
        "$<TARGET_FILE:${PLUGIN_NAME}>"
        "${ACROPLUGIN_BUNDLE}/Contents/MacOS/AcrobatImpositionPlugin"
      COMMAND ${CMAKE_COMMAND} -E copy
        "${CMAKE_SOURCE_DIR}/src/AcrobatImpositionPlugin-Info.plist"
        "${ACROPLUGIN_BUNDLE}/Contents/Info.plist"
      COMMENT "Assembling AcrobatImpositionPlugin.acroplugin bundle"
    )
  else()
    set_target_properties(${PLUGIN_NAME} PROPERTIES
      OUTPUT_NAME "AcrobatImpositionPlugin"
    )
  endif()
endif()

if(AIMP_BUILD_CLI)
  add_executable(imposr_cli src/CliMain.cpp)
  target_link_libraries(imposr_cli PRIVATE aimp_core)
  aimp_apply_target_options(imposr_cli)
endif()

if(AIMP_BUILD_TESTS)
  enable_testing()
  add_executable(aimp_planner_tests tests/ImpositionPlanTests.cpp)
  target_link_libraries(aimp_planner_tests PRIVATE aimp_core)
  aimp_apply_target_options(aimp_planner_tests)
  add_test(NAME aimp_planner_tests COMMAND aimp_planner_tests)

  add_executable(aimp_new_modules_tests tests/NewModulesTests.cpp)
  target_link_libraries(aimp_new_modules_tests PRIVATE aimp_core)
  aimp_apply_target_options(aimp_new_modules_tests)
  add_test(NAME aimp_new_modules_tests COMMAND aimp_new_modules_tests)
  find_package(Python3 COMPONENTS Interpreter)
  if(Python3_Interpreter_FOUND)
    add_test(
      NAME aimp_host_gate_tooling
      COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/test_host_gate_tooling.py
    )
    add_test(
      NAME aimp_macos_installer_cli
      COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/test_macos_installer_cli.py
    )
  endif()
  if(AIMP_BUILD_CLI)
    add_test(
      NAME imposr_cli_smoke
      COMMAND imposr_cli two-up --pages 4 --sheet-width 1000 --sheet-height 700
    )
    add_test(
      NAME imposr_cli_quality_gate_smoke
      COMMAND imposr_cli two-up --pages 4 --sheet-width 1000 --sheet-height 700 --pdfx-profile pdfx-4 --pdf-trim-marks 1 --pdf-bleed-box 1 --pdf-bleed 6 --fail-on-quality-gate 1 --output-dir ${CMAKE_BINARY_DIR}/qa-smoke --output-stem gate-smoke
    )
    add_test(
      NAME imposr_cli_batch_smoke
      COMMAND imposr_cli batch --batch-csv ${CMAKE_SOURCE_DIR}/tests/data/batch_smoke.csv --output-dir ${CMAKE_BINARY_DIR}/batch-smoke --batch-report-out ${CMAKE_BINARY_DIR}/batch-smoke/report.json --fail-on-quality-gate 1
    )
  endif()
endif()

if(AIMP_ENABLE_PACKAGING)
  install(TARGETS aimp_core
    COMPONENT devel
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  )

  if(AIMP_BUILD_CLI)
    install(TARGETS imposr_cli
      COMPONENT runtime
      RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )
  endif()

  if(AIMP_BUILD_PLUGIN)
    install(TARGETS ${PLUGIN_NAME}
      COMPONENT runtime
      LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
      RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )

    if(APPLE)
      install(PROGRAMS
        scripts/install_acrobat_plugin_macos.sh
        scripts/uninstall_acrobat_plugin_macos.sh
        COMPONENT runtime
        DESTINATION .
      )
    endif()
  endif()

  install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR} COMPONENT devel)
  install(FILES LICENSE README.md DESTINATION ${CMAKE_INSTALL_DOCDIR} COMPONENT docs)
  install(DIRECTORY docs/ DESTINATION ${CMAKE_INSTALL_DOCDIR}/docs COMPONENT docs)

  set(CPACK_PACKAGE_NAME "Imposr")
  set(CPACK_PACKAGE_VENDOR "Imposr Project")
  set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Acrobat imposition planner, CLI, and plugin skeleton")
  set(CPACK_PACKAGE_VERSION ${AIMP_VERSION})
  set(CPACK_PACKAGE_CONTACT "maintainers@imposr.local")
  set(CPACK_PACKAGE_INSTALL_DIRECTORY "Imposr")
  set(CPACK_PACKAGE_FILE_NAME "Imposr-${AIMP_VERSION}-${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}")
  set(CPACK_VERBATIM_VARIABLES YES)
  set(CPACK_ARCHIVE_COMPONENT_INSTALL ON)
  set(CPACK_COMPONENTS_ALL runtime devel docs)
  set(CPACK_COMPONENT_RUNTIME_DISPLAY_NAME "Imposr Runtime")
  set(CPACK_COMPONENT_DEVEL_DISPLAY_NAME "Imposr Development Files")
  set(CPACK_COMPONENT_DOCS_DISPLAY_NAME "Imposr Documentation")
  set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE")

  if(WIN32)
    find_program(AIMP_MAKENSIS_EXECUTABLE makensis)
    if(AIMP_MAKENSIS_EXECUTABLE)
      set(CPACK_GENERATOR "ZIP;NSIS")
      set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
      set(CPACK_NSIS_MODIFY_PATH OFF)
      if(AIMP_BUILD_PLUGIN AND AIMP_DEPLOY_PLUGIN_TO_ACROBAT_ON_INSTALL AND AIMP_ACROBAT_PLUGIN_INSTALL_DIR)
        file(TO_NATIVE_PATH "${AIMP_ACROBAT_PLUGIN_INSTALL_DIR}" AIMP_ACROBAT_PLUGIN_INSTALL_DIR_NATIVE)
        set(CPACK_NSIS_EXTRA_INSTALL_COMMANDS
          "CreateDirectory \\\"${AIMP_ACROBAT_PLUGIN_INSTALL_DIR_NATIVE}\\\"\\n"
          "IfFileExists \\\"$INSTDIR\\\\${CMAKE_INSTALL_BINDIR}\\\\AcrobatImpositionPlugin.dll\\\" 0 +2\\n"
          "CopyFiles /SILENT \\\"$INSTDIR\\\\${CMAKE_INSTALL_BINDIR}\\\\AcrobatImpositionPlugin.dll\\\" \\\"${AIMP_ACROBAT_PLUGIN_INSTALL_DIR_NATIVE}\\\\AcrobatImpositionPlugin.dll\\\"\\n"
          "WriteINIStr \\\"$INSTDIR\\\\plugin-deploy.ini\\\" \\\"acrobat\\\" \\\"plugin_path\\\" \\\"${AIMP_ACROBAT_PLUGIN_INSTALL_DIR_NATIVE}\\\\AcrobatImpositionPlugin.dll\\\"\\n"
        )
        set(CPACK_NSIS_EXTRA_UNINSTALL_COMMANDS
          "Delete \\\"${AIMP_ACROBAT_PLUGIN_INSTALL_DIR_NATIVE}\\\\AcrobatImpositionPlugin.dll\\\"\\n"
          "Delete \\\"$INSTDIR\\\\plugin-deploy.ini\\\"\\n"
        )
      endif()
    else()
      set(CPACK_GENERATOR "ZIP")
    endif()
  elseif(APPLE)
    set(CPACK_GENERATOR "DragNDrop;TGZ")
  else()
    set(CPACK_GENERATOR "TGZ")
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Imposr Maintainers")
    set(CPACK_DEBIAN_PACKAGE_SECTION "utils")
    set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
    find_program(AIMP_FILE_EXECUTABLE file)
    if(AIMP_FILE_EXECUTABLE)
      set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
    else()
      set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS OFF)
    endif()
    find_program(AIMP_DPKG_EXECUTABLE dpkg)
    if(AIMP_DPKG_EXECUTABLE)
      set(CPACK_GENERATOR "TGZ;DEB")
    endif()
  endif()

  include(CPack)
endif()
