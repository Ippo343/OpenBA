---
name: ghidra
description: Ghidra + GhidraMCP setup for this project, the naming conventions for renaming functions/vtables/globals, and what belongs in Ghidra versus markdown. Use before renaming or annotating anything in Ghidra, or when the Ghidra MCP bridge needs starting.
---

# Ghidra

## Ghidra is the primary documentation tool
Function renames, type annotations, struct layouts and per-function comments go directly
in Ghidra: persistent, searchable, in sync with the analysis. Markdown is for what does
not fit there (architecture, algorithm analysis).

## Setup
- Ghidra 12.0.3 at `C:\tools\ghidra_12.0.3_PUBLIC`; JAVA_HOME `C:\Program Files\Java\jdk-24`
- The project holds 7 programs, including `Runtime-x64-profile.dll` (ToolKit build, debug
  strings + 5,182 exports) — the primary source for naming unexported Runtime internals
- GhidraMCP (bethington/ghidra-mcp v3.0.0, 179 tools): bridge
  `tools/ghidra_mcp/bridge_mcp_ghidra.py`, Python 3.10 venv `tools/ghidra_mcp/.venv/`,
  extension `tools/ghidra_mcp/GhidraMCP-3.0.0.zip`
- Startup steps: `HOW_TO_START_GHIDRA.md`

## Naming conventions
- Functions: `Namespace_ClassName_MethodName` (e.g. `Engine_MainLoop_Run`)
- Vtables: `vftable_ClassName` or `vtable_Namespace_ClassName`
- Globals: `g_Namespace_variableName`
- Components: keep the full component name (e.g. `CitizenAIComponent_Update`)

## No archaeology needed for import slots
The exe's `0x14015xxxx` dispatch slots are its PE import table; get a slot's mangled name
with `tools/ghidra_mcp/.venv/Scripts/python.exe tools/dispatch_slot_map.py <addr>`.

Binary facts (bases, sizes, export counts): `docs/reference/binaries.md`.
