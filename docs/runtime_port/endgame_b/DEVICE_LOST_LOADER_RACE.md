# The device-lost / loader-thread race — investigation, retail lock map, open question

Owner: the loader defect (`B34_BRIEFS` §D / `[[project_menu_heap_crash]]` family), **not** B.3.5
step 5. Investigated 2026-09-19 on branch `b35-DL` off main `9352dacc` (which contains
`Merge branch 'b35-J'`).

**Verdict in one line.** The crash is **PRE-EXISTING**, not a step-5 regression: the pre-step
baseline `bd483075` crashes on the same route **more** often than current main (5/11 vs 1/11) and
with the same stack families, three of which it produced unprompted. And
**Q2 does not yield a retail-backed locking fix**: retail's `Video::Device` lock map is
reproduced in our port *site for site*, and retail leaves **exactly the same window** — the one
in which `DeviceData::d3dDevice` is `nullptr` — with the Device critical section **released**.
Nothing is fixed here. This document records the evidence so the next pass does not re-derive it.

---

## 1. The defect as reproduced

Route: `BANISHED_AUTOSTART=1 BANISHED_PAUSEMENU=600 BANISHED_PAUSEMENU_QUIT=120
BANISHED_FORCE_DEVICE_LOST=400`. The injection fires on Present #400, which on this machine
lands **while the `Engine::Loader` thread is still building the world** on the new-game route.
With the injection moved past the world build (`=2000`) the route is clean.

Three distinct faulting stacks, all on the loader thread, all inside the recursive
`Object::External::Load` chain, all while the main thread is between
`[DeviceLost] … onLost -> calling` and `[DeviceLost] recreate OK`:

| Family | Top frame | Fault | What is null |
|---|---|---|---|
| **A** | `VideoDX11::GpuBuffer_Create +0xe0` (the `ActiveDevice()->CreateBuffer` call) | READ of `0x0` | `Device::_singleton->activeDeviceData->d3dDevice` |
| **B** | `Graphics::MaterialInstance::BuildParameters +0x3c6` | READ of `0x2c` (`RDI=0`) | a resource pointer inside a material being loaded |
| **C** | `rtcoll::ArrayAssignCore +0x3c` ← `Graphics::MeshInstance::SetMesh` ← `BuildSkyMesh_Step28` | READ of `0x8` (`RCX=0`) | `GraphicsMesh+0x70` (`_materialLists`) of `Models/Sky/SkyMesh.rsc` |
| **D** | `Object::Factory::IsDerivedFrom +0x46` ← `Object::Base::IsDerivedFrom` ← `ExternalPtrSerializeBody` | READ of `0x8` (`RDI=0`) | a near-null `Object::Factory*` (commit `511bd7b1`'s stack) |

Family **A** is the only one that names a device pointer directly. **B**, **C** and **D** are all
"a field inside an object that `ExternalFactory::FindOrLoad` just returned is null" — the shape
`B34B_RULES.md` §6.3 records as the allocator/loader family. In every captured dump the crash
handler's `MemoryAllocator::_singleton cell=… value=…` line shows a **live, plausible**
allocator and `[REDZONE-DUMP] no tag corruption … free-list forward links also intact`, so §6.3's
W2 (post-`Destruct` null singleton) and W2′ (corrupted free list) are both **excluded by the dump**
for these four.

Ordering evidence that the window is the overlap, from `q1_base_1`'s `standalone_error.log`
(stderr, unbuffered): `[DeviceLost] hr=0x887a0007 onLost -> calling` is followed by the loader
thread's entire world-build log (`[GUL::DiseaseList]`, `[GUL::ProfList]`, `[GUL::UIMat]`,
`[GUL::Help]`, `[GUL::Weather]`) and only *then* by `[DeviceLost] recreate OK`. The main thread
spends the whole D3D teardown+recreate inside that window, and the loader runs right through it.

---

## 2. Q1 — is it a regression of step 5?  **NO.**

