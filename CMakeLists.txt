cmake_minimum_required(VERSION 3.22)
project(assistant
        VERSION 0.0.0.1
        DESCRIPTION "is a embedded developer assistant"
        LANGUAGES C CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_PREFIX_PATH "C:\\Programs\\Qt\\6.7.3\\msvc2022_64_static")

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets)

find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS
        Core
        Gui
        Widgets
        Network
        SerialPort
        PrintSupport
        Core5Compat
        Bluetooth
)

include_directories(${PROJECT_NAME}/)
include_directories(${PROJECT_NAME}/qcustomplot)

add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")

if(MSVC)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /manifest:no")
    set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} /manifest:no")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /manifest:no")
endif()


macro(header_directories parent)
    file(GLOB_RECURSE newList ${parent}/*.h)
    set(dir_list "")
    foreach (file_path ${newList})
        get_filename_component(dir_path ${file_path} DIRECTORY)
        set(dir_list ${dir_list} ${dir_path})
    endforeach ()
    list(REMOVE_DUPLICATES dir_list)

    include_directories(${dir_list})
endmacro()

header_directories(assistant)

file(GLOB_RECURSE SRC "${PROJECT_NAME}/*.c*")
file(GLOB_RECURSE INC "${PROJECT_NAME}/*.h*")
file(GLOB_RECURSE UI "${PROJECT_NAME}/*.ui")
file(GLOB_RECURSE QRC "${PROJECT_NAME}/*.qrc")
file(GLOB_RECURSE RC "${PROJECT_NAME}/*.rc")


if(CMAKE_BUILD_TYPE MATCHES "Release")
    #发布版除去黑窗口，无qDebu打印
    add_compile_definitions(QT_NO_DEBUG_OUTPUT)
    add_executable(${PROJECT_NAME} WIN32 ${SRC} ${INC} ${UI} ${QRC} ${RC})
else ()
    #调试版保留黑窗口,有qDebug打印
    add_executable(${PROJECT_NAME} ${SRC} ${INC} ${UI} ${QRC} ${RC})
endif()

target_link_libraries(${PROJECT_NAME} PUBLIC
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Gui
        Qt${QT_VERSION_MAJOR}::Widgets
        Qt${QT_VERSION_MAJOR}::Network
        Qt${QT_VERSION_MAJOR}::SerialPort
        Qt${QT_VERSION_MAJOR}::PrintSupport
        Qt${QT_VERSION_MAJOR}::Core5Compat
        Qt${QT_VERSION_MAJOR}::Bluetooth
        )

# Windows下需要的系统库 (DWM API，用于自定义标题栏)
if (WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE dwmapi)
endif()

# 自qt6.3起支持使用cmake生成安装动态库规则，生成安装规则
qt_generate_deploy_app_script(
        TARGET ${PROJECT_NAME}
        OUTPUT_SCRIPT deploy_script
        NO_UNSUPPORTED_PLATFORM_ERROR
)

# 添加安装命令
install(SCRIPT ${deploy_script})

# 添加安装命令
install(TARGETS ${PROJECT_NAME}
        DESTINATION bin
)
