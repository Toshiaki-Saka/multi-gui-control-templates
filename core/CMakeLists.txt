# core/CMakeLists.txt — builds the MSD core as a shared library.
cmake_minimum_required(VERSION 3.16)
project(msd_core LANGUAGES CXX VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(CMAKE_AUTOMOC OFF)
set(CMAKE_AUTOUIC OFF)
set(CMAKE_AUTORCC OFF)

# Put DLL/SO next to executables so frontends in the same build tree can
# find it without RPATH gymnastics. Essential on Windows.
if(NOT CMAKE_RUNTIME_OUTPUT_DIRECTORY)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
endif()
if(NOT CMAKE_LIBRARY_OUTPUT_DIRECTORY)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
endif()
if(NOT CMAKE_ARCHIVE_OUTPUT_DIRECTORY)
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
endif()

add_library(msd_core SHARED
    src/msd_core.cpp
)

target_include_directories(msd_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_compile_definitions(msd_core PRIVATE MSD_CORE_BUILD)

if(MSVC)
    target_compile_options(msd_core PRIVATE /W4 /permissive- /O2)
    target_compile_definitions(msd_core PUBLIC NOMINMAX)
else()
    target_compile_options(msd_core PRIVATE
        -Wall -Wextra -Wpedantic
        $<$<CONFIG:Release>:-O3>
    )
endif()

set_target_properties(msd_core PROPERTIES
    OUTPUT_NAME msd_core
    VERSION ${PROJECT_VERSION}
    SOVERSION 1
)

# Numerical regression test. Registered with CTest so `ctest` gates on it.
# Only built/registered when core is the top-level project, so that frontend
# builds (which pull core in via add_subdirectory) don't drag in the test.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    enable_testing()
    add_executable(msd_core_smoke tools/smoke_test.cpp)
    target_link_libraries(msd_core_smoke PRIVATE msd_core)
    add_test(NAME msd_core_smoke COMMAND msd_core_smoke)
endif()

install(TARGETS msd_core
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib)
install(FILES include/msd_core.h DESTINATION include)
