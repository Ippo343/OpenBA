# WAVE 9 LANE GFX2 — the `Graphics` cluster, picked up where lane GFX declined it

**Base:** `29cfaa2` (the census macro-door fix, on top of `309c015` attempt 14).
**Kill switch:** `BANISHED_RT_GFXSUN=0` removes the SunLight half; the RenderBuffer
half rides `BANISHED_RT_VIDBUFFERS` with the rest of lane C's file.
Date: 2026-09-08.

---

## 0. The stale-worktree check — seven for seven now

This worktree was created at **`965361d`**, which predates the entire runtime port:
`ls docs/runtime_port/` failed on the first command. `git reset --hard` onto
`wave/upgrade-path-complete` (`309c015`), then fast-forwarded onto `29cfaa2` when the
integrator flagged the census fix. **Nothing below was concluded from the stale tree.**
Lane FS counted six lanes hitting this; this is the seventh. It is a harness defect.

---

## 1. Verdict table — the six symbols handed to this lane

| # | Symbol | Retail | Verdict |
|---|---|---|---|
| 1 | `??0SunLight@Graphics@@QEAA@XZ` | `0x1801081f0` | **PORTED — closes the row** (§3, §4) |
| 2 | `?Set@SunLight@Graphics@@QEAAXAEBVVector3@Math@@AEBVVector4@4@1@Z` | `0x180107f10` | **PORTED — closes the row**, 1,786 diffed calls, 0 divergences (§3) |
| 3 | `?Draw@Renderer@Graphics@@…` | `0x180104bb0` | **DECLINED — blocker re-checked and still live** (§6.1) |
| 4 | `?BuildParameters@MaterialInstance@Graphics@@QEAAXXZ` | `0x1801033e0` | **DECLINED — blocker re-checked and still live** (§6.2) |
| 5 | `?Update@AnimationManager@Graphics@@QEAAXAEBVTime@System@@_N@Z` | `0x1800fc240` | **DECLINED — blocker re-checked and still live** (§6.3) |
| 6 | `?Serialize@MeshInstance@Graphics@@QEAAXAEAVSerializer@IO@@@Z` | `0x1800ff680` | **DECLINED — blocker re-checked and still live** (§6.4) |

Ported to get #1 and #2 there, neither of them on the residue list:

| Symbol | Retail | Why |
|---|---|---|
| `?PlatformConstruct@RenderBuffer@Video@@AEAAXXZ` | `0x1800f4320` | lane GFX's *stated* blocker for #1 (§2) |
| `?PlatformDestruct@RenderBuffer@Video@@AEAAXXZ` | `0x1800f4400` | same |
| `??1SunLight@Graphics@@UEAA@XZ` | `0x180108160` | reached through a MINTED vtable, so invisible to `BANISHED_RT_STRICT`; also the teardown both halves of the ctor probe need (§4) |

**So: 2 of 6 closed, 4 declined with the blocker re-checked rather than re-read, plus 3
supporting ports.** The four declines are the four lane GFX and lane F declined before
me; I did not find a new way through any of them, and §6 says exactly what I checked.

---

## 2. `Video::RenderBuffer::Platform{Construct,Destruct}` — the fourth member of a family of three

Lane GFX §6.5's recommended order was *"`RenderBuffer::PlatformConstruct` /
`PlatformDestruct` first (they close two rows of their own and unblock lane VID's
neighbourhood), then the `SunLight` pair, which then closes without moving anything."*
That is exactly what this lane did, and the first half was nearly free, because Wave 9
lane W had already built the machine: `runtime/src/video/buffers.cpp` implements
`Platform{Construct,Destruct}` as ONE body parameterised by a four-field `BufferClass`
descriptor, instantiated three times. This adds a fourth instantiation.

Both disassemblies were transcribed against the shared body instruction for instruction
and agree with the other three **with no exception at all** — which is why this is a
descriptor and two wrappers, not new logic.

```
kRenderClass = { vtSlot 0x98, companionOff 0x28, nullAfterRelease false,
                 links {next 0x18, prev 0x20}, head _firstRenderBuffer }
```

