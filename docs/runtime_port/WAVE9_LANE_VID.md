# Wave 9 lane VID — Video/Device and the two MainLoop video callbacks

Cluster assigned from cutover boot attempt 06's 61-symbol residue:

```
?Startup@Device@Video@@QEAA_NXZ
?Shutdown@Device@Video@@QEAAXXZ
?RecreateDynamic@Device@Video@@QEAAXXZ
?ReleaseDynamic@Device@Video@@QEAAXXZ
?ClearResources@Context@Video@@QEAAXXZ
??1Context@Video@@QEAA@XZ
?OnVideoError@MainLoop@Engine@@QEAAXXZ    0x1800dc5e0
?OnVideoRestart@MainLoop@Engine@@QEAAXXZ  0x1800dc3b0
```

---

## 0. The headline is not in the assignment

**`unsatisfied_export_scan.py` cannot see dx11's Runtime imports at all, and
there were 25 of them unsatisfied.** The scan's own declared blind spot is
"2 resolves go through a wrapper taking a non-literal argument". That
understates it by an order of magnitude: `dx11/` resolves its Runtime imports
from **table literals**, not from a `ResolveExport("literal")` call —

```cpp
struct Entry { void** dst; const char* mangled; uintptr_t iat; };
const Entry table[] = {
    { (void**)&g_im.DeviceReleaseDynamic, "?ReleaseDynamic@Device@Video@@QEAAXXZ", 0x180013230 },
    ...
};
for (const Entry& e : table)
    *e.dst = Dx11ResolveRuntimeImport(e.mangled, e.iat);
```

— and the scan's `DOOR` regex requires the name to sit inside the resolve call's
own argument list. Every row of both dx11 import tables is invisible to it.

Measured with a copy of the scan's own port/mint sets and a table-row regex:
**70 dx11 Runtime-import names, 25 with neither a port nor a mint entry.**
(Script: the four-line `ENTRY`/`DIRECT` regex in §7 below; it reuses
`unsatisfied_export_scan.py`'s `REG`/`MINT`/`strip_comments` verbatim so the
denominator is the same one.)

**Why that is a cutover-stopper and not an inventory row.** Both dx11 import
tables are all-or-nothing:

```cpp
const Imports* GetImports() { ...; return g_imOk ? &g_im : nullptr; }

void ContextData_Finish(ContextData* self) {
    const Imports* im = GetImports();
    if (!im) { Orig<void (*)(ContextData*)>(3)(self); return; }
```

`g_imOk` goes false if **any one** of the twenty entries resolved null. At
cutover `ResolveExport` misses the port registry for an unported name and falls
through to `ResolveExportRawImpl`, which has no module handle and returns null
(the mechanism `reference_converting_a_door_is_not_satisfying_it.md` records).
So **one** unported Runtime export in that table disables **every**
`ContextData` slot, and each slot then forwards to `Orig<>()` —
`ResolveAddr(Module::VideoDX11, …)` — and VideoDX11-x64.dll is **never loaded**
in this project. That is `EXEC-of-0` **on the first draw**, not on device loss.

`dx11/src/devicedata.cpp` has the identical structure and its own 20-odd
entries, seventeen of them unsatisfied.

This is why two of my assigned rows (`Device::RecreateDynamic`,
`Device::ReleaseDynamic`) are **per-frame infrastructure**, not device-lost
plumbing, and why they were the right thing to port first.

---

## 1. Ported — 13 functions

All thirteen mangled names were checked against the shipped export directory of
`C:\GOG Games\Banished\Runtime-x64.dll` with `pefile` **before** any body was
written (`reference_resolveexport_phantom_names.md`). All thirteen are present.

