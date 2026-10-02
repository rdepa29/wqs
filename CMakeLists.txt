cmake_minimum_required(VERSION 3.21)

project(wqs VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Quick Qml Network ShaderToolsTools)

qt_standard_project_setup(REQUIRES 6.5)

# the Quickshell QML module
qt_add_library(quickshell STATIC)
qt_add_qml_module(quickshell
    URI Quickshell
    VERSION 1.0
    OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/qml_modules/Quickshell
    SOURCES
        src/quickshell/doc.hpp
        src/quickshell/Types.h
        src/quickshell/AppBar.h
        src/quickshell/AppBar.cpp
        src/quickshell/Screen.h
        src/quickshell/Screen.cpp
        src/quickshell/Quickshell.h
        src/quickshell/Quickshell.cpp
        src/quickshell/incubator.h
        src/quickshell/incubator.cpp
        src/quickshell/reload.h
        src/quickshell/reload.cpp
        src/quickshell/generation.h
        src/quickshell/generation.cpp
        src/quickshell/qsintercept.h
        src/quickshell/qsintercept.cpp
        src/quickshell/scan.h
        src/quickshell/scan.cpp
        src/quickshell/singleton.h
        src/quickshell/singleton.cpp
        src/quickshell/ScriptModel.h
        src/quickshell/ScriptModel.cpp
        src/quickshell/Region.h
        src/quickshell/Region.cpp
        src/quickshell/TransformWatcher.h
        src/quickshell/TransformWatcher.cpp
        src/quickshell/variants.h
        src/quickshell/variants.cpp
        src/quickshell/persistentprops.h
        src/quickshell/persistentprops.cpp
        src/quickshell/lazyloader.h
        src/quickshell/lazyloader.cpp
        src/quickshell/ShellRoot.h
        src/quickshell/ShellRoot.cpp
        src/quickshell/Window.h
        src/quickshell/Window.cpp
)
target_include_directories(quickshell PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/quickshell)
target_link_libraries(quickshell PUBLIC Qt6::Core Qt6::Gui Qt6::Quick Qt6::Qml)

# SHAppBarMessage (app-bar registration) lives in shell32.
if(WIN32)
    target_link_libraries(quickshell PUBLIC shell32)
endif()

# === The Quickshell.Io QML module ==========================================
qt_add_library(quickshell_io STATIC)
qt_add_qml_module(quickshell_io
    URI Quickshell.Io
    VERSION 1.0
    OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/qml_modules/Quickshell/Io
    SOURCES
        src/quickshell/io/DataStreamParser.h
        src/quickshell/io/DataStreamParser.cpp
        src/quickshell/io/ProcessObject.h
        src/quickshell/io/ProcessObject.cpp
        src/quickshell/io/IpcHandler.h
        src/quickshell/io/IpcHandler.cpp
)
target_include_directories(quickshell_io PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/quickshell/io)
target_link_libraries(quickshell_io PUBLIC Qt6::Core Qt6::Gui Qt6::Quick Qt6::Qml Qt6::Network)

# === The Quickshell.Komorebi QML module =====================================
qt_add_library(quickshell_komorebi STATIC)
qt_add_qml_module(quickshell_komorebi
    URI Quickshell.Komorebi
    VERSION 1.0
    OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/qml_modules/Quickshell/Komorebi
    SOURCES
        src/quickshell/komorebi/KomorebiTypes.h
        src/quickshell/komorebi/Komorebi.h
        src/quickshell/komorebi/Komorebi.cpp
)
target_include_directories(quickshell_komorebi PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/quickshell/komorebi)
target_link_libraries(quickshell_komorebi PUBLIC Qt6::Core Qt6::Gui Qt6::Quick Qt6::Qml Qt6::Network)

# === The Quickshell.Widgets QML module =======================================
qt_policy(SET QTP0004 NEW)
qt_add_library(quickshell_widgets STATIC)
qt_add_qml_module(quickshell_widgets
    URI Quickshell.Widgets
    VERSION 1.0
    OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/qml_modules/Quickshell/Widgets
    SOURCES
        src/widgets/cliprect.hpp
        src/widgets/cliprect.cpp
        src/widgets/wrapper.hpp
        src/widgets/wrapper.cpp
        src/widgets/marginwrapper.hpp
        src/widgets/marginwrapper.cpp
    QML_FILES
        src/widgets/IconImage.qml
        src/widgets/ClippingRectangle.qml
        src/widgets/ClippingWrapperRectangle.qml
        src/widgets/ClippingWrapperRectangleInternal.qml
        src/widgets/WrapperItem.qml
        src/widgets/WrapperMouseArea.qml
        src/widgets/WrapperRectangle.qml
)
target_include_directories(quickshell_widgets PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/widgets)
target_link_libraries(quickshell_widgets PUBLIC Qt6::Core Qt6::Gui Qt6::Quick Qt6::Qml)

# clipping shaders; the "-ub" variant lets content render underneath the border
qt6_add_shaders(quickshell_widgets "widgets-cliprect"
    NOHLSL NOMSL BATCHABLE PRECOMPILE OPTIMIZED QUIET
    PREFIX "/Quickshell/Widgets"
    FILES src/widgets/shaders/cliprect.frag
    OUTPUTS shaders/cliprect.frag.qsb
)

qt6_add_shaders(quickshell_widgets "widgets-cliprect-ub"
    NOHLSL NOMSL BATCHABLE PRECOMPILE OPTIMIZED QUIET
    PREFIX "/Quickshell/Widgets"
    FILES src/widgets/shaders/cliprect.frag
    OUTPUTS shaders/cliprect-ub.frag.qsb
    DEFINES CONTENT_UNDER_BORDER
)

# console subsystem so shells wait and see exit codes; the shell relaunches detached
qt_add_executable(wqs
    src/main.cpp
    src/Shell.cpp
    src/Shell.h
    src/Cli.cpp
    src/Cli.h
    src/Instance.cpp
    src/Instance.h
    src/IpcServer.cpp
    src/IpcServer.h
)

# bundled fallback config (URI must match main.cpp)
qt_add_qml_module(wqs
    URI wqs
    VERSION 1.0
    QML_FILES
        qml/shell.qml
)

target_link_libraries(wqs PRIVATE
    Qt6::Core Qt6::Gui Qt6::Quick Qt6::Qml Qt6::Network
    quickshell quickshell_io quickshell_komorebi quickshell_widgets
    quickshellplugin quickshell_ioplugin quickshell_komorebiplugin quickshell_widgetsplugin
)

# pull in the plugin init objects the linker would otherwise drop
target_sources(wqs PRIVATE
    $<TARGET_OBJECTS:quickshellplugin_init>
    $<TARGET_OBJECTS:quickshell_ioplugin_init>
    $<TARGET_OBJECTS:quickshell_komorebiplugin_init>
    $<TARGET_OBJECTS:quickshell_widgetsplugin_init>
)

target_compile_definitions(wqs PRIVATE
    QT_DISABLE_DEPRECATED_BEFORE=0x060000
)
