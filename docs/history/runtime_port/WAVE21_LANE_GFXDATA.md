# WAVE 21 LANE GFXDATA — the Graphics DATA rows

Branch `wave21/gfxdata` off `wave21/base` (`5ce25be7`). Worktree-isolated per kickoff §5
rule 8.

## 0. Denominator

**This report covers 8 of the now-9 rows.** The lane started against the kickoff's
8-row list. Mid-lane, GFXVT reported closing and said their vtable-slot porting on
`??_7AnimationEvent@Graphics@@6B@` added a live `RT_DATA_SLOT` door on
`?_typeInfo@AnimationEvent@Graphics@@0VBaseTypeInfo@Object@@A`, moving Graphics
`called` 99->100 and the gap 29->30. That consumer lives in GFXVT's branch, which is
not merged into this worktree, so **I could not measure its attendance myself** — see
§4. I did check its binary safety (§3.5) and it is a MINT candidate on the same
evidence as the other four `?_typeInfo@` rows, but I am **not** promoting it in this
branch because I cannot personally confirm the "run in a real boot, reach the
consumer" half of the promotion bar (reg_mint_promoted.cpp's own criterion #2) without
GFXVT's code. Flagging this explicitly rather than silently reporting against the old
8-row denominator, per the coordinator's instruction.

## 1. Re-derivation

`python tools/runtime_census.py --csv census21_gfxdata.csv` (system Python 3.12,
`tools/ghidra_mcp/.venv` does not exist in this worktree copy — CLAUDE.md's quoted
venv path is stale for this checkout; ran under plain `python`, same `pefile` version
installed) reproduced **1311 rows total**, matching the kickoff's board exactly. All
8 kickoff rows still read `ported=0` at kickoff time — the kickoff's list re-derives
true, unchanged.

## 2. What each row does today, and the door it goes through

| row | caller(s) in our tree | door |
|---|---|---|
| `?_frameIndex@ParticleInstance@Graphics@@1HA` | `code/Game/Component/particlecomponent.cpp`, `runtime/src/graphics/animation.cpp`, `standalone/reg_unreg_lane3.cpp` | exe IAT dispatch slot `0x140151f38` resolved to the DLL export (`RegisterLane3Slot`) |
| `?_rng@Particle@Graphics@@1VRandom@System@@A` | `code/Game/Game/tornado.cpp` | `ResolveExport` direct, falls through to the DLL (unregistered as a port) |
| `?_rootOnlyUpdates@MeshInstance@Graphics@@0_NA` | `code/Game/Component/citizenai_executor.cpp`, `standalone/address_registry.cpp` (`RegisterDispatchSlot(0x140151f30, ...)`), `standalone/perf_experimental.cpp` | exe IAT dispatch slot resolved to the DLL export |
| `?_singleton@AnimationManager@Graphics@@0PEAV12@EA` | `runtime/src/graphics/animation.cpp` (`RtAnimMgrSingletonSlot`), `runtime/src/graphics/mesh.cpp` (`RtAnimationManager`) | `ResolveAddr`/`RT_DATA_SLOT`, explicitly resolved to the DLL's own global (both files carry a comment saying so) |
| `?_typeInfo@GeneratedImageBuffer@Graphics@@0VBaseTypeInfo@Object@@A` | `runtime/src/graphics/wave9_lane_gfx.cpp` (`GeneratedImageBufferTypeId`) | `ResolveExport`, cached |
| `?_typeInfo@Material@Graphics@@0VBaseTypeInfo@Object@@A` | `code/Game/Game/gameupdateloop_load.cpp` (`LoadUIMaterial_Step3`), `standalone/reg_unreg_lane3.cpp` (exe slot `0x140151858`) | `RT_DATA_SLOT` / `ResolveExport` |
| `?_typeInfo@MaterialInstance@Graphics@@0VBaseTypeInfo@Object@@A` | `code/Game/Game/serialize_struct60b_serialize.cpp`, `engine/funcs/engine_funcs.cpp` (`g_ppTypeInfo_MaterialInstance`), `standalone/reg_unreg_lane3.cpp` (exe slot `0x140151350`) | `ResolveExport` via exe dispatch slot |
| `?_typeInfo@Particle@Graphics@@0VBaseTypeInfo@Object@@A` | `code/Game/Game/serialize_arrays.cpp` (`Reimpl_Array_Serialize_External_Particle`), `standalone/reg_unreg_lane3.cpp` (exe slot `0x140151420`) | `ResolveExport` via exe dispatch slot |

