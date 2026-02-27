# Banished Reimplementation - Build System
# Usage:
#   make            - build debug DLL
#   make release    - build release DLL
#   make deploy     - build + copy to Banished directory
#   make run        - build + copy + launch game
#   make undeploy   - remove from Banished directory
#   make clean      - remove build artifacts

CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -Wno-cast-function-type
LDFLAGS = -shared -static-libgcc -static-libstdc++
LIBS = -luser32 -ldbghelp

# Include paths: project root + all source directories
INCLUDES = -I. -Ipatches/src -Iengine/types -Iengine/funcs

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

# Auto-discover reimplementation files
ENGINE_REIMPL_SRCS = $(wildcard engine/reimpl/*.cpp)

ALL_SRCS = $(PATCH_SRCS) $(ENGINE_FUNC_SRCS) $(ENGINE_REIMPL_SRCS)

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

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

deploy: debug
	cp "$(TARGET)" "$(BANISHED_DIR)/version.dll"
	@echo "Deployed to $(BANISHED_DIR)/version.dll"

undeploy:
	rm -f "$(BANISHED_DIR)/version.dll"
	@echo "Removed version.dll from $(BANISHED_DIR)"

run: deploy
	cd "$(BANISHED_DIR)" && start Application-x64.exe
	@echo "Launched Banished with patch"

clean:
	rm -rf $(BUILD_DIR)