| Export | Retail | Ord | Why |
|---|---|---|---|
| `?Lock@Device@Video@@QEAAXXZ` | 0x18000ca90 | 3214 | dx11 import, **both** tables |
| `?Unlock@Device@Video@@QEAAXXZ` | 0x18000cac0 | 4462 | dx11 import, **both** tables |
| `?RecreateDynamic@Device@Video@@QEAAXXZ` | 0x1800f80b0 | 3522 | assigned row; dx11 import, both tables |
| `?ReleaseDynamic@Device@Video@@QEAAXXZ` | 0x1800f80e0 | 3536 | assigned row; dx11 import, both tables |
| `?RecreateDynamic@ImageBuffer@Video@@SAXXZ` | 0x1800f2d90 | 3523 | callee of the above |
| `?ReleaseDynamic@ImageBuffer@Video@@SAXXZ` | 0x1800f2e80 | 3537 | callee |
| `?RecreateDynamic@IndexBuffer@Video@@SAXXZ` | 0x1800f3500 | 3524 | callee |
| `?ReleaseDynamic@IndexBuffer@Video@@SAXXZ` | 0x1800f35d0 | 3538 | callee |
| `?RecreateDynamic@VertexBuffer@Video@@SAXXZ` | 0x1800f5660 | 3526 | callee |
| `?ReleaseDynamic@VertexBuffer@Video@@SAXXZ` | 0x1800f5730 | 3540 | callee |
| `?RecreateDynamic@RenderBuffer@Video@@SAXXZ` | 0x1800f4030 | 3525 | callee |
| `?ReleaseDynamic@RenderBuffer@Video@@SAXXZ` | 0x1800f40c0 | 3539 | callee |
| `?Recreate@RenderBuffer@Video@@QEAAXXZ` | 0x1800f4170 | 3521 | callee of `RenderBuffer::RecreateDynamic` |

New file `runtime/src/video/dynamic_resources.cpp`; the Win32 half of the two
`this`-taking lock bodies is appended to `runtime/src/video/buffers_win.cpp`
(PLAN §4.1 decision 1). Kill switch `BANISHED_RT_VIDDYNAMIC=0`.

**The triage rule is satisfied, not dodged.** `Device::RecreateDynamic` and
`ReleaseDynamic` are four-call forwarders; porting only them would have moved
eight nulls one frame deeper. All eight callees are ported here, and the ninth
(`RenderBuffer::Recreate`, the only non-forwarder among them) with them. **No
callee of any of the thirteen is reached by address.** The remaining edges are
the dx11 companion vtable slots, which are `dx11/`'s own objects and are ours
already.

### Two stale blockers, both checked and both false

1. **`WAVE6_LANE_B.md` §2.4: "`_firstRenderBuffer` … is a DLL-internal variable
   with no export."** It is exported — ordinal **4787**, `0x1801fe790`,
   `?_firstRenderBuffer@RenderBuffer@Video@@0PEAV12@EA`, read straight out of
   the shipped export directory. Lane C2 minted the other three sibling heads
   (`_firstSurface`, both `_firstBuffer`s) and skipped this one, so the fourth
   head had no storage after cutover. **Added** as one row of
   `standalone/rt_data_mint_tables.inc` (storage block grew 3056 → 3072 bytes;
   minted DATA symbols 112 → 113).

2. **`WAVE6_LANE_B.md` §2.3: hold `Device::Recreate/ReleaseDynamic` because
   "three of the four callees belong to lane C; porting the forwarder before
   those classes have owners would hard-code call-by-address into three classes
   another lane is reversing."** That reason is discharged by porting the
   callees rather than by waiting: nothing is called by address.

### What is verified, and what is not

**There is no oracle for the twelve walkers and forwarders, and this lane does
not pretend otherwise.** Wave 6's argument is unchanged and still correct:
running ours and then theirs releases every live GPU companion twice and
recreates it twice, and the second pass operates on freed objects; an
`RT_DIFF_SHADOW` has nothing to assert because the observable effect is entirely
inside `dx11/`. No `RT_DIFF_BEGIN` appears in any of those bodies.

Two things *are* under a check:

* **`Device::Lock` / `Unlock`** are the exported twins of the inline copies
  already in `buffers_win.cpp`, which lane W validated against retail through
  the empty-list probe. The two new bodies are transcribed instruction by
  instruction from 0x18000ca90 / 0x18000cac0 — including the store order, which
  **differs** from the inline copy (the export increments the depth *before*
  writing the owner id; the inline copy writes the owner first). Both orders are
  preserved as written.
