# INVENTORY D — destructor calls, DeletingDtor calls, deletability (normal-virtual cutover, §3b Q3 + Q4)

Scope: product tree only — `code/ runtime/ standalone/ engine/ dx11/` (NOT `tests/`). Read-only. Tree = `main` at
`deaf604c` + the working-tree state of 2026-09-27/28. Comments stripped with `codegrep.strip` (strings kept), same
as `partc.py`, so line numbers are real file lines.

Scripts (scratch, re-runnable from the repo root, all read-only):
`C:\Users\USPC\AppData\Local\Temp\claude\C--code-GIT-untitled-rev-eng-project\e971e316-6783-48a1-a07d-308c78a61824\scratchpad\b3x\inv-d\`

| script | what |
|---|---|
| `classes.py` | class/struct index, polymorphic set (transitive over bases), "DeletingDtor hierarchy" set (461 names) |
| `classify.py dtor` / `classify.py dd` | Part 1 rows (`dtor.tsv`) / every `DeletingDtor(` call with its argument and receiver kind (`dd.tsv`) |
| `ddbodies.py [--show]` | every `DeletingDtor(unsigned)` definition body, normalised: STD / STD+VEC / NONSTD, and the free it uses |
| `inherited_destruct.py` | flip classes that inherit `Destruct`/`Reconstruct` from an ancestor |
| `deletable.py --list` | per class: dtor access, class-scoped `operator delete` in the hierarchy, hierarchy root (`deletable.tsv`) |
| `placement.py --list` | every new-expression constructing a flip class, by storage kind (`placement.tsv`) |
| `arrays.py` | containers holding a polymorphic class BY VALUE (Collection.h's `->~T()` / `element.~T()`) |

Known limits of the tooling: class identity is BY NAME (collisions such as `IO::Stream`/`Audio::Stream`,
`Graphics::MeshInstance`/`rt::MeshInstance`, `UI::Element`/`VertexLayout::Element` were resolved by hand for every
flagged row); typedef aliases of containers are NOT VERIFIED.

---------------------------------------------------------------------------------------------------------------------

## Part 1 — the 1,027 explicit destructor calls (partc's 1,051 minus 24 in tests)

### Counts

| area | IN-DELDTOR | IN-DELDTOR-VECTOR | MEMBER-VALUE | POLY-UNQUALIFIED | POLY-QUALIFIED | TEMPLATE | total |
|---|---:|---:|---:|---:|---:|---:|---:|
| code/Game/Action | 18 | 0 | 34 | 0 | 0 | 0 | 52 |
| code/Game/Component | 157 | 0 | 80 | 3 | 0 | 0 | 240 |
| code/Game/Dialog | 61 | 0 | 9 | 4 | 0 | 0 | 74 |
| code/Game/Entity | 1 | 0 | 1 | 0 | 0 | 0 | 2 |
| code/Game/Game | 17 | 0 | 100 | 1 | 0 | 0 | 118 |
| code/Game/Goal | 5 | 0 | 13 | 0 | 0 | 0 | 18 |
| code/Game/Player | 12 | 0 | 5 | 0 | 0 | 0 | 17 |
| code/Game/Tutorial | 21 | 0 | 42 | 0 | 0 | 0 | 63 |
| code/Game/Work | 23 | 0 | 28 | 0 | 0 | 0 | 51 |
| dx11 | 0 | 0 | 2 | 0 | 5 | 0 | 7 |
| runtime/src | 3 | 3 | 111 | 2 | 0 | 0 | 119 |
| runtime/types | 133 | 124 | 0 | 0 | 0 | 4 | 261 |
| standalone | 4 | 0 | 1 | 0 | 0 | 0 | 5 |
| **total** | **455** | **127** | **426** | **10** | **5** | **4** | **1,027** |

### The rules (mechanical majority)

- **IN-DELDTOR (455)** — the scalar arm `this->~T()` of T's own `DeletingDtor`, T == the enclosing class in every
  one of the 455 (the classifier requires it; zero mismatches). **Disappears with the body.**
- **IN-DELDTOR-VECTOR (127)** — the MSVC vector arm `reinterpret_cast<T*>(self + i*N)->~T()` inside
  `if (flags & 2)` (126 inline header bodies + `Object::Internal`'s uint64-count variant,
  `runtime/src/object/internal_class.cpp:390`). **Dead code** (see Part 2 — no caller sets bit 1);
  disappears with the body.
- **MEMBER-VALUE (426)** — non-polymorphic types: `IO::Serializer` 186 (placement-built on stack
  `uint8_t[24]`, every `Persist`), `Collection::Array` 103, `System::String` 81, `System::Filename` 36,
  `WideString` 3, plus 17 on non-polymorphic singletons (`AchievementManager`, `GoalList`, `Audio::Scene`,
  `Audio::/System::Configuration`, `IO::FileSystem`, `IO::Persistence`, `Picking::Scene`, `SaveInfo`,
  `SavedState`, `ModInfo`) — each re-checked: no `virtual`, no polymorphic base. **Unaffected.**
- **TEMPLATE (4)** — `Collection.h:277` (`TDestroy<T>`), `:1527/:1546/:1561` (tree node `element.~T()`).
  `arrays.py` finds NO container instantiated with a polymorphic class by value (the six hits were name
  collisions with PODs `VertexLayout::Element`, `VertexProgram::Element/Stream`). **Unaffected** (typedef'd
  instantiations NOT VERIFIED).
- **ALL DeletingDtor bodies are standard** (`ddbodies.py`): 458 definitions = 328 STD
  (`this->~T(); if (flags & 1) Free(this); return this;`) + 126 STD+VEC + 1 NONSTD (`Object::Internal`, only
  because its vector arm reads a uint64 count) + 3 pure. **No body does anything but its own dtor + free**
  (TRACE/printf/`EnsureInit()` lines aside: `GameUpdateLoop`, `InnerLoaderLoop`, splash/outer loader stubs).
- **No hand-written base chaining exists.** Zero `Base::~Base()` calls inside a derived dtor or DeletingDtor
  (the only qualified dtor calls on polymorphic classes are the 5 dx11 rows below), zero
  `Base::DeletingDtor(flags)` forwards, zero bare `X::~X();` statements anywhere in product code. Every derived
  `~T()` is a real C++ destructor whose base chain the compiler already emits (e.g.
  `ModelComponent::~ModelComponent`, `GameUpdateLoop::~GameUpdateLoop` bodies only touch their own members).

### FLAGGED — POLY-UNQUALIFIED (10): every unqualified explicit dtor call on a polymorphic class outside a DeletingDtor

| # | site | code | dynamic type at the site | verdict / post-flip spelling |
|---|---|---|---|---|
| 1 | `code/Game/Component/modelcomponent.cpp:242` | `reinterpret_cast<Graphics::MeshInstance*>(meshInstance)->~MeshInstance();` | exactly `Graphics::MeshInstance` — embedded member, placement-new'd in the ctor (`:231`); no subclass of MeshInstance exists | harmless, but retail is a DIRECT `CALL [??1MeshInstance]`: spell `->Graphics::MeshInstance::~MeshInstance()` (no vptr read) |
| 2,3 | `code/Game/Component/storagedisplaycomponent.cpp:316`, `:323` | `reinterpret_cast<Graphics::MeshInstance*>(p)->~MeshInstance();` | exactly MeshInstance — array elements placement-new'd at `:300/:304`, stride 0x128 = 0x120 + trailing int | same: qualify |
| 4-7 | `code/Game/Dialog/controluiconfig.cpp:84`, `resourcelimituiconfig.cpp:83`, `statusiconuiconfig.cpp:79`, `upgradeuiconfig.cpp:74` | `this->~ControlUIConfig();` then `return new (this) ControlUIConfig();` (Reconstruct) | all four classes are `final` | harmless (devirtualised); the ONLY 4 Reconstructs spelled with `this->~T()` instead of `DeletingDtor(0)` — keep unqualified like the other 171 (see Part 2) |
| 8 | `code/Game/Game/keystone_a_gamedtor.cpp:416` | `reinterpret_cast<PathfinderThread*>(queue)->~PathfinderThread();` then `MemFree(queue)` | `System::Thread` hierarchy | **EXEMPT hierarchy** (slot 0 = `Run`, `Thread.h:337` static_asserts no virtual dtor) — unaffected as long as Thread stays exempt |
| 9 | `runtime/src/engine/engine_lifecycle.cpp:892-893` | `reinterpret_cast< ::Audio::Stream*>(p + 0xa00)->~Stream();` | `Audio::Stream` (Thread subclass) | EXEMPT hierarchy — unaffected |
| 10 | `runtime/src/io/persistence_saves.cpp:340` | `reinterpret_cast<PersistenceWorker*>(&_workerVtable)->~PersistenceWorker();` | embedded `IO::PersistenceWorker` (Thread subclass) | EXEMPT hierarchy — unaffected |

**None of the 10 changes behaviour at the flip** (7 are exact-type or `final`, 3 are in the Thread hierarchy that
does not flip). Recommendation: qualify #1-3 (retail's call is direct). If the exempt list ever loses
`System::Thread`, #8-10 become qualify-or-verify rows.

### POLY-QUALIFIED (5) — the `companion_data.cpp` rule, and why it is the one place qualification is load-bearing

`dx11/src/buffers.cpp:637` (`VertexBufferData`), `:723` (`IndexBufferData`), `:847` (`ConstantBufferData`),
`dx11/src/devicedata.cpp:1143` (`DeviceData`), `dx11/src/smallclasses.cpp:142` (`RenderBufferData`):

```cpp
reinterpret_cast<Video::VertexBufferData*>(self)
    ->Video::VertexBufferData::~VertexBufferData();   // B.3.5 bridge   (inside dx11's slot-0 body)
```

The reason (`runtime/types/video/CompanionData.h:166-193`, RULE B5-3/B5-8; NEXT_SESSION cites it as
"companion_data.cpp:218", the text now lives in the header): the dx11 concrete companions are NOT C++ subclasses —
dx11 stamps its OWN hand table (`Dx11LiveVtable`, indexed by RETAIL slot) on the object, and its slot 0 IS the
function making this call. An unqualified `->~XData()` after the flip would dispatch through that hand table's
slot 0 (or slot 1 for the deleting variant) = re-enter this very body (infinite recursion) or call `Create`.
**These 5 must stay qualified** — and they are the proof that any post-flip dtor call on a `*Data` object must be
qualified or must not happen at all. (Whether the twelve `*Data` classes flip at all is §3b Q2; see the
hazards at the end.)

---------------------------------------------------------------------------------------------------------------------

## Part 2 — `DeletingDtor(...)` calls, `Object::Destroy`, other flag values

### Counts (product tree)

| call | count | receiver | virtual? |
|---|---:|---|---|
| `DeletingDtor(0)` / `(0u)` implicit-this, in `Destruct()` | 172 | this | VIRTUAL (unqualified) |
| `DeletingDtor(0)` implicit-this, in `Reconstruct()` | 171 | this | VIRTUAL |
| `X::DeletingDtor(0)` qualified, on another object | 20 | stack-built IO streams / `PackageFile` | non-virtual |
| `->DeletingDtor(0)` unqualified, on another object | 4 | stack-built `UI::Dialog` temporaries | virtual |
| `->DeletingDtor(1)` / `(1u)` | 24 (partc's regex misses the 3 `1u`) | heap objects | virtual |
| `Object::Destroy(p)` | 2 (+ the helper `TypeInfo.h:372`) | `AchievementInstance` / `GoalInstance` | virtual |
| any other argument | **0** | — | — |

Destruct per area: Component 83, ui 34, Game 13, Tutorial 12, graphics 8, video 5, Dialog 4, Goal 4, audio 2,
collision2d 2, achievement/io/object/pathfinding/picking 1 each. Reconstruct: the same with Tutorial 15 and Dialog 0
(Dialog's 4 are the `this->~T()` rows of Part 1).

### Destruct / Reconstruct — the pattern and the exact post-flip spelling

`code/Game/Component/firedescription.cpp:78-84`:
```cpp
// [6] 0x140005690: retail destroys and rebuilds in place: a virtual DeletingDtor(0),
// then the ctor again on `this` (D9 #10).
Object::External* FireDescription::Reconstruct() {
    DeletingDtor(0);
    return new (static_cast<void*>(this)) FireDescription();
}
// [7] 0x140001640: retail's shared body, a virtual DeletingDtor(0) (D9 #10).
void FireDescription::Destruct() { DeletingDtor(0); }
```
`runtime/src/ui/buttondescription.cpp:72-76`:
```cpp
Object::External* ButtonDescription::Reconstruct() {
    DeletingDtor(0);                                          // 180047fbe
    return ::new (this) ButtonDescription();                  // 180047fc8 JMP
}
void ButtonDescription::Destruct() { DeletingDtor(0); }
```
Semantics today: `DeletingDtor(0)` is an UNQUALIFIED VIRTUAL call → the most-derived `DeletingDtor` → the
most-derived `~T()` (compiler-chained to the bases), no free. `Destruct` leaves raw storage the caller (the External
/ factory machinery) frees; `Reconstruct` immediately placement-news the object again on the same storage and
returns the NEW-EXPRESSION's pointer.

**Post-flip spelling: `this->~T();` — UNQUALIFIED.** With `virtual ~T()` that is the same virtual dispatch
`DeletingDtor(0)` does today. It must NOT be qualified (`this->T::~T()`): retail's slot 7 body 0x140001640 is ONE
body shared by 119 exe tables precisely because it dispatches, and some tables inherit it:
`TutorialImageHotKey`, `TutorialImageInputKey`, `TutorialImageToolbarIcon` inherit
`TutorialImage::Destruct() { DeletingDtor(0); }` (`code/Game/Tutorial/tutorialimage.cpp:79`) — a qualified spelling
would skip the subclass destructor. (`GeneratedImageBuffer` also inherits `ImageBuffer::Destruct/Reconstruct`,
but is abstract.) The 4 `final` Dialog configs may keep `this->~T()` as they are.

Hazard after the call (unchanged by the flip, noted because the object is polymorphic): between `this->~T()` and
the placement new the object is dead and its vptr points at the ROOT's table (the dtor chain restamps it) — nothing
between the two statements touches `this`, in all 171 Reconstructs (the classifier shows every one is the two-line
shape). One irregular: `Video::ImageBuffer::Reconstruct` (`runtime/src/video/imagebuffer_vtable.cpp:345-349`) is
```cpp
    CallSlot0(this, 0u);
    ::new (this) ImageBuffer();
    return this;            // returns the old pointer, not the new-expression's
```
and reaches slot 0 through a RAW vt[0] read (`CallSlot0`, `:195-198`) — after the flip `vt[0]` is the Itanium
COMPLETE-object dtor, which for flag 0 happens to do the right thing, but the spelling must become `this->~ImageBuffer()`
(unqualified: `ReflectionImageBuffer`/`FrameCopyImageBuffer` override, `GeneratedImageBuffer` inherits).
Same for `ImageBuffer::Destruct` (`:281`).

### `X::DeletingDtor(0)` qualified on another object (20) and unqualified on stack temporaries (4)

All 24 destroy an object placement-new'd on **stack storage** (`alignas(16) uint8_t buf[sizeof(T)]`) a few lines
earlier; `placement.py` finds 19 such STACK constructions of flip classes, and every one is destroyed with flag 0,
**never flag 1** (so no existing stack-object-freed-by-MemFree bug):

- IO streams (qualified): `code/Game/Game/gameupdateloop_serialize.cpp:900/923/928` (`CompressedMemoryStream`),
  `runtime/src/audio/vtbatch7b_stream_run_win.cpp:522`, `runtime/src/io/closure_batch2b_win.cpp:252` (`FileStream`),
  `runtime/src/io/modification_refresh.cpp:1023/1029/1178` (`PackageFile`), `package_file_read_win.cpp:811`,
  `saveload_loadmeta.cpp:421`, `saveload_meta.cpp:573` (`CompressedMemoryStream`),
  `persistence_saveinfo_job.cpp:386`, `persistence_worker_win.cpp:699`, `video/save_image_to_file.cpp:391`
  (`BinaryFileStream`), `object/external_add.cpp:289` (`VisitorStream`), `object/external_load.cpp:449`
  (`CompressedFileStream`), `object/external_savetext_win.cpp:331` (`TextFileStream`),
  `object/internal_class.cpp:169`, `ui/dialog_settings.cpp:326`, `ui/userinterface.cpp:1548` (`MemoryStream`).
- `UI::Dialog` temporaries (unqualified, virtual, exact type `Dialog`): `runtime/src/ui/comboelement.cpp:208`,
  `dialog.cpp:2017`, `dialog_create.cpp:684`, `tabelement_addtab.cpp:166`.

**Post-flip spelling: `p->T::~T();` (qualified, keeps retail's direct call; dynamic type is exact so unqualified
would also be correct). NEVER `delete p`** — the storage is a stack array.

### `DeletingDtor(1)` (24) and `Object::Destroy` (2) → `delete p`

`code/Game/Game/gameupdateloop.cpp:2498`, `menuscene.cpp:145` (`WorldGeometry`), `w2_takescreenshot.cpp:294`
(`FrameCopyImageBuffer`), `runtime/src/engine/mainloop_run.cpp:230/330` (`UpdateLoop`),
`runtime/src/graphics/imagebuffer_derived.cpp:354/468` (`_depth`), `ui/userinterface.cpp:1051` (`DialogInstance`),
`ui/userinterface_frame.cpp:526`, `ui/userinterface_hover.cpp:266`, `ui/widgets_layout.cpp:264/606` (`UI::Element`),
`video/context_lifecycle.cpp:293` (`ConstantBufferData`), `:484` (`ContextData`), `:497/:499` (`ImageBuffer`),
`video/device_startup.cpp:611/709/797`, `video/enumerate_video_modes.cpp:160`, `video/video_resources.cpp:525`
(`DeviceData`), `video/renderstate.cpp:209` (`RenderStateData`), `runtime/types/object/TypeInfo.h:372`
(`Object::Destroy`), `standalone/menudialogloopt_stubs.cpp:448`; `Object::Destroy` at
`runtime/src/achievement/achievement_state.cpp:153` and `batchc_goallist_dtor.cpp:67` (receiver's dynamic type is an
EXE subclass — `TimeGoalInstance`, `FinishTutorialInstance` — which is why it must stay virtual → `delete inst`).
**FLAG: the 8 on `Video::*Data` companions (`context_lifecycle.cpp:293/484`, `device_startup.cpp:611/709/797`,
`enumerate_video_modes.cpp:160`, `video_resources.cpp:525`, `renderstate.cpp:209`) dispatch into dx11's
RETAIL-indexed hand table (or the Dx9 placeholder's, below) — they cannot become `delete p` unless the `*Data`
classes stay exempt (then they stay `DeletingDtor(1)`) or the seam re-indexes (§3b Q2).**

### The `flags & 2` question — DISPROVED: no caller can set bit 1

Every path that reaches a DeletingDtor / slot 0 with a flag, in the product tree:
1. `->DeletingDtor(<literal>)`: 367 × `0/0u`, 24 × `1/1u`; **0 non-literal** (`classify.py dd`; also
   `codegrep '\w*DeletingDtor\w*\s*\(\s*[^,()]+,\s*[A-Za-z_]…\)'` = 0 lines).
2. Raw slot-0 dispatches: partc's 45 non-test `vt[0]` rows plus 47 more spellings partc does NOT count
   (`rw_vtbl::kDtor`, `kRwDeletingDtor`, `DeletingDtorOf`, `ReadVtableSlot`, `VSlot(x,0)`, `kEncScalarDtor`,
   `*reinterpret_cast<DeletingDtorFn*>(*…)` — full list: `extra_vt0.txt` in the scratch dir, e.g.
   `runtime/src/save/saveload_draw.cpp:814-817`, `runtime/src/ui/dialog.cpp:1306/1315`,
   `runtime/src/video/buffers.cpp:266`, `runtime/src/graphics/sunlight.cpp:272`, 20+ `rvt[rw_vtbl::kDtor/8]` in
   runtime/src/ui). **Every one passes literal `1`/`1u`**; the non-dtor slot 0s (`statemachine.cpp:55`,
   `thread_win.cpp:333`, `graphicsmesh_vtable.cpp:486/488`, `font.cpp:410`) pass no flag.
3. Forwarders taking a flag variable — all closed:
   - `Video::(anon)::CallSlot0(self, flags)` `runtime/src/video/imagebuffer_vtable.cpp:195` — callers `:281`, `:346`,
     both `0u`.
   - `rtdesc::CallSlot0(self, flag)` `runtime/src/ui/description_common.h:104` — **zero callers** (dead).
   - dx11 `*_ScalarDeletingDtor(self, flags)` → `Dx11OriginalSlot(v, 0)(self, flags)` fallbacks
     (`dx11/src/buffers.cpp:624/713/856`, `devicedata.cpp:1125`, `imagebufferdata.cpp:434`): `Dx11OriginalSlot`
     (`dx11/funcs/dx11_seam.cpp:101`) ALWAYS returns null ("the shipped DLL is not loaded") — dead, and the flags it
     would forward come from the Runtime's `DeletingDtor(1)` above.
   - `Dx9P_DeletingDtor(self, int flag)` (`dx11/src/devicedata.cpp:1934`) — reached only via the Runtime's
     `DeviceData->DeletingDtor(1)`.
   - `RtEngine_Application_ScalarDeletingDtor(void*)`, `RtInput_KeyMap_DeletingDtor(void*)` — one-argument doors,
     no flag.
4. Registered doors: `RegisterRuntimeAddr/Export` rows are consulted only by OUR `ResolveAddr`/`ResolveExport`
   call sites (the exe loads no retail binary — `CLAUDE.md`, `reference_resolveaddr_consults_port_registry_first`);
   no product call site calls a resolved slot-0/DeletingDtor address with a variable flag (item 1-3 cover every
   call form found). With no retail code loaded, **nobody else can call a door**.
5. `delete[]` / `new T[n]` of a polymorphic class: **zero** in product code.

Verdict: bit 1 is never set; the 127 vector arms are dead code, and B8-2's "none" still holds. (Tests excluded —
NOT VERIFIED there.)

### Return value

**No caller uses the `T*` returned by `DeletingDtor` or a slot-0 dispatch.** Every call is an expression statement
(several spell it `(void)…->DeletingDtor(1)`); `Destruct` discards it; `Reconstruct` returns the placement-new
result, not the DeletingDtor result. Only the dead `Dx11OriginalSlot` fallbacks `return` the forwarded value.

---------------------------------------------------------------------------------------------------------------------

## Part 3 — deletability

### `delete` expressions already in product code
**One**: `dx11/src/devicedata.cpp:1935` `if (flag & 1) delete self;` — `Dx9Placeholder`, a POD `{vptr, device}`
built with global `new` at `:1960` and wearing a hand table whose slot 0 is `Dx9P_DeletingDtor` and slot 1
`Dx9P_EnumerateInterfaces`. Consistent today (global new/global delete), but see hazard H2.
No `new T(...)` (non-placement) of any polymorphic class exists in product code; every polymorphic object is
placement-constructed.

### The allocator pairs (all converge on ONE arena)
The exe/runtime has **no global `operator new`/`operator delete` replacement** (`codegrep` for a global definition:
0). `System::MemoryAllocator` is a custom VirtualAlloc arena (`runtime/src/system/allocator.cpp`, exports registered
at `:1278-1283`). Every polymorphic heap object is allocated from it and every DeletingDtor frees into it:

| allocate (placement sites, heuristic count) | free in DeletingDtor (bodies) | resolves to |
|---|---|---|
| `Object::detail::InternalMemAlloc` (~780) | `Object::detail::InternalMemFree` (435) | `?MemAlloc/?MemFree@MemoryAllocator` export → `rtalloc::MemAlloc/MemFree` (`internal_class.cpp:133-139`) |
| exe `fn_MemAlloc(g_pMemAllocSingleton,…)` (~80, incl. `WA_Alloc`/`AllocWork` wrappers) | `fn_MemFreeStatic` (5) | `MemoryAllocator::Allocate` / `?MemFree@MemoryAllocator` (`engine_funcs.cpp:635`) |
| `TypeTreeAlloc` | `TypeTreeFree` (4: `Object::Base`, …) | same exports (`type_reflection.cpp:74`) |
| `RtSystem::RtMemAlloc`, `rtdesc/rtcoll::MemAlloc`, `Video::ImageBuffer::operator new` (`context_ctor.cpp:289`) | `RtSystem::RtMemFree` (10) | same exports (`string.cpp:102`) |
| dx11 companions: `g_deps.MemAlloc` | dx11 slot 0 `g_deps.MemFree` / `im->MemFree` | Runtime MemoryAllocator (via dx11 imports) |
| `Dx9Placeholder`: global `new` | global `delete` | **CRT heap** — the one exception |

(The wrapper-to-export equivalence was checked for each named free function and sampled for the exe alloc wrappers;
not every one of the ~40 caller-supplied-storage factories was traced — NOT VERIFIED individually.)

### FLAGGED — what `delete p` needs and does not have today

461 class names sit in a DeletingDtor hierarchy (roots: `Object::Base` 414, `Graphics::Geometry` 13, `IO::Stream` 9,
`Engine::UpdateLoop` 9, `IO::ReaderWriter` 4, and the twelve `Video::*Data` 1 each).

1. **No usable class `operator delete` anywhere.** 396 of the 461 have NO class-scoped `operator delete` in
   themselves or any ancestor; the other 65 inherit/declare one that is **declared but never defined**
   (`static void operator delete(void* p);` in `ImageBuffer.h:431` "NOT PORTED", `StringTable.h:120`,
   `DialogInstance.h:487`, `Dialog.h:380`, `Goal.h:301`, `AchievementManager.h:119/211`, `SoundEffect.h:142`,
   `SoundBuffer.h:143`, `GraphicsMesh.h:447`, `Material.h:336/460`, `Mesh.h:963`, `Particle.h:672-674`,
   `PointList.h:145`, `PickingMesh.h:210`, the UI headers listed by
   `codegrep 'operator\s+(new|delete)\s*\('`, 40 declaring lines). The only DEFINED class operator delete in the
   tree is `Video::RenderBuffer::operator delete` (`context_lifecycle.cpp:315`) — a non-polymorphic class. The
   `??3…` "operator delete" rows of `class_operator_delete.cpp` / `ui_operator_delete.cpp` are `extern "C"` door
   functions, not class members.
   Consequences at the flip: (a) for the 396, `delete p` → the deleting dtor calls GLOBAL `::operator delete` =
   CRT `free()` on MemoryAllocator arena storage — heap corruption, silently; (b) for the 65, a `virtual ~T()`
   ODR-uses the in-class `operator delete` → **link error** (undefined reference), and the in-class declaration also
   HIDES any root-level one added later. **Required:** one DEFINED `static void operator delete(void*)` →
   `MemoryAllocator::MemFree` on each hierarchy root (`Object::Base`, `Graphics::Geometry`, `IO::Stream`,
   `IO::ReaderWriter`, `Engine::UpdateLoop`), and the 40 undefined in-class declarations either defined (same
   body — retail's `??3` bodies are all `MemFree`, `class_operator_delete.cpp` banner) or deleted.
2. **Destructor access.** 447 of the 461 declare the dtor `protected`; 14 `public` (the twelve `*Data`,
   `Graphics::MeshInstance`, `StatusIconGeometry`). `delete p` on a protected-dtor static type does not compile
   outside the class: every `DeletingDtor(1)` / `Object::Destroy` receiver type above (`Object::Base`,
   `UI::Element`, `UI::DialogInstance`, `Engine::UpdateLoop`, `WorldGeometry`, `FrameCopyImageBuffer`,
   `Video::ImageBuffer`, `AchievementInstance`, `GoalInstance`, `Video::*Data`) needs a public virtual dtor (or a
   member/friend deleter). Simplest rule: the flipped `virtual ~T()` is public on every hierarchy root.
3. **Objects that must NEVER be `delete`d** (constructed on storage not from the allocator): the 19 STACK sites
   (Part 2 list: IO streams, `PackageFile`, `UI::Dialog` temporaries) and the embedded members
   (`ModelComponent::meshInstance` `modelcomponent.cpp:231`, `ShowTool`/`action_base_complete` MeshInstances at
   `e + 0xb8`, `tool_base_render.cpp:513` `EdgeMeshInstance` at `self + 0x28`, the StorageDisplay MeshInstance array,
   `IO::Persistence::_workerVtable` — Thread hierarchy, exempt). **None of the 19 stack objects, and none of
   `ModelComponent`/`StorageDisplay`'s MeshInstances, is destroyed with flag 1 today** (flag 0 or a direct dtor) — no
   latent bug; the destruction of the two `e + 0xb8` action MeshInstances and the `self + 0x28` EdgeMeshInstance
   was NOT VERIFIED (not in any flag-1 site's receiver list, which is the check that matters). The cutover must map
   all of them to `p->T::~T()`, never `delete`.

---------------------------------------------------------------------------------------------------------------------

## Hazards the cutover owner must carry (beyond Q3/Q4)

- **H1 — the `Video::*Data` companions.** Runtime code dispatches on them by COMPILER slot
  (`->DeletingDtor(1)` ×8, `video/buffers.cpp:266` raw `slot0(companion, 1)`), but the live object wears dx11's
  RETAIL-indexed table. After a flip, `delete companion` reads Itanium slot 1 (D0) = dx11's retail slot 1
  (`Create` / `Lock` / …). Either the twelve `*Data` classes are exempt (their 8 `DeletingDtor(1)` calls and the 5
  dx11 qualified dtor calls stay as they are) or the seam re-indexes — decide in Q2 before any VID block.
- **H2 — `Dx9Placeholder`** (`dx11/src/devicedata.cpp:1932-1961`) masquerades as a `Video::DeviceData` with a
  4-slot hand table and lives on the CRT heap. The Runtime's `DeviceData->DeletingDtor(1)` sites reach it. It rides
  on H1's decision; if `DeviceData` ever flips, a root-level `operator delete` → MemFree would also be the wrong
  allocator for it.
- **H3 — partc under-counts slot-0 dispatch.** 47 extra raw slot-0 delete dispatches (listed in `extra_vt0.txt`;
  mostly `IO::ReaderWriter` deletes via `rw_vtbl::kDtor`, `UI::Dialog` via `DeletingDtorOf`, and `saveload_draw`'s
  four) are not in partc's 56. After the flip `vt[0](p, 1)` runs the COMPLETE dtor and ignores the flag (a leak).
  Feed them to the §3b Q1 owner.
- **H4 — keep `Destruct`/`Reconstruct` virtual** (`this->~T()`, unqualified); qualify only the stack/embedded
  destructions and the direct-call retail sites (#1-3 above).
