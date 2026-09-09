# Wave 10 — driving the UI namespace census to 100%

> ## ✅ CLOSED 2026-09-09 at `730c544` — **UI 170 / 164 = 96.5%** (from 82.3%)
> TOTAL 1083 / 869 = 80.2%. Twelve lanes, all merged.
> Integration verified on the tip: reached gameplay (37,506 component draws),
> clean shutdown, **0-byte crash log, 0 aborts, `[RtResolve] 0 misses`**;
> debug, release AND perf targets all build clean.
>
> **Read the caveat with the number** (lane DLG2): a per-namespace census is a
> **LOWER BOUND** on residue. UI read 22→20 with nothing entering while the FULL
> census read 228→228, because two unexported callees entered as
> `addr/internal` and `--namespace UI` cannot see an unexported callee at all.

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
body to reimplement.

**ANSWERED 2026-09-08 by lane UIDATA — the verdict is NO for all 13, and the
reason REFRAMES the class.** It is not a cutover decision after all; it is
blocked on concrete UI porting work, and the global `BANISHED_RT_MINT=1` switch
was never a route to it (that switch does not even boot with the DLL present —
it publishes our zeroed `?_singleton@MemoryAllocator@System@@` over the DLL's
live one and dies in `EnterCriticalSection` on a null self; that blocker lives
in the 38 storage rows and is not a UI fact).

* **The 4 vtables are blocked on 27 ABORT TRAPS** across 42 non-purecall slots
  — `DialogController` 9, `Element` 8, `SpriteSheet` 8, `DialogInstance` 2.
  Registering a vtable is NOT passive: our ported ctors *stamp* these tables, so
  the DLL's own `UserInterface::Update` would dispatch into `abort()`.
  **So class A converts into 27 named UI function ports** — real, scopeable UI
  work, which is the opposite of the "no UI work can move these" reading.
* **The 9 typeinfos fail SILENTLY, which is worse.** Our minted `typeId` is the
  ctor sentinel `0xFFFFFFFF` against the DLL's real CRC, and stays so because
  our nodes sit on `g_mintTypeInfoHead`, a chain only the mint knows. Readers
  take exactly that dword. `reg_element_typeids.cpp` is default-on and already a
  live consumer of six of the nine, and its header records that the DLL route
  was chosen precisely so no window holds `0xFFFFFFFF`. Registering
  `?_firstTypeInfo@` does not rescue it — the DLL reads its own head
  RIP-relative, so the split brain just moves.

Lane UIDATA added a per-vtable READY/BLOCKED readiness table to
`RtDataMintShadowReport` (shadow-only): **15 of 55 vtables READY, none of them
UI**. Two blockers for the integrator: `RegisterMintedData()` has **no per-row
selectivity** (one switch over all 197 rows), so promoting a single row needs
that built first; and `runtime_census.py` prints that minted rows "cannot move
the columns above at all, in either direction" while all 13 DO appear under
`--unported` — `MINT_FILE` is excluded from the ported side and not the called
side. That contradiction is why these 13 read as UI work.

### B — the 8 allocator-gated rows

`??0DialogInstance`, both `?AddController@DialogInstance@`, `?StartTimer@DialogInstance@`,
`?CreateInstance@ElementDescription@`, `??1UserInterface@`, `?CloseDialog@UserInterface@`,
`?SaveAllSettings@UserInterface@`.

**GATE OPENED 2026-09-08 — lane ALLOC verdict GO**, merged to `wave10/base`.
`BANISHED_RT_ALLOC=1` now arms with the DLL PRESENT: 27 exports, atomic swap
taken, `allocs=901685 frees=540750` through our own bodies (the effectiveness
control against the `calls=0` trap), and a redzone stress town — 3,000 frames,
802 citizens, 23,705 entities — with ZERO violations and `[RtResolve] 0 misses`.

