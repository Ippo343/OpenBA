# Cutover closure — the one progress number for the Runtime port

The DLL can be unloaded when every function the exe reaches **through the DLL's
own call graph** exists in our tree. The census (`runtime_census.py`) only counts
what our source calls directly, so it undercounts by everything a faithful port
calls next. This tool computes the real set.

Three steps, ~1 minute:

```bash
# 1. census -> roots (needs the venv's pefile)
tools/ghidra_mcp/.venv/Scripts/python.exe tools/cutover_closure/build_roots.py

# 2. in Ghidra, program Runtime-x64.dll: run tools/cutover_closure/CutoverClosure.java
#    (Script Manager -> add this directory, or the MCP run_ghidra_script tool)

# 3. summarise + worklist
tools/ghidra_mcp/.venv/Scripts/python.exe tools/cutover_closure/report.py
```

Outputs live in `docs/runtime_port/closure/` and are checked in: `closure.csv`
(one row per reachable function) and `WORKLIST.md` (the unported ones, by
namespace, smallest first). The headline is the `ENGINE_TO_PORT` count.

What it cannot see: virtual calls whose target is not already a census row, and
callbacks stored in data. So it is a lower bound, and the cutover boot
(`make cutover`) remains the gate that catches what the closure missed.
