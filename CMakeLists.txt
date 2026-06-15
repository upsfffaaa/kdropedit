cmake_minimum_required(VERSION 3.16)
project(kdropedit LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Gui LinguistTools)

find_package(ECM REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

find_package(KF6GlobalAccel REQUIRED)

add_executable(kdropedit
    main.cpp
    kdropedit.cpp
    kdropedit.h
    settings.h
    settings.cpp
    resources.qrc
)

target_link_libraries(kdropedit
    PRIVATE
        Qt6::Widgets
        Qt6::Gui
        KF6::GlobalAccel
)

set(TS_FILES
    translations/app_en.ts
    translations/app_ru.ts
)

qt_add_translations(kdropedit
    TS_FILES ${TS_FILES}
    QM_FILES_OUTPUT_VARIABLE QM_FILES
)
add_custom_target(translations ALL DEPENDS ${QM_FILES})