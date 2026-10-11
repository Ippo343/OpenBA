# Banished Reimplementation - Build System (standalone replacement exe)
# Usage:
#   make                  - build debug standalone exe
#   make standalone-debug - build debug standalone exe
#   make standalone       - build release standalone exe
#   make sa-deploy        - build debug + copy to Banished directory
#   make sa-run           - build + copy + launch (auto-starts a new game)
#   make sa-run-menu      - build + copy + launch, stop at main menu
#   make standalone-og    - build FAST debug exe (-Og, same logs) -- use this
#                           for play-throughs; -O0 runs the tutorial at ~1 fps
#   make sa-run-og        - -Og build + copy + launch (auto-starts a new game)
#   make sa-run-og-menu   - -Og build + copy + launch, stop at main menu
#   make clean            - remove build artifacts

# Enable parallel builds by default (override with make -j1 for sequential).
# Only when NO -j was given: an unconditional append overrode the caller's -jN in
# every SUB-make (standalone-perf, standalone-og), so `make -j3 standalone-perf`
# compiled at -j16 and several such builds at once ran the machine out of memory.
ifeq (,$(findstring -j,$(MAKEFLAGS))$(findstring jobserver,$(MAKEFLAGS)))
MAKEFLAGS += -j$(shell nproc 2>/dev/null || echo 4)
endif

CXX = ccache g++
CC = ccache gcc

BANISHED_DIR = C:/GOG Games/Banished

