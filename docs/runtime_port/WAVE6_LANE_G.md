# Wave 6, lane G — `Graphics::Animation*` / `Particle*` / `FrameCopyImageBuffer`

Branch `wave/upgrade-path-complete`, 2026-09-07. Scope:
`docs/runtime_port/wave6_lanes.txt` "=== LANE G ===", 18 rows (15 code exports + 3 data
exports). Every claim below is **MEASURED** (read out of the retail binary, its export
directory via `pefile`, or a `ghidra-mcp` decompile/disassembly against `program=
"Runtime-x64.dll"`) unless marked **UNVERIFIED**.

This lane did not run the build or the game (PLAN §6 / the orchestrator owns that); every
oracle below is code that compiles but has not executed.

## 1. Result table

| # | Export | Retail | Ord | Status | Oracle |
|---|---|---|---|---|---|
| 1 | `??0AnimationEvent@Graphics@@QEAA@XZ` | `0x1800a2150` | 41 | **PORTED** | FULL, scratch-object |
| 2 | `??1AnimationEvent@Graphics@@UEAA@XZ` | `0x1800a2120` | 526 | **PORTED** | structural only (see §3) |
| 3 | `??0AnimationManager@Graphics@@QEAA@XZ` | `0x1800fc430` | 45 | **PORTED** | FULL on the MT recurrence (seed itself excluded, see §3) |
| 4 | `??1AnimationManager@Graphics@@QEAA@XZ` | `0x1800fc400` | 528 | **PORTED** | post-state only |
| 5 | `?Update@AnimationManager@Graphics@@…` | `0x1800fc240` | 4473 | **HELD BACK** | — see §4 |
| 6 | `?Serialize@AnimationManager@Graphics@@…` | `0x1800fc190` | 3623 | **HELD BACK** | — see §4 |
| 7 | `??0Particle@Graphics@@QEAA@XZ` | `0x180103e80` | 298 | **PORTED** | FULL, scratch-object minus a measured never-written gap (see §7) |
| 8 | `??0ParticleInstance@Graphics@@QEAA@XZ` | `0x180104130` | 300 | **PORTED** | FULL over the fields this class adds; Geometry base excluded (see §7) |
| 9 | `?Update@ParticleInstance@Graphics@@…` | `0x180104180` | 4498 | **PORTED** | idempotent replay |
| 10 | `?IncrementInstanceIndex@ParticleInstance@Graphics@@SAXXZ` | `0x1800201f0` | 3065 | **PORTED** | FULL (same-slot replay) |
| 11 | `??0ParticleSystemInstance@Graphics@@QEAA@XZ` | `0x1800a55a0` | 304 | **PORTED** | FULL over the fields this class adds; Geometry base excluded (see §7) |
| 12 | `??0FrameCopyImageBuffer@Graphics@@QEAA@XZ` | `0x1800a3d10` | 198 | **HELD BACK** | — see §4 |
| 13 | `??1FrameCopyImageBuffer@Graphics@@UEAA@XZ` | `0x1800a3c80` | 597 | **HELD BACK** | — see §4 |
| 14 | `?Create@FrameCopyImageBuffer@Graphics@@…` | `0x1800a3b70` | 1877 | **HELD BACK** | — see §4 |
| 15 | `?Generate@FrameCopyImageBuffer@Graphics@@…` | `0x1800a3b20` | 2395 | **HELD BACK** | — see §4 |
| 16 | `?_frameIndex@ParticleInstance@Graphics@@1HA` | `0x1801fe77c` | 4790 | **DATA, not registrable** | already resolved elsewhere, see §5 |
| 17 | `?_rng@Particle@Graphics@@1VRandom@System@@A` | `0x1801fe7f0` | 4795 | **DATA, not registrable** | see §5 |
| 18 | `?_typeInfo@Particle@Graphics@@0VBaseTypeInfo@Object@@A` | `0x1801fa7b0` | 4891 | **DATA, not registrable** | already resolved elsewhere, see §5 |

**9 of 15 code exports ported, 6 held back, 3 data exports out of scope for registration.**
`RtGraphicsAnimationRegister()` registers 9 functions, each by both mangled name and retail
address (18 `RegisterRuntime*` calls total).

All 18 names were checked against the shipped export directory of
`C:\GOG Games\Banished\Runtime-x64.dll` with `pefile` **before any body was written**
(`reference_resolveexport_phantom_names.md`). Every one is present; addresses/ordinals above
are that query's output, not Ghidra's rendering.

