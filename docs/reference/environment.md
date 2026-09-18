# Dev environment (reference)

## Toolchain
- GCC 13.1 (MSYS2 UCRT64) at `C:\msys64\ucrt64\bin\`; C++17, static-linked runtime
- `mingw32-make` (full path `/c/msys64/ucrt64/bin/mingw32-make.exe`)
- MSYS2 packages are outdated (stale signatures): avoid `pacman -S` for new installs
- Python 3.9.10 system-wide; 3.10.11 in the project venv `tools/ghidra_mcp/.venv/`
- `uv` v0.9.24; Node.js + npx (for the vscode MCP server)
- JAVA_HOME `C:\Program Files\Java\jdk-24` (for Ghidra)

## VS Code debugging
- Configurations live in `.vscode/launch.json`: "Launch Standalone (debug, autostart new
  game)" sets `BANISHED_AUTOSTART`; "Launch Standalone (debug)" stops at the menu
- `vscode-debugger` MCP: breakpoints, stepping, variables, stack traces
- `vscode` MCP `start_debug_session` launches a config
- `workspaceFolder` must use a lowercase drive letter (`c:\...`)

## Ghidra
See the `ghidra` skill (`.claude/skills/ghidra/SKILL.md`) and `HOW_TO_START_GHIDRA.md`.
