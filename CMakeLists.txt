cmake_minimum_required(VERSION 3.21)

project(wqs VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Quick Qml Network)

qt_standard_project_setup(REQUIRES 6.5)

# === The Quickshell QML module =============================================
# A static library so the module owns its own qmltypes/qmldir. The URI and type names
# match upstream Quickshell, so an existing config loads unchanged.
qt_add_library(quickshell STATIC)
qt_add_qml_module(quickshell
    URI Quickshell
    VERSION 1.0
    OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/qml_modules/Quickshell
    SOURCES
        src/quickshell/Types.h
        src/quickshell/AppBar.h
        src/quickshell/AppBar.cpp
        src/quickshell/Screen.h
        src/quickshell/Screen.cpp
        src/quickshell/Quickshell.h
        src/quickshell/Quickshell.cpp
        src/quickshell/Scope.h
        src/quickshell/Scope.cpp
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

# === The executable ========================================================
# Console subsystem (no WIN32), so cmd/PowerShell/fish wait for the process,
# capture its output, and see its exit code. When it starts the shell it
# re-launches itself detached, so the bar still runs without a console window.
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

# The bundled fallback config. The URI prefix must match the load path in main.cpp.
qt_add_qml_module(wqs
    URI wqs
    VERSION 1.0
    QML_FILES
        qml/shell.qml
)

target_link_libraries(wqs PRIVATE
    Qt6::Core Qt6::Gui Qt6::Quick Qt6::Qml Qt6::Network
    quickshell quickshell_io
    quickshellplugin quickshell_ioplugin
)

# A static QML module's type registration lives in an object file that nothing references,
# so the linker would drop it. Pull the generated plugin init objects in explicitly.
target_sources(wqs PRIVATE
    $<TARGET_OBJECTS:quickshellplugin_init>
    $<TARGET_OBJECTS:quickshell_ioplugin_init>
)

target_compile_definitions(wqs PRIVATE
    QT_DISABLE_DEPRECATED_BEFORE=0x060000
)
