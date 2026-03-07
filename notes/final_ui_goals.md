# Final UI/Dialog Deep Reimplementation Goals

Remaining work to eliminate ALL trampoline calls from dialog/UI reimpl files.
Once complete, every dialog/UI hook will run pure clean C++ with zero exe dependencies.

---

## Step 0 — DONE (2026-03-07) — Cleanup stale trampoline comments

## Step 1 — DONE (2026-03-07) — UIComponent (2 trampoline calls)

## Step 2 — DONE (2026-03-07) — Description classes (52 trampoline calls across 14 files)

---

## Step 4 — Audit and eliminate ResolveAddr exe calls

Scan all dialog/UI code for remaining `ResolveAddr(Module::Exe, ...)` calls.
These are direct exe dependencies that bypass the engine_funcs layer. Each one
should be either:
- Moved to a proper function pointer in `engine/funcs/engine_funcs.h`
- Or reimplemented in clean C++ if the target function is simple enough

Files to audit:
- `code/Game/Dialog/*.cpp`
- `engine/reimpl/*.cpp` (dialog/UI files only)

---

## Step 4 — Detailed TODO

Audit complete. All `ResolveAddr(Module::Exe, ...)` calls in dialog/UI code fall into
four categories. Category breakdown:

| Category | Count | Action |
|----------|-------|--------|
| A: Vtable reads (Reimpl_ Dtor vtable restore) | 37 | Eliminate by generating vtable in clean C++ |
| B: Hook install targets (Install* functions) | 198 | Inherent to hook mechanism — keep until full standalone |
| C: Inline helpers in clean code (code/Game/Dialog/) | 14 | Move to `engine_funcs.h` immediately |
| D: Data/rdata reads (constants) | 1 | Hardcode constant in clean code |
| **Total** | **250** | |

---

### Category A — Vtable reads in Reimpl_ Dtor functions
These exist in reimpl hook wrappers to restore the class vtable before calling the base
dtor. They are needed because the hook intercepts the Dtor and must re-set the original
vtable. These go away naturally once every method is reimplemented and the hooks are
removed. **No immediate action needed** — they are scaffolding, not exe dependencies in
clean code.

Unique vtable addresses (37):

