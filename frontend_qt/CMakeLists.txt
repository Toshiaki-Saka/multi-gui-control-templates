# frontend_qt/CMakeLists.txt
cmake_minimum_required(VERSION 3.16)
project(tdof_qt LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

if(NOT TARGET tdof_core)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../core
                     ${CMAKE_CURRENT_BINARY_DIR}/core_build)
endif()

find_package(Qt6 6.2 COMPONENTS Core Gui Widgets REQUIRED)

add_executable(tdof_qt
    main.cpp
    MainWindow.cpp
    MainWindow.hpp
    Widgets.cpp
    Widgets.hpp
)

target_link_libraries(tdof_qt PRIVATE
    tdof_core
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
)

if(UNIX AND NOT APPLE)
    set_target_properties(tdof_qt PROPERTIES
        BUILD_RPATH "\$ORIGIN;\$ORIGIN/../core"
        INSTALL_RPATH "\$ORIGIN"
    )
endif()
