cmake_minimum_required(VERSION 3.14)
project(lisper VERSION 0.4.0 LANGUAGES C CXX)
if(APPLE)
    # Objective-C (whisper.cpp's Metal backend, *.m) and Objective-C++ (our
    # gui/platform_mac.mm). Enable both up front so .m files are compiled as
    # Objective-C, not Objective-C++.
    enable_language(OBJC)
    enable_language(OBJCXX)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(LISPER_BUILD_GUI "Build the Lisper desktop GUI" ON)
option(LISPER_ENABLE_LIVE "Enable microphone capture (live mode, dictation) when SDL2 is available" ON)
option(LISPER_BUILD_TESTS "Build the Lisper unit tests" OFF)

include(GNUInstallDirs)
if(LISPER_BUILD_TESTS)
    enable_testing()
endif()

set(SDL2_FOUND FALSE)
if(LISPER_ENABLE_LIVE OR LISPER_BUILD_GUI)
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(SDL2 QUIET sdl2 IMPORTED_TARGET)
    endif()
    # vcpkg / SDL2Config.cmake fallback (Windows and some macOS setups)
    if(NOT SDL2_FOUND)
        find_package(SDL2 CONFIG QUIET)
        if(TARGET SDL2::SDL2)
            add_library(PkgConfig::SDL2 INTERFACE IMPORTED)
            target_link_libraries(PkgConfig::SDL2 INTERFACE SDL2::SDL2)
            set(SDL2_FOUND TRUE)
        endif()
    endif()
endif()

# whisper.cpp build options: link statically so lisper, lisper_gui and
# lisper-mcp are self-contained executables (no DLL/dylib to ship).
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
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
    src/subprocess.cpp
    src/paths.cpp
    src/model_download.cpp
    src/text_cleanup.cpp
    src/settings.cpp
    src/history.cpp
    src/audio_capture.cpp
    src/mcp_server.cpp
)

if(SDL2_FOUND AND LISPER_ENABLE_LIVE)
    list(APPEND LISPER_CORE_SOURCES src/live.cpp)
    message(STATUS "SDL2 found - enabling live transcription feature")
else()
    message(STATUS "SDL2 not found - live transcription will be disabled")
endif()

add_library(lisper_core STATIC ${LISPER_CORE_SOURCES})

target_include_directories(lisper_core PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/lisper>
)

target_include_directories(lisper_core PRIVATE ${CMAKE_SOURCE_DIR}/third_party)
target_compile_definitions(lisper_core PRIVATE LISPER_VERSION="${PROJECT_VERSION}")
target_link_libraries(lisper_core PUBLIC whisper)

if(SDL2_FOUND AND LISPER_ENABLE_LIVE)
    target_link_libraries(lisper_core PUBLIC PkgConfig::SDL2)
    target_compile_definitions(lisper_core PUBLIC LISPER_ENABLE_LIVE)
endif()

add_executable(lisper
    src/main.cpp
    src/cli_style.cpp
)

target_link_libraries(lisper PRIVATE lisper_core)

# MCP server (stdio) for Claude Desktop, Claude Code, Cursor and other clients
add_executable(lisper-mcp src/mcp_main.cpp)
target_compile_definitions(lisper-mcp PRIVATE LISPER_VERSION="${PROJECT_VERSION}")
target_link_libraries(lisper-mcp PRIVATE lisper_core)

set(IMGUI_DIR ${CMAKE_SOURCE_DIR}/third_party/imgui)