Three of those four values were cross-checked against something other than the function
being transcribed:

* **`vtSlot 0x98`** — `dx11/src/devicedata.cpp:2148` registers slot **19** as
  `Factory_RenderBufferData`, and 19 × 8 = 0x98. VideoDX11 is already ours, so this
  dispatch lands in the project on both sides of the port, exactly as the other three do.
* **`links {0x18, 0x20}`** — read out of `0x1800f4381..0x1800f43bd` (retail loads the tail
  as `[head+0x20]` and writes `this+0x18 = head`, i.e. `prev = 0x20`, `next = 0x18`), then
  **independently confirmed against the class's own list WALKER**:
  `runtime/src/video/dynamic_resources.cpp` records that all four `RecreateDynamic`
  walkers step the `prev` field (ImageBuffer +0x58, Index/Vertex +0x18, RenderBuffer
  +0x20). +0x20 is exactly what this assignment calls `prev`. A swap would have shown up
  as the RenderBuffer walker being the only one of four to step `next`.
* **`nullAfterRelease false`** — `0x1800f4400` releases the companion at +0x28 and leaves
  the freed pointer there. That is the *ImageBuffer* asymmetry, not the Index/Vertex one,
  so the destruct oracle needs the "null it, run theirs, put the dangling value back"
  dance and it has it.

`?_firstRenderBuffer@RenderBuffer@Video@@0PEAV12@EA` is already minted
(`rt_data_mint_tables.inc`, `0x1801fe790`) and already read PORT-AWARE by lane VID, so
our linkage and lane VID's walkers agree on ONE list on both sides of the cutover.

### Measured (run `gfx2P`, §9)

```
Video::RenderBuffer::PlatformConstruct                        calls=3   divergences=0
Video::RenderBuffer::PlatformConstruct[branch:appended]       calls=3
Video::RenderBuffer::PlatformConstruct[branch:first-in-list]  calls=0   <- see below
Video::RenderBuffer::PlatformConstruct[PROBE:first-in-list]   calls=1   divergences=0
Video::RenderBuffer::PlatformDestruct                         calls=1   divergences=0
Video::RenderBuffer::PlatformDestruct[branch:unlinked]        calls=1
Video::RenderBuffer::PlatformDestruct[branch:list-emptied]    calls=0   <- see below
Video::RenderBuffer::PlatformDestruct[PROBE:list-emptied]     calls=1   divergences=0
```

The two `calls=0` branch rows are exactly the hole lane W's empty-list probe exists to
fill, and it fills them: with the DLL present the shipped `Video::Device` startup has
already put objects on the list, so every live call takes `appended`/`unlinked`. **At
cutover that inverts** — the mint owns the head, it starts NULL, and the first
RenderBuffer in the process is the one `SunLight`'s constructor builds, which takes the
branch no live call ever takes. The probe compares that branch against the shipped body
on a synthetic node under the Device lock, and it now runs for RenderBuffer too.

---

## 3. `SunLight::Set` — and a correction to lane GFX's stated live path

### 3.1 The fact that decides the shape of all three bodies

Both `Set` and the constructor open by asking the device a yes/no question through
`Video::Device::_singleton->deviceData` **vtable slot 6** (`+0x30`). That answer decides
whether a colour shadow target exists.

**It is a hardcoded `false`.** Verified two independent ways rather than assumed:

1. The shipped `VideoDX11-x64.dll`'s `DeviceData` vtable is at `0x180018bb0`. The eight
   bytes at `0x180018be0` (slot 6) read `40 54 00 80 01 00 00 00` = `0x180005440`, and
   `0x180005440` is the two-byte body `32 c0 / c3` — `xor al,al; ret`.
2. VideoDX11 is already replaced and never loaded; **our** slot 6 is
   `dx11/src/devicedata_stubs.cpp:10`, whose header records the same two bytes and which
   returns false.

Consequences, on both sides of the port:

