# core/CMakeLists.txt — builds the 2-DOF comparison core as a shared library.
cmake_minimum_required(VERSION 3.16)
project(tdof_core LANGUAGES CXX VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(CMAKE_AUTOMOC OFF)
set(CMAKE_AUTOUIC OFF)
set(CMAKE_AUTORCC OFF)

find_package(Eigen3 3.3 NO_MODULE QUIET)
if(NOT Eigen3_FOUND)
    find_path(EIGEN3_INCLUDE_DIR
        NAMES Eigen/Dense
        PATHS /usr/include/eigen3 /usr/local/include/eigen3
              /opt/homebrew/include/eigen3
        NO_DEFAULT_PATH
    )
    if(NOT EIGEN3_INCLUDE_DIR)
        message(FATAL_ERROR
            "Eigen3 not found. Install libeigen3-dev or the eigen package.")
    endif()
endif()

add_library(tdof_core SHARED
    src/tf.cpp
    src/simulate.cpp
    src/tdof_core.cpp
)

target_include_directories(tdof_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

if(Eigen3_FOUND)
    target_link_libraries(tdof_core PRIVATE Eigen3::Eigen)
else()
    target_include_directories(tdof_core SYSTEM PRIVATE ${EIGEN3_INCLUDE_DIR})
endif()

target_compile_definitions(tdof_core PRIVATE TDOF_CORE_BUILD)

if(MSVC)
    target_compile_options(tdof_core PRIVATE /W4 /permissive- /O2)
else()
    target_compile_options(tdof_core PRIVATE
        -Wall -Wextra -Wpedantic
        $<$<CONFIG:Release>:-O3>
    )
endif()

set_target_properties(tdof_core PROPERTIES
    OUTPUT_NAME tdof_core
    VERSION ${PROJECT_VERSION}
    SOVERSION 1
)

# Regression test, registered with CTest. Guarded to the top-level project so
# frontend builds (which pull core in via add_subdirectory) don't drag it in.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    enable_testing()
    add_executable(tdof_core_smoke tools/smoke_test.cpp)
    target_link_libraries(tdof_core_smoke PRIVATE tdof_core)
    add_test(NAME tdof_core_smoke COMMAND tdof_core_smoke)
endif()

install(TARGETS tdof_core
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib)
install(FILES include/tdof_core.h DESTINATION include)
