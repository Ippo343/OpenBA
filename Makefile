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

# ============================================================
# Standalone EXE build
# ============================================================
SA_BUILD_DIR = standalone/build
SA_OBJ_DIR = $(SA_BUILD_DIR)/obj
SA_TARGET = $(SA_BUILD_DIR)/reimpl-x64.exe

SA_CXXFLAGS = -std=c++17 -Wall -Wextra -Wno-cast-function-type -MMD -MP
SA_LDFLAGS = -static-libgcc -static-libstdc++ -mwindows
SA_LIBS = -luser32 -ldbghelp
SA_INCLUDES = -I. -Iengine/types -Iengine/funcs -Icode/Game \
              -Iruntime/types -Iruntime/funcs -Istandalone

# Standalone sources: game logic + engine funcs + runtime src + standalone entry
SA_SRCS = $(GAME_SRC_SRCS) \
          $(ENGINE_FUNC_SRCS) \
          $(RUNTIME_FUNC_SRCS) \
          $(RUNTIME_SRC_SRCS) \
          $(wildcard standalone/*.cpp)

SA_OBJECTS = $(addprefix $(SA_OBJ_DIR)/, $(subst /,__,$(SA_SRCS:.cpp=.o)))

# Windows resources (exe icon)
SA_RC = standalone/icon.rc
SA_RC_OBJ = $(SA_OBJ_DIR)/standalone__icon__rc.o
WINDRES = windres

.PHONY: all clean standalone standalone-debug sa-deploy sa-run sa-run-menu

all: standalone-debug

# Strip all printf() debug logging in release builds (zero runtime cost).
# See standalone/log_strip.h for details. -include must use a forward-slash
# path so it resolves the same on MSYS2 bash and from VS Code task shells.
STRIP_LOGS_FLAGS = -DBANISHED_STRIP_LOGS -include standalone/log_strip.h

standalone: SA_CXXFLAGS += -O3 -DNDEBUG $(STRIP_LOGS_FLAGS)
standalone: $(SA_TARGET)

standalone-debug: SA_CXXFLAGS += -g -O0 -D_DEBUG
standalone-debug: $(SA_TARGET)

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

# Live-feature defaults for the standalone test driver. These feature groups are
# user-verified working and enabled by default on every sa-run so the game
# launches with the GUI, citizen status-icon sprites, the full live citizen-AI
# stack, and housed founding families without having to set envs by hand. To A/B
# against a bare run, override on the command line (e.g.
# `BANISHED_LIVE_CITIZENAI=0 mingw32-make sa-run`) or launch reimpl-x64.exe
# directly — the code gates keep their byte-faithful default-off semantics; this
# only sets them at launch.
#
# Economy Phase 2 (housing): BANISHED_LIVE_RESIDENCE graduates the per-type
# ResidenceComponent factory + runs the WoodHouse OnCreate pass (captures the
# founding houses); BANISHED_LIVE_HOUSING then drives the faithful AssignHousing
# primitive so each founding family is rostered into a WoodHouse (citizen+0x248
# residence ref + house WorkPlaceComponent occupant roster +0x60 + occupied
# mesh visual). Verified live: 6 houses, ~21-25 members housed, crash-free.
SA_LIVE_ENV = \
	BANISHED_LIVE_TOOLBAR=1 BANISHED_GAME_PRELOAD=1 \
	BANISHED_LIVE_STATUSICON=1 BANISHED_LIVE_STATUSICON_DRAW=1 BANISHED_LIVE_CITIZEN_STATUSICON=1 \
	BANISHED_LIVE_CITIZEN=1 BANISHED_LIVE_CITIZEN_UPDATE=1 BANISHED_LIVE_CITIZENAI=1 \
	BANISHED_LIVE_CHARACTER=1 BANISHED_LIVE_CHARACTER_MOVETICK=1 BANISHED_LIVE_STATEMACHINE=1 \
	BANISHED_LIVE_RESIDENCE=1 BANISHED_LIVE_HOUSING=1 \
	BANISHED_LIVE_STORAGE=1 BANISHED_LIVE_REGIONMAP=1 \
	BANISHED_LIVE_STORAGEDISPLAY=1 BANISHED_LIVE_DECAL=1 \
	BANISHED_LIVE_TOOLS=1 BANISHED_LIVE_PICKING=1 \
	BANISHED_LIVE_PARTICLE=1 BANISHED_SELECTION_OUTLINE=1 \
	BANISHED_LIVE_EVENTS=1

sa-run: sa-deploy
	cd "$(BANISHED_DIR)" && BANISHED_AUTOSTART=1 $(SA_LIVE_ENV) start reimpl-x64.exe
	@echo "Launched standalone reimpl (autostart + GUI/sprites/citizen-AI live)"

# Same as sa-run but stops at the main menu (no auto-new-game). Use when you
# want to exercise the menu itself. Still enables the live-feature defaults.
sa-run-menu: sa-deploy
	cd "$(BANISHED_DIR)" && $(SA_LIVE_ENV) start reimpl-x64.exe
	@echo "Launched standalone reimpl (menu only; GUI/sprites/citizen-AI live)"

# Include auto-generated header dependency files
-include $(SA_OBJECTS:.o=.d)

clean:
	rm -rf $(SA_BUILD_DIR)
