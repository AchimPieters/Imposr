# CMake generated Testfile for 
# Source directory: /mnt/data/imposr/Imposr
# Build directory: /mnt/data/imposr/Imposr/build
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test([=[aimp_planner_tests]=] "/mnt/data/imposr/Imposr/build/aimp_planner_tests")
set_tests_properties([=[aimp_planner_tests]=] PROPERTIES  _BACKTRACE_TRIPLES "/mnt/data/imposr/Imposr/CMakeLists.txt;83;add_test;/mnt/data/imposr/Imposr/CMakeLists.txt;0;")
add_test([=[imposr_cli_smoke]=] "/mnt/data/imposr/Imposr/build/imposr_cli" "two-up" "--pages" "4" "--sheet-width" "1000" "--sheet-height" "700")
set_tests_properties([=[imposr_cli_smoke]=] PROPERTIES  _BACKTRACE_TRIPLES "/mnt/data/imposr/Imposr/CMakeLists.txt;85;add_test;/mnt/data/imposr/Imposr/CMakeLists.txt;0;")
