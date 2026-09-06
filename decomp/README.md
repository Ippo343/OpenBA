# Decompilation Status & Type Inventory

Status key (standalone mode — there is no proxy DLL / hooking anymore):
- `-` = not started
- `~` = partially decompiled
- `x` = fully decompiled & documented
- `R` = reimplemented and registered in the standalone exe (`standalone/address_registry.cpp`)

## Binary Summary

| Binary | Functions | Classes | Role | Status File |
|--------|-----------|---------|------|-------------|
| Application-x64.exe | 11,473 (Ghidra today; CLAUDE.md's 10,476 is the initial import count) | ~67 ECS components + 61 UI classes + game logic | Game logic, UI controllers, ECS | [application.md](application.md) |
| Runtime-x64.dll | 15,109 iterated / 15,290 with thunks | 100+ across 18 namespaces | Full engine runtime | [runtime.md](runtime.md) |
| VideoDX11-x64.dll | 576 (85 rendering, rest CRT) | 12 Video::*Data subclasses | DX11 renderer plugin | [videodx11.md](videodx11.md) |

---

## Reimplementation status and blockers

The exe port (Goal #2) is baseline-complete: standalone `reimpl-x64.exe` links
`Runtime-x64.dll` + `VideoDX11-x64.dll`, boots, builds a world, simulates, renders and has a
working HUD. Remaining exe-side gaps and the current registration census are tracked in
[`docs/runtime_port/PLAN.md`](../docs/runtime_port/PLAN.md) (§1) rather than here.

The next phase — reimplementing `Runtime-x64.dll` itself — is planned in the same document
(§4 strategy, §6 per-function recipe). Read it before starting runtime work.
