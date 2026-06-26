# core/CMakeLists.txt — builds the PID demo core as a shared library.
cmake_minimum_required(VERSION 3.16)
project(pid_core LANGUAGES CXX VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(CMAKE_AUTOMOC OFF)
set(CMAKE_AUTOUIC OFF)
set(CMAKE_AUTORCC OFF)

add_library(pid_core SHARED
    src/pid_core.cpp
)

target_include_directories(pid_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_compile_definitions(pid_core PRIVATE PID_CORE_BUILD)

if(MSVC)
    target_compile_options(pid_core PRIVATE /W4 /permissive- /O2)
else()
    target_compile_options(pid_core PRIVATE
        -Wall -Wextra -Wpedantic
        $<$<CONFIG:Release>:-O3>
    )
endif()

set_target_properties(pid_core PROPERTIES
    OUTPUT_NAME pid_core
    VERSION ${PROJECT_VERSION}
    SOVERSION 1
)

# Regression test, registered with CTest so `ctest` gates on it. Guarded to
# the top-level project so frontend builds (which pull core in via
# add_subdirectory) don't drag in the test target.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    enable_testing()
    add_executable(pid_core_smoke tools/smoke_test.cpp)
    target_link_libraries(pid_core_smoke PRIVATE pid_core)
    add_test(NAME pid_core_smoke COMMAND pid_core_smoke)
endif()

install(TARGETS pid_core
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib)
install(FILES include/pid_core.h DESTINATION include)
