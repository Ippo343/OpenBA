# WAVE 27 LANE GFXPART — the Graphics particle and light vtable families

Branch `wave27/gfxpart`, cut from `wave27/base`.

Everything below is labelled **MEASURED** or **INFERRED**. Nothing in a MEASURED
section was taken from the kickoff, from a previous wave's doc, or from Ghidra's
decompiled C.

---

## 1. RE-DERIVED BOARD (MEASURED)

`tools/runtime_census.py` run in THIS worktree at the branch point. The venv does
not exist inside a worktree (it is gitignored), so the interpreter came from the
main checkout while the script ran with this worktree as its cwd — which is what
its own SUBJECT block prints and checks:

```
  tree root      ...\.claude\worktrees\agent-a144a03b8a82d059f
  TOTAL          1372 called / 1237 ported / 90.2%
```

The five tables the brief named, as the census printed them — **the brief's counts
were correct, re-derived rather than trusted**:

| vtable | ours / slots |
|---|---|
| `??_7Particle@Graphics@@6B@` | 5 / 6 |
| `??_7Light@Graphics@@6B@` | 5 / 6 |
| `??_7ParticleInstance@Graphics@@6B@` | 5 / 9 |
| `??_7ParticleSystemInstance@Graphics@@6B@` | 5 / 10 |
| `??_7SunLight@Graphics@@6B@` | 5 / 11 |

`tools/mint_slot_sweep.py` agrees from the other direction, and **its `slots`
column counts differently from the census's** — a fact worth writing down because
the two numbers look like a disagreement and are not. The sweep prints the
**raw** table length and the census prints the **non-purecall** length:

```
  0x180155040    5/10  unported      (ParticleSystemInstance)
  0x1801550e0    4/10  unported      (ParticleInstance)
  0x180158e10    6/11  unported      (SunLight)
```

### 1.1 The seventeen slots, enumerated (MEASURED)

No tool prints this list; it was built by taking every slot address out of
`standalone/rt_data_mint_tables.inc` and grepping the tree for a
`RegisterRuntimeExport` of its mangled name (for `RT_MINT_NAMED` rows) or a
`RegisterRuntimeAddr` of its address (for `RT_MINT_UNNAMED` rows). Fourteen
DISTINCT functions fill seventeen slots, because three ParticleInstance bodies
are shared slots in the ParticleSystemInstance table.

| table | slot | symbol | retail | this lane |
|---|---|---|---|---|
| Light | 0 | *(unnamed vector deleting dtor)* | 0x180058dd0 | **PORTED** |
| Particle | 5 | `?Serialize@Particle@Graphics@@` | 0x180103f00 | **PORTED** |
| ParticleInstance | 2 | `?Draw@ParticleInstance@Graphics@@…MHHH@Z` | 0x180104230 | **PORTED** |
| ParticleInstance | 3 | `?AddDrawItems@ParticleInstance@Graphics@@` | 0x180104700 | **PORTED** |
| ParticleInstance | 6 | `?GetInstanceTransform@ParticleInstance@Graphics@@` | 0x1801047f0 | **PORTED** |
| ParticleInstance | 9 | `?SetParticle@ParticleInstance@Graphics@@` | 0x180104a00 | **PORTED** |
| ParticleSystemInstance | 2,3,6 | *(the three PI bodies above)* | — | **closed by the PI ports** |
| ParticleSystemInstance | 9 | `?SetParticle@ParticleSystemInstance@Graphics@@` | 0x1800a6a80 | **PORTED** |
| ParticleSystemInstance | 1 | `?Update@ParticleSystemInstance@Graphics@@` | 0x1800a5600 | **DEFERRED — §6.1** |
| SunLight | 0 | *(unnamed vector deleting dtor)* | 0x180064620 | **PORTED** |
| SunLight | 1 | `?Update@SunLight@Graphics@@` | 0x1801067a0 | not attempted — §6.2 |
| SunLight | 2 | `?Draw@SunLight@Graphics@@…MHHH@Z` | 0x180106790 | not attempted — §6.2 |
| SunLight | 3 | `?AddDrawItems@SunLight@Graphics@@` | 0x180106f50 | not attempted — §6.2 |
| SunLight | 9 | `?DrawShadowMap@SunLight@Graphics@@` | 0x180106bc0 | not attempted — §6.2 |
| SunLight | 10 | `?Draw@SunLight@Graphics@@…HHHH@Z` | 0x1801067b0 | not attempted — §6.2 |

Table state after this lane (STATIC half only — see §3). **The "after" column is
MEASURED: `tools/runtime_census.py` re-run on the finished branch, not predicted
from the port count.**

| table | before | after (re-run) |
|---|---|---|
| `??_7Light@Graphics@@6B@` | 5 / 6 | **6 / 6 COMPLETE** |
| `??_7Particle@Graphics@@6B@` | 5 / 6 | **6 / 6 COMPLETE** |
| `??_7ParticleInstance@Graphics@@6B@` | 5 / 9 | **9 / 9 COMPLETE** |
| `??_7ParticleSystemInstance@Graphics@@6B@` | 5 / 10 | **9 / 10** (Update only) |
| `??_7SunLight@Graphics@@6B@` | 5 / 11 | **6 / 11** |