Files owned and touched by this lane, and no others:

* `runtime/src/graphics/animation.cpp` — the nine bodies plus `RtGraphicsAnimationRegister()`
  (was a stub `return 0;`)
* `runtime/types/graphics/Animation.h` — **new**; `AnimationEvent`, `AnimationManager`
* `runtime/types/graphics/Particle.h` — **new**; `Particle`, `ParticleInstance`,
  `ParticleSystemInstance`, `ParticleBoundsRecord`, `ParticleResource`, `GeometryBase`
* `docs/runtime_port/WAVE6_LANE_G.md` — this file

`runtime/src/graphics/rt_graphics_port.cpp` already declared `RtGraphicsAnimationRegister()`
and the `BANISHED_RT_GFXANIM` kill switch; neither was edited.

Build: `g++ -std=c++17 -fno-strict-aliasing -Wall -Wextra -Wno-cast-function-type -fsyntax-only`
against this file is **clean** (zero warnings) — confirmed after fixing one
`-Wunused-local-typedefs`. `RtGraphicsAnimationRegister()` returns **9**.

---

## 2. Struct layouts recovered, and the evidence for each

Full commentary is in the two header files; summary here.

### `Graphics::AnimationEvent` — 0x20 bytes (base only)

Pinned by the ctor (`0x1800a2150`, disassembled instruction-by-instruction). The retail body
stores THREE vtable pointers in sequence (`0x180154788` Object::Base, `0x1801547a0`
Object::External, `0x180154bf0` AnimationEvent's own) — the first two are MSVC's fully-inlined
`Object::External::External()` base ctor, immediately overwritten and therefore invisible in
the constructed object's final state. Confirmed by disassembly, not inferred from the
decompile's variable naming. `+0x08/+0x10/+0x18` are three zeroed qwords (Object::External's
own fields, opaque to this lane). The dtor (`0x1800a2120`) is two real instructions: stamp the
vtable, then a **tail `JMP`** into `Object::External::~External`
(`0x1800bc950`, confirmed **EXPORTED**: `??1External@Object@@UEAA@XZ`, ordinal 588 — found by
disassembling the dtor, not assumed).

### `Graphics::AnimationManager` — 0x9d8 bytes, NO VTABLE

