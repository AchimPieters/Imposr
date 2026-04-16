cmake_minimum_required(VERSION 3.21)
project(AcrobatImpositionPlugin LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)

if(NOT DEFINED ACROBAT_SDK_DIR)
  if(DEFINED ENV{ACROBAT_SDK_DIR})
    set(ACROBAT_SDK_DIR "$ENV{ACROBAT_SDK_DIR}")
  endif()
endif()

if(NOT ACROBAT_SDK_DIR)
  message(FATAL_ERROR "Set ACROBAT_SDK_DIR to your Acrobat SDK root.")
endif()

message(STATUS "Using Acrobat SDK at: ${ACROBAT_SDK_DIR}")

set(PLUGIN_NAME AcrobatImpositionPlugin)

add_library(${PLUGIN_NAME} SHARED
  src/ImposePlugin.cpp
  src/ImpositionPlan.cpp
  src/PIMainShim.cpp
)

target_include_directories(${PLUGIN_NAME} PRIVATE
  include
  ${ACROBAT_SDK_DIR}/API
  ${ACROBAT_SDK_DIR}/API/Headers
  ${ACROBAT_SDK_DIR}/PluginSupport
  ${ACROBAT_SDK_DIR}/PluginSupport/Headers
)

if(WIN32)
  target_compile_definitions(${PLUGIN_NAME} PRIVATE WIN_PLATFORM _WINDOWS)
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
