# Wave 6 — Lane E — `Graphics::MeshInstance` + `GraphicsMesh` + `EdgeMesh` + `EdgeMeshInstance`

Branch `wave/upgrade-path-complete`. Files owned by this lane:
`runtime/src/graphics/mesh.cpp`, `runtime/types/graphics/Mesh.h`, this document.

> **Status after integration run 4.** Runs 1 and 2 crashed in this lane (§6.6, §6.7,
> §6.8 — fixed). Runs 3 and 4 exit clean with the world running, 25.9M diffed calls, and
> the oracle reported **three divergent sites in this lane**. All three are now fixed, and
> the split matters:
>
> * **One real PORT defect** — `UpdateHierarchy` reassociated a float sum (§6.9). It
>   produced wrong last bits on ~5% of nodes on every frame.
> * **Two ORACLE defects** — the `MeshInstance()` ctor check compared seven bytes retail
>   never writes (§6.10), and the `PlayAnimation` check read the wrong end of the layer
>   array (§6.11). In both the port was correct and the predicate was wrong.
>
> The wave lead's A/B ruled out the tempting excuse for §6.9 before it reached me: under
> `BANISHED_RT_LIBM=dll` the two known Wave 1 sin/cos sites go to zero while
> `UpdateHierarchy` goes **up**, 2110 → 2593. It was arithmetic, not libm.
>
> **Every oracle verdict in this document is still EXPECTED-CLEAN, not clean.** No run has
> yet completed with all five fixes in.
>
> Five defects, one root cause worth stating once: **they were written from Ghidra's C
> output where only the disassembly is authoritative.** The decompile lost a dereference
> twice (§6.6, §6.8), printed a sign extension as a zero extension once (§6.7), and
> flattened an operation order once (§6.9); the two oracle bugs came from reasoning about
> structure rather than reading the instructions that write it. Every pointer-shaped,
> shift-shaped and float-shaped expression in this lane has since been re-read against the
> instruction stream.

**20 of the 24 functions in scope are ported and registered. `RtGraphicsMeshRegister()`
returns 20.** Two of the four not ported are DATA exports that cannot be registered before
cutover; the other two are held back with reasons in §2. `-fsyntax-only` with the exact
command the brief specifies is **warning-clean**.

Nothing in this lane has been RUN. The wave lead runs the game; every claim below is
static — read out of the shipped `Runtime-x64.dll` with Ghidra and `pefile` — except
where it says otherwise.

---

## 1. The scope table

Retail addresses and ordinals are `pefile` output against
`C:\GOG Games\Banished\Runtime-x64.dll`, taken **before any body was written**
(`reference_resolveexport_phantom_names.md`). **All 24 names are present in the shipped
export directory. Zero phantoms.** `tools/export_name_audit.py` reports 0 findings in
this lane's files (9 phantoms remain elsewhere in the tree, all pre-existing, all in
`engine/funcs/engine_funcs.cpp`).

| Export | Retail | Ord | Ported? | Oracle kind | Diff site name |
|---|---|---|---|---|---|
| `??0MeshInstance@Graphics@@QEAA@XZ` | 0x1800fffc0 | 275 | yes | FULL (scratch) | `Graphics::MeshInstance::MeshInstance()` |
| `??0MeshInstance@Graphics@@QEAA@AEBV01@@Z` | 0x1800170c0 | 274 | yes | post-state (invariant) | `Graphics::MeshInstance::MeshInstance(const&)` |
| `??1MeshInstance@Graphics@@UEAA@XZ` | 0x180017060 | 632 | yes | post-state | `Graphics::MeshInstance::~MeshInstance` |
| `?IsAnimating@MeshInstance@Graphics@@QEBA_NXZ` | 0x1800ff7d0 | 3097 | yes | FULL | `Graphics::MeshInstance::IsAnimating` |
| `?GetNodeToWorld@MeshInstance@Graphics@@QEBA?AVTransform@Math@@H@Z` | 0x180016a40 | 2813 | yes | FULL (scratch) — **never executed, see §6.7/§6.8** | `Graphics::MeshInstance::GetNodeToWorld` |
| `?RootAnimationOffset@MeshInstance@Graphics@@QEBA?AVVector3@Math@@XZ` | 0x180016ef0 | 3589 | yes | FULL (scratch) — **never executed, see §6.7/§6.8** | `Graphics::MeshInstance::RootAnimationOffset` |
| `?UpdateHierarchy@MeshInstance@Graphics@@QEAAXXZ` | 0x1800ff910 | 4518 | yes | FULL (replay) | `Graphics::MeshInstance::UpdateHierarchy` |
| `?SetMaterialInstance@MeshInstance@Graphics@@…` | 0x1800ffe80 | 3908 | yes | FULL (replay) | `Graphics::MeshInstance::SetMaterialInstance` |
| `?SetMesh@MeshInstance@Graphics@@…` | 0x1800ffea0 | 3915 | yes | FULL (replay) — **never executed, see §6.6** | `Graphics::MeshInstance::SetMesh` |
| `?PlayAnimation@MeshInstance@Graphics@@…` | 0x1800ff840 | 3421 | yes | post-state (partial, see §3) | `Graphics::MeshInstance::PlayAnimation` |
| `?GetMaterialList@GraphicsMesh@Graphics@@…H@Z` | 0x1800126c0 | 2768 | yes | FULL | `Graphics::GraphicsMesh::GetMaterialList` |
| `?GetNodeIndexFromName@GraphicsMesh@Graphics@@QEBAHPEBD@Z` | 0x1800fee90 | 2812 | yes **(+ by address)** | FULL | `Graphics::GraphicsMesh::GetNodeIndexFromName` |
| `??0EdgeMesh@Graphics@@QEAA@XZ` | 0x1800a2830 | 161 | yes | post-state | `Graphics::EdgeMesh::EdgeMesh()` |
| `??0TriangleVertex@EdgeMesh@Graphics@@…` | 0x1800a2f50 | 462 | yes | FULL (scratch) | `Graphics::EdgeMesh::TriangleVertex::TriangleVertex` |
| `??0EdgeVertex@EdgeMesh@Graphics@@…` | 0x1800a2df0 | 164 | yes | FULL (scratch) | `Graphics::EdgeMesh::EdgeVertex::EdgeVertex` |
| `?Set@EdgeMesh@Graphics@@QEAA_NH…` | 0x1800a23e0 | 3813 | yes | post-state | `Graphics::EdgeMesh::Set` |
| `?SetMaterials@EdgeMesh@Graphics@@…` | 0x1800a2760 | 3910 | yes | FULL (replay) | `Graphics::EdgeMesh::SetMaterials` |
| `??0EdgeMeshInstance@Graphics@@QEAA@XZ` | 0x1800a3ab0 | 163 | yes | FULL (scratch) | `Graphics::EdgeMeshInstance::EdgeMeshInstance()` |
| `??0EdgeMeshInstance@Graphics@@QEAA@AEBV01@@Z` | 0x1800174f0 | 162 | yes | post-state (invariant) | `Graphics::EdgeMeshInstance::EdgeMeshInstance(const&)` |
| `?SetMesh@EdgeMeshInstance@Graphics@@…` | 0x1800a3a20 | 3914 | yes | FULL (replay) | `Graphics::EdgeMeshInstance::SetMesh` |
| `?AddDrawItems@MeshInstance@Graphics@@UEBAX…` | 0x180100f20 | 1550 | **NO** | — | — |
| `?Serialize@MeshInstance@Graphics@@QEAAXAEAVSerializer@IO@@@Z` | 0x1800ff680 | 3707 | **NO** | — | — |
| `?_rootOnlyUpdates@MeshInstance@Graphics@@0_NA` | 0x1801fc70b | 4796 | **NO** (DATA) | — | — |
| `?_typeInfo@GraphicsMesh@Graphics@@0VBaseTypeInfo@Object@@A` | 0x1801fa708 | 4869 | **NO** (DATA) | — | — |

