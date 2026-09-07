# Wave 8 — Lane C — `Audio::Scene` + `Audio::Device` + `Audio::Configuration`

Branch `wave/upgrade-path-complete`. Files owned by this lane:
`runtime/src/audio/audio.cpp`, `runtime/src/audio/rt_audio_port.cpp`,
`runtime/types/audio/Audio.h`, this document, and the one line in
`engine/funcs/engine_funcs.cpp` this lane disproved (§4).

**Result: 13 of 22 ported, 7 held back with written reasons, 1 data export
(nothing to port), 1 phantom name fixed at its source.**
`RtAudioPortInit()` calls `RtAudioSceneRegister()` (11) +
`RtAudioConfigurationRegister()` (2) = **13**.
`tools/runtime_census.py`: Audio 137 in DLL / 21 called / **13 ported**
(the "called" count dropped from the pre-wave 22 to 21 because fixing the
phantom `PlayEffect` name in engine_funcs.cpp merged it with the
already-counted real name — one distinct string instead of two).
`tools/export_name_audit.py`: **0 of 651** names not in the shipped DLL. There
were 3 phantoms tree-wide at the wave boundary (WAVE8.md §4): 2 belonged to
lane E's `Save::SaveLoad` (`CheckMod`, `SetSaveName`), already fixed by that
lane, and this lane's `PlayEffect` (QEAI) was the third and last. **This
lane's fix (§4) closes the last remaining phantom export name in the entire
tree** — `export_name_audit.py` is now clean, tree-wide, for the first time
in the project's history. `tools/abi_retslot_audit.py`: **0 suspicious of
48**. Compiling `audio.cpp` and `rt_audio_port.cpp` with the wave's exact
command line at `-O0` and `-O2` is **warning-clean**; `Audio.h` compiles
standalone and `static_assert`s every field offset plus each struct's size.

Not run by this lane, by instruction: the game, `make`, `tools/rt_run.sh`,
`sa-run`, `git commit`. Every runtime number below is therefore a
*prediction of what the oracle will report*, not a measurement.

---

## 1. The function table

Retail addresses and ordinals were read out of `docs/runtime_port/wave8_lanes.txt`
(generated from the shipped export directory before any body was written) and
independently confirmed against `Runtime-x64.dll` in Ghidra.

| Export | Retail | Ord | Ported | Oracle | Diff site name |
|---|---|---|---|---|---|
| `??0Scene@Audio@@QEAA@XZ` | 0x1800e0160 | 368 | yes | build | — |
| `??1Scene@Audio@@QEAA@XZ` | 0x1800e0120 | 668 | yes | build | — |
| `?AddEmitter@Scene@Audio@@…` | 0x1800dedf0 | 1556 | yes | shadow | `Audio::Scene::AddEmitter` |
| `?FindEffect3D@Scene@Audio@@…` | 0x1800dee70 | 2345 | yes | full replay | `Audio::Scene::FindEffect3D` |
| `?FreeChannelPlatform@Device@Audio@@…` | 0x1800de6f0 | 2380 | **no** | — | held back, §3 |
| `?GetEmitterCount@Scene@Audio@@…` | 0x1800dec00 | 2592 | yes | full replay | `Audio::Scene::GetEmitterCount` |
| `?IsEffectPlaying@Scene@Audio@@…` | 0x1800df680 | 3118 | yes | full replay | `Audio::Scene::IsEffectPlaying` |
| `?MoveEmitter@Scene@Audio@@…` | 0x1800dec40 | 3234 | yes | shadow | `Audio::Scene::MoveEmitter` |
| `?Pause@Scene@Audio@@…` | 0x1800df070 | 3313 | **no** | — | held back, §3 |
| `?PlayEffect3D@Scene@Audio@@…` | 0x1800deeb0 | 3422 | yes | shadow | `Audio::Scene::PlayEffect3D` |
| `?PlayEffect@Scene@Audio@@…` (real, QEAA) | 0x1800def60 | 3423 | yes | shadow | `Audio::Scene::PlayEffect` |
| `?PlayEffect@Scene@Audio@@…` (PHANTOM, QEAI) | — | — | n/a | — | name fixed at source, §4 |
| `?RemoveEmitter@Scene@Audio@@…` | 0x1800decf0 | 3554 | yes | shadow | `Audio::Scene::RemoveEmitter` |
| `?Save@Configuration@Audio@@…` | 0x1800dde10 | 3603 | yes | shadow | `Audio::Configuration::Save` |
| `?Serialize@Scene@Audio@@…` | 0x1800e0050 | 3730 | **no** | — | held back, §3 |
| `?SetEmitterVolume@Scene@Audio@@…` | 0x1800ded60 | 3877 | yes | shadow | `Audio::Scene::SetEmitterVolume` |
| `?Startup@Configuration@Audio@@…` | 0x1800ddee0 | 4084 | yes | scalar (trivial) | `Audio::Configuration::Startup` |
| `?StopAll@Scene@Audio@@…` | 0x1800df150 | 4107 | **no** | — | held back, §3 |
| `?StopEffect3D@Scene@Audio@@…` | 0x1800df370 | 4108 | **no** | — | held back, §3 |
| `?Update@Device@Audio@@…` | 0x1800de520 | 4481 | **no** | — | held back, §3 |
| `?Update@Scene@Audio@@…` | 0x1800df6c0 | 4502 | **no** | — | held back, §3 |
| `?_singleton@Configuration@Audio@@0PEAV12@EA` | 0x1801fe758 | 4801 | n/a (DATA) | — | §5 |

