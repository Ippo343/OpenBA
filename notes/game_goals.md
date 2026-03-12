# GameUpdateLoop Reimplementation Status

Tracks the current state of the GameUpdateLoop reimpl (`code/Game/Game/gameupdateloop.cpp`)
and its hook wrapper (`engine/reimpl/gameupdateloop.cpp`).

**Milestone (2026-03-11):** Game runs through menu → tutorial → gameplay with no crashes.
Update reimpl is live, Draw/Startup/Shutdown/Load/Destroy are passthroughs.

---

## Vtable Methods (8 slots)

| Slot | Method | Address | Hook Type | Status | Notes |
|------|--------|---------|-----------|--------|-------|
| +0x00 | Destroy | 0x140019DD0 | Prologue (19B) | Passthrough | Scalar deleting destructor |
| +0x08 | Startup | 0x140097C00 | Prologue (14B) | Passthrough | Reimpl written but crashes on ExternalList::Find |
| +0x10 | Shutdown | 0x1400977C0 | Prologue (12B) | Passthrough | Reimpl written, waiting on Startup |
| +0x18 | **Update** | 0x140096380 | **Vtable patch** | **REIMPL LIVE** | Main simulation tick, working |
| +0x20 | Draw | 0x140096010 | Vtable patch | Passthrough | Reimpl written but crashes in VideoDX11 |
| +0x28 | DrawFailed | 0x140096000 | None (3B, unhookable) | N/A | Just a `ret` |
| +0x30 | Load | 0x140096B00 | Prologue (16B) | Passthrough | 759 instructions, large function |
| +0x38 | SaveRestore | 0x1400959C0 | None (Bug Class 3) | N/A | MOV RAX,RSP — unhookable |

---

## Update Reimpl — Step-by-Step Status

### Simulation Loop (Steps 1–3)

| Step | What | Status | Notes |
|------|------|--------|-------|
| 1 | UI::PausesGame check | Working | |
| 2 | Speed multiplier from EntityUI | Working | |
| 3a | Game::UpdateTimedEvents | Working | |
| 3b | AnimationManager::Update | Working | rootOnly flag set correctly |
| 3c | EntityList::Update | **Custom reimpl** | Original hangs (spinlock). Manual 5-bucket iteration works. Tree node iteration skipped. |
| 3d | DayCounter increment | Working | |
| 3e | Weather::ResetTime | Working | |
| 3f | CitizenList::Update | Working | |
| 3g | Tutorial frame/time increment | Working | |
| 3h | Global frame index increment | Working | |

### Achievement Check (Step 4)

| Step | What | Status | Notes |
|------|------|--------|-------|
| 4 | AchievementInstance::Update/IsMet/HasFailed | Working | |
| 4b | AchievementManager::Update | **DISABLED** (`if (false)`) | Crashes in FinishTutorialInstance::Slot5 — null vtable entry. Needs investigation. |

### Post-Simulation (Steps 5–17)

| Step | What | Status | Notes |
|------|------|--------|-------|
| 5 | EntityList::PostUpdate | Working | |
| 6 | Camera transform (CalcTransform → SetCameraToWorld) | Working | |
| 7 | AmbientSound::Update | Working | |
| 8 | WorldGeometry::Update (vtable call) | Working | |
| 9 | UI::UserInterface::Update | Working | Returns cursor index |
| 10 | EntityUI::Update (player tool tick) | Working | |
| 11 | Set cursor from KeyMap | Working | |
| 12 | Audio::Scene::Update | Working | Mangled name fixed (was NULL) |
| 13 | Quicksave/Quickload key check | **DISABLED** (skipped) | KeyMap triple-deref crashes — null field at +0x10. Not needed for gameplay. |
| 14 | SaveLoad::Update | Working | |
| 14b | Sun direction normalize | **Inline math** | fn_Vector3_Normalize had wrong ABI (returns by value, hidden retval ptr). Replaced with inline sqrtf. |
| 14b+ | Pass sun dir to Scene::Clean | **TODO** | Normalized dir computed but not used — original likely passes it to Graphics::Scene::Clean |
| 15 | Graphics::Scene::Clean | Working | |
| 16 | Image capture check | Working | |
| 17 | Next loop transition | Working | |

---

## EntityList::Update — Custom Reimpl Details

The original `EntityList::Update` (0x1400c4090) hangs during gameplay. Our reimpl:

- [x] Accumulate time (`*elp += deltaTime`)
- [x] PreUpdate tree walk (call FUN_1400c3bd0)
- [ ] Tree node iteration (param_1[0xd] binary tree, calls FUN_1400c2de0 per node) — **SKIPPED**
- [x] 5-bucket round-robin entity iteration
- [x] Component vtable[+0x48] Update calls per entity
- [x] Deleted entity flag check (0x400)
- [x] Time delta check (skip if ≤ 0)
- [x] Update flag check (bit 3 = 0x8)
- [x] InternalBasePtr::CheckPtr on entity pointers
- [x] Update last-update timestamp

**Open question:** Why does the original function hang? Could be the tree node iteration part we skip, or a subtle difference in bucket handling. Our version works correctly without it.

---

## Disabled/Stubbed Functions — Fix Priority

### High Priority
- [ ] **AchievementManager::Update** — crashes in FinishTutorialInstance::Slot5 (null vtable). Tutorial achievements may not complete without this.
- [ ] **Draw reimpl** — currently passthrough. Reimpl written but crashed in VideoDX11 (RCX=0 null pointer to renderer). Need to debug parameter passing.
- [ ] **Startup reimpl** — currently passthrough. Reimpl written but crashes on ExternalList::Find (null fptr or wrong ABI). Dialog creation code needs fixing.

### Medium Priority
- [ ] **Shutdown reimpl** — currently passthrough. Reimpl is written and ready, but should be paired with Startup (both or neither).
- [ ] **EntityList tree node iteration** — skipped in our reimpl. May affect entity lifecycle (add/remove during gameplay).
- [ ] **Sun direction → Scene::Clean** — computed but not passed. May affect lighting/shadow updates.

### Low Priority
- [ ] **Quicksave/Quickload keys** — KeyMap triple-deref chain broken. Not critical for gameplay testing.
- [ ] **Load reimpl** — 759 instructions, large function. Passthrough works fine.
- [ ] **Destroy reimpl** — scalar deleting destructor. Passthrough works fine.
- [ ] **SaveRestore** — unhookable (Bug Class 3, MOV RAX,RSP). Would need vtable patch like Update/Draw.

---

## Resolved Bugs (for reference)

| Bug | Root Cause | Fix |
|-----|-----------|-----|
| Draw crash (VideoDX11 null RCX) | Wrong parameters to renderer | Made Draw passthrough |
| Startup crash (EXEC of 0x0) | fn_ExternalList_Find null fptr | Made Startup passthrough |
| EntityList::Update hang | Original function spinlocks | Custom bucket iteration reimpl |
| AchievementManager crash | FinishTutorialInstance::Slot5 null vtable | Disabled with `if (false)` |
| Audio::Scene::Update crash | Wrong mangled name (`@3@` → `@4@`, void → bool) | Fixed mangled name |
| Vector3::Normalize crash | Returns by value (hidden retval ptr ABI mismatch) | Inline math replacement |
| KeyMap crash (READ of 0x10) | Triple-deref chain hits null | Skipped quicksave/quickload |
| 7 NULL ResolveExport ptrs | Wrong MSVC mangled names | Fixed via objdump verification |
