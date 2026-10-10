# WAVE 23 LANE GFXDATA — the 7 Graphics statics, re-derived

Branch `wave23/gfxdata` off `wave23/base` (`a60351e3`). Private worktree
(`.claude/worktrees/agent-a9528c0e3bc869e95`).

## 0. Denominator

**7 of 7 kickoff rows covered.** All seven still read `called=0, ported=0` on this
branch's own re-derivation (`tools/runtime_census.py --csv gfxdata_w23.csv`, plain
`python` 3.12 — no `tools/ghidra_mcp/.venv` in this worktree checkout, same situation
Wave 21's GFXDATA lane hit), matching `WAVE23_KICKOFF.md` §0 exactly:
`TOTAL 4632 1314 called 1137 ported 9 detour 86.5%`. **The kickoff is NOT stale for
these seven rows** (rule 1).

```
?_frameIndex@ParticleInstance@Graphics@@1HA
?_rng@Particle@Graphics@@1VRandom@System@@A
?_rootOnlyUpdates@MeshInstance@Graphics@@0_NA
?_singleton@AnimationManager@Graphics@@0PEAV12@EA
?_typeInfo@AnimationEvent@Graphics@@0VBaseTypeInfo@Object@@A
?_typeInfo@MaterialInstance@Graphics@@0VBaseTypeInfo@Object@@A
?_typeInfo@Particle@Graphics@@0VBaseTypeInfo@Object@@A
```

Two of the nine rows Wave 21's GFXDATA lane examined (`?_typeInfo@GeneratedImageBuffer@`
and `?_typeInfo@Material@Graphics@@`) are **absent from this list because Wave 21
promoted both** (`standalone/reg_mint_promoted.cpp:950-958`) — confirmed still promoted
on this tip, not re-litigated here.

## 1. Grep before porting (rule 2) — nothing was hiding

Grepped `runtime/`, `engine/`, `code/`, `standalone/` (including
`standalone/rt_data_mint_tables.inc`) for all seven mangled names. Findings, all
consistent with the census:

- All seven are already **minted** in `standalone/rt_data_mint_tables.inc` (storage
  exists, `rt_mint_gen.py --check` reports UP TO DATE — 62 vtables, 104 BaseTypeInfo,
  39 storage, unchanged by this lane).
- The four plain statics (`_frameIndex`, `_rng`, `_rootOnlyUpdates`,
  `_singleton@AnimationManager`) carry `RT_UNPROMOTABLE ... HARMFUL` tags in
  `reg_mint_promoted.cpp:1359-1362`, dated `WAVE17_LANE_CEIL.md §3.2`, and are also
  listed `NONE, EVER` in `tools/unpromotable_slot_check.py:306-315`.
- The three `?_typeInfo@` rows are **read** (not written) by
  `runtime/src/graphics/graphics_typeinfo_accessors.cpp` — a **Wave 21 lane GFXVT**
  file that ported the `Type`/`TypeName`/`GetFileVersion` vtable-slot bodies for
  `AnimationEvent`, `MaterialInstance` and `Particle`, reading each node through
  `RT_DATA_SLOT` by name. This is new since Wave 21's GFXDATA report (which could not
  see it — GFXVT was a separate, unmerged worktree at the time). No body here writes
  any of the three nodes; it is a reader-side consumer, addressed below.
- No body/mint anywhere is unregistered — nothing here reproduces
  `reference_half_registered_port_reads_as_absent.md`.

