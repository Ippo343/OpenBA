# GameUpdateLoop — Detailed Reimplementation Status

Moved from the old `game_goals.md` for reference. See `game_goals.md` for the module overview.

---

## Vtable Methods (8 slots)

| Slot | Method | Address | Hook Type | Status | Notes |
|------|--------|---------|-----------|--------|-------|
| +0x00 | Destroy | 0x140019DD0 | Prologue (19B) | Passthrough | Scalar deleting destructor |
| +0x08 | Startup | 0x140097C00 | Prologue (14B) | Passthrough | Reimpl written but crashes on ExternalList::Find |
| +0x10 | Shutdown | 0x1400977C0 | Prologue (12B) | Passthrough | Reimpl written, waiting on Startup |
| +0x18 | **Update** | 0x140096380 | **Vtable patch** | **REIMPL LIVE** | Main simulation tick, working |
| +0x20 | Draw | 0x140096010 | Vtable patch | Passthrough | Reimpl written but crashes in VideoDX11 |
| +0x28 | DrawFailed | 0x140096000 | None (3B) | N/A | Just a `ret` |
| +0x30 | Load | 0x140096B00 | Prologue (16B) | Passthrough | 759 instructions |
| +0x38 | SaveRestore | 0x1400959C0 | None (Bug Class 3) | N/A | MOV RAX,RSP — unhookable |

---

## Update Reimpl — Step-by-Step Status

### Simulation Loop (Steps 1-3)

| Step | What | Status |
|------|------|--------|
| 1 | UI::PausesGame check | Working |
| 2 | Speed multiplier from EntityUI | Working |
| 3a | Game::UpdateTimedEvents | Working |
| 3b | AnimationManager::Update | Working |
| 3c | EntityList::Update | Custom reimpl (5-bucket) |
| 3d | DayCounter increment | Working |
| 3e | Weather::ResetTime | Working |
| 3f | CitizenList::Update | Working |
| 3g | Tutorial frame/time increment | Working |
| 3h | Global frame index increment | Working |

### Achievement Check (Step 4)

| Step | What | Status |
|------|------|--------|
| 4 | AchievementInstance::Update/IsMet/HasFailed | Working |
| 4b | AchievementManager::Update | DISABLED — crashes in FinishTutorialInstance::Slot5 |

### Post-Simulation (Steps 5-17)

| Step | What | Status |
|------|------|--------|
| 5 | EntityList::PostUpdate | Working |
| 6 | Camera transform | Working |
| 7 | AmbientSound::Update | Working |
| 8 | WorldGeometry::Update | Working |
| 9 | UI::UserInterface::Update | Working |
| 10 | EntityUI::Update | Working |
| 11 | Set cursor from KeyMap | Working |
| 12 | Audio::Scene::Update | Working |
| 13 | Quicksave/Quickload keys | DISABLED — KeyMap null deref |
| 14 | SaveLoad::Update | Working |
| 14b | Sun direction normalize | Inline math |
| 14b+ | Sun dir → Scene::Clean | TODO |
| 15 | Graphics::Scene::Clean | Working |
| 16 | Image capture check | Working |
| 17 | Next loop transition | Working |

---

## Resolved Bugs

| Bug | Root Cause | Fix |
|-----|-----------|-----|
| Draw crash | Wrong renderer parameters | Made passthrough |
| Startup crash | fn_ExternalList_Find null fptr | Made passthrough |
| EntityList hang | Original spinlocks | Custom bucket iteration |
| AchievementManager crash | Null vtable in FinishTutorialInstance | Disabled with `if (false)` |
| Audio::Scene::Update crash | Wrong mangled name | Fixed mangled name |
| Vector3::Normalize crash | Hidden retval ptr ABI mismatch | Inline math |
| KeyMap crash | Triple-deref hits null | Skipped quicksave/quickload |
| 7 NULL ResolveExport ptrs | Wrong MSVC mangled names | Fixed via objdump verification |
