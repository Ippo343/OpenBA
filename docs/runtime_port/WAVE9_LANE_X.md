# Wave 9, Lane X — the fourteen singleton-publishing constructors

Closes lane C2's **tier 1** (WAVE9_LANE_C2.md §4): the minted storage rows whose
writer is that class's own constructor, and where the mint would otherwise hand
out zeroed storage nothing ever writes.

No build and no game run in this lane. Static tools, `objdump` against the
shipped DLL, and `-fsyntax-only` plus a real `-c` compile + `nm` symbol check of
every file written. The integrator owns the build.

**Delivered**

| File | What it is |
|---|---|
| `runtime/src/engine/singleton_ctors.cpp` | 13 ported constructors + 1 destructor, 15 registrations, and the probe |
| `runtime/src/engine/singleton_ctors_win.cpp` | the two Win32 calls Video::Device's ctor makes (PLAN §4.1 platform seam) |
| `runtime/funcs/rt_port.h` | one declaration: `RtSingletonCtorsPortInit()` |
| `standalone/resolve_standalone.cpp` | one call, between lane K and `RtDataMintInit()` |

Kill switch: `BANISHED_RT_SINGLETON_CTORS=0`.

---

## 1. All fourteen are ported and registered

| # | class | ctor (retail) | singleton slot | probed? |
|---|---|---|---|---|
| 1 | `System::MemoryAllocator` | `0x1800c79a0` (+ dtor `0x1800c7960`) | `?_singleton@MemoryAllocator@System@@` | yes |
| 2 | `IO::Modification` | `0x1800b04b0` | `?_singleton@Modification@IO@@` | yes |
| 3 | `Object::Factory` | `0x1800c09d0` | `?_singleton@Factory@Object@@` | yes |
| 4 | `Object::ExternalFactory` | `0x1800be7e0` | `?_singleton@ExternalFactory@Object@@` | yes |
| 5 | `Object::InternalFactory` | `0x1800c0030` | `?_singleton@InternalFactory@Object@@` | yes (partial, §4) |
| 6 | `IO::FileSystem` | `0x1800ac7c0` | `?_singleton@FileSystem@IO@@` | yes |
| 7 | `IO::Persistence` | `0x1800b8020` | `?_singleton@Persistence@IO@@` | **no** (§5) |
| 8 | `Video::Device` | `0x1800f7160` | `?_singleton@Device@Video@@` | yes (+ negative control) |
| 9 | `Video::Configuration` | `0x1800f6f10` | `?_singleton@Configuration@Video@@` | yes |
| 10 | `Application::Application` | `0x18009f590` | `?_singleton@Application@1@` | yes |
| 11 | `Input::Device` | `0x1800e13b0` | `?_singleton@Device@Input@@` | yes |
| 12 | `Input::Configuration` | `0x1800e0de0` | `?_singleton@Configuration@Input@@` | yes |
| 13 | `Audio::Device` | `0x1800de3a0` | `?_singleton@Device@Audio@@` | **no** (§5) |
| 14 | `Audio::Configuration` | `0x1800ddeb0` | `?_singleton@Configuration@Audio@@` | yes |

Row 1's BODY was already written — lane E's `rtalloc::Construct`/`Destruct` in
`runtime/src/system/allocator.cpp`, singleton store included. It had never been
REGISTERED, which is the only reason lane C2's writer audit still ranked it (c).
This lane registers it and does not duplicate it; the `nm` check confirms
`_ZN7rtalloc9ConstructEPN2rt6system15MemoryAllocatorE` resolves.

---

## 2. The acceptance criterion, and how it is shown

Every one of the fourteen reaches its slot through `RT_DATA_SLOT` — the
**port-aware** door (`RtResolveCached` → `ResolveExport`), never
`ResolveExportRaw` and never `ResolveAddr` on the .data address. That is what
makes the store follow the mint at cutover instead of writing the DLL's copy
forever.

Source alone is not evidence — lane E3's failure was a `Publish()` call that was
*present in the source* and skipped at runtime because the resolve returned null.
So the probe checks the store as its own comparison:

```
RT_DIFF_SHADOW(*slot == our object, "_singleton == our object",
               "the ctor must publish the slot")
```

per class, after our body runs. `?_singleton@Device@Video@@` gets a **second**
site asserting the opposite: retail's `CMOVE` publishes only when the
`DeviceType` argument is 0, so a port that published unconditionally would pass
every other check in this lane and silently hand the slot to a secondary device.

---

## 3. `calls=` is ZERO for all fourteen without the probe — and that is structural

