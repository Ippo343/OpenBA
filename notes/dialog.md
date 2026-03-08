# Dialog Module — Reimplementation Status

**Status: 100% complete**

57 source files in `code/Game/Dialog/`, all hooks enabled, zero trampoline passthroughs.

---

## Remaining exe dependencies (3, all cross-module)

| File | Dependency | Exe Address | Module |
|------|-----------|-------------|--------|
| `destroyui.cpp:371` | `g_StateMachine_Tick` | `0x1400a9660` | StateMachine |
| `tutorialsetupdialog.cpp:189,207` | `fn_StringArray_Resize` | `0x140028860` | Serialization |
| `tutorialsetupdialog.cpp:222` | `fn_SerializeExternalPtr` | `0x140028650` | Serialization |

`tutorialsetupdialog.cpp` also reads vtable addresses from exe `.rdata` — scaffolding that disappears with standalone exe (TutorialSetupDialog extends `Object::External`, not `DialogInstance`).

All other `fn_*` calls (238+) resolve from Runtime DLL exports/ordinals.
