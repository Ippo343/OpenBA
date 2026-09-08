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

# Enable parallel builds by default (override with make -j1 for sequential)
MAKEFLAGS += -j$(shell nproc 2>/dev/null || echo 4)

CXX = ccache g++
CC = ccache gcc

BANISHED_DIR = C:/GOG Games/Banished

# Auto-discover clean game logic files (mirrors original source tree)
GAME_SRC_SRCS = $(wildcard code/Game/*/*.cpp)

ENGINE_FUNC_SRCS = engine/funcs/engine_funcs.cpp

# Runtime-x64.dll reimplementation sources
RUNTIME_FUNC_SRCS = $(wildcard runtime/funcs/*.cpp)
RUNTIME_SRC_SRCS = $(wildcard runtime/src/*/*.cpp)
# Directory names under runtime/src that hold sources (io, ui, math, ...). Used
# to generate one compile rule each — see RUNTIME_SRC_RULE below.
RUNTIME_SRC_DIRS = $(sort $(notdir $(patsubst %/,%,$(dir $(RUNTIME_SRC_SRCS)))))

define RUNTIME_SRC_RULE
$$(SA_OBJ_DIR)/runtime__src__$(1)__%.o: runtime/src/$(1)/%.cpp | $$(SA_OBJ_DIR)
	$$(CXX) $$(SA_CXXFLAGS) $$(SA_INCLUDES) -c $$< -o $$@
endef