- [ ] `0x14015b020` — AchievementDialog vtable — `engine/reimpl/achievementdialog.cpp` (vtable read)
- [ ] `0x14015b070` — AchievementDialog secondary vtable (DialogController) — `engine/reimpl/achievementdialog.cpp` (vtable read)
- [ ] `0x14015b120` — AutoSaveDialog vtable — `engine/reimpl/autosavedialog.cpp` (vtable read)
- [ ] `0x14015bca0` — EventNotifyDialog vtable — `engine/reimpl/eventnotifydialog.cpp` (vtable read)
- [ ] `0x14015bd80` — EventPopupDialog vtable — `engine/reimpl/eventpopupdialog.cpp` (vtable read)
- [ ] `0x14015be00` — FollowDialog vtable — `engine/reimpl/followdialog.cpp` (vtable read)
- [ ] `0x14015beb0` — GoalDialog vtable — `engine/reimpl/goaldialog.cpp` (vtable read)
- [ ] `0x14015f300` — LoaderDialog vtable — `engine/reimpl/loaderdialog.cpp` (vtable read)
- [ ] `0x14015f550` — MainMenuDialog vtable — `engine/reimpl/mainmenudialog.cpp` (vtable read)
- [ ] `0x14015c180` — MapParamsDialog vtable — `engine/reimpl/mapparamsdialog.cpp` (vtable read)
- [ ] `0x14015c480` — MessageDialog vtable — `engine/reimpl/messagedialog.cpp` (vtable read)
- [ ] `0x14015c480` — OptionInputBindDialog base vtable (MessageDialog vtable) — `engine/reimpl/optioninputbinddialog.cpp` (vtable read)
- [ ] `0x14015c670` — ModDetailsDialog vtable — `engine/reimpl/moddetailsdialog.cpp` (vtable read)
- [ ] `0x14015c870` — ModDialog vtable — `engine/reimpl/moddialog.cpp` (vtable read)
- [ ] `0x14015c500` — ModBrowseUI vtable — `engine/reimpl/modbrowseui.cpp` (vtable read)
- [ ] `0x14015c6c0` — ModUploadDialog vtable — `engine/reimpl/moduploaddialog.cpp` (vtable read)
- [ ] `0x14015c920` — NewGameDialog vtable — `engine/reimpl/newgamedialog.cpp` (vtable read)
- [ ] `0x14015ceb0` — OptionsDialog vtable — `engine/reimpl/optionsdialog.cpp` (vtable read)
- [ ] `0x14015f900` — SaveLoadDialog vtable — `engine/reimpl/saveloaddialog.cpp` (vtable read)
- [ ] `0x14015d2d0` — ScenarioDialog vtable — `engine/reimpl/scenariodialog.cpp` (vtable read)
- [ ] `0x14015fa00` — ToolbarDialog vtable — `engine/reimpl/toolbardialog.cpp` (vtable read)
- [ ] `0x1401609e0` — TutorialHighlightDialog vtable — `engine/reimpl/tutorialhighlightdialog.cpp` (vtable read, 4x)
- [ ] `0x140160d20` — TutorialSetupDialog vtable — `engine/reimpl/tutorialsetupdialog.cpp` (vtable read)
- [ ] `0x140160c30` — TutorialSetupDialog base vtable (intermediate) — `engine/reimpl/tutorialsetupdialog.cpp` (vtable read)
- [ ] `0x14015d0f0` — ProfessionUI vtable — `engine/reimpl/professionui.cpp` (vtable read)
- [ ] `0x14015dce0` — TownHallUI vtable — `engine/reimpl/townhallui.cpp` (vtable read)
- [ ] `CropUIAddr::Vtable` (0x14015d1c0) — CropUI vtable used by OrchardUI base restore — `engine/reimpl/orchardui.cpp` (vtable read)
- [ ] `CropUIAddr::Vtable` (0x14015d1c0) — CropUI vtable used by PastureUI base restore — `engine/reimpl/pastureui.cpp` (vtable read)
- [ ] All remaining panel Addr::Vtable constants (AttendanceUI, BuildUI, BuildRequirementUI, CemeteryUI, CitizenUI, ConsumeProduceUI, ControlUI, DebugUI, DestroyUI, EntityUI, EventLogUI, HelpUI, HotKeyToolTipUI, ImmigrationUI, MapUI, NoiseUI, PageToggleUI, ProductionUI, RadiusUI, ResidenceUI, ResourceLimitUI, SpriteCaptureUI, StatusBarUI, StatusIconUI, StorageCacityUI, StorageUI, TitleDescToolTipUI, ToolUI, TradeUI, TutorialUI, UpgradeUI, WorkPlaceUI, MenuScene, UIComponent)

---

### Category B — Hook install targets in Install* functions
Every `Install*()` function calls `ResolveAddr(Module::Exe, addr)` to get the address of
the exe function to patch. These are the address-of-hook, not exe calls from clean code.
They are structurally required for the hook mechanism and will be removed only when the
full standalone exe is achieved. **No action needed now.**

Listed by class (198 total hook-install ResolveAddr calls across all dialog/UI reimpl files).
Only noting the unique function addresses per file for reference:

