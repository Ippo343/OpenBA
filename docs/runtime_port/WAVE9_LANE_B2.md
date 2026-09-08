# Wave 9, Lane B2 — the near-complete classes, round 3

2026-09-07, branch `wave/upgrade-path-complete`, HEAD `ef22d4a`. Read
[WAVE9.md](WAVE9.md), [WAVE9_LANE_B.md](WAVE9_LANE_B.md) (including its correction banner
and §3a), [WAVE9_LANE_P.md](WAVE9_LANE_P.md) and [PLAN.md](PLAN.md) §4/§6 first.

**This lane wrote code and did not build or run anything**, per the lane brief. Everything
below is verified statically: every body was read out of the shipped `Runtime-x64.dll` in
Ghidra as **disassembly, not decompiled C**
(`reference_ghidra_c_output_vs_disassembly.md`), by **address** via `disassemble_function`
(never `get_function_metrics`, whose `address` parameter is broken in this bridge —
WAVE9_LANE_B.md §3a); and the sources were checked with
`g++ -std=c++17 -Wall -Wextra -fsyntax-only` using the Makefile's own flags and include
paths. **Zero warnings.** The integrator owns the exe and all runtime verification.

## Result in one line

**Sixteen classes completed, 29 functions ported, one new lane-D row, ten new lane-C rows.**
Twenty-five of the 29 land with a full differential oracle; four are trampolines into
`dx11/`, which is already ours, and carry a shadow with the reason stated at each.

## What was ported, and why these

All 29 are reached from `dx11/` — our VideoDX11 replacement — and from nowhere else in the
tree. WAVE9.md §4 records that `dx11/` is part of the cutover surface and was invisible to
the census until 2026-09-07; **these are the cheapest part of that surface**, and they were
chosen over the other near-complete classes because they close whole classes rather than
scattering single functions (PLAN §4.1 decision 5).

Runtime declares one abstract companion base per renderer resource — `DeviceData`,
`ContextData`, `VertexBufferData`, … — and the plugin derives from each. Every base is the
same two-pointer object, and every ctor/dtor pair is three or four instructions:

```
??0XData(XData* this, X* owner)   this->owner = owner; this->vtable = ??_7XData; return this;
??1XData(XData* this)             this->vtable = ??_7XData;
```

**All 24 were disassembled individually rather than one being read and the rest assumed.**
The point of checking is the vtable operand, which differs per class and is the only thing
that can be wrong:

| Class | ctor | dtor | `??_7` (the LEA operand in BOTH) |
|---|---|---|---|
| `RenderBufferData` | `0x1800f8b70` | `0x1800f8db0` | `0x180158a28` |
| `VideoProgramData` | `0x1800f8b90` | `0x1800f8d90` | `0x180158a48` |
| `RenderStateData` | `0x1800f8bb0` | *(ported W6)* | `0x180158a60` |
| `PixelProgramData` | `0x1800f8bd0` | `0x1800f8dd0` | `0x180158a78` |
| `VertexProgramData` | `0x1800f8bf0` | `0x1800f8d10` | `0x180158a90` |
| `VertexLayoutData` | `0x1800f8c10` | `0x1800f8d30` | `0x180158aa8` |
| `VertexBufferData` | `0x1800f8c30` | `0x1800f8d50` | `0x180158ac0` |
| `IndexBufferData` | `0x1800f8c50` | `0x1800f8df0` | `0x180158af8` |
| `ConstantBufferData` | `0x1800f8c70` | `0x1800f8e50` | `0x180158b30` |
| `ImageBufferData` | `0x1800f8c90` | *(ported W6)* | `0x180158b50` |
| `ContextData` | `0x1800f8cb0` | `0x1800f8e30` | `0x180158b90` |
| `DeviceData` | `0x1800f8cd0` | `0x1800f8cf0` | `0x180158c30` |

`??1ImageBufferData` landed in `buffers.cpp` (Wave 6 lane C) and `??1RenderStateData` in
`device.cpp` (Wave 6 lane B). **Neither is re-registered here.** Registering a name twice is
not an error anything reports: the later binding silently wins and the earlier line becomes
dead code — the SHADOWED case `tools/dispatch_slot_map.py --check` reports on the exe side.
Those two classes complete on their ctor alone.

Plus five trampolines and one small value class:

| Function | Retail | Body |
|---|---|---|
| `Video::IndexBuffer::Lock` | `0x1800f34e0` | `platformData->vtable[+0x18]` (slot 3) |
| `Video::IndexBuffer::Unlock` | `0x1800f34c0` | `platformData->vtable[+0x20]` (slot 4) |
| `Video::VertexBuffer::Lock` | `0x1800f5640` | identical, same slots |
| `Video::VertexBuffer::Unlock` | `0x1800f5620` | identical, same slots |
| `Video::ConstantBuffer::Bind` | `0x1800eef70` | `this+0x40 ->vtable[+0x10]` (slot 2) |
| `Video::Device::DisplayMode::DisplayMode(int,int)` | `0x18000d300` | width, height, zero the `Array<RefreshRate>` triple |
| `Video::Device::DisplayMode::~DisplayMode` | `0x18000cb80` | `ADD RCX,8; JMP 0x180010760` |

