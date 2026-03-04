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
CXXFLAGS = -std=c++17 -Wall -Wextra -Wno-cast-function-type -MMD -MP
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

DEF_FILE = patches/src/proxy.def
TARGET = $(BUILD_DIR)/version.dll

.PHONY: all debug release deploy clean undeploy run

all: debug

debug: CXXFLAGS += -g -O0 -D_DEBUG
debug: $(TARGET)

release: CXXFLAGS += -O2 -DNDEBUG
release: $(TARGET)

# Link all objects into the DLL
$(TARGET): $(OBJECTS) $(DEF_FILE) | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $(OBJECTS) $(DEF_FILE) $(LIBS)
	@echo "Built: $@"
	@ls -lh $@

# Pattern rules: compile each source directory
$(OBJ_DIR)/patches__src__%.o: patches/src/%.cpp | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

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

# Include auto-generated header dependency files
-include $(OBJECTS:.o=.d)

clean:
	rm -rf $(BUILD_DIR)
