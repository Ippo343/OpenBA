# Wave 10 — lane EXTPTR: `ExternalPtr<T>::Serialize`, and the row it unblocks

**Date 2026-09-08. Branch `wave10/lane-extptr`, cut from `wave10/base` (3318690).**
**Worktree `C:\code\GIT\untitled_rev_eng_project\.claude\worktrees\agent-ab26a0d9e3a563d05`.**

This executes the recipe in
[`WAVE10_EXTPTR_SERIALIZE.md`](WAVE10_EXTPTR_SERIALIZE.md) §6 and closes
`?Serialize@ElementDescription@UI@@UEAA_NAEAVStream@IO@@@Z` (0x180116b00,
ordinal 3687) — the row Wave 7 lane B declined once and Wave 9 lane UIW2
declined twice.

---

## 1. Headline

| | |
|---|---|
| `Object::ExternalPtr<T>::Serialize` | ported ONCE, 748 bytes, `BaseTypeInfo*` as a parameter |
| addresses that body serves | **25 in the shipped DLL**, of which **4 registered** |
| `?Serialize@ElementDescription@` | ported and registered, by name and by address |
| oracle | lane S emission oracle, **32 comparisons, 0 divergences**, `calls > 0` on both subjects |
| new address doors onto unported code | **none** — `FindOrLoad` is reached by exported NAME |

The gate doc predicted "port one body, register it at three addresses". The
measurement says the family is **twenty-five**, not three, and one of the extra
twenty-two is what made the port verifiable at all.

---

## 2. The gate doc's numbers, reproduced by a second tool