Both `??0…QEAA@XZ` / `??1…QEAA@XZ` are non-virtual (matches the `gameupdateloop_load.cpp`
step-22 comment: `AllocSub(0x9d8, 8)`, an exact match once the 0x9d4-byte content is rounded to
8-byte alignment). The ctor (`0x1800fc430`) zeroes a 0x14-byte prefix shaped like the first
three fields of a Win32 `CRITICAL_SECTION` (`DebugInfo`, `LockCount`, `RecursionCount`) but
**never calls `InitializeCriticalSection`** — `DebugInfo` is always null. It then seeds a
624-entry `uint32_t` array at `+0x14` with the textbook Mersenne-Twister `init_genrand`
recurrence, seeded from `_time64(NULL)`, and finishes by storing `this` into the exported
static `_singleton` (`0x1801fe7b0`, ordinal 4799,
`?_singleton@AnimationManager@Graphics@@0PEAV12@EA`). The dtor (`0x1800fc400`) nulls
`_singleton` then calls `FUN_18005bf60(this)` **as if** `this` were an `LPCRITICAL_SECTION`
(decompiled: reads `->DebugInfo`/`->LockCount`/`->RecursionCount` at exactly
`+0x00`/`+0x08`/`+0x0c`) — since `DebugInfo` is always null, that function's only live branch
(freeing `DebugInfo`'s allocator block) never fires. This port calls the SAME unexported
internal at its own address for that teardown (PLAN §4.1 decision 1: no `<windows.h>` outside
a `_win` twin) rather than reproducing it.

`+0x10` is a fourth dword neither the ctor nor the dtor reads — **UNVERIFIED**, left named but
uncommented on further than "never read by anything this lane decompiled".

### `Graphics::Particle` — 0x48 bytes (base only)

Pinned by the ctor (`0x180103e80`, disassembled). Same base-ctor-inlining shape as
`AnimationEvent` (`0x180154788`/`0x1801547a0` transient, `0x180155130` Particle's own vtable
survives) — **zero external calls**, confirmed by disassembly, which is why this port needs no
DLL internal for it at all. `+0x20` (byte, =1), `+0x24` (int, =1), `+0x28` (int, =0xf) are the
only fields this lane could attach any meaning to (none — no reader was found in the ctor,
dtor, or the exported instance methods this lane read); `+0x30/+0x38/+0x40` (three zeroed
qwords) are **UNVERIFIED**. `Particle::_rng` and `Particle::_typeInfo` are **protected static
class members** (mangling codes `1` and `0`), not instance fields — they do not appear in this
struct at all; see §5.

### `Graphics::ParticleInstance : Graphics::Geometry` — 0xd8 bytes

Pinned by the ctor (`0x180104130`, decompiled + disassembled). Calls
`Geometry::Geometry(this)` (retail `0x1800a4270`, **EXPORTED**
`??0Geometry@Graphics@@QEAA@XZ`, ordinal 205 — confirmed by disassembly's `CALL`, not the
decompile's un-mangled rendering), stamps its own vtable (`0x1801550e0`, confirmed by the LEA
immediately preceding the store), then writes `+0xc0` (pointer, zeroed), `+0xc8` (pointer,
zeroed), `+0xd0` (int, `= -1`). `Graphics::Geometry` itself is **NOT this lane's class** — Wave
6 lane D owns `Graphics::Geometry` + `Scene` — so its `[0x00, 0xc0)` is left as an opaque
`GeometryBase` blob, exactly the "what stays in the DLL" shape `scene.cpp` uses for Picking's
unexported grid internals, except `Geometry::Geometry` and `Geometry::SetBounds` both happen to
be **exported**, so `RT_ORIGINAL_ONCE` applies rather than a raw `ResolveAddr`.

`Geometry::Geometry(void)`'s own body (decompiled, `0x1800a4270`) makes **zero external calls**
— pure field stores, including six `Volume::Box`-shaped empty-bounds sentinels
(`0x4cbebc20`/`0xccbebc20` = ±1.0e8f, the same constants `Volume::Box`'s own ctor uses,
Wave 1). This is why this lane's scratch-object oracles for `ParticleInstance` and
`ParticleSystemInstance` are safe to build on the stack and simply discard: nothing inside
`Geometry::Geometry` registers the object anywhere.

`ParticleInstance::Update` (`0x180104180`, decompiled) pins the meaning of `+0xc0`/`+0xc8`/
`+0xd0`: `+0xc0` is a resource pointer (`ParticleResource*`) whose `+0x20` byte is an active
flag and whose `+0x38` field is the base of an array of `ParticleBoundsRecord*` (stride 8,
confirmed by the `* 8` index math), indexed by `+0xd0`; `+0xc8` is a cache of the element at
that index. `ParticleBoundsRecord+0x2c` (an `int`, the per-frame "last touched" stamp compared
against `ParticleInstance::_frameIndex`) is the only field of that record this lane could pin —
its exported signature (`Geometry::SetBounds` takes `const Volume::Box&`) says the record's
first 0x18 bytes ARE Box-shaped, but `Volume::Box` itself is only 0x18 bytes total
(`runtime/types/volume/Volume.h`), so bytes `0x18..0x2b` of the record are **UNKNOWN** — this
lane never needs to read or write them, only pass the pointer through unchanged.

### `Graphics::ParticleSystemInstance : Graphics::ParticleInstance` — 0xe0 bytes

Pinned by the ctor (`0x1800a55a0`, decompiled + disassembled). Re-runs
`ParticleInstance`'s own field inits **inline** (same `Geometry::Geometry` call, a transient
stamp of `ParticleInstance::_vftable_` at `0x1801550e0` — the SAME constant
`ParticleInstance`'s own ctor uses, confirming this is retail re-executing that ctor's logic
rather than calling it), then overwrites the vtable with its own (`0x180155040`) and zeroes one
more field at `+0xd8` — **UNVERIFIED** beyond "zeroed by the ctor", no reader found.

---

## 3. Oracle notes worth stating explicitly

* **`AnimationEvent::AnimationEvent` / `Particle::Particle` / `ParticleInstance::ParticleInstance`
  / `ParticleSystemInstance::ParticleSystemInstance`** all get a genuine full scratch-object
  oracle: construct "theirs" on the stack, `memcmp` the whole struct against ours. This is safe
  specifically because every external call these ctors make (`Geometry::Geometry`) was verified
  by disassembly to make **no further external calls itself** — there is nothing for a scratch
  instance to have registered anywhere, unlike e.g. Picking::Scene's ctor, which needed the
  DLL's own dtor run afterward to avoid a 40 KB leak per diff call. No scratch teardown call is
  made here because there is nothing to tear down.