All 21 real names exist in the shipped DLL (`export_name_audit.py`, §above).

---

## 2. The vtable verdict

**Neither `Audio::Scene` nor `Audio::Configuration` has a vtable**, confirmed
from each ctor's FIRST STORE (not just the absence of a `??_7` export, per
WAVE8.md §1's instruction not to assume the answer from Wave 6/7's luck):

* `Audio::Scene::Scene` (0x1800e0160): the very first instruction is
  `*(undefined8*)this = 0`, which is `Scene::_time` (confirmed by
  `Audio::Scene::Update`'s own `*(undefined8*)this = param_3` at the same
  offset) — not a vtable pointer.
* `Audio::Configuration::Save`/`Startup` only ever read/write `this+0x00`
  through `this+0x0f` as four plain `int32`s; no ctor for `Configuration` is
  in this lane's scope, but nothing in either ported body treats `this` as
  polymorphic, and no `??_7Configuration@Audio@@` export exists.

`Audio::Device` was NOT constructed anywhere in this lane's scope (its ctor
is not in the 22), so its vtable status is unverified here; the class is a
process-lifetime singleton (`Device::_singleton`) touched only by the held-back
functions in §3, all of which read raw offsets, never a vtable slot at +0x00.

Consequence (WAVE8.md §1): since neither ported class has a vtable, the
"two implementations in play" hazard reduces to whichever near-CALLs the DLL
makes to its OWN body internally — and every ported function here is a pure
function of the object's own fields (Collection::Array<T> layouts already
confirmed independently by Wave 2), so there is no cached derived state for
the two halves to disagree about. No detour was installed or needed.

---

## 3. The seven hold-backs

Full reasoning is in the header comment of `audio.cpp`; summarized here.

