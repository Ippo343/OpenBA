# Wave 9 lane VID2 — Video::Context lifecycle, and where Device::Startup actually stops

Assigned from cutover boot attempt 14's 43-symbol residue
(`docs/runtime_port/wave9_attempt14_unresolved.txt`):

```
?Startup@Device@Video@@QEAA_NXZ
?Shutdown@Device@Video@@QEAAXXZ
??1Context@Video@@QEAA@XZ
?ClearResources@Context@Video@@QEAAXXZ
?OnVideoError@MainLoop@Engine@@QEAAXXZ
?OnVideoRestart@MainLoop@Engine@@QEAAXXZ
```

**Result: 2 of the 6 closed, 5 exports and 7 address doors ported, and the other
4 are blocked on exactly TWO named things instead of the five stale ones on
record.** The two blockers are measured, not estimated, and both were misstated
in the tree before this lane.

Branch `wave9/lane-vid2`, three commits on `29cfaa2`.

---

## 1. The headline is a correction, not a port

Lane VID classified `??1Context@Video@@` and `?ClearResources@Context@Video@@` as
**RESOLVE-ONLY** — "the resolve happens, nothing calls them; at cutover each is a
null function pointer that is never invoked — harmless." That verdict was reached
by grepping for `fn_VideoCtx_Dtor` / `fn_VideoCtx_ClearResources`, and for those
two names it is correct: both are dead resolves in `engine/funcs/engine_funcs.cpp`.

**`??1Context@Video@@` has a live caller anyway, and it runs on every clean exit.**

```cpp
// runtime/src/engine/engine.cpp:590  —  RtEngine_MainLoop_Dtor, a PORTED body
void* context = self->_context;
if (context) {
    reinterpret_cast<VoidThisFn>(
        ResolveAddr(Module::Runtime, 0x1800f16e0))(context);   // ~Context
    reinterpret_cast<VoidThisFn>(
        ResolveAddr(Module::Runtime, 0x180018500))(context);   // operator delete
}
```

The grep could not find it because **the call site mentions no name at any point**
— it is a bare hex literal. That is the same blind spot lane VID itself documented
twenty lines above this code for the two `Video::Device` +0x50/+0x58 stores, hit a
second time **in the same function**. A "RESOLVE-ONLY" verdict derived from a name
grep cannot see an address door, and this cluster has both kinds.

The integrator's address-door census reached the same cluster from the other side
and flagged `0x1800f1820` (ctor) and `0x180018500` (deleting operator) as
unregistered. Between the two findings the picture is complete: **the
`Video::Context` lifecycle reaches the DLL through three doors of two different
kinds, and at cutover all three are null.**

---

## 2. Ported — 5 exports, 7 address doors

New file `runtime/src/video/context_lifecycle.cpp`. Kill switch
`BANISHED_RT_VIDCTXLIFE=0`.

| Export | Retail | Ord | Why |
|---|---|---|---|
| `?ClearResources@Context@Video@@QEAAXXZ` | 0x1800f2090 | 1712 | assigned row |
| `??1Context@Video@@QEAA@XZ` | 0x1800f16e0 | 562 | assigned row; **live** via `RtEngine_MainLoop_Dtor` |
| `??1ConstantBuffer@Video@@QEAA@XZ` | 0x1800eeee0 | 558 | callee ×7 of the dtor |
| `?PlatformDestruct@RenderBuffer@Video@@AEAAXXZ` | 0x1800f4400 | 3369 | callee of the dtor |
| `??3RenderBuffer@Video@@SAXPEAX@Z` | 0x18001f360 | 995 | callee of the dtor |

All five mangled names were checked against the shipped export directory of
`C:\GOG Games\Banished\Runtime-x64.dll` with `pefile` **before** any body was
written (`reference_resolveexport_phantom_names.md`). `??1` for the destructors,
`QEAA` not `UEAA` for `Context`'s and `ConstantBuffer`'s
(`reference_phantom_dtor_mangling_qeaa_vs_ueaa.md`). `tools/export_name_audit.py`
is unchanged at 1 known placeholder.

**Address doors registered — seven, and each has a reader:**

| Address | What | Reader |
|---|---|---|
| 0x1800f2090 | `ClearResources` | symmetry with the name door |
| 0x1800f16e0 | `~Context` | `engine.cpp:590`, `engine_funcs.cpp:1623` fallback |
| 0x1800eeee0 | `~ConstantBuffer` | the dtor above |
| 0x1800f4400 | `RenderBuffer::PlatformDestruct` | the dtor above |
| 0x18001f360 | `??3RenderBuffer` | the dtor above |
| **0x180018500** | `Video::Context::operator delete` | `engine.cpp:592`, `engine_funcs.cpp:1625` — **one of the integrator's two rows** |
| **0x180006a10** | the buffers' `operator delete` | the dtor above; **unnamed in the image**, so a name registration alone would register something nobody calls |

Three unexported helpers are transcribed inline rather than registered, because
nothing outside this file reaches them: `0x18001cab0` (the `Array<string-pair>`
dtor `~ConstantBuffer` calls) and its element loop `0x18001cfc0`, and the
`System::String` release both carry.

### Faithfulness notes worth reading before trusting the bodies

* **The last-reference `String` release does NOT null the field.** Retail rewinds
  the field by 8 and frees, leaving it pointing at freed memory
  (`0x1800eef26`); the *shared* branch (refcount still non-zero) *does* null it
  (`0x1800eef36`). Both kept. Anything that "tidies" the first changes what the
  next reader sees.
* **`0x18001cfc0`'s spelling of that release re-reads the field after the rewind
  and skips the free when the result is null** — the ZF comes from the
  `ADD [RBX+8],-8`, not from a test. Written apart from the other spelling for
  that reason, even though the two agree on behaviour (the `0x1800c7ad0` form has
  its null test inside `MemFree`).
* **`ClearResources` rotates TWO SEPARATE MASKS.** `EAX` for the image slots and
  `EDX` for the vertex slots, both seeded from the same `1` at
  `0x1800f2090`/`0x1800f209f`, and `EDX` survives the first loop untouched. One
  shared mask would be wrong the moment either loop's length changed.