* **`AnimationEvent::~AnimationEvent`** has a **structural, not runtime** oracle: its entire
  computed content is a vtable stamp this port also performs identically, followed by a tail
  call into the exact same retail `Object::External::~External` this port also calls. There is
  no independent second run possible without a double-free, and no state beyond the vtable slot
  that this lane owns the layout of to check. It is registered (so `calls=` is visible in the
  diff summary) but the verdict proves only "did not crash", stated as such rather than dressed
  up as a real comparison.
* **`AnimationManager::AnimationManager`** cannot use a plain scratch-object diff: `_time64`
  legitimately advances between two calls in the same process, so the seed word (and everything
  the MT recurrence derives from it) will differ call to call even given a byte-perfect port.
  The oracle instead isolates what the function actually COMPUTES — the recurrence — by
  re-running retail's own textbook formula against OUR seed in a private buffer and comparing
  that, not by calling `RtOriginal` at all for this function. `kAnimMgrCtor` is therefore
  declared (documents the export/ordinal) but intentionally unused in the body.
* **`ParticleInstance::Update`** is an idempotent-replay oracle, the same shape `scene.cpp` uses
  for `SetTransform`: `Geometry::SetBounds` is exported, pure, and its own decompile shows it
  writes only when its six floats actually differ — so replaying retail's real body on the REAL
  live object immediately after ours is safe (a second call with an unchanged record is a true
  no-op). What is NOT independently confirmed: the boolean return on the branch that advances
  the frame tag (the first call of a given frame) — a replay of that exact call necessarily
  lands on the "already tagged" branch and returns `false`, so there is no second observation of
  `true` to compare against. That branch's condition is exercised by the byte-state check on
  every single call regardless of which way it went, so a clean run over many calls is strong
  evidence, not proof, and is stated as such rather than implied. **Faithfully unguarded**: this
  body does not null-check `_resource` before reading `_resource->_active`, nor `_boundsPtr`
  before writing `_boundsPtr->frameTag` on the "needs update" branch — retail doesn't either
  (CLAUDE.md: "don't fix a faithful crash with a null check"). An entity that legitimately lacks
  an attached resource here would crash in both implementations identically.
* **`ParticleInstance::IncrementInstanceIndex`** diffs by letting retail's own body perform the
  identical increment on the SAME live global immediately after ours, from a restored starting
  point, so what's compared is "does retail also add exactly 1" rather than two independently
  drifting counters.

---

## 4. Held back, with reasons

### `Graphics::FrameCopyImageBuffer` — all four functions, as one block

This class exists only to grab a screenshot (`code/Game/Game/w2_takescreenshot.cpp`). Its ctor,
`Create` and `Generate` allocate and drive `Video::RenderBuffer` / `Video::ImageBuffer` objects
through **virtual calls into vtables this lane does not own** — `Video::Device` /
`Video::Context` / `Video::RenderBuffer` are lanes A/B/C's scope this wave, and
`Video::RenderBuffer::PlatformConstruct` ultimately dispatches into `dx11/`, this project's own
DX11 replacement, on the other side. A wrong field in the `RenderBuffer` layout here would
corrupt a live GPU-resource object three other concurrent lanes' code also touches — the
"half-ported class" hazard PLAN §4.1 decision 5 names, compounded because the entanglement is
with THREE other lanes' in-flight layouts rather than the DLL's own dormant internals. There is
also no `memcmp` oracle for "did the GPU texture get copied correctly" — RT_DIFF's whole model
assumes comparable in-process state, and a screenshot's correctness is a pixel comparison
outside that model.

This decision is independently corroborated by code already in this tree:
`w2_takescreenshot.cpp`'s own header comment (lines 40–50) explicitly chose NOT to reconstruct
this class's exe vtable, for the same reason — "leaving it alone is strictly safer: anything
inside Create/Generate that dispatches on its own vptr keeps working, which a partially
reconstructed exe table could not guarantee" — and calls `Create`/`Generate`/the dtor through
the DLL's own vtable slots today. This lane's decision to leave the whole class on the DLL is
the same judgment applied one level deeper (the class body, not just its vtable).