Eight distinct new bodies/rows; twelve of the seventeen slots closed; **three
tables reach COMPLETE and a fourth is one body short.**

**And the headline percentage went DOWN while that happened** —
1372/1237 90.2% → 1376/1237 89.9%. That is the ceiling identity, not a
regression; §6.3 has the four-row accounting.

---

## 2. WHAT WAS PORTED, AND THE READINGS THAT WERE NOT OBVIOUS

Every body is in `runtime/src/graphics/wave27_lane_gfxpart.cpp` except the two
deleting destructors, which are two rows in
`runtime/src/object/deleting_dtor.cpp`'s existing table (one body, N rows — see
that file's banner). Each body's own comment block carries the full transcription;
this section records only what a reader would otherwise get wrong.

### 2.1 The two deleting destructors (MEASURED)

Disassembled at 0x180058dd0 and 0x180064620:

```
0x180058dd0:  LEA R9,[0x180058d20]  ~Light     MOV EDX,0xc0   sizeof(Light)
0x180064620:  LEA R9,[0x180108160]  ~SunLight  MOV EDX,0x108  sizeof(SunLight)
```

Both dtor VAs resolve **by VA** in the shipped export directory to
`??1Light@Graphics@@UEAA@XZ` / `??1SunLight@Graphics@@UEAA@XZ` — `UEAA`, the
virtual spelling. Both deleting-dtor addresses are themselves `<UNNAMED>` there,
checked rather than assumed (`AEAA` means PRIVATE, not unexported, so "it has no
name" is never free).

**The Light row is a THIRD shape this family had not recorded.** Its scalar branch
calls `0x1800a41c0` (`??1Geometry@Graphics@@UEAA@XZ`) rather than the address its
own array branch puts in R9. That is not a second family: `~Light` (0x180058d20)
disassembles to a bare frame set-up and `JMP 0x1800a41c0` — Light adds no members
over Geometry, so MSVC devirtualised the tail call in one branch and not the
other. The size agrees: 0xc0 here is exactly the existing `Graphics::Geometry`
row's 0xc0. Calling `~Light` in both branches, which is what `DeletingDtorCore`
does, lands on identical code.

SunLight's scalar-branch `operator delete` is `0x1800643b0`, an **eleventh**
per-translation-unit copy not in `deleting_dtor.cpp`'s banner list of ten.
Disassembled for this lane: `if (p) { Enter(cs); Free(_singleton, p); Leave(cs); }`
— the same body as the other ten. Its array branch uses `0x180006a10`, already
listed. Nothing about the port changes.

### 2.2 `Particle::Serialize` — the layout it pins (MEASURED)

Three `Graphics::Particle` fields that `runtime/types/graphics/Particle.h` carried
as `UNVERIFIED, zeroed by the ctor` now have a measured meaning, and two
independent readings agree on the first one:

* **+0x30 `ExternalPtr<MaterialInstance>`.** `Particle::Serialize` hands `&+0x30`
  to the `Object::ExternalBasePtr::Serialize` template instantiation at
  0x180014520 — and, from the other end, `ParticleInstance::AddDrawItems` passes
  `*(void**)(particle+0x30)` as `Renderer::AddDrawItem`'s
  `const MaterialInstance*` argument. Two unrelated bodies, one conclusion.
* **+0x38 / +0x40 / +0x44 an `Array<void*>`** of pool-record pointers:
  `Serialize` calls `SetSize(&+0x38, count)` and then stores into
  `*(void**)(this+0x38)`, and the PROFILE build's `~Particle` loops
  `i < *(int*)(this+0x40)` over `*(void**)(this+0x38) + i*8`.
* **+0x24 the record count** (the argument to that `SetSize`) and **+0x28 the
  per-record sub-array size** (passed by the record ctor to a second `SetSize`).

`Particle.h` was updated in place with these readings; the struct's field names,
offsets and `sizeof` are UNCHANGED, deliberately — `animation.cpp`'s ctor oracles
compare ranges of this struct byte-for-byte and renaming is free while
re-shaping is not.

### 2.3 The pool record is 0x40 bytes, not 0x30 (MEASURED)

`Particle.h` described `ParticleBoundsRecord` as `opaque[0x2c] + frameTag`. Its
constructor (0x180103bf0, unnamed) writes through +0x38 and both allocation sites
ask for **0x40**:

```
+0x00..+0x18  Volume::Box, min seeded (1e8,1e8,1e8), max (-1e8,-1e8,-1e8)
+0x18         the live-particle Array's data pointer  (sized to particle->+0x28)
+0x20         that array's size
+0x28         the live particle count
+0x2c         the per-frame tag, seeded -1    <- the field the header already had
+0x30, +0x38  zeroed
```

The `+0x2c` reading in the header is CONFIRMED (seeded to `0xffffffff`, and
`ParticleSystemInstance::Update` compares it against
`?_frameIndex@ParticleInstance@Graphics@@`). The 0x30-vs-0x40 size is a
CORRECTION, recorded in `Particle.h` beside the struct and used by this lane
through named offset constants rather than by widening the struct.

### 2.4 The shared RNG — the brief's warning, confirmed and acted on (MEASURED)

`?_rng@Particle@Graphics@@1VRandom@System@@A` (0x1801fe7f0) is not merely
*consumed* by a Particle body — `ParticleInstance::SetParticle` **inlines the
whole MT19937 draw**: the index load, the conditional
`?NextGroup@Random@System@@AEAAXXZ` twist, the four tempering steps, the
`CMP EDX,0x270 / CMOVZ` wrap, and the unsigned `DIV` that reduces the word modulo
the record count. `ParticleSystemInstance::Update` inlines the same sequence
**twelve** times.

So the brief's warning is not a style note: a port that constructed its own
generator would compile, run, look plausible and place every particle
differently from the DLL's other consumers of that same object. This port reads
and writes THAT object, through the same `ResolveExport` door
`code/Game/Game/tornado.cpp:253` already uses, so a minted copy is followed at
cutover and the DLL's copy is followed today.

**The shipped tempering masks are `0xff3a58ad` and `0xffffdf8c`**, not the
textbook `0x9d2c5680` / `0xefc60000`. They are those constants with the bits the
paired shift discards already cleared. They were lifted from the bytes, not
retyped from a reference.

### 2.5 Arguments retail IGNORES (MEASURED)

Three of the four ParticleInstance bodies take parameters they never read, and in
each case the proof is that the register is overwritten before any load through
it. A port that "used" them would be wrong, and a decompile that dropped them
would look the same as one that used them:

| body | ignored |
|---|---|
| `GetInstanceTransform(int, float)` | the **int**. First instruction is `LEA R8,[RCX+0x84]`. The base `Geometry::GetInstanceTransform` (0x1800154a0) does use an index; this override deliberately does not. |
| `AddDrawItems(const Scene&, Renderer&)` | the **Scene&**. RDX is overwritten with `this` at 0x18010477c. |
| `Draw(Context&, const Camera&, ExternalPtr, float, int, int, int)` | the **Camera&**, the **ExternalPtr**, and args **5 and 6**. Only the float (alpha) and the SEVENTH argument (instance count) are read. |

### 2.6 Four float readings that a rewrite would silently change (MEASURED)

* `GetInstanceTransform` and `Draw` part A both blend opacity as
  `((1 - t) * _opacityPrev) + (t * _opacity)` — retail forms both products and
  adds the `t` term **last**.
* The translation blend is `prev + (cur - prev) * t` — retail subtracts,
  multiplies, then adds onto the PREVIOUS value. The two spellings are
  algebraically identical and numerically different.
* `AddDrawItems`' squared distance is `(dy + dx) + dz`, in that association:
  `ADDSS XMM2,XMM0` (Y += X) then `ADDSS XMM2,XMM1` (+= Z).
* `Draw`'s per-particle rotation scales **each term** by `pi/180` before the add
  (`MULSS XMM1,XMM12` on both sides, then `ADDSS`), so the record's rotation
  fields are stored in DEGREES. `pi/180` is `[0x18016167c]` = `0x3C8EFA35`,
  lifted as a bit pattern because `3.14159f / 180.0f` is a different float.
* The shipped `1.0f` is `[0x18016169c]` = `0x3F800000`, read out of the DLL with
  `read_memory` rather than assumed from the mnemonic.

### 2.7 `GetInstanceTransform` overwrites ALL FOUR w-components (MEASURED)

Retail does not return `AsMatrix`'s matrix with one field patched. It writes
`m[3] = m[7] = m[11] = 1.0f` from the shipped constant and `m[15]` from the
opacity blend. A port that copied the matrix wholesale and patched only +0x3c
would leave `AsMatrix`'s own w-row in place and hand the shader a different
matrix. This is the single easiest thing to get wrong in this lane.

### 2.8 `ParticleInstance::Draw` — three details that survive only in the bytes (MEASURED)

* **The quad-index base is 16-bit arithmetic.** `ADD R14W,0x4` wraps at 65536 and
  every index store is `MOV word`. Kept in a `uint16_t`; an `int` would emit
  different indices past 16384 particles.
* **The particle walk is BACKWARD while the index buffer fills FORWARD.** The
  record offset is `(count - i - 1) * 0x58`. Particle *k* is drawn as quad
  *count-1-k*. Reordering changes the draw order of overlapping translucent
  sprites, which is visible.
* **`0x18008a290` is `memmove`**, not a vertex format converter — disassembled in
  full for this lane (rep-movsb path, 32-byte unrolled path, MOVNTI path, ERMS
  feature bit tested at `0x1801fc65c`). It is called as
  `(vertexCursor, &record, stride)` and `std::memmove` is the same contract.
* The two ContextData slots it dispatches are **+0x90 = slot 18
  `BeginInstancedIndexedVertices(vertexCount, stride, indexCount, instances)`**
  and **+0x98 = slot 19 `EndInstancedIndexedVertices(primitiveType)`**, per
  `dx11/types/VideoDX11Data.h`'s slot map — which derived every name from the
  Runtime thunk that dispatches to it, so this is a name, not a guess.

---

## 3. PROMOTION — NOT CLAIMED, AND WHY (MEASURED premise, INFERRED verdict)

**No table is promoted by this lane.** COMPLETE is the static half only; promoting
turns on owning the function that STAMPS the table, and no source scan answers
that.

What was actually established: **MEASURED** — the five tables' `ours/slots`
counts, and that four reach COMPLETE. **NOT established** — who stamps
`??_7Particle@`, `??_7Light@`, `??_7ParticleInstance@` or
`??_7ParticleSystemInstance@`. This lane did not run a stamper census on them
(`tools/mint_stamper_census.py` exists; running it, reading its output, and
deciding what its silence means is a measurement this lane did not make, and
recording an unrun tool's expected answer beside the run ones is precisely the
defect the wave's warning #6 names).

So the honest state is **COMPLETE-but-unpromoted, four tables**, with the
promotion question OPEN.

> **RESIDUAL 1 — ADDRESSEE: the integrator running Wave 27's cutover attempt 30,
> and failing that, the Wave 28 lane that owns Graphics vtables.**
> Four tables (`??_7Particle@`, `??_7Light@`, `??_7ParticleInstance@`,
> `??_7ParticleSystemInstance@` at 9/10) are now STATICALLY complete or nearly so
> and none is tagged UNPROMOTABLE. The open question is the stamper for each.
> Re-derive with:
> ```
> tools/ghidra_mcp/.venv/Scripts/python.exe tools/runtime_census.py   # confirm the counts moved
> tools/ghidra_mcp/.venv/Scripts/python.exe tools/mint_stamper_census.py
> ```
> and promote only the ones whose stamper we own. Note what
> `reference_a_ported_row_can_be_permanently_unpromotable.md` says: a row can be
> ported in full and permanently unpromotable, because an in-DLL rel32 call or a
> RIP-relative LEA stamp is not a door.

---

## 4. THE TWO UNPROMOTABLE DATA ROWS — READ, NOT TOUCHED (MEASURED)

```
grep -n RT_UNPROMOTABLE standalone/reg_mint_promoted.cpp
```

* `?_frameIndex@ParticleInstance@Graphics@@1HA` (0x1801fe77c) — a protected
  static `int`, the global frame counter `ParticleSystemInstance::Update` compares
  against each pool record's `+0x2c` tag.
* `?_rng@Particle@Graphics@@1VRandom@System@@A` (0x1801fe7f0) — the shared
  MT19937, tagged `NONE, EVER` on the DLL-PRESENT route and `[SCOPE: DLL-PRESENT]`,
  i.e. the row **inverts to REQUIRED at cutover** when the in-DLL RIP-relative
  readers that make promotion harmful cease to exist.

Neither was promoted, neither has a body to write, and §2.4 records what the
second one costs a Particle port that ignores it. The `_frameIndex` row is read
by `ParticleSystemInstance::Update`, which this lane deferred — see §6.1.

---

## 5. FINDING: `RendererDrawGroup + 0x90` IS AN EYE POSITION (MEASURED)

`runtime/types/graphics/Camera.h` declares `RendererDrawGroup::_head[0xe0]` with
the note "not read by lane F". `ParticleInstance::AddDrawItems` is the first body
in the tree to read it, and it reads three floats at **+0x90** and subtracts them
from the instance's world translation to form a squared distance — i.e. a view/eye
position, used as the draw item's sort key.

It is read here through a documented byte offset rather than by splitting
`_head`, because lane F's renderer oracles memcmp `RendererDrawGroup` and
splitting the array would change nothing about the bytes but move the row's
ownership.

> **RESIDUAL 2 — ADDRESSEE: the Wave 28 lane that owns `Graphics::Renderer`
> (lane F's successor).** Name `RendererDrawGroup + 0x90..0x9b` in
> `runtime/types/graphics/Camera.h` as the group's eye position, with
> `?AddDrawItems@ParticleInstance@Graphics@@` (0x180104700, at 0x18010471e and
> 0x180104758) as the pinning body. Re-derive with
> `disassemble_function 0x180104700` and look for `MOVSS XMM0,[RDX+RAX+0x90]`.

---

## 6. WHAT WAS NOT DONE, WITH ADDRESSEES

### 6.1 `?Update@ParticleSystemInstance@Graphics@@` (0x1800a5600) — DEFERRED

**MEASURED size: 0x1479 bytes (0x1800a5600..0x1800a6a79), the whole particle
simulation.** It contains twelve separately-inlined MT19937 draws, three calls
into unnamed float helpers (0x180127d90, 0x180126d10 — a sin/cos pair by
position), a spawn loop, an age/kill loop with a compacting `0x58`-stride
swap-remove, and a `Box` accumulation. It is a GFXBIG-scale body, not a slot.

It is the ONLY thing between `??_7ParticleSystemInstance@Graphics@@6B@` and
COMPLETE (9/10 after this lane).

> **ADDRESSEE: Wave 28, as a lane of its own** — scoped like Wave 27's GFXBIG
> (one large body), not as a vtable lane. It needs: the 0x58 record layout (§2.3
> gives the pool; the record's own fields are named in
> `wave27_lane_gfxpart.cpp`'s `kRec_*` constants for the ten fields the Draw
> bodies read, and Update touches roughly twice that many), both unpromotable
> DATA rows in §4, and the unnamed trig pair. Re-derive its size with
> `get_function_by_address 0x1800a5600`.

### 6.2 The five remaining `??_7SunLight@Graphics@@6B@` bodies — NOT ATTEMPTED

`?Update@`, two `?Draw@` overloads, `?AddDrawItems@` and `?DrawShadowMap@`. Slot
0 (the deleting dtor) is ported, taking the table 5/11 → 6/11. The five were not
attempted: this lane spent its budget taking four tables to COMPLETE rather than
moving a fifth from 5/11 to 11/11, and **that was a scoping choice, not a
technical blocker** — nothing was found that makes them hard beyond their size.

Two of them are already partly read elsewhere in the tree:
`code/Game/Game/gameupdateloop.cpp:2171` and
`code/Game/Game/gameupdateloop_load.cpp:1747` record that `DrawShadowMap`
(0x180106bc0) is gated by `renderer+0x3c != 0` while `Draw` (0x1801067b0) is not.
`runtime/src/graphics/sunlight.cpp` already exists (Wave 9 lane GFX2).

> **ADDRESSEE: Wave 28's Graphics vtable lane.** Five bodies, all in
> `??_7SunLight@Graphics@@6B@` (DLL 0x180158e10), none tagged UNPROMOTABLE.
> Re-derive the table's state with `tools/runtime_census.py` and the per-slot
> split with `tools/mint_slot_sweep.py` (remember §1: the sweep's denominator is
> the RAW slot count and the census's is the NON-PURECALL count).

### 6.3 Declared cutover-surface growth — MEASURED FROM BOTH SIDES

**THE HEADLINE PERCENTAGE WENT DOWN. That is this lane working, not failing.**
Re-run at the end of the lane, in the same worktree, with the same command:

```
              before              after
TOTAL   1372 called / 1237 ported / 90.2%   ->   1376 called / 1237 ported / 89.9%
Graphics 113 called /   94 ported / 83.2%   ->    115 called /   94 ported / 81.7%
```

`ported` did not move **because none of these rows is counted until its vtable is
promoted** — that is what the PROMOTION BACKLOG is — while `called` grew by the
four doors this lane opened. This is the ceiling identity in
`reference_the_ceiling_percentage_is_a_moving_target.md` seen from the other
side: `called` is in both halves, so opening a door lowers the percentage.
**Score this lane on the table identities in §1, never on 89.9%.**

The four rows are NOT a guess about what grew — they were read out of
`runtime_census.py --csv` by grepping for this lane's own file name, and they
account for the delta exactly (Graphics +2, System +1, Video +1 = 4):

| newly `called`, still unported | namespace | kind |
|---|---|---|
| `?RemoveState@ParticleInstance@Graphics@@IEAAXXZ` | Graphics | BODY |
| `?_typeInfo@ParticleSystem@Graphics@@0VBaseTypeInfo@Object@@A` | Graphics | DATA |
| `?NextGroup@Random@System@@AEAAXXZ` | System | BODY |
| `?SetTransformScale@Context@Video@@QEAAXHAEBVMatrix@Math@@AEBVVector4@4@@Z` | Video | BODY |

(`?_rng@Particle@Graphics@@` was ALREADY `called` — `code/Game/Game/tornado.cpp`
holds the other door — so this lane did not grow that row, it joined it.)

### 6.3.1 And the growth the census CANNOT see (MEASURED)

This lane's bodies reach four DLL internals **by address** that no port
registers, so each becomes newly reachable through our code
(`reference_porting_can_grow_the_cutover_surface.md`):

| address | what it is | reached from |
|---|---|---|
| 0x180014520 | `Object::ExternalBasePtr<MaterialInstance>::Serialize` | `Particle::Serialize` |
| 0x180062110 | `Collection::Array<void*>::SetSize(this,int)` | `Particle::Serialize` |
| 0x180103bf0 | the pool-record constructor | `Particle::Serialize`, `PI::SetParticle` |
| 0x1800205a0 / 0x180020e90 / 0x180020d60 | emitter new / init / release | `PSI::SetParticle` |

Plus the two deleting-dtor rows make `??1Light@` (0x180058d20) and
`??1SunLight@` (0x180108160) reachable through our thunk.

**MEASURED, and it confirms the known blind spot rather than assuming it:**
after the ports, `runtime_census.py --csv` contains **no row for any of those
six addresses**, and no row for the two destructors either. Grepping the CSV
for each of `0x180014520`, `0x180062110`, `0x180103bf0`, `0x1800205a0`,
`0x180020e90`, `0x180020d60` returns zero hits. So the cutover surface really
grew by eight functions beyond the four in §6.3, and the board will not say so —
the same thing the `IO::PackageFile` row in `deleting_dtor.cpp` already recorded,
now measured a second time on a different family
(`reference_census_cannot_see_nonliteral_registrations.md`;
`DeletingDtorCore` reaches `~Class` through a table field, and `RtAddr<>` reaches
these six through a template rather than a literal at a `ResolveAddr` call site).

> **RESIDUAL 3 — ADDRESSEE: whoever runs Wave 27's cutover attempt 30.**
> `0x180062110` is a ready-made `J3_ENTRY2` row for
> `runtime/src/collection/array.cpp`: its body reads `SHL RCX,3 / MOV EDX,8` with
> a plain qword copy and an EMPTY else-arm in the copy loop, i.e. stride 8,
> align 8, new tail UNINITIALISED — `kSpec_Fill8` exactly, the same spec as the
> existing `0x18002a9d0` row. It was NOT added here because `array.cpp` is a
> shared file three lanes touch in this wave and an append there is a merge
> hazard for no gain before cutover. Re-derive with
> `disassemble_function 0x180062110` and compare against `kSpec_Fill8`'s
> definition in `array.cpp`.

---

## 6.4 THE GhidraMCP PROFILE/RETAIL DEFECT — CHECKED, AND THIS LANE IS CLEAN (MEASURED)

Mid-lane, the integrator relayed lane FRONTIER's finding that GhidraMCP can serve
the PROFILE DLL while `list_open_programs` reports retail as current, and that
`switch_program` can return `{"success": true}` while changing nothing.

**This lane ran the one-call discriminator, `disassemble_function(0x180002190)`,
on BOTH paths:**

| call | result |
|---|---|
| with `program="Runtime-x64.dll"` | `MOV RAX,[0x1801fae10] ; RET` — **2 instructions, RETAIL** |
| with no `program` argument | `MOV RAX,[0x1801fae10] ; RET` — **also RETAIL** |

(The second is itself evidence of the reported defect, in the opposite direction:
this lane had earlier called `switch_program("Runtime-x64-profile.dll")` and never
switched back, yet the implicit path still serves retail. `switch_program`'s
success return means nothing either way.)

**Exposure audit.** Every Ghidra call in this lane after the fourth passed
`program="Runtime-x64.dll"` explicitly, and the two calls that deliberately
targeted the profile build (`decompile_function 0x18011cdf0` for `~Particle`,
`search_functions_enhanced`) passed `program="Runtime-x64-profile.dll"`
explicitly. **Four calls were made with no `program` argument** — the two
deleting-dtor thunks and their two callees, before the bodies in §2.1 were
written. All four were **RE-TAKEN with `program="Runtime-x64.dll"` explicit and
diffed against the originals: byte-for-byte identical, all four.** Nothing in §2.1
changes.

Two independent corroborations, worth stating because "I re-ran the same tool" is
not by itself a second witness:

* `tools/rt_deleting_dtor_verify.py` re-derives both new rows' `dtorVA` and
  `sizeof` **from the shipped retail DLL's bytes, not through Ghidra**, and
  printed the same `0x180058d20 / 0xc0` and `0x180108160 / 0x108` this lane wrote.
* The `?TypeName@StringTable@UI@@` discriminator's target,
  `0x1801fae10`, and the export-name lookups this lane used throughout came from
  `pefile` reading `C:\GOG Games\Banished\Runtime-x64.dll` directly.

**No vtable offset was carried across builds in this lane.** The one place that
risk was live is the `IO::Encoding` / `IO::ReaderWriter` slot offsets used by
`Particle::Serialize` (+0x18/+0x20 create, +0x78 bool, +0xa0 int, +0x00 dtor) —
and those come from `runtime/types/io/Serializer.h`'s RETAIL constants, which
`runtime/src/graphics/wave24_lane_gfxmat.cpp` already re-confirmed against retail
at three call sites, not from any disassembly taken here.

`list_segments` was never called by this lane, so its reported section-bounds
defect had no opportunity to affect anything.

Per warning #1, none of this was fixed.

## 6.5 THE OVERLAPPING-BODY DEFECT — EVERY BODY IN THIS LANE HAS A SECOND WITNESS (MEASURED)

The integrator then relayed lane GFXBIG's second, worse finding:
`disassemble_function` can serve a **different function's listing under the
address that was asked for** (measured at `0x1800fdb20`, two overlapping bodies),
with output that is internally consistent and relabelled with the requested
address. It has **no natural tell**, and `program="Runtime-x64.dll"` does not
protect against it.

This lane is the most exposed in the wave: fourteen distinct function addresses,
and its float work is exactly the kind an overlapping-body mixup would corrupt
invisibly (a plausible but wrong term order reads as correct code).

**So every load-bearing region of all six ported bodies plus both deleting-dtor
thunks was re-taken with `objdump -d --start-address= --stop-address=` against
`C:/GOG Games/Banished/Runtime-x64.dll` — a different disassembler, addressing by
true VA, reading the retail file by explicit path.** The discriminator reproduced
first, as a control on the method itself:

```
$ objdump -d --start-address=0x180002190 --stop-address=0x1800021a0 "C:/GOG Games/Banished/Runtime-x64.dll"
   180002190:  48 8b 05 79 8c 1f 00   mov 0x1f8c79(%rip),%rax   # 0x1801fae10
   180002197:  c3                     ret
```

| region | address range | objdump verdict |
|---|---|---|
| Light deleting dtor, whole body | 0x180058dd0–0x180058e46 | **identical**, incl. `lea -0xde(%rip) # 0x180058d20`, `mov $0xc0,%edx`, and the scalar branch's `call 0x1800a41c0` (the §2.1 third shape) |
| SunLight deleting dtor | 0x180064620–0x180064691 | **identical** (also re-derived non-Ghidra by `rt_deleting_dtor_verify.py`) |
| `Particle::Serialize` codec + field chain + read gate | 0x180103f31–0x180103fc6 | **identical**: rw `+0x78` on +0x20, `+0xa0` on +0x24 and +0x28, `call 0x180014520` on +0x30, `jne` on `0x8(%rbx)`, `cmpb $0x0,0x20(%r15)`, `call 0x180062110` on +0x38 with `0x24(%r15)` |
| `GetInstanceTransform` w-column + opacity blend | 0x180104930–0x1801049b4 | **identical**: `subss %xmm9,%xmm13` / `mulss 0x7c` / `mulss 0x80` / `addss %xmm9,%xmm13` → `movss %xmm13,0x3c(%rdi)`, and `movss %xmm12,0xc/0x1c/0x2c(%rdi)` — **all three w's from the 1.0f register, confirming §2.7** |
| `AddDrawItems` distance + flag + opacity | 0x180104773–0x1801047e6 | **identical**: `addss %xmm0,%xmm2` then `addss %xmm1,%xmm2` → `(dy+dx)+dz`; `neg %al / sbb %ecx,%ecx / and $0x34,%ecx`; `0x5cef3(%rip) # 0x18016169c` |
| `ParticleInstance::Draw` per-particle float chain | 0x180104480–0x18010458e | **identical**: `imul $0x58,%rax,%rax`, `add 0x18(%rcx),%rax`, all six position lerps as subtract/scale/add-back, `mulss %xmm12` on **both** rotation terms before the add, and `movslq 0x20e8(%rbx)` (signed stride) |
| `PI::SetParticle` inlined MT19937 | 0x180104a20–0x180104ac8 | **identical**: `and $0xff3a58ad`, `and $0xffffdf8c`, shifts 11/7/15/18, `cmp $0x270 / cmove`, unsigned `div %edi`, and `shl %cl,%edx` (unmasked — hardware masks to &31) |
| `PSI::SetParticle` whole body | 0x1800a6a9d–0x1800a6b49 | **identical**, incl. `mov 0x152f85(%rip),%edx # 0x1801f9a58`, `cmovne %rbx,%rsi` (null on a false IsDerivedFrom), and `movslq 0xd0(%rdi)` |

**No discrepancy was found anywhere.** That is a clean result, not luck to be
recorded as a control — but it is also not a general claim: it says these ten
regions are right, not that the defect is absent.

`?Update@ParticleSystemInstance@` (§6.1) was read through
`disassemble_function` only and **was NOT objdump-confirmed** — it is deferred,
nothing was ported from it, and the §6.1 size figure should be re-taken with
objdump by whoever picks it up.

Per warning #1, the defect was not fixed.

---

## 7. INSTRUMENT DEFECT OBSERVED, NOT FIXED

Per warning #1, nothing below was fixed. §6.4 and §6.5 record two more — the
GhidraMCP profile/retail confusion and the overlapping-body mixup — each with
this lane's own measurement rather than a relayed claim.

**THE STANDING CONSEQUENCE, for whoever writes Wave 28's briefs:** two of the
three instrument defects this wave found are in `disassemble_function`, and
neither is detectable from its output. `objdump -d --start-address= --stop-address=`
against the retail file by explicit path is immune to both (different
disassembler, true-VA addressing, no program selection) and cost this lane about
ten minutes to re-confirm ten regions. **A decompilation lane should be told to
use objdump as the primary witness and Ghidra only for navigation.**

### 7.1 `tools/rt_deleting_dtor_verify.py` reports `BAD` for a CORRECT row

**File:line: `tools/rt_deleting_dtor_verify.py`, the scalar-branch classifier
(the code that prints `call-the-dtor (K1)` / `restamp-inline (K2/K3)` and the
message "scalar branch neither calls 0x... nor restamps a vtable").**

MEASURED output after this lane's two rows were added:

```
OK  0x180064620  size=0x108  dtor=0x180108160  Graphics::SunLight
BAD 0x180058dd0  size=0xc0   dtor=0x180058d20  Graphics::Light
        scalar branch neither calls 0x180058d20 nor restamps a vtable
call-the-dtor (K1): 21    restamp-inline (K2/K3): 24
problems: 1
```

The verdict is **correct about the bytes and wrong about the port.** The tool
knows two shapes; Light is a third (§2.1). The fix is a third classifier arm, and
it belongs to Wave 28.

**A red line in a tool's own output is the surface the next person reaches**
(warning #9), and this lane cannot change that surface without touching the tool.
So the correction was put in the file the tool PARSES, immediately above the row,
as an `EXPECTED RED, READ THIS BEFORE "FIXING" THE ROW` block naming the exact
message the tool prints. See `runtime/src/object/deleting_dtor.cpp`, the Wave 27
block above `RT_DELDTOR(0x180058dd0, ...)`.

### 7.2 `tools/mint_slot_sweep.py` and `tools/runtime_census.py` print the same
column name for different denominators

Both print a `slots` figure for the same vtable and they differ (10 vs 9 for
`??_7ParticleInstance@`, 11 vs 6 for `??_7Light@`) because one counts raw slots
and the other non-purecall slots. Neither says which. That reads as an instrument
disagreement and is not one (§1). **Not fixed**; recorded so the next reader does
not investigate a phantom.

---

## 8. VERIFICATION STATE — STATED HONESTLY

* **MEASURED:** `mingw32-make standalone-debug` exits 0, twice — once after the
  bodies and once after the `Particle.h` comment edits. `reimpl-x64.exe`
  33,129,705 bytes.
* **MEASURED:** every load-bearing region of all six bodies and both dtor thunks
  re-confirmed with `objdump` against the retail file on disk — a second
  disassembler, no discrepancy (§6.5). This is the strongest verification this
  lane has, and it is a verification of TRANSCRIPTION, not of BEHAVIOUR.
* **MEASURED:** the five tables' post-state, re-run rather than predicted (§1).
* **MEASURED:** every mangled name registered by this lane was read out of the
  shipped `Runtime-x64.dll` export directory with `pefile`, by VA, BEFORE the
  bodies were written — the guard against a phantom name that resolves NULL in
  silence.
* **MEASURED:** `tools/rt_deleting_dtor_verify.py` re-derives both new
  deleting-dtor rows' constants from the DLL and agrees on both (the Light row's
  `BAD` is about the classifier, not the constants — it printed the size and dtor
  this lane wrote).
* **NOT MEASURED — no oracle was run, and no run was made.** Four of the six new
  bodies dispatch ONLY through vtables that are not promoted, so with the DLL
  present nothing calls them: an attendance run would print zeros and a `calls=0`
  diff site would report clean, which is not verification
  (`reference_uncalled_port_is_not_verified.md`). The attendance line
  `[GFXPART27] …` is wired unconditionally into `standalone/main.cpp` so that the
  expected zero is PRINTED rather than inferred from silence.
* **The one oracle that IS available and was NOT built:** `Particle::Serialize`
  is a WRITE-path Serialize, and `runtime/src/io/save_trace.h` replays those into
  two independent synthetic sinks — the mechanism
  `runtime/src/achievement/goal_serialize.cpp` uses on exactly this shape. This
  lane did not build it.

> **RESIDUAL 4 — ADDRESSEE: Wave 28's Graphics vtable lane, as its FIRST task
> before adding any body.** Wire `Particle::Serialize` into
> `RtSaveTrace::Compare` the way `goal_serialize.cpp` does. Note the same
> adapter problem that file solved: `Compare` hands each side a
> `Serializer*` and `Particle::Serialize` takes a `Stream*`, so recover the
> stream from `Serializer.stream` (+0x00). The sink pins `direction = 1`
> (writing), which means the read-side allocation block is NOT covered — say so
> when reporting the result rather than letting a green line read as full
> coverage.

---

## 9. FILES CHANGED

| file | change |
|---|---|
| `runtime/src/graphics/wave27_lane_gfxpart.cpp` | NEW — six bodies + registration + attendance |
| `runtime/src/graphics/rt_graphics_port.cpp` | call `RtGraphicsWave27GfxpartRegister()` behind `BANISHED_RT_GFXPART`; identity line |
| `runtime/src/object/deleting_dtor.cpp` | two rows (Light, SunLight), two entry points, two registrations, and the EXPECTED-RED block |
| `runtime/src/object/deleting_dtor_internal.h` | `kSpecCount` 44 → 46 |
| `runtime/types/graphics/Particle.h` | measured meanings for +0x24/+0x28/+0x30/+0x38/+0x40 and the 0x40-byte record correction |
| `standalone/main.cpp` | unconditional `RtGraphicsWave27GfxpartSummary()` |
| `docs/runtime_port/WAVE27_LANE_GFXPART.md` | this file |