* **`RenderBuffer::PlatformDestruct` fixes the list head TWICE** — once before the
  unlink (`head == this ? this->next : head`, `0x1800f445d`) and once after
  (`head == this ? nullptr : head`, `0x1800f4494`). The second is what makes the
  last buffer leave an *empty* list rather than a self-referencing one. Its
  `if (prev && next)` guard is retail's and is load-bearing: a buffer that was
  never linked is skipped rather than unlinked through null.
* **The dtor's ORDER is not normalised.** The backend companion at +0x2160 goes
  first, so every buffer released afterwards is released against a backend that
  has already torn its own state down. The seven constant buffers go in reverse
  declaration order, as MSVC emits.
* **The dtor nulls nothing.** It does not clear the seven pointers it releases and
  does not touch the bindings `ClearResources` clears. Retail treats a Context as
  dead after this — `CreateVideoContext` calls the dtor and hands the block
  straight to `operator delete`.
* **The three `operator delete` bodies at 0x180006a10, 0x180018500 and 0x18001f360
  are byte-identical** (`if (p) { Enter alloc CS; Free(_singleton, p); Leave; }`)
  and are three separate instantiations the compiler did not fold. One C++ body
  serves all three; each address is registered separately.
* **`MemFree` is reached by address with the RtResolveCached contract — cache a
  hit, RETRY a miss.** Freezing a null would take the heap down for the life of
  the process if anything ran before attempt 14's
  `RegisterAllocatorAddresses()` armed the door.

---

## 3. Verification — what is checked, and by what

### `ClearResources`: FULL oracle plus the callers it does not have

`?ClearResources@Context@Video@@QEAAXXZ` has no call site anywhere in `code/`,
`engine/`, `runtime/`, `standalone/` or `dx11/`. A registered port on a path
nothing executes reports `calls=0 divergences=0` forever, which is not a
verification (`reference_uncalled_port_is_not_verified.md`). So the probe supplies
the callers.

It drives the **same** `RtVideo_Context_ClearResources` the registry hands out —
not a re-derivation — and the reference side is the **shipped body** reached
through `ResolveExportRaw`, because `ResolveExport` would hand back our own port
once registered and the comparison would be vacuous
(`reference_resolveaddr_consults_port_registry_first.md`).

512 cases sweeping which image slots and which vertex streams are bound, each with
a non-zero pre-existing dirty state so the `|=` is exercised against something
other than 0. Compared field by field over the only fields either side writes —
3 dirty words, 16 image slots, 16 vertex slots, 16 offsets, 16 strides — so no
byte that neither side writes enters the comparison
(`reference_oracle_must_not_compare_unwritten_bytes.md`).

```
[CtxClear] cases=512 mismatches=0 cleared=511 theirsCleared=511 (effectiveness)
           negative-control=CAUGHT
```

* **EFFECTIVENESS, measured on BOTH sides.** `theirsCleared` is produced by the
  shipped body, which this lane did not write. 511 of 512, not 512: seed 0 builds
  an all-null Context that neither body can act on. The two agree exactly, so the
  bodies agree about *when* to act as well as about the result.
* **NEGATIVE control CAUGHT.** A copy of the first loop with the image mask seeded
  at 2 instead of 1 must be, and is, caught — otherwise the comparison is not
  reading `_imageBufferDirty` and all 512 green cases mean nothing.

#### The probe's own effectiveness control was noise on its first run, and I fixed it

The first run reported `cleared=512` out of 512 cases. **That was impossible** —
seed 0 provably cannot change anything — and chasing it found the defect *in the
probe*: `ClearSnapshot` carries four bytes of padding before its first pointer
array (three `uint32_t`s, then a `void*` needing 8-byte alignment) and
`SameSnapshot` memcmps the whole struct, so the control was counting uninitialised
stack bytes. The mismatch column survived only by luck — the two long-lived
snapshots happened to share their padding; had they differed, every one of the 512
cases would have read as a divergence and this port would have looked catastrophically
broken.