* **`RenderBuffer::Recreate`'s width/height ladder** is not transcribed a fourth
  time. It is exactly `ImageBuffer::Width()` / `Height()` — bit8 = device
  backbuffer size, bit9 = half of it, otherwise the buffer's own size shifted by
  `Configuration::shadowScalar` when bit6 is set — applied to the colour
  attachment if present and the depth attachment otherwise. Those two accessors
  are already ported **and carry a FULL scalar oracle**
  (`buffers.cpp:429/450`), so calling them makes the equivalence compiler-
  enforced instead of comment-asserted, and puts the only real arithmetic in the
  new file under a check that already runs. Retail inlines the ladder, so under
  `BANISHED_RT_DIFF` this appears as extra `Video::ImageBuffer::Width` call
  counts — a measurement artefact, not a behaviour change.

### Faithfulness notes worth reading before trusting the bodies

* **The stale terminator is deliberate.** `ImageBuffer::ReleaseDynamic` and both
  buffer `RecreateDynamic`/`ReleaseDynamic` load the list head once, reload it
  **only inside the taken branch**, and compare the walk against that possibly
  stale value (`0x1800f2eb7` / `0x1800f2eef` / `0x1800f2efa`, and the twins).
  An element that fails the per-element guard therefore does not refresh the
  terminator. Reproduced literally. `ImageBuffer::RecreateDynamic` and both
  `RenderBuffer` walkers do the opposite — they re-read the head from memory
  every iteration (`0x1800f2e39`, `0x1800f407c`, `0x1800f4120`) — and are
  written that way.
* **The unguarded companion deref is retail's.** When `Device::deviceData` is
  null every `RecreateDynamic` stores a null companion and then dispatches
  through it (`0x1800f2df3 XOR EAX,EAX` → `0x1800f2e00` store →
  `0x1800f2e14 CALL [RDX+8]`). Left unguarded per CLAUDE.md: retail relies on
  the device data being there whenever anything is on these lists.
* **The per-element guards differ per class and one is a near-miss.**
  ImageBuffer's is `kind <= 1 || (unsigned)(kind - 3) <= 2`, i.e. kind in
  `{0,1,3,4,5}`. That is **not** `ImageBuffer::IsRenderBuffer()`, which is
  `{0,1,4,5}` — the dynamic kind 3 is included here and excluded there — so the
  existing accessor is deliberately **not** reused. Index/VertexBuffer guard on
  the `dynamic` bool at `+0x04`; RenderBuffer has no guard at all.
* **The link offsets are not uniform**, and two headers disagree with retail.
  ImageBuffer walks `+0x58` (`ImageBuffer.h` calls `+0x50`/`+0x58` `field50` /
  `field58`, "unknown; copied in the copy ctor" — they are the list links).
  Index/VertexBuffer walk `+0x18`, RenderBuffer walks `+0x20` (`Buffers.h`
  names the buffers' `+0x18` `listNext` and `Device.h` names RenderBuffer's
  `+0x20` `prev`). The lists are circular so both directions terminate; this
  lane transcribes the offset retail uses and **renames nothing** on the
  strength of it. Flagged for whoever owns those headers.
* **`Device::Recreate/ReleaseDynamic` take no `this`.** They are `QEAAXXZ` but
  neither body reads RCX (`0x1800f80b0` goes straight to `SUB RSP,0x28`).
  Declared without a parameter; dx11's table types them `void (*)(void*)` and
  passes an argument the callee ignores — both spellings interoperate on the
  Microsoft x64 ABI.

---

## 2. The MainLoop ctor thunk defect — FIXED

`runtime/src/engine/engine.cpp` (was lines 359-368) read:

```cpp
uint8_t* device = static_cast<uint8_t*>(*VideoDeviceSingletonSlot());
*(void**)(device + 0x50) = (void*)ResolveAddr(Module::Runtime, 0x1800dcd20);
*(void**)(device + 0x58) = (void*)ResolveAddr(Module::Runtime, 0x1800dc5c0);
```