| Function | Reason |
|---|---|
| `Update@Scene@Audio` | ~150 decompiled lines: spatializes every live emitter against `Device::PlayPlatform`, consumes `System::Random` state, and calls back into `Device`'s COM channel objects. No oracle (plays sound, consumes RNG); needs `Audio::SoundBuffer`/`SoundEffect` internals this lane does not own (WAVE8.md §2: none of the three is in this wave). |
| `Update@Device@Audio` | `Device` platform-channel completion callbacks — COM vtable calls, critical sections at `Device+0xa50`/`+0xa88`, none independently verified. |
| `FreeChannelPlatform@Device@Audio` | PRIVATE member; COM vtable call at `(**channel)+0x90` under a critical section. Same hazard class as `Update`. |
| `StopAll@Scene@Audio` | Walks `Device`'s platform channel array directly (COM calls at `Device+0xaa0`, `SetEvent` on a wait handle at `Device+0xa40`). Out of scope, no oracle (stops real audio). |
| `Pause@Scene@Audio` | Calls the sibling member `Scene::PauseEffect`, which reaches `Device::Stop` — an actual platform mixer call not in this lane's 22. Porting the outer loop while leaving the inner call on retail would put two implementations of the SAME state machine in play (WAVE8.md §1) for a function with an audible side effect and no oracle either way. |
| `StopEffect3D@Scene@Audio` | Same reason: calls `Scene::StopEffect` -> `Device::Stop`. |
| `Serialize@Scene@Audio` | Sits on the save stream and drives an `IO::Encoding` vtable dispatch resolved from the `Serializer` argument (calls through that object's vtable at +0x18/+0x20/+0xa0). WAVE8.md §6 and this project's own history (`reference_encoding_vtable_offsets_differ_between_builds.md`, Wave 4) say the profile build's `Encoding` vtable OFFSETS do not match retail's. This lane did not independently re-derive retail's own offsets within its budget; a wrong guess here desyncs every save silently, which is exactly the failure class this project has been bitten by before. Left on the DLL rather than risk it. |

**CORRECTED 2026-09-07 during Wave 9 prep: the stated blocker was already answered, by a fact
this project has had since Wave 4.** This lane held `Scene::Serialize` back because *this lane*
"did not independently re-derive retail's own offsets within its budget" — which was honest, and
unnecessary. Retail's `IO::Encoding` vtable offsets are written down in the warning block at the
top of `runtime/src/io/rt_io_port.cpp`: **bool `+0x78` / uint `+0x98` / int `+0xa0`**, against the
profile build's +0x90 / +0xb0 / +0xb8.

Disassembling `0x1800e0050` confirms it and narrows the exposure to a single slot: the function
touches exactly ONE Encoding entry, `CALL [RAX+0xa0]` — the **int** slot, at the recorded retail
offset — after the usual `[RAX+0x18]`/`[RAX+0x20]` read/write pair selected on `Stream+0x8`.
`ExternalFactory::SerializeReferenceTable` corroborates the pair independently, using `+0x98` and
`+0xa0` in the same shape. The whole function is 40 instructions.

What actually remains is **an oracle** — it writes the save stream, so replay double-writes — and
two unexported callees, `0x180051390` and `0x180052640`. Those are the real work; the offsets
were never the obstacle. Left in the table above unchanged so the original reasoning stays
legible.


A held-back function with a stated reason is a result (PLAN §6); nothing
above was skipped for lack of time to *read* it — every one was decompiled
and understood, and held back on its merits.

---

## 4. The phantom name

`?PlayEffect@Scene@Audio@@QEAIV?$ExternalPtr@VSoundEffect@Audio@@@Object@@W4SoundType@12@VTime@System@@@Z`
does not exist in the shipped DLL (`QEAI` where every other member in the
whole tree spells `QEAA`, and missing the `AEBV` reference qualifier the real
export has on its trailing `Time` parameter). One of only 3 phantoms in the
whole tree at the wave boundary (WAVE8.md §4).

**Verdict: never a functional gap.** `engine_funcs.cpp`'s
`fn_Audio_Scene_PlayEffect` already carried its own
`if (!fn) fn = ResolveAddr(Module::Runtime, 0x1800def60)` fallback (the Wave
7 lane C test WAVE8.md §4 instructs applying), and `0x1800def60` is exactly
the real, correctly-spelled export (ord 3423). Every call this project has
ever made through that pointer landed on the real function.

**Fixed at its source** in `engine_funcs.cpp` (the `ResolveExport` string,
not the working fallback) rather than left as a landmine for the next reader
who trusts the string over the behaviour. Confirmed clean:
`tools/export_name_audit.py` now reports **0 phantoms tree-wide**. This was
the LAST of the 3 phantoms present at the wave boundary — the other 2
(`Save::SaveLoad::CheckMod`, `::SetSaveName`) were lane E's and are already
fixed. This lane's fix closes out the whole tree.

Our own port of the real `PlayEffect` is registered under **both** the
correct mangled name and the retail address (`0x1800def60`), so
`engine_funcs.cpp`'s address fallback lands on our port too, not just the
name lookup — WAVE8.md §4's explicit instruction for a Wave 8 port of a
phantom-adjacent name.

There is also a second, unrelated, differently-shaped declaration
`fn_AudioScene_PlayEffect` (no underscore; `engine_funcs.h` line ~2501,
`engine_funcs.cpp` line ~3339) resolved via an exe dispatch slot at
`0x140152830`, with a guessed signature (`ExternalPtr<Object>`, `int`, `Time`,
void return) that matches NONE of the three real `PlayEffect` exports. This
lane did not touch it — auditing its callers is outside this lane's owned
files — but it is flagged here for the integrator as a likely stale/dead
declaration from before the real `PlayEffect` was identified.

---

## 5. The data export

`?_singleton@Configuration@Audio@@0PEAV12@EA` (ord 4801, retail 0x1801fe758)
is a DATA export, not a function. The registry seam maps names to functions
(WAVE3/4's own precedent for `InternalFactory::_singleton` and
`FileSystem::_singleton`); `engine_funcs.cpp` already resolves this exact
symbol (`g_pAudioConfigSingleton`) with an exe-exref-then-Runtime-fallback
pattern. Nothing for this lane to register.

---

## 6. Layout evidence and one shared-container reuse

`runtime/types/audio/Audio.h` defines `Channel` (0x70 bytes, the 256-slot
2D-channel table at `Scene+0x10`), `EffectSlot` (0x40 bytes, the SHARED
4096-slot table at `Scene+0x20` used by both the ambient-emitter API and the
3D-effect API — see the header's own note on why one table serves two
callers), `EmitterInstance` (the 16-byte `{x,y,z,volume}` record inside
`EffectSlot::positions`), `Scene` itself (0x38 bytes), and `Configuration`
(0x10 bytes, the four ported int fields only).

`Collection::Array<T>` (already pinned by Wave 2, `runtime/types/collection/
Collection.h`) is reused directly for `Scene::channels`, `Scene::effects`,
and `EffectSlot::positions`. Its documented `SetSize(n)` growth policy was
INDEPENDENTLY RE-CONFIRMED a third time here from three retail bodies that
all agree: `FUN_180051020` (hardcoded target 0x100, builds `Scene::channels`),
`FUN_180051390` (hardcoded target 0x1000, builds `Scene::effects`), and
`FUN_180051aa0` (the fully generic two-argument form — the one `AddEmitter`
calls, which is also how push_back is implemented here: `SetSize(_size + 1,
&newElement)`). A local `ArraySetSize<T>` helper transcribing that policy
lives in `audio.cpp` (not added to `Collection.h`, which this lane does not
own per WAVE8.md's cross-lane ownership contract).

Two independent 32-bit handle/id encodings share nothing but the general
shape (index in the low bits, a generation counter in the high bits,
validated by XOR-and-mask); the full derivation, from `AddEmitter`'s
`0xffffe000` mask and `PlayEffect`/`PlayEffect3D`'s handle construction, is in
`Audio.h`'s file header. Getting the two masks (`0xfffffe00` for Channel
handles, `0xffffe000` for EffectSlot ids) backwards would silently validate
the wrong table; both are cross-checked against three independent call sites
each (`GetEmitterCount`/`IsEffectPlaying`/`FindEffect3D` reading, `AddEmitter`/
`PlayEffect`/`PlayEffect3D` writing).

**One dependency this lane does not own, called into retail rather than
guessed:** `PlayEffect3D` delegates slot SELECTION to the DLL's own
`FindFree3DEffect` (retail 0x1800deae0, ord 2352 — itself an export, called
via `RT_ORIGINAL_ONCE` like `MemAlloc`/`MemFree`) because that helper
dereferences `Audio::SoundEffect` internals this lane does not own (WAVE8.md
§2: none of `SoundBuffer`/`SoundEffect`/`Stream` is in this wave). This keeps
`PlayEffect3D` portable without guessing `SoundEffect`'s layout, at the cost
of one extra out-of-process call per invocation — the same "ask the DLL for
what you don't own" pattern this project already uses for `MemAlloc`,
`MemFree`, `Map_Find`, and `IO::Persistence::Get/Set`.

---

## 7. Oracle discipline, per function

* **Full replay** (both implementations run on the same call, results
  compared): `GetEmitterCount`, `IsEffectPlaying`, `FindEffect3D`. All three
  are pure reads of Scene-owned state; since there is exactly one live
  `Scene` object regardless of which side constructed it, both
  implementations read the SAME memory — this is the established pattern
  across every prior wave, not new to this lane.
* **Shadow** (ours runs for real; theirs is a READ-ONLY verification call
  into the DLL against the state ours just wrote, on the same live object —
  the Wave 4 `Persistence::Set` pattern exactly): `AddEmitter`,
  `RemoveEmitter` (verified via the DLL's own `GetEmitterCount`),
  `PlayEffect` (verified via the DLL's own `IsEffectPlaying` on the handle
  just minted), `PlayEffect3D` (verified via the DLL's own `FindEffect3D`),
  `Configuration::Save` (verified via the DLL's own `Persistence::Get`
  round-trip, mirroring Wave 4's `Set(int)` oracle exactly).
* **No independent check, stated rather than dressed up**: `MoveEmitter` and
  `SetEmitterVolume` overwrite one field of an entry found by an EXACT
  floating-point position match; there is no side channel to verify the
  match without re-deriving the same comparison, so `RT_DIFF_SHADOW` records
  only whether OUR OWN scan found a match, not an independent verdict.
* **Build-only**: `Scene::Scene`/`~Scene` (constructing/destroying the live
  singleton twice would leak or double-free).
* **Trivial scalar**: `Configuration::Startup` always returns `true` in
  retail (no failure path in the decompiled body), so the only thing to
  diff is that constant.
* Never memcmp bytes neither side writes; never assume a written-extent
  constant without naming its writer — both traps this project has paid for
  before (`reference_oracle_must_not_compare_unwritten_bytes.md`,
  `reference_never_written_needs_by_whom.md`) do not apply here since every
  oracle above is either a live functional round-trip or an explicit
  same-object field read, never a raw byte range.

---

## 8. What the integrator must know

* Declare `void RtAudioPortInit();` in `runtime/funcs/rt_port.h` (Wave 8 init
  entry point block) and call it from `RuntimePortInit()` in
  `standalone/resolve_standalone.cpp`, alongside the other Wave 8 lanes.
* `RtAudioPortInit()` respects `BANISHED_RT_AUDIO=0` (whole-namespace
  kill switch) as its only gate; this lane's scope is small enough that no
  finer-grained switch was added (unlike Wave 6/7's per-lane switches).
* This lane's fix to `engine_funcs.cpp` (the `PlayEffect` `ResolveExport`
  string) is a one-line change inside a file this lane does not own the rest
  of, sanctioned by PLAN §6 step 8 ("comments/defects you disprove,
  anywhere"). It does not touch the working address-fallback line beside it.
* Flagged, not fixed: the second, differently-shaped `fn_AudioScene_PlayEffect`
  declaration (§4) looks stale/dead and worth a follow-up audit of its
  callers.
* 7 holdbacks remain on the DLL exactly as before; no kill-switch is needed
  for them since they were never registered.
