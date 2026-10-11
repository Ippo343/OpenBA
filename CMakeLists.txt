# Banished x64 reimplementation -- CMake build (Phase D, 2026-10-10).
#
# Mirrors the Makefile flavour for flavour; the Makefile stays the gate's build until the
# kit scripts are ported (NEXT_SESSION.md §2.1). Configure one build directory per flavour:
#
#   cmake -S . -B build/cmake-debug -G Ninja -DBANISHED_FLAVOUR=debug
#   cmake -S . -B build/cmake-perf  -G Ninja -DBANISHED_FLAVOUR=perf
#   cmake --build build/cmake-debug            # -> build/cmake-debug/reimpl-x64.exe
#   cmake --build build/cmake-debug -t unit    # host unit tests (build/cmake-debug/unit.exe)
#
# Flavours (the Makefile target in brackets):
#   debug   [standalone-debug]  -O0 -g -D_DEBUG, logs kept
#   og      [standalone-og]     -Og -g -D_DEBUG, logs kept
#   perf    [standalone-perf]   -O2 -g -DNDEBUG, logs stripped, PerfX knobs default-on
#   release [standalone]        -O3 -DNDEBUG, logs stripped, PerfX knobs default-off
cmake_minimum_required(VERSION 3.20)
project(banished_reimpl CXX RC)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)

set(BANISHED_FLAVOUR "debug" CACHE STRING "debug | og | perf | release")
set_property(CACHE BANISHED_FLAVOUR PROPERTY STRINGS debug og perf release)
option(BANISHED_USE_CCACHE "Compile through ccache when it is on PATH" ON)
set(BANISHED_PERF_EXPERIMENTAL "" CACHE STRING "override the PerfX default (1 / 0) for perf / release")
set(BANISHED_GAME_DIR "C:/GOG Games/Banished" CACHE PATH "where sa-deploy copies the exe")

if(BANISHED_USE_CCACHE)
  find_program(CCACHE_PROGRAM ccache)
  if(CCACHE_PROGRAM)
    set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
  endif()
endif()

# The three flags that are NOT tuning knobs (the Makefile's comment block explains each):
#   -fno-strict-aliasing : ported bodies pun raw offsets through two lvalue types
#   -fno-lifetime-dse    : retail dtors null the fields they free and callers rely on it
#   -fexceptions         : spelled out (ENDGAME B.1 decision 7); no object may be built without it
set(BANISHED_CXX_FLAGS
  -fexceptions -fno-strict-aliasing -fno-lifetime-dse
  -Wall -Wextra -Wno-cast-function-type -Wno-invalid-offsetof)
set(BANISHED_STRIP_LOGS -DBANISHED_STRIP_LOGS -include ${CMAKE_SOURCE_DIR}/src/app/log_strip.h)

if(BANISHED_FLAVOUR STREQUAL "debug")
  set(FLAVOUR_FLAGS -g -O0 -D_DEBUG)
elseif(BANISHED_FLAVOUR STREQUAL "og")
  set(FLAVOUR_FLAGS -Og -g -D_DEBUG)
elseif(BANISHED_FLAVOUR STREQUAL "perf")
  if(BANISHED_PERF_EXPERIMENTAL STREQUAL "")
    set(BANISHED_PERF_EXPERIMENTAL 1)
  endif()
  set(FLAVOUR_FLAGS -O2 -g -DNDEBUG -DBANISHED_PERF_EXPERIMENTAL=${BANISHED_PERF_EXPERIMENTAL} ${BANISHED_STRIP_LOGS})
elseif(BANISHED_FLAVOUR STREQUAL "release")
  if(BANISHED_PERF_EXPERIMENTAL STREQUAL "")
    set(BANISHED_PERF_EXPERIMENTAL 0)
  endif()
  set(FLAVOUR_FLAGS -O3 -DNDEBUG -DBANISHED_PERF_EXPERIMENTAL=${BANISHED_PERF_EXPERIMENTAL} ${BANISHED_STRIP_LOGS})