Second worktree at the pre-step commit, built and run alongside the current tree, alternating
exe by exe under `tools/game_dir_lock.sh`, one run per acquire, foreground:

* baseline `bd483075`, md5 `9c0dacc573dba7a93281c2caba7a7c5d`
* current main `9352dacc` (branch `b35-DL`), md5 `57b92bc66ed9847333754434ae6fc366`

Eleven runs each, `BANISHED_FORCE_DEVICE_LOST=400`, alternating base/cur so machine load is
shared. `calls=` is `[ExtLoad] calls=`; it is not the plain route's 5686 because this route
reloads through the recovery.

| # | baseline `bd483075` | current main `9352dacc` |
|---|---|---|
| 1 | **exit 3** · family **D** `Object::Factory::IsDerivedFrom +0x46`, READ `0x8` | **exit 3** · family **A** `GpuBuffer_Create +0xe0`, READ `0x0` |
| 2 | **exit 3** · family **C** `rtcoll::ArrayAssignCore +0x3c`, READ `0x8` | exit 0 · calls=6595 · round trip |
| 3 | exit 0 · calls=6595 · round trip | exit 0 · calls=6601 · round trip |
| 4 | exit 0 · calls=6598 · round trip | exit 0 · calls=6601 · round trip |
| 5 | **exit 3** · family **A** `GpuBuffer_Create +0xdd`, READ `0x0` | exit 0 · calls=6600 · round trip |
| 6 | **exit 3** · family **C** `rtcoll::ArrayAssignCore +0x3c`, READ `0x8` | exit 0 · calls=6600 · round trip |
| 7 | exit 0 · calls=6601 · round trip | exit 0 · calls=6595 · round trip |
| 8 | **exit 3** · family **C** `rtcoll::ArrayAssignCore +0x3c`, READ `0x8` | exit 0 · calls=6598 · round trip |
| 9 | exit 0 · calls=6616 · round trip | exit 0 · calls=6681 · round trip |
| 10 | exit 0 · calls=6681 · round trip | exit 0 · calls=6681 · round trip |
| 11 | exit 0 · calls=6681 · round trip | exit 0 · calls=6601 · round trip |
| **crashed** | **5 / 11** (45%) | **1 / 11** (9%) |

Every clean run on both exes shows the full round trip (`ARMED` → `INJECTING
DXGI_ERROR_DEVICE_RESET` → `hr=0x887a0007 onLost -> calling` → `recreate OK; onRestored ->
calling` → `device-lost round trip COMPLETE`), `[RtResolve] 0 misses`, no hang log, and a
zero-byte crash log. Every crashed run died on the **loader** thread
(`System::Thread::ThreadFunc` → `RtEngine_Loader_Run` at the base of the stack, or the recursive
`External::Load` chain), never on the main thread, and the main thread's half of the recovery
completed in each case.

Controls — every one clean, so the crash is specific to the *overlap*, not to the device-lost
path and not to the loader:

| Control | exe | Result |
|---|---|---|
| `BANISHED_FORCE_DEVICE_LOST=2000` (injection past the world build) | current main | **3 / 3 exit 0**, calls=6681, full round trip, empty crash log |
| plain new game (no injection) | current main | **3 / 3 exit 0**, calls=**5686**, `[RtResolve] 0 misses`, empty crash log |
| plain new game (no injection) | baseline `bd483075` | **3 / 3 exit 0**, calls=**5686**, `[RtResolve] 0 misses`, empty crash log |

That is the discriminator stated properly: the same binary, the same loader thread doing the same
world build, and the same device-lost recovery — but with the recovery moved **out** of the build
window it is clean. **The window is the overlap.**

