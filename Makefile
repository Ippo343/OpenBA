# Banished Reimplementation - Build System (standalone replacement exe)
# Usage:
#   make                  - build debug standalone exe
#   make standalone-debug - build debug standalone exe
#   make standalone       - build release standalone exe
#   make sa-deploy        - build debug + copy to Banished directory
#   make sa-run           - build + copy + launch (auto-starts a new game)
#   make sa-run-menu      - build + copy + launch, stop at main menu
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
RUNTIME_FUNC_SRCS = runtime/funcs/runtime_funcs.cpp
RUNTIME_SRC_SRCS = $(wildcard runtime/src/*/*.cpp)

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
SA_CXXFLAGS = -std=c++17 -fno-strict-aliasing -Wall -Wextra -Wno-cast-function-type -MMD -MP
SA_LDFLAGS = -static-libgcc -static-libstdc++ -mwindows
SA_LIBS = -luser32 -ldbghelp
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

SA_OBJECTS = $(addprefix $(SA_OBJ_DIR)/, $(subst /,__,$(SA_SRCS:.cpp=.o)))

# Windows resources (exe icon)
SA_RC = standalone/icon.rc
SA_RC_OBJ = $(SA_OBJ_DIR)/standalone__icon__rc.o
WINDRES = windres

.PHONY: all clean standalone standalone-debug standalone-perf sa-deploy sa-run sa-run-menu sa-stress sa-stress-debug

all: standalone-debug

# Strip all printf() debug logging in release builds (zero runtime cost).
# See standalone/log_strip.h for details. -include must use a forward-slash
# path so it resolves the same on MSYS2 bash and from VS Code task shells.
STRIP_LOGS_FLAGS = -DBANISHED_STRIP_LOGS -include standalone/log_strip.h

standalone: SA_CXXFLAGS += -O3 -DNDEBUG $(STRIP_LOGS_FLAGS)
standalone: $(SA_TARGET)

standalone-debug: SA_CXXFLAGS += -g -O0 -D_DEBUG
standalone-debug: $(SA_TARGET)

# Perf build: optimised WITH symbols, logs stripped, in its OWN object dir so a
# debug<->perf switch never reuses objects compiled with the other flags (the
# standalone/standalone-debug pair share build/ and rely on `make clean`).
# This is what the stress/perf baseline (make sa-stress) measures.
SA_PERF_DIR = standalone/build-perf
standalone-perf:
	$(MAKE) SA_BUILD_DIR=$(SA_PERF_DIR) \
	        SA_CXXFLAGS="$(SA_CXXFLAGS) -O2 -g -DNDEBUG $(STRIP_LOGS_FLAGS)" \
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

$(SA_OBJ_DIR)/runtime__src__ui__%.o: runtime/src/ui/%.cpp | $(SA_OBJ_DIR)
	$(CXX) $(SA_CXXFLAGS) $(SA_INCLUDES) -c $< -o $@

$(SA_OBJ_DIR)/runtime__src__system__%.o: runtime/src/system/%.cpp | $(SA_OBJ_DIR)
	$(CXX) $(SA_CXXFLAGS) $(SA_INCLUDES) -c $< -o $@

$(SA_OBJ_DIR)/runtime__src__math__%.o: runtime/src/math/%.cpp | $(SA_OBJ_DIR)
	$(CXX) $(SA_CXXFLAGS) $(SA_INCLUDES) -c $< -o $@

$(SA_OBJ_DIR)/runtime__src__graphics__%.o: runtime/src/graphics/%.cpp | $(SA_OBJ_DIR)
	$(CXX) $(SA_CXXFLAGS) $(SA_INCLUDES) -c $< -o $@

$(SA_OBJ_DIR)/runtime__src__io__%.o: runtime/src/io/%.cpp | $(SA_OBJ_DIR)
	$(CXX) $(SA_CXXFLAGS) $(SA_INCLUDES) -c $< -o $@

$(SA_OBJ_DIR)/dx11__funcs__%.o: dx11/funcs/%.cpp | $(SA_OBJ_DIR)
	$(CXX) $(SA_CXXFLAGS) $(SA_INCLUDES) -c $< -o $@

$(SA_OBJ_DIR)/dx11__src__%.o: dx11/src/%.cpp | $(SA_OBJ_DIR)
	$(CXX) $(SA_CXXFLAGS) $(SA_INCLUDES) -c $< -o $@

$(SA_OBJ_DIR)/standalone__%.o: standalone/%.cpp | $(SA_OBJ_DIR)
	$(CXX) $(SA_CXXFLAGS) $(SA_INCLUDES) -c $< -o $@

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

# Include auto-generated header dependency files
-include $(SA_OBJECTS:.o=.d)

clean:
	rm -rf $(SA_BUILD_DIR) $(SA_PERF_DIR)