**Dialogs (engine/reimpl/):**
- AchievementDialog: `0x1400103F0`, `0x14005D490`, `0x14005D240`, `0x14005D440`, `0x140010560`, `0x14005DD40`, `0x14005DC50`
- AutoSaveDialog: `0x140010780`, `0x14005DFC0`, `0x14005E090`
- EventNotifyDialog: `0x140011550`, `0x1400686d0`, `0x1400680c0`, `0x140067c30`, `0x140068680`
- EventPopupDialog: `0x1400119B0`, `0x140068C20`, `0x140068BD0`
- FollowDialog: `0x140011AF0`, `0x140069340`, `0x140069320`, `0x140068F70`, `0x140069250`, `0x140069230`
- GoalDialog: `0x140011C10`, `0x140069810`, `0x140069640`, `0x1400697C0`
- LoaderDialog: `0x140020C70`, `0x1400B2D90`, `0x1400B2D00`, `0x1400B2D50`
- MainMenuDialog: `0x140020DA0`, `0x1400B3CD0`, `0x1400B3C60`, `0x1400B3B50`, `0x140020CB0`
- MapParamsDialog: `0x140012150`, `0x14006b810`, `0x14006b790`, `0x14006b5f0`
- MessageDialog: `0x140012660`, `0x14006D140`, `0x14006CFF0`, `0x14006D030`, `0x14006CFB0`, `0x14006CF70`
- ModDetailsDialog: `0x140012890`, `0x14006D700`, `0x14006D6A0`
- ModDialog: `0x140012A30`, `0x14006F150`, `0x14006ED40`, `0x1400129D0`
- ModBrowseUI: `0x140012720`
- ModUploadDialog: `0x140012840`
- NewGameDialog: `0x140012cc0`, `0x1400706d0`, `0x140070650`, `0x140070110`
- OptionInputBindDialog: `0x140012f80`, `0x14006d140`, `0x140071c80`, `0x14006d070`, `0x14006cff0`, `0x14006d030`, `0x14006cfb0`, `0x14006cf70`
- OptionsDialog: `0x140012f30`, `0x1400742d0`, `0x140072f20`
- SaveLoadDialog: `0x1400211b0`, `0x1400b7d00`, `0x1400b6880`, `0x1400b79b0`, `0x1400b67e0`
- ScenarioDialog: `0x140013AF0`, `0x140078010`, `0x140077BE0`, `0x140077F80`
- ToolbarDialog: `0x140021760`, `0x1400b8d40`, `0x1400b92a0`
- TutorialHighlightDialog: `0x140027910`, `0x1400e0020`
- TutorialSetupDialog: `0x140028620`, `0x1400e2a60`, `0x140028600`, `0x1400e25f0`, `0x1400e24c0`, `0x1400e2460`