**Conclusion.** Both exes crash on this route, and the **baseline crashes more often** — 5/11
against current main's 1/11 — producing three of the four families unprompted, including family
**D**, which is commit `511bd7b1`'s stack, and family **A**, which is `c0077b00`'s `crash_2`.
**The defect predates B.3.5 step 5.** Block J's thirty seam→virtual conversions are not
implicated, and neither is any other step-5 commit. The 5-vs-1 gap is on eleven runs of a race
and should not be read as step 5 having *improved* anything; the honest reading is "same defect,
same families, rate indistinguishable at this sample size, certainly not introduced by step 5".

---

## 3. Q2 — what does retail do?

### 3.1 The complete `Video::Device::Lock`/`Unlock` map in `VideoDX11-x64.dll`

`?Lock@Device@Video@@QEAAXXZ` is imported at IAT `0x180013408`, `?Unlock@…` at `0x180013410`
(identified by the two `.idata` name strings at `0x18001c000` / `0x18001bfe0` and confirmed by
the receiver: `ContextData::Finish`'s `MOV RCX,RBX` passes the `Device` whose `+0x100` is the
`DeviceData`). Xrefs to those two slots are **exhaustive — seven call sites, no more**:

| Retail fn | Lock | Unlock | Our port | Faithful? |
|---|---|---|---|---|
| `ContextData::BeginCommands` `0x180001650` | `0x1800016bd` | — (see below) | `dx11/src/contextdata.cpp:276` | yes |
| `ContextData::Finish` `0x1800017c0` | — | `0x180001805` | `dx11/src/contextdata.cpp:508` | yes |
| `IndexBufferData::SetData` `0x1800031c0` | `0x1800031fa` | `0x180003265` | `GpuBuffer_SetData`, `buffers.cpp:594/609` | yes |
| `IndexBufferData::Serialize` `0x1800033b0` | `0x180003495` | `0x180003560` | `GpuBuffer_Serialize`, `buffers.cpp:549/570` | yes |
| `VertexBufferData::SetData` `0x180003c60` | `0x180003c9a` | `0x180003d05` | same shared body | yes |
| `VertexBufferData::Serialize` `0x180003d90` | `0x180003e7d` | `0x180003f40` | same shared body | yes |
| `DeviceData::SetImageBuffer` `0x180005450` | `0x1800054ab` | `0x1800054cf` | `devicedata.cpp:1493-1495` | yes |

Two facts from that table matter.

**(i) The lock spans the whole frame and is dropped BEFORE the recovery.**
`BeginCommands` takes it and never releases it; the matching release is
`ContextData::Finish`'s `MOV RCX,RBX / CALL [0x180013410]` at **`0x180001800`-`0x180001805`** —
which is **immediately after `Present` (`CALL [R9+0x40]` at `0x1800017fc`) and BEFORE the first
HRESULT test (`TEST EDI,EDI` at `0x18000180b`)**. Everything from there on — the two DXGI
comparisons, `GetDeviceRemovedReason`, the `onLost` notify, `Video::Device::ReleaseDynamic`, the
`MOV qword ptr [RSI+0x20],0x0` that nulls `d3dDevice` at **`0x180001857`**, `DeviceData` slot 3
(shutdown, `CALL [RAX+0x18]` at `0x18000185f`) and slot 2 (create, `CALL [RAX+0x10]` at
`0x180001868`), `RecreateDynamic`, the `onRestored` tail-jump — **runs with the Device critical
section RELEASED**. Our `ContextData_Finish` is byte-faithful in exactly this respect: `Present`,
then `DevOf(device)->Unlock()`, then the tests.

**(ii) `Create` does not take the lock, and neither does the LOAD arm of `Serialize`.**
`IndexBufferData::Create` (`0x1800030f0`) and `VertexBufferData::Create` (`0x180003b90`) appear
in no row above: retail creates GPU buffers **unlocked**, walking
`Device::_singleton -> +0x100 -> +0x20` for the `ID3D11Device` with no protection. And retail's
`…BufferData::Serialize` takes the lock **only on the save arm**: at `0x180003426` the
`CMP dword ptr [RAX+0x8],0x0 / JNZ 0x18000346f` splits load from save, the load arm
(`0x180003428`-`0x18000346a`, `MemAlloc` → `SerializeBytes` → `CALL 0x1800030f0` → `MemFree`)
takes **nothing**, and the `Lock` at `0x180003495` sits *inside* the save arm. Our
`GpuBuffer_Serialize` reproduces that split exactly — the `VideoDeviceObject()->Lock()` is after
the `SerializerStreamMode(serializer) == 0` early return.