`tools/extptr_serialize_diff.py` (the gate doc's tool) compares exactly two
hardwired addresses. This lane wrote an n-way generalisation,
**`tools/extptr_clone_check.py`**, and ran it as an independent control before
trusting the conclusion — same method, different code, so agreement is evidence
rather than a restatement.

```
tools/extptr_clone_check.py 0x1800042a0 0x180076610 0x180038f90 0x180005dc0
```

| pair (vs `0x1800042a0`) | identical | position-only | genuinely different |
|---|---|---|---|
| `0x180076610` | 193 | 20 | **2** |
| `0x180038f90` | 193 | 20 | **2** |
| `0x180005dc0` | 193 | 20 | **2** |

193 / 20 / 2 is exactly what the gate doc reported for its two pairs. In every
pair the two real differences are the same difference appearing twice — the
`MOV EDX, dword ptr [rip+…]` type-id load, once per `IsDerivedFrom` call site.

**Why a raw byte-diff cannot answer this** (restated because it is the whole
method): every `rel32` and every RIP displacement encodes a distance FROM THE
INSTRUCTION, so two identical bodies at different addresses differ in each of
those fields. Naively measured, `0x1800042a0` vs `0x180076610` differ in 210 of
the first 900 bytes — 23.3%, which reads like two different functions and is an
artefact. Resolve each difference to the ABSOLUTE address it encodes first.

---

## 3. THE FAMILY IS TWENTY-FIVE, NOT THREE

The three-address recipe came from reading `Serialize@ElementDescription`'s
three call targets. Nobody had asked how many copies of the body the DLL ships.

**`tools/extptr_family_sweep.py`** answers it in one pass: build a mask from the
reference body in which every byte belonging to a `rel32` or a RIP displacement
is a wildcard (165 of 748 bytes — exactly the fields that MUST differ between
two copies), then scan `.text` for matches and read each hit's type-id global
back out.

| address | `T` |
|---|---|
| `0x1800042a0` | `UI::StringTable` |
| `0x180004590` | `UI::SpriteSheet` |
| `0x180005dc0` | `Achievement::Goal` |
| `0x180006d10` | `Achievement::Achievement` |
| `0x180014230` | `Graphics::GraphicsMesh` |
| `0x180014520` | `Graphics::MaterialInstance` |
| `0x18002f4c0` | `Object::External` |
| `0x180038f90` | `UI::ElementDescription` |
| `0x18003dc90` | `Object::ExternalList` |
| `0x18003f1e0` | `UI::Font` |
| `0x180051790` | `Audio::SoundEffect` |
| `0x180052a50` | `Audio::SoundBuffer` |
| `0x1800542a0` | `Video::VertexProgram` |
| `0x180055090` | `Video::VertexLayout` |
| `0x180055380` | `Video::PixelProgram` |
| `0x180055670` | `Video::RenderState` |
| `0x1800594a0` | `Graphics::AnimationEvent` |
| `0x18005b840` | `Graphics::AnimationGroup` |
| `0x18005d0f0` | `Video::ImageBuffer` |
| `0x180060930` | `Video::VideoProgram` |
| `0x180061100` | `Graphics::Material` |
| `0x180067120` | `Collision2D::Surface` |
| `0x1800756d0` | `UI::DialogControllerConfig` |
| `0x180076610` | `UI::Dialog` |
| `0x180076e90` | `UI::CharacterSet` |

**Twenty-one of these are free rows for later waves.** One already-written,
already-oracled body plus one two-line thunk each closes them. They are NOT
registered here, deliberately: they reach Video / Graphics / Audio /
Collision2D resource loading — meshes, materials, shaders, sounds, fonts — the
oracle route below does not cover any of them, and a defect in this body would
land on all of them at once. Registering twenty-five on the strength of an
oracle that exercises two would be a confident number about the wrong thing.

The four that ARE registered:

| address | `T` | why this one |
|---|---|---|
| `0x180038f90` | `UI::ElementDescription` | `ElementDescription+0x48` — the recipe |
| `0x180076610` | `UI::Dialog` | `ElementDescription+0x50` — the recipe |
| `0x1800042a0` | `UI::StringTable` | `ElementDescription+0x58` — the recipe |
| `0x180005dc0` | `Achievement::Goal` | **the oracle route**, see §5 |

---

## 4. The port

`runtime/src/object/external_ptr_serialize.cpp`. One body,
`RtObject_ExternalPtr_Serialize(External** self, Serializer* ser, const void* typeInfo)`,
plus four two-line thunks that supply their own `?_typeInfo@T@…`.

Three modes, in retail's order:

1. **`Encoding` type == 3** — no stream I/O. Re-resolve the pointer we already
   hold through `FindOrLoad`, run `IsDerivedFrom` and **discard its answer**
   (retail does), release the scratch path. The resource-reference VISIT pass.
2. **reference mode ON** (`ExternalFactory+0x40`) — a `uint` reference id
   crosses the stream; on load it becomes a path through the factory's id-keyed
   tree, then `AdjustDelimiters`.
3. **reference mode OFF** — a `bool` "present" flag then the path string.

Modes 2 and 3 converge on one load tail: `FindOrLoad(path)`, `IsDerivedFrom`,
and a write **through `self`** (a `CMOVNE`: a resource of the wrong type is
dropped, not kept).

Two details worth naming:

* **The type id is read at CALL TIME**, from the first dword of the
  `BaseTypeInfo`, at each of the two sites — exactly as retail's two
  `MOV EDX,[rip+…]` do. `BaseTypeInfo+0x00` is `0xFFFFFFFF` until
  `Factory::Startup` assigns it, so caching it at init would freeze the
  pre-Startup value.
* **Mode 2 has an arm that looks like a bug and is transcribed anyway**
  (0x180004372): loading, already holding an object, and that object's
  reference id is `-1` ⇒ consume the uint record and bail, writing nothing
  through `self`.

### The exe-side sibling, and where it differs

`code/Game/Component/ambient_externalptr_serialize.cpp` is the same function on
the exe side (0x1400019a0) and was a useful cross-check of the Serializer/Stream
shape. It **deliberately relaxes** the final type check ("keeps the loaded
External regardless of type") because the standalone factory graph does not
register the runtime `SoundEffect` TypeInfo. This runtime port must NOT do that
and does not: it is running inside the real factory, and the `CMOVNE` is
load-bearing.

### `ExternalFactory::FindOrLoad` — sequenced after OBJ1, not taken

The recipe's step 2 offered "take `FindOrLoad` with you, or sequence after OBJ1;
say which you did". **Sequenced after OBJ1.** It is lane OBJ1's row in
`WAVE10_PARTITION.md`, and porting it here would be the duplicate-registration
trap — last binding wins, one body silently becomes dead code.

It is reached through `RT_EXPORT_ONCE` on the **exported mangled name**, not by
address. Today that falls through to the shipped DLL exactly as before; the
moment OBJ1 registers the name, this call site picks up their body with no edit.
So this file opens **no new address door onto an unported callee** — the
[[reference_porting_can_grow_the_cutover_surface]] failure the gate doc flagged
does not occur, and at cutover the wall is OBJ1's row and stays OBJ1's row.

Every other callee is already ours by name: `External::GetPath`,
`Base::IsDerivedFrom`, `Filename::AdjustDelimiters`, `MemoryAllocator::MemFree`,
`String::Serialize`, `ExternalFactory::GetReferenceId`,
`ExternalFactory::GetPathForReferenceId`.

---

## 5. `Serialize@ElementDescription` — the fourteen records

`runtime/src/ui/elementdescription_serialize.cpp`. Its own file, not
`dialog.cpp`: four UI lanes are live in this wave and the tree's rule is one
lane, one file. Registered under `BANISHED_RT_ELEMDESCSER`.

The record order is **not declaration order** and is transcribed from the
listing:

```
+0x20 _x  +0x24 _y  +0x28 _width  +0x2c _height  +0x30 _minWidth  +0x34 _minHeight   int  (+0xA0)
+0x40 _elementType   +0x44 _flags   +0x3c _id                                        uint (+0x98)
+0x38 _tabOrder                                                                      int  (+0xA0)
+0x48 ExternalPtr<UI::ElementDescription>   +0x50 ExternalPtr<UI::Dialog>
+0x58 ExternalPtr<UI::StringTable>          +0x60 System::String
```

`_id` is declared `int32_t` and retail serialises it through the **unsigned**
slot. Kept as retail has it: the two slots are different vtable entries and a
reader picks by slot. The `+0x98` / `+0xA0` numbers are RETAIL's — in
`Runtime-x64-profile.dll` the same two sit at `+0xb0` / `+0xb8`
([[reference_encoding_vtable_offsets_differ_between_builds]]).

---

## 6. Verification

### 6.1 The oracle, and why the FOURTH registration exists

Lane S's emission oracle (`runtime/src/io/save_trace.{h,cpp}`,
`BANISHED_RT_SAVE_TRACE=1`) on the WRITE path. It reached the `ExternalPtr` body
**with no new harness**, because `runtime/src/achievement/goal_serialize.cpp`
already probes the two functions that call it — and both had been reaching the
body BY ADDRESS since Wave 5:

```
Achievement::Goal::Serialize   -> 0x1800042a0 over Goal+0x20
GoalInstance::Persist          -> 0x180005dc0 over GoalInstance+0x18
```

Before this lane those probes compared retail's inner body **with itself** — a
self-check, not an oracle ([[reference_shadow_marker_is_not_an_oracle]]).
Registering those two addresses makes the `ours` side call this port while the
`theirs` side keeps calling the DLL's copy. That is the entire reason
`0x180005dc0` is registered: it converts an existing probe into a real one.

### 6.2 `stopAfter` — RE-MEASURED, and NOT needed

The recipe said to cap the prefix before `GetReferenceId` "exactly as
`SerializeReferenceTable` does — do NOT assume the write path is pure".

Read at 0x1800bf100 rather than assumed: `GetReferenceId` is `QEBA` (const) and
its whole body is `GetPath` → `Filename` copy → `Set::Find(this+0x10, key)` →
`return node[0x14]`, then two String releases. **No insert, no counter, no store
into the factory.** So the emission sequence is idempotent, the whole body
replays, and the oracle covers all fourteen records rather than a prefix of
them. No `stopAfter` is used. (`SerializeReferenceTable` needs one because it
WRITES an id into every node — a different function.)

### 6.3 `calls=0` — found, diagnosed, and fixed

The first instrumented run registered the port and printed **no
`UI::ElementDescription::Serialize` line at all**. That is the trap the brief
names: a body with no callers reports clean
([[reference_uncalled_port_is_not_verified]]).

Cause, measured with `tools/`-style xref scan of the shipped DLL:

* **21** `rel32` CALLs to 0x180116b00 inside Runtime-x64.dll, plus the
  `??_7ElementDescription@UI@@6B@` slot at 0x1801561e8 (+0x28);
* **2** call sites in our entire tree (`captureboxdescription.cpp`,
  `frustumoutlinedescription.cpp`, both via exe import slot 0x1401518c8), and
  **neither fires on a boot**.

`RegisterRuntimeExport` cannot redirect a `rel32` CALL. So the only way this
port ever executes before cutover is a **detour**:
`standalone/elementdesc_detour.cpp`, opt-in on
`BANISHED_RT_ELEMDESCSER_DETOUR=1`, 15-byte prologue on an exact instruction
boundary with no PC-relative operand and no RSP capture, installed through
`InstallRuntimeDetourChecked` with the shipped bytes as a safety belt.

The oracle's `theirs` side then MUST use the **trampoline**: once the detour is
in, `ResolveExportRaw` hands back the patched address and calling it recurses to
a stack overflow ([[reference_detour_machinery_exists_wave7_zero_is_stale]]).
`RtUiElementDescSerializeTrampoline()` is that door and returns null when no
detour is installed.

### 6.4 Results

Run 1 — menu, `BANISHED_AUTOSTART=1 BANISHED_AUTOQUIT=1 BANISHED_RT_SAVE_TRACE=1`,
no detour:

```
16 pass Achievement::Goal::Serialize: 6 emissions identical
   (no UI::ElementDescription::Serialize line -- calls=0)
==== 18 compared, 0 diverged ====
```

Run 2 — same, plus `BANISHED_RT_ELEMDESCSER_DETOUR=1`:

```
[ElemDescDetour] DETOUR INSTALLED at 0x180116b00
16 pass Achievement::Goal::Serialize:        6 emissions identical
16 pass UI::ElementDescription::Serialize:  20 emissions identical
==== 34 compared, 0 diverged ====
standalone_crash.log: 0 bytes
```

Both subjects have `calls > 0`, both compare OUR body against the DLL's, and
neither diverges in any of the 32 comparisons that concern this lane.

Run 3 — full in-game autostart with the detour (`BANISHED_AUTOSTART=1
BANISHED_PAUSEMENU=900 BANISHED_PAUSEMENU_QUIT=180`), so every `.rsc`
UIDescription the world build parses runs the port:

```
[ElemDescDetour] DETOUR INSTALLED at 0x180116b00
16 pass UI::ElementDescription::Serialize:  20 emissions identical
16 pass Achievement::Goal::Serialize:        6 emissions identical
 1 pass Object::InternalBasePtr::Serialize   (prefix)
 1 pass Audio::Scene::Serialize             32772 emissions identical
==== 36 compared, 0 diverged ====
[worldgen] BuildTerrain EXIT ... dim=768
[DoSpawnCensus:worldgen-seed] CREATED=632
[GUL::Load] EXIT — world built
[PauseMenu] frame 901 - opening ... frame 1081 - dispatching OnCommand(id=6)
standalone_crash.log: 0 bytes
[ResLoad] census: FindOrLoad=22624 External::Load=5686
```

Terrain built, 632 entities spawned, `startTown` ran, the pause menu opened and
Quit was dispatched, clean exit. Every UIDescription in that run — menu and
in-game — was parsed through the port.

### 6.4a THE NEGATIVE CONTROL — the oracle DOES discriminate on this subject

`0 diverged` only means something if the instrument could have said otherwise.
One record was deliberately mis-typed — `_id` emitted through the **int** slot
(+0xA0) instead of the **uint** slot (+0x98), a one-line change that leaves the
emission COUNT and the VALUE identical and changes only the slot — rebuilt, and
re-run:

```
==== 34 compared, 16 diverged ====
16 DIVERGE UI::ElementDescription::Serialize at emission 9:
      ours int(+0xa0)=4294967295   theirs uint(+0x98)=4294967295
      (20 vs 20 emissions total)
```

**16 of 16 caught**, at the exact emission index, naming both slots — and it
caught a difference that is invisible in the emitted VALUE. The control was then
reverted and the run repeated green (`34 compared, 0 diverged`). So the oracle's
silence on the real port is evidence, not inertness.

### 6.5 What the oracle does NOT reach

Stated because a partial oracle that reads like a full one is worse than none:

* the `Encoding` type == 3 arm — lane S's sink binds all 16 Encoding slots to a
  recording thunk that returns the stub `ReaderWriter` POINTER, never 3;
* both `FindOrLoad` call sites and the entire READ path — the sink pins
  `stream.direction = 1` (writing) and every load-side branch is behind
  `direction == 0`;
* whichever of reference-mode-on / reference-mode-off was not taken on the run.
  Both sides read the same live `ExternalFactory+0x40`, so they always agree
  with each other, but only one arm is exercised per run;
* the twenty-one unregistered instantiations.

---

## 7. Blast radius

`Serialize@ElementDescription` is bound BY NAME to exe PE-import slot
`0x1401518c8` (`standalone/address_registry.cpp:1109`), and `RuntimePortInit()`
runs before `AddressRegistryInit()`, so registering the name re-points the exe's
own `.rdata` slot too; `engine/funcs/engine_funcs.cpp:4063` reads the same slot.
With the detour on, it is additionally the `UIDescription` parse path of **every
`.rsc` in the game** — where a past defect on the neighbouring slot 0x1401516b0
"silently SKIPPED the per-element Type read → stream drift → bogus String length
→ `CompressedStream::Read` zero-progress spin", and a slot-8 defect once
"doubled every loaded entity's roster and made `FindComponent` return the empty
duplicate — invisible in a screenshot."

So verification is not a screenshot. It is the emission oracle (§6.4), which
compares the record sequence itself, plus a full boot with the detour on
(run 3).

Run 3 (§6.4) is that boot: world built, 632 entities, clean quit, zero-byte
crash log, and the 16 compared calls all identical to retail's emission
sequence.

---

## 7a. Census after

`tools/runtime_census.py --unported --namespace UI`, run in this worktree:

```
namespace   total  called  ported  detour  called covered
UI           1625     164     136       2    82.9%
Object        191      56      32       2    57.1%
internal              126     112       0    88.9%
TOTAL        4632    1067     834       6    78.2%
```

`?Serialize@ElementDescription@UI@@` **no longer appears in the unported list**.
The four `ExternalPtr<T>::Serialize` registrations are by address and so land in
the `internal` bucket, not in `UI`. `UI detour = 2` is this lane's detour plus
the concurrent lane's `TabElement::UpdateContainers`.

(The census walks `os.walk("runtime")` relative to the CURRENT DIRECTORY, so
this number is this worktree's — [[reference_scanner_walks_cwd_not_script]].)

---

## 8. Kill switches

| var | effect |
|---|---|
| `BANISHED_RT_EXTPTR=0` | all four `ExternalPtr<T>::Serialize` addresses stay on the DLL |
| `BANISHED_RT_ELEMDESCSER=0` | `Serialize@ElementDescription` stays on the DLL |
| `BANISHED_RT_ELEMDESCSER_DETOUR=1` | **opt-in**; installs the detour. Default off. |

Nested under `BANISHED_RT_EXTERNAL` and `BANISHED_RT_UI` respectively.

---

## 9. Files

| file | what |
|---|---|
| `runtime/src/object/external_ptr_serialize.cpp` | the one body + 4 thunks + registration |
| `runtime/src/ui/elementdescription_serialize.cpp` | `Serialize@ElementDescription` + its probe |
| `standalone/elementdesc_detour.cpp` | the opt-in detour and its trampoline door |
| `runtime/src/object/rt_object_port.cpp` | +1 extern, +1 gated call |
| `runtime/src/ui/rt_ui_port.cpp` | +1 extern, +1 gated call |
| `tools/extptr_clone_check.py` | n-way position-resolved clone check |
| `tools/extptr_family_sweep.py` | masked-pattern sweep that found all 25 |

---

## 10. Handoff

* **21 free rows.** The table in §3 is the whole of the work: one
  `RegisterRuntimeAddr` and one `EXTPTR_THUNK` line each, against a body that is
  already written and already diffed. What they need before landing is an
  oracle route in their own namespace, not more analysis.
* **OBJ1's `FindOrLoad`** is the only unported callee. Nothing here has to
  change when it lands.
* `ElementDescription.h`'s `+0x50 _userData` is still misnamed (it is an
  `ExternalPtr<UI::Dialog>`). The comment says so; the field is left named as it
  is because call sites use it.