* the ctor's capability-gated block is DEAD — `_colourTarget` (+0xF0) is never built;
* `Set`'s first rebuild branch needs `cap == true` → unreachable;
* `Set`'s second needs `cap == false && _colourTarget != 0` → unreachable, because
  nothing else ever writes +0xF0.

**`SunLight::Set` is therefore exactly eleven dword copies, every call.**

### 3.2 The correction

WAVE9_LANE_GFX.md §6.5 wrote that the rebuild branches are skipped *"with the capability
present and +0xf0 non-null (the state the ctor leaves)"*. The conclusion is right and the
stated condition is **the exact inverse of the live one**: the capability is absent, and
the state the ctor leaves is `+0xf0` **NULL**. A later lane following that sentence would
look for the capability to be true, find it false, and conclude the analysis was wrong.

Recorded rather than quietly fixed, because it is the third time this wave that a
holdback's *stated* reason and its *real* reason diverged
(`reference_holdback_blockers_are_stale_check_them_first.md`).

A second correction from the same section: lane GFX named the two image buffers
*"depth `ImageBuffer*` +0xf0, colour `ImageBuffer*` +0xf8"*. They are the other way
round. `+0xf0` is built with `Usage = 0` (render target) and `+0xf8` with `Usage = 1`
(depth), and `RenderBuffer::Create(rb, +0xf0, +0xf8)` lands them in a struct whose layout
is `{colorBuffer @ +0x00, depthBuffer @ +0x08}`. That ordering is also what makes the
capability gate sensible: the **colour** target is the optional one and the depth target
is unconditional, which is how every shadow-map path works.

### 3.3 The oracle, and its one refusal

On the copy-only path this is a pure write of three inputs our body never touches, so it
replays: snapshot the eleven dwords, run ours, read back, restore the snapshot, run the
shipped body, compare. The three source vectors are the caller's and are written by
neither side, so `reference_oracle_must_snapshot_aliased_inputs.md` does not bite — but
the DESTINATION is `this`, live state, which is why the snapshot is restored *before* the
shipped body runs. The object is deliberately left holding **retail's** values, the same
way round lane W's oracle leaves the list.

If a rebuild branch ever ran the diff is **REFUSED**, on its own site, not faked: a replay
would destroy and rebuild GPU resources a second time and "compare" two different objects.

```
Graphics::SunLight::Set                                   calls=1786  worked=1786  divergences=0
Graphics::SunLight::Set[branch:build-colour-target, DEAD] calls=0
Graphics::SunLight::Set[branch:drop-colour-target, DEAD]  calls=0
```

`worked=1786` of `calls=1786` is the effectiveness control
(`reference_probe_needs_an_effectiveness_control.md`): every one of those calls did the
eleven copies and was compared, none took a vacuous early-out. The two `calls=0` rows are
the *measured* form of the unreachability claim in §3.1 rather than an assertion.

---

## 4. `SunLight::SunLight` and `~SunLight`

The ctor is: base ctor, two vtable stamps into ONE slot (`Light`'s then `SunLight`'s —
kept as two stores, because an intervening throw would be observed with `Light`'s
installed), eleven scalar defaults, three null pointers, `_materialTypeFlags = 0x8000`,
the capability-gated colour target, the unconditional depth target, the render buffer,
and an **inlined** `Geometry::SetBounds(Box(-1e6…, +1e6…))`.

Layout recovered in `runtime/types/graphics/SunLight.h`, `sizeof == 0x108`. Two things
in it are load-bearing:

* **`Graphics::Light` contributes no bytes.** The ctor stamps both vtables into the same
  slot and its next store is at `+0xC0`, which is `sizeof(Geometry)`. Single inheritance,
  vtable override only.
* **`+0xEC` is a hole that NO body writes** — the ctor goes `+0xe8 → +0xf0` and skips it,
  `Set` stops at `+0xe8`. The probe excludes it; comparing it would compare whatever the
  allocator left there, which is
  `reference_oracle_must_not_compare_unwritten_bytes.md` exactly.

The two bounds constants are lifted as **raw bits** and not retyped from a decimal:
`DAT_18016173c = 0x49742400` (+1e6, the max) and `DAT_180161778 = 0xc9742400` (−1e6, the
min), read with `read_memory` rather than from Ghidra's rendering.