So family **A**'s null read is on a path that retail also walks with no lock.

### 3.2 The `Runtime-x64.dll` side

`?Lock@Device@Video@@` (`0x18000ca90`) has **no internal callers** in `Runtime-x64.dll` — only the
export table. Every runtime-side acquisition is an **inline copy** of the singleton-reading
`EnterCriticalSection(Device+0x60); owner = GetCurrentThreadId(); depth++` shape. Reading the
xrefs to `?_singleton@Device@Video@@` (`0x1801fe7a8`) gives the set, and it is the set our port
already reproduces:

* `{Image,Index,Vertex,Render}Buffer::PlatformConstruct` — `0x1800f2f40` / `0x1800f3680` /
  `0x1800f57e0` / `0x1800f4320` (reads at `f2f57/f2f7b/f2fda`, `f3697/f36bb/f371a`,
  `f57f7/f581b/f587a`, `f4337/f435e/f43bd`), and the four `PlatformDestruct` twins
  (`f304a/f30c9`, `f3795/f3811`, `f58f5/f5971`, `f442a/f44a9`).
  → `runtime/src/video/buffers.cpp:598-624` (`PlatformConstructBody` / `PlatformDestructBody`),
  which holds the lock **only around the intrusive list splice**, exactly as retail does — the
  companion object is created *outside* it.
* the eight `{Image,Index,Vertex,Render}Buffer::{Release,Recreate}Dynamic` statics
  (`0x1800f2d90/2e80`, `0x1800f3500/35d0`, `0x1800f5660/5730`, `0x1800f4030/40c0`; singleton
  reads at `f2d9f/f2de0/f2e42`, `f2e94/f2eff`, `f350f/f3547/f358d`, `f35e4/f3645`,
  `f403f/f4085`, `f40d4/f4129`, `f566f/f56a7/f56ed`, `f5744/f57a5`) and
  `RenderBuffer::Recreate` (`0x1800f4170`).
  → `runtime/src/video/dynamic_resources.cpp`, one `RtVideoDeviceLock`/`Unlock` pair each.
* `RenderBuffer::PlatformDestruct`'s pair again from the `Video::Context` dtor
  → `runtime/src/video/context_lifecycle.cpp:68`.

**Paths that read the Device and take NO lock in retail** (checked instruction by instruction, so
that a future pass does not "add the missing lock" here):

* `Video::RenderBuffer::Create` `0x1800f3f10` — five singleton reads (`f3f2b`, `f3f4c`, `f3f5e`,
  `f3fb1`, `f3fd0`), all plain `MOV EDX,[RAX+4]` / `[RAX+8]` width-height reads. No CS.
* `Video::ShaderProgram::PlatformSerialize` `0x1800f52b0` — three singleton reads (`f5317`,
  `f5406`, `f5509`), each a `CMP dword ptr [RAX+0xf8], {0,1,2}` backend-kind test that selects
  which shader variant to deserialise. No CS. This is the **programs** load path.
* the `RenderState` / `VertexLayout` / `VertexProgram` / `PixelProgram` / `VideoProgram`
  constructors (`0x1800f4bf1`, `0x1800f5db4`, `0x1800f6176`, `0x1800f39c6`, `0x1800f460a`) —
  one singleton read each, to reach `DeviceData` and call its companion-factory slot. No CS.

### 3.3 Can retail lose the device while the loader runs, and what serialises it?

It can, and **nothing serialises it.**