# Auto-discover clean game logic files (mirrors original source tree)
GAME_SRC_SRCS = $(wildcard src/game/*/*.cpp)

# Runtime-x64.dll reimplementation sources
RUNTIME_FUNC_SRCS = $(wildcard src/engine/*.cpp) $(wildcard src/platform/win/*.cpp) \
                    $(wildcard src/platform/common/*.cpp) $(wildcard src/platform/sdl/*.cpp)
RUNTIME_SRC_SRCS = $(wildcard src/engine/*/*.cpp)
# VideoDX11-x64.dll reimplementation sources (see src/render/dx11/README.md)
DX11_FUNC_SRCS =
DX11_SRC_SRCS = $(wildcard src/render/dx11/*.cpp)

# ============================================================
# Standalone EXE build
# ============================================================
SA_BUILD_DIR = build/debug
SA_OBJ_DIR = $(SA_BUILD_DIR)/obj
SA_TARGET = $(SA_BUILD_DIR)/reimpl-x64.exe

# -fno-strict-aliasing is MANDATORY, not a tuning knob. Every ported function reads
# and writes binary-layout struct fields by punning raw offsets (e.g. the action-stack
# top index at +0x18 is read as `param_1[3]` through an int64_t* and written as
# `*(int*)(param_1 + 3)` — retail's exact instruction pair). Under strict aliasing GCC
# assumes those two lvalues cannot be the same object, so the store is invisible to the
# load: at -O2+ it proved CitizenAI_ActionExecutor_c4c50's pop loop could never exit and
# emitted a literal `jmp .` self-loop, which froze the game in-world with the citizens
# T-posing (the sim tick never returned). Do not remove.
# -fno-lifetime-dse is MANDATORY for the same reason. Retail's destructors NULL the fields
# they free (`~Array` zeroes data/size/capacity) and retail's callers rely on it: a class
# destructor body frees a member array through its ported dtor and the compiler's member
# destructor then runs on the same field. GCC at -O2 treats stores to a dying object as
# dead and deletes them, so the second pass freed a stale pointer (the perf exe corrupted
# the heap in ~MeshInstance at the menu shutdown, 2026-09-27; -O0 kept the stores and
# passed). The same rule deletes field stores made BEFORE a placement `::new (p) T()`.
# Do not remove.
# -fexceptions is GCC's default for C++ and is spelled out on purpose (ENDGAME B.1 decision 7,
# B9): the tree has no setjmp / longjmp / SEH; where retail unwinds, the port unwinds with C++
# exceptions through real destructors, so no object may ever be built with -fno-exceptions.
SA_CXXFLAGS = -std=c++17 -fexceptions -fno-strict-aliasing -fno-lifetime-dse -Wall -Wextra -Wno-cast-function-type -Wno-invalid-offsetof -MMD -MP
SA_LDFLAGS = -static -static-libgcc -static-libstdc++ -mwindows
# -lshell32: src/platform/win/settings_win.cpp calls SHGetFolderPathW for
# Platform::DocumentsDir (FileSystem::GetSaveDirectory's OS half, Phase D.4 SETTINGS E.2).
# No -lole32 of ours since Phase D.4 SDL-5 D2 (the COM init is gone). SDL3's own
# libraries (SDL3_SYSLIBS below) still link ole32: SDL calls CoInitializeEx itself.
SA_LIBS = -luser32 -ldbghelp -lshell32
SA_INCLUDES = -I. -Isrc/game \
              -Isrc/engine \
              -Isrc/render/dx11 -Isrc/app -Isrc/diag \
              -Isrc/platform/common

# ============================================================
# Third-party: SDL3, a pinned submodule (third_party/SDL, tag in docs/reference/environment.md),
# built ONCE as a static Release library and installed under build/third_party/install. Every
# flavour (debug / og / perf / release) links the same library. The stamp file is the rule's
# output: delete build/third_party/sdl3.stamp (or all of build/third_party) to rebuild it.
# The unit tests and the resave tool do NOT link SDL (D4_RULES C7): only SA_LIBS carries it.
# The system libraries SDL needs are read from the installed sdl3.pc at LINK time, never retyped
# here (a static-only SDL build lists them on `Libs:`, a shared one on `Libs.private:`; both
# lines are read). `-L<dir> -lSDL3` is dropped from that list: the archive is linked by path.
# ============================================================
CMAKE ?= cmake
SDL3_SRC_DIR = third_party/SDL
SDL3_BUILD_DIR = build/third_party/sdl3
SDL3_PREFIX = build/third_party/install
SDL3_STAMP = build/third_party/sdl3.stamp
SDL3_LIB = $(SDL3_PREFIX)/lib/libSDL3.a
SDL3_PC = $(SDL3_PREFIX)/lib/pkgconfig/sdl3.pc
SDL3_CMAKE_OPTS = -G Ninja -DCMAKE_BUILD_TYPE=Release -DSDL_SHARED=OFF -DSDL_STATIC=ON \
                  -DSDL_TEST_LIBRARY=OFF -DSDL_TESTS=OFF -DSDL_EXAMPLES=OFF -DSDL_GPU=OFF \
                  -DSDL_RENDER=OFF -DSDL_CAMERA=OFF -DSDL_JOYSTICK=OFF -DSDL_HAPTIC=OFF \
                  -DSDL_HIDAPI=OFF -DSDL_SENSOR=OFF -DSDL_POWER=OFF -DSDL_DIALOG=OFF -DSDL_TRAY=OFF \
                  -DCMAKE_INSTALL_PREFIX=$(CURDIR)/$(SDL3_PREFIX)
# Expanded inside the link recipe, where `$$(...)` becomes a shell command substitution that runs
# after the stamp rule (a parse-time $(shell) would read nothing on a fresh checkout).
SDL3_SYSLIBS = $$(sed -n -e 's/^Libs\(\.private\)\{0,1\}: *//p' $(SDL3_PC) | sed -e 's/-L[^ ]*//g' -e 's/-lSDL3\b//g')
SA_INCLUDES += -I$(SDL3_PREFIX)/include
SA_LIBS += $(SDL3_LIB) $(SDL3_SYSLIBS)

# Third-party: the WMA v2 decoder extracted from FFmpeg n8.1.3 (third_party/wmadec, LGPL-2.1;
# EXTRACT.md there has the file list, the shim and the licence notes). Every shipped sound is
# xWMA (D4_RULES §3 Q5). Built ONCE as a static C library with fixed flags whatever the flavour,
# like SDL3, under build/third_party/wmadec; the archive is its own stamp (a plain file rule over
# its objects). -w: the warnings are upstream's (EXTRACT.md "Build"). The exe and `make unit`
# link it; the CMake twin is third_party/wmadec/CMakeLists.txt.
WMADEC_DIR = third_party/wmadec
WMADEC_BUILD_DIR = build/third_party/wmadec
WMADEC_LIB = $(WMADEC_BUILD_DIR)/libwmadec.a
WMADEC_SRCS = $(addprefix $(WMADEC_DIR)/, \
                ffmpeg/libavcodec/wmadec.c ffmpeg/libavcodec/wma.c ffmpeg/libavcodec/wma_common.c \
                ffmpeg/libavcodec/wma_freqs.c ffmpeg/libavcodec/vlc.c ffmpeg/libavcodec/sinewin.c \
                ffmpeg/libavutil/tx.c ffmpeg/libavutil/tx_float.c ffmpeg/libavutil/float_dsp.c \
                ffmpeg/libavutil/float_scalarproduct.c ffmpeg/libavutil/reverse.c \
                shim/aac_scalefactor_tables.c shim/wmadec_shim.c shim/wmadec_glue.c)
WMADEC_OBJS = $(addprefix $(WMADEC_BUILD_DIR)/, $(subst /,__,$(WMADEC_SRCS:.c=.o)))
WMADEC_CFLAGS = -std=gnu11 -O2 -DNDEBUG -DHAVE_AV_CONFIG_H -w -MMD -MP \
                -I$(WMADEC_DIR)/include -I$(WMADEC_DIR)/shim -I$(WMADEC_DIR)/shim/libavcodec \
                -I$(WMADEC_DIR)/shim/libavutil -I$(WMADEC_DIR)/ffmpeg
SA_INCLUDES += -I$(WMADEC_DIR)/include
SA_LIBS += $(WMADEC_LIB)

.PHONY: third_party
third_party: $(SDL3_STAMP) $(WMADEC_LIB)
$(SDL3_STAMP): $(SDL3_SRC_DIR)/CMakeLists.txt
	$(CMAKE) -S $(SDL3_SRC_DIR) -B $(SDL3_BUILD_DIR) $(SDL3_CMAKE_OPTS)
	$(CMAKE) --build $(SDL3_BUILD_DIR) -j2 --target install
	@touch $@

$(WMADEC_BUILD_DIR):
	@mkdir -p $@
define WMADEC_OBJ_RULE
$$(WMADEC_BUILD_DIR)/$(subst /,__,$(1:.c=.o)): $(1) | $$(WMADEC_BUILD_DIR)
	$$(CC) $$(WMADEC_CFLAGS) -c $$< -o $$@
endef
$(foreach src,$(WMADEC_SRCS),$(eval $(call WMADEC_OBJ_RULE,$(src))))
$(WMADEC_LIB): $(WMADEC_OBJS)
	rm -f $@
	$(AR) rcs $@ $(WMADEC_OBJS)
-include $(WMADEC_OBJS:.o=.d)

# Standalone sources: game logic + engine funcs + runtime src + dx11 + standalone entry
SA_SRCS = $(GAME_SRC_SRCS) \
          $(RUNTIME_FUNC_SRCS) \
          $(RUNTIME_SRC_SRCS) \
          $(DX11_FUNC_SRCS) \
          $(DX11_SRC_SRCS) \
          $(wildcard src/app/*.cpp) $(wildcard src/diag/*.cpp)
# src/diag/win: the Windows-only diagnostics (crash / hang handlers, watchdog, sampler,
# hardware watchpoints, redzone allocator; D4_RULES C10). Globbed only on Windows.
ifeq ($(OS),Windows_NT)
SA_SRCS += $(wildcard src/diag/win/*.cpp)
endif

SA_OBJECTS = $(addprefix $(SA_OBJ_DIR)/, $(subst /,__,$(SA_SRCS:.cpp=.o)))
# Any standalone object may include <SDL3/...>: the installed headers must exist first.
$(SA_OBJECTS): | $(SDL3_STAMP)

# Windows resources (exe icon)
SA_RC = src/app/icon.rc
SA_RC_OBJ = $(SA_OBJ_DIR)/src__app__icon__rc.o
WINDRES = windres

.PHONY: all clean standalone standalone-debug standalone-og standalone-perf sa-deploy sa-run sa-run-menu sa-deploy-og sa-run-og sa-run-og-menu sa-stress sa-stress-debug

all: standalone-debug

# Strip all printf() debug logging in release builds (zero runtime cost).
# See src/app/log_strip.h for details. -include must use a forward-slash
# path so it resolves the same on MSYS2 bash and from VS Code task shells.
STRIP_LOGS_FLAGS = -DBANISHED_STRIP_LOGS -include src/app/log_strip.h

# PERF_EXPERIMENTAL_RELEASE=1 compiles the NON-RETAIL PerfX knobs default-on in the
# release exe too (default 0 = off; see src/app/perf_experimental.h).
PERF_EXPERIMENTAL_RELEASE ?= 0
# Gate the LOG-STRIPPED builds on the one rule log_strip.h states and that has
# been broken twice in two days: no qualified `std::printf`. The preprocessor
# cannot replace a qualified name, so it survives the macro and the compiler
# then blames log_strip.h instead of the offending file. tools/check_log_strip.py
# strips comments and string literals first -- a naive grep reports three PROSE
# hits on a clean tree and would send someone to "fix" documentation.
# Run its controls with: python tools/check_log_strip.py --self-test
PYTHON ?= python
.PHONY: check-log-strip
check-log-strip:
	@$(PYTHON) tools/check_log_strip.py

# ============================================================
# Host unit tests (ENDGAME B): the real classes standing on their own.
# tests/unit/README.md. `make unit` builds and runs build/unit/unit.exe.
# Object names flatten the path with "__" like SA_OBJECTS; the rule maps back.
# ============================================================
include tests/unit/sources.mk
UNIT_BUILD_DIR = build/unit
UNIT_SRCS = $(wildcard tests/unit/*.cpp) $(UNIT_TREE_SRCS)
UNIT_OBJECTS = $(addprefix $(UNIT_BUILD_DIR)/, $(subst /,__,$(UNIT_SRCS:.cpp=.o)))
UNIT_CXXFLAGS = -std=c++17 -fexceptions -fno-strict-aliasing -fno-lifetime-dse -Wall -Wextra -Wno-invalid-offsetof -g -O0 -D_DEBUG -DBANISHED_UNIT_TEST -MMD -MP
.PHONY: unit
unit: $(UNIT_BUILD_DIR)/unit.exe
	@$(UNIT_BUILD_DIR)/unit.exe
$(UNIT_BUILD_DIR)/unit.exe: $(UNIT_OBJECTS) $(WMADEC_LIB)
	$(CXX) $(UNIT_OBJECTS) $(WMADEC_LIB) -o $@ -static -static-libgcc -static-libstdc++
$(UNIT_BUILD_DIR):
	@mkdir -p $(UNIT_BUILD_DIR)
define UNIT_OBJ_RULE
$$(UNIT_BUILD_DIR)/$(subst /,__,$(1:.cpp=.o)): $(1) | $$(UNIT_BUILD_DIR)
	$$(CXX) $$(UNIT_CXXFLAGS) $$(SA_INCLUDES) -Itests/unit -c $$< -o $$@
endef
$(foreach src,$(UNIT_SRCS),$(eval $(call UNIT_OBJ_RULE,$(src))))
-include $(UNIT_OBJECTS:.o=.d)

# (check-cutover-manifest, the Wave 26 gate on standalone/cutover_manifest.inc,
# is deleted: ENDGAME B.3.9 close retired the manifest with its last rows;
# the mint itself went at B3R W0-SM M3.)

standalone: SA_CXXFLAGS += -O3 -DNDEBUG -DBANISHED_PERF_EXPERIMENTAL=$(PERF_EXPERIMENTAL_RELEASE) $(STRIP_LOGS_FLAGS)
standalone: check-log-strip $(SDL3_STAMP) $(WMADEC_LIB) $(SA_TARGET)

# WAVE 25 INTEGRATION: standalone-debug now runs check-log-strip too.
#
# It did not, and `standalone` / `standalone-perf` did. Since standalone-debug
# is the default development and integration path (it is what `sa-run` and the
# integrator build), a qualified `std::printf` could be written, built, tested
# and MERGED without anything complaining -- the guard only fired for the
# log-stripped target that nobody built until a perf A/B was needed.
#
# It broke the perf build for an ENTIRE WAVE twice running: Wave 24's gfxmat
# lane landed two, discovered by Wave 25 lane ALLOC when its A/B could not
# build; Wave 25's BODY lane then landed two more, discovered at integration
# minutes after the first pair was fixed. Both times every checker was green
# and standalone-debug was clean, so "build clean, checkers green" was NOT
# evidence the tree built.
#
# The guard is a fast source scan, so the cost here is negligible and it now
# fails in the lane that wrote the line rather than in the wave that needs it.
standalone-debug: SA_CXXFLAGS += -g -O0 -D_DEBUG
standalone-debug: check-log-strip $(SDL3_STAMP) $(WMADEC_LIB) $(SA_TARGET)

# Fast-debug build: -Og with EVERYTHING else identical to standalone-debug —
# logs kept (no STRIP_LOGS), _DEBUG on, CheckPtr left real. Own object dir so a
# debug<->og switch never reuses objects built with the other -O level.
#
# Why it exists: at -O0 an entity-heavy world (the tutorial save is 5,354
# entities) runs at ~1 fps, which is slow enough to change what you are testing
# — animation, pathing and any per-tick timing behave differently at that frame
# rate, and a play-through to a late tutorial step takes longer than the bug
# does to reproduce. -Og keeps the code debuggable (no inlining of the big
# ported functions, frame pointers intact, most locals still live) while giving
# back an order of magnitude of speed.
#
# Use standalone-debug only when you need EVERY local to survive for a
# breakpoint session; use this for interactive play-throughs and log reading.
# -fno-strict-aliasing is already unconditional in SA_CXXFLAGS and is what makes
# any -O level above 0 safe here (see the note on that variable).
SA_OG_DIR = build/og
standalone-og: $(SDL3_STAMP) $(WMADEC_LIB)
	$(MAKE) SA_BUILD_DIR=$(SA_OG_DIR) \
	        SA_CXXFLAGS="$(SA_CXXFLAGS) -Og -g -D_DEBUG" \
	        $(SA_OG_DIR)/reimpl-x64.exe

# Perf build: optimised WITH symbols, logs stripped, in its OWN object dir so a
# debug<->perf switch never reuses objects compiled with the other flags (the
# standalone/standalone-debug pair share build/ and rely on `make clean`).
# This is what the stress/perf baseline (make sa-stress) measures.
SA_PERF_DIR = build/perf
# PERF_EXTRA: extra compile+link flags for A/B builds (e.g. PERF_EXTRA="-O3 -flto").
# SA_CXXFLAGS is also on the link line, so -flto reaches the linker. make does not
# track flags: `rm -rf standalone/build-perf` between flag sets.
PERF_EXTRA ?=
# PERF_EXPERIMENTAL=1 (default for this target) compiles the NON-RETAIL PerfX knobs
# default-on (src/app/perf_experimental.h); =0 leaves them compiled but default-off.
# Runtime kill-switch either way: BANISHED_PERF_EXPERIMENTAL=0.
PERF_EXPERIMENTAL ?= 1
standalone-perf: check-log-strip $(SDL3_STAMP) $(WMADEC_LIB)
standalone-perf:
	$(MAKE) SA_BUILD_DIR=$(SA_PERF_DIR) \
	        SA_CXXFLAGS="$(SA_CXXFLAGS) -O2 -g -DNDEBUG -DBANISHED_PERF_EXPERIMENTAL=$(PERF_EXPERIMENTAL) $(STRIP_LOGS_FLAGS) $(PERF_EXTRA)" \
	        $(SA_PERF_DIR)/reimpl-x64.exe

# ============================================================
# Standalone EXE link + compile rules
# ============================================================
# The object list exceeds the Windows CreateProcess ~32KB command-line limit, so
# link via a GNU-make-generated response file (@file) instead of inline args.
$(SA_TARGET): $(SA_OBJECTS) $(SA_RC_OBJ) $(SDL3_STAMP) $(WMADEC_LIB) | $(SA_BUILD_DIR)
	$(file >$(SA_BUILD_DIR)/link_objects.rsp,$(SA_OBJECTS) $(SA_RC_OBJ))
	$(CXX) $(SA_CXXFLAGS) $(SA_LDFLAGS) -o $@ @$(SA_BUILD_DIR)/link_objects.rsp $(SA_LIBS)
	@echo "Built standalone: $@"
	@ls -lh $@

$(SA_BUILD_DIR):
	mkdir -p $(SA_BUILD_DIR)

$(SA_OBJ_DIR):
	mkdir -p $(SA_OBJ_DIR)

# Standalone compile rules: ONE pattern rule per source directory, generated from the
# source list itself (SA_SRC_DIRS). Object names flatten the path with "__" exactly as
# SA_OBJECTS spells them. A new directory anywhere under the source roots builds with no
# edit here (the old hand-written per-directory rules silently produced "no rule to make
# target" for a directory nobody had added).
SA_SRC_DIRS = $(sort $(patsubst %/,%,$(dir $(SA_SRCS))))
define SA_SRC_RULE
$$(SA_OBJ_DIR)/$(subst /,__,$(1))__%.o: $(1)/%.cpp | $$(SA_OBJ_DIR)
	$$(CXX) $$(SA_CXXFLAGS) $$(SA_INCLUDES) -c $$< -o $$@
endef
$(foreach d,$(SA_SRC_DIRS),$(eval $(call SA_SRC_RULE,$(d))))

# Exe icon resource (windres compiles .rc -> COFF object)
$(SA_RC_OBJ): $(SA_RC) src/app/ba_icon.ico | $(SA_OBJ_DIR)
	$(WINDRES) $(SA_RC) -o $@

# ============================================================
# Deploy / run
# ============================================================
sa-deploy: standalone-debug
	cp "$(SA_TARGET)" "$(BANISHED_DIR)/reimpl-x64.exe"
	rm -f "$(BANISHED_DIR)/standalone_debug.log"
	@echo "Deployed to $(BANISHED_DIR)/reimpl-x64.exe"

# 2026-09-13 (ENDGAME C.2): there are no live-feature env gates left. Every
# BANISHED_LIVE_* flag either graduated to unconditional code or was deleted
# with its feature. The full inventory of the flags the exe still honours --
# harness drivers, world setup, perf knobs -- is src/app/options.h.

sa-run: sa-deploy
	cd "$(BANISHED_DIR)" && BANISHED_AUTOSTART=1 start reimpl-x64.exe
	@echo "Launched standalone reimpl (autostart)"

# Same as sa-run but stops at the main menu (no auto-new-game). Use when you
# want to exercise the menu itself.
sa-run-menu: sa-deploy
	cd "$(BANISHED_DIR)" && start reimpl-x64.exe
	@echo "Launched standalone reimpl (menu only)"

# -Og variants of the three above. Same logs, same env vars, ~10x the frame rate.
sa-deploy-og: standalone-og
	cp "$(SA_OG_DIR)/reimpl-x64.exe" "$(BANISHED_DIR)/reimpl-x64.exe"
	rm -f "$(BANISHED_DIR)/standalone_debug.log"
	@echo "Deployed -Og build to $(BANISHED_DIR)/reimpl-x64.exe"

sa-run-og: sa-deploy-og
	cd "$(BANISHED_DIR)" && BANISHED_AUTOSTART=1 start reimpl-x64.exe
	@echo "Launched -Og standalone reimpl (autostart)"

sa-run-og-menu: sa-deploy-og
	cd "$(BANISHED_DIR)" && start reimpl-x64.exe
	@echo "Launched -Og standalone reimpl (menu only)"

# Stress test / performance baseline (src/diag/stress_harness.cpp, perf/README.md).
# Builds the perf exe, deploys it, runs the BANISHED_STRESS town to completion, and
# writes the metrics CSV + a summary under perf/results/. Knobs are passed through:
#   make sa-stress STRESS_ARGS="--label mychange --speed 5 --citizens 600"
# `make sa-stress-debug` does the same with the -O0 debug exe (functional check only —
# never record a baseline from it).
sa-stress: standalone-perf
	cp "$(SA_PERF_DIR)/reimpl-x64.exe" "$(BANISHED_DIR)/reimpl-x64.exe"
	tools/stress_run.sh --no-build $(STRESS_ARGS)

sa-stress-debug: sa-deploy
	tools/stress_run.sh --no-build --debug $(STRESS_ARGS)

# Include auto-generated header dependency files
-include $(SA_OBJECTS:.o=.d)

clean:
	rm -rf $(SA_BUILD_DIR) $(SA_OG_DIR) $(SA_PERF_DIR)

# `cutover` is now an alias for the ordinary route: since ENDGAME A.1 the exe
# never loads Runtime-x64.dll, so there is nothing left to cut over to. Kept
# under this name only because people already know it; see `sa-run`.
.PHONY: cutover
cutover: sa-run
