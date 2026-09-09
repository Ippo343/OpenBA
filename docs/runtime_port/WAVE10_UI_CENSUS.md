# Wave 10 — driving the UI namespace census to 100%

**Opened 2026-09-08 on `wave10/base` @ 3318690.** This is the tracking doc for
the UI census push. It is deliberately a DECOMPOSITION, not a to-do list: the
headline number ("UI is 29 short") is not 29 pieces of UI work, and anyone who
reads it that way will scope the wave wrong.

Baseline, from `census_20260908_corrected.txt` (the FIXED tool — `census.txt`
in the same directory is the superseded scoping snapshot):

```
UI    total 1625    called 164    ported 135    detour 1    82.3%
```

Gap = 29. Regenerate with:

```
tools/ghidra_mcp/.venv/Scripts/python.exe tools/runtime_census.py --unported --namespace UI
```

---

## 1. The gap, decomposed by BLOCKER CLASS

| # | class | rows | who owns it |
|---|---|---|---|
| A | **DATA, not code** — 4 `??_7` vtables + 9 `?_typeInfo@` | 13 | lane **UIDATA** |
| B | **Allocator NO-GO** — body allocates, the DLL's unported dtor frees | 8 | gate: lane **ALLOC**; bodies: round 2 |
| C | **Split brain → needs a detour** | 2 | lane **UI1** |
| D | **Not replayable → needs the predictive oracle** | 2 | lane **UIORACLE** |
| E | **Claimed unreachable** | 3 | lanes **UI1** (ShowDialog) / **UI4** (SpriteCapture) |
| F | **`Serialize@ElementDescription`** — gate answered, recipe written | 1 | lane **EXTPTR** |
| | | **29** | |

### A — the 13 DATA rows

`??_7DialogController`, `??_7DialogInstance`, `??_7Element`, `??_7SpriteSheet`,
and `?_typeInfo@` of `CheckElement`, `ClipRegionElement`, `Dialog`,
`DialogInstance`, `ImageElement`, `LabelElement`, `NumberEditElement`,
`ScrollElement`, `TextElement`.

All 13 are ALREADY minted in `standalone/rt_data_mint_tables.inc`. They have no
body to reimplement. They are gated behind `BANISHED_RT_MINT=1`, and by
`runtime_census.py`'s own rule a gated registration is not a port — so they sit
in the gap permanently without ever being UI work. **Moving them is a CUTOVER
decision, not a porting one.**

### B — the 8 allocator-gated rows

`??0DialogInstance`, both `?AddController@DialogInstance@`, `?StartTimer@DialogInstance@`,
`?CreateInstance@ElementDescription@`, `??1UserInterface@`, `?CloseDialog@UserInterface@`,
`?SaveAllSettings@UserInterface@`.

**The recorded NO-GO is suspected STALE and is being re-tested.** Wave 9 lane E
reached "GATE STEP 2 MET — READY TO ARM"; the retire-then-guard ordering fix
landed; `standalone/main.cpp:1696/1703` wire `RtAllocSwapInstall` and the cutover
armer; `rt_alloc_swap.cpp` retires the `allocator_lock.cpp` prologue patches. It
**armed on the DLL-ABSENT boot** (`[RtAlloc] ARMED (cutover: …) -- 27 exports`).
What has never been done is a DLL-PRESENT `BANISHED_RT_ALLOC=1` run. That is
lane ALLOC's entire deliverable.

### C, D, E, F

See the per-lane docs. Every one of these blockers is being re-derived from
scratch rather than inherited — in this project, recorded blockers have gone
stale three-for-three in a single session
(`reference_holdback_blockers_are_stale_check_them_first`).

---

## 2. Round 1 lanes (dispatched 2026-09-08)

| lane | branch | scope |
|---|---|---|
| **ALLOC** | `wave10/lane-alloc` | GO/NO-GO on `BANISHED_RT_ALLOC=1` with the DLL PRESENT. Gates class B. |
| **UI1** | `wave10/lane-ui1` | `UserInterface::Update` / `Draw` via detour; `ShowDialog`; `CloseDialog` |
| **UIDATA** | `wave10/lane-uidata` | the 13 minted DATA symbols — can they go live? |
| **UIORACLE** | `wave10/lane-uioracle` | `AddTab@TabElement`, `CacheText@TextElement` |
| **UI4** | `wave10/lane-ui4` | `SpriteSheet` vtable, `AddSprite`/`AddScissor@SpriteCapture` |
| **EXTPTR** | `wave10/lane-extptr` | the one 748-byte `ExternalPtr<T>::Serialize` body → `Serialize@ElementDescription` |

Round 2 is class B's eight bodies, and is scheduled ONLY on lane ALLOC's verdict.

---

## 3. Standing rules for every lane in this push

* One worktree per lane, and the report must state its **PATH and branch**.
  Telling N lanes to "cut branch X" without a private worktree each makes them
  branch-switch in a shared checkout
  (`reference_parallel_lanes_need_private_worktrees`).
* Every deploy/run goes through `tools/game_dir_lock.sh`. All lanes share ONE
  `C:/GOG Games/Banished`; an unlocked run silently measures another lane's
  binary. Never background a blocking acquire — the lock is held from the moment
  it is GRANTED, not when the holder notices.
* Poll boot with `tools/boot_state.sh --wait 300`, never a grep-until-timeout.
* **Grep for the port before writing it**, by ADDRESS *and* by MANGLED NAME. A
  body ported but never REGISTERED is indistinguishable from an absent one to
  the census, and a duplicate registration is last-binding-wins.
* Read the oracle's `calls=` column, not just `divergences=`.
* The `RT_DIFF_SHADOW` sites are SELF-CHECKS, not oracles. The predictive oracle
  is `runtime/funcs/rt_predict.h`.
* No lane edits after it reports.