* Retail's loader thread is `Engine::Loader` (`?_loader@LoaderLoop@Engine@@1VLoader@2@A`,
  `0x1801fe7b8`), and it is the thread that runs the recursive `External::Load` chain — the same
  architecture our `standalone/loader_loop_stubs.cpp` reproduces.
* Retail's main thread keeps presenting throughout: `?Draw@LoaderLoop@Engine@@UEAAXM@Z`
  (`0x1800dc310`) is *four* effective instructions — `MainLoop::_singleton->_context(+0x10)
  ->_contextData(+0x2160)->vtable[4](ctx, 7, 0, 0.0f)`, i.e. `ContextData::Clear` and nothing
  else. It draws a black clear; the surrounding `MainLoop` still runs
  `BeginCommands … EndCommands … Finish`, so **`Present` — the only place a lost device is ever
  detected — is reached on every frame of the world build.**
* `?OnVideoError@LoaderLoop@Engine@@UEAAXXZ` (`0x1800dc360`) is a **bare `RET`**: the loader loop
  has a device-lost hook and it does nothing. It does not park the loader.
* `?OnVideoError@MainLoop@Engine@@QEAAXXZ` (`0x1800dc5e0`), the `Device+0x50` notify, does **not**
  wait for or join the loader either. Its ~250 instructions build a `Collection::Array<int>` of
  ten resource type ids from ten `.data` globals (`0x1801fa548`, `0x1801fa698`, `0x1801fa580`,
  `0x1801fa5f0`, `0x1801fa660`, `0x1801fa5b8`, `0x1801f99b0`, `0x1801fa708`, `0x1801f9a90`,
  `0x1801f99e8`), walk the `ExternalFactory` AVL tree per id (`0x1800be9d0`), release each object
  through vtable `+0x38`, and destroy the `Video::Context`. The only lock it takes is the
  **`MemoryAllocator` critical section at `_singleton+0x640`**, and only inline around each
  `MemFree` (`0x1800dc70e`/`0x1800dc728`, `0x1800dc8e8`/`0x1800dc905`, and five more) — i.e. the
  ordinary allocator discipline, not a loader barrier. `0x180033880`, the one call it makes on
  `this` before the walk, is an `Array` clear, not a join.

**Therefore retail contains the same race.** A real alt-tab out of fullscreen during a world
build lets retail's `ContextData::Finish` null `d3dDevice` at `0x180001857` while retail's loader
thread is inside `IndexBufferData::Create` at `0x1800030f0` reading it — and retail's
`MainLoop::OnVideoError` additionally releases resources out of the very `ExternalFactory` tree
the loader is inserting into, with no lock over the tree. Retail presumably never sees it because
a genuine driver reset in the few seconds of a world build is vanishingly rare, whereas
`BANISHED_FORCE_DEVICE_LOST=400` hits that window **deterministically, every run.**

### 3.4 Is the injection itself unfaithful?

No — not in the locking sense, which is what was asked. `ForceDeviceLostThisFrame()`
(`dx11/src/contextdata.cpp:617-637`) substitutes only the **return value** of `Present`, at
`0x1800017fc`'s site, *after* the `Unlock` at `0x180001805` has already happened in both retail
and our port. It skips no lock the genuine path takes; every instruction of the recovery,
including the `d3dDevice = nullptr` store and the two `DeviceData` slots, then runs for real.

The one thing the injection **does** change is the *probability*: it fires on a frame chosen by
frame number, so it is guaranteed to land mid-load, which a driver reset is not. That makes it an
excellent instrument for this defect and a misleading one for "does device-lost work" — the two
questions want different injection points, which is why `=2000` is clean and `=400` is not.

### 3.5 What retail has that we do not (stated, but it is not the fix)