Nothing in our tree calls any of these constructors. The callers are
`Engine::Engine` and `Engine::Startup` **inside the DLL**, which we do not
intercept. Without the probe, all fourteen would report the classic
clean-because-dead result (`reference_uncalled_port_is_not_verified.md`) and this
lane would have shipped fourteen unexecuted bodies.

The probe is therefore the lane's **effectiveness control**, and it prints one:

```
[RtPort] Wave9 lane X probe: N subjects executed, M published their singleton,
         control caught C of R corruptions
```

* `subjects executed` — expected **13**: the 12 probed classes of §1 plus the
  extra Video::Device `type=1` negative-control call. `calls=1` per RtDiff site,
  and 13 is the whole of this lane's live coverage.
* `published their singleton` — expected **12**: every probed subject except the
  deliberate `type=1` case, which must NOT publish.
* `control caught C of R` — **C must equal R** (expected **12 of 12**; the
  `type=1` site has no byte compare, so it runs no corruption). The control flips
  one byte of our copy after each compare and asserts the *same* memcmp reports a
  difference. Without it, a memcmp over two identically 0xCD-filled buffers is
  exactly the shape that passes when a size or a pointer is wrong.

Expected divergences: **0** at every site.

The probe runs only under `if (!RtDiffEnabled()) return;` — semantically right
(it is an oracle) and what keeps its `RT_ORIGINAL_ONCE` calls off a normal run's
path for `tools/rawdoor_audit.py`.

**Cells needed from the integrator**: one `BANISHED_RT_DIFF=1` run of any route
(the probe runs at init, so the menu is enough), reading the `[RtPort] Wave9 lane
X probe:` line and the `Wave9X::…` RtDiff rows.

---

## 4. Two things the byte oracle deliberately does not compare, and why

* **CRITICAL_SECTIONs.** `MemoryAllocator+0x640` and `Video::Device+0x60` are
  initialised by `InitializeCriticalSection`, which stores a `DebugInfo` pointer
  the kernel allocates per call. The two copies legitimately differ there, so the
  compare is split into the ranges around the lock. Both copies' locks are
  `DeleteCriticalSection`d rather than leaked.
* **`InternalFactory`'s Mersenne-Twister state.** Retail seeds `mt[0]` from
  wall-clock seconds and `0x6c078965` amplifies a one-count difference across all
  624 words. Compared instead: bytes `[0x00, 0x24)` (the two arrays and
  `Random::index`) against the shipped body, plus a check that the 623-word
  recurrence in OUR copy really follows from OUR `mt[0]`. Same rationale
  `value_types.cpp` already records for `??0Random@System@@QEAA@XZ` itself, whose
  ported body this ctor CALLS rather than transcribing a second time.

Everything else is a whole-object `memcmp`. Retail leaves real gaps —
Input::Device never writes `+0x08..+0x0F` of any of its 190 `State` records,
Video::Device never writes `+0x1c..+0x1f` or `+0x98`, Video::Configuration never
writes `+0x01..+0x03` — and both scratch subjects are pre-filled `0xCD`, so a
byte neither side writes compares equal on both sides. That is the *inverse* of
the Wave 6 trap (`reference_oracle_must_not_compare_unwritten_bytes.md`): there
the fill was one-sided.

---

## 5. The two that are registered but NOT probed, and the exact blocker

`IO::Persistence` and `Audio::Device` both embed a **worker thread**:

* Persistence's base call at `0x1800ca4e0` is `??0Thread@System@@QEAA@I@Z`
  (stack size `0x10000`).
* Audio::Device's call at `0x1800a0e30` is `??0Stream@Audio@@QEAA@XZ`, a
  `System::Thread` subclass that calls the same base.

That constructor `_beginthreadex`s a SUSPENDED worker, creates three events, and
resumes it. So a scratch-buffer probe would (a) start a real thread bound to a
fake object, twice per run, and (b) compare four kernel handle values that differ
between the two runs by construction. **Not "no oracle possible" — an oracle that
would have to stop and join two threads it started on stack-shaped objects, which
is not something to land untested from a lane that cannot run the game.**

They are registered anyway, and that is safe by inspection: registering only
redirects OUR call sites, and neither name has a call site in our tree today. So
the registration is inert on a live run and load-bearing only at cutover, where
it is the only thing that writes those two slots.

**Handover, two rows:**

1. `IO::Persistence`'s own vtable at `0x180155860` and Audio::Device's
   per-record vtable at `0x180157c60` are **not exports** (pefile finds no name)
   and are **not in lane C2's mint table**. Both are reached by
   `ResolveAddr(Module::Runtime, …)` and will be **null at cutover**. That is a
   mint row for lane C2 or a port for whoever takes IO::Persistence's virtuals;
   a silent `if (v)` here would only have hidden it. These two are the `+2` in
   the census `called` column below.
