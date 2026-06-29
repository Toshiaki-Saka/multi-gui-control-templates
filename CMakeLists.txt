# multi-gui-control-templates — umbrella build.
#
# Builds the four control-example cores (each a self-contained shared library
# imported with its history under examples/<name>/core) from a single root,
# collecting every artifact into one output directory so the shared GUI
# launcher and the cross-example test can find them all.
cmake_minimum_required(VERSION 3.16)
project(multi_gui_control_templates LANGUAGES CXX)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

enable_testing()

# ── Example cores (★ the only per-repo parts; the shell is shared) ────────────
add_subdirectory(examples/pid/core)                 # PID 1-DOF
add_subdirectory(examples/pi_path_tracking/core)    # PI path tracking
add_subdirectory(examples/two_dof/core)             # 2-DOF vs PID
add_subdirectory(examples/mass_spring_damper/core)  # mass-spring-damper

# ── Cross-example integration test (runs all four through the shared GUI) ─────
find_package(Python3 COMPONENTS Interpreter)
if(Python3_Interpreter_FOUND)
    add_test(NAME cross_example_python
             COMMAND ${Python3_EXECUTABLE}
                     ${CMAKE_SOURCE_DIR}/tests/test_examples.py)
    if(WIN32)
        set_tests_properties(cross_example_python PROPERTIES
            ENVIRONMENT "PATH=${CMAKE_BINARY_DIR}/lib;$ENV{PATH}")
    endif()
endif()

message(STATUS "multi-gui-control-templates: 4 control cores configured "
               "(PID / PI-tracking / 2-DOF / mass-spring-damper)")