`ResolveAddr(Module::Runtime, …)` returns **0** when the module handle is null
and nothing has registered the address
(`standalone/resolve_standalone.cpp:484`). With the DLL absent this ctor stored
**two nulls into the live `Video::Device`**.

**No gate row covers this, and the reason generalises.** It is an unported
function reached by **ADDRESS**. No residue list, no `rawdoor_audit` row, no
`port_bypass_audit` row and no export-name audit reports it, because **there is
no name involved at any point** — the same blind spot
`reference_bypassed_port_needs_a_source_checker.md` records for the reverse
direction.

**The consequence is NOT the EXEC-of-0 the brief predicted, and this matters.**
`dx11/src/contextdata.cpp:610-613` reads both slots and calls each behind a null
check:

```cpp
DeviceNotifyFn onLost = Field<DeviceNotifyFn>(owner, Dev::kOnDeviceLost);
if (onLost) onLost();
...
DeviceNotifyFn onRestored = Field<DeviceNotifyFn>(owner, Dev::kOnDeviceRestored);
if (onRestored) onRestored();
```

So the real cost was a **silently skipped notification**: on device loss the GPU
companions would be released (`Device::ReleaseDynamic`) and `Engine::MainLoop`
would never be told to rebuild the engine-side `Video::Context` resource list —
stale handles, nothing in the log. Quieter than a crash, and harder to find.

### The fix — three parts, as the brief required

1. **Our own thunks.** `RtEngine_MainLoop_OnVideoError_static` and
   `…OnVideoRestart_static` in `engine.cpp`. Each is our equivalent of the
   shipped two-instruction thunk (`MOV RCX,[MainLoop::_singleton]; JMP <member>`;
   verified by disassembly at 0x1800dcd20 / 0x1800dc5c0), and each reaches the
   member through `RT_EXPORT_ONCE`, i.e. the **port registry**, so a later port
   of either member is picked up with no edit here.
2. **The ctor stores those**, unconditionally, never `ResolveAddr`.
3. **The members themselves are declined** — see §3.

**Deviation, stated plainly.** Retail's thunk cannot fail; the member is in the
same image. Ours can, and when it does it logs one line naming the exact missing
member and returns instead of jumping to zero. That is *not* a null check
papering over a faithful crash: there is no upstream null to find, because our
standalone genuinely differs from retail — the member does not exist in the
process once the DLL is gone. Commented as such at the site.

**How I proved the fix.** By inspection plus `-fsyntax-only`; I did not and
could not run the game (the integrator holds the game directory). The proof
obligations and how each is discharged:

* *The stores no longer depend on the DLL* — the two operands are now
  `&RtEngine_MainLoop_OnVideoError_static` / `&…Restart_static`, addresses of
  our own `extern "C"` functions with external linkage in the same image. There
  is no resolver on that path at all.
* *The thunk reproduces the shipped one* — disassembly above, two instructions,
  and `MainLoopSingletonSlot()` already resolves
  `?_singleton@MainLoop@Engine@@1PEAV12@EA` through `RT_DATA_SLOT`, which is
  mint-aware, so the singleton the thunk loads is the one the mint owns after
  cutover (row already present in `rt_data_mint_tables.inc`).
* *The callers still fire* — `get_xrefs_to` on both thunks returns the export
  table, `.pdata`, and this ctor. Nothing else takes their address, so replacing
  what the ctor stores replaces every path to them.

**What I could not prove:** that the device-lost path executes at all. No run in
this project has ever triggered it, so the thunks are, like the members they
call, unexercised. `reference_uncalled_port_is_not_verified.md` applies to them
in full.

### A third null in the same ctor, found and NOT fixed

```cpp
reinterpret_cast<VoidThisFn>(
    ResolveAddr(Module::Runtime, MainLoopAddr::CreateVideoContext))(self);
```

`Engine::MainLoop::CreateVideoContext` (0x1800dd500) is unported **and
unexported**, so this resolves to 0 with the DLL absent and the call is an
`EXEC-of-0` **on the boot path** — earlier and harder than the two stores, and
invisible to every name-based instrument for the same reason they were.