## Classes completed: 16

| Class | Functions ported here | Complete? |
|---|---|---|
| `Video::ConstantBufferData` | ctor, dtor | **yes** |
| `Video::ContextData` | ctor, dtor | **yes** |
| `Video::DeviceData` | ctor, dtor | **yes** |
| `Video::IndexBufferData` | ctor, dtor | **yes** |
| `Video::PixelProgramData` | ctor, dtor | **yes** |
| `Video::RenderBufferData` | ctor, dtor | **yes** |
| `Video::VertexBufferData` | ctor, dtor | **yes** |
| `Video::VertexLayoutData` | ctor, dtor | **yes** |
| `Video::VertexProgramData` | ctor, dtor | **yes** |
| `Video::VideoProgramData` | ctor, dtor | **yes** |
| `Video::RenderStateData` | ctor (dtor was Wave 6) | **yes** |
| `Video::ImageBufferData` | ctor (dtor was Wave 6) | **yes** |
| `Video::IndexBuffer` | Lock, Unlock | **yes** (5/5) |
| `Video::VertexBuffer` | Lock, Unlock | **yes** (5/5) |
| `Video::ConstantBuffer` | Bind | **yes** (1/1) |
| `Video::Device::DisplayMode` | ctor, dtor | **yes** |

## Oracle, per function — and the reaching path

**Everything here runs on the ordinary boot.** The twelve ctors are called from
`dx11/src/devicedata.cpp`'s creation table and `dx11/src/exports.cpp`'s `CreateDeviceData`;
the ten dtors from each `*Data` subclass's scalar-deleting destructor at shutdown;
`Lock`/`Unlock`/`Bind` from `dx11/src/contextdata.cpp` on the per-frame draw path;
`DisplayMode`'s pair from `dx11/src/devicedata.cpp`'s mode enumeration. There is **no
function in this lane for which a reaching path could not be named** — which the brief asked
to be stated either way.

One command exercises all 29:

```
mingw32-make sa-deploy
tools/rt_run.sh laneB2 --timeout 900 -- BANISHED_AUTOSTART=1 BANISHED_RT_DIFF=1 \
                                        BANISHED_PAUSEMENU=600 BANISHED_PAUSEMENU_QUIT=60
grep -E '^\[RtDiff\].*Video::(.*Data|IndexBuffer|VertexBuffer|ConstantBuffer|Device::DisplayMode)' \
     "C:/GOG Games/Banished/standalone_error.log"
```

**Read the raw log, not a `tail`.** `reference_truncated_tool_output_manufactures_calls_zero.md`
is the case where a `tail -80` hid 1,100 of 1,200 `RtDiff` lines, faked sixteen sites
"disappearing", and survived an A/B plus five bisection runs. There are 29 sites here and
the summary is longer than a default tail.

**The pass condition is `calls=` non-zero AND `divergences=0` on every site.** A
`Video::*Data` site reporting `calls=0` means the name registration did not flip dx11's
resolve, not that the port is fine — `reference_uncalled_port_is_not_verified.md`. Kill
switch for an A/B: `BANISHED_RT_VIDCOMPANION=0`.

### Why the ctor oracle is a real test and not a tautology

Our ctor writes the vtable pointer this file resolves **by exported name**; retail writes an
absolute LEA. Those agree only if the export name is right and resolves — which is precisely
the phantom-name failure mode (`reference_resolveexport_phantom_names.md`), caught at the
oracle instead of at cutover. Retail's ctor is replayed into a **private 16-byte scratch**,
never into the live object, and the two are compared byte for byte.

That replay is sound for a reason worth writing down rather than assuming: the ctor is a
**pure write of two fields**. It reads no prior contents of `self`, so ours having already
run cannot change what theirs produces, and its only input is the scalar `owner`, which
nothing aliases. `reference_oracle_must_snapshot_aliased_inputs.md`'s trap — round 2's
`AsTransform3`, where the call site passed one buffer as both `self` and `ret` and the
oracle ran the shipped body on memory our own write had destroyed — **cannot arise here**,
and this was checked rather than waved past.

The dtors replay the same way into a bare `void*`, following `device.cpp`'s existing
`??1RenderStateData` port exactly.

`DisplayMode`'s ctor gets the full 0x18-byte comparison: two scalar parameters and two
zeroing stores spanning three named fields, replayed into a private scratch.

### The four shadows, each with its reason