Four of the eight (`_frameIndex`, `_rootOnlyUpdates`, and the two `ResolveExport`
consumers whose slot is wired through `reg_unreg_lane3.cpp`) already had a **prior
lane's decision on record before this one started**: `standalone/reg_mint_promoted.cpp`
carries four `RT_UNPROMOTABLE ... HARMFUL` tags dated `WAVE17_LANE_CEIL.md §3.2` for
`_singleton@AnimationManager`, `_frameIndex@ParticleInstance`, `_rng@Particle`, and
`_rootOnlyUpdates@MeshInstance`. This lane's job for those four was to **re-verify the
claim independently**, not take it on faith (kickoff rule 1).

## 3. MINT vs HARMFUL, with independent binary evidence

I re-derived the four HARMFUL verdicts and the two PROMOTABLE ones directly against
**Runtime-x64.dll** (not the profile DLL — stated because CLAUDE.md and the
coordinator both warn the two binaries' offsets disagree) using
`mcp__ghidra-mcp__get_xrefs_to` with `program=Runtime-x64.dll` pinned explicitly on
every call, so a concurrent lane's `switch_program` could not affect the read.

### 3.1 HARMFUL — confirmed, holds on the DLL-present route

| symbol | retail VA | in-DLL xrefs | verdict |
|---|---|---|---|
| `_singleton@AnimationManager` | `0x1801fe7b0` | 20 refs: 2 **WRITE** (`AnimationManager` ctor `0x1800fc490`, `~AnimationManager` dtor `0x1800fc412`), 18 READ across `Update`, `Draw`, `GetNodeToWorld`, `PlayAnimation`, `GetInstanceTransform(s)`, `Get`, etc. | HARMFUL |
| `_frameIndex@ParticleInstance` | `0x1801fe77c` | 5 refs: `IncrementInstanceIndex` (`0x1800201f0`) does a **READ_WRITE** in one RIP-relative instruction; two more READs in `Update` | HARMFUL |
| `_rng@Particle` | `0x1801fe7f0` | ~45 refs, overwhelmingly inside `Update` (`0x1800a5xxx`) and `SetParticle`, alternating READ/WRITE — retail's own particle simulation consumes and advances this exact RNG stream every tick | HARMFUL, most clear-cut of the four |
| `_rootOnlyUpdates@MeshInstance` | `0x1801fc70b` | 4 refs: `EnableRootOnlyUpdates` (**WRITE**), `AreRootOnlyUpdateEnabled` (READ), one READ in `Update` | HARMFUL |

Every write is a **RIP-relative MOV inside the DLL**, not a call through an exported
door. Per this project's own doctrine (quoted verbatim in `reg_mint_promoted.cpp`):
*"we intercept only at doors and a RIP-relative MOV is not one"* — minting our own
storage for any of these four would split the brain: retail's own ctor/dtor/Update
write to `0x1801fXXXX` directly, our reader would read a different address, and
nothing would report the divergence. **This independently confirms
`WAVE17_LANE_CEIL.md §3.2`; nothing changed. LEAVE all four, unconditionally.**

**Route qualifier (per the coordinator's SHOT-lesson note):** this verdict is scoped
to the **DLL-present** route, which is the only route that exists today — the
consumers named above (`AnimationManager` ctor/dtor/Update, `ParticleInstance::
IncrementInstanceIndex`, `Particle::Update`, `MeshInstance::EnableRootOnlyUpdates`)
are all **unported**, so `Runtime-x64.dll`'s own code is still the one running and
writing these addresses. If a future lane ports all of a symbol's in-DLL
readers/writers to our own code (i.e. genuinely reaches cutover for that class), the
verdict **reverses**: at that point OUR code becomes the sole reader and writer, the
DLL's copy is no longer live, and minting our own storage stops being harmful and
becomes the only correct arrangement (this is exactly the reasoning already written
next to `?_singleton@Application@1@` in the same file for the analogous case). None of
the four are anywhere near that point today — `AnimationManager`'s `Update`,
`PlayAnimation`, `Draw` etc. are all still-unported Runtime bodies.

Live attendance this run (`BANISHED_W17_ATTEND=1`, real New Game boot — see §4):
`_singleton@AnimationManager` and `_frameIndex@ParticleInstance` are **currently
ATTENDED** (`post=1` each) — i.e. this is not a theoretical hazard, our own ported
code (`mesh.cpp`'s `RtAnimationManager`, `particlecomponent.cpp`) exercises these
exact addresses today, through the DLL's storage, correctly. `_rng@Particle` and
`_rootOnlyUpdates@MeshInstance` read `never/post=0` in this particular run (no
tornado/particle system and no root-only-update path was hit in this short session) —
absence of attendance here does not change the verdict, since the HARMFUL finding
rests on the in-DLL writer, not on our own attendance.

### 3.2 The two PROMOTABLE rows — MINTED this lane

`GeneratedImageBuffer` and `Material` were named `WAVE17_LANE_CEIL.md §3.3
PROMOTABLE (2)` and explicitly **left for a porting lane** ("Both are the shape the UI
namespace promoted eleven of in Wave 10 ... it does not move the ceiling, which is why
this lane did not do it") — this is precisely this lane's job.

Re-verified against Runtime-x64.dll:

| symbol | retail VA | in-DLL xrefs |
|---|---|---|
| `_typeInfo@GeneratedImageBuffer` | `0x1801f9a20` | 6 refs, all READ (`RenderTextures`, `GenerateTextures`, `Type`, `TypeId`) — **zero writers besides the static's own constructor at load** |
| `_typeInfo@Material` | `0x1801fa740` | 6 refs, all READ (`Type`, `TypeId`, two internal helper functions) — same shape |

No in-DLL writer other than CRT static init for either — matches the project's
standing finding that `BaseTypeInfo::name` (and, by the same argument, `typeId` after
`Factory::Startup` runs) is **content-only** in both shipped binaries
(`reference_basetypeinfo_name_is_content_only.md`), so minting our own copy cannot
diverge from a live retail writer the way the four HARMFUL rows can.

**Reader-bounds check (per GFXVT's finding that a correct, complete row can still be
HARMFUL if a consumer walks past it):** I checked every consumer in our tree of both
symbols. Every one dereferences the object **exactly once**, reading a fixed 4-byte
`typeId` at offset 0 of a fixed-layout `Object::BaseTypeInfo` — there is no loop, no
count derived from another table, and no walk past the struct:

* `GeneratedImageBufferTypeId()` (`runtime/src/graphics/wave9_lane_gfx.cpp:95-101`):
  `cached ? *cached : 0xffffffffu` — one dereference.
* `LoadUIMaterial_Step3` (`code/Game/Game/gameupdateloop_load.cpp:247-251`):
  `fn_IsDerivedFrom(mat, *s_matTypeInfo)` — one dereference, passed by value into an
  engine call, not walked further on our side.
* `mapui.cpp:81`, `noiseui.cpp:84`, `moddialog.cpp:432` (same `g_TypeId_Material`
  slot, per `reg_unreg_lane3.cpp`'s own comment): same one-dereference shape.

Unlike GFXVT's `??_7Geometry@Graphics@@` finding — a fixed-size vtable **array**
walked by a loop bound the reader assumes rather than derives — a `BaseTypeInfo` is a
single scalar struct with no analogous "walk N of them" consumer anywhere in this
tree for either symbol. **The class of harm GFXVT found does not apply here.**

**Live attendance, both rows, this wave (2026-09-11):**

First attempt used `BANISHED_AUTOSTART=1 BANISHED_AUTOQUIT=600`, which is the
documented trap (`reference_autoquit_overrides_autostart_newgame.md`): AUTOQUIT
overrides AUTOSTART's New Game path and quits from the **menu**, never building a
world. That run reported `_typeInfo@Material` and `_typeInfo@MaterialInstance` and
`_typeInfo@Particle` all `never/post=0` — worthless, since `[GUL::UIMat]` /
`[GUL::ProfList]` (the world-build steps that would exercise Material) never printed
at all in either log. Caught by grepping for those lines coming up empty and
recognising the memory note, not by disbelieving the number on its face.

Re-ran correctly: `BANISHED_AUTOSTART=1 BANISHED_PAUSEMENU=1200
BANISHED_PAUSEMENU_QUIT=60 BANISHED_PAUSEMENU_BUTTON=6 BANISHED_W17_ATTEND=1
BANISHED_RT_MINT_SHADOW=1` — a real New Game boot that reaches gameplay and then
exits cleanly through the in-game pause menu's Quit button (so the exit-time shadow
report actually fires). Result:

```
[GUL::ProfList] gw+0xb0=... defaultProf(+0x30)=... profRef2(+0x38)=...
[GUL::UIMat] gw+0xb8=0000025519d907c8 (loaded=0000025519d907c8 isMaterial=1)
...
[W17Attend] ATTENDED init=0    post=1      ?_typeInfo@GeneratedImageBuffer@Graphics@@0VBaseTypeInfo@Object@@A
[W17Attend] ATTENDED init=1    post=1      ?_typeInfo@Material@Graphics@@0VBaseTypeInfo@Object@@A
```

Both ATTENDED, matching `WAVE17_LANE_CEIL.md §3.3`'s own measurement exactly, one
wave later, on an independently re-run boot.

**Action taken:** promoted both in `standalone/reg_mint_promoted.cpp` using the
project's existing `RtMintPromotionStorage` + `RegisterRuntimeExport` +
`RegisterRuntimeAddr` pattern (no new mechanism). Rebuilt (`mingw32-make
standalone-debug`, clean compile), redeployed under `tools/game_dir_lock.sh`, and
re-ran the identical boot:

```
[RtMintShadow]   READY    ?_typeInfo@GeneratedImageBuffer@Graphics@@0VBaseTypeInfo@Object@@A id=0xa1af3cd9 idcmp=yes strs=0 ptr=2 fac=2traps  [PROMOTED]
[RtMintShadow]   READY    ?_typeInfo@Material@Graphics@@0VBaseTypeInfo@Object@@A     id=0x418398a4 idcmp=yes strs=0 ptr=2 fac=2traps  [PROMOTED]
[W17Attend] ATTENDED init=0    post=1      ?_typeInfo@GeneratedImageBuffer@Graphics@@0VBaseTypeInfo@Object@@A
[W17Attend] ATTENDED init=1    post=1      ?_typeInfo@Material@Graphics@@0VBaseTypeInfo@Object@@A
```

`idcmp=yes`, `strs=0` (content still agrees with the live DLL post-promotion),
`standalone_crash.log` stayed empty, both rows still attended with the promotion
live. Census (`tools/runtime_census.py`) confirms `ported` flipped `0 -> 1` for both
symbols and for no others. `tools/rt_mint_gen.py --check` still reports UP TO DATE
(62 vtables, 104 BaseTypeInfo, 39 storage) — the generator's cached table did not
need regenerating, only the promotion file changed.

### 3.3 The two READY-but-unattended rows — left unpromoted

`_typeInfo@MaterialInstance` and `_typeInfo@Particle` are READY on **identical**
evidence to the two promoted rows — same content-only argument, same "0 of 106,251
factory dispatches walk a minted node" structural exemption
(`WAVE17_LANE_CEIL.md §2.6`, which is stated as applying to *any* minted `?_typeInfo@`
row, not just the two it names), same one-dereference consumer shape
(`serialize_arrays.cpp`'s `Reimpl_Array_Serialize_External_Particle`,
`serialize_struct60b_serialize.cpp`'s `**g_ppTypeInfo_MaterialInstance` — both single
scalar reads, no walk). The shadow report's own aggregate line — `typeinfo rows: 104
READY, 0 BLOCKED (of 104)` — confirms both are content-correct today, promoted or not.

The same live boot reported both **`never init=1 post=0`**, matching
`WAVE17_LANE_CEIL.md`'s "blocked on attendance" verdict for these exact two rows,
one wave later and on freshly built code. I did not promote them.

This is a deliberate, doctrine-driven choice, not an oversight: `reg_mint_promoted.cpp`
states its own promotion bar up front (criterion 2 — "the row must be run in a real
boot and the run must reach the same place the control reaches"), and the file's own
precedent for the ten `OnVideoError` typeinfo rows shows the project **actually
enforcing** that bar in practice — those ten sat "blocked on attendance" in
`WAVE17_LANE_CEIL.md §3.4` until a *later* wave synthesised the device-lost route and
measured `[FactoryDispatch]` attendance directly, and only then were they promoted
(comment at `reg_mint_promoted.cpp:884-888`). Promoting `MaterialInstance`/`Particle`
on readiness alone, without having driven their route, would be exactly the "green
readiness screen read as a route" mistake kickoff rule 4 names.

**What would close this:** `MaterialInstance`'s consumer needs a save/load with a
struct60b-bearing entity, or a route that walks `MaterialInstance` reference arrays;
`Particle`'s needs `Reimpl_Array_Serialize_External_Particle` to run, i.e. a save/load
that touches an object owning a `Particle` external-pointer array (candidate: an
active particle effect — chimney smoke, tornado — present when a save happens). I did
not attempt to synthesize either route in this lane; it is a concrete, scoped follow-up
for whichever lane picks up Save/Load or particle-effect work next.

### 3.4 The 9th row, added mid-lane by GFXVT

`?_typeInfo@AnimationEvent@Graphics@@0VBaseTypeInfo@Object@@A`, retail VA
`0x1801f9978` (already present in `rt_data_mint_tables.inc` since it was pulled in by
the vtable-closure rule when `??_7AnimationEvent@Graphics@@6B@` was first minted — see
`tools/rt_mint_gen.py`'s "CLOSURE" comment — independent of GFXVT's new consumer).

**Binary safety, checked the same way as the other four:** `get_xrefs_to` against
Runtime-x64.dll returns 6 refs, all READ (`Type`, `TypeId`, two internal helpers at
`0x1800594a0`/`0x180059b20`) — **zero in-DLL writers besides its own static ctor**,
same content-only shape as the two promoted rows. It is already part of the "104
READY, 0 BLOCKED" aggregate this run reported, so its content matches the live DLL
exactly.

**Not promoted, and not asserted READY-plus-attended, because I cannot check the
attendance half:** GFXVT's `RT_DATA_SLOT` consumer lives on `wave21/gfxvt`, a branch
this worktree does not have merged. I cannot run a boot that exercises it, and I will
not claim attendance I did not personally measure. **This row is examined for binary
safety only; its promotion decision (readiness confirmed, attendance unconfirmed by
this lane) is left for GFXVT or the integrator to close after merge**, using the exact
method in §3.2 above (`BANISHED_W17_ATTEND=1` is not wired to watch this symbol by
name yet — `standalone/wave17_door_attend.cpp`'s `g_watch[]` table would need this
9th symbol added, or a fresh grep/attendance check done independently).

## 4. Live-boot method, stated once

All live evidence above came from one build (`mingw32-make standalone-debug`, clean,
this worktree) deployed under `tools/game_dir_lock.sh acquire GFXDATA` /
`tools/game_dir_lock.sh release GFXDATA` (never piped, per kickoff rule 9), run twice:
once with the `BANISHED_AUTOQUIT` mistake (discarded), once corrected with
`BANISHED_AUTOSTART=1 BANISHED_PAUSEMENU=1200 BANISHED_PAUSEMENU_QUIT=60
BANISHED_PAUSEMENU_BUTTON=6 BANISHED_W17_ATTEND=1 BANISHED_RT_MINT_SHADOW=1`, then a
third time after the promotion edit to confirm no regression. `standalone_crash.log`
was empty after every run.

## 5. The headline number

**Of the 8 rows this lane owned at kickoff: 2 are real mint work (done), 4 are
correctly-permanent LEAVE (independently re-confirmed HARMFUL, not merely inherited),
and 2 are READY-but-not-yet-actionable (correctly withheld pending an attendance
route no one has driven yet).**

So: **2 of 8 converted to `ported` this lane. 0 of the remaining 6 are "unported work
still to do" in the ordinary sense** — 4 are a permanent documented non-port (minting
them would be a regression, not progress) and 2 are a scoped, named, but currently
un-driveable follow-up. **Zero of the 8 were simple oversights nobody had looked at
before** — every row already had either a Wave 17 verdict or a structural (content-only)
argument on record; this lane's contribution was re-deriving each one live rather than
trusting the record, executing the two that were actionable, and catching one
methodology trap (`BANISHED_AUTOQUIT`) on the way.

The 9th row (added mid-lane, §3.4) is additional: binary-safety-checked and READY, but
its promotion is explicitly left open pending attendance evidence this lane could not
gather.

**For the wave's own candidate doctrine (kickoff §1):** this is a second, independent
confirmation of the UI-namespace lesson — a namespace gap's DATA rows are not
"unported work" in the way its BODY rows are. Of Graphics's 8 (now 9) DATA rows,
**at most 3 will ever become `ported`** (2 done, MaterialInstance/Particle pending a
route) and up to 1 more pending GFXVT's merge; the other **4 are permanently
unportable by design**, and no amount of future effort should ever move them into
`ported` — doing so would be the regression, not the fix.

## 6. What I did NOT check

* **`?_typeInfo@AnimationEvent@Graphics@@` attendance** (§3.4) — GFXVT's consumer is
  not in this worktree; I checked its binary safety only.
* **Did not attempt to synthesize a route** for `MaterialInstance` or `Particle`
  (save/load with a matching entity, or an active particle effect at save time) —
  named as follow-up in §3.3, not attempted here.
* **Did not re-verify the profile DLL** (`Runtime-x64-profile.dll`) for any of the
  nine rows — all binary evidence in this report is against **retail
  `Runtime-x64.dll`** specifically, named per binary as instructed, since the two are
  known to disagree on offsets and only retail is what standalone actually loads.
* **Did not check GFXBODY's or GFXVT's own subject rows** (the 13 BODY / CTOR/DTOR/
  operator-new rows and the 8 VTABLE rows) beyond what was needed to understand the
  shared consumers (`MaterialInstance`/`Particle` vtable TRAP counts, quoted in §3.3,
  came from this lane's own live shadow output, not from re-deriving GFXBODY/GFXVT's
  work).
* **Did not run the stress harness or any perf measurement** — this lane's two live
  changes are DATA-only (a pointer swap at a resolve door), not on any hot path; no
  perf claim is made or needed.
* **Did not attempt the `_singleton@AnimationManager` cutover-side reversal** described
  in §3.1's route qualifier — that requires porting `AnimationManager`'s ctor/dtor/
  Update/PlayAnimation/Draw first, which is GFXBODY-scale work far outside this lane's
  8-row DATA subject.
