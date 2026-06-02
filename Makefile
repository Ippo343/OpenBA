# Banished Reimplementation - Build System
# Usage:
#   make            - build debug DLL
#   make release    - build release DLL
#   make deploy     - build + copy to Banished directory
#   make run        - build + copy + launch game
#   make undeploy   - remove from Banished directory
#   make clean      - remove build artifacts

# Enable parallel builds by default (override with make -j1 for sequential)
MAKEFLAGS += -j$(shell nproc 2>/dev/null || echo 4)

CXX = ccache g++
CC = ccache gcc
CXXFLAGS = -std=c++17 -Wall -Wextra -Wno-cast-function-type -MMD -MP
CFLAGS = -Wall -Wextra -MMD -MP
LDFLAGS = -shared -static-libgcc -static-libstdc++
LIBS = -luser32 -ldbghelp

# Include paths: project root + all source directories
INCLUDES = -I. -Ipatches/src -Iengine/types -Iengine/funcs -Icode/Game \
           -Iruntime/types -Iruntime/funcs

# Directories
BUILD_DIR = patches/build
OBJ_DIR = $(BUILD_DIR)/obj
BANISHED_DIR = C:/GOG Games/Banished

# Source files
PATCH_SRCS = \
    patches/src/dllmain.cpp \
    patches/src/proxy.cpp \
    patches/src/hooks.cpp \
    patches/src/resolve.cpp

# HDE64 length disassembler (C source from MinHook, used by hook relocation)
HDE_SRCS = patches/src/hde/hde64.c

ENGINE_FUNC_SRCS = \
    engine/funcs/engine_funcs.cpp

# Auto-discover reimplementation files (hook wrappers)
ENGINE_REIMPL_SRCS = $(wildcard engine/reimpl/*.cpp)

# Auto-discover clean game logic files (mirrors original source tree)
GAME_SRC_SRCS = $(wildcard code/Game/*/*.cpp)

# Runtime-x64.dll reimplementation sources
RUNTIME_FUNC_SRCS = runtime/funcs/runtime_funcs.cpp
RUNTIME_REIMPL_SRCS = $(wildcard runtime/reimpl/*.cpp)
RUNTIME_SRC_SRCS = $(wildcard runtime/src/*/*.cpp)

ALL_SRCS = $(PATCH_SRCS) $(ENGINE_FUNC_SRCS) $(ENGINE_REIMPL_SRCS) $(GAME_SRC_SRCS) \
           $(RUNTIME_FUNC_SRCS) $(RUNTIME_REIMPL_SRCS) $(RUNTIME_SRC_SRCS)

# Object files: flatten paths into OBJ_DIR to avoid collisions
#   patches/src/dllmain.cpp -> patches/build/obj/patches__src__dllmain.o
OBJECTS = $(addprefix $(OBJ_DIR)/, $(subst /,__,$(ALL_SRCS:.cpp=.o)))

# HDE64 C objects (separate from C++ objects)
HDE_OBJECTS = $(addprefix $(OBJ_DIR)/, $(subst /,__,$(HDE_SRCS:.c=.o)))

DEF_FILE = patches/src/proxy.def
TARGET = $(BUILD_DIR)/version.dll

# ============================================================
# Standalone EXE build
# ============================================================
SA_BUILD_DIR = standalone/build
SA_OBJ_DIR = $(SA_BUILD_DIR)/obj
SA_TARGET = $(SA_BUILD_DIR)/reimpl-x64.exe

SA_CXXFLAGS = -std=c++17 -Wall -Wextra -Wno-cast-function-type -MMD -MP -DSTANDALONE_EXE
SA_LDFLAGS = -static-libgcc -static-libstdc++ -mwindows
SA_LIBS = -luser32 -ldbghelp
SA_INCLUDES = -I. -Ipatches/src -Iengine/types -Iengine/funcs -Icode/Game \
              -Iruntime/types -Iruntime/funcs -Istandalone

