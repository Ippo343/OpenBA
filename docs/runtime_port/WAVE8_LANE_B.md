# Wave 8 — Lane B — `Engine::Engine` + `Engine::MainLoop`

Branch `wave/upgrade-path-complete`. Lane B of [WAVE8.md](WAVE8.md), following
[PLAN.md](PLAN.md) §4.2 wave 8 and the §6 recipe.

**Result: 3 of 11 ported, 8 held back with written reasons.** That is the
outcome WAVE8.md §3 predicted for the `Engine::Engine` half and it is *worse*
than predicted for the `MainLoop` half — `Run`, `OnVideoError` and
`OnVideoRestart` are held back too, for three different reasons, one of which
is the first genuine detour requirement this project has found.

Files:

| File | Status |
|---|---|
| `runtime/types/engine/Engine.h` | new, lane B owns it |
| `runtime/types/engine/MainLoop.h` | new, lane B owns it |
| both new headers | in `namespace RtEngine` — see §8, lane A caught a real collision |
| `runtime/src/engine/engine.cpp` | new, exports `int RtEngineCoreRegister()` |
| `engine/types/MainLoop.h` | **edited** — two disproved claims fixed at source (PLAN §6 step 8) |

Lane B did **not** create or edit `runtime/src/engine/rt_engine_port.cpp`,
`runtime/funcs/rt_port.h`, `standalone/resolve_standalone.cpp`,
`runtime/src/engine/loops.cpp` or `runtime/types/engine/UpdateLoop.h`.

**Every runtime number in this document is a PREDICTION** until the integration
run measures it. Lane B ran no build, no game, no `rt_run.sh`, no commit.

---

## 1. The function table

Addresses and ordinals are from the export directory of
`C:\GOG Games\Banished\Runtime-x64.dll`, read with `pefile` **before any body
was written** (`reference_resolveexport_phantom_names.md`). All eleven names
are present in the shipped DLL; lane B contributes none of the three phantom
names WAVE8.md §4 lists.

| Export | Retail | Ord | Ported | Oracle | Diff site name |
|---|---|---|---|---|---|
| `??0Engine@0@QEAA@PEAX@Z` | `0x1800dbf00` | 174 | **NO** | — | — |
| `??1Engine@0@QEAA@XZ` | `0x1800dbec0` | 586 | **NO** | — | — |
| `?Startup@Engine@1@QEAAIPEBDII_N@Z` | `0x1800db8a0` | 4094 | **NO** | — | — |
| `?Shutdown@Engine@1@QEAAXXZ` | `0x1800db4f0` | 4047 | **NO** | — | — |
| `??0MainLoop@Engine@@QEAA@PEAV11@@Z` | `0x1800dd6b0` | 263 | yes | shadow | `Engine::MainLoop::MainLoop` |
| `??1MainLoop@Engine@@QEAA@XZ` | `0x1800dd640` | 627 | yes | shadow | `Engine::MainLoop::~MainLoop` |
| `?SetNextLoop@MainLoop@Engine@@QEAAXPEAVUpdateLoop@2@@Z` | `0x1800dd7a0` | 3929 | yes | **replay** | `Engine::MainLoop::SetNextLoop` |
| `?Run@MainLoop@Engine@@QEAAXPEAVUpdateLoop@2@@Z` | `0x1800dd3c0` | 3601 | **NO** | — | — |
| `?OnVideoError@MainLoop@Engine@@QEAAXXZ` | `0x1800dc5e0` | 3291 | **NO** | — | — |
| `?OnVideoRestart@MainLoop@Engine@@QEAAXXZ` | `0x1800dc3b0` | 3295 | **NO** | — | — |
| `?_singleton@MainLoop@Engine@@1PEAV12@EA` | `0x1801fe750` | 4813 | n/a | — | DATA symbol |

Each port is registered by **both** the mangled name and the retail address
(`RegisterRuntimeExport` + `RegisterRuntimeAddr`), each name spelled as one
string literal on one line. Verified: `tools/runtime_census.py` after the file
landed reports `Engine 108 / 25 called / **3 ported**`, so the census can see
all three (`reference_census_cannot_see_nonliteral_registrations.md`).

Audits, run after the file landed:

