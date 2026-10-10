# Integrator queue — code-fn wave

Edits requested by lanes that touch SHARED files. Applied in one batch, with one
build, after all six lanes report. Nothing here is applied yet.

## Lane 3 — delete 8 vestigial `fn_*` pointers (VERIFIED by integrator)

Independently confirmed before queueing: each identifier has exactly 3 source
mentions (defn / resolve / decl) and **no dereference anywhere**, and each of the
7 dead addresses has **exactly one** `ResolveAddr` site — the dead pointer
itself. So deleting them removes 7 census rows and changes zero instructions.

| identifier | addr | defn | resolve | decl |
|---|---|---|---|---|
| `fn_SelectResourceUI_GetResourceCount` | 0x140078a40 | cpp:409 | cpp:2623-2624 | h:1232-1234 |
| `fn_TownHallUI_InitProfessions` | 0x140080160 | cpp:716 | cpp:3554-3555 | h:2551 |
| `fn_TownHallUI_InitProduction` | 0x14007fc30 | cpp:717 | cpp:3556-3557 | h:2552 |
| `fn_TownHallUI_InitInventory` | 0x14007f660 | cpp:718 | cpp:3558-3559 | h:2553 |
| `fn_TownHallUI_InitAgriculture` | 0x14007ebf0 | cpp:719 | cpp:3561-3562 | h:2554 |
| `fn_TradeUI_RefreshDisplay` | 0x1400846b0 | cpp:765 | cpp:3651-3652 | h:2679-2682 |
| `fn_CitizenList_ShuffleEntities` | 0x140089470 | cpp:745 | cpp:3606-3607 | h:2613-2616 |
| `fn_CitizenBehavior_ShuffleSchools` | 0x1400896a0 | cpp:1122 | cpp:4847-4848 | h:5295-5297 |

All in `engine/funcs/engine_funcs.{cpp,h}`. **Delete bottom-up per file or match
on the identifier** — the line numbers are declaration-order sensitive.

### Do NOT touch
- `s_optimize89470` (`code/Game/Game/town_workplace_registry.cpp:56/64/238`) —
  the LIVE, deliberately-gated consumer of 0x140089470. Verified independent of
  the dead `fn_CitizenList_ShuffleEntities`; deleting that pointer cannot disable
  the gated feature.
- `engine_funcs.h:2535-2538` — the four `void TownHallUI_Init*(TownHallUI*)`
  prototypes for the REAL ports in `townhallui.cpp`. These stay.

### Note on overriding a stated intent
`engine_funcs.h:2679-2680` says the TradeUI pointer is "kept for reference /
fallback only". There is no fallback code — the comment is stale in exactly the
way the two corrected earlier in this branch were. Deleting deliberately, not
silently.

### Expected census effect
7 rows disappear. **0x140089470 will REMAIN** in the census whenever
`BANISHED_LIVE_SEAT_SHUFFLE` is off, because `s_optimize89470` still resolves it.
That is correct — it is a genuine deliberate-leave and should stay visible.

---

## Lane 1 — 1 port, 1 already-done, 3 dead, 4 deliberate-leave

### Apply: one line pair in `standalone/address_registry.cpp`
Next to the other batch decls/calls:
```
extern "C" void RegisterCodeLane1Batch();
RegisterCodeLane1Batch();
```
New files (lane-owned, no integrator edit needed):
- `code/Game/Game/unreg_lane1_mapgenerator_dtorbody.cpp`
- `standalone/reg_code_lane1.cpp`

### The one real gap: `0x14001aea0` `MapGenerator::~MapGenerator` (dtor body)
VERIFIED by integrator: `code/Game/Game/mapgenerator.cpp:74` is
`if (s_fn_DtorBody) s_fn_DtorBody(self);` — a null-guard-and-SKIP, so registering
RESTORES retail. Consequence is worse than a leak: `MapGenerator_Cleanup`
(`0x14001ae30`) calls `Dtor(self, 0)` and then re-runs Init, so with the body
skipped it **overwrites live array headers instead of releasing them**.
Lane reports closure verified — all five callees already registered.