### 4.1 The probe — because a replay is impossible and a probe is not

Lane F declined the ctor with *"no oracle exists and constructing one is not free"* —
running the shipped ctor over a live `SunLight` overwrites `_depthTarget` and
`_renderBuffer` with a second set, orphaning 2048×2048 of GPU memory and leaving a stale
node on `_firstRenderBuffer`. That is true of a REPLAY. It is not true of a **synthetic
subject** (`reference_synthetic_subject_beats_a_fake.md`): each side constructs into its
own zeroed 0x108 scratch and is torn down by the matching destructor before the next step.

It is also much cheaper than lane F assumed, *because* of §3.1: with slot 6 answering
false only ONE image buffer is built, not two.

What is compared, once per process, under `BANISHED_RT_DIFF` only:

| Site | What it compares | Result | Under `=neg` |
|---|---|---|---|
| `[PROBE:object fields, ours vs shipped]` | the whole 0x108 object except `+0xEC` and the three pointer VALUES (reduced to a null/non-null verdict) — so Geometry's own ctor output, the bounds the SetBounds tail rewrites, `_dirtyFlags`, `_materialTypeFlags` and the eleven scalars are all in | `calls=1 divergences=0` | **RED** |
| `[PROBE:depth-target Create tuple, ours vs shipped]` | the depth target's ten scalar fields `+0x20..+0x44` (w, h, dimension, kind, format, flags, addressU/V/W, filter) — **this is what actually verifies the eleven-argument `ImageBuffer::Create` tuple**, the most error-prone thing this port transcribes. Every one of the ten is written by ImageBuffer's ctor or by Create, so none is an unwritten byte | `calls=1 divergences=0` | **RED** |
| `[PROBE:_firstRenderBuffer restored by each dtor]` | the global list head at entry vs after ours' teardown vs after theirs' | `calls=1 divergences=0` | **RED** |

Every comparison in this lane is **bitwise with tolerance zero** — `RT_DIFF_BYTES` over
the 44 bytes for `Set`, `memcmp` over the object and over the ten `ImageBuffer` scalars
for the probe. Nothing here observes a float through an integer cast, a quantiser or a
threshold, so the hole lane PICK found in its grid comparison (a 1-ULP AABB error
swallowed by `(int)(f * invCellSize)`) does not exist in this oracle.

Branch coverage from the same run:

```
Graphics::SunLight::SunLight[branch:bounds differed -> written]  calls=3
Graphics::SunLight::SunLight[branch:bounds already equal -> skipped] calls=0
Graphics::SunLight::SunLight[branch:colour-target built, DEAD]   calls=0
```

`bounds differed` 3 of 3 is not luck: `Geometry::Geometry` leaves `_localBounds` at the
empty-box sentinel ±1e8, so the SetBounds tail's compare can never match ±1e6 from a
constructor. The "already equal" arm is unreachable **from this caller** and the counter
says so instead of the doc asserting it.

### 4.2 `~SunLight`: no oracle, and the honest reason

It frees. Nothing to replay. It IS executed on the ours side of the probe, which is what
stops it being wholly unexercised code, and its `calls=1` is that execution and nothing
more. Its live path did not reach us this run: with the DLL present the exe destroys a
SunLight through the retail vtable's slot-0 deleting destructor, which calls
`0x180108160` with a direct intra-DLL `CALL` that our registry cannot intercept. **At
cutover the minted `??_7SunLight@Graphics@@6B@` is what dispatches, so the port matters
then and only then** — which is also why it is ported at all despite not being on the
residue list.

---

## 5. THE DEFECT THE ORACLE CAUGHT, and it would have shipped

The first diff run **crashed**, in `ImageBufferCreate` reading a vtable at `-1`
(`standalone_crash.log`: `Fault: READ of ffffffffffffffff`, RIP inside
`ImageBufferCreate+0x76`, with `R8=1 R9=7 RSI=0x40` — this file's own depth-target
argument tuple, which is how the site was identified before any symbolisation).

