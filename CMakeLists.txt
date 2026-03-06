cmake_minimum_required(VERSION 3.14)
project(lisper VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2)

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

add_executable(lisper
    src/main.cpp
    src/lisper.cpp
    src/media.cpp
    src/formatter.cpp
    src/watcher.cpp
    src/live.cpp
)

target_include_directories(lisper PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${SDL2_INCLUDE_DIRS}
)

target_link_libraries(lisper PRIVATE whisper PkgConfig::SDL2)
target_compile_definitions(lisper PRIVATE LISPER_ENABLE_LIVE)

if(APPLE)
    target_link_libraries(lisper PRIVATE "-framework Foundation")
endif()