### Pending question sent back to the lane
`0x140002280` `Entity::FindComponent<AmbientEmitterComponent>` is filed
`deliberate — leave`, but the lane's own "if registered" column says it would make
ambient/fire/tornado audio start working AND could fault on an unallocated
`+0x38`. That is a `needs a run to decide`, not a `leave`. Awaiting the lane's
answer on which run settles it.

### Lane also recommends deleting two more dead pointers
`fn_StringHash` (0x14001b1d0 — unused AND misnamed) and `fn_ToolbarSort`
(0x140021790). Not yet independently verified by the integrator; verify with the
same 3-mentions/no-deref + single-ResolveAddr test used for lane 3 before applying.

---

## CORRECTION TO THE WAVE'S OWN ADDRESS LIST (integrator error)

`0x1400192a0` is **already registered** — `standalone/reg_unreg_lane1.cpp:220`,
committed in `a8a24ba`, which `git merge-base --is-ancestor` confirms predates
`f119f3b`. It was already fixed on the tree when the lane split was built.

Cause: the split was the **union** of `held_back_code_fns.txt` (49) and the
AUTOLOAD census (48). The census correctly omitted this address *because it is
registered*; the union resurrected it from the stale held-back file, and it was
then hand-tagged LOAD-WINDOW on the strength of an older tutorial census.

**The census is authoritative; `held_back_code_fns.txt` is a pre-`a8a24ba`
artifact.** The real total for this wave is **48**, not 49. Do not re-derive the
union — regenerate from the census alone.

---

## Lane 4 — 1 LIVE CRASH, 5 free ports, 2 leaves (harm)

### ⚠ `0x140095da0` — `GameUpdateLoop::TakeScreenshot`. VERIFIED LIVE CRASH.
**Not a QuickSave handler.** `standalone/reg_mods.cpp:30` names it
"ModDialog_impl_6010_sub_5da0 = GameUpdateLoop QuickSave handler" — a misnomer;
lane 4's decompile says `TakeScreenshot` (allocs a `FrameCopyImageBuffer`, grabs
the backbuffer, walks `<saveDir>\Screenshots\Screenshot%i.<ext>`, then clears
both flags with a 2-byte store to `self+0xC0`).

Integrator-verified chain, end to end:
- `fn_QuickSave` resolves `0x140095da0` (`gameupdateloop.cpp:526-527`) → null.
- Called **UNGUARDED** at `gameupdateloop.cpp:2060` and `:2135`.
- Flags set from `KeyMap::Pressed(0x21)` / `(0x22)` at `:1645-1648`.
- `CHECK_FN` is only `TRACE("... is NULL!")` (`:675`) — non-fatal.

**Pressing quicksave or quickload in-game is an EXEC-of-0 crash in the default
build.** Highest-priority item in the wave. Also fix the name in `reg_mods.cpp:30`.

### Other lane 4 rows
| addr | what | verdict |
|---|---|---|
| `0x140089ed0` | `CitizenBehavior::CountResources` | port (free — zero live consumers) |
| `0x14008a730` | `CitizenBehavior::CollectStats` | port (free) |
| `0x14008c100` | `CitizenBehavior::CheckThresholds` | port (free) |
| `0x1400932a0` | `Game::GetMonthFromTime` | port (free) |
| `0x140099c20` | `KeyMap::BuildKeyName` | port (free) — **needs an ABI shim**: port is `(retval, device, key)`, retail is `(container, retval, device, key)` returning RDX |
| `0x1400938d0` | `Weather::DrawUpdate` | **leave — HARM**: would double-write fog/sun/cbuffer; lane re-verified the "Environment.rsc not loaded" claim is NOT stale |
| `0x14009e8c0` | `Game::BuildPathfindingSystems` | **leave — HARM**: `gameupdateloop_load.cpp:1562-3` already does the `+0x68/+0x70/+0x78` subset; registering builds all three twice and leaks the seeded objects |

