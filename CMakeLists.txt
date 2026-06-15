cmake_minimum_required(VERSION 3.16)
project(kdropedit LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Gui)

find_package(ECM REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

find_package(KF6GlobalAccel REQUIRED)

add_executable(kdropedit
    main.cpp
    kdropedit.cpp
    kdropedit.h
)

target_link_libraries(kdropedit
    PRIVATE
        Qt6::Widgets
        Qt6::Gui
        KF6::GlobalAccel
)