The cause is a **port-seam ABI hazard** worth carrying as a rule:

> MSVC returns `this` in RAX from a constructor, so retail's `CALL ctor; MOV RCX,RAX` is
> correct **against the shipped body**. But `??0ImageBuffer@Video@@QEAA@XZ` is PORTED, and
> our port is declared `void Video_ImageBuffer_Ctor(ImageBuffer*)`
> (`runtime/src/video/buffers.cpp:387`) — a void-returning C++ function whose RAX is
> whatever the last expression left there. **Reading a ported constructor's return value
> reads garbage.**

The convention is not uniform across the tree, which is exactly why it cannot be relied
on: `RtGraphics_Geometry_Ctor` *does* return `Geometry*`, `Video_ImageBuffer_Ctor` does
not. The fix is to use the pointer `operator new` returned, which is the same value by
construction and independent of how any port spells its signature. `~SunLight`'s
tail-jump into `~Geometry` is the same hazard read the other way round and is now
spelled as a plain call with a void return.

This is a byte-faithful transcription that was WRONG because faithfulness was measured
against retail's callee and the callee had been replaced. A run with the oracle off would
have crashed identically, but a run with the oracle off would not have told anyone why.

---

## 6. The four declines — blockers RE-CHECKED, not re-read

The project rule is that a holdback's stated blocker is checked before the holdback is
believed (`reference_holdback_blockers_are_stale_check_them_first.md`), and three-for-three
in one earlier session those blockers had already dissolved. Here they had not. Each row
below names the check that was actually run.

### 6.1 `Renderer::Draw` (`0x180104bb0`)
Lane GFX declined on five unported callees, the worst being `FUN_180062d60`, 0x2306 bytes
of unexported per-pass sort. **Checked:** grepped all five addresses
(`0x180105110`, `0x180105700`, `0x180104fd0`, `0x1801017a0`, `0x180062d60`) across
`runtime/`, `standalone/`, `code/` and `engine/`. Every hit is a COMMENT; none is a port
and none is a `RegisterRuntimeAddr`. The blocker stands unchanged. Porting `Draw` alone
would move the residue row from a named export to an unexported internal, which is a
*worse* row — the census cannot report a name for it.

### 6.2 `MaterialInstance::BuildParameters` (`0x1801033e0`)
Lane F and lane GFX declined on `Graphics::Material` being a class no lane has laid out.
**Checked:** `runtime/types/graphics/Material.h` exists but pins only
`MaterialInstance` (0x60) and its four entry structs — `MaterialTexture`,
`MaterialBinding`, `MaterialParameterSet`, `MaterialConstant`. There is no
`struct Material` anywhere in `runtime/types/` or `engine/types/`. The blocker stands.
It still belongs to a lane paired with `Graphics::Material`.

### 6.3 `AnimationManager::Update` (`0x1800fc240`)
Lane GFX showed BOTH of lane G's stated grounds were false on the facts and declined
anyway, on the real one: `AnimationState::Update` (`0x1800fdb20`, 266 instructions,
exported) is unported. **Checked:** `0x1800fdb20` appears nowhere in the tree. The blocker
stands. This is the hottest skeleton path in the game and the next lane should start from
lane GFX's argument map, which is correct and complete.

### 6.4 `MeshInstance::Serialize` (`0x1800ff680`)
Twenty instructions, and genuinely tempting — on the WRITE path a `Serialize` is a pure
read that replays into two sinks. Declined on three unported callees. **Checked:**
`0x1800fe250` (serialize the mesh handle), `0x18005cc60` (`Array::SetCount`) and
`0x18005d4a0` (serialize N array elements) appear nowhere in the tree — in particular
lane Q's Collection array work did not pick up the two array-**serialisation** bodies. The
blocker stands, and this one is the save/load stream: a serializer that consumes the wrong
byte count does not fail where it is wrong, it desyncs and surfaces on an unrelated record.
It should go to a lane that runs a save/load round trip, and that lane must use **retail's**
`IO::Encoding` vtable offsets, not the profile build's
(`reference_encoding_vtable_offsets_differ_between_builds.md`).