if(LISPER_BUILD_GUI AND SDL2_FOUND AND EXISTS ${IMGUI_DIR}/imgui.cpp)
    set(LISPER_FONT_DIR ${CMAKE_SOURCE_DIR}/third_party/fonts)
    set(LISPER_FONTS_C ${CMAKE_BINARY_DIR}/generated/lisper_fonts.c)
    file(GLOB LISPER_FONT_FILES ${LISPER_FONT_DIR}/*.ttf)
    add_custom_command(
        OUTPUT ${LISPER_FONTS_C}
        COMMAND ${CMAKE_COMMAND} -DOUT=${LISPER_FONTS_C} -DFONT_DIR=${LISPER_FONT_DIR}
                -P ${CMAKE_SOURCE_DIR}/cmake/embed_fonts.cmake
        DEPENDS ${LISPER_FONT_FILES} ${CMAKE_SOURCE_DIR}/cmake/embed_fonts.cmake
        COMMENT "Embedding UI fonts")

    add_executable(lisper_gui
        ${LISPER_FONTS_C}
        src/gui_main.cpp
        src/gui_app.cpp
        src/gui/state.cpp
        src/gui/theme.cpp
        src/gui/background_transcriber.cpp
        src/gui/file_dialog.cpp
        src/gui/layout.cpp
        src/gui/hotkey.cpp
        src/gui/text_insert.cpp
        src/gui/control_channel.cpp
        src/gui/sounds.cpp
        src/gui/dictation.cpp
        src/gui/overlay.cpp
        src/gui/platform.cpp
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

    target_compile_definitions(lisper_gui PRIVATE SDL_MAIN_HANDLED LISPER_VERSION="${PROJECT_VERSION}")
    target_link_libraries(lisper_gui PRIVATE lisper_core PkgConfig::SDL2)
    if(WIN32)
        target_link_libraries(lisper_gui PRIVATE comdlg32 ole32 user32)
        # GUI subsystem (no console window) while keeping a standard main().
        set_target_properties(lisper_gui PROPERTIES WIN32_EXECUTABLE TRUE)
        if(MSVC)
            target_link_options(lisper_gui PRIVATE /ENTRY:mainCRTStartup)
        endif()
    elseif(APPLE)
        target_sources(lisper_gui PRIVATE src/gui/platform_mac.mm)
        set_source_files_properties(src/gui/platform_mac.mm PROPERTIES COMPILE_OPTIONS "-fobjc-arc")
        target_link_libraries(lisper_gui PRIVATE
            "-framework Carbon" "-framework ApplicationServices" "-framework CoreFoundation"
            "-framework AppKit" "-framework QuartzCore")
        set_target_properties(lisper_gui PROPERTIES
            MACOSX_BUNDLE TRUE
            OUTPUT_NAME "Lisper"
            MACOSX_BUNDLE_INFO_PLIST ${CMAKE_SOURCE_DIR}/cmake/Info.plist.in
            MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION}
            MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION})
    else()
        # Global dictation shortcut on X11 (Wayland users bind `lisper_gui --toggle`).
        find_package(X11 QUIET)
        if(X11_FOUND)
            target_compile_definitions(lisper_gui PRIVATE LISPER_HAVE_X11)
            target_include_directories(lisper_gui PRIVATE ${X11_INCLUDE_DIR})
            target_link_libraries(lisper_gui PRIVATE ${X11_LIBRARIES})
            message(STATUS "X11 found - enabling global dictation shortcut")
        endif()
    endif()
    if(LISPER_BUILD_TESTS)
        add_executable(lisper_gui_tests tests/gui_tests.cpp src/gui/hotkey.cpp)
        target_include_directories(lisper_gui_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)
        target_link_libraries(lisper_gui_tests PRIVATE PkgConfig::SDL2)
        if(APPLE)
            target_link_libraries(lisper_gui_tests PRIVATE "-framework Carbon")
        elseif(UNIX AND X11_FOUND)
            target_compile_definitions(lisper_gui_tests PRIVATE LISPER_HAVE_X11)
            target_link_libraries(lisper_gui_tests PRIVATE ${X11_LIBRARIES})
        endif()
        add_test(NAME lisper_gui_tests COMMAND lisper_gui_tests)
    endif()
    message(STATUS "Building lisper_gui desktop app")
elseif(LISPER_BUILD_GUI)
    message(STATUS "GUI target disabled: install SDL2 and ensure third_party/imgui is available")
endif()

if(APPLE)
    target_link_libraries(lisper_core PUBLIC "-framework Foundation")
endif()

if(LISPER_BUILD_TESTS)
    add_executable(lisper_tests tests/unit_tests.cpp)
    target_include_directories(lisper_tests PRIVATE ${CMAKE_SOURCE_DIR}/third_party)
    target_link_libraries(lisper_tests PRIVATE lisper_core)
    add_test(NAME lisper_unit_tests COMMAND lisper_tests)
    set_tests_properties(lisper_unit_tests PROPERTIES
        ENVIRONMENT "LISPER_HOME=${CMAKE_BINARY_DIR}/test_home;LISPER_SAMPLE_WAV=${CMAKE_SOURCE_DIR}/third_party/whisper.cpp/samples/jfk.wav")
endif()

# ---------------------------------------------------------------------------
# Install
# ---------------------------------------------------------------------------
install(TARGETS lisper lisper-mcp RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
if(TARGET lisper_gui)
    install(TARGETS lisper_gui
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        BUNDLE DESTINATION Applications)
    if(UNIX AND NOT APPLE)
        install(FILES packaging/lisper.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
    endif()
endif()
install(FILES
    src/lisper.h src/formatter.h src/media.h src/model_profiles.h src/paths.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/lisper)