* **`IndexBuffer::Lock`/`Unlock`, `VertexBuffer::Lock`/`Unlock`, `ConstantBuffer::Bind`** —
  the far side of the dispatch is **dx11's own vtable, and dx11 is already ours**. There is
  no second implementation to diverge from. A replay would additionally be *unsound*:
  Lock/Unlock are a matched pair over a mapped GPU buffer, and calling Lock twice or Unlock
  on an unlocked buffer is the mutate-twice trap. Same posture Wave 6 lane C already took
  for `Create`/`Set` on the same two classes.
* **`~DisplayMode`** — a replay is not merely unhelpful, it is a **double free**. Ours frees
  `_data`; retail's body run afterwards on a copy of the pre-state passes the same pointer
  to `MemoryAllocator::Free` again. Run on the *post*-state it takes the null branch, only
  re-zeroes, compares nothing and reports **green** — the `Audio::Scene::Pause` early-out
  trap (WAVE9.md rows 5-7) in its exact shape. The shadow asserts what the body can honestly
  assert: the `{data,size}` pair came back zeroed.

## New surface this lane creates

WAVE9.md's "porting a function can GROW the cutover surface" applies, and here it is
measured rather than estimated. `tools/runtime_census.py` on this HEAD, with this lane's two
new files moved aside and `rt_video_port.cpp` reverted, then restored:

| | called | ported | % |
|---|---|---|---|
| before | 832 | 556 | 66.8% |
| after | **843** | **585** | **69.4%** |
| Video namespace | 112 → 122 | 53 → **82** | 47.3% → **67.2%** |

**+29 ported is exactly this lane's 29 functions. +11 called is the cost**, and it splits:

* **One new lane-D row (unnamed DLL internal): `0x180010760`.** `~DisplayMode`'s tail call.
  It is the twelfth member of the `Collection::Array<T>::~Array` J1 family lane Q ported
  (`runtime/src/collection/array.cpp`'s `J1_ENTRY` list) — byte-identical in shape to the
  eleven there, including `0x18001c750` — but it is **not** one of the eleven and is not
  registered, so the call reaches the shipped DLL. Called **by address**, the same posture
  `ClearAll`, `KeyMap::RegisterInput` and `Audio::Scene::Serialize` already take toward
  `Collection` internals. **Adding it to lane Q's `J1_ENTRY` list is a one-line change and
  the obvious follow-up** — not done here only because that file belongs to another lane
  this round and a registration race is not worth one line.
* **Ten new lane-C rows (exported DATA):** the ten `??_7*Data@Video@@6B@` vtables the ported
  ctors and dtors stamp. `??_7ImageBufferData` and `??_7RenderStateData` were already
  resolved by `buffers.cpp`/`device.cpp`, so only ten are new. **They are not in
  `wave9_data_harvest.csv`** — that harvest is driven by what the tree references, and until
  today the tree referenced two of the twelve. Lane C needs the other ten.

  Their shape, read at `0x180158a28` and confirmed against the two rows the harvest already
  has: **slot 0 is a per-class scalar-deleting destructor at `0x180058590 + 0x80·k` for
  k = 0…11, and every remaining slot is `_purecall` (`0x18007ec28`)** — the bases are
  abstract and dx11's derived object overrides all of them, including slot 0. So minting
  these ten is cheap, and the twelve slot-0 dtors are **not** twelve new lane-D bodies: no
  reachable path destroys a bare base object virtually. That claim is a reading of the
  vtables and the class hierarchy, not a measurement, and lane C should confirm it before
  relying on it.

## Corrections to existing material

* **`docs/runtime_port/wave9_laneB_inventory.csv` sizes for this cluster are right at the
  bucket level.** All 24 companion ctors/dtors are marked `trivial` (32/48 bytes by
  RVA delta) and are genuinely 3-5 instructions. No repeat of the `Lerp` 85→"8" /
  `Goal::Serialize` 33→"3" digit-truncation lane P found; those were in the
  mis-scoped `ghidra-exact` column, and none of this lane's 29 rows carries that column.
* **`Video::RenderStateData` and `Video::ImageBufferData` are listed in the CSV as
  1-unported/1-ported.** Correct, and both close here on the ctor.
* **No existing comment was made false by this lane's registrations**, checked by reading
  `buffers.cpp`'s and `device.cpp`'s banners: neither asserts a holdback over anything
  registered here. `buffers.cpp`'s `SaveImageToFile` holdback banner is untouched and still
  accurate.

## Deferred, with reasons

* **`Application::Application` (2 of 2 unported, `dx11`-cutover-critical).**
  `?GetInstance@Application@1@QEBAPEAUHINSTANCE__@@XZ` (`0x18009fb90`) is two instructions —
  `MOV RAX,[RCX+0x8]; RET` — a pure read with a full scalar oracle, and it would be the
  cheapest port in the wave. It is **left out deliberately to keep class atomicity**: its
  sibling `?RegisterMessageCallback@…` (`0x18009fa60`) is not cheap. Read at the address, it
  is a **`Collection::Set` AVL insert**: it takes the allocator critical section
  (`[0x1801fc718]+0x640`), calls `MemAlloc` (`0x1800c6e60`) for a 0x30-byte node, walks the
  tree comparing the id, inserts left or right through `0x18000a5b0`, rebalances through
  `0x18000adc0`, and bumps the count. Running ours then theirs **inserts the callback
  twice**. It wants the **our-writer / their-readers** oracle that closed
  `KeyMap::RegisterInput` (WAVE9.md row 10) — insert with ours, then have the DLL's own
  lookup walk the tree back — and that is a slot, not a spare five minutes. Porting
  `GetInstance` alone leaves the class at 1/2 and buys nothing at cutover.
* **`Video::Device` (8 remaining), `Video::Context` (5, 82% done), `UI::UserInterface` (11)**
  — Tier 4 in lane B's sequencing, untouched here.
* **`UI::GraphElement::AddGraph`/`AddGraphPoint`** (2 functions, would close a 3rd class)
  — looked at and rejected: both mutate arrays in place (`AddGraph` grows through
  `0x1800417d0` then frees a temporary through `0x18001c750`; `AddGraphPoint` appends
  through `0x180023830`), so a naive ours-then-theirs appends twice. Diffable only with a
  scratch-object harness. Cheap in instructions, not cheap in oracle.
* **`Object::InternalFactory::ClearPersistState`/`ResetPersistState`** — `ClearPersistState`
  is `ADD RCX,0x10; JMP 0x18001c750`, i.e. free the persist array; `ResetPersistState` takes
  an `IO::Stream&` and is therefore **save-path** (lane S's emission-sequence oracle, not
  byte comparison). Left as a pair for whoever takes lane S's next subject.

## Files

* `runtime/types/video/CompanionData.h` — new. The shared 0x10-byte companion base, and
  `Video::ConstantBuffer`'s two confirmed fields. `static_assert` on every offset used; no
  `sizeof` assertion on either struct, with the reason written at each — 0x10 and 0x48 are
  lower bounds read from the ctors, not measured totals, and a `static_assert` on a guessed
  size is one that cannot fail for the right reason.
* `runtime/src/video/companion_data.cpp` — new. The 29 bodies and
  `RtVideoCompanionRegister()`.
* `runtime/src/video/rt_video_port.cpp` — three lines: the `extern`, the call, and the
  `BANISHED_RT_VIDCOMPANION` kill switch in the banner. No `Makefile` change is needed;
  `RUNTIME_SRC_SRCS` is a wildcard.

### A tooling note this lane paid for twice

The first draft built both the vtable names and the registration names with macro
stringization — `ResolveExportRaw("??_7" #CLASS "@Video@@6B@")` and
`RegisterRuntimeExport("??0" #CLASS "@Video@@…", …)`. It compiled, it was shorter, and it
was wrong in a way this project has a memory entry about:

* `tools/export_name_audit.py` reported **two names not in the DLL, `??_7` and `??_7XData@Video@@6B@`** —
  it scans source for a literal argument, and stringization hides the real twelve.
* `tools/runtime_census.py` reported **two ports named `??0` and `??1` with no caller** —
  i.e. it could not see 22 of this lane's 29 registrations, which is
  `reference_census_cannot_see_nonliteral_registrations.md` reproduced live inside the file
  that cites it.

Both are now written out in full: twelve one-line vtable accessors and 22 explicit
`RegisterRuntimeExport` calls. Verbose beats invisible. **A macro that hides a name from the
audits does not save work, it moves the work to whoever has to find out why the census is
short.**

## Static audits, all run after the edits

```
tools/export_name_audit.py       662 distinct ResolveExport names; 0 are NOT in Runtime-x64.dll
tools/abi_retslot_audit.py       0 suspicious of 48 by-value-returning exports
tools/resolve_fallback_audit.py  163 guarded fallbacks; 0 do NOT point at their own name
tools/port_bypass_audit.py       351 by-address sites, 0 reach a name-ported function;
                                 85 name-door sites, 0 are a ported function on a live path
```

Separately, **all 41 mangled names this file uses were checked against the shipped export
directory by direct lookup** (`tools/pe_exports.py` over
`C:\GOG Games\Banished\Runtime-x64.dll`) — 12 vtables, 12 ctors, 10 dtors, 7 others,
**0 missing**. That check matters more than usual here because the companion dtors are
`UEAA` (virtual) and `DisplayMode`'s is `QEAA` (non-virtual):
`reference_phantom_dtor_mangling_qeaa_vs_ueaa.md` is exactly the bug of picking the wrong
one, and it resolves NULL silently.