The blocker was an ORDERING DEADLOCK, not a design problem: the allocator lock
installs at `main.cpp:1688`, before `RuntimePortInit()`, so both prologues were
patched when the arming guard ran — and `AllocatorLockRetireForPort()`, the step
that satisfies the guard, lived INSIDE the detour installer, downstream of it.
**The guard prevented reaching the thing that would satisfy it.** And WAVE9.md
recorded the cure as *"done with a third seam (`RtSystemAllocatorSetLockRetirer`)"*
— **a symbol that did not exist anywhere in the tree**. A prescription written in
the past tense, read as a completion for a whole wave
(`reference_a_prescription_read_as_a_completion`). WAVE9.md's cited DLL-present
pass run is likewise an unarmed clean-heap control: its log shows the lock held
the prologues throughout and no swap lines at all.

`BANISHED_RT_ALLOC` remains OPT-IN and off by default, so every lane porting one
of these bodies must verify BOTH armed and unarmed, and say which conclusions
depend on the armed configuration.

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

---

## 4. Progress log (2026-09-08 → 09)

| after | UI called/ported | % | what moved |
|---|---|---|---|
| baseline | 164 / 135 | 82.3% | — |
| EXTPTR | 164 / 136 | 82.9% | `Serialize@ElementDescription`; family found to be 25 bodies, not 3 |
| UI4 | 166 / 141* | — | `AddSprite`+`AddScissor`; SpriteSheet vtable 1/8 → 8/1 |
| UI1 | — | — | `UserInterface::Update` ported + detoured |
| UIF | 169 / 144 | 85.2% | `~UserInterface`, `SaveAllSettings`, `CreateInstance` |
| UI3 | 170 / 148 | 87.1% | the four `DialogInstance` rows |
| UIDRAW | 170 / 149 | 87.6% | `UserInterface::Draw`; census blind spot **7** (detour column read 2, truth was 4) |
| UIORACLE | 170 / 151 | **88.8%** | `AddTab`, `CacheText`, `SetParameterCount`, `Array<String>::SetSize` |

\* intermediate readings taken before later merges; the tool also gained fixes
mid-wave, so read the SET diffs in the lane docs, not this column alone.

**90% needs `ported ≥ 153` at `called = 170`.** Remaining gap is 19 rows:
2 code (`??0Dialog`, `??1Dialog` — lane DLG2), 1 code (`CloseDialog` — lane
CLOSEDLG), 1 code (`ShowDialog` — no route, see below), 15 data (lane MINTSEL).

### What is genuinely still blocked

* **`ShowDialog`** — lane UI1 established that `BANISHED_MODAL_ANSWER` is **not
  a route, it is a bypass**: both call sites skip `fn_UI_ShowDialog` entirely.
  A predictive oracle exists but would compare one flag bit. **What would expire
  it:** `ShowDialog` registers `UpdateStandAloneUI` into `MainLoop+0x90`, which
  the nested pump walks every frame — a harness callback in that array reaches
  inside the loop no driver can currently touch.
* **The 15 data rows** — see §1 class A. Not a switch anyone may flip; blocked
  on per-row selectivity (buildable), the `_typeInfo` CRC (a silent wrong value
  that live default-on code reads), and the remaining vtable abort traps.

## 5. Instrument work this wave — the part that is not a row count

Six defects were found IN THE INSTRUMENTS, several of which were producing
confident green readings over nothing:

1. **census blind spot 7** — detour patterns matched only `InstallRuntimeDetour*`
   front doors, so `RuntimeDetourInstall`, the installer the hardest prologues
   are REQUIRED to use, was invisible.
2. **`AddTab` was dead on arrival** — `engine_funcs.cpp` resolved it from a PE
   import slot FIRST, bypassing the port registry. Only `rounds=0` said so.
3. **`InstallDetour`'s `ADD RAX,8`** — wrong in every configuration, live and
   corrupting on `Engine::Startup`, whose trampoline the allocator shadow driver
   calls. The old self-test was structurally blind to it.
