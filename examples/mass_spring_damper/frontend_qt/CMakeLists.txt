# frontend_qt/CMakeLists.txt — Windows-friendly Qt6 build.
cmake_minimum_required(VERSION 3.16)
project(msd_qt LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

if(NOT CMAKE_RUNTIME_OUTPUT_DIRECTORY)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
endif()
if(NOT CMAKE_LIBRARY_OUTPUT_DIRECTORY)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
endif()

if(NOT TARGET msd_core)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../core
                     ${CMAKE_CURRENT_BINARY_DIR}/core_build)
endif()

find_package(Qt6 6.2 COMPONENTS Core Gui Widgets REQUIRED)

add_executable(msd_qt
    main.cpp
    MainWindow.cpp
    MainWindow.hpp
    Widgets.cpp
    Widgets.hpp
)

target_link_libraries(msd_qt PRIVATE
    msd_core
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
)

if(UNIX AND NOT APPLE)
    set_target_properties(msd_qt PROPERTIES
        BUILD_RPATH "\$ORIGIN;\$ORIGIN/../core"
        INSTALL_RPATH "\$ORIGIN"
    )
endif()

if(WIN32)
    find_program(WINDEPLOYQT_EXECUTABLE windeployqt
                 HINTS "${_qt_bin_dir}" "${Qt6_DIR}/../../../bin")
    if(WINDEPLOYQT_EXECUTABLE)
        add_custom_command(TARGET msd_qt POST_BUILD
            COMMAND "${WINDEPLOYQT_EXECUTABLE}"
                --no-translations --no-system-d3d-compiler
                --no-opengl-sw --no-compiler-runtime
                "$<TARGET_FILE:msd_qt>"
            COMMENT "Running windeployqt")
    endif()
endif()
