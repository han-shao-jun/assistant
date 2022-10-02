cmake_minimum_required(VERSION 3.22)
project(assistant)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

set(CMAKE_PREFIX_PATH "D:\\Programs\\QT5\\5.12.9\\mingw73_64")

find_package(Qt5 COMPONENTS
        Core
        Gui
        Widgets
        Network
        SerialPort
        Charts
        PrintSupport
        Bluetooth
        REQUIRED)

include_directories(${PROJECT_NAME}/)

FILE(GLOB_RECURSE SRC
        "${PROJECT_NAME}/*.c*"
        "${PROJECT_NAME}/*.c*")
FILE(GLOB_RECURSE INC
        "${PROJECT_NAME}/*.h*"
        "${PROJECT_NAME}/*.h*")
FILE(GLOB_RECURSE UI
        "${PROJECT_NAME}/*.ui")

add_executable(${PROJECT_NAME} ${SRC} ${INC} ${UI})
target_link_libraries(${PROJECT_NAME}
        Qt5::Core
        Qt5::Gui
        Qt5::Widgets
        Qt5::Network
        Qt5::SerialPort
        Qt5::Charts
        Qt5::PrintSupport
        Qt5::Bluetooth
)

if (WIN32)
    set(DEBUG_SUFFIX)
    if (CMAKE_BUILD_TYPE MATCHES "Debug")
        set(DEBUG_SUFFIX "d")
    endif ()
    set(QT_INSTALL_PATH "${CMAKE_PREFIX_PATH}")
    if (NOT EXISTS "${QT_INSTALL_PATH}/bin")
        set(QT_INSTALL_PATH "${QT_INSTALL_PATH}/..")
        if (NOT EXISTS "${QT_INSTALL_PATH}/bin")
            set(QT_INSTALL_PATH "${QT_INSTALL_PATH}/..")
        endif ()
    endif ()
    if (EXISTS "${QT_INSTALL_PATH}/plugins/platforms/qwindows${DEBUG_SUFFIX}.dll")
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E make_directory
                "$<TARGET_FILE_DIR:${PROJECT_NAME}>/plugins/platforms/")
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy
                "${QT_INSTALL_PATH}/plugins/platforms/qwindows${DEBUG_SUFFIX}.dll"
                "$<TARGET_FILE_DIR:${PROJECT_NAME}>/plugins/platforms/")
    endif ()
    foreach (QT_LIB Core Gui Widgets Network SerialPort Charts PrintSupport Bluetooth)
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy
                "${QT_INSTALL_PATH}/bin/Qt5${QT_LIB}${DEBUG_SUFFIX}.dll"
                "$<TARGET_FILE_DIR:${PROJECT_NAME}>")
    endforeach (QT_LIB)
endif ()
