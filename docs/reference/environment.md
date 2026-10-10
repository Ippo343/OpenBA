# Dev environment (reference)

## Toolchain
- GCC 13.1 (MSYS2 UCRT64) at `C:\msys64\ucrt64\bin\`; C++17, static-linked runtime
- `mingw32-make` (full path `/c/msys64/ucrt64/bin/mingw32-make.exe`)
- MSYS2 packages are outdated (stale signatures): avoid `pacman -S` for new installs
- Python 3.9.10 system-wide; 3.10.11 in the project venv `tools/ghidra_mcp/.venv/`
- `uv` v0.9.24; Node.js + npx (for the vscode MCP server)
- JAVA_HOME `C:\Program Files\Java\jdk-24` (for Ghidra)
- CMake 4.4 + ninja from pip (`C:\Users\USPC\AppData\Roaming\Python\Python312\Scripts`, on PATH)

## Third-party (pinned submodules; run `git submodule update --init` after a clone)
- **SDL3** at `third_party/SDL`: tag **`release-3.2.30`**, commit
  `f5e5f6588921eed3d7d048ce43d9eb1ff0da0ffc`, the newest `release-3.2.x` (D4_RULES §3 Q4: SDL3
  only, static). `.gitmodules` has `shallow = true`. `mingw32-make third_party` builds it once
  into `build/third_party/install` (stamp `build/third_party/sdl3.stamp`; every `standalone*`
  target depends on it). GCC 13.1 ucrt64 builds it as is, no fallback:
  ```
  cmake -S third_party/SDL -B build/third_party/sdl3 -G Ninja -DCMAKE_BUILD_TYPE=Release
        -DSDL_SHARED=OFF -DSDL_STATIC=ON -DSDL_TEST_LIBRARY=OFF -DSDL_TESTS=OFF -DSDL_EXAMPLES=OFF
        -DSDL_GPU=OFF -DSDL_RENDER=OFF -DSDL_CAMERA=OFF -DSDL_JOYSTICK=OFF -DSDL_HAPTIC=OFF
        -DSDL_HIDAPI=OFF -DSDL_SENSOR=OFF -DSDL_DIALOG=OFF
        -DCMAKE_INSTALL_PREFIX=<repo>/build/third_party/install
  cmake --build build/third_party/sdl3 -j2 --target install
  ```
  The exe links `libSDL3.a` plus the system libraries the installed `sdl3.pc` lists, read at
  link time. The unit tests and the resave tool do not link SDL (D4_RULES C7). The CMake twin
  builds the same tree with `add_subdirectory(third_party/SDL EXCLUDE_FROM_ALL)`.
  To move the pin: `git -C third_party/SDL fetch --depth 1 origin tag <tag>`, check the tag
  out, commit the submodule, update this entry, delete `build/third_party`.

## VS Code debugging
- Configurations live in `.vscode/launch.json`: "Launch Standalone (debug, autostart new
  game)" sets `BANISHED_AUTOSTART`; "Launch Standalone (debug)" stops at the menu
- Launch AND inspect with the `vscode-debugger` MCP (`start_config`, `mcpManaged: true`):
  a session started through the `vscode` MCP's `start_debug_session` is invisible to it
- `vscode` MCP is for general editor operations (terminal, tasks, open files)
- Run `mingw32-make sa-deploy` first: the preLaunchTask may not run through MCP
- `workspaceFolder` must use a lowercase drive letter (`c:\...`)

## Ghidra
See the `ghidra` skill (`.claude/skills/ghidra/SKILL.md`) and `docs/reference/HOW_TO_START_GHIDRA.md`.
