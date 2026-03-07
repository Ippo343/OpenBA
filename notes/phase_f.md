# Phase F: UI Deep Reimplementation — Remaining Work

**Progress: ~137 done, 13 N/A, ~7 blocked.**
Waves 1-5 complete. Wave 6 done as Runtime DLL methods. Waves 7-8 complete (except blocked items below).

## Wave 8: Passthroughs

### Still delegated to exe (enabled hooks, clean dispatch but sub-functions via fn pointers):
| Method | Reason |
|--------|--------|
| TradeUI::Serialize | 500+ lines, builds 4 dynamic arrays, enumerates ExternalFactory |
| TradeUI::Startup | 200+ lines, calculates trade values, manages warnings |
| TradeUI::OnRemove | 80+ lines, event dispatcher calling 8 sub-functions |

### Blocked (cannot reimpl without significant new infrastructure):
| Method | Blocker |
|--------|---------|
| SaveLoadDialog::Load | MSVC vtable construction (UpdateLoop/LoaderLoop/DialogLoaderLoopT ctors with 11 params) |
| OptionsDialog::Startup | 352 lines, heavy _singleton_exref + StringTable dependencies |
| SpriteCaptureUI::OnCreate | Needs ~6 new fn ptrs (Input::Device, FileSystem, External::SaveText) — feasible but low priority |
| ModDialog::Slot4 | 428 lines, ImageBuffer/MaterialInstance/SpriteSheet construction pipeline |

---

## Workflow Reminder

1. Decompile in Ghidra → rename, annotate
2. Add deps to `engine/funcs/engine_funcs.h/.cpp`
3. Write clean C++ in `code/Game/Dialog/`
4. Update hook wrapper in `engine/reimpl/` to call clean code
5. `mingw32-make deploy` + test

Group by class — do all methods within one class together.
