# Wave 6, lane D — `Graphics::Geometry` / `Graphics::Scene`

Branch `wave/upgrade-path-complete`, 2026-09-07.

Every claim below is labelled **MEASURED** (read out of the shipped binary, its export
directory, a tool's output, or a decompile/disassembly of a named retail address) or
**INFERRED** (a reading of a decompile that nothing independently corroborates).

**This lane wrote the port without running anything.** Seven lanes were working Wave 6
concurrently and the wave lead owns the build machine, the game directory and the logs.
Nothing here is an integration result; §7 lists exactly what a run has to establish.

Files owned and changed by this lane:

* `runtime/src/graphics/scene.cpp` — 19 ports, replaced wholesale
* `runtime/types/graphics/Geometry.h` — new
* `runtime/types/graphics/Scene.h` — new
* `docs/runtime_port/WAVE6_LANE_D.md` — this file

`-fsyntax-only` with the exact command line in the lane brief is **warning-clean**
(MEASURED, re-run after the last edit). `RtGraphicsSceneRegister()` returns **19**.
`tools/abi_retslot_audit.py`: `0 suspicious of 49 by-value-returning exports`.
`tools/export_name_audit.py`: 9 phantoms tree-wide, **none of them a lane-D name**.
`tools/runtime_census.py` lists **no lane-D port in the orphan list** — all 19 have a
caller in our tree.

---

## 1. The table

Retail addresses and ordinals are the output of a `pefile` query against the export
directory of `C:\GOG Games\Banished\Runtime-x64.dll`, run **before any body was written**
(MEASURED). Every port is registered **twice**, under its mangled name and under its
retail address — see §4.

| Export | Retail | Ord | Ported? | Oracle kind | Diff site name |
|---|---|---|---|---|---|
| `??0Geometry@Graphics@@QEAA@XZ` | 0x1800a4270 | 205 | yes | FULL, private scratch | `Graphics::Geometry::Geometry()` |
| `??1Geometry@Graphics@@UEAA@XZ` | 0x1800a41c0 | 599 | yes | post-state only | `Graphics::Geometry::~Geometry()` |
| `?Attach@Geometry@Graphics@@QEAAXPEAV12@H@Z` | 0x1800a3fa0 | 1631 | yes | post-state | `Graphics::Geometry::Attach` |
| `?Detach@Geometry@Graphics@@QEAAXPEAV12@@Z` | 0x1800a3f20 | 2216 | yes | post-state | `Graphics::Geometry::Detach` |
| `?Clean@Geometry@Graphics@@UEAA_NXZ` | 0x1800a4420 | 1702 | yes | FULL replay | `Graphics::Geometry::Clean` |
| `?UpdateAttached@Geometry@Graphics@@UEAAXXZ` | 0x1800a4020 | 4512 | yes | post-state | `Graphics::Geometry::UpdateAttached` |
| `?GetSubTransform@Geometry@Graphics@@UEBA?AVTransform@Math@@H@Z` | 0x1800a43c0 | 2949 | yes | FULL | `Graphics::Geometry::GetSubTransform` |
| `?GetTransform@Geometry@Graphics@@QEBA?AVTransform@Math@@XZ` | 0x180015800 | 2979 | yes | FULL | `Graphics::Geometry::GetTransform` |
| `?GetInstanceTransform@Geometry@Graphics@@UEBA?AVMatrix@Math@@HM@Z` | 0x1800154a0 | 2720 | yes | FULL | `Graphics::Geometry::GetInstanceTransform` |
| `?SetSharedInstanceData@Geometry@Graphics@@UEBAHAEAVContext@Video@@HM@Z` | 0x180015590 | 3965 | yes | FULL | `Graphics::Geometry::SetSharedInstanceData` |
| `?SetBounds@Geometry@Graphics@@QEAAXAEBVBox@Volume@@@Z` | 0x180015700 | 3836 | yes | FULL replay | `Graphics::Geometry::SetBounds` |
| `?SetTransform@Geometry@Graphics@@QEAAXAEBVTransform@Math@@@Z` | 0x1800155a0 | 3992 | yes | FULL replay (no children) / post-state (children) | `Graphics::Geometry::SetTransform` |
| `??0Scene@Graphics@@QEAA@XZ` | 0x180102730 | 372 | yes | FULL, private scratch | `Graphics::Scene::Scene()` |
| `??1Scene@Graphics@@QEAA@XZ` | 0x1801026c0 | 670 | yes | post-state only | `Graphics::Scene::~Scene()` |
| `?AddObject@Scene@Graphics@@QEAAIPEAVGeometry@2@@Z` | 0x180102670 | 1575 | yes | post-state | `Graphics::Scene::AddObject` |
| `?RemoveObject@Scene@Graphics@@QEAAXI@Z` | 0x180102600 | 3563 | yes | post-state | `Graphics::Scene::RemoveObject` |
| `?AddGlobalObject@Scene@Graphics@@QEAAXPEAVGeometry@2@@Z` | 0x180102540 | 1559 | yes | post-state | `Graphics::Scene::AddGlobalObject` |
| `?RemoveGlobalObject@Scene@Graphics@@QEAAXPEAVGeometry@2@@Z` | 0x1801024c0 | 3555 | yes | post-state | `Graphics::Scene::RemoveGlobalObject` |
| `?Clean@Scene@Graphics@@QEAAXXZ` | 0x1801023f0 | 1703 | yes | post-state | `Graphics::Scene::Clean` |

**19 exports, 19 ported, 0 held back.**

`docs/runtime_port/wave6_lanes.txt` lists this lane as **20** functions. It is 19: two of
its twenty rows are the same function under two spellings — see §2.

---

## 2. The two phantom names in the lane list

MEASURED, `pefile` against the shipped DLL:

```
MISS   ?Attach@Geometry@Graphics@@QEAAXAEAV12@H@Z
MISS   ?Detach@Geometry@Graphics@@QEAAXAEAV12@@Z
OK     0x1800a3fa0   1631  ?Attach@Geometry@Graphics@@QEAAXPEAV12@H@Z
OK     0x1800a3f20   2216  ?Detach@Geometry@Graphics@@QEAAXPEAV12@@Z
```

Both list rows spell the Geometry argument as a **reference** (`AEAV12@`). There is no
reference overload — `Graphics::Geometry` exports exactly one `Attach` and one `Detach`
and both take a **pointer**. So the list's row 7 is a phantom duplicate of row 8, and row
11 is `Detach` misspelled; the real function count is 19.

The only consumer of the phantom spellings was
`code/Game/Action/action_base_complete.cpp`, which resolved both with `ResolveExport` and
**no address fallback**, so `s_GeomAttach` and `s_GeomDetach` were NULL for the life of
that file and the two `if (s_Geom*)` guards around them had never fired — every attach and
detach `ActCt_ShowToolOnEntity` / `ActCt_HideTool` perform were silently skipped. **That
call site was corrected in the tree during this session** (it now reads `PEAV12@`, with a
comment crediting Wave 6), so this lane's registrations reach it. Recorded here because
the lane list still carries the phantom spellings and the next reader will meet them.

This file registers the two **correct** names only. Registering a name the DLL does not
contain would have put a live implementation behind a spelling nothing should ever use
again.

---

## 3. Struct layouts recovered, and the evidence

Full per-field evidence is in the two headers; this is the summary and the load-bearing
parts.

### 3.1 `Graphics::Geometry` — 0xC0 bytes (`runtime/types/graphics/Geometry.h`)

Whole-object witness (MEASURED, decompile + disassembly of 0x1800a4270): the ctor writes
every field except two four-byte holes. Independently corroborated on the exe side by
`engine/types/StatusIconComponent.h`, which has reserved `uint8_t geometry[0xC0]` at +0x28
and been live in-game for months.

| Off | Field | Pinned by |
|---|---|---|
| 0x00 | `_vtable` | ctor `LEA RAX,[0x180154e60]; MOV [RCX],RAX`; the dtor re-stamps it |
| 0x08 | `_attachParent` | `Attach` writes `child+0x08 = this`; `Detach`/dtor clear it |
| 0x10 | `_attachNode` | `Attach` writes the node index; `Detach`/dtor write -1; ctor -1 |
| 0x14 | **uninitialised, never read or written** | ctor writes +0x10 as a dword and skips it |
| 0x18 | `Array<Geometry*> _attached` (`data`, `size@0x20`, `cap@0x24`) | `Attach` grows it via the stride-8 `Array<T>::SetSize` 0x18001f990; dtor frees it via 0x18001bf50; `SetTransform`/`Detach`/`UpdateAttached` read `+0x20` |
| 0x28 | `_detailId` (ctor 2) | `?GetDetailId@` 0x180015890 is `return *(int*)(this+0x28)` |
| 0x2c | `_dirtyFlags` (ctor 7) | `?IsDirty@` 0x180015860 masks 7; `?NeedsCleaning@` 0x180015870 tests `!= 0`; `Clean` is the whole machine |
| 0x30 | `_materialTypeFlags` | `?GetMaterialTypeFlags@` 0x1800157f0 |
| 0x34 | `Box _localBounds` | `SetBounds` 0x180015700 compares then copies six floats; `Clean` hands `&this[0x34]` to the box transform |
| 0x4c | `Box _worldBounds` (ctor EMPTY: +1e8 / −1e8) | `Clean`'s bounds branch writes it |
| 0x64 | `Box _prevWorldBounds` (ctor EMPTY) | `Clean` rolls +0x4c into it; also the box `MeshInstance::AddDrawItems` unions for its LOD pick (`standalone/scene_watch.h`) |
| 0x7c | `_opacity` (ctor 1.0f) | `?GetOpacity@` 0x1800156f0 |
| 0x80 | `_opacityPrev` (ctor 1.0f) | `?SetOpacity@` 0x1800156c0 writes **both** as one qword `CONCAT44(v,v)` |
| 0x84 | `Transform _transform` | `SetTransform` compares and writes all seven floats |
| 0xa0 | `Transform _prevTransform` | `SetTransform` rolls +0x84 into it; `GetInstanceTransform` lerps from it |
| 0xbc | **uninitialised, never read or written** | ctor's last store is at +0xb8 |

`_dirtyFlags` bit meanings (MEASURED, from `Clean` plus the three setters that raise them):
bit 0 transform dirty, bit 1 bounds dirty, bit 2 opacity dirty; bits 3/4/5 are the
"changed last frame, previous copy is still fresh" bits `Clean` rolls them into and clears
on the following call. That two-stage roll is what gives the renderer one extra frame of
previous transform and previous world box to interpolate between.

### 3.2 The `Geometry` vtable — nine slots (MEASURED)

`??_7Geometry@Graphics@@6B@` is exported at **0x180154e60, ordinal 1439**. Reading the
bytes there gives nine pointers followed immediately by the ASCII string `"ImageBuffer\0"`,
which is how a vtable's end is read in this project
(`reference_vtable_slot_count_from_the_name_string.md`):

| Slot | Off | Target | What |
|---|---|---|---|
| 0 | +0x00 | 0x180015e90 | scalar deleting destructor |
| 1 | +0x08 | 0x18007ec28 | `_purecall` |
| 2 | +0x10 | 0x18007ec28 | `_purecall` |
| 3 | +0x18 | 0x18007ec28 | `_purecall` |
| 4 | +0x20 | 0x1800a4020 | `UpdateAttached` |
| 5 | +0x28 | 0x1800a43c0 | `GetSubTransform` |
| 6 | +0x30 | 0x1800154a0 | `GetInstanceTransform` |
| 7 | +0x38 | 0x180015590 | `SetSharedInstanceData` |
| 8 | +0x40 | 0x1800a4420 | `Clean` |

Corroborated by the call sites: `Attach` and `UpdateAttached` call `vt+0x28`,
`SetTransform` and `UpdateAttached` tail-call `vt+0x20`, `Scene::Clean` calls `vt+0x40`.

### 3.3 `Graphics::Scene` — 0x78 bytes (`runtime/types/graphics/Scene.h`)

`Graphics::Scene` is **not polymorphic** (MEASURED): the export directory contains no
`??_7Scene@Graphics@@6B@`, the ctor stores no vtable, and every exported member is
non-virtual. It is a quadtree hash grid plus a doubly linked list of "global" objects that
skip the grid.

| Off | Field | Pinned by |
|---|---|---|
| 0x00 | `_cellSize` = 2048.0f | ctor passes `DAT_180161730`; the bytes there are `00 00 00 45` = 2048.0f (MEASURED, read out of the DLL, not a Ghidra rendering) |
| 0x04/0x08 | `_halfCellX/_halfCellY` | grid init writes `CONCAT44(p*0.5f, p*0.5f)` |
| 0x0c | `_levelCount` = 8 | grid init; `FUN_18005ee30` clamps against it |
| 0x10 | `Array<Node> _nodes` (`size@0x18`) | `Clean` reads `+0x18`; `RemoveObject` indexes `+0x10` at stride 0x18 |
| 0x20 | `Array<uint32> _freeIds` | `RemoveObject` appends through the stride-4 `SetSize` 0x180023830; the insert pops the last entry |
| 0x30 | `Array<int32> _cellHeads` — 0x5555 entries, seeded −1 | grid init; `FUN_18005f090` indexes it by `node->_cell` |
| 0x40 | `Array<int32> _levelOffsets` — 8 entries `{0,1,5,0x15,0x55,0x155,0x555,0x1555}` | grid init (the 4ⁿ prefix sums) |
| 0x50 | `Array<int32> _cellCounts` — 0x5555 entries, seeded 0 | grid init |
| 0x60 | `LinkedList<Geometry*> _globals` (head, tail@0x68, count@0x70) | `AddGlobalObject` allocates a 0x18-byte `{element, prev, next}` node and links at the tail; `RemoveGlobalObject` unlinks it — exactly the Wave 2 `Collection::LinkedList` shape |

`Scene::Node`, 0x18 bytes: `_object@0x00`, `_next@0x08`, `_prev@0x0c`, `_cell@0x10`
(`< 0` = free slot), `_pad14@0x14`. Corroborated by `standalone/scene_watch.cpp`, which
has been reading `+0x10` as the object pointer and `node+0x10` as the free flag since
2026-09-04.

---

## 4. Registration: both doors, and why

MEASURED, grep across `code/`, `engine/`, `standalone/`, `runtime/`, `dx11/`: almost every
lane-D call site resolves **by name**, but two resolve **by address**:

* `code/Game/Action/showtool.cpp:171` — `Attach` via `ResolveAddr(Module::Runtime, 0x1800a3fa0)`
* `code/Game/Game/weather_particles.cpp:69` — `~Geometry` via `ResolveAddr(Module::Runtime, 0x1800a41c0)`

`standalone/resolve_standalone.cpp` keeps `RtExportReg` and `RtAddrReg` apart, so a
name-only registration would have left those two call sites on the DLL while every other
caller ran the port — two implementations taking turns on one live `Geometry`, and a diff
oracle that under-counts. All 19 are therefore registered under **both** the mangled name
and the retail address, and every name is spelled as a **string literal at the
registration call** so `tools/runtime_census.py` can see it
(`reference_census_cannot_see_nonliteral_registrations.md`).

---

## 5. Vtable ownership — what this registration does and does not redirect

This is the PLAN §4.3 risk 3 answer for lane D, stated rather than assumed.

**This port does NOT stamp a vtable of its own.** The ported `Geometry` constructor stores
the DLL's own exported `??_7Geometry@Graphics@@6B@` (0x180154e60), byte for byte what
retail stores. Consequences:

* **Reached through this registration (all 19):** every call that goes through
  `ResolveExport`/`ResolveAddr` — which is every one of our call sites, including the five
  virtuals, because `engine/funcs/engine_funcs.cpp:5270-5278` resolves
  `UpdateAttached` / `GetSubTransform` / `GetInstanceTransform` / `SetSharedInstanceData` /
  `Clean` by NAME into direct `fn_Geometry_*` pointers and calls them directly, not
  through any object's vtable.
* **Still dispatches into the DLL:** every genuine *virtual* dispatch on a base
  `Geometry`, including the three inside this file (`Attach` and `UpdateAttached` → slot 5;
  `SetTransform` and `UpdateAttached` → slot 4; `Scene::Clean` → slot 8). For a subclass it
  dispatches to the subclass, in the DLL or in `code/Game/`, exactly as retail does.
  **This is not a gap in the port — it is retail's dispatch, reproduced.** A `Geometry`
  built by a DLL ctor and one built by our ctor carry the identical vtable pointer, so the
  two implementations can never diverge on which body a virtual call reaches.
* **No detour is installed.** The DLL's own internal callers of these nineteen still reach
  the DLL. That is deliberate: the objects are shared and byte-identical in layout, so a
  DLL-internal `Geometry::Clean` and our `Clean` operate on the same bytes with the same
  result. Installing a detour would be the first ever use of that mechanism on a port
  (PLAN §4.2 closing note) and this lane has no oracle that would justify it.

**Why not stamp our own table.** `code/Game/Component/statusicon_geometry.cpp:336`
(`StatusIconGeometry_BuildVtable`) builds the status-icon billboard's vtable by **copying
slots out of whatever the `Geometry` ctor stamped** — `statusiconcomponent.cpp:611` reads
`baseV = *(void***)(s + 0x28)` immediately after calling the ctor. Keeping the DLL's table
means that copy is unchanged by this port (MEASURED: both before and after, `baseV` is
0x180154e60). Stamping our own would silently change every slot it copies.

---

## 6. Findings and stale comments elsewhere in the tree

Neither is in a file this lane may edit; both are quoted with the correction.

### 6.1 `StatusIconGeometry_BuildVtable` copies **12** slots out of a **9**-slot vtable

`code/Game/Component/statusicon_geometry.cpp:336-348`:

```cpp
    for (int i = 0; i < 16; ++i) {
        // The billboard vtable has 12 real slots; copy what the base provides (slot 9
        // is null in the original) and stub anything we cannot source.
        if (baseVtbl && i < 12 && baseVtbl[i])
            s_siGeomVtable[i] = baseVtbl[i];
```

**Correction (MEASURED):** the base `Graphics::Geometry` vtable at 0x180154e60 has **nine**
slots, terminated by the type-name string `"ImageBuffer\0"` (§3.2). `baseVtbl[9]`,
`[10]` and `[11]` are therefore the ASCII bytes of `"ImageBuffer"` /
`"GeneratedImageBuffer"`, which are non-zero, so the `baseVtbl[i]` test passes and three
string fragments are installed as function pointers in slots 9-11. Slot 9 is not "null in
the original" — it is not a slot at all.

This is dormant today: nothing dispatches past slot 8 on a Geometry, which is exactly the
shape `reference_vtable_slot_count_from_the_name_string.md` warns about (*"slots past the
end are dormant, so fixing one unleashes a burst of new crashes"*). The fix is `i < 9`,
and it should be made by whoever owns that file, with an eye on whether the two overridden
slots (2 and 3) are still the ones intended — in the base table those are `_purecall`.

### 6.2 `engine/funcs/engine_funcs.h:6142-6145` — "best-effort" arg layouts, now confirmed

```
// Inherited base methods of Graphics::Geometry (used by +0x20..+0x40 thunks).
// These are dispatched via .rdata IAT slots in the original; in standalone we
// resolve them once and call directly. Signatures inferred from Runtime export
// mangled names — none are heavily used by the menu-scene render path so the
// arg layouts are best-effort.
```

**Correction:** the four typedefs under that banner are no longer best-effort — all four
were checked against the retail disassembly by this lane and all four are **correct**:

* `fn_Geometry_UpdateAttached_t (void*)` ✓ (0x1800a4020 takes only `this`)
* `fn_Geometry_GetSubTransform_t (void* self, void* retval, int idx)` ✓ — hidden return
  slot in RDX, index in R8D (0x1800a43c0)
* `fn_Geometry_GetInstanceTransform_t (void* self, void* retval, int idx, float t)` ✓ —
  hidden return slot in RDX, index in R8D, `t` in XMM3 (disassembly of 0x1800154a0)
* `fn_Geometry_SetSharedInstanceData_t (void* self, void* context, int idx, float t) -> int` ✓
  (0x180015590, whose entire body is `return 0`)

The `+0x20..+0x40 thunks` phrasing is also right but under-specified: those are vtable
slots 4-8 of the nine-slot table in §3.2.

### 6.3 Retail leaves `Scene::Node+0x14` **uninitialised**, in the shipped DLL

MEASURED, disassembly of `Scene::RemoveObject` at 0x180102600:

```
180102630: MOV qword ptr [RSP + 0x28],RCX        ; RCX = 0
180102639: MOV dword ptr [RSP + 0x30],0xffffffff
180102641: MOV RAX,qword ptr [RSP + 0x30]        ; reads [RSP+0x34] too — never written
18010264b: MOV qword ptr [RBX + 0x10],RAX
```

`[RSP+0x34]` is a local that nothing writes and that is outside the shadow space of the
preceding call, so the high dword stored into `node+0x14` is whatever the stack happened
to hold. The grid insert `FUN_18005e5e0` builds the same `CONCAT44(uStack_14, 0xffffffff)`
pair. This is the same class of finding as Wave 5's `Picking::Scene::RemoveMesh`
uninitialised `Entry::_userValue2`, and it says the same thing: `node+0x14` is a
**don't-care** that no retail code reads.

The port writes the half retail specifies (`_cell = -1`) and leaves the other half alone —
a port cannot reproduce an indeterminate value — and **no oracle in this file compares
`+0x14`**. Both `Geometry` replay comparisons likewise skip the object's own two
uninitialised holes (+0x14 and +0xbc).

---

## 7. What stays in the DLL, and why

Thirteen retail bodies are called at their addresses rather than re-derived. All are
`FUN_`-named internals shared across containers, and every one of them allocates or frees
through `System::MemoryAllocator` under that allocator's Win32 critical section — which
cannot appear under `runtime/src/` (PLAN §4.1 decision 1). This is the same shape Wave 5
lane D used for `Picking::Scene` and Wave 4 lane C used for `Map_Find`/`Map_Insert`.

| Address | What it is | Called from |
|---|---|---|
| 0x18001f990 | `Array<Geometry*>::SetSize(n, &fill)` (stride 8) | `Geometry::Attach` |
| 0x18001bf50 | `Array<Geometry*>::~Array` | `~Geometry` |
| 0x180023830 | `Array<uint32>::SetSize(n, &fill)` (stride 4) | `Scene::RemoveObject` |
| 0x18001c750 | `Array<int32>::~Array` (`Collection_Array_FreeBufferAndZero`) | `~Scene`, four times |
| 0x18005fc20 | `Array<Node>::~Array` | `~Scene`, tail call |
| 0x1800c7a50 | `MemoryAllocator::MemAlloc(size, align)` — the Enter/Allocate/Leave static | `Scene::AddGlobalObject` |
| 0x180009c30 | the allocator-lock `Free` wrapper | `Scene::RemoveGlobalObject` |
| 0x18005e4a0 | the Scene grid initialiser (~350 KB of allocation) | `Scene::Scene` |
| 0x18005e5e0 | the grid insert, returns the node id | `Scene::AddObject` |
| 0x18005f090 | the grid unthread | `Scene::RemoveObject` |
| 0x18005ee30 | the grid re-index | `Scene::Clean` |
| 0x18005f1f0 | the global-list walk-and-free | `~Scene` |
| 0x18001f470 | `Volume::Box` transform (world AABB of a transformed local box) | `Geometry::Clean` |
| 0x18001d500 | `Math::Quaternion::Lerp` (hidden return slot) | `Geometry::GetInstanceTransform` |
| 0x18001e5f0 | `Math::Transform::AsMatrix` (hidden return slot) | `Geometry::GetInstanceTransform` |

Two deliberate choices worth naming:

* **The last three are called by ADDRESS, not by export name.** `Transform::AsMatrix` is a
  Wave 1 port registered by NAME (`runtime/src/math/transform.cpp:982`); routing this
  lane's arithmetic through another lane's registration would mean a divergence here could
  not be attributed to this lane. Neither address is in the runtime address registry today
  (MEASURED by grep).
* **`MemAlloc` is called by address for a different reason.** Wave 2 registers a port
  under `?MemAlloc@MemoryAllocator@System@@SAPEAX_KH@Z` that is armed only opt-in with a
  standing NO-GO verdict (`WAVE2_ALLOCATOR.md`). Going by name would make the global-list
  nodes come from whichever allocator that switch selected; going by address always
  reaches the DLL's, which is where the DLL's own free path (0x180009c30) will return them.

`FUN_18001f470` is the one arithmetic body left in the DLL on purpose rather than for the
allocator: it is a float-exact routine this lane has no independent way to verify, and
re-deriving it would plant an unverifiable divergence source inside the one `Geometry` body
that has a full replay oracle.

---

## 8. Oracle design, and its limits

Three shapes are used. All of them route their verdict through `RtFirstFailure`, which
reports the **first failing term by name** with its actual and expected value —
`reference_shadow_predicate_must_name_its_failing_term.md` is the reason: Wave 5's
`RemoveMesh` printed two identical strings on failure and cost an extra integration run.

**FULL (5 sites).** Pure reads: `GetTransform`, `GetSubTransform`, `GetInstanceTransform`,
`SetSharedInstanceData`, and the two constructors against a private scratch object. Both
implementations run, into separate buffers, and the results are compared byte for byte.

**FULL REPLAY (3 sites).** `Geometry::Clean`, `SetBounds`, and `SetTransform` when the
geometry has no children. Their entire effect is the object's own 0xC0 self-contained
bytes, so: snapshot, run ours, snapshot ours, restore the snapshot, run theirs, compare
0xC0 bytes (minus the two uninitialised holes) and the return value, restore ours. This is
the strongest oracle in the lane and it covers the two most intricate bodies.

**POST-STATE (11 sites).** Everything that mutates a live scene graph, allocates, frees, or
dispatches virtually into another object. Running the original afterwards would double-add,
double-free, or remove a second element, so it is not run at all; instead the port asserts
the contract retail's body must have established. Examples: `Detach` asserts the child is
gone from the parent's array, the count dropped by exactly one, and the child's
parent/node fields are cleared; `Scene::Clean` asserts no live node and no global is left
with any of the low three dirty bits.

**What post-state cannot say.** It cannot prove the *frees* in the two destructors were
correct — only `BANISHED_REDZONE=watch` can. It cannot prove the grid rethreading in
`AddObject`/`RemoveObject` put the node in the *right* cell, only that it is in *a* cell —
that is what `standalone/scene_watch.h`'s census exists for. And it cannot catch a
divergence that both implementations would produce, because there is only one
implementation of the grid internals.

`Scene::Scene`'s scratch oracle allocates and frees ~350 KB per construction under
`BANISHED_RT_DIFF=1`. That is deliberate and affordable: `Graphics::Scene` is constructed a
handful of times per session (the game world's and the minimap's), not per frame. The
scratch is torn down with the DLL's own destructor so it does not leak.

---

## 9. What a run has to establish

Nothing in this document is an integration result. The wave lead's run should confirm:

1. **`calls=` is non-zero for all 19 sites.** A port with no callers reports clean
   (`reference_uncalled_port_is_not_verified.md`). The ones at risk are `Attach`, `Detach`
   and `UpdateAttached` — `Attach`/`Detach` are reached only from the tool-carrying path in
   `action_base_complete.cpp` / `showtool.cpp`, which was **dead until the phantom-name fix
   landed today**, so this may be the first run in which they execute at all. A citizen
   picking up and putting down a tool is what exercises them.
2. **No new divergent site.** The Wave 6 pre-wave control is 103 sites / 24,858,882 calls /
   102 divergences in 2 sites, both the pre-existing Wave 1 1-ULP sin/cos family. Any new
   divergent site is a Wave 6 defect.
3. **`GetInstanceTransform` demands bit equality** on all 64 matrix bytes. It is the one
   lane-D body doing float arithmetic of its own (three lerps). If it diverges by 1 ULP the
   term to look at is the translation lerp order: retail is `prev + (cur − prev) * t`,
   component by component, x then y then z, and both sides call the same DLL
   `Quaternion::Lerp` and `AsMatrix`.
4. **`Scene::Scene`'s scratch oracle does not blow the heap** on a run that constructs the
   minimap scene repeatedly.
5. **`BANISHED_RT_GFXSCENE=0` A/B.** Free, and the only thing that separates a lane-D
   regression from another lane's.