* `tools/export_name_audit.py` → **643 distinct `ResolveExport` names, 1 not in
  the shipped DLL** — `?PlayEffect@Scene@Audio@@QEAI…` (lane C's). Note this is
  down from WAVE8.md §4's "644 names, 3 phantoms": the two `Save::SaveLoad`
  phantoms are already gone, presumably fixed by lane E running concurrently.
  Lane B added none. The four auxiliary export names lane B resolves by hand
  (`?_singleton@Device@Video@@0PEAV12@EA`,
  `?_secondsPerTick@Timer@System@@0NA`,
  `?SecondsToTicks@Timer@System@@SA_JM@Z`,
  `?_singleton@MainLoop@Engine@@1PEAV12@EA`) were each checked individually
  with `pefile`; all four exist (ordinals 4807, 4797, 3615, 4813).
* `tools/abi_retslot_audit.py` → **0 suspicious of 48** by-value-returning
  exports. Nothing in lane B's scope returns by value, so this was expected;
  it is recorded because the brief asked for the number.

Compile check (lane B does not build the project):

```
g++ -std=c++17 -Wall -Wextra -I. -c runtime/src/engine/engine.cpp   # -O0 and -O2
g++ -std=c++17 -Wall -Wextra -I. -c <both new headers>              # -O0 and -O2
g++ -std=c++17 -Wall -Wextra -I. -Iengine/types -c engine/types/MainLoop.h
```

All **warning-clean at both -O0 and -O2**, every `static_assert` passing.

---

## 2. The vtable verdict — **neither class is polymorphic**

WAVE8.md §2 records that `??_7Engine@Engine@@6B@` and
`??_7MainLoop@Engine@@6B@` are both absent from the export table, and is right
that absence is *consistent with* but not *proof of* a non-polymorphic class.
Lane B proved it from the ctors' first stores, as the brief required.

`Engine::Engine` (`0x1800dbf00`):

```
1800dbf1e  CALL 0x1800c79a0                    ; MemoryAllocator::MemoryAllocator(this)
1800dbf24  XOR EAX,EAX
1800dbf26  MOV qword ptr [RDI + 0x670],RAX     ; <- a ZERO, at +0x670
```

The first thing that happens is the embedded allocator's ctor on an unmodified
`RCX`; the first store the Engine ctor itself makes is a **zero at +0x670**.
Offset 0 belongs to the allocator subobject, whose own ctor's first store is
`ranges = nullptr`. No code address is written to offset 0 anywhere.

`Engine::MainLoop` (`0x1800dd6b0`):

```
1800dd6c6  XOR EAX,EAX
1800dd6c8  MOV qword ptr [RCX],RAX             ; <- a ZERO, at +0x00
```

and the **profile build's** ctor (`0x1800d5420`) opens with the byte-identical
zero store, so this is not a retail-only codegen accident.

No method of either class is virtual — every `Engine::Get*` and every
`MainLoop` accessor mangles `QEAA`/`QEBA`, not `UEAA`/`UEBA`. So **PLAN §4.3
risk 3 does not apply to lane B at all**, and a lane-B port can coexist with
un-ported DLL code operating on the same live object.

---

## 3. The detour verdict — **no detour installed, and this time it is not luck**

Waves 6 and 7 each expected to be the first user of
`standalone/runtime_detour.h` and each was not. Lane B measured rather than
assumed. `get_xrefs_to` over retail for every entry point in scope:

| Function | DLL-internal CALL sites |
|---|---|
| `??0MainLoop` `0x1800dd6b0` | **0** (refs are export table `0x18020ec98`, unwind `0x1801b0e60`, scope table `0x18019922c`) |
| `??1MainLoop` `0x1800dd640` | **0** |
| `?Run` `0x1800dd3c0` | **0** |
| `?SetNextLoop` `0x1800dd7a0` | **0** |
| `??0Engine` `0x1800dbf00` | **0** |
| `?Startup@Engine` `0x1800db8a0` | **0** |
| `?OnVideoError` `0x1800dc5e0` | **1** — `0x1800dcd27`, inside `MainLoop::OnVideoError_static` |
| `?OnVideoRestart` `0x1800dc3b0` | **1** — `0x1800dc5c7`, inside `MainLoop::OnVideoRestart_static` |

None of these rows is an `Unwind@` cleanup funclet masquerading as a call
(lane A's warning): for the three ported functions `get_xrefs_to` returns **no
code rows at all**, only DATA, so there was nothing to filter.

**For the three ported functions: no detour, because there is no DLL-internal
caller to detour.** These are exe entry points — `code/Game/Game/game_main.cpp`
and `engine/funcs/engine_funcs.cpp` are the only callers in the process, and
registering the export flips them. The reason is the same *shape* as Wave 6's
("only our call sites matter") but for a different underlying cause: Wave 6's
functions had internal callers and relied on state equivalence, whereas lane
B's simply have none.

### The first real detour requirement in the port

`OnVideoError` and `OnVideoRestart` each have exactly one internal caller: a
static thunk. Those two thunks are what the **MainLoop ctor installs into
`Video::Device::_singleton`** at +0x50/+0x58 —

```
1800dd75c  MOV RAX,qword ptr [0x1801fe7a8]     ; Video::Device::_singleton
1800dd763  LEA RCX,[0x1800dcd20]               ; MainLoop::OnVideoError_static
1800dd76a  MOV qword ptr [RAX + 0x50],RCX
1800dd76e  LEA RCX,[0x1800dc5c0]               ; MainLoop::OnVideoRestart_static
1800dd775  MOV qword ptr [RAX + 0x58],RCX
```

(the profile build names the setter: `Video::Device::SetVideoErrorFuncs`,
`0x1800fd860`) — and the device's own lost/restore path is the **only** thing
that ever calls them. So a port of either member that registered the export and
stopped there would be dead on the only path that reaches it, while the oracle
reported `calls=0 divergences=0`: precisely
`reference_uncalled_port_is_not_verified.md`.

Whoever ports those two must install the detour. Lane B's ported ctor keeps
installing **the DLL's own thunk addresses**, so until then that whole path
stays inside the DLL and is unaffected by this lane.

### `SetNextLoop` is INLINED at two DLL-internal sites — not a detour question

Lane A pointed at two more writers of `MainLoop+0x28`, and they check out
(`?Exit@LoaderLoop@Engine@@`, `0x1800dc130`):

```
1800dc130  MOV RAX,qword ptr [0x1801fe750]   ; MainLoop::_singleton
1800dc137  MOV RDX,qword ptr [RCX + 0x28]    ; this(LoaderLoop)->_nextLoop
1800dc13b  MOV qword ptr [RAX + 0x28],RDX    ; mainLoop->_nextLoop = ...
```

`?Update@LoaderLoop@Engine@@` (`0x1800dc2a0`) does the same. That is
`SetNextLoop` **inlined** — a direct store, with no CALL to intercept — which is
why the xref sweep showed zero internal callers and was still not the whole
picture.

It is *not* a detour requirement: a detour patches a function prologue, and
there is no prologue here to patch. It is also harmless — the DLL writes the
same field with the same semantics our port does, and `_nextLoop` is plain
storage with no invariant our body maintains. It is recorded because the
opposite conclusion is the tempting one: **nobody should later assume
`MainLoop::_nextLoop` is written only through this port.** Any future work that
wants to observe or validate every transition has to account for these two
inlined stores as well.

This also slightly changes what a `calls=0` on the SetNextLoop diff site would
mean — see §10.4.

---

## 4. The allocator embedding, and why it takes four functions with it

PLAN §4.3 risk 2 says `Engine::Engine` embeds the allocator at offset 0. Lane B
verified it rather than repeating it. Four independent proofs, all in
`runtime/types/engine/Engine.h`:

1. `?GetMemoryAllocator@Engine@1@…` (`0x18000c8b0`, ord 2782) is literally
   `48 8b c1 c3` — `MOV RAX,RCX; RET`. A bare `return this`.
2. The ctor calls the allocator ctor on an unmodified `RCX` and only then
   starts zeroing at `+0x670`; `sizeof(MemoryAllocator) == 0x670`, so the two
   abut with no gap.
3. The dtor tail-jumps to `DeleteCriticalSection` with `RCX = this + 0x640`,
   which is `MemoryAllocator::lock`.
4. **`Engine::Startup` writes three allocator fields directly** — and this is
   the part WAVE8.md did not anticipate:

```
1800db8d9  SHL RAX,0x14 ; MOV [RCX+0x10],RAX   ; _allocator.minRangeSize = arg2 << 20
1800db8e4  SHL RAX,0x14 ; MOV [RCX+0x18],RAX   ; _allocator.maxTotalSize = arg3 << 20
1800db8f0  CALL [0x18014b180]                  ; GetSystemInfo(&si)
1800db8f9  MOV [RDI+0x668],RAX                 ; _allocator.pageSize = si.dwPageSize
1800db905  CALL 0x1800c7690                    ; the allocator's own init
...
1800db87a  CALL 0x1800c75b0                    ; (Shutdown) the allocator's own shutdown
```

Wave 2's `runtime/types/system/MemoryAllocator.h` already labels `+0x010` and
`+0x018` "Startup arg1/arg2 << 20"; this is the other end of that attribution,
confirmed.

**So the NO-GO verdict on the allocator port (`WAVE2_ALLOCATOR.md`) reaches all
four `Engine::Engine` members, not just the ctor and dtor.** Startup and
Shutdown are not thin wiring around an untouched allocator; their bodies sit
inside the object the NO-GO covers, on the one path where being wrong destroys
the heap the whole process allocates from.

Neither has an oracle either. Replay is invalid by construction — running both
would start every subsystem twice or free every subsystem twice. A shadow would
have to replicate allocator state the NO-GO says we may not replicate.

### The `VideoSettings` arming bit — checked, and unchanged

The brief asked specifically whether a lane-B port would change when the
launcher-dialog arming bit is written, because `HarnessGuardUnattendedBoot`
(`standalone/main.cpp:184`) depends on retail's behaviour. Answer: **it is
written by `Engine::Shutdown`, not `Engine::Startup`, and both are held back,
so nothing changes.**

```
Startup  1800db8cf  MOV byte ptr [RCX+0x6f0],0x0     ; _startupComplete = 0, at entry
         1800dbe96  MOV byte ptr [RDI+0x6f0],0x1     ; = 1, only on full success
Shutdown 1800db50f  MOV RCX,[RBX+0x6b8]              ; cfg = _videoConfiguration
         1800db51b  CMP byte ptr [RBX+0x6f0],BPL     ; BPL == 0
         1800db522  SETZ AL
         1800db525  MOV byte ptr [RCX],AL            ; *(bool*)cfg = (_startupComplete == 0)
         1800db60e  CALL 0x1800f6d10                 ; Video::Configuration::Shutdown -> persists it
```

So `Video::Configuration` byte 0 is the in-memory arming bit, set from
`Engine+0x6f0` at the top of Shutdown and persisted by the video config's own
shutdown. Also worth recording for whoever touches this next:
`Engine::Startup` calls `PlatformConfigure` **itself**, at `0x1800dbda9`,
guarded by `this->_noConfigure == 0` (Startup's 5th argument, the `bool`) —
so the "skip the dialog" switch retail already has is Startup's own parameter.

---

## 5. The three ports, and the oracle argument for each

WAVE8.md §5 rules the Wave 1/2 replay oracle invalid for anything that mutates
shared state. Lane B argues per function.

### (a) `SetNextLoop` — **replay, and it is genuinely valid here**

The whole function is `MOV [RCX+0x28],RDX; RET`. The store is **idempotent**:
theirs writes the identical pointer to the identical field, so calling theirs
after ours is a no-op on the live object. That is the one case in this lane
where "run both and compare" does not mutate twice in any observable way.

The comparison is not tautological, either. A post-assignment
`self->_nextLoop == loop` would prove nothing; reading the field back *after
the DLL's own body ran* checks the thing that could actually be wrong — that
our store lands at `+0x28` and not somewhere else. (The compile-time half is
already covered by the `static_assert` in `MainLoop.h`.)

**Prediction:** several calls per loop transition, `divergences=0`.

### (b) `??0MainLoop` — shadow, four terms

Replay is invalid: running theirs afterwards would allocate a **second**
0x2168-byte `Video::Context`, leak the first, and re-stamp two globals.

The shadow checks four things the port did not compute directly:

1. `_deltaTime` against the DLL's exported
   `?SecondsToTicks@Timer@System@@SA_JM@Z` applied to the same float. **This is
   the real check** — it is the only arithmetic in the function, it is exactly
   what the float/double trap below would break, and the comparison value comes
   out of the DLL rather than out of us.
2. `_maxAccumulator`, likewise.
3. `MainLoop::_singleton == this`, read back through the exported data symbol,
   so a ctor that stamped the wrong global fails here.
4. `_context != nullptr` — `CreateVideoContext`'s post-condition.

Per `reference_shadow_predicate_must_name_its_failing_term.md` the verdict is
the AND of exactly those four terms and the log message names all four with
their individual booleans, so a divergence says *which* term failed.

This is weaker than a byte diff and the source file says so. It is not nothing:
every failure mode the disassembly suggests is covered by one of the four
terms. **Prediction:** `calls=1`, `divergences=0`.

### (c) `??1MainLoop` — shadow on post-conditions

Replay is invalid: running theirs afterwards would double-free the context and
the three buffers.

The shadow reads back, after our body, the post-conditions the DLL helpers we
called are responsible for producing: `_context == nullptr`, and all three of
`_videoResources` / `_postDraw` / `_postUpdate` with `_data == nullptr`,
`_size == 0`, `_capacity == 0`. None of those is written by lane B's own code
— they are written by the three Array destructors and by the context release —
which is what keeps it non-tautological. All three counts are asserted, not
just the pointers, because `0x180033880` ends by zeroing `_data` and then a
single qword covering both `_size` and `_capacity`.

**Prediction:** `calls=1` (process teardown), `divergences=0`.

---

## 6. Faithfulness details that a "tidier" port would get wrong

**The two clock constants are floats widened to double.** Retail:

```
1800dd723  MOVSD XMM0,[0x1801616a8]     ; 0x3f91111120000000
1800dd72b  DIVSD XMM0,[0x1801fe728]     ; System::Timer::_secondsPerTick
1800dd733  CVTTSD2SI RAX,XMM0
```

`0x3f91111120000000` is `(double)(1.0f/60.0f)`, **not** `1.0/60.0`; likewise
`0x3fb1111120000000` is `(double)(1.0f/15.0f)`. The profile build shows why —
its ctor does `MOVSS XMM0,[0x1801b04e4]` and `CALL SecondsToTicks(float)`, i.e.
the source wrote a `float` and the compiler widened it when it inlined the
call. Writing the decimal literals in the port would produce a different double
and a different tick count. The port keeps the float constants and widens.

**The ctor writes a WORD at +0x18 and never touches +0x1a..+0x1f.** A
`memset(self, 0, 0xa0)` would be wrong there, and wrong in a way no oracle in
this project could catch, because nothing reads those six bytes
(`reference_oracle_must_not_compare_unwritten_bytes.md`,
`reference_never_written_needs_by_whom.md`). The port writes field by field.

**The dtor's `_context = nullptr` is OUTSIDE the null check.** Retail
(`0x1800dd674`) stores it unconditionally, even when the context was already
null. Kept.

**Three different Array destructors.** `0x180033a40` (+0x90), `0x180033960`
(+0x80) and `0x180033880` (+0x00) are three distinct functions because the
element types differ — `0x180033880` destroys a `System::String` inside each
0x18-byte entry, the other two destroy a plain 0x10-byte POD pair. This is the
per-instantiation trap of PLAN §4.3 risk 1 showing up again
(`reference_one_bst_find_two_node_layouts.md`).

**Code vs data resolution is deliberately split.** Code dependencies go through
`ResolveAddr(Module::Runtime, …)`, which is registry-aware, so a later port of
`~Context` or `CreateVideoContext` is picked up automatically (goal.cpp's
stated position). Data globals go through `ResolveExportRaw` — GetProcAddress,
never the registry — because `ResolveAddr` checks the port registry first
(`reference_resolveaddr_consults_port_registry_first.md`) and for a data symbol
that is the wrong answer: what the ctor needs is the address of the global
**the DLL's own code reads**. A ctor that stamped our copy of
`MainLoop::_singleton` would leave every DLL-internal reader looking at null,
and nothing in the file would notice.

Ghidra's decompiled C was not used for anything load-bearing here; every claim
in this document cites disassembly
(`reference_ghidra_c_output_vs_disassembly.md`).

---

## 7. Layouts recovered

### `runtime/types/engine/Engine.h` — 0x6f8 bytes

Offsets 0x670..0x6e8 are **each named by an exported one-line accessor**
(`MOV RAX,[RCX+off]; RET`), read with `read_memory` over
`0x18000c8b0..0x18000c9b0`. That is the strongest evidence available: the
offset and the field's type come straight out of the export table.

`+0x000` embedded `MemoryAllocator` · `+0x670` `_fileSystem` · `+0x678`
`_persistence` · `+0x680` `_modification` · `+0x688` `_configuration` ·
`+0x690` `_application` · `+0x698` `_platformHandle` (ctor arg) · `+0x6a0`
`_factory` · `+0x6a8` `_internalFactory` · `+0x6b0` `_externalFactory` ·
`+0x6b8` `_videoConfiguration` · `+0x6c0` `_videoDevice` · `+0x6c8`
`_audioConfiguration` · `+0x6d0` `_audioDevice` · `+0x6d8`
`_inputConfiguration` · `+0x6e0` `_inputDevice` · `+0x6e8`
`_achievementManager` · `+0x6f0` `_startupComplete` · `+0x6f1` `_noConfigure`.

Note `_factory` (+0x6a0) and `_internalFactory` (+0x6a8) sit in the **opposite**
order to their accessors' addresses; the header follows the bytes.

**The class SIZE is the one thing inferred rather than read** — no retail site
allocates an `Engine` (`game_main.cpp` does), so `0x6f8` is `0x6f2` rounded up
to 8-byte alignment. The header says so.

Also recorded: the `Engine` singleton global is `0x1801fe748`, and the
allocator's own singleton is a **second** global `0x1801fc718`. Every
allocation site in the DLL reads `0x1801fc718` and adds `0x640` to reach the
lock, which is only coherent because both point at the same object.

### `runtime/types/engine/MainLoop.h` — 0xa0 bytes

Source file name recovered from the profile build's assert string
(`0x1800d54dc`): **`Engine\MainLoop.cpp`**, with an assert at line 40
(`_singleton == NULL`, *"Only a single instance of main loop may exist!"*) and
one at line 174 (`_singleton != NULL`, for the Video::Device). Retail compiles
both out.

Two new sub-types are pinned:

* `VideoResourceEntry`, 0x18 bytes — `{int32 _typeId; int32 _pad04; void*
  _object; char* _name}`. Built by `OnVideoError` (`0x1800dca4d`/`50`/`ac6`),
  walked by `OnVideoRestart` with `ADD RSI,0x18`.
* `MainLoopCallback`, 0x10 bytes — `{void* _func; void* _context}`. Both
  `AddPostDraw` (`0x1800dd040`, `ADD RCX,0x80`) and `AddPostUpdate`
  (`0x1800dcf60`, `ADD RCX,0x90`) build exactly this pair in a stack temp and
  hand it to Array::SetSize. The two arrays use different `Array`
  instantiations (`0x18004f790` vs `0x18004f8e0`) because the callback
  signatures differ, but the element width is the same.

---

## 8. What lane B reconciled, and what it fixed at source (PLAN §6 step 8)

`engine/types/MainLoop.h` (hook era) **got the size and all 21 offsets right**.
Two claims were wrong; both are corrected in that file, with a line saying what
was previously believed so the old claim is recognisable if met elsewhere:

1. **`_pad0C`, `_pad8C` and `_pad9C` are not padding — they are Array
   capacities.** `+0x00`, `+0x80` and `+0x90` are each a
   `Collection::Array<T>`. Proof: `~MainLoop` calls a distinct Array destructor
   on each, and `0x180033880` ends with `_data = 0` followed by a single qword
   zero covering both `_size` and `_capacity`. Code that grows either callback
   array reads the capacity; treating it as padding would reallocate on every
   append. The three fields are renamed and three new `static_assert`s added.
2. **`MainLoopAddr::IsRunning` was commented as a no-arg query.** The shipped
   export at `0x1800314b0` is
   `?IsRunning@MainLoop@Engine@@QEBA_NPEBVUpdateLoop@2@@Z` (ord 3158) and takes
   a `const UpdateLoop*`. The no-arg `IsRunning` is `UpdateLoop`'s, at a
   different address (`0x1800dd7b0`, ord 3160) — which is **lane A's**, and is
   listed under lane A in `wave8_lanes.txt`.

A third correction was **deliberately not made**: that header calls `+0x30`
`m_lastTimestamp` ("last QPC tick"). `Run` writes it once, before the frame
loop (`0x1800dd404`), which suggests "start", but lane B did not read
`MainLoop::Frame` (`0x1800dd100`) and so cannot say whether it is rewritten per
frame. The new header names it `_startTicks` and its comment states exactly
that limit rather than asserting a correction lane B did not verify.

### A third correction: lane B's own headers were at global scope (defect)

`runtime/types/engine/Engine.h` and `MainLoop.h` were first published at
**global scope**, which was wrong and lane A caught it. `engine/types/MainLoop.h`
defines a global `struct MainLoop` and `engine/types/UpdateLoop.h` a global
`struct UpdateLoop`; `engine/types` sits AHEAD of `runtime/types` on the include
path. So at global scope (a) any TU including both trees was a hard
redefinition, and (b) `_currentLoop`/`_nextLoop` pointed at a *different,
incomplete* `UpdateLoop` from the one lane A defines in `RtEngine` — it would
compile and then refuse to assign.

Both headers now live in **`namespace RtEngine`**, matching lane A's
`UpdateLoop.h` and the `RtVideo` / `RtObject` convention Waves 3 and 6 set
(`runtime/types/video/Context.h:131`, `runtime/types/object/External.h:37`).
`runtime/src/engine/engine.cpp` picks them up with `using` declarations and a
`namespace MainLoopAddr = RtEngine::MainLoopAddr` alias.

Verified, not assumed — a TU that includes `engine/types/MainLoop.h` plus both
lane B headers now compiles clean and can assign a `RtEngine::UpdateLoop*` into
`_nextLoop`.

**No defect was found in shipped or ported behaviour by this lane.** The three
items above are defects in our own tree — two documentation, one header
scoping — not port defects.

---

## 9. What this lane could NOT verify

* **`sizeof(Engine)`** — inferred from alignment, not read (§7).
* **`MainLoop::Frame` and `MainLoop::Update`** (`0x1800dd100`, `0x1800dd070`)
  were not decompiled. They are DLL-internal, unexported, and out of scope; but
  they are the reason `Run` is a holdback, and any later port of `Run` must
  take all three together.
* **`Engine::Shutdown`'s ~30 unexported callees** were identified by address
  and by the subsystem field each operates on, but not decompiled. The
  Shutdown holdback does not depend on them.
* **The predictions in §5 are predictions.** No lane-B code has executed.

---

## 10. What the integrator must wire

0. **Lanes A and B are now one unit: take both or neither.** Lane A's
   `runtime/src/engine/loops.cpp:94` includes lane B's
   `runtime/types/engine/MainLoop.h` and reads
   `MainLoopSingleton()->_currentLoop` through it instead of a hand-written
   `+ 0x20`. That is a good thing — our two independent derivations of +0x20 are
   now checked by the compiler rather than by two comments agreeing — but it
   means neither lane's files compile without the other's headers. Verified
   here: `loops.cpp` and `engine.cpp` each compile warning-clean at -O2 against
   the current tree.
1. `runtime/src/engine/engine.cpp` exports **`int RtEngineCoreRegister();`**
   (C++ linkage, matching `RtVideoContextRegister` et al.). Declare it in
   `runtime/src/engine/rt_engine_port.cpp` next to lane A's register function
   and call it from `RtEnginePortInit()` under the lane kill switch
   `BANISHED_RT_ENGINECORE` and the namespace switch `BANISHED_RT_ENGINE`,
   mirroring `runtime/src/video/rt_video_port.cpp`. It returns **3**.
2. Add `void RtEnginePortInit();` to `runtime/funcs/rt_port.h` and to the call
   list in `standalone/resolve_standalone.cpp` (lane B did not touch either).
3. Add `runtime/src/engine/engine.cpp` to the build.
4. Run the integration with `BANISHED_RT_DIFF=1` on the same command line as
   the pre-wave control (WAVE8.md "Pre-wave control") so the two compare, and
   check three new sites appear: `Engine::MainLoop::MainLoop`,
   `Engine::MainLoop::~MainLoop`, `Engine::MainLoop::SetNextLoop`. **Read the
   `calls=` column, not just `divergences=`**
   (`reference_uncalled_port_is_not_verified.md`): the two lifecycle sites
   should report `calls=1` each. `SetNextLoop` has exactly ONE caller in this
   tree — `code/Game/Game/gameupdateloop.cpp:1909`,
   `if (fn_MainLoop_SetNextLoop) fn_MainLoop_SetNextLoop(*g_pMainLoopSingleton,
   self->m_nextLoop)` — reached on a game-loop transition, so a low single-digit
   `calls` is expected and correct on the control's menu -> new game -> quit
   path. `calls=0` there would mean the registration did not reach
   `engine/funcs/engine_funcs.cpp:4401`'s resolve of `fn_MainLoop_SetNextLoop`,
   not that the port is idle. (Note `SetEngineNextLoop` in `engine_funcs.h` is a
   DIFFERENT, exe-side function on a different object — it is not this export
   and does not exercise this port. And per §3, the DLL ALSO writes
   `MainLoop+0x28` from two inlined sites in `LoaderLoop`, which this port never
   sees — so the diff `calls` count is a count of OUR call site, not of every
   `_nextLoop` transition in the process.)
5. Nothing in lane B needs a detour, and lane B installed none. If a later wave
   ports `OnVideoError`/`OnVideoRestart`, it must (see §3).
6. **`struct Time` is defined twice at global scope. Reachable, not currently
   hit anywhere, and NOT a Wave 8 blocker — but measure it before dismissing
   it.** `engine/types/common.h:12` and `runtime/types/system/Value.h:65` both
   define a global `struct Time`.

   This entry has been wrong twice, so here is the measured version. Lane B
   first reported it as live *through the engine headers*; lane A removed that
   vector by dropping a gratuitous `Value.h` include from `UpdateLoop.h`. Lane A
   then reported it as live through `Object::Internal` and likely to hit lanes C
   and E. Both statements were true when made and neither is the whole shape:

   * The **`Time`-definer side** is not one exe-tree file — the exe tree's
     `common.h` is pulled in by FIVE runtime headers:
     `math/Math.h:30`, `volume/Volume.h:23`, `graphics/Geometry.h:47`,
     `graphics/Mesh.h:25`, `picking/Scene.h:29` (all for `Vector2/3/4`), plus
     lane C's `runtime/src/audio/audio.cpp:69` directly.
   * The **`Value.h` side** is `object/Internal.h:37` (which genuinely embeds
     `::Random`, 0x9c4 bytes — so lane A's drop-the-include fix is NOT available
     there), `achievement/AchievementManager.h:30`, and `Value.h` itself.
   * So the collision is reachable through two entirely ordinary headers:

     ```
     printf '#include "math/Math.h"
#include "object/Internal.h"
int main(){}
' > w.cpp
     g++ -std=c++17 -I. -Iruntime/types -c w.cpp
     -> runtime/types/system/Value.h:65:8: error: redefinition of 'struct Time'
     ```

   * **And zero `.cpp` files under `runtime/src/` include both sides today** —
     swept all of them, count is 0. Lane C's `audio.cpp` is the closest: it
     includes the exe-tree `common.h` and is one `Value.h`-chain include away.

   Conclusion: nothing is broken now, nothing in Wave 8 needs to act, and the
   next person to add an ordinary include to the wrong `.cpp` gets a confusing
   error in a file they did not touch. The fix is namespacing one of the two
   `Time` definitions — files no Wave 8 lane owns.

7. `engine/types/MainLoop.h` gained three fields and three `static_assert`s.
   Nothing referenced the old `_pad0C`/`_pad8C`/`_pad9C` names (grepped across
   `engine/`, `code/`, `standalone/`, `dx11/`), so no call site changes — but
   the exe tree does need a rebuild of anything including it.

---

## 11. Ghidra

No renames were applied. `Engine_MainLoop_CreateVideoContext`,
`Engine_MainLoop_Update`, `Engine_MainLoop_Frame`,
`Engine_MainLoop_OnVideoError_static` and
`Engine_MainLoop_OnVideoRestart_static` already carry recovered names in the
retail program from earlier work, and they matched the export table and the
profile build wherever both could be checked. The one name this lane recovered
that Ghidra did not have is `Video::Device::SetVideoErrorFuncs`
(profile `0x1800fd860`, inlined in retail), which is recorded in
`runtime/types/engine/MainLoop.h` rather than applied as a retail rename, since
retail has no function there to rename.