`tools/runtime_census.py` sees all 20 registrations (verified by running the census's own
`concat_literal()` over the file: 20 names, each byte-identical to a shipped export).

### Registration form — why only one address registration

The brief warned that "several of your call sites are address-only
(`standalone/address_registry.cpp`)". **That reading of the tree is wrong and the
correction matters, because acting on it would have registered `AddDrawItems` by address
and broken a diagnostic.** `address_registry.cpp` spells every one of this lane's
dispatch slots as

```
RegisterDispatchSlot(0x1401514a8, (uintptr_t)ResolveExport("??1MeshInstance@Graphics@@UEAA@XZ"));
```

— the exe slot is *storage*, the *resolution* is by name, so a name registration flips it.
A grep of `code/`, `engine/`, `standalone/`, `runtime/` and `dx11/` for all 22 code
addresses in this lane finds exactly three hits, and only one of them is a call:

| Address | Site | What it is |
|---|---|---|
| `0x1800fee90` | `code/Game/Action/showtool.cpp:169` | **a real call** — `ResolveAddr(Module::Runtime, …)` fallback, taken because exe slot `0x140152970` is empty in standalone. **Registered by address.** |
| `0x180100f20` | `standalone/scene_watch.cpp:401` | **not a call — an identity token.** See §2.1. |
| `0x1800174f0` | `engine/funcs/engine_funcs.cpp:3391` | a latent fallback under a wrong name. See §5.1. |

---

## 2. Held back, with reasons

### 2.1 `AddDrawItems` — two independent reasons

**(a) It is virtual and the DLL is the only dispatcher.** Objects built by the ported ctor
carry `??_7MeshInstance@Graphics@@6B@` (0x180154dc0), the DLL's own exported vtable. The
only caller is the DLL's `Graphics::Renderer` walking the scene, and it dispatches through
that vtable. Registering the export redirects nothing that executes — the port would be a
registered function with `calls=0`, which is precisely what
`reference_uncalled_port_is_not_verified.md` exists to prevent.

**(b) Registering it *by address* would be active harm.** `standalone/scene_watch.cpp:401`
does not call it:

```cpp
if (s_addDrawItems == 0) s_addDrawItems = ResolveAddr(Module::Runtime, 0x180100f20);
...
return reinterpret_cast<uintptr_t>(vt[3]) == s_addDrawItems;
```

It resolves the address and compares it against a **live object's vtable slot 3** to decide
whether the object is a `MeshInstance`. Register the address and `ResolveAddr` returns our
function, the compare fails for every object in the scene, and `BANISHED_LOD_CENSUS` /
`BANISHED_FORCE_LOD0` quietly report an empty world. This is the same shape as
`0x1400f2f00` in PLAN §1.1, which "must stay unregistered" because registering makes the
dispatcher call itself.

The body *is* reversed (see §4.5) and depends on `Graphics::Renderer::AddDrawItem`, which
is **lane F's**. If lane F wants it, it must first give `scene_watch` a different identity
token.

