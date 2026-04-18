cmake_minimum_required(VERSION 3.21)
project(AcrobatImpositionPlugin LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)

option(AIMP_BUILD_PLUGIN "Build the Acrobat plug-in target." ON)
option(AIMP_BUILD_TESTS "Build planner tests." ON)
option(AIMP_BUILD_CLI "Build the CLI app." ON)
option(AIMP_ENABLE_EXPERIMENTAL_SDK_COMPOSER "Enable experimental native SDK composer adapter (plugin only)." OFF)

if(NOT DEFINED ACROBAT_SDK_DIR)
  if(DEFINED ENV{ACROBAT_SDK_DIR})
    set(ACROBAT_SDK_DIR "$ENV{ACROBAT_SDK_DIR}")
  endif()
endif()

if(AIMP_BUILD_PLUGIN AND NOT ACROBAT_SDK_DIR)
  message(WARNING "ACROBAT_SDK_DIR is not set. Disabling plug-in build.")
  set(AIMP_BUILD_PLUGIN OFF)
endif()

if(ACROBAT_SDK_DIR)
  message(STATUS "Using Acrobat SDK at: ${ACROBAT_SDK_DIR}")
endif()

set(PLUGIN_NAME AcrobatImpositionPlugin)

add_library(aimp_core STATIC
  src/ImpositionPlan.cpp
  src/PdfComposer.cpp
  src/Preset.cpp
)

target_include_directories(aimp_core PUBLIC
  include
)

if(AIMP_BUILD_PLUGIN)
  add_library(${PLUGIN_NAME} SHARED
    src/ImposePlugin.cpp
    src/PIMainShim.cpp
  )

  target_include_directories(${PLUGIN_NAME} PRIVATE
    include
    ${ACROBAT_SDK_DIR}/API
    ${ACROBAT_SDK_DIR}/API/Headers
    ${ACROBAT_SDK_DIR}/PluginSupport
    ${ACROBAT_SDK_DIR}/PluginSupport/Headers
  )

  target_link_libraries(${PLUGIN_NAME} PRIVATE aimp_core)

  if(WIN32)
    target_compile_definitions(${PLUGIN_NAME} PRIVATE WIN_PLATFORM _WINDOWS)
  elseif(APPLE)
    target_compile_definitions(${PLUGIN_NAME} PRIVATE MAC_PLATFORM)
  endif()
  if(AIMP_ENABLE_EXPERIMENTAL_SDK_COMPOSER)
    target_compile_definitions(${PLUGIN_NAME} PRIVATE AIMP_ENABLE_EXPERIMENTAL_SDK_COMPOSER=1)
  endif()

  # Adobe's SDK requires PIMain.c from the SDK support files.
  # This shim includes it via the SDK path so the repo remains lightweight.

  # NOTE:
  # Actual link libraries vary by SDK version and platform.
  # Add required Acrobat SDK import libraries here after matching your SDK layout.
  # target_link_libraries(${PLUGIN_NAME} PRIVATE ...)

  set_target_properties(${PLUGIN_NAME} PROPERTIES
    OUTPUT_NAME "AcrobatImpositionPlugin"
  )
endif()

if(AIMP_BUILD_CLI)
  add_executable(imposr_cli src/CliMain.cpp)
  target_link_libraries(imposr_cli PRIVATE aimp_core)
endif()

if(AIMP_BUILD_TESTS)
  enable_testing()
  add_executable(aimp_planner_tests tests/ImpositionPlanTests.cpp)
  target_link_libraries(aimp_planner_tests PRIVATE aimp_core)
  add_test(NAME aimp_planner_tests COMMAND aimp_planner_tests)
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