---

## 7. The seven `scene.cpp` address doors — the verdict the integrator asked for

From `docs/runtime_port/wave9_unregistered_addr_doors.csv` (`ce1d8e9`, corrected in
`4ef55b6`; `0x180154e60` correctly dropped, it is minted).

**All seven are HARD cutover dependencies, not a watch list.**

| Address | Wrapper | Live caller in a PORTED body | Exported? |
|---|---|---|---|
| `0x180009c30` | `RtFreeLocked` | `Scene::RemoveGlobalObject` (scene.cpp:1586) | no |
| `0x18001f470` | `RtBoxTransform` | `Geometry::Clean` (scene.cpp:971) — **per frame** | no |
| `0x18005e4a0` | `RtSceneGridInit` | `Scene::Scene` (scene.cpp:1247) | no |
| `0x18005e5e0` | `RtSceneGridAdd` | `Scene::AddObject` (scene.cpp:1400) | no |
| `0x18005f090` | `RtSceneGridUnthread` | `Scene::RemoveObject` (scene.cpp:1454) | no |
| `0x18005ee30` | `RtSceneGridReindex` | `Scene::Clean` (scene.cpp:1656) — **per frame** | no |
| `0x18005f1f0` | `RtSceneFreeGlobals` | `Scene::~Scene` (scene.cpp:1343) | no |

* **None is exported** — `tools/pe_exports.py` over the shipped DLL returns nothing for
  any of the seven. So no name-based instrument (census, residue, `unsatisfied_export_scan`)
  can see them; the call site never mentions a name at all, which is the deepest form of
  the door problem this wave keeps rediscovering.
* **Every one has a live, non-diff-guarded caller**, listed above. With the DLL absent each
  is a call through null on the first scene built or the first frame drawn.
* `0x18001f470` is now covered: **lane PICK owns the body and the registration** (integrator
  ruling). This lane wrote no body for it and registers nothing.

**What this lane DID change:** six of the seven (plus `Quaternion::Lerp` `0x18001d500`)
cached their resolve as `static Fn fn = ResolveAddr(...)`, which **caches a null forever**.
That is the second half of the defect commit `309c015` found in this same file and fixed
only for `RtMemAlloc`. All eight now cache a hit and retry a miss, so registering them
later is a one-line change rather than an ordering lottery. **This does not make them
resolve** — they are unexported internals with no port — and saying otherwise would be the
"converting a door is not satisfying it" error.

Cheapest of the six remaining: `0x180009c30` is literally
`if (p) { Enter; Free(_singleton, p); Leave; }`, i.e. the `MemFree` static that `309c015`
already registered, with an ignored first parameter.

---

## 8. Audit numbers, before and after, with denominators

"Before" measured on a clean `git archive` of this lane's base into a scratch tree, so the
delta is this lane's and not another lane's.

| Audit | Before (`29cfaa2`) | After |
|---|---|---|
| `tools/unsatisfied_export_scan.py` | 789 names through a door; **149** unsatisfied; **146** with a site outside a diff guard | 795 names; **147**; **144** |
| … set-diff on the 146/144 | — | **CLOSED 2:** `??0SunLight@Graphics@@QEAA@XZ`, `?Set@SunLight@Graphics@@…`. **NEW 0.** |
| `tools/rawdoor_audit.py` | 501 sites, **0 UNGUARDED-CALL**, 36 DEFERRED-CALL | 506 sites, **0**, 36 |
| `tools/port_bypass_audit.py` ADDRESS door | 209 sites, **0** reach an already-ported fn; ADDR-REGISTERED 33, DETOURED 3, GUARDED-FALLBACK 1, NO-EXPORT 96, UNPORTED 76 | 209 sites, **0**; GUARDED-FALLBACK **1 -> 4**, NO-EXPORT **96 -> 93**, the other three unchanged. That shift is entirely the §7 change: three of the eight `static Fn fn = ...` sites now read `if (!fn) fn = ...`, which the audit classifies as a guarded fallback rather than a bare no-export call. No site moved into or out of BYPASS. |
| … NAME door | 429 raw-door sites, **0 BYPASS** on a live path | 434 sites, **0** |
| … by-name cross-check | 670 ported names / 417 raw-door statements / **0** the parser missed | 675 / 422 / **0** |
| `tools/export_name_audit.py` | 662 names, 1 not in the DLL (`?_singleton@...`, a prose ellipsis in `rt_resolve_cache.cpp:8`) | 665 names, the **same 1** |
| `tools/abi_retslot_audit.py` | — | **0 suspicious of 48** by-value-returning exports |