### 2.2 `MeshInstance::Serialize` — no safe oracle

The body is `AnimationState::Serialize(this+0xd8, s)`, then the material count through the
`Serializer`'s encoding vtable at **+0xa0**, then on the read path a material-array resize
and a stream of that many elements. A differential run consumes or emits the record
**twice** and desyncs the stream — and a stream desync surfaces later, on an innocent
record (`reference_slot3_wrapper_must_resolve_first.md`,
`reference_serialize_lazy_resolve_and_wrong_slot.md`). Wave 5 held four functions back on
exactly this standard and lane E applies it unchanged.

Second, independent hazard: the encoding vtable **offsets differ between retail and the
profile build** (`reference_encoding_vtable_offsets_differ_between_builds.md`). Retail's
call here is `+0xa0`; a port written from the profile decompile would use the profile
offset, desync every save, and look correct in the source. Recording it so nobody rewrites
this from the profile build later.

It stays on the DLL, where dispatch slot `0x140152990` already points it.

### 2.3 / 2.4 The two DATA exports

`?_rootOnlyUpdates@MeshInstance@Graphics@@0_NA` (0x1801fc70b, ord 4796) is a single static
`bool`; `?_typeInfo@GraphicsMesh@Graphics@@0VBaseTypeInfo@Object@@A` (0x1801fa708, ord
4869) is the class's `Object::BaseTypeInfo`. The runtime registry redirects *calls*; a data
symbol has nothing to register before cutover — the same reason Wave 3 left
`FileSystem::_singleton` and Wave 4 left `MemoryAllocator::_singleton` unported. Both were
looked up in the export directory anyway so that WAVE6.md's denominators are not silently
short by two. Their three consumers (`citizenai_executor.cpp`,
`address_registry.cpp:1086`, `perf_experimental.cpp:125`) already resolve and use them
correctly and this lane changes nothing about them.

### 2.5 NOT a hold-back, but the wave lead must know: the EdgeMesh family may never run

`standalone/address_registry.cpp:584` describes `RegisterPlayerRenderBatch` — the
selection-outline chain that is the **only** consumer of `EdgeMesh`, `EdgeMeshInstance` and
the two vertex ctors — as *"GATED default-OFF on `BANISHED_SELECTION_OUTLINE`"*. If that is
still true, all **seven** EdgeMesh ports will report `calls=0`, and a clean run would be
telling you nothing about them.

**Ask for one run with `BANISHED_SELECTION_OUTLINE=1` and a building selected, and read the
`calls=` column, not just `divergences=`** (`reference_uncalled_port_is_not_verified.md`).

---

## 3. What each oracle actually proves — and what it does not

`rt_diff.h` is explicit that "anything that mutates shared state is NOT diffable" and that
the original "must never write into live state". Six of the twenty could not get a
full ours-vs-theirs comparison. Stating what each substitute really tests:

| Function | Why not FULL | What the check does test |
|---|---|---|
| `~MeshInstance` | theirs would double-free three buffers | the three arrays and Geometry's attach array are cleared |
| `MeshInstance` copy ctor | theirs into scratch allocates three arrays, and the only way to free them is `~MeshInstance` → `~Geometry`, which walks an attach array **shared with the source** and clears live siblings' parent link and sceneId. That is corrupting live state to run an oracle. | every field this port writes equals the source's, the three array element counts match, the vtable is stamped |
| `EdgeMeshInstance` copy ctor | same | mesh, followed pointer, vtable |
| `EdgeMesh::EdgeMesh` | `Video::VertexBuffer::PlatformConstruct` (0x1800f57e0) asks `Video::Device::_singleton`'s plugin for a companion object **and links the buffer into the device's global intrusive list** under the device critical section. A scratch run registers eight phantom buffers in a live global list and creates eight orphan dx11 companions. | vtable, the four External fields, the four material slots, the two list Arrays, the bounds box |
| `EdgeMesh::Set` | the eight uploads go to the GPU through `dx11/`; a second `Create` on an already-sized buffer is not a proven no-op | the bounds copy and both array sizes — the port's own arithmetic. The eight virtual uploads are left to the return value, which the caller already tests. |
| `PlayAnimation` | it starts/blends an animation; replaying applies the blend twice | that the layer `AnimationState` just wrote carries the group handle and the animation index we forwarded — i.e. the argument marshalling, which is the real risk here |

**`PlayAnimation` is the weakest of the twenty and it is worth naming precisely what stays
unverified.** The check covers arguments 1 and 2 (the `ExternalPtr` in RDX and the `int` in
R8D). It does **not** cover `const Time& startTime` (arg 4), `const Time& blendTime`
(arg 5) or `float weight` (arg 6). Arguments 5 and 6 are exactly where this project's
recorded pointer-typed-as-`int` trap bites — a pointer parameter is only wrong from the
fifth argument on, where the stack slot's high dword is garbage
(`reference_hidden_retval_and_int_pointer_args.md`). They are typed against the **mangled
name**, not the decompiler's parameter list, but they are not *tested*. If a citizen walks
at the wrong speed or an animation starts at the wrong frame after this wave, this is the
first function to suspect.

Every predicate in the file is a conjunction whose message names **all** of its terms, and
they are kept in sync deliberately: a message that describes fewer terms than the verdict
computes is how Wave 5 lost a day on `RemoveMesh`
(`reference_shadow_predicate_must_name_its_failing_term.md`).

---

## 4. Struct layouts recovered, and the evidence

Full per-field evidence is in `runtime/types/graphics/Mesh.h` next to each field. Summary:

### 4.1 `Graphics::MeshInstance` — 0x120 bytes

Witnessed field by field by the **disassembly** of `??0MeshInstance@Graphics@@QEAA@XZ`
(the decompile alone is ambiguous about widths):

| Off | Field | Instruction |
|---|---|---|
| 0x00 | `Geometry` base | `CALL 0x1800a4270` then `LEA RAX,[0x180154dc0]; MOV [RBX],RAX` |
| 0xc0 | `ExternalPtr<GraphicsMesh> _mesh` | `MOV qword ptr [RBX+0xc0],RCX` |
| 0xc8 | `Array<ExternalPtr> _materials` | qwords at +0xc8 / +0xd0 |
| 0xd8 | `AnimationState` (0x18) | `LEA RAX,[RBX+0xd8]`, then **`MOV byte ptr [RAX],CL`** — a BYTE — then qwords at `[RAX+8]` / `[RAX+0x10]` |
| 0xf0 | `Array<InterpolatedState<Transform>> _nodes` | qwords at +0xf0 / +0xf8 |
| 0x100 | `Vector3 _rootAnimationOffset` | qword at +0x100 + `MOV dword [RBX+0x108],ECX` |
| 0x10c | `float _color[4]` | four `MOV dword ptr [...],0x3f800000` |
| 0x11c | `int16_t _sortBias` | **`MOV word ptr [RBX+0x11c],CX`** — a WORD |

The last store being a word, not a dword, is what fixes `sizeof` at **0x120** and leaves
+0x11e as padding. Cross-check: `standalone/address_registry.cpp:1343` calls
StorageDisplayComponent's array element "0x128-byte" and says it hand-copies "sceneId at
+0x120" — that is the *exe's* element (a MeshInstance plus a trailing int), and the two
numbers agree.

### 4.2 `Graphics::InterpolatedState<Transform>` — 0x38 (two Transforms)

`UpdateHierarchy` strides 0x38 (`ADD RSI,0x38`), writes seven dwords, then copies those
seven to +0x1c..+0x37. So the element is two adjacent 0x1c-byte `Math::Transform`s.

**Which half is "current" is NOT pinned by this lane** and the header says so. Every write
lane E ported sets both halves to the same value, and `GetNodeToWorld` reads the first.
`InterpolatedState<T>::Lerp` is the function that settles it and PLAN §4.2 wave 1 deferred
it for want of a layout — still deferred. **Lane G should close this**; until then nobody
should read `_state[0]` as "current" on this lane's authority.

### 4.3 `Graphics::GraphicsMesh::Node` — 0x48

Stride pinned twice independently: `GetNodeIndexFromName` advances nine qwords per node
(`puVar8 = puVar8 + 9`), `UpdateHierarchy` advances 0x48 (`ADD RBP,0x48`).
`+0x00` a plain `char*` name (dereferenced directly by the inlined strcmp), `+0x08` the
parent index, `+0x0c` the node-local `Transform`. That last one is pinned by
`LEA R8,[RCX+0xc]` at `0x1800ffa50`: the same R8 becomes the **third argument** of
`Quaternion::operator*` (so +0x0c is a Quaternion) *and* is read at +0x10/+0x14/+0x18 for
the local translation (so +0x1c is a Vector3) — exactly `Math::Transform`'s own shape.
`+0x28..+0x47` is not pinned; no lane-E body touches it.

### 4.4 `Graphics::EdgeMesh` — 0x180, and `EdgeMeshInstance` — 0xd0

`EdgeMesh` = `Object::External` inlined (three vtable stamps at +0x00, zeros at
+0x08/+0x10/+0x18), four material `ExternalPtr`s at +0x28..+0x40, **eight** 0x20-byte
video-buffer headers at 0x48/0x68/0x88/0xa8/0xc8/0xe8/0x108/0x128, two `Array`s at
+0x148/+0x158, a `Box` at +0x168. The 0x20 header size and the `_deviceData` at +0x08 are
pinned from two directions: `PlatformConstruct` writes `*(this+8)`, and `EdgeMesh::Set`
reads the buffer *object* pointers at +0x50/+0x70/+0x90/+0xb0/+0xd0/+0xf0/+0x110/+0x130 —
each exactly 8 past a header base.

`EdgeMeshInstance` = `Geometry` + `ExternalPtr _mesh` at +0xc0 + `MeshInstance* _followed`
at +0xc8. See §5.2 for the uninitialised-field finding.

### 4.5 `Graphics::Geometry` — deferred to lane D, and we agree

`runtime/types/graphics/Geometry.h` already existed (lane D, running concurrently) with a
better-evidenced `Geometry`. Lane E **includes it instead of redeclaring it** — two
definitions of `RtGraphics::Geometry` in one namespace across two TUs is an ODR violation
whose drift would be invisible until a field moved. Lane E independently confirmed
+0x2c `_dirtyFlags`, +0x30 `_materialTypeFlags`, +0x34 `_localBounds` and +0x84
`_transform` against the same retail bodies and **agrees with lane D on all of them**, and
adopted lane D's names (`_detailId`, `_opacity`, `_opacityPrev`) over the ones
`AddDrawItems` alone suggested (`_sortBias`, `_fadeA`, `_fadeB`) because lane D's come from
the exported getters `?GetDetailId@` / `?GetOpacity@`, which is stronger evidence.

`MeshAnimationState` / `MeshAnimationLayer` are named with a `Mesh` prefix on purpose, so
that if lane G declares the real `AnimationState` in `Animation.h` the two cannot collide.