4. **`BANISHED_REDZONE=watch`** crashed at teardown; and `BANISHED_REDZONE=1`
   walks NOTHING unless `BANISHED_REDZONE_WATCH` is also set, so a bare-shim
   "0 violations" is vacuous.
5. **the allocator shadow gate** — Wave 9's zero-divergence result does not
   reproduce; all seven cells red. See `WAVE10_LANE_SHADOWRE.md`.
6. **`rt_predict.cpp` superseded on exact address+size**, so an overlapping
   different-width write produced false divergences.

Plus two in a lane's own probe (an effectiveness counter reading freed memory;
a verdict counting its control's collateral), and a pin pattern that matched
seven instructions and would have reported NOT-UNIQUE about a correct body.

**The pattern worth carrying:** in every case the instrument's failure mode was
to read GREEN, or to read confidently WRONG — never to fall silent. That is why
this wave scored effectiveness counts and negative controls separately from
verdicts, and why `calls=`/`rounds=`/`armed=` were read alongside
`divergences=` every time.


---

## 6. Wave close — what the 96.5% does and does not mean

**Does:** 164 of the 170 UI functions our tree resolves are now our own code,
each behind a kill switch, and the tree builds and plays on all three targets.

**Does not:** it is not a claim that all 164 are differentially verified against
retail. Three rows are explicitly **ported, not verified**, and say so in their
lane docs rather than in a footnote:

* `CloseDialog` — `armed=0`; no harness route both reaches the body and exits
  cleanly. `entries=0` was the finding, not a footnote.
* the two `AddController`s and the `DialogInstance` ctor — 40 shipped-byte pins
  and a live boot each, which is a pre-flight on the transcription, **not** a
  post-state comparison against retail.

**Still open, named:**

* `?ShowDialog@UserInterface@` — the only remaining code row. Expires when a
  harness callback can be placed in `MainLoop+0x90`, which the nested pump walks
  every frame.
* The four minted UI vtables — `??_7SpriteSheet` is **one slot** away (slot 5,
  `?Serialize@SpriteSheet@`); `??_7Dialog` is not in the mint tables at all.
* `0x180039280` / `0x180039360` — array dtors that are **NOT** the J1 family, as
  lane DLG2 proved against lane UI3's recipe. Porting them as J1 entries would
  have leaked under a false byte-identity claim.
* `autostart.cpp` checks `BANISHED_AUTOQUIT` before `BANISHED_AUTOSTART` and
  returns early, so "get in-game" and "quit cleanly" are mutually exclusive.
  Fixing that converts a whole class of UI-mutator coverage from a capability
  into a measurement.
* The mods click list **published in CLAUDE.md** crashes on an install with zero
  `.pkm` (`ModDialog_ToggleMod_Impl+0x43`, READ of `0xd0`). It is also the only
  live route to `??0Dialog`, so it caps that row's coverage.

## 7. The allocator, in one place

`BANISHED_RT_ALLOC` **arms** with the DLL present (lane ALLOC) — but arming is
still **not recommended on one lane's authority**, and it stayed unset in every
cell of every lane this wave. The two pillars under that GO were each knocked
down and rebuilt:

| pillar | knocked down by | restored by |
|---|---|---|
| redzone pass condition | `watch` crashed at teardown | REDZONE — instrument proved CAPABLE by injection, 41,349 walks / 0 violations |
| shadow gate | SHADOWRE — red in all 7 cells | SHADOWBISECT — cause was ONE C++ call; 26,516,271 ops / 0 divergences |

**The finding that outlives the allocator:** on one path, *"the allocator port is
off by default"* was **false**. An unarmed run was mutating retail's arena with
our ported free logic, by a route no kill switch reached and no counter
reported. A kill switch gates the REGISTRATION door; a body that calls the port
by C++ name never consults it.