Held back as a whole class rather than split field-by-field, matching `WAVE5_LANE_D.md` §4's
reasoning for NOT splitting `Picking::Scene` — except in reverse: that class turned out to be
safe to keep whole because nothing else touches it, this one is unsafe to touch AT ALL because
everything else does.

### `Graphics::AnimationManager::Update` and `::Serialize`

Both mutate through a nested, **unexported** `AnimationState` structure this lane did not have
budget to fully pin: `Update`'s groups are 0x30-byte records (an inline `Array<GraphicsMesh::
Node>` at `+0x10` — `GraphicsMesh::Node` is lane E's scope this wave — plus an `Array<
AnimationState>` at `+0x20`), and each 0x48-byte `AnimationState` element carries an untyped
`InterpolatedState<Transform>*` at `+0x38` and calls a 9-argument unexported
`AnimationState::Update` this lane read the calling convention of from the decompile only, not
from disassembly.

`Update` is also the single largest blast-radius function in this lane's entire scope: it runs
on every character's skeleton, every frame (`gameupdateloop.cpp:1293`), and already sits behind
a default-on non-retail batching path (`standalone/perf_experimental.cpp`,
`ANIM_BATCH_ALL`) — this lane's own brief said to read that file before touching `Update`, and
having done so, the honest assessment is that a wrong argument-slot mapping in the 9-argument
internal call would silently corrupt every animated model's pose in the game with no oracle
cheaper than full visual QA across every animation. `Serialize` walks the same unpinned
`AnimationState` array onto a save stream; Wave 5 held back every serializer-constructing
function project-wide for want of an oracle
(`reference_slot3_wrapper_must_resolve_first.md` — a serializer that consumes the wrong byte
count desyncs the stream and surfaces LATER on an unrelated record), and this lane applies the
same standard rather than a different one.

Both stay entirely on the DLL. The exe dispatch slot (`0x140152050`,
`gameupdateloop.cpp:601-609`) and the `ResolveExport` call site
(`gameupdateloop_serialize.cpp:163`) are **unchanged** by this lane — this lane touched none of
`standalone/`, `code/`, or `engine/`, per its file-ownership boundary.

---

## 5. The three data exports — not this lane's to register

`?_frameIndex@ParticleInstance@Graphics@@1HA` and
`?_typeInfo@Particle@Graphics@@0VBaseTypeInfo@Object@@A` are **already resolved and registered
elsewhere**: `standalone/reg_unreg_lane3.cpp` lines 213 and 195 respectively resolve both by
this exact mangled name and register their address for the exe's own `.rdata` consumers
(`code/Game/Component/particlecomponent.cpp:995` reads `_frameIndex` directly, per the lane
brief). `?_rng@Particle@Graphics@@1VRandom@System@@A` is read directly by
`code/Game/Game/tornado.cpp:253` via `ResolveExport`. All three are files this lane does not
own (`standalone/`, `code/`) and none needed a new registration — this lane's `RtOriginal`/
`RT_ORIGINAL_ONCE` calls for `Geometry::Geometry`/`Geometry::SetBounds`/`_frameIndex` all read
through the SAME live addresses these existing consumers already resolve, so there is no
divergent second copy of any of the three.

As data symbols (not functions), none of the three could be added to
`RtGraphicsAnimationRegister()`'s function-pointer registry even if this lane owned the files
that already handle them — `RegisterRuntimeExport`/`RegisterRuntimeAddr` bind a function
pointer, not a data slot.

---

## 6. What this lane could not verify

* **No run of any kind.** Every oracle above is code that has compiled and never executed
  (`-fsyntax-only` clean; `mingw32-make` was not run, per this lane's rules).
* **`AnimationManager::_f10`, `Particle::_f30`/`_f38`/`_f40`, `ParticleSystemInstance::_f_d8`,
  `ParticleBoundsRecord`'s bytes `0x18..0x2b`.** No reader was found in any function this lane
  decompiled. Marked UNVERIFIED in the headers rather than guessed.
* **The exact register/stack ABI slot mapping inside the held-back
  `AnimationState::Update`.** Not attempted — see §4's reasoning for holding the whole caller
  back rather than guessing at it.
* **Whether `ParticleInstance::Update`'s idempotent-replay oracle is actually SAFE at volume**
  under `BANISHED_RT_DIFF=1` — it calls `Geometry::SetBounds` a second time on the real live
  object on every diffed call where the "already tagged" branch is taken. `SetBounds` is proven
  idempotent by its own decompile (§2/§3), but this has not been exercised by a real run.
* **Whether `Graphics::Geometry`'s `[0x00, 0xc0)` interior is genuinely opaque to lane D's own
  Wave 6 work landing concurrently.** This lane read `Geometry::Geometry`'s ctor once
  (disassembled, confirmed pure) and did not otherwise coordinate with lane D; if lane D's port
  changes what `Geometry::Geometry` does, this lane's scratch-object oracles for
  `ParticleInstance`/`ParticleSystemInstance` would start comparing against a DIFFERENT
  function than the one measured here (though since `RtGeometryCtor` calls the export through
  `RT_ORIGINAL_ONCE`, it always calls whatever is CURRENTLY bound to that name — the DLL's own,
  unless lane D's port also registers over it, in which case this lane's ctors would be
  comparing our own Geometry port against itself if diffed after lane D lands and BEFORE this
  paragraph is revisited).

---

## 7. Post-landing fix — two ctor oracles reported a false divergence on every call

**The rule, stated first because it is the point:** *a byte-comparison oracle must compare only
the bytes the function under test actually writes.* A whole-struct `memcmp` against a
deliberately-poisoned scratch buffer does not test the port — it tests the port PLUS every gap
neither implementation touches, and in every such gap it is really comparing "what garbage was
already sitting in this scratch buffer's poison fill" against "what garbage was already sitting
in the real object's allocator residue". Those two garbages have no reason to agree, so the
oracle fails **100% of the time**, which reads exactly like a catastrophic, universal port
defect rather than what it is: a measurement artifact that says nothing about correctness. This
project already had one example of the fix (`WAVE5_LANE_D.md`'s `Picking::Scene` ctor oracle
excludes its own `+0x2c`/`+0x44`) before this lane repeated the mistake twice in the same file;
lane E hit the identical trap the same day, on `MeshInstance()` at `+0xd9..+0xdf`. Three
independent hits of the same class of bug in one wave makes it a pattern worth naming explicitly
for Wave 7 (UI, 1,625 functions — almost entirely ctors of exactly this shape): **before writing
a scratch-object ctor oracle, disassemble the ctor and identify every gap between its stores,
then either exclude those byte ranges from the comparison or compare field-by-field and simply
never mention the gap fields at all.** Do this BEFORE a run reports a false 100%, not after.

The wave's first integration run (`BANISHED_AUTOSTART=1 BANISHED_RT_DIFF=1`, 25.9M diffed
calls, clean exit) flagged two of this lane's ports at 100% divergence:

* `Graphics::Particle::Particle()` — 1 of 1 call.
* `Graphics::ParticleSystemInstance::ParticleSystemInstance()` — 123 of 123 calls.

Both were **oracle bugs, not port bugs.** Re-reading both retail ctors in DISASSEMBLY (not the
Ghidra decompile, which folds byte/dword/qword stores into higher-level pseudocode that does not
show the gaps between them) confirms retail leaves specific byte ranges genuinely untouched.
**Four gaps in total were found across two follow-up runs, all MEASURED directly from
disassembly rather than inferred from C++ alignment rules:**

| Gap | Owning function | Evidence (consecutive stores either side) |
|---|---|---|
| `[0x21,0x24)` in `Particle` | `Particle::Particle` (`0x180103e80`) | `MOV byte ptr [RCX+0x20],0x1` then `MOV dword ptr [RCX+0x24],0x1` |
| `[0x2c,0x30)` in `Particle` | `Particle::Particle` (`0x180103e80`) | `MOV dword ptr [RCX+0x28],0xf` then `MOV qword ptr [RCX+0x30],RDX` |
| `[0x14,0x18)` in `Geometry` | `Graphics::Geometry::Geometry` (`0x1800a4270`, **EXPORTED**) | `*(undefined4*)(this+0x10) = 0xffffffff` then `*(undefined8*)(this+0x18) = 0` |
| `+0xd4` (alignment pad) | `ParticleInstance::ParticleInstance` (`0x180104130`) | `*(undefined4*)(this+0xd0) = 0xffffffff` and nothing further before the function returns |

The `[0x2c,0x30)` gap in `Particle` was found on a SECOND rerun, after the first fix narrowed
`Particle::Particle`'s oracle to a single `[0x24,0x48)` tail comparison that still happened to
straddle it — reported as `calls=1 divergences=1, ours="tail [0x24,0x48) differs"`. The cheaper
way to find every gap in a ctor in one pass, rather than one gap per run, is to walk its full
store list once (as the table above does for `Particle`: `+0x20, +0x24, +0x28, +0x30, +0x38,
+0x40`, each a fixed-width store) and exclude every address range the list skips, before writing
the oracle at all.

The `Geometry` gap is the important one structurally: it sits in the **base class both
`ParticleInstance::ParticleInstance` and `ParticleSystemInstance::ParticleSystemInstance` call
identically** (`Geometry::Geometry(this)`, exported, called by both ctors with no override), so
one root cause in one function produced two divergent sites in this lane's result table — the
same underlying gap, inherited twice.

This port's original ctor oracles did a raw `memcmp` of the WHOLE struct between the real
production object and a scratch buffer this lane deliberately poisoned with `0xCD` before
calling retail's own ctor on it (to make any byte retail forgot to write visible). For a byte
range NEITHER implementation writes, that comparison measures only which memory arena's
pre-existing leftover content survived — the real object's actual heap garbage (non-zero for
`Particle`, zero for `ParticleSystemInstance`'s `Geometry` base) versus this lane's own `0xCD`
fill — not a behavioural difference between the ports. The poison bytes surviving in "theirs"
(the direction the divergence report showed) is exactly what "retail never writes here" looks
like from this construction; the mistake was comparing that range at all, not writing to it.

This is the identical trap `WAVE5_LANE_D.md` §3's `Scene` ctor oracle already avoids — its
`+0x2c`/`+0x44` are excluded from that oracle's field list for the same reason — and this lane's
own file banner (`runtime/types/graphics/Animation.h`) already flags `AnimationManager::_f10` as
"no reader found" without applying the same discipline to these two ctors' oracles. It should
have.

**Fix, in `runtime/src/graphics/animation.cpp`:**

* `Particle::Particle`'s oracle now compares `[0x00,0x21)`, `[0x24,0x2c)` and `[0x30,0x48)` as
  three separate ranges, skipping both gaps, instead of one whole-struct `memcmp`.
* `ParticleInstance::ParticleInstance`'s oracle no longer touches the `Geometry` base at all —
  both implementations call the identical retail export there, so it can never genuinely
  diverge, and comparing it only ever measured which arena's leftover bytes filled the
  `[0x14,0x18)` gap. It now compares only `[0xc0,0xd4)`, the three fields this class itself
  adds, and also excludes the trailing alignment pad at `+0xd4` (the ctor writes `-1` to the
  dword at `+0xd0` and nothing further).
* `ParticleSystemInstance::ParticleSystemInstance`'s oracle: the same fix, comparing
  `[0xc0,0xd4)` (inherited) and `[0xd8,0xe0)` (`_f_d8`, this class's own field), excluding the
  `Geometry` base.

Both header files were updated to record the two gaps as **MEASURED** (disassembly evidence
quoted at the point of use), not merely inferred from C++'s natural alignment padding — see the
banners on `Particle`, `GeometryBase`, `ParticleInstance` and `ParticleSystemInstance` in
`runtime/types/graphics/Particle.h`.

**Confirmed executed, not just structurally argued.** The run that found the fourth gap also
reported **zero `calls=0` sites across all 183 sites in the wave** — so every one of this lane's
nine registered ports, including `AnimationEvent::AnimationEvent` and `AnimationManager::
AnimationManager`, was actually constructed at least once, not merely reasoned about as
"probably not exercised". Both report **0 divergences**, which now rests on a measurement rather
than the structural argument this section originally made (that `AnimationEvent`'s three qword
stores are contiguous with no gap, and `AnimationManager`'s oracle never did a whole-struct
`memcmp` in the first place — it isolates only the Mersenne-Twister recurrence, §3, for the
unrelated reason that `_time64` genuinely differs seed-to-seed). `ParticleInstance` itself had
the identical `Geometry`-base exposure as `ParticleSystemInstance` even though it was never
reported divergent — it received the same fix pre-emptively rather than waiting for a run that
happened to exercise it, and this run confirms it now executes cleanly too.

`-fsyntax-only -Wall -Wextra -Wno-cast-function-type` remains clean after the fix.
`RtGraphicsAnimationRegister()` still returns 9.