### The set-diff needed a second pass, and that is the finding

The **first** run of the after-scan read `146 → 146`, with 2 closed and **2 NEW**:
`??_7Light@Graphics@@6B@` and `??_7SunLight@Graphics@@6B@`, the two vtables the ctor
stamps. Those are exactly the "a minted vtable is blind to what its own minted vtables
READ" shape (`reference_mint_population_rule_blind_to_its_own_output.md`) — my new source
was the first thing in the tree to name them as literals, so they became unsatisfied the
moment I wrote them. Re-running `tools/rt_mint_gen.py` mints both (`0x180158d30` and
`0x180158e10`, 11 slots each, tail-not-a-pointer) and the set-diff becomes **2 closed, 0
new** on an unmoved denominator. `standalone/rt_data_mint_tables.inc` is regenerated in
this lane's commit.

**Quoting "146 → 144, 2 closed" without the set-diff would have hidden two new rows I had
just created.** That is the accounting lane FIND asked for and it earned its keep here.

---

## 9. Provenance — six lanes, one game directory, and a run that lied

Every number in §2, §3 and §4 comes from a matched PAIR of runs on the **same binary**,
`md5 b56517fce2624704806fb4f56d35db4d`, taken under `tools/game_dir_lock.sh` held as
`lane-GFX2`:

| Run | Config | This lane's result | Whole run |
|---|---|---|---|
| `gfx2P` | `BANISHED_RT_DIFF=1` | every site **0 divergences** | 409 sites, 15,940,158 calls, **1** divergence in 1 site |
| `gfx2N` | `+ BANISHED_RT_SUNLIGHT_PROBE=neg BANISHED_RT_EMPTYLIST_PROBE=neg` | all **five** probe sites RED (this lane's three plus lane W's two for RenderBuffer) | 409 sites, 18,472,756 calls, **12** divergences in 12 sites |

A control and a measurement on the same binary is the pair this project asks for: the
green run cannot be a probe that compares nothing, because the red run proves the same
probes fire.

### The negative control had to be extended once, and the first version proved it

Version 1 perturbed only the object-field copy. It went RED on `[PROBE:object fields]` and
on lane W's two RenderBuffer probes — and left `[PROBE:depth-target Create tuple]` and
`[PROBE:_firstRenderBuffer restored]` GREEN. Those two were therefore still
indistinguishable from probes that compare nothing, in the very run that was supposed to
establish that they are not. Version 2 perturbs all three inputs and all four go RED.
**A negative control that does not perturb every site it is supposed to cover is a partial
control that reads like a complete one.**

### The first attempt at this measurement was VOID and looked clean

`rt_run.sh` reported a normal exit and a full `[RtDiff]` summary with 0 divergences, and
**not one of this lane's sites was in it** — another lane had overwritten
`reimpl-x64.exe` between the `cp` and the launch. A tail of that summary reads exactly
like a passing run. Four hazards were observed, and they defeat a naive "check the mtime"
or even "check the md5" rule:

1. **The on-disk md5 says which exe is on disk when you look, not which exe ran.** The
   acceptance test that works is an **identity line emitted by this build alone, read out
   of the run's own log**. Ours is the SunLight branch-site name containing the literal
   `xor al,al;ret`. This lane's scratch wrapper (not committed — it was a
   throwaway around `tools/game_dir_lock.sh` + `tools/rt_run.sh`) VOIDed the run
   outright if that line was absent, which is the behaviour worth copying into
   `rt_run.sh` itself: **a per-lane identity gate belongs in the shared runner, not
   in six private wrappers, because the lanes that skip it are the ones that get
   clobbered.** Both runs above
   passed that gate.