---

## Lane 6 — 5 register-only ports, 3 leaves (all active harm)

New file `standalone/reg_code_lane6.cpp` (registration only; all five ports exist).

| addr | what | verdict |
|---|---|---|
| `0x1400fc6b0` | `DragTool::OnKey` | port (register-only; `fn_DragTool_OnKey` has zero callers) |
| `0x140101ce0` | `EntityUI::OpenPauseMenu` | port (register-only; `fn_Toolbar_ToggleAll` is a misnomer, zero callers) |
| `0x14011b560` | `DecalComponent::Draw` | port, gated `BANISHED_LIVE_DECAL` to match the vtable gate |
| `0x14012bbd0` | `StatusIconComponent` icon positioning | port, gated `BANISHED_LIVE_STATUSICON_DRAW` to match the slot-8 install |
| `0x14012db70` | `ToolbarDescription::~ToolbarDescription` | port (register-only, **via adapter**) |
| `0x1400fcc90` | `EditTool` deselect + close info dialog | **leave**: a live cc90 would deselect + wipe the outline EVERY frame, because our `0x140100380` screen-rect helper fails for citizens. The guard IS the feature. |
| `0x140101ee0` | `EntityUI::Startup` | **leave — HARM**: would double-insert the 8 tool-mode AVL nodes |
| `0x1401026a0` | `EntityUI::Update` | **leave — HARM**: would double-drive camera and tool every frame |

---

## INTEGRATOR DECISION NEEDED: delete vs register a zero-consumer pointer

Lanes 3 and 4 reached opposite recommendations on the same situation — a `fn_*`
that resolves an address and is **never dereferenced**:
- **Lane 3** → DELETE the pointer (removes dead code and the census row).
- **Lane 4** → REGISTER it ("port (free)", makes the dead pointer truthful).

Both silence the census. Ruling for this wave, to keep the tree consistent:

- **Zero consumers anywhere → DELETE.** Registering a function nothing calls adds
  registry weight and preserves a misleading "we support this" signal.
- **Any real consumer — including a `_DEBUG`-only or feature-gated one → REGISTER**
  per the lane's argument (this is lane 6's decal/statusicon shape, which is NOT
  the same situation).

Apply the same 3-mentions/no-deref + single-`ResolveAddr` verification used for
lane 3 before deleting anything.

## Still to verify before applying
- `fn_StringHash` (0x14001b1d0) and `fn_ToolbarSort` (0x140021790) — lane 1's
  extra deletion candidates, not yet integrator-verified.
- Lane 1 flag: `entity_finders_disaster.cpp:50`'s stub comment is STALE — it
  blames an `AmbientEmitterComponent::OnAdded` that has been live and default-on
  since `1b7a50b` (2026-06-30). Rewrite with the measured cause.
- Lane 1 nit (latent, unreachable today): `SubArrayDeepCopy`
  (`serialize_struct60b.cpp:76-82`) zeroes `dst->data` and `dst->count` but not
  `dst->capacity`; retail's `param_1[1] = 0` clears count AND capacity as one qword.

## Queued run (no risky registration; cannot crash)
`0x140002280` `Entity::FindComponent<AmbientEmitterComponent>` — lane 1 upgraded
this from `leave` to **needs a run to decide** on challenge. The unguarded retail
deref needs `+0x38` non-null AND the description to declare **at least 2**
emitters (a710 asks for emitter index 1, reads `entry[1].handle` at stride 0x10).
Run: `mingw32-make sa-run` with `BANISHED_AMBIENT_TRACE=1`, after raising the
16-line cap at `ambientemittercomponent.cpp:283`. If every workplace line shows
`count>=2`, register it and ambient/fire/tornado audio comes alive. If any shows
0 or 1, keep it unregistered and rewrite the note with the measured count.