**Re-checked the Wave 21 GFXDATA blocker (rule 3):** `WAVE21_LANE_GFXDATA.md` §3.4 left
one explicit, named follow-up: `?_typeInfo@AnimationEvent@Graphics@@`'s attendance was
never wired into `standalone/wave17_door_attend.cpp`'s `g_watch[]` table. Checked —
still true on this tip (`grep -n "typeInfo@AnimationEvent" standalone/wave17_door_attend.cpp`
returned nothing before this lane's edit). Fixed in §3 below.

## 2. Task 1 — who reads/writes each static in Runtime-x64.dll (rule 6)

All eight `get_xrefs_to` calls below were run against **Runtime-x64.dll explicitly**
(`program=Runtime-x64.dll` pinned on every call — the profile DLL's offsets are known
to disagree and standalone loads retail, not the profile build).

### 2.1 `?_frameIndex@ParticleInstance@Graphics@@1HA` (`0x1801fe77c`)

| caller | kind | ported in our tree? |
|---|---|---|
| `IncrementInstanceIndex` (`0x1800201f0`) | READ_WRITE (one RIP-relative `add` in place) | **YES** — `RtGraphics_ParticleInstance_IncrementInstanceIndex`, `runtime/src/graphics/animation.cpp:582`, registered by name+address |
| `Update` (`0x1800a5642`) | READ | no |
| `Update` (`0x1801041ad`) | READ | no |

**Verdict: still HARMFUL, unchanged.** The one writer is now ported — but it writes
through `RtFrameIndexSlot()`, which is `RT_DATA_SLOT`-shaped and today resolves to the
DLL's own live address (unpromoted). The two remaining `Update` readers are **still
unported** and read the raw RVA directly. Promoting would move our writer's target to
mint storage while those two readers kept reading the DLL's now-frozen copy — the exact
split-brain kickoff rule 6 describes. **A partially-ported consumer set does not flip
the verdict**; every unported reader has to be gone first.

### 2.2 `?_rng@Particle@Graphics@@1VRandom@System@@A` (`0x1801fe7f0`)

~45 refs, all inside `Update` (`0x1800a5xxx`, alternating READ/WRITE — retail's own
particle simulation advances this RNG stream every tick) and `SetParticle`
(`0x180104a5e`/`70`/`b9`, READ/READ/WRITE). Grepped `runtime/src/graphics/animation.cpp`
for any port of `Particle::Update` or `Particle::SetParticle` — **neither exists**; both
callers are entirely unported.

**Verdict: still HARMFUL, unchanged, most clear-cut of the four** (unchanged from
Wave 17/21 — nothing in our tree touches either caller).

### 2.3 `?_rootOnlyUpdates@MeshInstance@Graphics@@0_NA` (`0x1801fc70b`)

| caller | kind | ported? |
|---|---|---|
| `Update` (`0x1801008d6`) | READ | no |
| `EnableRootOnlyUpdates` (`0x180017040`) | WRITE | no |
| `AreRootOnlyUpdateEnabled` (`0x180017050`) | READ | no |

Grepped for `EnableRootOnlyUpdates`/`RootOnlyUpdate` and both addresses across
`runtime/` — zero matches. **Verdict: still HARMFUL, unchanged** — nothing has moved
here at all since Wave 17.

### 2.4 `?_singleton@AnimationManager@Graphics@@0PEAV12@EA` (`0x1801fe7b0`)

20 refs. The two WRITEs:

| caller | kind | ported? |
|---|---|---|
| `AnimationManager` ctor (`0x1800fc490`) | WRITE | **YES** — `RtGraphics_AnimationManager_Ctor`, `animation.cpp:306` |
| `~AnimationManager` (`0x1800fc412`) | WRITE | **YES** — `RtGraphics_AnimationManager_Dtor`, `animation.cpp:356` |

18 READs, **all unported**: `AnimationState::{GetNodeToWorld x2, RootAnimationOffset,
Update, UpdateRootOnly, UpdateNodes, CopyInstancedToUnique, PlayAnimation x2,
ResetAnimationState, GetInstanceTransforms, InstancedRootAnimationOffset}`,
`MeshInstance::{GetBones, Draw, GetInstanceTransform}`, `AnimationManager::Get`, and one
unexported internal (`FUN_1800fc7d0`).

**Verdict: still HARMFUL, unchanged, on freshly re-derived evidence.** Same shape as
§2.1: both writers are ported (via `RtAnimMgrSingletonSlot()`, currently resolving to
the DLL's live address, unpromoted), and every one of the 18 readers remains unported
DLL code hard-coded to the raw RVA. Promoting would leave every `AnimationState`/
`MeshInstance` method reading a singleton the ported ctor/dtor no longer write.
**The route-scoped reversal named in `WAVE21_LANE_GFXDATA.md` §3.1** ("if a future
lane ports all of a symbol's in-DLL readers... the verdict reverses") has NOT
happened: `AnimationState`'s methods and `MeshInstance::Draw`/`GetInstanceTransform`
are all still unported Runtime bodies. Two of eighteen readers ported would not be
enough even if it had started — it would need all eighteen, plus `AnimationManager::Get`.

### 2.5 `?_typeInfo@AnimationEvent@Graphics@@0VBaseTypeInfo@Object@@A` (`0x1801f9978`)

Refs to the `+0x00` (`typeId`) field only — `+0x08` (`name`) is a different address,
not queried by a caller here:

| caller | kind | ported? |
|---|---|---|
| `Type` (`0x1800116d0`) | READ | **YES** — `RtGraphics_AnimationEvent_Type`, `graphics_typeinfo_accessors.cpp:162` |
| `TypeId` (`0x1800116e0`) | READ | no (distinct exported function; not the same slot GFXVT ported) |
| `FUN_1800594a0` (2 refs) | READ | no, unexported internal |
| `FUN_180059b20` | READ | no, unexported internal |
| `FUN_1800598d0` | READ | no, unexported internal |

**Zero writers besides the static's own CRT init** — same content-only shape as the
two rows Wave 21 already promoted (`reference_basetypeinfo_name_is_content_only.md`).
**Verdict: READY, DLL-present route.** Promotion is binary-safe today exactly as it was
in Wave 21; what is missing is **attendance evidence**, addressed in §3.

### 2.6 `?_typeInfo@MaterialInstance@Graphics@@0VBaseTypeInfo@Object@@A` (`0x1801fa778`)

| caller | kind | ported? |
|---|---|---|
| `Type` (`0x1800027f0`) | READ | **YES** — `RtGraphics_MaterialInstance_Type` |
| `TypeId` (`0x180002800`) | READ | no |
| `FUN_180014520` (2 refs), `FUN_180014c10`, `FUN_180014f10`, `FUN_18005d4a0` (2 refs) | READ | no, unexported internals |

One `[DATA]` ref at `0x180149587` (a pointer table entry, not a code read). **Zero
writers besides CRT init.** **Verdict: READY, DLL-present route** — identical shape to
Wave 21's finding, re-confirmed on this tip's binary.

### 2.7 `?_typeInfo@Particle@Graphics@@0VBaseTypeInfo@Object@@A` (`0x1801fa7b0`)

| caller | kind | ported? |
|---|---|---|
| `Type` (`0x18001ff10`) | READ | **YES** — `RtGraphics_Particle_Type` |
| `TypeId` (`0x18001ff20`) | READ | no |

One `[DATA]` ref at `0x1801495f7`. Shortest reader list of the three — **zero writers
besides CRT init.** **Verdict: READY, DLL-present route**, re-confirmed.

## 3. Task 2 — what was actually minted/published this lane

**Nothing new was minted.** All seven rows were already minted in Wave 17/21; this
lane's job was re-verification (rule 3) plus closing Wave 21's one **named** follow-up.

One edit made: added the missing watch entry Wave 21 GFXDATA flagged in its §3.4 —
`standalone/wave17_door_attend.cpp`'s `g_watch[]` table now includes
`?_typeInfo@AnimationEvent@Graphics@@0VBaseTypeInfo@Object@@A` (it previously watched
only the `??_7AnimationEvent@Graphics@@6B@` vtable row, not this DATA row). This makes
the row's attendance measurable by the next boot that runs with
`BANISHED_W17_ATTEND=1`, closing the gap the prior lane could not close because its
worktree lacked GFXVT's merged consumer.

**No promotion was attempted on any of the three READY typeinfo rows.** Reasons, stated
per rule 5 (route-scoped) and rule 4 (a green readiness screen is not a route):

1. **Binary safety is confirmed** (§2.5-2.7): content-only, zero writers, same shape
   already used to promote `GeneratedImageBuffer`/`Material` in Wave 21.
2. **Attendance is NOT confirmed on this tip.** `reg_mint_promoted.cpp`'s own
   promotion bar (criterion 2, precedent set by the ten `OnVideoError` rows) requires
   a real boot to actually reach the consumer. GFXVT's `Type`/`TypeName` accessor
   bodies (the only ported consumers of these three nodes) are only **reachable** once
   their owning vtable row (`??_7AnimationEvent@`, `??_7MaterialInstance@`,
   `??_7Particle@Graphics@@6B@`) is **promoted** — i.e. retail's own vtable pointer for
   an instance is swapped to point at our vtable array, so a virtual `Type()`/
   `TypeName()` call actually dispatches into our body instead of the DLL's. Checked:
   none of the three vtables carry a promotion entry anywhere in `standalone/` on this
   tip (`grep -rn "RT_UNPROMOTABLE\|PROMOTED" standalone/*.cpp` for all three `??_7`
   names returned nothing) — that work belongs to **lane GFXVT of Wave 23**, which owns
   the 8 Graphics VTABLE rows this wave, `??_7AnimationEvent@`/`MaterialInstance@`/
   `Particle@Graphics@@6B@` among them.
3. **No run was available to measure attendance directly either way.** The shared game
   directory was held by `lane-ALLOC` for the entire session
   (`tools/game_dir_lock.sh status` — `HELD by 'lane-ALLOC'`, active writes, checked
   twice ~45s apart, still held both times). Per kickoff §1b this is an acceptable,
   expected outcome, not fabricated as "unverifiable" — I did not skip checking the
   harness first (rule per §1b): the exact env vars and route Wave 21 used
   (`BANISHED_AUTOSTART=1 BANISHED_PAUSEMENU=1200 BANISHED_PAUSEMENU_QUIT=60
   BANISHED_W17_ATTEND=1`) already exist and are unchanged; the only missing piece was
   the watch-table entry fixed in this section, plus GFXVT's vtable promotion.

Promoting any of the three now, on readiness alone, without attendance and without the
vtable door even existing yet, would be exactly the mistake kickoff rule 4 names.

## 4. Build check

`mingw32-make standalone-debug` was started in this worktree to confirm the one-line
`wave17_door_attend.cpp` edit compiles cleanly (no game-directory access needed for a
build, only for a run/deploy). `tools/rt_mint_gen.py --check` reports UP TO DATE (62
vtables, 104 BaseTypeInfo, 39 storage — unchanged) and `tools/dispatch_slot_map.py
--check` reports 110 registrations, 0 mismatches, 0 duplicate addresses — both green,
neither touched by this lane's edit.

## 5. Headline

**0 of 7 rows promoted this lane. This is the correct outcome, not a shortfall**
(kickoff §4: "Zero promotions is a perfectly good outcome if that is what the evidence
says"). Breakdown, independently re-derived against Runtime-x64.dll on this tip:

- **4 permanently HARMFUL / LEAVE** (`_frameIndex@ParticleInstance`,
  `_rng@Particle`, `_rootOnlyUpdates@MeshInstance`, `_singleton@AnimationManager`) —
  unchanged from Wave 17/21, but with a finding worth stating explicitly for future
  lanes: **two of the four now have a ported WRITER** (`IncrementInstanceIndex`,
  `AnimationManager`'s ctor+dtor) and the verdict still holds, because promotion
  safety is governed by the **unported READERS**, not by whether any writer has been
  ported. A future lane should not read "the writer is ported" as progress toward
  promotability for these two — it isn't, until every reader is ported too (18 for
  `_singleton@AnimationManager`, 2 for `_frameIndex`).
- **3 READY, DLL-present route, blocked on attendance + a vtable door that does not
  exist yet** (`_typeInfo@AnimationEvent`, `_typeInfo@MaterialInstance`,
  `_typeInfo@Particle`) — binary-safety re-confirmed on fresh xrefs, identical shape to
  the two Wave 21 already promoted. Addressee named in §6.

## 6. Residuals, named (rule 11)

- **Lane GFXVT (Wave 23)**: promote `??_7AnimationEvent@Graphics@@6B@`,
  `??_7MaterialInstance@Graphics@@6B@`, `??_7Particle@Graphics@@6B@` (your 8 VTABLE
  rows already include these three). Once any one is promoted, its `?_typeInfo@` node
  in this lane's §2.5-2.7 becomes attendance-measurable through
  `graphics_typeinfo_accessors.cpp`'s already-ported `Type`/`TypeName` bodies — no
  further body work needed on the DATA side, only a re-run with
  `BANISHED_W17_ATTEND=1` and a look at the `[W17Attend]` line for the matching
  `?_typeInfo@` symbol (now wired into the watch table by this lane, §3).
- **Integrator or the next lane to hold the game directory**: run the boot in §3 once
  GFXVT's vtable work lands, check `[W17Attend]` for the three `_typeInfo@` rows named
  above, and promote whichever report `ATTENDED` using the exact
  `RtMintPromotionStorage` pattern at `reg_mint_promoted.cpp:950-958` (same two lines
  per row, `Material`/`GeneratedImageBuffer` are the template).
- **Nobody**: the 4 HARMFUL rows. Do not open this file again looking for a way to
  promote them without first porting every reader named in §2.1-2.4 — that is Graphics
  BODY-lane-scale work (`AnimationState`'s ~10 methods, `Particle::Update`/
  `SetParticle`, `MeshInstance::EnableRootOnlyUpdates`/`Update`), not a DATA-lane task.

## 7. What I did NOT check

- Did not re-verify against `Runtime-x64-profile.dll` (retail only, per CLAUDE.md's
  standing warning that offsets disagree between the two builds).
- Did not attempt to synthesize an attendance route myself (e.g. building GFXVT's
  vtable promotion in this worktree) — that is explicitly GFXVT's row set this wave,
  not mine, and duplicating it here would be exactly the kind of file/scope collision
  kickoff rule 14 warns against.
- Did not wait for or acquire the game-directory lock — checked twice, held by ALLOC
  both times, moved on per kickoff §1b.
- Did not examine the other Graphics BODY/VTABLE rows beyond what was needed to
  understand these seven statics' consumers (GFXBODY/GFXVT's own subject rows).
- Did not run `sa-stress` or any perf measurement — no runtime behavior changed by
  this lane (one watch-table entry, comment-only elsewhere).