Left as a faithful call, deliberately: a guard would leave `MainLoop::_context`
null and move the fault to whichever `Video::Context` call site touches it
first, which is strictly worse to diagnose than a VEH `EXEC-of-0` whose return
address names this line.

The body is small and fully reversed (documented at the site): free the old
context if any, take the allocator lock at `MemoryAllocator::_singleton + 0x640`,
`MemAlloc(0x2168, align 8)` through `0x1800c6e60`, drop the lock, and on success
run `??0Context@Video@@QEAA@PEAVDevice@1@HHHPEAVRenderBuffer@1@@Z` (0x1800f1820)
with `(Device::_singleton, 0x800000, 0x400000, 0x100000, nullptr)`. What blocks
it is the `Video::Context` lifecycle hold-backs in `context.cpp` (the ctor
allocates five sub-objects the dtor frees) plus the allocator NO-GO. **It needs
the lane that owns those.**

---

## 3. Declined, with evidence

### `?OnVideoError@MainLoop@Engine@@QEAAXXZ` (0x1800dc5e0)

~250 instructions. It builds a `Collection::Array<int>` of ten resource type ids
read from **ten unexported `.data` globals** (0x1801fa548, 0x1801fa698,
0x1801fa580, 0x1801fa5f0, 0x1801fa660, 0x1801fa5b8, 0x1801f99b0, 0x1801fa708,
0x1801f9a90, 0x1801f99e8 — none minted), walks the `ExternalFactory` AVL tree
once per id, releases each object through a vtable slot, then destroys the
`Video::Context`. Its internal callees are 0x1800c7a50, 0x1800c6c00,
0x180033880, 0x1800be9d0, 0x180006250, 0x18004f620, 0x1800c6970 and 0x1800f16e0
— **none ported**, plus two more singletons at 0x1801fc6c8 / 0x1801fc6f0.

A faithful port relocates ~17 nulls and removes none. That is exactly the shape
lane Z's triage rule exists to refuse.

### `?OnVideoRestart@MainLoop@Engine@@QEAAXXZ` (0x1800dc3b0)

~120 instructions, and its **first call is `CreateVideoContext` (0x1800dd500)** —
the unported, unexported function that is already the ctor's own open null (§2).
Porting `OnVideoRestart` cannot close that; it duplicates it. Also calls
0x1800bec10, 0x1800bc620, 0x180034440, 0x1800c0f10 and the allocator, none
ported.

**Both were "held back: needs a detour" in Wave 8.** That reason genuinely
cannot exist at cutover — there is no DLL body to detour past. What replaces it
is the ctor defect above, which is fixed, and the callee census here, which is
not.

### `?ClearResources@Context@Video@@QEAAXXZ` and `??1Context@Video@@QEAA@XZ`

**RESOLVE-ONLY rows. Verified, not taken from the comment that claims it.**
`grep -rn` across `runtime/ engine/ code/ standalone/ dx11/` for
`fn_VideoCtx_ClearResources` and `fn_VideoCtx_Dtor` returns, for each: the
`extern` declaration, the `extern` in the header, the assignment in
`engine_funcs.cpp` (1569 / 1622), and **one `printf` of the pointer value**
(3748 / 3765). No call site exists anywhere. Neither has a DLL-internal caller
either.

They appear in the residue because the **resolve** happens, not because anything
calls them. At cutover each is a null function pointer that is never invoked —
harmless. `context.cpp` §"A FOURTH IS HELD BACK" already reaches the right
conclusion for `ClearResources` for the right reason, and cutover does not
change it: a registered port on a path nothing executes reports `calls=0
divergences=0` forever.

### `?Startup@Device@Video@@QEAA_NXZ` (0x1800f7270 → 0x1800f8260) and `?Shutdown@Device@Video@@QEAAXXZ`