# Standalone sources: game logic + engine funcs + runtime src + standalone entry (NO reimpl/patches)
SA_SRCS = $(GAME_SRC_SRCS) \
          $(ENGINE_FUNC_SRCS) \
          $(RUNTIME_FUNC_SRCS) \
          $(RUNTIME_SRC_SRCS) \
          $(wildcard standalone/*.cpp)

SA_OBJECTS = $(addprefix $(SA_OBJ_DIR)/, $(subst /,__,$(SA_SRCS:.cpp=.o)))

.PHONY: all debug release deploy clean undeploy run standalone standalone-debug sa-deploy sa-run sa-run-menu

all: debug

# Strip all printf() debug logging in release builds (zero runtime cost).
# See patches/src/log_strip.h for details. -include must use a forward-slash
# path so it resolves the same on MSYS2 bash and from VS Code task shells.
STRIP_LOGS_FLAGS = -DBANISHED_STRIP_LOGS -include patches/src/log_strip.h

debug: CXXFLAGS += -g -O0 -D_DEBUG
debug: $(TARGET)

release: CXXFLAGS += -O2 -DNDEBUG $(STRIP_LOGS_FLAGS)
release: $(TARGET)

standalone: SA_CXXFLAGS += -O3 -DNDEBUG $(STRIP_LOGS_FLAGS)
standalone: $(SA_TARGET)

standalone-debug: SA_CXXFLAGS += -g -O0 -D_DEBUG
standalone-debug: $(SA_TARGET)

# Link all objects into the DLL
$(TARGET): $(OBJECTS) $(HDE_OBJECTS) $(DEF_FILE) | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $(OBJECTS) $(HDE_OBJECTS) $(DEF_FILE) $(LIBS)
	@echo "Built: $@"
	@ls -lh $@

# Pattern rules: compile each source directory
$(OBJ_DIR)/patches__src__%.o: patches/src/%.cpp | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# HDE64 C source (length disassembler)
$(OBJ_DIR)/patches__src__hde__%.o: patches/src/hde/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

$(OBJ_DIR)/engine__funcs__%.o: engine/funcs/%.cpp | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

$(OBJ_DIR)/engine__reimpl__%.o: engine/reimpl/%.cpp | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# Pattern rules for code/Game subdirectories (mirrors original source tree)
$(OBJ_DIR)/code__Game__Action__%.o: code/Game/Action/%.cpp | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

$(OBJ_DIR)/code__Game__Component__%.o: code/Game/Component/%.cpp | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

$(OBJ_DIR)/code__Game__Dialog__%.o: code/Game/Dialog/%.cpp | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

$(OBJ_DIR)/code__Game__Entity__%.o: code/Game/Entity/%.cpp | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

$(OBJ_DIR)/code__Game__Game__%.o: code/Game/Game/%.cpp | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

$(OBJ_DIR)/code__Game__Goal__%.o: code/Game/Goal/%.cpp | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

$(OBJ_DIR)/code__Game__Object__%.o: code/Game/Object/%.cpp | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

$(OBJ_DIR)/code__Game__Player__%.o: code/Game/Player/%.cpp | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

$(OBJ_DIR)/code__Game__Tutorial__%.o: code/Game/Tutorial/%.cpp | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

$(OBJ_DIR)/code__Game__Work__%.o: code/Game/Work/%.cpp | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# Pattern rules for runtime/ directories
$(OBJ_DIR)/runtime__funcs__%.o: runtime/funcs/%.cpp | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

$(OBJ_DIR)/runtime__reimpl__%.o: runtime/reimpl/%.cpp | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# Pattern rules for runtime/src subdirectories (18 namespaces)
$(OBJ_DIR)/runtime__src__system__%.o: runtime/src/system/%.cpp | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

$(OBJ_DIR)/runtime__src__math__%.o: runtime/src/math/%.cpp | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

$(OBJ_DIR)/runtime__src__volume__%.o: runtime/src/volume/%.cpp | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

$(OBJ_DIR)/runtime__src__object__%.o: runtime/src/object/%.cpp | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

$(OBJ_DIR)/runtime__src__io__%.o: runtime/src/io/%.cpp | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

$(OBJ_DIR)/runtime__src__graphics__%.o: runtime/src/graphics/%.cpp | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

$(OBJ_DIR)/runtime__src__video__%.o: runtime/src/video/%.cpp | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

$(OBJ_DIR)/runtime__src__ui__%.o: runtime/src/ui/%.cpp | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

$(OBJ_DIR)/runtime__src__audio__%.o: runtime/src/audio/%.cpp | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

$(OBJ_DIR)/runtime__src__engine__%.o: runtime/src/engine/%.cpp | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

$(OBJ_DIR)/runtime__src__collision__%.o: runtime/src/collision/%.cpp | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

$(OBJ_DIR)/runtime__src__pathfinding__%.o: runtime/src/pathfinding/%.cpp | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

$(OBJ_DIR)/runtime__src__application__%.o: runtime/src/application/%.cpp | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

$(OBJ_DIR)/runtime__src__map__%.o: runtime/src/map/%.cpp | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

$(OBJ_DIR)/runtime__src__network__%.o: runtime/src/network/%.cpp | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

$(OBJ_DIR)/runtime__src__achievement__%.o: runtime/src/achievement/%.cpp | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# ============================================================
# Standalone EXE link + compile rules
# ============================================================
$(SA_TARGET): $(SA_OBJECTS) | $(SA_BUILD_DIR)
	$(CXX) $(SA_CXXFLAGS) $(SA_LDFLAGS) -o $@ $(SA_OBJECTS) $(SA_LIBS)
	@echo "Built standalone: $@"
	@ls -lh $@

$(SA_BUILD_DIR):
	mkdir -p $(SA_BUILD_DIR)

$(SA_OBJ_DIR):
	mkdir -p $(SA_OBJ_DIR)

# Standalone compile rules — same source dirs but with STANDALONE_EXE define
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

$(SA_OBJ_DIR)/standalone__%.o: standalone/%.cpp | $(SA_OBJ_DIR)
	$(CXX) $(SA_CXXFLAGS) $(SA_INCLUDES) -c $< -o $@

# ============================================================
# DLL build directories
# ============================================================
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

deploy: debug
	cp "$(TARGET)" "$(BANISHED_DIR)/version.dll"
	rm -f "$(BANISHED_DIR)/patch_debug.log"
	@echo "Deployed to $(BANISHED_DIR)/version.dll (log cleared)"

undeploy:
	rm -f "$(BANISHED_DIR)/version.dll"
	@echo "Removed version.dll from $(BANISHED_DIR)"

run: deploy
	cd "$(BANISHED_DIR)" && start Application-x64.exe
	@echo "Launched Banished with patch"

sa-deploy: standalone-debug
	cp "$(SA_TARGET)" "$(BANISHED_DIR)/reimpl-x64.exe"
	rm -f "$(BANISHED_DIR)/standalone_debug.log"
	@echo "Deployed to $(BANISHED_DIR)/reimpl-x64.exe"

sa-run: sa-deploy
	cd "$(BANISHED_DIR)" && BANISHED_AUTOSTART=1 start reimpl-x64.exe
	@echo "Launched standalone reimpl (BANISHED_AUTOSTART=1 -> auto New Game)"

# Same as sa-run but stops at the main menu (no auto-new-game). Use when you
# want to exercise the menu itself.
sa-run-menu: sa-deploy
	cd "$(BANISHED_DIR)" && start reimpl-x64.exe
	@echo "Launched standalone reimpl (menu only, no autostart)"

# Include auto-generated header dependency files
-include $(OBJECTS:.o=.d)
-include $(HDE_OBJECTS:.o=.d)
-include $(SA_OBJECTS:.o=.d)

clean:
	rm -rf $(BUILD_DIR) $(SA_BUILD_DIR)
