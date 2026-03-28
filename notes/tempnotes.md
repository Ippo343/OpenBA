# Crash Debugging Session — 2026-03-23

## Status: Bug Class 3 removal causes MenuScene::Slot5 startup crash

The Bug Class 3 `ADD RAX,8` / `ADD R11,8` injection in trampoline prologues was
removed because it was proven incorrect for DecalComponent::Draw (MOVAPS alignment
fault) and save/restore consistency. However, removing it introduces a startup crash
during menu scene loading.

## What was done

### Bug Class 3 removal (hooks.cpp, hooks.h)
- Removed `ADD RAX,8` injection after `MOV RAX,RSP` in `InstallHook()`
- Removed `ADD R11,8` injection after `MOV R11,RSP` in `InstallHookRelocating()`
- Prologue bytes now copied verbatim in both functions
- **This fixes** DecalComponent::Draw MOVAPS crash (confirmed)
- **This breaks** MenuScene::Slot5 startup (confirmed by disabling other changes)

### Bug Class 9 — RIP-relative from mutable .data (NEW)
- `InstallHookRelocating()` copies VALUES from RIP-relative targets into data area
- For `.data` globals (like engine singleton at `0x1401c3dd8`), value is 0 at install time
- 5 hooks affected, all reading from `0x1401c3dd8`:
  - BuildComponent::Draw (vtable 0x140162260, slot 6)
  - NomadComponent::OnCreate (vtable 0x14015a020, slot 7)
  - ClearStorage::Slot10 (vtable 0x1401611a0, slot 10)
  - Pickup::Slot10 (vtable 0x140161810, slot 10)
  - RemoveAnimal::Purecall (vtable 0x14015ffe0, slot 4)
- Fix: switched all 5 to `InstallHookVTable()` in their reimpl .cpp files
- **Currently DISABLED** in hooks.cpp (set to false) pending the Bug Class 3 resolution
- The vtable hook code is ready in the reimpl files, just needs re-enabling

### FinishTutorialInstance::Slot5
- Re-enabled (true) in hooks.cpp — was previously disabled, user's own change
- Not related to either crash

## The startup crash (needs fixing)

### Crash details
- Exception: ACCESS_VIOLATION (0xC0000005) in **VideoDX11-x64.dll+0x260F**
- Fault: READ of NULL (RCX=0)
- Last hook breadcrumb: "LoaderDialog::Slot6"
- Happens during menu scene loading (DialogLoaderLoopT::Startup)
- Call chain: Reimpl_Slot5 (MenuScene) → original MenuScene::Slot5 (0x1400A3630) →
  EXE+0xB07CF → EXE+0x72F8 → Runtime (several frames) → VideoDX11 crash

### Key observation
MenuScene::Slot5 uses **vtable hooking** (InstallHookVTable), NOT prologue hooking.
The Bug Class 3 removal should NOT affect it directly. But some OTHER hook that IS
affected by Bug Class 3 (one of the 18 hooks with MOV RAX,RSP or MOV R11,RSP) must
be corrupting state during startup that cascades into the DX11 crash.

### The 18 hooks affected by Bug Class 3 removal
These hooks have `MOV RAX,RSP` or `MOV R11,RSP` in their prologue and previously had
`ADD RAX/R11,8` injected. They're tagged in hooks.cpp with "RSP correction" comments:
- AnimalComponent::Update
- DecalComponent::Draw
- ParticleComponent::OnRemove
- ClearStorage::Slot9
- FireWork::Slot9
- ForesterWork::Slot9
- HarvestArea::Slot9
- HarvestOrchard::Slot9
- HerbalistWork::Slot9
- HuntWork::Slot9
- MarketWork::Slot4, Slot9
- MoveTo::Execute
- PastureWork::Slot9
- Pickup::Slot4
- TendArea::Slot9
- TradeWork::Slot4, Slot9

### Why the +8 was wrong but removing it also breaks things
The analysis showed:
- With +8: RAX = RSP_tramp + 8 (misaligned for MOVAPS, breaks save/restore)
- Without +8: RAX = RSP_tramp (correct alignment, consistent save/restore)

BUT the function body code after the trampoline jump-back may use RAX for
**absolute addressing** that assumes RAX = original_entry_RSP. The save/restore
at [RAX+0x20] and [R11+0x58] are consistent WITH EACH OTHER regardless of +8,
but other code in the function body might pass RAX-derived addresses to sub-calls
that expect them to be at specific stack locations.

### Suggested approach for next session
1. **Bisect which of the 18 hooks causes the crash** — disable them in groups
   (use DEBUG_HOOK_FILTER or flip to false) to find which one corrupts state
   during startup. Most are gameplay hooks (Work/Action slots) unlikely to run
   during menu loading. Focus on hooks that might fire during initialization:
   - ParticleComponent::OnRemove
   - MoveTo::Execute (unlikely during menu load)

2. **Consider a per-function fix** — instead of globally removing +8, only remove
   it for functions that use MOVAPS (like DecalComponent::Draw). Keep +8 for
   functions where it happens to produce correct behavior.

3. **Or use vtable hooking for all Bug Class 3 functions** — the safest approach,
   since vtable hooks don't touch the prologue at all. Need vtable addresses and
   slot indices for all 18 affected functions.

4. **Or implement RSP-safe hooking** — `InstallHookRSPSafe()` already exists in
   hooks.cpp (line ~154). It's designed for JMP-based (not CALL-based) detours
   where RSP is preserved. The Reimpl wrapper would need to be a naked function
   that JMPs to the trampoline instead of CALLing it.

## Files changed (uncommitted)
- `patches/src/hooks.cpp` — Bug Class 3 removal + 5 hooks disabled + FinishTutorial enabled
- `patches/src/hooks.h` — Updated InstallHook doc comment
- `engine/reimpl/buildcomponent.cpp` — vtable hook for Draw
- `engine/reimpl/nomadcomponent.cpp` — vtable hook for OnCreate
- `engine/reimpl/clearstorage.cpp` — vtable hook for Slot10
- `engine/reimpl/pickup.cpp` — vtable hook for Slot10
- `engine/reimpl/removeanimal.cpp` — vtable hook for Purecall
- `engine/reimpl/decalcomponent.cpp` — unchanged but prologue size comment (13 vs 14)
- `CLAUDE.md` — Bug Class 3 rewrite + Bug Class 9 added + disabled hooks updated
