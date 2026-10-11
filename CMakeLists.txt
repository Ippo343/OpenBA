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
#
# Off Windows (Phase D.4 POSIX + LINUX-1; handins/LINUX-1.md has the recipe) the targets are
# `reimpl-x64` (the Linux game over src/platform/posix + sdl and dxvk-native; needs
# tools/fetch_dxvk_native.sh first) and `unit` (the host unit tests over the src/platform/posix
# twins). src/platform/win and src/diag/win are never built there. (`linux-check`, the
# compile-only dry run of POSIX, went when the exe linked: the exe builds every TU it did.)
#   cmake -S . -B build/linux -G Ninja -DBANISHED_FLAVOUR=debug
#   cmake --build build/linux -t unit && build/linux/unit
#   cmake --build build/linux -t reimpl-x64
cmake_minimum_required(VERSION 3.20)
project(banished_reimpl CXX)
if(WIN32)
  enable_language(RC)   # src/app/icon.rc
endif()

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
# Off Windows, two flags pin what MinGW gives for free (plans/ABI.md C.7): `char` is signed
# (abi.h asserts it; ARM64 Linux defaults to unsigned) and bit-fields lay out MSVC-style
# (the tree has none today; abi_audit keeps it that way).
if(NOT WIN32)
  set(BANISHED_PORTABLE_FLAGS -fsigned-char -mms-bitfields)
  list(APPEND BANISHED_CXX_FLAGS ${BANISHED_PORTABLE_FLAGS})