---

## 5. Perf-sensitive members — exactly what the port does per call

The brief singles out `UpdateHierarchy` / `IsAnimating` / `PlayAnimation` because
`?_rootOnlyUpdates@…` being unregistered once cost 38% of sim time
(`project_stress_perf_rootonly_and_libcalls.md`) and because a `GetProcAddress` inside a
hot body cost 5.6 points on its own (`reference_rtoriginal_is_getprocaddress.md`).

**No `GetProcAddress` sits on a hot path in this file.** Every resolution is either

* inside an `if (rt_diff_on__)` guard — the oracle, which no shipping run enters — or
* wrapped in `RT_ORIGINAL_ONCE` or a function-local `static`, resolved once for the life of
  the process and thereafter a load-and-indirect-call.

Per call, concretely:

* **`IsAnimating`** — four loads and a compare. Nothing resolved. Identical work to retail.
* **`UpdateHierarchy`** — one `memcpy` for node 0, then per node: one parent-index load, a
  transcribed float block, one call through the cached `Quaternion::operator*` pointer
  (retail emits a direct `CALL 0x18001e280` at the same place), and two 0x1c-byte
  `memcpy`s. `RtQuatMul()` is a function-local static. The diff block, including its
  `malloc`, is entirely inside the guard.
* **`PlayAnimation`** — one cached indirect call, same as retail's.
* **`SetMesh`** — calls `UpdateHierarchy` exactly where retail does; all four DLL helpers
  it uses are cached `ResolveAddr` results (a base + ASLR-delta add, cached anyway).

**This port does not touch `_rootOnlyUpdates`.** Neither does retail's `UpdateHierarchy` —
the flag is read by `MeshInstance::Update`, which is a virtual that stays in the DLL. So
`standalone/perf_experimental.cpp`'s save/clear/restore of it (`FlushCulled`,
`FlushReplays`) keeps working unchanged, and its `ANIM_BATCH` decision still reads the same
`_instanced` byte at +0xd8 that `RootAnimationOffset` branches on. Its documented layout —
"layers at +0x08 (stride 0x38 …), count at +0x10" relative to the AnimationState — matches
every read in this lane exactly, which is a useful independent corroboration of +0xd8.

The float math in `UpdateHierarchy` is **transcribed, not rederived**, including the two
`* 0.0f` terms (the zero `w` of the vector-as-quaternion, which a compiler may not fold
without `-ffast-math`) and the exact association of every sum — a reassociated float sum is
a real divergence and Wave 1 found one in `Transform::FromDirection`.

---

## 6. Findings

### 6.1 A stale comment and a latent defect in `engine/funcs/` — lane E cannot fix it

`engine/funcs/engine_funcs.h:5513-5516` says:

```
// Graphics::EdgeMeshInstance::EdgeMeshInstance() — default ctor (constructs the
// ...
// ??0EdgeMeshInstance@Graphics@@QEAA@XZ   (Runtime 0x1800174f0)
```

and `engine/funcs/engine_funcs.cpp:3389-3392` acts on it:

```cpp
fn_EdgeMeshInstance_Ctor = ... ResolveExport("??0EdgeMeshInstance@Graphics@@QEAA@XZ");
if (!fn_EdgeMeshInstance_Ctor) {
    fn_EdgeMeshInstance_Ctor = ... ResolveAddr(Module::Runtime, 0x1800174f0);
}
```

**The address is wrong.** The shipped export directory says:

* `??0EdgeMeshInstance@Graphics@@QEAA@XZ` (default ctor) = **0x1800a3ab0**, ordinal 163
* `??0EdgeMeshInstance@Graphics@@QEAA@AEBV01@@Z` (**copy** ctor) = **0x1800174f0**, ordinal 162

The fallback is latent, not live — the name resolves, so the `if` is never entered. But if
it ever fired it would call the **copy ctor** with an uninitialised RDX and dereference it
twice (`*(other+0xc0)`, `*(other+0xc8)`) after running `Geometry`'s copy ctor over it.

**Correction for whoever owns `engine/funcs/`:** the comment's address should read
`Runtime 0x1800a3ab0`, and the fallback should resolve `0x1800a3ab0`. Better still, delete
the fallback — the name is present in the DLL and now also in the runtime registry.

### 6.2 An uninitialised field in the shipped DLL

`??0EdgeMeshInstance@Graphics@@QEAA@XZ` writes **only** `+0xc0` after the `Geometry` base
and the vtable stamp. The **copy** ctor writes `+0xc0` *and* `+0xc8`. So a
default-constructed `EdgeMeshInstance` leaves `+0xc8` — the back-pointer to the
`MeshInstance` the outline follows — holding whatever the allocator returned, and only
`EdgeMeshInstance::SetMesh` ever fills it in. A copy of such an object propagates the
garbage.

Same class of finding as Wave 5's `RemoveMesh` leaving `Entry::_userValue2` uninitialised.
**The port reproduces it**: it does not zero `+0xc8`, and its oracle excludes that offset
from the byte comparison (two different stacks hold two different pieces of garbage there,
so comparing them would report a divergence on every call). Zeroing it would make our ctor
differ from retail's on a live shared object, which is the one thing WAVE6.md forbids a
Video/Graphics lane.

### 6.3 `EdgeMesh::SetMaterials` crosses its middle two arguments