This is `reference_oracle_must_not_compare_unwritten_bytes.md` occurring inside
the instrument rather than inside the subject, and it is the reason the fix adds
*two* self-checks that print when `cleared != theirsCleared` and when
`cleared == cases` (the artefact's exact signature). Commit `76ee14a`.

### `~Context`: LIVE, executed, and clean — but with no bit-level oracle

The destructors have **no oracle and this lane does not pretend otherwise**:
running ours and then theirs frees every sub-object twice and the second pass
operates on freed memory. That is Wave 6's argument for the same family and it is
unchanged.

What *is* evidence:

* `RtEngine_MainLoop_Dtor`'s address door now reaches **our** `~Context` and
  **our** `operator delete`, so the port executes on every clean exit, on a real,
  fully-constructed 0x2168-byte `Video::Context` (seven constant buffers, the
  dynamic vertex/index/instance buffers, a render buffer and two image buffers)
  built by the **DLL's** ctor.
* `Engine::MainLoop::~MainLoop calls=1 divergences=0` — its shadow asserts
  `_context == nullptr` plus the three `Collection::Array` teardowns.
* `rc=0`, empty `standalone_crash.log`, no allocator or redzone complaint. A wrong
  transcription of a destructor that walks seven sub-objects and does eight
  refcounted `System::String` frees would very likely fault or trip the allocator.

**Stated plainly: that is an execution signal, not a bit-exactness result.** The
shadow's `_context == nullptr` term is written by our own code and proves nothing
about the teardown; the load-bearing evidence is that the port ran on a live
object and the process came down clean.

`~ConstantBuffer`, `RenderBuffer::PlatformDestruct` and `??3RenderBuffer` are
exercised only through that path and have no independent check.

### Run hygiene

Six lanes share one game directory. **The first run I took was void** — the
deployed exe's md5 changed under it and the log I copied belonged to another
lane's exe (build stamp 09:16:41, `Video: 102 exports registered`, i.e. without
this lane). A later redzone run was clobbered mid-flight the same way (`rc=139`
against another lane's 09:08 exe) and is also discarded.

Every number quoted above comes from a run whose deployed exe md5 was verified
**identical before and after**, with the logs copied inside the same shell
invocation:

| run | exe md5 | rc | result |
|---|---|---|---|
| `vid2diff` | `5d612c9b…` | 0 | 16.2M diffed calls, 1 divergence (pre-existing) |
| `vid2p2` | `a76e66aa…` | 0 | probe fix; 512/0/511/511/CAUGHT |
| `vid2p3` | `c24a7717…` | 0 | 15.5M diffed calls, 1 divergence (pre-existing) |

The single divergence in every run is `Object::Factory::Startup [probe, not
diffable]` (`chain=416 nodes=415` vs `nodes=416`), which is pre-existing and
unrelated to this lane.

**Package-set caveat.** All three runs were taken **before** lane MOD restored
`betterFields.pkm` / `DebugMenu.pkm` into `WinData/`, so they are on the
mods-absent set that every earlier measurement in this wave used. Neither result
is package-sensitive in any case: the probe runs at registration time, before any
package is mounted, on two `.bss` Contexts, and the `MainLoop` object is built and
destroyed regardless of what resources exist.

---

## 4. Not ported, and the measured reason

### `??0Context@Video@@QEAA@PEAVDevice@1@HHHPEAVRenderBuffer@1@@Z` (0x1800f1820) — DECLINED

**Verdict for the integrator: NOT TAKEN.** The door stays unregistered.

The ctor itself is 361 instructions of field zeroing plus seven inline
`ConstantBuffer` constructions, and that part is transcribable. What blocks it is
one callee:

> **`?AssignConstants@Context@Video@@AEAAXXZ` (0x1800ef3d0, ordinal 1629) is
> 1975 instructions** — 20 `?AddConstant@ConstantBuffer@Video@@QEAAXAEBVString@System@@0H@Z`
> calls, 75 `?Set@String@System@@QEAAXPEBD@Z` calls and 104 `MemFree`s, building
> the shader constant-name table.

That is a lane, not a slot, and it needs `System::String` construction from
literals through the game allocator. Secondary, and small by comparison:
`?PlatformConstruct@RenderBuffer@Video@@AEAAXXZ` (0x1800f4320) and three class
`operator new` bodies (0x180011640 unexported, `??2ImageBuffer@Video@@SAPEAX_K@Z`
0x18001e910, `??2RenderBuffer@Video@@SAPEAX_K@Z` 0x18001f2e0) are all ~25
instructions each.

**Is 0x1800f1820 reached at cutover?** Not today, and the reason matters: the
fault happens one level up. `RtEngine_MainLoop_Ctor` calls `CreateVideoContext`,
which is itself unported, so the boot dies there before the ctor is ever entered.
The ctor's door becomes live only once `CreateVideoContext` is ported, and porting
`CreateVideoContext` without the ctor would relocate the null rather than remove
it.

### `?CreateVideoContext@MainLoop@Engine@@AEAAXXZ` (0x1800dd500, ord 2139) — DECLINED, but GIVEN A NAME

Lane VID recorded this as "unported **and unexported**". **The unexported half is
wrong** — it is ordinal 2139, read out of the shipped export directory — and the
error was load-bearing, because it justified resolving it by address at
`runtime/src/engine/engine.cpp:521`, on the live boot path, which made the
boot-path `EXEC-of-0` **nameless**.

It is now `RT_EXPORT_ONCE("?CreateVideoContext@MainLoop@Engine@@AEAAXXZ")`. With
the DLL present nothing changes (`ResolveExport` finds the same export; verified,
`MainLoop::MainLoop calls=1 divergences=0`, no MISS line). At cutover the fault is
preceded by `[RtResolve] MISS "?CreateVideoContext@MainLoop@Engine@@AEAAXXZ"`, the
way `??0PackageFile@IO@@` announced itself on attempt 14. Still an unguarded call,
for lane VID's reason: a guard would leave `_context` null and move the fault to
whichever of the ~90 `Video::Context` call sites touches it first.

Two of its three callees are now ported (`~Context`, `operator delete`), and the
allocator NO-GO it also cited is discharged by attempt 14. **The ctor is the whole
remaining gap.**

### `?Startup@Device@Video@@` / `?Shutdown@Device@Video@@` (+ the two Platform bodies) — BLOCKED ON ONE THING

`?Startup@` (0x1800f7270) and `?Shutdown@` (0x1800f7280) are confirmed
one-instruction `JMP` thunks onto `?PlatformStartup@Device@Video@@AEAA_NXZ`
(0x1800f8260, ord 3405) and `?PlatformShutdown@Device@Video@@AEAAXXZ` (0x1800f83f0,
ord 3396) — disassembled, not taken on trust. All four are live unsatisfied doors.

**`?PlatformStartup@` is a HARD CUTOVER BLOCKER and the next boot wall after
`IO::PackageFile`.** Our own ported `Engine::Startup` calls it at
`runtime/src/engine/engine_lifecycle.cpp:719`; a null there returns 0 with
`g_startupFailAt = 11` and the boot ends. (Attempt 14 never reached it — the run
died at `LoadPackages`, and the `?Startup@`/`?Shutdown@` rows in its residue come
from `engine/funcs/engine_funcs.cpp`'s bulk resolve pass, not from a call.)

**Three of lane VID's four stated blockers for this cluster are false, and I
checked each against the disassembly rather than inheriting them:**

1. > "loops the renderer interfaces through `CreateVideoInterface` (which
   > `LoadLibrary`s a plugin DLL and calls its `CreateDeviceData`)"

   **False.** `?CreateVideoInterface@Device@Video@@QEAA_NH@Z` (0x1800f7930) is 30
   instructions and contains **no `LoadLibrary`**. It tests three already-populated
   pointers in `Device::dllSlots[idx]` (+0xb0/+0xb8/+0xc0), releases the old
   `DeviceData` through its vtable, stores the index at +0xf8 and calls the slot's
   `createDeviceData`. The `LoadLibraryW` lives in `InitializeInterfaces`
   (0x1800f79f0), which `EnumerateVideoModes` drives and which
   `standalone/video_interface.cpp` already detours. "A second `Startup` makes a
   second D3D11 device" is true of the `DeviceData`, not of loading a DLL twice.

2. > "calls the **private** no-argument `ValidateConfiguration` (0x1800f8830)"

   **False.** It is exported: `?ValidateConfiguration@Device@Video@@QEAA_NXZ`,
   ordinal **4533**. (Wave 6 lane B ported the eight-argument overload, ordinal
   4532, which is a different function — that part of the note was right.)

3. > "⚠ with the DLL gone, `Engine::Startup` — which is itself held back — no
   > longer runs. **Nothing in our tree starts the video device.**"

   **Stale.** `Engine::Startup` **is** ported (Wave 9 lane ENG,
   `runtime/src/engine/engine_lifecycle.cpp`) and line 719 is precisely the call
   that starts the video device. Something in our tree does start it, and that
   call is the wall.

**What actually blocks it, after those three fall: the `Application`
message-callback `Collection::Set`, and nothing else.** `PlatformStartup` ends by
registering two USER32 message callbacks
(`?RegisterMessageCallback@Application@1@QEAAXIP6AHPEAUHWND__@@I_K_J@Z@Z`,
0x18009fa60, WM_SIZE=5 → `?SizeWindow@Device@Video@@` 0x1800f7d20, and 0x232 →
`?MoveWindow@Device@Video@@` 0x1800f7d90); `PlatformShutdown` opens by inlining
the matching `UnregisterMessageCallback` twice. Both are AVL operations on a
`Collection::Set<Pair<unsigned, callback>>` rooted at `Application+0x18`, and
their five helpers are all unexported and unported:

| address | what | size |
|---|---|---|
| 0x18000a5b0 | node alloc + element copy | ~25 instr |
| 0x18000adc0 | AVL rebalance | ~120 instr |
| 0x18000b040 / 0x18000b170 | the two rotations | — |
| 0x18000aba0 | AVL erase | ~110 instr |
| 0x180009c30 | node free | ~15 instr |

`?RegisterMessageCallback@` and `?UnregisterMessageCallback@` are **also** two of
the twenty-odd rows in `dx11/src/devicedata.cpp`'s all-or-nothing import table, so
closing them pays twice. Lane D's `KeyMap::RegisterInput`
(`runtime/src/input/input.cpp:242`) is the precedent for the descent-and-link half
against a *different* instantiation of the same template.

Everything else `PlatformStartup` needs is either ported or small and
self-contained, and I sized each rather than guessing:

* `?ValidateConfiguration@Device@Video@@QEAA_NXZ` 0x1800f8830 — ~180 instructions,
  fully read (clamp `Configuration` against the selected adapter's mode/refresh/
  multisample/filter lists, then copy the adapter's mode and multisample arrays
  into the Device). Needs three unexported `Collection::Array` `operator=`/dtor
  helpers — 0x18000fcd0 + 0x180010830 + 0x180010cf0 (`Array<DisplayMode>`),
  0x180008b00 + 0x180009560 (`Array<bool>`), 0x180010760 + 0x18000fbf0
  (`Array<RefreshRate>`) — all in lane Q's `ArraySpec` idiom.
* `?GetDefaultResolution@Device@Video@@QEBAHAEBUAdapter@12@@Z` 0x1800f7540 — ~35
  instructions, `EnumDisplaySettingsW`.
* `?SizeWindow@` / `?MoveWindow@` — ~25 instructions each, `GetClientRect` /
  `GetWindowPlacement` / `ClientToScreen`. Both must be ported: their addresses are
  what `RegisterMessageCallback` stores, so leaving them unported stores a null
  that `Application::PlatformUpdate` later calls.
* `?SetText@Error@System@@SAXPEBD_N@Z` — already ported.

I did **not** port that subset. Porting five prerequisites that close **zero**
unsatisfied rows, have **no live caller** without `PlatformStartup`, and cannot be
exercised, is `reference_uncalled_port_is_not_verified.md` five times over; the
honest deliverable is the sharpened blocker.

### `?OnVideoError@` / `?OnVideoRestart@MainLoop@Engine@@` — re-checked, disposition unchanged

`WAVE9.md`'s group-D triage says **STUB**: "at cutover they need to exist, not to
be faithful; a port would be dead code reporting `calls=0`." Re-checked and it
stands, and lane VID's thunk fix already satisfies the "need to exist" half —
`RtEngine_MainLoop_OnVideoError_static` / `…Restart_static` reach the members
through `RT_EXPORT_ONCE` and log-and-return when the member is missing, rather
than jumping to zero.

`OnVideoError`'s callee census is unchanged: ten resource type ids in ten
unexported, unminted `.data` globals, seven unported internal callees, two more
singletons — ~17 nulls relocated and none removed. `OnVideoRestart`'s first call
is `CreateVideoContext`, so it duplicates §4's open row rather than closing it.
Their appearance in attempt 14's residue is `engine_funcs.cpp`'s bulk resolve
(consecutive `asked by` addresses `…407c`, `…4092`), not a call.

### §5.5's forced device-lost event — NOT STAGED, and here is why

The PLAN owes a forced device-lost event. **This lane did not make it cheap and
says so plainly.** The device-lost path is entered from `Video::DeviceData` on a
`D3D11 DXGI_ERROR_DEVICE_REMOVED`, which reaches `Engine::MainLoop` only through
the two `Video::Device` +0x50/+0x58 slots. Forcing it needs either a real device
removal or a synthetic call through those slots — and `OnVideoError`, the thing it
would exercise, is unported, so the forced event would exercise the DLL's body,
not ours. Staging it is worth doing **after** `OnVideoError` is ported, not before.

---

## 5. Audit accounting, with denominators and a set-diff

The project standard is an unmoved denominator plus a set-diff
(`docs/runtime_port/WAVE9.md`, attempt 13). **My denominator moved by exactly one,
deliberately, and I am declaring it rather than netting it out.**

| Instrument | Before (`29cfaa2`) | After | Δ |
|---|---|---|---|
| `unsatisfied_export_scan.py` | **789** names, 149 unsatisfied | **790** names, **148** unsatisfied | denominator **+1**, unsatisfied **−1** |
| listed unsatisfied names | 146 | 145 | −2 removed, **+1 added** |
| `runtime_census.py` TOTAL | 4632 / called **958** / ported **665** / 69.4 % | 4632 / called **959** / ported **668** / **69.7 %** | +1 called, +3 ported |
| `runtime_census.py` Video | 573 / 136 / **91** / 66.9 % | 573 / 136 / **94** / 69.1 % | +3 |
| `rawdoor_audit.py` | **501** sites, **0** UNGUARDED-CALL, 36 DEFERRED | **502** sites, **0** UNGUARDED-CALL, 37 DEFERRED | +1 site |
| `port_bypass_audit.py` ADDRESS | **209** sites, ADDR-REGISTERED 33, UNPORTED 76 | **210** sites, ADDR-REGISTERED **36**, UNPORTED **74** | +1 site, 2 doors satisfied |
| `port_bypass_audit.py` NAME | **429** sites, **670** ported names, 0 missed | **430** sites, **675** ported names, 0 missed | +5 names |
| `export_name_audit.py` | 662 names, 1 not in the DLL | 663 names, **1** not in the DLL | unchanged (the known placeholder) |

**Set-diff on the unsatisfied list:**

```
REMOVED   ??1Context@Video@@QEAA@XZ
REMOVED   ?ClearResources@Context@Video@@QEAAXXZ
ADDED     ?CreateVideoContext@MainLoop@Engine@@AEAAXXZ
```

The addition is the point of §4's second row, not a leak: it converts a **nameless**
boot-path `EXEC-of-0` that no instrument could count into a named one that every
instrument counts. Netting it against the two removals would hide exactly the
thing the change was for.

Reading the rest honestly:

* The census moves **+3, not +5**, because it counts only ports among the 959
  functions our tree calls by name; `~ConstantBuffer` and `??3RenderBuffer` have no
  by-name caller — my own dtor calls them as C++ symbols
  (`reference_uncalled_port_is_not_verified.md`, stated not hidden).
* `rawdoor` and `port_bypass` ADDRESS each move **+1**, which is the single
  `ResolveAddr(Module::Runtime, 0x1800c7ad0)` this file adds for `MemFree`.
  **0 UNGUARDED-CALL and 0 BYPASS in both columns**, unchanged.
* `port_bypass` ADDR-REGISTERED moves **+3** and UNPORTED **−2**: `engine.cpp:590`
  and `:592` are now satisfied doors.
* The five new `RegisterRuntimeExport` names show up as `port_bypass`'s +5 ported
  names; the seven `RegisterRuntimeAddr` calls are the *same* functions through the
  other registry and are correctly not counted again
  (`reference_census_three_blind_spots.md`).

---

## 6. ⚠ AN INSTRUMENT GAP THE INTEGRATOR SHOULD FOLD IN

`tools/port_bypass_audit.py`'s address-door regex is

```python
ADDR_SITE = re.compile(
    r"Resolve(?:Addr\(\s*Module::Runtime\s*,|RuntimeRaw\()\s*(0x[0-9a-fA-F]+)\s*\)")
```

— it requires a **hex literal**. A call site that spells the address as a named
constant is invisible to it, and (by the same shape) to the new address-door
census. **The boot-path `EXEC-of-0` at `engine.cpp:521` was hidden by exactly
this**: it read `ResolveAddr(Module::Runtime, MainLoopAddr::CreateVideoContext)`.
This is the mirror image of
`reference_census_cannot_see_nonliteral_registrations.md` — that one is about
non-literal *registrations*, this one is about non-literal *requests*.

Tree-wide there are **20** such sites. Nine take an `addr` / `ghidra` / `dtor`
**parameter** (wrapper helpers; their callers pass literals). Eleven use a named
**constant**:

| site | constant | value | verdict |
|---|---|---|---|
| `code/Game/Dialog/mod_dialog_vtables.cpp:501,549,632` | `kRVA_FileSystem_Singleton` | 0x1801fc6a8 | **safe** — minted |
| `code/Game/Dialog/mod_dialog_vtables.cpp:665,708` | `kRVA_Modification_Singleton` | 0x1801fc6b0 | **safe** — minted |
| `code/Game/Object/internal_factory_audit.cpp:54` | `kInternalFactorySingleton` | 0x1801fc6d8 | **safe** — minted |
| `dx11/src/dx11_backend_access.h:78` | `kRt_VideoDeviceSingleton` | 0x1801fe7a8 | **safe** — minted |
| `standalone/video_interface.cpp:141` | `kInitializeInterfaces` | 0x1800f79f0 | known — attempt 14 logs `[VideoInterface] could not resolve Video::Device::InitializeInterfaces` |
| `standalone/persistobject_probe.cpp:166` | `kPersistObject` | 0x1800bff10 | probe only |
| `runtime/src/engine/engine.cpp:478` | `MainLoopAddr::OnVideoError_static` | 0x1800dcd20 | already replaced by lane VID's own thunks |
| `runtime/src/engine/engine.cpp:521` | `MainLoopAddr::CreateVideoContext` | 0x1800dd500 | **was the live boot-path null; converted to a name door by this lane** |

The four `.data` rows are safe because `standalone/rt_data_mint.cpp:577-588`
registers **both** doors for every mint row — name *and* address. So the gap
produced exactly one live defect, and it is now closed; but the regex should still
be widened, because the next one will not be as lucky.

---

## 7. What I could not verify

* **The destructors have no bit-level oracle** and cannot have one before cutover.
  Their correctness rests on transcription from disassembly plus the execution
  signal in §3.
* **`~ConstantBuffer`, `RenderBuffer::PlatformDestruct` and `??3RenderBuffer` are
  exercised only through `~Context`.** No independent check.
* **`ClearResources` still has no in-game caller.** Its 512-case sweep is the whole
  of its verification; nothing in a real frame drives it.
* **I did not run the DLL-less cutover boot** — that is the integrator's, and the
  brief forbids it.
* **The `RenderBuffer` list-head fixup is unexercised.** Nothing in the runs put a
  `RenderBuffer` on the `_firstRenderBuffer` list *and* destroyed it through my
  port; the Context's own default render buffer does go through it at teardown, but
  no instrument reports whether the list was non-trivial at that moment.
* **The three discarded runs** (§3) are named rather than dropped, because a run
  clobbered by another lane looks exactly like a run that worked.

---

## 8. Falsifiable predictions for the next DLL-less boot

1. `??1Context@Video@@QEAA@XZ` and `?ClearResources@Context@Video@@QEAAXXZ` will
   **not** appear in `BANISHED_RT_STRICT`'s residue. If either does, the
   registration is not running — check that `[RtPort] Video:` moved by 5 (102 →
   107) and that `BANISHED_RT_VIDCTXLIFE` is not 0.
2. `?CreateVideoContext@MainLoop@Engine@@AEAAXXZ` **will** appear, as a
   `[RtResolve] MISS` line, immediately before an `EXEC-of-0` whose return address
   is inside `RtEngine_MainLoop_Ctor`. **That is the intended outcome of §4's
   second row.** If the fault is there but the MISS line is absent, the
   `RT_EXPORT_ONCE` conversion did not take.
3. Once `IO::PackageFile` lands and the boot clears `LoadPackages`, the next wall
   will be `Engine::Startup` returning 0 with `g_startupFailAt == 11` — the
   `?PlatformStartup@Device@Video@@AEAA_NXZ` call at
   `engine_lifecycle.cpp:719` — **not** a crash. If the boot instead dies inside
   `RtEngine_MainLoop_Ctor` first, prediction 2 fired ahead of this one and both
   are still consistent; the ordering between them depends on which runs first at
   boot.
4. `Engine::MainLoop::~MainLoop` will run our `~Context` at shutdown. If the
   process faults on exit rather than at the walls above, suspect this port
   first — it is the only thing this lane put on a live path.

---
---

# PHASE 2 — `?PlatformStartup@Device@Video@@` CLOSES

Re-tasked by the integrator after attempt 15: with `?ReadPackfile@` owned by lane
PKG2, the wall behind it is `PlatformStartup`, which this lane's phase 1
identified by measurement (`g_startupFailAt == 11`). **It is closed, with its
whole transitive closure.** Commit `070b546` on `wave9/lane-vid2`, on top of
`abef0bc`.

## P1. The result, in one table

| | before (`abef0bc`) | after (`070b546`) |
|---|---|---|
| `unsatisfied_export_scan` | **139** of 836 | **135** of 839 |
| listed unsatisfied names | 136 | 132 |
| `runtime_census` TOTAL | 980 called / **711** ported / 72.6 % | 983 called / **720** ported / **73.2 %** |
| `runtime_census` Video | 138 / **96** / 69.6 % | 139 / **101** / 72.7 % |
| `runtime_census` Application | 12 / **6** / 50.0 % | 13 / **8** / 61.5 % |
| `runtime_census` internal | 94 / **65** | 95 / **67** |
| `addr_door_audit` UNREGISTERED | **44** | **43** |
| `rawdoor_audit` | 545 sites, **0** UNGUARDED-CALL | 551 sites, **0** UNGUARDED-CALL |
| `port_bypass_audit` NAME | 456 sites, **0** bypass | 459 sites, **0** bypass |
| `export_name_audit` | 1 known placeholder | 1 known placeholder |

**Set-diff on the unsatisfied list — four removed, NOTHING added:**

```
REMOVED  ?PlatformStartup@Device@Video@@AEAA_NXZ
REMOVED  ?PlatformShutdown@Device@Video@@AEAAXXZ
REMOVED  ?Startup@Device@Video@@QEAA_NXZ          (a one-instruction JMP thunk)
REMOVED  ?Shutdown@Device@Video@@QEAAXXZ          (likewise)
ADDED    (none)
```

The denominator moves 836 → 839: three names this lane resolves that nothing
previously asked for (`?_singleton@Application@1@`, and the two
`Application::*MessageCallback` exports the probe reaches raw). All three are
satisfied, which is why the ADDED list is empty.

## P2. The closure was counted before any body was written

Lane PICK's rule, applied. `PlatformStartup`'s closure is **21 bodies**:

**11 exports** — `PlatformStartup` · `Startup` · `PlatformShutdown` · `Shutdown` ·
`ValidateConfiguration` (the NO-ARGUMENT one, ord 4533) · `CreateVideoInterface` ·
`SizeWindow` · `MoveWindow` · `GetDefaultResolution` ·
`Application::RegisterMessageCallback` · `Application::UnregisterMessageCallback`

**12 unexported addresses** — the four `Collection::Set` AVL bodies
(`0x18000adc0` retrace, `0x18000b040`/`0x18000b170` rotations, `0x18000aba0`
erase), the Set node allocator (`0x18000a5b0`), and the seven
`Collection::Array` helpers `ValidateConfiguration` drives (`0x18000fcd0`,
`0x180010830`, `0x180010cf0`, `0x18000fbf0`, `0x180010760`, `0x180008b00`,
`0x180009560`).

| | doors before | doors after |
|---|---|---|
| **Four-row lane (rejected)** | 4 unsatisfied names | 17 new UNREGISTERED address doors |
| **Twenty-one-body lane (shipped)** | 4 unsatisfied names | **0 new doors**; the census moved 44 → 43 |

**The blocker's own closure did NOT explode, and that was checked rather than
assumed.** Both AVL rotations are LEAVES — `disassemble_function(filter_mnemonics="CALL,RET")`
returns one `RET` and no `CALL` for each — and the retrace and erase call only
each other plus two allocator entry points attempt 14 already registered.

Files: `runtime/src/collection/set_avl.{h,cpp}`,
`runtime/src/application/message_callbacks.cpp`,
`runtime/src/video/device_startup.cpp`, `…_win.cpp`, plus two probes.
Kill switches `BANISHED_RT_SETAVL`, `BANISHED_RT_APPMSGCB`,
`BANISHED_RT_VIDDEVSTART`.

## P3. Three recorded blockers re-checked, and all three were false

The integrator asked for the same re-check discipline phase 1 applied. All three
came from `WAVE9_LANE_VID.md` and all three were read against the disassembly
rather than inherited:

1. **"`CreateVideoInterface` LoadLibrary's a plugin DLL."** False. `0x1800f7930`
   is thirty instructions and contains no import call at all. It tests three
   ALREADY-POPULATED pointers in `Device::dllSlots[idx]` (+0xb0/+0xb8/+0xc0) and
   calls the slot's `createDeviceData`. The `LoadLibraryW` is in
   `InitializeInterfaces` (`0x1800f79f0`), which `EnumerateVideoModes` drives and
   which `standalone/video_interface.cpp` already detours — the run log confirms
   it: `[VideoInterface] DX11 slot pre-filled … Runtime will not LoadLibrary`.
2. **"the PRIVATE no-argument `ValidateConfiguration`."** False. Exported,
   `?ValidateConfiguration@Device@Video@@QEAA_NXZ`, **ordinal 4533**. Wave 6
   lane B ported the eight-argument overload at ordinal **4532** — adjacent
   ordinal, different function, exactly the confusion
   `reference_ghidra_mcp_address_param_silently_ignored.md` exists for.
3. **"nothing in our tree starts the video device."** Stale. `Engine::Startup` is
   ported (lane ENG) and `engine_lifecycle.cpp:719` **is** that call.

The fifth Set helper was also re-checked: `0x180009c30` is already registered,
and this file relies only on its shared contract (ignore arg 1, free arg 2)
because — reported, not fixed — **it is registered THREE times**, by
`allocator.cpp:1429`, `linkedlist.cpp:298` and `modification.cpp:913`. Last
binding wins, so two are dead code and which survives depends on init order.
All three bodies agree today. Deciding the owner is not this lane's call.

## P4. Verification

Every number below comes from a run **gated on an identity line this build
emits**, taken under `tools/game_dir_lock.sh`, on exe md5 `4314282d…` verified
identical before and after:

```
[LaneVID2] identity: device_startup.cpp built Sep  8 2026 10:36:07
```

### The AVL: lockstep, 65 steps

```
[SetAvl] steps=65 mismatches=0 parent-linkage-bad=0
[SetAvl] effectiveness: depthAfter24Ascending=5 (a spine would be 24)
         rootChanged=6 erases=15 finalCount=35 dupCount=49
[SetAvl] negative-controls: signed-descent=CAUGHT  shape-sensitivity=CAUGHT
```

Two private Sets over a shared script, ours through the ported bodies and theirs
through `ResolveExportRaw`, **compared after every operation**. Lockstep is not
belt-and-braces here: **AVL defects cancel.** A rotation wrong in one direction
and wrong again in the other yields the same key set and frequently the same
final shape, so one comparison at the end reports green on a tree that was wrong
throughout. The fingerprint carries shape (which side of its parent each node
hangs off), balance and height, and parent linkage is walked separately because
a fingerprint built from `left`/`right` cannot see a wrong `parent` — which is
exactly what a mis-transcribed rotation produces and what breaks the *next*
retrace.

`depthAfter24Ascending=5` is the effectiveness control and it cannot be
satisfied by noise: 24 ascending inserts build a 24-deep spine unless rotations
actually ran. The population also carries a descending block, a zig-zag block
(the only path through the pre-rotation predicates at `0x18000ae40`/`0x18000ae74`),
ids above `0x7FFFFFFF` (the descent is UNSIGNED — `JC`, not `JL`), a duplicate
insert that must not move the count, an absent erase, and erases chosen to hit
all three cases of `0x18000aba0` including **case 3 with `s != z->left`** — the
PREDECESSOR splice a successor-splice misreading would get wrong while still
producing a valid tree.

### `ValidateConfiguration`: 19 cases over a synthetic adapter graph

```
[VidCfg] cases=19 mismatches=0 return-value-mismatches=0
[VidCfg] effectiveness (all measured on THEIRS): defaultRes=1 msReset=1
         filterClamp=2 detailClamp=2 refreshFallback=2 indexClamp=4
[VidCfg] negative-control=CAUGHT
```

**Every effectiveness counter is measured on THEIRS**, so none can be satisfied
by our port agreeing with itself. All six are non-zero. The comparison is
field-by-field, and the two copied arrays are compared **by content** — they
hold independent allocations by construction, so comparing `_data` would fail
100% of calls. `_capacity` *is* compared, because it is the growth rule's output
and the reason `GrowFor` is transcribed rather than simplified.

The negative control corrupts our `shadowScalar` cap from 2 to 3 — the
`shaderDetail` cap, one line above it in the disassembly, and the exact
copy-paste this function invites.

### The real result: live exercise, in order

With these registered, `Engine::Startup`'s `RT_EXPORT_ONCE` resolves **our**
`PlatformStartup`. The log shows the whole path:

```
[VidCfg] …                                   <- top of OUR PlatformStartup
[BootTime]   DX11 CreateDevice: entry
[BootTime]   DX11 D3D11CreateDevice
[BootTime]   DX11 swapchain + color/depth buffers
[SetAvl] …                                   <- only reachable from
                                                RegisterMessageCallback, i.e.
                                                PlatformStartup's SUCCESS path
[GameMain] Engine::Startup returned 1
```

`[SetAvl]` firing is the load-bearing line: it is called from
`RtApp_RegisterMessageCallback`, which `PlatformStartup` reaches **only** after
`CreateVideoInterface` succeeded and `DeviceData::Create` returned true. So our
`ValidateConfiguration`, our `CreateVideoInterface`, our retry loop and our
message-callback insert all ran, against a real D3D11 device and swap chain.
16,145,527 diffed calls, 1 divergence (the pre-existing
`Object::Factory::Startup [probe, not diffable]`), empty crash log, exit 0.

`PlatformStartup` and `PlatformShutdown` have **no probe and cannot have one**:
they create and destroy the one D3D11 device, so ours-then-theirs makes a second
swap chain against a window that already has one. Live exercise is what they
get, and a wrong transcription there does not report a divergence — it fails to
render.

## P5. Transcription findings worth carrying

* **The retry loop is not a for-loop.** It starts at the configured interface
  index clamped by an **unsigned** compare (`CMP EBX,2 / CMOVA EBX,1` — a
  *negative* configured index is above 2 unsigned and becomes **1**, not 0),
  walks **downward**, and wraps to 2 at −1. From index 1 the order is **1, 0, 2**
  — not 1, 2, 0. A three-byte stack array records what has been tried.
* **`ValidateConfiguration`'s result is checked once and ignored once.** Fatal
  on the first call, discarded inside the loop (`0x1800f82f9`).
* **The refresh-rate fallback divides.** No exact match falls back to the first
  entry whose **integer ratio** matches, so 60000/1001 (→59) and 60/1 (→60) do
  NOT match while 120000/1001 and 119/1 both reduce to 119 and DO. Retail's
  arithmetic; ours picks the same surprising entry.
* **Two unguarded indexes**, both retail's: `multisampleData[msIdx]` is not
  bounds-checked, and the second-pass mode lookup can index with −1. Left
  unguarded per CLAUDE.md.
* **`ValidateConfiguration` walks the mode list TWICE** — once against the
  requested size, once against the clamped size — and recomputes the adapter
  pointer in between. A single pass agrees whenever the first lookup hits, which
  is every case a well-formed adapter reaches.
* **`MoveWindow` bails on `showCmd == 0x20000000`**, which is not a documented
  `SW_` value. Reproduced as the literal retail compares rather than "corrected".
* **`0x18000a5b0` never reads RCX**, and `RegisterMessageCallback` never sets it
  — a register neither set by the caller nor read by the callee is not a
  parameter (`reference_read_the_callee_before_holding_back_on_an_arg.md`).
* **The Set element's `+0x04` padding carries caller stack**: the node allocator
  copies two qwords and `RegisterMessageCallback` writes only the leading DWORD.
  Reproduced, and excluded from the probe's comparison.

## P6. One instrument artefact I caused and fixed

`tools/port_bypass_audit.py` classified my probe's `ResolveExportRaw` calls as
**BYPASS=2** on a live path, because it recognises oracle machinery by the
*enclosing function's name* (`ORACLE_FN_WORDS`) and mine was called
`LoadShipped`. Renamed to `ProbeLoadShippedBodies` — not to game the checker, but
because the checker's rule is right and the name was wrong. Two local
`ResolveExportRaw` re-declarations also counted as raw-door *sites*
(`UNRESOLVED-NAME`); removed in favour of `rt_port.h`'s. Both audits are back to
**0 BYPASS, 0 UNRESOLVED-NAME, 0 UNGUARDED-CALL**.

## P7. Still open, and the integrator's `AssignConstants` question answered

**`?AssignConstants@Context@Video@@AEAAXXZ` (0x1800ef3d0, 1975 instructions) is
MECHANICAL — a small lane, not a dedicated wave — but it is not a slot either,
and it must not be taken first.** Evidence rather than a guess:

* It is a **~20-row table**, not 1975 lines of distinct logic: 20
  `?AddConstant@ConstantBuffer@Video@@QEAAXAEBVString@System@@0H@Z` calls,
  each preceded by two `?Set@String@System@@` calls and followed by the inlined
  refcounted `System::String` release. That inlined release, repeated ~40 times,
  is where the 75 `String::Set` and 104 `MemFree` calls come from — the same
  four-instruction pattern this lane already transcribed for `~ConstantBuffer`.
* The literals are **plain and fully recoverable**. `0x180158508` onward reads
  `float2` / `texelSize` / `float4` / `fogColor` / `float3` / `lightColor` …:
  45 distinct strings across 76 references, i.e. (typeName, constantName) pairs.
* **`?Set@String@System@@QEAAXPEBD@Z` is already ported** (Wave 2,
  `runtime/src/system/string.cpp`).

**The ordering constraint is the actual answer.** Two dependencies are unported
and both belong to `Video::ConstantBuffer`, not to `Context`:
`?AddConstant@ConstantBuffer@Video@@…` (0x1800eec40) and
`?PlatformCreate@ConstantBuffer@Video@@AEAA_NXZ` (0x1800ef010). So the sequence
is **ConstantBuffer → AssignConstants → the `Context` ctor → `CreateVideoContext`**,
and taking `AssignConstants` first would relocate two nulls rather than remove
any.

**And one warning for whoever takes it.** The risk is not the control flow, it
is the 45 string literals: a mistyped shader-constant NAME is a parameter that
silently never binds, with no oracle short of a rendering comparison. Dump the
`.rdata` region and transcribe it mechanically; do not retype it. That is the
same class of hazard as
`reference_crt_coefficients_must_be_lifted_not_retyped.md`.

Also still open from phase 1 and unchanged: `?CreateVideoContext@MainLoop@Engine@@`
(now a NAMED door, §4), and `?OnVideoError@`/`?OnVideoRestart@` (WAVE9.md's
group-D **STUB** disposition re-checked and standing; lane VID's thunks already
degrade them to a logged no-op).

## P8. Falsifiable predictions

1. The next DLL-less boot will **not** stop at `?PlatformStartup@Device@Video@@`,
   and `Engine::Startup` will not return 0 with `g_startupFailAt == 11`. If it
   does, check `[RtPort] Video:` moved to 118 and that
   `BANISHED_RT_VIDDEVSTART` is not 0.
2. None of the four removed names will appear in `BANISHED_RT_STRICT`'s residue.
3. `?RegisterMessageCallback@Application@1@…` and
   `?UnregisterMessageCallback@Application@1@QEAAXI@Z` will drop out of
   `dx11/src/devicedata.cpp`'s unsatisfied import rows — **two of seventeen**.
   That table is all-or-nothing, so this does NOT make it resolve; fifteen
   remain and any claim that the renderer is cutover-ready is still refuted.
4. If the boot instead faults inside `Video::Context`'s constructor, that is
   §P7's chain and is expected, not a regression from this lane.
