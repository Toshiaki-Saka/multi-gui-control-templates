# frontend_qt/CMakeLists.txt — Windows-friendly Qt6 build.
cmake_minimum_required(VERSION 3.16)
project(track_qt LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Put outputs next to one another so the loader can find track_core
# (essential on Windows, where the DLL must sit beside the .exe).
if(NOT CMAKE_RUNTIME_OUTPUT_DIRECTORY)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
endif()
if(NOT CMAKE_LIBRARY_OUTPUT_DIRECTORY)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
endif()

if(NOT TARGET track_core)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../core
                     ${CMAKE_CURRENT_BINARY_DIR}/core_build)
endif()

find_package(Qt6 6.2 COMPONENTS Core Gui Widgets REQUIRED)

add_executable(track_qt
    main.cpp
    MainWindow.cpp
    MainWindow.hpp
    Widgets.cpp
    Widgets.hpp
)

target_link_libraries(track_qt PRIVATE
    track_core
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
)

if(UNIX AND NOT APPLE)
    set_target_properties(track_qt PROPERTIES
        BUILD_RPATH "\$ORIGIN;\$ORIGIN/../core"
        INSTALL_RPATH "\$ORIGIN"
    )
endif()

# On Windows, deploy the Qt DLLs next to the exe after build.
if(WIN32)
    find_program(WINDEPLOYQT_EXECUTABLE windeployqt
                 HINTS "${_qt_bin_dir}" "${Qt6_DIR}/../../../bin")
    if(WINDEPLOYQT_EXECUTABLE)
        add_custom_command(TARGET track_qt POST_BUILD
            COMMAND "${WINDEPLOYQT_EXECUTABLE}"
                --no-translations --no-system-d3d-compiler
                --no-opengl-sw --no-compiler-runtime
                "$<TARGET_FILE:track_qt>"
            COMMENT "Running windeployqt")
    endif()
endif()
