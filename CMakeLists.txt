cmake_minimum_required(VERSION 3.14)
project(lisper VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(LISPER_BUILD_GUI "Build the Lisper desktop GUI" ON)

find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(SDL2 QUIET sdl2 IMPORTED_TARGET)
endif()

# whisper.cpp build options
set(WHISPER_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(WHISPER_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(WHISPER_BUILD_SERVER OFF CACHE BOOL "" FORCE)

if(EXISTS ${CMAKE_SOURCE_DIR}/third_party/whisper.cpp/CMakeLists.txt)
    add_subdirectory(third_party/whisper.cpp)
else()
    message(FATAL_ERROR
        "whisper.cpp not found. Run:\n"
        "  git submodule add https://github.com/ggerganov/whisper.cpp third_party/whisper.cpp\n"
        "  git submodule update --init --recursive"
    )
endif()

set(LISPER_CORE_SOURCES
    src/interrupt.cpp
    src/model_profiles.cpp
    src/lisper.cpp
    src/media.cpp
    src/formatter.cpp
    src/watcher.cpp
)

if(SDL2_FOUND)
    list(APPEND LISPER_CORE_SOURCES src/live.cpp)
    message(STATUS "SDL2 found - enabling live transcription feature")
else()
    message(STATUS "SDL2 not found - live transcription will be disabled")
endif()

add_library(lisper_core STATIC ${LISPER_CORE_SOURCES})

target_include_directories(lisper_core PUBLIC
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(lisper_core PUBLIC whisper)

if(SDL2_FOUND)
    target_include_directories(lisper_core PRIVATE ${SDL2_INCLUDE_DIRS})
    target_link_libraries(lisper_core PUBLIC PkgConfig::SDL2)
    target_compile_definitions(lisper_core PUBLIC LISPER_ENABLE_LIVE)
endif()

add_executable(lisper
    src/main.cpp
    src/cli_style.cpp
)

target_link_libraries(lisper PRIVATE lisper_core)

set(IMGUI_DIR ${CMAKE_SOURCE_DIR}/third_party/imgui)

if(LISPER_BUILD_GUI AND SDL2_FOUND AND EXISTS ${IMGUI_DIR}/imgui.cpp)
    add_executable(lisper_gui
        src/gui_main.cpp
        src/gui_app.cpp
        src/gui/state.cpp
        src/gui/theme.cpp
        src/gui/background_transcriber.cpp
        src/gui/file_dialog.cpp
        src/gui/layout.cpp
        third_party/tinyfiledialogs/tinyfiledialogs.c
        ${IMGUI_DIR}/imgui.cpp
        ${IMGUI_DIR}/imgui_draw.cpp
        ${IMGUI_DIR}/imgui_tables.cpp
        ${IMGUI_DIR}/imgui_widgets.cpp
        ${IMGUI_DIR}/backends/imgui_impl_sdl2.cpp
        ${IMGUI_DIR}/backends/imgui_impl_sdlrenderer2.cpp
    )

    target_include_directories(lisper_gui PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${IMGUI_DIR}
        ${IMGUI_DIR}/backends
        ${CMAKE_SOURCE_DIR}/third_party/tinyfiledialogs
    )

    target_compile_definitions(lisper_gui PRIVATE SDL_MAIN_HANDLED)
    target_link_libraries(lisper_gui PRIVATE lisper_core PkgConfig::SDL2)
    if(WIN32)
        target_link_libraries(lisper_gui PRIVATE comdlg32 ole32)
    endif()
    message(STATUS "Building lisper_gui desktop app")
elseif(LISPER_BUILD_GUI)
    message(STATUS "GUI target disabled: install SDL2 and ensure third_party/imgui is available")
endif()

if(APPLE)
    target_link_libraries(lisper_core PUBLIC "-framework Foundation")
endif()