endif()
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
if(WIN32)
  file(GLOB_RECURSE PLATFORM_SRCS CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/src/platform/win/*.cpp
                                                    ${CMAKE_SOURCE_DIR}/src/platform/common/*.cpp
                                                    ${CMAKE_SOURCE_DIR}/src/platform/sdl/*.cpp)
else()
  # D4_RULES C1: posix/ is globbed only off Windows. (src/platform/stub, the link stubs for
  # seam functions with no POSIX body, is gone: its one PORT GAP, the CRT's _purecall, is the
  # POSIX crash handler's since Phase D.4 LINUX-1, and the unit's own in tests/unit.)
  file(GLOB_RECURSE PLATFORM_SRCS CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/src/platform/posix/*.cpp
                                                    ${CMAKE_SOURCE_DIR}/src/platform/common/*.cpp
                                                    ${CMAKE_SOURCE_DIR}/src/platform/sdl/*.cpp)
endif()
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
if(NOT WIN32)
  # The Linux game links SDL3 shared (dxvk-native dlopens libSDL3.so.0; see reimpl-x64 below).
  set(SDL_SHARED ON CACHE BOOL "Banished: SDL3 vendored build option" FORCE)
endif()
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

if(WIN32)
add_executable(reimpl-x64 WIN32
  ${GAME_SRCS} ${ENGINE_SRCS} ${PLATFORM_SRCS} ${RENDER_SRCS} ${APP_SRCS} ${DIAG_SRCS}
  ${CMAKE_SOURCE_DIR}/src/app/icon.rc)
set_target_properties(reimpl-x64 PROPERTIES OUTPUT_NAME reimpl-x64 RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
target_include_directories(reimpl-x64 PRIVATE ${BANISHED_INCLUDES})
target_compile_options(reimpl-x64 PRIVATE ${BANISHED_CXX_FLAGS} ${FLAVOUR_FLAGS})
# the flavour flags are on the link line too (the Makefile does the same: -flto reaches the linker)
target_link_options(reimpl-x64 PRIVATE ${FLAVOUR_FLAGS} -static -static-libgcc -static-libstdc++ -mwindows)
# shell32: SHGetFolderPathW (Platform::DocumentsDir). No ole32 of ours since Phase D.4 SDL-5 D2;
# SDL3's own system libraries still bring it.
target_link_libraries(reimpl-x64 PRIVATE user32 dbghelp shell32)
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
else()
# ---------------------------------------------------------------------------------------
# The Linux game (Phase D.4 LINUX-1, RENDER step 4): the Windows exe's tree -- game, engine,
# app, diag (+ src/diag/posix, the twins of src/diag/win), the dx11 renderer -- over
# src/platform/posix + sdl + common. `cmake --build <dir> -t reimpl-x64` -> <dir>/reimpl-x64.
# The renderer is the same dx11 code over dxvk-native 3.1.1 (tools/fetch_dxvk_native.sh;
# third_party/dxvk-native/README.md). It links no graphics library:
# src/platform/posix/gfx_posix.cpp dlopens libdxvk_dxgi / libdxvk_d3d11 at run time, so only
# the headers are used, and only by src/render/dx11 (as -isystem: they are MinGW's and
# dxvk's, not ours). DXVK_WSI_DRIVER=SDL3 is set by the run script (tools/linux_run.sh),
# never read by our code (D4_RULES C11). Skipped, with a message, until the fetch has run.
# ---------------------------------------------------------------------------------------
set(BANISHED_DXVK_NATIVE ${CMAKE_SOURCE_DIR}/third_party/dxvk-native/3.1.1/usr)
if(EXISTS ${BANISHED_DXVK_NATIVE}/include/dxvk/d3d11.h)
  file(GLOB DIAG_POSIX_SRCS CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/src/diag/posix/*.cpp)
  add_executable(reimpl-x64
    ${GAME_SRCS} ${ENGINE_SRCS} ${PLATFORM_SRCS} ${RENDER_SRCS} ${APP_SRCS} ${DIAG_SRCS}
    ${DIAG_POSIX_SRCS})
  # RPATH $ORIGIN: the game directory carries libSDL3.so.0 beside the exe (below).
  set_target_properties(reimpl-x64 PROPERTIES OUTPUT_NAME reimpl-x64 RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
                                              BUILD_RPATH "$ORIGIN")
  target_include_directories(reimpl-x64 PRIVATE ${BANISHED_INCLUDES})
  set_source_files_properties(${RENDER_SRCS} PROPERTIES
                              COMPILE_OPTIONS "-isystem;${BANISHED_DXVK_NATIVE}/include/dxvk")
  target_compile_options(reimpl-x64 PRIVATE ${BANISHED_CXX_FLAGS} ${FLAVOUR_FLAGS})
  # -rdynamic: the exe's own functions in the dynamic symbol table, so the crash and hang
  # dumps (backtrace + dladdr, src/diag/posix) name them.
  target_link_options(reimpl-x64 PRIVATE ${FLAVOUR_FLAGS} -rdynamic)
  find_package(Threads REQUIRED)
  # SDL3 SHARED here, not static: dxvk-native's SDL3 WSI dlopens libSDL3.so.0 and resolves
  # every SDL call through that handle, so it must reach the SAME SDL instance that owns
  # our SDL_Window (a static copy in the exe would be a second SDL with no window). It is
  # built from the same pinned submodule; glibc's dlopen returns the copy the exe already
  # loaded (matched by SONAME).
  target_link_libraries(reimpl-x64 PRIVATE SDL3::SDL3-shared wmadec Threads::Threads ${CMAKE_DL_LIBS})
else()
  message(STATUS "reimpl-x64 (Linux) not configured: run tools/fetch_dxvk_native.sh for the dxvk-native headers")
endif()
endif()

# ---------------------------------------------------------------------------------------
# Host unit tests (ENDGAME B): the real classes standing on their own. tests/unit/README.md.
# tests/unit/sources.cmake lists the tree sources each test family links against (the
# CMake twin of tests/unit/sources.mk; keep the two in step until the Makefile retires).
# ---------------------------------------------------------------------------------------
include(${CMAKE_SOURCE_DIR}/tests/unit/sources.cmake)
if(NOT WIN32)
  # Each src/platform/win/<seam>_win.cpp the tests link has its src/platform/posix twin.
  list(TRANSFORM UNIT_TREE_SRCS REPLACE "/src/platform/win/([a-z_]+)_win\\.cpp$" "/src/platform/posix/\\1_posix.cpp")
  # error.cpp's fallback exit is kernel32!ExitProcess on Windows and Platform::ExitProcess
  # (os_posix.cpp) here; the POSIX test file exercises the rest of os_posix.cpp.
  list(APPEND UNIT_TREE_SRCS ${CMAKE_SOURCE_DIR}/src/platform/posix/os_posix.cpp)
  # os_posix.cpp's ReleaseModule / ExecutableImageBase (dlclose, dl_iterate_phdr).
  set(UNIT_POSIX_LIBS ${CMAKE_DL_LIBS})
endif()
file(GLOB UNIT_SRCS CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/tests/unit/*.cpp)
add_executable(unit EXCLUDE_FROM_ALL ${UNIT_SRCS} ${UNIT_TREE_SRCS})
set_target_properties(unit PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
target_include_directories(unit PRIVATE ${BANISHED_INCLUDES} ${CMAKE_SOURCE_DIR}/tests/unit)
target_compile_options(unit PRIVATE
  -fexceptions -fno-strict-aliasing -fno-lifetime-dse -Wall -Wextra -Wno-invalid-offsetof
  -g -O0 -D_DEBUG -DBANISHED_UNIT_TEST ${BANISHED_PORTABLE_FLAGS})
if(WIN32)
  target_link_options(unit PRIVATE -static -static-libgcc -static-libstdc++)
else()
  # glibc stays dynamic (getpwuid_r needs NSS at run time); pthreads for the POSIX twins.
  find_package(Threads REQUIRED)
  target_link_libraries(unit PRIVATE Threads::Threads ${UNIT_POSIX_LIBS})
endif()
# tests/unit/xwma_decode_test.cpp decodes through the extracted WMA v2 decoder
target_link_libraries(unit PRIVATE wmadec)
add_custom_target(run-unit COMMAND unit DEPENDS unit WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
