# Dialog Module — Reimplementation Goals

**Directory:** `code/Game/Dialog/`
**Source files:** 57 .cpp + 3 .h files
**Status: 100% COMPLETE**

---

All 57 dialog/panel controllers are reimplemented with zero trampoline passthroughs.

## Remaining Exe Dependencies (3, all cross-module)

| File | Dependency | Exe Address | Target Module |
|------|-----------|-------------|---------------|
| `destroyui.cpp:371` | `g_StateMachine_Tick` | `0x1400a9660` | Game (StateMachine) |
| `tutorialsetupdialog.cpp:189,207` | `fn_StringArray_Resize` | `0x140028860` | Entity (Serialization) |
| `tutorialsetupdialog.cpp:222` | `fn_SerializeExternalPtr` | `0x140028650` | Entity (Serialization) |

All other `fn_*` calls (238+) resolve from Runtime DLL exports/ordinals.

## Future: Standalone Exe

When building as standalone, `tutorialsetupdialog.cpp` reads vtable addresses from exe `.rdata` — this scaffolding disappears once `TutorialSetupDialog` is fully standalone (it extends `Object::External`, not `DialogInstance`).