**Panels (engine/reimpl/):**
- AttendanceUI: `0x140010560`, `0x14005DD40`, `0x14005DC50`
- BuildUI: `0x140020A90`, `0x1400B1F40`, `0x1400B1A50`, `0x1400B1EA0`
- BuildRequirementUI: `0x140010860`, `0x14005E7C0`
- CemeteryUI: `0x1400109B0`, `0x14005F090`, `0x14005EF50`
- CitizenUI: `0x140010FD0`, `0x1400651A0`, `0x140065130`, `0x140064AE0`, `0x1400650F0`
- ConsumeProduceUI: `0x140010B90`, `0x14005F3D0`, `0x14005F1F0`, `0x14005F330`
- ControlUI: `0x140010E20`, `0x14005F9B0`, `0x14005F870`, `0x14005F7F0`
- CropUI: `0x140013CA0`, `0x140078BE0`, `0x140078CB0`, `0x140078D30`, `0x140078D80`
- DebugUI: `0x140010F10`, `0x1400619E0`, `0x140061280`, `0x1400610C0`, `0x140060BC0`
- DestroyUI: `0x140011200`, `0x140066150`, `0x140065B00`, `0x140066080`
- EntityUI: `0x1400112D0`, `0x1400670D0`, `0x140067090`, `0x140066D80`, `0x140066AD0`, `0x140067050`
- EventLogUI: `0x1400114B0`, `0x140067640`, `0x140067440`, `0x1400675D0`
- HelpUI: `0x140011CF0`, `0x14006A500`, `0x14006A230`, `0x14006A290`
- HotKeyToolTipUI: `0x140011E70`, `0x14006A980`
- ImmigrationUI: `0x140011FD0`, `0x14006B330`, `0x14006B130`, `0x14006B300`
- MapUI: `0x1400124D0`, `0x14006CA80`, `0x14006C9E0`
- NoiseUI: `0x140012DA0`, `0x1400714C0`, `0x1400714A0`
- OrchardUI: `0x1400130E0`, `0x1400755B0`, `0x1400754B0`, `0x140078D30`, `0x140078D80`, `0x140075380`, `0x140075430`
- PageToggleUI: `0x1400131C0`, `0x140075850`, `0x140075780`, `0x140075710`
- PastureUI: `0x140020EE0`, `0x1400B5090`, `0x1400B4E50`, `0x140078D30`, `0x1400B4E20`, `0x1400B4C40`, `0x1400B4D00`
- ProductionUI: `0x1400132B0`, `0x140075DA0`, `0x140075A20`
- ProfessionUI: `0x140013450`, `0x140076CA0`, `0x1400765A0`, `0x140076C10`
- RadiusUI: `0x140013640`, `0x140077650`, `0x140077600`
- ResidenceUI: `0x140020FC0`, `0x1400B5850`, `0x1400B51F0`, `0x1400B5820`
- ResourceLimitUI: `0x1400138D0`, `0x140077970`, `0x1400778D0`, `0x1400777E0`
- SpriteCaptureUI: `0x140013fc0`, `0x140079640`, `0x1400793a0`
- StatusBarUI: `0x140014130`, `0x14007a3d0`, `0x1400798d0`, `0x14007a3a0`
- StatusIconUI: `0x140014370`, `0x14007b270`, `0x14007b0a0`
- StorageCapacityUI: `0x140014450`, `0x14007b600`, `0x14007b4d0`
- StorageUI: `0x140021390`, `0x1400b8820`, `0x1400b8450`
- TitleDescToolTipUI: `0x140014510`, `0x14007b730`
- ToolUI: `0x1400145f0`, `0x14007bd70`, `0x14007b9b0`, `0x14007bc30`
- TownHallUI: `0x1400146b0`, `0x140081000`, `0x14007d550`, `0x14007e550`, `0x14007c180`, `0x140080db0`
- TradeUI: `0x140016020`, `0x140084dd0`, `0x140082a70`, `0x140082a00`, `0x140082150`, `0x1400827b0`, `0x140081f20`, `0x1400868c0`
- TutorialUI: `0x140029470`, `0x1400e37a0`, `0x1400e3700`, `0x1400e36e0`
- UpgradeUI: `0x140016d50`, `0x140086d90`, `0x140086c90`
- WorkPlaceUI: `0x140016e90`, `0x140087940`, `0x140087080`, `0x1400877e0`
- MenuScene: `0x14001b830`, `0x1400a3630`, `0x14001b810`, `0x14001b800`
- UIComponent: `0x14012dd00`, `0x14012dca0`

---

### Category C — Inline helper functions in clean code DONE

### Category D — Data/rdata reads in clean code DONE

### Summary Counts
- **Category A** (vtable reads): 37 unique vtable addresses (scaffolding, auto-removed with hooks)
- **Category B** (hook install targets): 198 `ResolveAddr` calls (inherent to hook mechanism)
- **Category C** (inline helpers in clean code): 14 unique addresses — **needs engine_funcs.h entries**
- **Category D** (rdata constants): 1 — **hardcode the constant**
- **Grand total unique exe addresses**: ~250 `ResolveAddr` calls across all dialog/UI files

## Step 5 — Replace exe-resolved engine_funcs with clean C++

Some `fn_*` function pointers in `engine/funcs/engine_funcs.h` resolve to **exe**
functions at runtime. These need to be decompiled and reimplemented as clean C++
so dialog/UI code has zero exe dependencies.

Runtime DLL (`fn_*` pointers resolved via `ResolveExport`/`ResolveOrdinal`) are fine
to keep — the DLL is a separate binary and will eventually be consumed via
`#include <dllheader.h>` style imports.

Audit `engine/funcs/engine_funcs.h` for `fn_*` pointers that resolve from
`Module::Exe` and are used by dialog/UI code. Prioritize by usage count.

---

## Summary

| Category | Files | Trampoline calls | Status |
|----------|-------|-----------------|--------|
| Step 0: Comment cleanup | 10 | 0 | DONE |
| Step 1: UIComponent | 1 | 2 | DONE |
| Step 2: Descriptions | 14 | 52 | DONE |
| Step 3: Elements | 2 | 5 | DONE |
| Step 4: ResolveAddr audit | ~30 | — | TODO |
| Step 5: Exe engine_funcs | TBD | — | TODO |