Retail stores arg1→+0x28, **arg2→+0x38, arg3→+0x30**, arg4→+0x40. Not a decompiler
artefact — the four `MOV`s name their source registers — and not something to tidy:
`EdgeMeshInstance::SetMesh` reads the same four slots back (in the order +0x38, +0x28,
+0x40, +0x30) and would see a different assignment if this were "corrected". Reproduced as
shipped, and the note is repeated in both `mesh.cpp` and `Mesh.h` so a reader of either
meets it.

### 6.4 A bug this lane found in its own port before it shipped

`GetNodeToWorld` and `RootAnimationOffset` share a pool-slot computation and then
**diverge by one dereference**. `RootAnimationOffset` reads three floats at `slot + 0x18`.
`GetNodeToWorld` loads a **pointer** at `slot + 0x38` and indexes *that* by `node * 0x38`.
The first draft shared the helper past the slot, which would have made `GetNodeToWorld`
read 0x38-byte strides out of the 0x48-byte pool entry itself — plausible-looking garbage
for every instanced (animation-pooled) model, i.e. every walking citizen. Caught by
re-reading the two decompiles side by side, not by any tool. The helper now stops at the
slot and each caller does its own last step; the reason is a comment on the helper.

### 6.6 The defect integration run 1 found: address-of vs value on `GraphicsMesh+0x70`

`MeshInstance::SetMesh` passed `B(mesh.p) + 0x70` — the ADDRESS of the material-list field —
to the Runtime `Array<ExternalPtr>::operator=`. Retail dereferences it:

```
FUN_180014c70((longlong *)(this + 200), *(longlong **)(param_2 + 0x70));
```

`GraphicsMesh+0x70` is a POINTER to an array of `Array` headers, not an embedded `Array`.
The helper therefore read the material-list pointer as its `_data` and whatever sits at
mesh+0x78 as its `_size`, so the copy loop walked `{_size=1,_capacity=1}` (0x100000001) as
an element pointer and faulted reading `+0x20` off it — `0x100000021`, exactly the address
in the crash log. Fixed to pass `_materialLists`, the same loaded pointer `GetMaterialList`
already used correctly 500 lines earlier.

**Why it looked plausible, which is the part worth keeping.** The two Runtime calls
immediately after it, and the one in `PlayAnimation`, correctly take the ADDRESS of an
embedded `Array` (`&mi->_nodes`, `mesh + 0x60`). Three lines apart, the same-looking
expression means opposite things. The tell is in the retail decompile every time: an
embedded Array is passed `(longlong *)(x + off)`, a pointer field `*(longlong **)(x + off)`.
**Read the stars, not the offsets.** The failure mode is silent whenever the garbage count
happens to be 0, so it will not always crash.

Two things now make this site un-repeatable rather than merely fixed: `_materialLists` is
declared as a real `Collection::Array<ExternalPtr>*` in `Mesh.h` (so the compiler rejects the
byte-offset form), and the +0x60-vs-+0x70 contrast is written out at both the field and the
call site.

**Audit of every other site in this file that hands a `Collection::Array` — or any
pointer-shaped field — to a Runtime helper.** Each was re-read against the retail decompile
for the same address-vs-value slip. **One defect, the one above; the other twelve are
correct.**

| Line | Call | Retail | Verdict |
|---|---|---|---|
| 800 | `RtMatArrayCopy(&mi->_materials, mesh+0x70)` | `*(longlong**)(param_2+0x70)` — **load** | **WAS THE DEFECT**, fixed to the loaded pointer |
| 813 | `RtAnimReset(&mi->_animation)` | `(AnimationState*)(this+0xd8)` — address (a `this`) | correct |
| 814 | `RtNodeArrayResize(&mi->_nodes, Rd<int32_t>(mesh,0x68))` | `(longlong*)(this+0xf0)`, `*(int*)(param_2+0x68)` | correct — Array by address, count by load |
| 904 | `RtAnimPlay(…, mesh+0x60, …)` | `*(longlong*)(this+0xc0) + 0x60` — address, **no load** | correct — `_nodes` IS embedded at +0x60. This is the near-miss that mirrors the defect. |
| 1031 | `RtMatArrayCopy(&mi->_materials, &src->_materials)` | `(this+200)`, `(param_1+200)` | correct |
| 1038 | `RtLayerArrayCopy(&…_layers, &src…_layers)` | `(this+0xe0)`, `(param_1+0xe0)` | correct |
| 1043 | `RtNodeArrayCopy(&mi->_nodes, &src->_nodes)` | `(this+0xf0)`, `(param_1+0xf0)` | correct |
| 1103-05 | three `*ArrayFree` on +0xf0, +0xe0, +0xc8 | `FUN_18001c550/…c480/…14d00` on the same three | correct, and in retail's order |
| 1371/75 | `RtTriListArrayResize(&em->_triangleLists / _edgeLists, n)` | `(this+0x148)`, `(this+0x158)` | correct |
| 1544 | `RtGeometrySetBounds(self, mesh+0x168)` | `(Box*)(param_2+0x168)` — embedded Box | correct |

Non-`Array` pointer fields were audited the same way and all load where retail loads:
`EdgeMeshInstance::SetMesh`'s four material slots (`*(longlong*)(param_2+0x38)` etc.),
`SetMesh`'s material-flag loop (`lVar1 = *plVar3` then `+0x20`), `IsAnimating`'s
group/animation chain, `EdgeMesh::Set`'s eight buffer-object pointers
(`*(longlong**)(this+0x70)`), and `UpdateHierarchy`'s `*(mesh+0x60)` — which loads because
it wants the Array's `_data`, the Array's first field, not the Array itself.

