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
SA_CXXFLAGS = -std=c++17 -fno-strict-aliasing -Wall -Wextra -Wno-cast-function-type -MMD -MP
SA_LDFLAGS = -static-libgcc -static-libstdc++ -mwindows
# -lshell32: runtime/src/io/filesystem_win.cpp (Wave 4 lane D) calls SHGetFolderPathW
# for FileSystem::GetSaveDirectory. Behind the _win seam per PLAN.md §4.1 decision 1.
# -lole32: Wave 9 lane ENG's Engine::Startup port calls CoInitializeEx /
# CoUninitialize, as retail's does. A lane verifying with -fsyntax-only
# cannot see a missing import library at all -- this failed only at link.
SA_LIBS = -luser32 -ldbghelp -lshell32 -lole32 -ldinput8 -ldxguid
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

.PHONY: all clean standalone standalone-debug standalone-og standalone-perf sa-deploy sa-run sa-run-menu sa-deploy-og sa-run-og sa-run-og-menu sa-stress sa-stress-debug

all: standalone-debug

# Strip all printf() debug logging in release builds (zero runtime cost).
# See standalone/log_strip.h for details. -include must use a forward-slash
# path so it resolves the same on MSYS2 bash and from VS Code task shells.
STRIP_LOGS_FLAGS = -DBANISHED_STRIP_LOGS -include standalone/log_strip.h

# PERF_EXPERIMENTAL_RELEASE=1 compiles the NON-RETAIL PerfX knobs default-on in the
# release exe too (default 0 = off; see standalone/perf_experimental.h).
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
# tests/unit/README.md. `make unit` builds and runs tests/unit/build/unit.exe.
# Object names flatten the path with "__" like SA_OBJECTS; the rule maps back.
# ============================================================
include tests/unit/sources.mk
UNIT_BUILD_DIR = tests/unit/build
UNIT_SRCS = $(wildcard tests/unit/*.cpp) $(UNIT_TREE_SRCS)
UNIT_OBJECTS = $(addprefix $(UNIT_BUILD_DIR)/, $(subst /,__,$(UNIT_SRCS:.cpp=.o)))
UNIT_CXXFLAGS = -std=c++17 -fno-strict-aliasing -Wall -Wextra -g -O0 -D_DEBUG -DBANISHED_UNIT_TEST -MMD -MP
.PHONY: unit
unit: $(UNIT_BUILD_DIR)/unit.exe
	@$(UNIT_BUILD_DIR)/unit.exe
$(UNIT_BUILD_DIR)/unit.exe: $(UNIT_OBJECTS)
	$(CXX) $(UNIT_OBJECTS) -o $@ -static-libgcc -static-libstdc++
$(UNIT_BUILD_DIR):
	@mkdir -p $(UNIT_BUILD_DIR)
define UNIT_OBJ_RULE
$$(UNIT_BUILD_DIR)/$(subst /,__,$(1:.cpp=.o)): $(1) | $$(UNIT_BUILD_DIR)
	$$(CXX) $$(UNIT_CXXFLAGS) $$(SA_INCLUDES) -Itests/unit -c $$< -o $$@
endef
$(foreach src,$(UNIT_SRCS),$(eval $(call UNIT_OBJ_RULE,$(src))))
-include $(UNIT_OBJECTS:.o=.d)

# WAVE 26 LANE ATTEND: gate the build on standalone/cutover_manifest.inc, which
# is a GENERATED file that nothing regenerated.
#
# It shipped FOURTEEN REQUIRED ROWS SHORT -- every one of Wave 25 lane DATA's --
# through cutover attempt 26 and into Wave 26. `tools/cutover_manifest.py
# --check` existed the whole time, answers in a second, and says STALE; the
# integrator queue even carried a written instruction to re-derive rather than
# trust the numbers. None of that fired, because a detector only helps someone
# who runs it, and the staleness was MASKED: the blanket mint switch of the day
# published the 14 missing rows anyway, so cutover attempt 26 booted deep
# and `published 44, withheld 1` READ AS CONFIRMATION. The number matched its
# prediction BECAUSE it was stale -- 44 was the pre-DATA count.
#
# So the fix is not another detector, it is wiring the existing one to something
# that runs by itself. `--gate` (not `--check`) is used deliberately: `--check`
# is content-exact and part of that content is a tree-wide by-name DOOR COUNT,
# so any lane adding one ResolveExport() makes it "stale" (measured: merging
# lane FACTORY moved two counts by one, with every row and disposition
# identical). A gate that fails a six-lane wave over a cosmetic count is a gate
# that gets commented out. `--gate` fails on the ROW SET, the DISPOSITIONS,
# an unminted REQUIRED row and an undeclared scope -- the fields the process
# acts on -- and only WARNs on a moved count.
#
# Its three positive controls, and they must be re-run if it is touched
# (a screen looking for the ABSENCE of a shape must be handed one):
#   delete a REQUIRED row from the .inc  -> GATE FAIL  (this is the Wave 25 defect)
#   flip one row's disposition           -> GATE FAIL
#   change only a door count             -> GATE WARN, exit 0
.PHONY: check-cutover-manifest
check-cutover-manifest:
	@$(PYTHON) tools/cutover_manifest.py --gate

standalone: SA_CXXFLAGS += -O3 -DNDEBUG -DBANISHED_PERF_EXPERIMENTAL=$(PERF_EXPERIMENTAL_RELEASE) $(STRIP_LOGS_FLAGS)
standalone: check-log-strip check-cutover-manifest $(SA_TARGET)

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
standalone-debug: check-log-strip check-cutover-manifest $(SA_TARGET)

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
standalone-perf: check-log-strip check-cutover-manifest
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

# 2026-09-13 (ENDGAME C.2): there are no live-feature env gates left. Every
# BANISHED_LIVE_* flag either graduated to unconditional code or was deleted
# with its feature. The full inventory of the flags the exe still honours --
# harness drivers, world setup, perf knobs -- is standalone/options.h.

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
	rm -rf $(SA_BUILD_DIR) $(SA_OG_DIR) $(SA_PERF_DIR)

# `cutover` is now an alias for the ordinary route: since ENDGAME A.1 the exe
# never loads Runtime-x64.dll, so there is nothing left to cut over to. Kept
# under this name only because people already know it; see `sa-run`.
.PHONY: cutover
cutover: sa-run