# VideoDX11-x64.dll reimplementation sources (see dx11/README.md)
DX11_FUNC_SRCS = $(wildcard dx11/funcs/*.cpp)
DX11_SRC_SRCS = $(wildcard dx11/src/*.cpp)

# ============================================================
# Standalone EXE build
# ============================================================
SA_BUILD_DIR = standalone/build
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
SA_CXXFLAGS = -std=c++17 -fno-strict-aliasing -Wall -Wextra -Wno-cast-function-type -MMD -MP -include standalone/env_cache.h
SA_LDFLAGS = -static-libgcc -static-libstdc++ -mwindows
# -lshell32: runtime/src/io/filesystem_win.cpp (Wave 4 lane D) calls SHGetFolderPathW
# for FileSystem::GetSaveDirectory. Behind the _win seam per PLAN.md §4.1 decision 1.
SA_LIBS = -luser32 -ldbghelp -lshell32
SA_INCLUDES = -I. -Iengine/types -Iengine/funcs -Icode/Game \
              -Iruntime/types -Iruntime/funcs \
              -Idx11/types -Idx11/funcs -Istandalone

# Standalone sources: game logic + engine funcs + runtime src + dx11 + standalone entry
SA_SRCS = $(GAME_SRC_SRCS) \
          $(ENGINE_FUNC_SRCS) \
          $(RUNTIME_FUNC_SRCS) \
          $(RUNTIME_SRC_SRCS) \
          $(DX11_FUNC_SRCS) \
          $(DX11_SRC_SRCS) \
          $(wildcard standalone/*.cpp)

# hde64: the x86-64 length disassembler used by standalone/runtime_detour.cpp to
# relocate a displaced prologue. Upstream C, kept unmodified — compiled with $(CC).
SA_C_SRCS = standalone/hde/hde64.c

SA_OBJECTS = $(addprefix $(SA_OBJ_DIR)/, $(subst /,__,$(SA_SRCS:.cpp=.o)))              $(addprefix $(SA_OBJ_DIR)/, $(subst /,__,$(SA_C_SRCS:.c=.o)))

# Windows resources (exe icon)
SA_RC = standalone/icon.rc
SA_RC_OBJ = $(SA_OBJ_DIR)/standalone__icon__rc.o
WINDRES = windres

.PHONY: all clean standalone standalone-debug standalone-og standalone-perf sa-deploy sa-run sa-run-menu sa-deploy-og sa-run-og sa-run-og-menu sa-stress sa-stress-debug

all: standalone-debug

# Strip all printf() debug logging in release builds (zero runtime cost).
# See standalone/log_strip.h for details. -include must use a forward-slash
# path so it resolves the same on MSYS2 bash and from VS Code task shells.
STRIP_LOGS_FLAGS = -DBANISHED_STRIP_LOGS -include standalone/log_strip.h

# PERF_EXPERIMENTAL_RELEASE=1 compiles the NON-RETAIL PerfX knobs default-on in the
# release exe too (default 0 = off; see standalone/perf_experimental.h).
PERF_EXPERIMENTAL_RELEASE ?= 0
standalone: SA_CXXFLAGS += -O3 -DNDEBUG -DBANISHED_PERF_EXPERIMENTAL=$(PERF_EXPERIMENTAL_RELEASE) $(STRIP_LOGS_FLAGS)
standalone: $(SA_TARGET)

standalone-debug: SA_CXXFLAGS += -g -O0 -D_DEBUG
standalone-debug: $(SA_TARGET)

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
SA_OG_DIR = standalone/build-og
standalone-og:
	$(MAKE) SA_BUILD_DIR=$(SA_OG_DIR) \
	        SA_CXXFLAGS="$(SA_CXXFLAGS) -Og -g -D_DEBUG" \
	        $(SA_OG_DIR)/reimpl-x64.exe

# Perf build: optimised WITH symbols, logs stripped, in its OWN object dir so a
# debug<->perf switch never reuses objects compiled with the other flags (the
# standalone/standalone-debug pair share build/ and rely on `make clean`).
# This is what the stress/perf baseline (make sa-stress) measures.
SA_PERF_DIR = standalone/build-perf
# PERF_EXTRA: extra compile+link flags for A/B builds (e.g. PERF_EXTRA="-O3 -flto").
# SA_CXXFLAGS is also on the link line, so -flto reaches the linker. make does not
# track flags: `rm -rf standalone/build-perf` between flag sets.
PERF_EXTRA ?=
# PERF_EXPERIMENTAL=1 (default for this target) compiles the NON-RETAIL PerfX knobs
# default-on (standalone/perf_experimental.h); =0 leaves them compiled but default-off.
# Runtime kill-switch either way: BANISHED_PERF_EXPERIMENTAL=0.
PERF_EXPERIMENTAL ?= 1
standalone-perf:
	$(MAKE) SA_BUILD_DIR=$(SA_PERF_DIR) \
	        SA_CXXFLAGS="$(SA_CXXFLAGS) -O2 -g -DNDEBUG -DBANISHED_CHECKPTR_STATIC_NOP -DBANISHED_PERF_EXPERIMENTAL=$(PERF_EXPERIMENTAL) $(STRIP_LOGS_FLAGS) $(PERF_EXTRA)" \
	        $(SA_PERF_DIR)/reimpl-x64.exe

# ============================================================
# Standalone EXE link + compile rules
# ============================================================
# The object list exceeds the Windows CreateProcess ~32KB command-line limit, so
# link via a GNU-make-generated response file (@file) instead of inline args.
$(SA_TARGET): $(SA_OBJECTS) $(SA_RC_OBJ) | $(SA_BUILD_DIR)
	$(file >$(SA_BUILD_DIR)/link_objects.rsp,$(SA_OBJECTS) $(SA_RC_OBJ))
	$(CXX) $(SA_CXXFLAGS) $(SA_LDFLAGS) -o $@ @$(SA_BUILD_DIR)/link_objects.rsp $(SA_LIBS)
	@echo "Built standalone: $@"
	@ls -lh $@

$(SA_BUILD_DIR):
	mkdir -p $(SA_BUILD_DIR)

$(SA_OBJ_DIR):
	mkdir -p $(SA_OBJ_DIR)

# Standalone compile rules — one per source directory
$(SA_OBJ_DIR)/code__Game__Action__%.o: code/Game/Action/%.cpp | $(SA_OBJ_DIR)
	$(CXX) $(SA_CXXFLAGS) $(SA_INCLUDES) -c $< -o $@

$(SA_OBJ_DIR)/code__Game__Component__%.o: code/Game/Component/%.cpp | $(SA_OBJ_DIR)
	$(CXX) $(SA_CXXFLAGS) $(SA_INCLUDES) -c $< -o $@

$(SA_OBJ_DIR)/code__Game__Dialog__%.o: code/Game/Dialog/%.cpp | $(SA_OBJ_DIR)
	$(CXX) $(SA_CXXFLAGS) $(SA_INCLUDES) -c $< -o $@

$(SA_OBJ_DIR)/code__Game__Entity__%.o: code/Game/Entity/%.cpp | $(SA_OBJ_DIR)
	$(CXX) $(SA_CXXFLAGS) $(SA_INCLUDES) -c $< -o $@

$(SA_OBJ_DIR)/code__Game__Game__%.o: code/Game/Game/%.cpp | $(SA_OBJ_DIR)
	$(CXX) $(SA_CXXFLAGS) $(SA_INCLUDES) -c $< -o $@

$(SA_OBJ_DIR)/code__Game__Goal__%.o: code/Game/Goal/%.cpp | $(SA_OBJ_DIR)
	$(CXX) $(SA_CXXFLAGS) $(SA_INCLUDES) -c $< -o $@

$(SA_OBJ_DIR)/code__Game__Object__%.o: code/Game/Object/%.cpp | $(SA_OBJ_DIR)
	$(CXX) $(SA_CXXFLAGS) $(SA_INCLUDES) -c $< -o $@

$(SA_OBJ_DIR)/code__Game__Player__%.o: code/Game/Player/%.cpp | $(SA_OBJ_DIR)
	$(CXX) $(SA_CXXFLAGS) $(SA_INCLUDES) -c $< -o $@

$(SA_OBJ_DIR)/code__Game__Tutorial__%.o: code/Game/Tutorial/%.cpp | $(SA_OBJ_DIR)
	$(CXX) $(SA_CXXFLAGS) $(SA_INCLUDES) -c $< -o $@

$(SA_OBJ_DIR)/code__Game__Work__%.o: code/Game/Work/%.cpp | $(SA_OBJ_DIR)
	$(CXX) $(SA_CXXFLAGS) $(SA_INCLUDES) -c $< -o $@

$(SA_OBJ_DIR)/engine__funcs__%.o: engine/funcs/%.cpp | $(SA_OBJ_DIR)
	$(CXX) $(SA_CXXFLAGS) $(SA_INCLUDES) -c $< -o $@

$(SA_OBJ_DIR)/runtime__funcs__%.o: runtime/funcs/%.cpp | $(SA_OBJ_DIR)
	$(CXX) $(SA_CXXFLAGS) $(SA_INCLUDES) -c $< -o $@

# One compile rule per runtime/src subdirectory, generated from RUNTIME_SRC_DIRS
# (itself derived from what is on disk). The Runtime port adds directories as it
# works down the wave order; without this a new directory silently produced "no
# rule to make target" instead of building.
$(foreach d,$(RUNTIME_SRC_DIRS),$(eval $(call RUNTIME_SRC_RULE,$(d))))

$(SA_OBJ_DIR)/dx11__funcs__%.o: dx11/funcs/%.cpp | $(SA_OBJ_DIR)
	$(CXX) $(SA_CXXFLAGS) $(SA_INCLUDES) -c $< -o $@

$(SA_OBJ_DIR)/dx11__src__%.o: dx11/src/%.cpp | $(SA_OBJ_DIR)
	$(CXX) $(SA_CXXFLAGS) $(SA_INCLUDES) -c $< -o $@

$(SA_OBJ_DIR)/standalone__%.o: standalone/%.cpp | $(SA_OBJ_DIR)
	$(CXX) $(SA_CXXFLAGS) $(SA_INCLUDES) -c $< -o $@

$(SA_OBJ_DIR)/standalone__hde__%.o: standalone/hde/%.c | $(SA_OBJ_DIR)
	$(CC) -std=c99 -O2 -Wall -MMD -MP -c $< -o $@

# Exe icon resource (windres compiles .rc -> COFF object)
$(SA_RC_OBJ): $(SA_RC) ba_icon.ico | $(SA_OBJ_DIR)
	$(WINDRES) $(SA_RC) -o $@

# ============================================================
# Deploy / run
# ============================================================
sa-deploy: standalone-debug
	cp "$(SA_TARGET)" "$(BANISHED_DIR)/reimpl-x64.exe"
	rm -f "$(BANISHED_DIR)/standalone_debug.log"
	@echo "Deployed to $(BANISHED_DIR)/reimpl-x64.exe"

# 2026-07-19: the former SA_LIVE_ENV flag block is GONE — every verified live
# feature is now DEFAULT-ON in code (kDefaultOnFlags in
# code/Game/Component/component_factory_base.cpp). Env vars remain as
# kill-switches only: `BANISHED_LIVE_CITIZENAI=0 mingw32-make sa-run` disables
# one feature for an A/B run. New features start opt-in (NOT in the table) and
# get promoted into kDefaultOnFlags once verified.

sa-run: sa-deploy
	cd "$(BANISHED_DIR)" && BANISHED_AUTOSTART=1 start reimpl-x64.exe
	@echo "Launched standalone reimpl (autostart; live features default-on)"

# Same as sa-run but stops at the main menu (no auto-new-game). Use when you
# want to exercise the menu itself.
sa-run-menu: sa-deploy
	cd "$(BANISHED_DIR)" && start reimpl-x64.exe
	@echo "Launched standalone reimpl (menu only; live features default-on)"

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

# Stress test / performance baseline (standalone/stress_harness.cpp, perf/README.md).
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

# ------------------------------------------------------------------
# libm bit-exactness sweep (Wave 9 lane L)
# ------------------------------------------------------------------
# runtime/src/crt/msvc_libm.cpp is a port of Runtime-x64.dll's own statically
# linked MSVC CRT sqrtf/sinf/cosf/atan2f/powf. `make libm-sweep` rebuilds the
# oracle and compares the port against those five bodies at their retail
# addresses on raw bit patterns, with a branch-coverage effectiveness control
# and a negative control. It needs the DLL present; it exits non-zero on any
# divergence AND on a negative control that finds nothing (which would mean the
# sweep is comparing the DLL against itself).
#   --quick skips the exhaustive sin/cos band (seconds instead of minutes).
LIBM_SWEEP_FLAGS = -std=c++17 -O2 -DMSVC_LIBM_COVERAGE -Iruntime/src/crt
.PHONY: libm-sweep
libm-sweep:
	$(CXX) $(LIBM_SWEEP_FLAGS) tools/libm_probe/sweep.cpp \
	    runtime/src/crt/msvc_libm.cpp -o tools/libm_probe/sweep.exe
	tools/libm_probe/sweep.exe $(LIBM_SWEEP_ARGS)

# Re-extract the coefficient tables from the shipped DLL. Only needed if the
# table header is ever suspected of drift; the header is checked in.
.PHONY: libm-tables
libm-tables:
	tools/ghidra_mcp/.venv/Scripts/python.exe tools/extract_crt_tables.py \
	    > runtime/src/crt/msvc_libm_tables.h

# Include auto-generated header dependency files
-include $(SA_OBJECTS:.o=.d)

clean:
	rm -rf $(SA_BUILD_DIR) $(SA_OG_DIR) $(SA_PERF_DIR)
