# core/CMakeLists.txt — builds the path-tracking core as a shared library.
cmake_minimum_required(VERSION 3.16)
project(track_core LANGUAGES CXX VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(CMAKE_AUTOMOC OFF)
set(CMAKE_AUTOUIC OFF)
set(CMAKE_AUTORCC OFF)

# Put the DLL/SO next to the executables so frontends in the same build
# tree can find it without RPATH gymnastics. Essential on Windows.
if(NOT CMAKE_RUNTIME_OUTPUT_DIRECTORY)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
endif()
if(NOT CMAKE_LIBRARY_OUTPUT_DIRECTORY)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
endif()
if(NOT CMAKE_ARCHIVE_OUTPUT_DIRECTORY)
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
endif()

add_library(track_core SHARED
    src/track_core.cpp
)

target_include_directories(track_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_compile_definitions(track_core PRIVATE TRACK_CORE_BUILD)

if(MSVC)
    target_compile_options(track_core PRIVATE /W4 /permissive- /O2)
    target_compile_definitions(track_core PUBLIC NOMINMAX)
else()
    target_compile_options(track_core PRIVATE
        -Wall -Wextra -Wpedantic
        $<$<CONFIG:Release>:-O3>
    )
endif()

set_target_properties(track_core PROPERTIES
    OUTPUT_NAME track_core
    VERSION ${PROJECT_VERSION}
    SOVERSION 1
)

# Smoke / regression test — registered with CTest so `ctest` gates the
# Python-reference metrics (path/heading errors, moment) on every build.
option(TRACK_BUILD_TESTS "Build the smoke/regression test and register CTest" ON)
if(TRACK_BUILD_TESTS)
    enable_testing()
    add_executable(track_core_smoke tools/smoke_test.cpp)
    target_link_libraries(track_core_smoke PRIVATE track_core)
    add_test(NAME smoke COMMAND track_core_smoke)
    if(WIN32)
        set_tests_properties(smoke PROPERTIES
            ENVIRONMENT "PATH=$<TARGET_FILE_DIR:track_core>;$ENV{PATH}")
    endif()
endif()

install(TARGETS track_core
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib)
install(FILES include/track_core.h DESTINATION include)