2. **`cp` can fail while an md5 check still passes**, because the previous deploy of the
   same exe is still in place ("Device or resource busy" happened twice here). The copy
   must be checked for success.
3. **Windows locks a running image**, so a successful overwrite proves the other lane's
   process had *already exited* — which pins the race to the window between `cp` and
   launch rather than during the run.
4. **`rt_run.sh` deletes the three logs at start and kills by IMAGE NAME on timeout**, so
   a lane starting its run wipes another lane's logs and kills another lane's process
   mid-measurement. Two attempts here died that way (25 s exits, 100-line logs).

Logs are copied out of the game directory **before** the lock is released.

**The one divergence in `gfx2P` is not this lane's.**
`Object::Factory::Startup [probe, not diffable]  ours=chain=416 nodes=415  theirs=chain=416 nodes=416`
— somebody else's probe off by one. It appears identically in the void first run, on a
different lane's binary, which is what identifies it as pre-existing.

---

## 10. What this lane could NOT verify — stated, not buried

* **The ctor's own `RT_DIFF_BEGIN` site reports `calls=3 worked=3` with a description, not
  a comparison.** The comparison lives on the three PROBE sites. A late edit made the site
  report rather than print `calls=0`, because `calls=0` on a registered port is this
  project's standard read for "unverified" — but the number is entry count, and the doc
  says so here rather than letting the summary imply three verified constructions.
* **`~SunLight`'s live path was never exercised** — `calls=1` is the probe's own teardown.
  See §4.2 for why, and why it only matters at cutover.
* **The two dead rebuild branches in `Set` and the dead colour-target block in the ctor are
  TRANSCRIBED BUT NEVER EXECUTED**, on any run, by construction (§3.1). They are faithful
  to the disassembly and they have no oracle. A DX9 device would answer slot 6 differently
  and they would come alive; nothing in this project can produce that today.
* **The `ImageBuffer::Create` argument tuple is verified only through the DEPTH target.**
  The colour target is null on both sides, so its tuple — `(0, 0, 5, …)` — is transcribed
  and never compared.

---

## 11. A tool finding: `rawdoor_audit.py` loses its function-level guard to a brace initialiser

`diff_only_fn_depth` is set when the scanner sees `if (!RtDiffEnabled()) return;` and is
cleared by ANY `}` that brings the brace depth back to the function body's own level —
**including the closing brace of a one-line aggregate initialiser** such as
`static RtDiffSite s = { "name" };`. So a diff-only helper that opens with the early-out
and then declares a function-local `RtDiffSite` has its `RT_ORIGINAL_ONCE` calls reported
as UNGUARDED-CALL despite being unreachable without the oracle.

This lane hit it, and worked around it by moving the three sites to namespace scope, which
is better code anyway (it is what `buffers.cpp`'s probe does) — so the tree is at **0
UNGUARDED-CALL**. The blind spot is reported rather than papered over: the next probe
written in the obvious style will hit it too, and the temptation will be to reach for the
ALLOWLIST, which is how a checker gets talked out of a true finding.

---

## 12. Files

| File | Change |
|---|---|
| `runtime/types/graphics/SunLight.h` | NEW — 0x108 layout, `+0xEC` hole documented, the two lane-GFX corrections |
| `runtime/src/graphics/sunlight.cpp` | NEW — ctor, `Set`, `~SunLight`, the ctor probe, registration |
| `runtime/src/graphics/rt_graphics_port.cpp` | `+ BANISHED_RT_GFXSUN` / `RtGraphicsSunLightRegister()` |
| `runtime/src/video/buffers.cpp` | `+ kRenderClass`, the two RenderBuffer bodies, registration |
| `runtime/src/graphics/scene.cpp` | eight cached resolves now retry a null; the §7 verdict block |
| `standalone/rt_data_mint_tables.inc` | regenerated — mints the `Light` / `SunLight` vtables |