Declined. Wave 6's reasons are re-read and confirmed against the disassembly:
`Startup` copies the HWND from `Application::_singleton[0]` into `Device+0x90`,
calls the **private no-argument** `ValidateConfiguration` (0x1800f8830 — a
different function from the exported 8-argument one lane B ported), loops the
renderer interfaces through `CreateVideoInterface` (which `LoadLibrary`s a
plugin DLL and calls its `CreateDeviceData` — in this process **our own** dx11
factory, so a second run makes a second D3D11 device and swap chain), and on
success registers two USER32 message callbacks at 0x1800f7d20 (WM_SIZE) and
0x1800f7d90 (0x232) through `Application::RegisterMessageCallback`.
`Shutdown` is the mirror.

Their callees in *our* satisfaction terms:
`?RegisterMessageCallback@Application@1@…` and
`?UnregisterMessageCallback@Application@1@QEAAXI@Z` are **both unsatisfied**
(they are also two of the 21 remaining dx11 rows), `System::Error::SetText` is
unsatisfied, and the private `ValidateConfiguration` and `CreateVideoInterface`
are unported internals. Porting `Startup` moves the row at least five frames
deeper and needs `Application` and `System::Error` first.

**What that costs today, measured:** `fn_VideoDevice_Startup` /
`_Shutdown` / `_RecreateDynamic` / `_ReleaseDynamic` are called from exactly one
place — `code/Game/Dialog/optionsdialog.cpp:1137/1138/1168/1169` — and every
call is `if (fn) fn(vidDev);`. So at cutover **applying video settings from the
Options dialog silently does nothing** for the two still-unported halves rather
than crashing. `RecreateDynamic`/`ReleaseDynamic` there now work, because this
lane ported them.

**⚠️ A bigger question this lane opens and does not answer:** with the DLL gone,
`Engine::Startup` — which is itself held back and is what calls
`Device::Startup` inside the DLL — no longer runs. **Nothing in our tree starts
the video device.** That belongs to whoever owns `Engine::Engine`, and it is not
a `Video::Device::Startup` port that fixes it.

---

## 4. Audit numbers, before and after, with denominators

| Instrument | Before | After |
|---|---|---|
| `rawdoor_audit.py` | **481** sites, 0 UNGUARDED-CALL, 34 DEFERRED-CALL | **481** sites, **0** UNGUARDED-CALL, 34 DEFERRED-CALL |
| `port_bypass_audit.py --by-name` | **407** raw-door sites, BYPASS=**2**; **608** ported names, 396 statements, 0 missed by the parser | **407** raw-door sites, BYPASS=**2**; **621** ported names, 396 statements, 0 missed |
| `rt_data_door_audit.py` | **112** minted DATA symbols, **176** sites, 3 UNCLASSIFIED | **113** minted DATA symbols, **181** sites, 3 UNCLASSIFIED |
| `runtime_census.py` | TOTAL 4632 / called **846** / ported **595** / **70.3 %** | TOTAL 4632 / called **846** / ported **599** / **70.8 %** |
| `unsatisfied_export_scan.py` | **141** of 717 unsatisfied, 138 live | **134** of 717 unsatisfied, **131** live |
| dx11 import scan (§7, new) | **25** of 70 unsatisfied | **21** of 70 unsatisfied |

Reading the numbers honestly:

* `rawdoor_audit` is **unchanged in both columns**, which is the right result:
  this lane added no raw door and removed none. A moved denominator here would
  have meant I had silently deleted a site
  (`reference_documenting_a_check_can_delete_it.md`).
* The two BYPASS rows are the pre-existing allocator pair inside
  `RuntimeDllPresent`; untouched.
* `port_bypass`'s ported-name count moves **+13**, exactly the number of
  `RegisterRuntimeExport` calls added. The 13 `RegisterRuntimeAddr` calls are
  the *same* functions through the other registry and are correctly **not**
  counted again (`reference_census_three_blind_spots.md`).
* The census moves **+4, not +13**, because it counts only ports among the 846
  functions our tree *calls*. Nine of the thirteen have no by-name caller in the
  tree today — my own forwarders call them as C++ symbols. That is the
  `reference_uncalled_port_is_not_verified.md` shape and it is stated, not
  hidden.