2. `??0Stream@Audio@@QEAA@XZ` is exported and unported. This body reaches it BY
   NAME so the call site follows a future port automatically.

---

## 6. One defect found in this lane's own first draft

`tools/port_bypass_audit.py --by-name` reported

```
BYPASS  runtime/src/engine/singleton_ctors.cpp:426
        0x1800ca4e0   ??0Thread@System@@QEAA@I@Z
```

The Persistence port reached its base constructor by ADDRESS, and
`System::Thread`'s constructor is **already ported** (lane B3/P,
`runtime/src/system/thread_win.cpp`). That is a port walking past a port — the
same class of defect Wave 9 lane R found in `graphics/scene.cpp`. Fixed by
reaching it through `RT_EXPORT_ONCE` by name, which also means the thread the
object gets is ours. The audit is back to 0.

**This is the argument for running the bypass audit on a lane's own new file
rather than only on the tree it inherited**: nothing else in this lane would have
caught it, and the failure mode at cutover is a DLL-internal call, not a null.

---

## 7. Gate results — with denominators, before and after

| gate | before lane X | after lane X | verdict |
|---|---|---|---|
| `tools/rawdoor_audit.py` | 473 sites, 8 UNGUARDED-CALL, 36 DEFERRED-CALL, exit 1 | **478** sites, **8** UNGUARDED-CALL, **36** DEFERRED-CALL, exit 1 | unchanged findings; +5 sites is this lane's 5 `RT_ORIGINAL_ONCE` spellings (1 macro definition + 4 hand-written), all guarded |
| `tools/port_bypass_audit.py --by-name` | ADDRESS door 189 sites, 0 bypasses | ADDRESS door **191** sites, **0** bypasses | one bypass introduced and fixed (§6); +2 sites are the two unexported vtables of §5 |
| `tools/rt_data_door_audit.py` | exit 0, 2 UNCLASSIFIED | exit 0, **2** UNCLASSIFIED (the same two: `audio.cpp:880`, `allocator.cpp:649`) | unchanged; none of this lane's 14 `RT_DATA_SLOT` reads is flagged |
| `g++ -fsyntax-only` (project flags) | — | clean on all 4 touched files | |
| `g++ -c` + `nm` | — | `rtalloc::Construct`/`Destruct` resolve | |

The eight UNGUARDED-CALL rows are the eight known false positives:
`rt_port.h:133` (the `RT_ORIGINAL_ONCE` macro DEFINITION) and seven
`ui/element.cpp` sites inside `if (replay.copy)`.

---

## 8. What the census says, and the correction it forces on lane C2's writer audit

`tools/runtime_census.py`:

| | before | after |
|---|--:|--:|
| `called` | 844 | **846** |
| `ported` | 589 | **589** |
| "ports with no caller in our tree" (exports) | 36 | **51** |

Read that carefully, because two of the three numbers are counter-intuitive and
both readings matter:

* **`called` grew by 2 and `ported` did not move.** `ported` counts only
  functions that are BOTH called by our tree and registered. All fifteen of this
  lane's registrations have no caller, so none of them can move that column. The
  `+2` is the two unexported vtables of §5 — a faithful port growing the cutover
  surface, exactly as `reference_porting_can_grow_the_cutover_surface.md` records.
* **"ports with no caller" grew by exactly 15**, which IS the visible evidence
  that all fifteen registrations landed. The census's own note says *"A NEW name
  appearing here has NOT been triaged"* — §3 above is the written verdict for all
  fifteen: they have no caller BY CONSTRUCTION (their callers are DLL-internal),
  and each is executed once by this lane's probe.

**Therefore `tools/rt_mint_writer_audit.py` still reports these rows as (c), and
that reading is now STALE.** The audit decides "is the writer ported" from
`runtime_census.py --csv`, and the CSV has one row per CALLED function — so a
port with no caller in our tree is invisible to it and reads as unported. The
audit is not wrong about anything it can see; it simply cannot see this shape,
which is the shape every one of lane C2's tier-1 writers necessarily has.

Not fixed here: `rt_mint_writer_audit.py` is a shared gate instrument several
lanes quote hourly, and changing its verdict function mid-wave is how a
denominator moves under other people. The change it wants is small — consult the
census's `ported_exports` set (or its "ports with no caller" list) in addition to
the CSV — and it belongs to lane C2, who owns the tool. Until it lands, the
authoritative answer for these fourteen rows is the table in §1 plus the probe's
`published their singleton` count, not the audit's `(c)` bucket.