### 6.7 `RtInstancedPoolSlot` used a logical shift where retail uses an arithmetic one

The bank index. Retail (`0x180016f12`, and identically at `0x180016ae7`):

```
MOVSXD RCX, dword ptr [R8 + RAX - 0x2c]   ; poolId, SIGN-extended to 64
SAR    RAX, 0x8                           ; ARITHMETIC shift
MOVZX  EAX, AL                            ; & 0xff
```

The port had `(static_cast<uint64_t>(poolId) >> 8) & 0xff` on a `uint32_t` — a logical
shift on a zero-extended value. For a negative pool id the two pick banks 0x30 bytes apart:
for `0xFF123456` retail gets `0x45`, the port got `0x34`. Fixed to a sign-extended
`int64_t` and an arithmetic `>>`.

**Why the decompile invited it.** Ghidra prints `uVar5 = (ulonglong)*(int *)(...)` — which
reads as a zero extension — and only then `(longlong)uVar5 >> 8`. The `(ulonglong)` is the
decompiler's variable typing, not the instruction; the instruction is `MOVSXD`. Note also
that `0xffffffff` is the one negative id where both forms agree, so the bug hides on
precisely the value most likely to show up in a debug session.

### 6.8 `AnimationManager::_singleton` needs TWO dereferences — this was the run-2 crash

Retail, in both callers:

```
MOV RAX, qword ptr [0x1801fe7b0]      ; the _singleton VARIABLE's value = AnimationManager*
MOV RDX, qword ptr [RAX]              ; *** SECOND dereference ***
MOV RAX, qword ptr [RDX + R8*8 + 0x20]  ; bank = *(that + hi*0x30 + 0x20)
```

The port did one dereference and used the `AnimationManager*` itself as the base. `+0x20 +
hi*0x30` then lands **inside** the 0x9d8-byte AnimationManager object, so the "bank" was a
plausible-looking garbage qword rather than a fault — the crash surfaced one level later,
as a `memcpy` of 12 bytes from `0x09D5A497246892AF` in `RootAnimationOffset`'s instanced
branch, on the first citizen move tick.

**Why the decompile invited it.** Ghidra spells the load
`*(longlong *)AnimationManager::_singleton`, where the bare symbol **already means "the
contents of 0x1801fe7b0"**. The single printed star is therefore the *second* dereference,
not the first. One star in the C output, two `MOV`s in the machine code.

The whole helper is now transcribed from the disassembly with both traps written out above
it, and both callers share it — so `GetNodeToWorld` (mesh.cpp:521), which carried §6.7 and
§6.8 identically and sits on the same hot path, is fixed by the same change. Its own extra
step (`ADD RCX, [bank + lo*0x48 + 0x38]`, a third dereference to reach the per-node pose
array) was already correct — that was §6.4.

**What was ruled OUT, with instruction-level evidence, and why no guard was added.** The
lead's three candidates for the garbage pointer:

* *`_instanced` read from the wrong offset* — no. Retail's first instruction is
  `CMP byte ptr [RCX + 0xd8], 0x0`; the port reads a `uint8_t` at +0xd8.
* *`_layers._data` / `_size` not really at +0xe0 / +0xe8* — no.
  `MOV RAX, qword ptr [RCX + 0xe0]` and `MOVSXD RCX, dword ptr [RCX + 0xe8]`, exactly the
  struct.
* *the caller not passing the hidden return slot* — no. Retail's prologue is `MOV R9, RDX`
  and its epilogue writes three dwords through R9 and returns `RAX = R9`; the port's
  signature is `(const void* self, void* ret)` = RCX, RDX. The crash's `RDX`/`R8` were the
  `memcpy`'s own source and size, not the function's arguments.

So the empty-layer case is not what happened, and **no guard was added**: retail computes
`size*0x38 - 0x2c` unguarded too, and with the double dereference restored the port reads
the same three fields the DLL reads and reaches the same state.

### 6.9 `UpdateHierarchy` added the parent translation FIRST; retail adds it LAST

**A real port defect** — the only one of run 3/4's three divergent sites that was.
2110 of 41359 calls (run 3), 2593 of 41127 (run 4).

Retail accumulates each translation component's rotation sum to completion and only then
does a single `ADDSS` with the parent translation:

```
ffbf6 ADDSS XMM3,XMM1   / ffbfd SUBSS XMM3,XMM0   -> R_X
ffc04 ADDSS XMM8,XMM3                             -> p.x + R_X
ffc3d ADDSS XMM2,XMM1   / ffc41 ADDSS XMM2,XMM0   -> R_Y
ffc4f ADDSS XMM7,XMM2                             -> p.y + R_Y
ffc45 SUBSS XMM10,XMM11 / ffc4a ADDSS XMM10,XMM4  -> R_Z
ffc53 ADDSS XMM6,XMM10                            -> p.z + R_Z
```

The port wrote `py + (a - b) + c + d`, which folds `p.y` into the running sum after the
**first** term. Same for Z. A reassociated float sum — precisely the Wave 1 defect in
`Transform::FromDirection` (WAVE1.md). Fixed by computing `rx`/`ry`/`rz` in full and adding
`p` once.

Two things about how it presented. It diverged on ~5% of nodes, not all, because
reassociation only changes the result when the intermediate rounding differs — which is why
it reads as a "1-ULP float family" and invites the libm excuse. And the **X** component was
correct by accident: the first draft happened to wrap that whole rotation sum in one pair
of parentheses, so `p.x` was already added last there. Only Y and Z were wrong, and the
reported divergences are at +0x4c and +0x84 — inside node transforms, not at node 0.