else()
  message(FATAL_ERROR "BANISHED_FLAVOUR must be debug, og, perf or release (got '${BANISHED_FLAVOUR}')")
endif()

# ---------------------------------------------------------------------------------------
# Sources: the tree mirrors the original source layout; every directory is globbed.
# CONFIGURE_DEPENDS re-globs on build so a new file needs no edit here.
# ---------------------------------------------------------------------------------------
file(GLOB_RECURSE GAME_SRCS     CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/src/game/*.cpp)
file(GLOB_RECURSE ENGINE_SRCS   CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/src/engine/*.cpp)
file(GLOB_RECURSE PLATFORM_SRCS CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/src/platform/win/*.cpp
                                                  ${CMAKE_SOURCE_DIR}/src/platform/common/*.cpp
                                                  ${CMAKE_SOURCE_DIR}/src/platform/sdl/*.cpp)
file(GLOB_RECURSE RENDER_SRCS   CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/src/render/dx11/*.cpp)
file(GLOB         APP_SRCS      CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/src/app/*.cpp)
file(GLOB         DIAG_SRCS     CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/src/diag/*.cpp)
# src/diag/win: the Windows-only diagnostics (crash / hang handlers, watchdog, sampler,
# hardware watchpoints, redzone allocator; D4_RULES C10). Globbed only on Windows.
if(WIN32)
  file(GLOB       DIAG_WIN_SRCS CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/src/diag/win/*.cpp)
  list(APPEND DIAG_SRCS ${DIAG_WIN_SRCS})
endif()

set(BANISHED_INCLUDES
  ${CMAKE_SOURCE_DIR}/src/game
  ${CMAKE_SOURCE_DIR}/src/engine
  ${CMAKE_SOURCE_DIR}/src/render/dx11
  ${CMAKE_SOURCE_DIR}/src/app
  ${CMAKE_SOURCE_DIR}/src/diag
  ${CMAKE_SOURCE_DIR}/src/platform/common)

# ---------------------------------------------------------------------------------------
# SDL3: the pinned submodule third_party/SDL (tag in docs/reference/environment.md), built static
# from source with the same options as the Makefile's `third_party` rule. EXCLUDE_FROM_ALL: only
# what reimpl-x64 links is built; `unit` never links SDL (D4_RULES C7). The game itself compiles no
# C, so the C flags below reach only the vendored C (SDL and wmadec): optimised whatever the flavour
# (the Makefile links one Release SDL into every flavour), without touching the flavour's C++ flags.
# ---------------------------------------------------------------------------------------
if(NOT EXISTS ${CMAKE_SOURCE_DIR}/third_party/SDL/CMakeLists.txt)
  message(FATAL_ERROR "third_party/SDL is empty: run `git submodule update --init`")
endif()
foreach(_sdl_opt SDL_SHARED SDL_TEST_LIBRARY SDL_TESTS SDL_EXAMPLES SDL_GPU SDL_RENDER SDL_CAMERA
                 SDL_JOYSTICK SDL_HAPTIC SDL_HIDAPI SDL_SENSOR SDL_POWER SDL_DIALOG SDL_TRAY)
  set(${_sdl_opt} OFF CACHE BOOL "Banished: SDL3 vendored build option" FORCE)
endforeach()
set(SDL_STATIC ON CACHE BOOL "Banished: SDL3 vendored build option" FORCE)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O2 -DNDEBUG")
endif()
add_subdirectory(${CMAKE_SOURCE_DIR}/third_party/SDL ${CMAKE_BINARY_DIR}/third_party/sdl3 EXCLUDE_FROM_ALL)
if(NOT TARGET SDL3::SDL3-static)
  message(FATAL_ERROR "third_party/SDL did not define SDL3::SDL3-static")
endif()

# The WMA v2 decoder extracted from FFmpeg (third_party/wmadec/EXTRACT.md; D4_RULES §3 Q5): every
# shipped sound is xWMA. A static C library with its own fixed flags; reimpl-x64 and unit link it
# (src/engine/audio/xwma_decode.cpp and its test).
add_subdirectory(${CMAKE_SOURCE_DIR}/third_party/wmadec ${CMAKE_BINARY_DIR}/third_party/wmadec EXCLUDE_FROM_ALL)

# The log-strip guard the Makefile runs before every log-stripped build (tools/check_log_strip.py)
find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_Interpreter_FOUND)
  add_custom_target(check-log-strip
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/check_log_strip.py
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "check_log_strip: no qualified std::printf")
endif()

add_executable(reimpl-x64 WIN32
  ${GAME_SRCS} ${ENGINE_SRCS} ${PLATFORM_SRCS} ${RENDER_SRCS} ${APP_SRCS} ${DIAG_SRCS}
  ${CMAKE_SOURCE_DIR}/src/app/icon.rc)
set_target_properties(reimpl-x64 PROPERTIES OUTPUT_NAME reimpl-x64 RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
target_include_directories(reimpl-x64 PRIVATE ${BANISHED_INCLUDES})
target_compile_options(reimpl-x64 PRIVATE ${BANISHED_CXX_FLAGS} ${FLAVOUR_FLAGS})
# the flavour flags are on the link line too (the Makefile does the same: -flto reaches the linker)
target_link_options(reimpl-x64 PRIVATE ${FLAVOUR_FLAGS} -static -static-libgcc -static-libstdc++ -mwindows)
# shell32: SHGetFolderPathW (Platform::DocumentsDir); ole32: CoInitializeEx in Engine::Startup
target_link_libraries(reimpl-x64 PRIVATE user32 dbghelp shell32 ole32)
# SDL3 static: the target carries its include directory and its system libraries
target_link_libraries(reimpl-x64 PRIVATE SDL3::SDL3-static)
# xWMA -> PCM (third_party/wmadec): the target carries its include directory
target_link_libraries(reimpl-x64 PRIVATE wmadec)
if(TARGET check-log-strip)
  add_dependencies(reimpl-x64 check-log-strip)
endif()

add_custom_target(sa-deploy
  COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:reimpl-x64> "${BANISHED_GAME_DIR}/reimpl-x64.exe"
  COMMAND ${CMAKE_COMMAND} -E rm -f "${BANISHED_GAME_DIR}/standalone_debug.log"
  DEPENDS reimpl-x64
  COMMENT "Deploy to ${BANISHED_GAME_DIR}/reimpl-x64.exe")

# ---------------------------------------------------------------------------------------
# Host unit tests (ENDGAME B): the real classes standing on their own. tests/unit/README.md.
# tests/unit/sources.cmake lists the tree sources each test family links against (the
# CMake twin of tests/unit/sources.mk; keep the two in step until the Makefile retires).
# ---------------------------------------------------------------------------------------
include(${CMAKE_SOURCE_DIR}/tests/unit/sources.cmake)
file(GLOB UNIT_SRCS CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/tests/unit/*.cpp)
add_executable(unit EXCLUDE_FROM_ALL ${UNIT_SRCS} ${UNIT_TREE_SRCS})
set_target_properties(unit PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
target_include_directories(unit PRIVATE ${BANISHED_INCLUDES} ${CMAKE_SOURCE_DIR}/tests/unit)
target_compile_options(unit PRIVATE
  -fexceptions -fno-strict-aliasing -fno-lifetime-dse -Wall -Wextra -Wno-invalid-offsetof
  -g -O0 -D_DEBUG -DBANISHED_UNIT_TEST)
target_link_options(unit PRIVATE -static -static-libgcc -static-libstdc++)
# tests/unit/xwma_decode_test.cpp decodes through the extracted WMA v2 decoder
target_link_libraries(unit PRIVATE wmadec)
add_custom_target(run-unit COMMAND unit DEPENDS unit WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