---

## 5. Falsifiable prediction for the next DLL-less boot

1. The `MainLoop` ctor will **not** die at the two `Video::Device` +0x50/+0x58
   stores, and the boot will reach the `CreateVideoContext` call two lines
   later, where it will fault as `EXEC-of-0` with a return address inside
   `RtEngine_MainLoop_Ctor`. **If the fault is at the stores instead, my fix is
   wrong.** If the boot dies *before* `RtEngine_MainLoop_Ctor` on some earlier
   row, this prediction is untested rather than refuted.
2. The residue/`unsatisfied` list will **no longer contain** any of the thirteen
   names in §1. If `?ReleaseDynamic@Device@Video@@QEAAXXZ` or
   `?Lock@Device@Video@@QEAAXXZ` still appears, the registration is not running
   — check `[RtPort] Video: N exports registered` moved by 13, and that
   `BANISHED_RT_VIDDYNAMIC` is not set to 0.
3. `dx11`'s `GetImports()` for `contextdata.cpp` will **still return nullptr**,
   because five of its twenty entries remain unsatisfied:
   `?BindTextures@Context@Video@@AEAAXXZ`, `?BindResources@Context@Video@@AEAAXXZ`,
   `?Bind@RenderBuffer@Video@@QEBAXAEAVContext@2@@Z`,
   `?SetText@Error@System@@SAXPEBD_N@Z`, `?Display@Error@System@@SAXXZ`.
   **This lane did not close that table** — it removed four of nine blockers.
   Any claim that the renderer is cutover-ready is refuted by that list.
   `devicedata.cpp` is worse: seventeen entries still unsatisfied.
4. Once `Video::Device` exists and something puts a buffer on one of the four
   lists, `Device::ReleaseDynamic`/`RecreateDynamic` executing without a fault
   is the first behavioural evidence any of the eight walkers is right. Until
   then they are unexercised and I claim nothing about them.

---

## 6. Stated plainly: what I could not verify

* **Nothing here was executed.** No build, no run — the integrator holds the
  game directory. Every file is `g++ -fsyntax-only` clean with the Makefile's
  `standalone-debug` flags; that is a compile check, not a test.
* **The twelve walkers/forwarders have no oracle** and, per §1, cannot have one
  before cutover. Their correctness rests entirely on transcription from
  disassembly.
* **The device-lost path has never run in this project.** Both the new thunks
  and the whole `Release/RecreateDynamic` tree are unexercised code.
* **The link-offset disagreement** between retail's walkers and
  `Buffers.h`/`Device.h`/`ImageBuffer.h` field names (§1) is reported, not
  resolved. I used retail's offsets and renamed nothing.
* **`?Recreate@RenderBuffer@Video@@QEAAXXZ` calls the ported
  `ImageBuffer::Width`/`Height` where retail inlines the same ladder.** I argue
  they are the same computation from the disassembly; I did not diff them
  against retail's inlined copy, because there is no separate retail body to
  call.
* **The mint row I added is untested storage.** `_firstRenderBuffer` now has
  16 bytes of `kStorageBytes` behind it; nothing has written through it yet.

---

## 7. The dx11 import scan (reproduce §0)

Reuses `tools/unsatisfied_export_scan.py`'s `REG`, `MINT` and `strip_comments`
verbatim, so `ported`/`minted` are the same sets that script uses; only the
request side differs:

```python
ENTRY  = re.compile(r'"(\?[^"\s]+)"\s*,\s*0x[0-9a-fA-F]+')     # dx11 table rows
DIRECT = re.compile(r'Dx11ResolveRuntimeImport\s*\(\s*"(\?[^"\s]+)"')
```

walked over `dx11/**/*.{cpp,h}`. Kept as
`docs/runtime_port/wave9_laneVID_dx11scan.py`. It should be folded into
`unsatisfied_export_scan.py` proper — its blind-spot line currently says "2
wrapper resolves" and the true figure is 70 names, 21 of them still
unsatisfied after this lane.