The four quaternion-product terms (`T_X`..`T_W`) were re-derived instruction by instruction
at the same time and are correct as written, including both `* 0.0f` terms and every
association.

### 6.10 The `MeshInstance()` ctor oracle compared seven uninitialised bytes

**An oracle defect, not a port defect** — 30130 of 30130 calls, i.e. every construction, and
the verdict string already localised it (`tail=0`).

The ctor's tail comparison spanned +0xc0..+0x11d. The disassembly writes that range with
one exception: `LEA RAX,[RBX+0xd8]`, `MOV byte ptr [RAX],CL`, `MOV qword ptr [RAX+0x8],RCX`
— **nothing writes RAX+1..RAX+7**. So +0xd9..+0xdf, the padding between the AnimationState
bool and its layer Array, is uninitialised in retail. `scratch` is memset to 0xcd and the
live object holds whatever the allocator left, so those seven bytes could never agree.

This is the third uninitialised gap in these two classes, alongside Geometry's +0x14 and
+0xbc which the check already skipped — the mistake was assuming there were only two. The
comparison is now split into `mid` (+0xc0..+0xd8) and `tail` (+0xe0..+0x11d), and the
verdict string names all four terms.

The port itself writes exactly the bytes retail writes, verified store by store against the
disassembly: qwords at +0xc0/+0xc8/+0xd0, a byte at +0xd8, qwords at +0xe0/+0xe8/+0xf0/
+0xf8/+0x100, a dword at +0x108, four `0x3f800000` dwords at +0x10c..+0x118, a word at
+0x11c.

### 6.11 The `PlayAnimation` oracle read the wrong end of the layer array

**Also an oracle defect** — 187 of 453 calls, reported as `ours anim=0` / `theirs anim=1`
on a matching group pointer.

`AnimationState::PlayAnimation` (0x1800fdf50) grows the layer array by one and then walks it
**downward** copying `layer[i] = layer[i-1]` — it **inserts at the front**. The check read
`layers[count-1]`, which is the layer being pushed out, so it compared the *previous*
animation's index against the new one. Corroboration that front-insertion is the convention
and not a one-off: `MeshInstance::IsAnimating` decides whether the instance is animating
from `layers[0]` and nothing else.

Fixed to read `layers[0]`, and while in there the predicate was widened to cover two retail
paths it did not model at all:

* `index < 0` **clears every layer** (the early-return branch calls
  `FUN_18005c8a0(this+8, 0)`). The correct expectation there is an *empty* array — and
  reading `layers[0]` on that path would have been an out-of-bounds read in the oracle.
* Re-playing the same (group, index) on a **looping** animation returns early after writing
  `1.0f` to `layers[0]+0x10` and adds no layer. `layers[0]` still carries the right group
  and index, so the same check holds.

This does not upgrade `PlayAnimation`'s oracle rating. The three arguments named in §3 as
unverified — `startTime`, `blendTime`, `weight` — are still unverified.

### 6.5 Ghidra renames were NOT done, deliberately

The brief asks for `Graphics_MeshInstance_SetMesh`-style renames with `program=` passed.
**No rename tool in this MCP surface accepts a `program` argument** —
`rename_function_by_address`, `rename_function`, `rename_or_label` and
`batch_rename_function_components` all operate on the *current* program, and
`switch_program` is forbidden because it is global state shared with six concurrent lanes.
A rename attempt returned `No function found at address 0x180014c70`, confirming the
current program is not `Runtime-x64.dll`. Renaming into whichever program happens to be
current would corrupt another lane's database, so none were made.

Names that would be worth applying to `Runtime-x64.dll` once the lanes are serialised:

| Address | Proposed name |
|---|---|
| 0x180014c70 / 0x180014d00 | `Collection_Array_ExternalPtr_Assign` / `_Free` |
| 0x18001b700 / 0x18001c480 | `Collection_Array_AnimLayer_Assign` / `_Free` |
| 0x18001b810 / 0x18001c550 / 0x18005ac50 | `Collection_Array_InterpTransform_Assign` / `_Free` / `_SetSize` |
| 0x1800136f0 | `Collection_Array_TriangleList_SetSize` |

---

## 7. What lane E asks the wave lead for

1. **A run with `BANISHED_SELECTION_OUTLINE=1`** and a building selected. Without it the
   seven EdgeMesh ports report `calls=0` and are unverified (§2.5).
2. **Read the `calls=` column**, not just `divergences=`, for all 20 sites.
3. **The two `engine/funcs/` corrections in §6.1**, which lane E is not permitted to make.
4. A decision on one deliberate trade: `UpdateHierarchy` calls
   `Quaternion::operator*` through `RT_ORIGINAL_ONCE`, which is `GetProcAddress` and
   therefore **always the DLL** — it does not consult the runtime registry. That keeps a
   divergence at this site attributable to lane E, and it matches retail's direct `CALL`.
   The cost is that Wave 1's port of `??DQuaternion@Math@@QEBA?AV01@AEBV01@@Z` stays on the
   census orphan list with zero callers — and
   `reference_uncalled_port_is_not_verified.md` records a *wrong*
   `Quaternion::operator*` surviving all of Wave 1 exactly that way. Switching this one
   call to `ResolveExport` would finally exercise it, at the cost of folding a neighbouring
   lane's correctness into this lane's diff site. Lane E judged that not its call to make.