`?OnVideoError@MainLoop@Engine@@` and `?OnVideoRestart@MainLoop@Engine@@` (`0x1800dc3b0`) are
**declined ports** (`runtime/src/engine/engine.cpp:376-420`, with the sizing in
`docs/runtime_port/WAVE9_LANE_VID.md`); ours are thunks that log once and return. So retail's
device reset releases and rebuilds the engine-side video resources and the `Video::Context`, and
ours does not. That is a real divergence and it is already recorded — but it **cannot be the fix
for this defect**: it takes no lock the loader respects, and porting it would put *more*
concurrent work into the same window, not less.

---

## 4. The candidate window, stated precisely

One writer, no lock, two readers:

```
main thread, dx11/src/contextdata.cpp ContextData_Finish (retail 0x1800017c0)
    Present()                                        0x1800017fc
    Device::Unlock()                                 0x180001805   <-- lock released HERE
    onLost()                                         0x180001845
    Video::Device::ReleaseDynamic()                  0x18000184b   (takes the lock per class)
==> dd->d3dDevice = nullptr                          0x180001857   <-- the write
    DeviceData slot 3  Shutdown()                    0x18000185f
    DeviceData slot 2  CreateDevice()                0x180001868   (restores d3dDevice)
    Video::Device::RecreateDynamic()                 0x180001873   (takes the lock per class)
    onRestored()                                     0x180001895

loader thread, inside Object::External::Load
==> VideoDX11 GpuBuffer_Create   reads Device->activeDeviceData->d3dDevice   (retail 0x1800030f0 /
                                 0x180003b90 — NO lock in retail either)
==> Video::ShaderProgram::PlatformSerialize  reads Device+0xf8                (retail 0x1800f52b0 —
                                 NO lock in retail either)
```

The window is roughly the duration of a D3D11 device teardown + recreate, which the logs show is
long enough for the loader to get through several resource loads.

Any fix must either (a) keep the loader out of that window, or (b) make the window not exist —
and **retail does neither**, so neither can be justified as "restoring retail's locking".
Candidate shapes for a future owner, none of them retail-backed and none implemented here:

1. Hold the Device lock across the recovery (`Unlock` moved from after `Present` to the end of
   `Finish`). **Diverges from retail at `0x180001805`** and would make the loader block for the
   whole device recreate. It would also deadlock nothing (the lock is recursive and the recovery
   takes it re-entrantly in `Release`/`RecreateDynamic`), which is what makes it tempting.
2. Park the loader thread for the duration, inside the `onLost`/`onRestored` pair — i.e. give
   `MainLoop::OnVideoError` a loader join retail does not have.
3. Accept it as a retail bug, and make `BANISHED_FORCE_DEVICE_LOST` refuse to fire while a world
   build is in flight, on the grounds that a synthetic reset at a frame retail could not have
   picked is measuring a race retail also has. This narrows the instrument, not the defect, and
   must be written down as such if it is ever done.

**Nothing above is done.** Per `CLAUDE.md` no null guard was added at any of the four faulting
derefs: all four are faithful, and the upstream null is a real concurrent write.

---

## 5. What a recurrence should capture

The crash dump already prints `MemoryAllocator::_singleton` (`5b332342`) and the free-list state,
and for all four families it comes back healthy — so the next capture wants the **device** cell
instead: `Device::_singleton`, `Device->activeDeviceData` and `activeDeviceData->d3dDevice`, plus
the Device lock's `ownerThreadId` (+0x88) and `depth` (+0x8C). Those five words separate
"the loader read the null the main thread wrote" from "the loader read a stale `DeviceData`" and
would tell us, for families **B**, **C** and **D**, whether a failed GPU-resource create left a
half-built `External` in the factory tree. That is a `standalone/` crash-handler change, not a
Video/Graphics one.

Cross-references: `B35_RULES.md` §5 (5.1 the lock is retail and load-bearing, 5.2 what runs off
the main thread, 5.3 device-lost is dx11's), `B34B_RULES.md` §6.3 (the allocator `LockEnter`
family, W1/W2/W2′), `NEXT_SESSION.md` §1 item 6, commits `511bd7b1` and `c0077b00`.
