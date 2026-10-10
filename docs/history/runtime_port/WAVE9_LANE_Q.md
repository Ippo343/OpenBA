# Wave 9 — Lane Q: the `Collection::Array<T>` cluster (lane D's J1 and J3)

Produced 2026-09-07 on `wave/upgrade-path-complete`. **Nothing was built and the game was
not run** — lane R owns the exe and the build tree this round. What was run: `g++
-fsyntax-only` on the two new files (writes nothing, touches no object directory),
`tools/runtime_census.py`, `tools/export_name_audit.py`, `tools/abi_retslot_audit.py`,
`tools/resolve_fallback_audit.py`, `tools/port_bypass_audit.py`, and Ghidra MCP.

## Result

**27 of the 66 lane-D addresses retired for two bodies**, and **5 new lane-D rows created**
by the three instantiations that are not trivially copyable. Net −22 on the `internal`
bucket.

Measured, not asserted:

```
tools/runtime_census.py, before lane Q:   internal  64 called,  0 ported
tools/runtime_census.py, after lane Q:    internal  69 called, 27 ported (39.1%)
TOTAL                                     827 -> 832 called, 545 ported
tools/port_bypass_audit.py                ADDR-REGISTERED=27, 0 bypasses
```

Those two lines were taken minutes apart on a shared tree, so only the `internal` bucket is
attributable to this lane: **+27 ported, +5 called, all 27 of them registrations in
`array.cpp`**. A later reading shows `internal 70 / 39` because lane D2 landed some of the
destroy helpers §4 names; the extra 12 are theirs, not lane Q's.

The `called` column going UP by 5 while 27 were retired is
[the mechanism WAVE9.md describes](WAVE9.md) — a faithful port that calls DLL internals by
address moves work into the unnamed bucket. It is not a regression; it is the honest
accounting, and the five rows are named below so the next census does not have to
rediscover them.

## Files

| File | What |
|---|---|
| `runtime/src/collection/array.cpp` | both bodies, the 13-row parameterization table, the 27 registered entry points, `RtCollectionPortInit()` |
| `runtime/src/collection/array_internal.h` | the two things array.cpp and its probe must agree on: `ArraySpec` and the two body signatures |
| `runtime/src/collection/array_probe.cpp` | the oracle: synthetic-subject replay against the shipped bodies, with an effectiveness control and a negative control |
| `runtime/funcs/rt_port.h`, `standalone/resolve_standalone.cpp` | `RtCollectionPortInit()` declared and called **last** of the wave inits |
| 8 files under `runtime/src/` | comment corrections — see "Comments this lane made false" |

No `Makefile` change: `runtime/src/*/*.cpp` is globbed and the per-directory compile rule is
generated from what is on disk.

## Kill switches

* `BANISHED_RT_COLLECTION=0` — the whole lane; all 27 addresses stay on the DLL.
* `BANISHED_RT_ARRAY_NONTRIVIAL=0` — only the three non-trivial instantiations
  (`0x180004e30`, `0x180051390`, `0x180061480`), so an integration failure can be halved
  with one env var without disarming the 24 that carry no element semantics at all.
* `BANISHED_RT_ARRAY_PROBE=1|all|neg` — the oracle. Off by default.

---

# 1. Re-verifying lane D's clustering

Lane D proved the clustering by hashing **mnemonic sequences**. Lane Q re-read all 27 bodies
as **disassembly**, never the decompiled C
(`reference_ghidra_c_output_vs_disassembly.md` — three Wave 6 defects including a crash came
from Ghidra printing a `MOVSXD` as a zero extension, and `SetSize` contains two of them).

## J1 — CONFIRMED, and stronger than lane D claimed

All eleven bodies are identical **instruction for instruction**, and — this is the part lane
D did not claim — **in every resolved absolute operand**. Lane D wrote that they differ "only
in RIP-relative displacements"; the displacement bytes differ because the instructions sit at
different addresses, but all eleven resolve to the same four targets:

```
0x1801fc718   ?_singleton@MemoryAllocator@System@@
+0x640        its CRITICAL_SECTION
0x18014b390   EnterCriticalSection (IAT)
0x18014b388   LeaveCriticalSection (IAT)
0x1800c6c00   ?Free@MemoryAllocator@System@@QEAAXPEAX@Z
```

**So J1 has no parameters at all — not even a stride.** Freeing a buffer does not need to
know what is in it. One C++ function serves all eleven and there is nothing to get wrong per
instantiation. That is the single highest-leverage fact in this lane.

Body, from `0x18001c750`:

```
p = this->_data;
if (p) { Enter(cs); Free(_singleton, p); Leave(cs); }
this->_data = 0;
*(int64*)(this + 8) = 0;      // _size AND _capacity, as ONE eight-byte store
return 0;                     // RAX is zero at the RET
```

Three details a tidy-up would break, and all three are commented in the port: the
`[this+8]` store clears **capacity** too (unlike `SetSize`, which never shrinks it); `RAX` is
zero at the `RET`, so the port returns `void*`/`nullptr` rather than leaving GCC's choice in
the register; and the `MOV [RSP+0x20],-2` / `MOV [RSP+0x40],RBX` spills are the MSVC unwind
funclet's state and have no effect on the object.

## J3 — the algorithm is one algorithm; the **signature** is three

Lane D's brief says the 16 instantiations "collapse to one algorithm parameterized by stride /
alignment / element-assign". The algorithm claim is right and is reproduced verbatim in the
port. **The parameterization claim is incomplete in the one dimension that corrupts memory.**

### Correction 1 — the arity varies, and it is not a detail

| arity | addresses | shape |
|---|---|---|
| **3** | `180004e30`, `18001f990`, `180023830`, `18004f790`, `18004f8e0`, `180061480`, `180076900`, `18007a0d0` | `SetSize(this, int n, const T* fill)` |
| **2** | `180008b90`, `180008d60`, `180008ff0`, `18002fea0`, `180051390`, `180059140`, `1800764c0` | `SetSize(this, int n)` — **no `fill` at all** |
| **1** | `18007b960` | `SetSize(this)` — n **and** the fill value are folded to immediates |

`0x18007b960` is `Array<int>::SetSize(10000)` filling with `-1`, and the shipped body says so
in immediates: `CMP ESI,0x2710`, `MOV [RCX+8],0x2710`, `MOV [RCX+0xc],0x7530` (the folded
`3 * 10000`), `MOV dword ptr [RDX],0xffffffff`.

Registering the eight two-argument and one one-argument bodies behind a three-argument typedef
and dereferencing `R8` reads a register the caller never set. Their `else` arm never touches
`fill`, so the *ported* code is safe either way — but the contract is part of the port, and
this is a lane whose failures are wild writes with no proximate symptom
(`reference_internalfactory_swapremove_wild_write.md`,
`reference_load_wild_write_high_dword_7fc.md`).

### Correction 2 — three are NOT trivially copyable, and they create 5 new lane-D rows

Lane D's ranked list attributes new rows to J2 (+6) and J8 (+6) and none to J3. Three J3
bodies call unexported per-element helpers, and — unique to them — carry a **shrink-path
destroy branch** that does not exist in the other thirteen bodies:

| address | element | copy-assign | destroy-range |
|---|---|---|---|
| `0x180004e30` | `SpriteSheet` entry, 0x38 | **`0x180004220`** (new row) | **`0x1800059e0`** (new row) |
| `0x180051390` | `Audio::EffectSlot`, 0x40 | **`0x180051650`** (new row) | **`0x180052110`** (new row) |
| `0x180061480` | `{String*, ExternalPtr}`, 0x10 | inlined (String refcount) | **`0x180005930`** (new row) |

The two destroy helpers that were read (`0x1800059e0`, `0x180005930`) are String-releasing
loops — `DEC dword ptr [RAX-4]`, then `SUB` to the `str-8` block and free it — and both take
`(ignoredThis, base, count)`: they spill `RCX` to the shadow space and never read it, and
`0x180004e30` does not set `RCX` at all before calling. That is the same shape as
`reference_read_the_callee_before_holding_back_on_an_arg.md`.

The port calls all five by address, exactly as `KeyMap::ClearAll`, `KeyMap::RegisterInput` and
`Audio::Scene::Serialize` already call `Collection` internals. What is needed from them is
LAYOUT, not ports.

### Correction 3 — two instantiations leave the new tail UNINITIALISED

`0x180008b90` (`Array<bool>`) and `0x18002fea0` (`Array<int32>`) have an **empty `else` arm**:
elements `[oldSize, newSize)` are left holding whatever the allocation contained. The port
keeps that (a helpful zero would not be faithful, and would paper over an uninitialised read
somewhere upstream), and the oracle must not compare that extent — comparing it would fail
100% of cases and read like a catastrophic defect
(`reference_oracle_must_not_compare_unwritten_bytes.md`, four such gaps in Wave 6).

`runtime/src/achievement/achievement_manager.cpp:107` already recorded this asymmetry for the
bool case and asked whoever ported `Collection::Array` to preserve it. It is preserved, and
that comment now points at the port.

### Correction 4 — one instantiation leaves a hole INSIDE each new element

`0x180051390`'s default `EffectSlot` writes `+0x00`, `+0x08`, `+0x10`, `+0x18`, `+0x20`,
`0x3f800000` as a **dword** at `+0x28`, `+0x30` and `+0x38`. The element is `0x40` bytes, so
**`+0x2c .. +0x2f` is never written.** Any oracle that memcmps a freshly grown `_effects`
array will fail on four bytes of allocator fill per element. The spec carries that window and
the probe skips it.

### What was confirmed exactly as lane D read it

The algorithm at `0x18007a0d0`, including the two `MOVSXD`s, the signed capacity compare, the
`* 3` treble gated on `oldSize != 0`, the `start = (old == _data) ? oldSize : 0` `CMOV`, and
the per-iteration null test of the **computed element address**. Also confirmed: the growth
consequences already written into `runtime/types/collection/Collection.h` — capacity never
shrinks, the first append to an empty array allocates exactly 1, a bulk `SetSize(n)` allocates
exactly `n`, and appending one at a time walks 1, 6, 21, 66, 201.

---

# 2. Parameterization table

`stride` is the `SHL`/`IMUL` at the `MemAlloc` call site, `align` the immediate in `EDX`.
"pre-zero" means the element assignment stores a zero into the slot before storing the value —
a smart-pointer `operator=` inlined; reproduced rather than folded, because
`BANISHED_REDZONE=watch` and a hardware write watchpoint can see the difference.

| address | arity | stride | align | element assign | new tail | destroy | spec |
|---|---|---|---|---|---|---|---|
| `180008b90` | 2 | 1 | 1 | byte | **untouched** | — | `kSpec_Bool` |
| `18002fea0` | 2 | 4 | 4 | dword | **untouched** | — | `kSpec_I32` |
| `180008ff0` | 2 | 8 | 8 | qword | zero | — | `kSpec_U64Zero` |
| `180008d60` | 2 | 8 | 8 | pre-zero + qword | zero | — | `kSpec_PtrZero` |
| `1800764c0` | 2 | 8 | 8 | pre-zero + qword | zero | — | `kSpec_PtrZero` |
| `180059140` | 2 | 0x1c | 4 | 7 dwords | `{0,0,0,1.0f,0,0,0}` | — | `kSpec_Transform` |
| `18007b960` | 1 | 4 | 4 | dword | `-1` (n folded to 10000) | — | `kSpec_I32Neg1` |
| `18001f990` | 3 | 8 | 8 | qword | `*fill` | — | `kSpec_Fill8` |
| `180023830` | 3 | 4 | 4 | dword | `*fill` | — | `kSpec_Fill4` |
| `18004f790` | 3 | 0x10 | 8 | 2 qwords | `*fill` | — | `kSpec_Fill16` |
| `18004f8e0` | 3 | 0x10 | 8 | 2 qwords | `*fill` | — | `kSpec_Fill16` |
| `180076900` | 3 | 8 | 8 | pre-zero + qword | `*fill` | — | `kSpec_Fill8Z` |
| `18007a0d0` | 3 | 8 | 8 | pre-zero + qword | `*fill` | — | `kSpec_Fill8Z` |
| `180004e30` | 3 | 0x38 | 8 | `CALL 0x180004220` | `*fill`, same call | `0x1800059e0` | `kSpec_Sprite38` |
| `180051390` | 2 | 0x40 | 8 | `CALL 0x180051650` | inlined, **hole at +0x2c** | `0x180052110` | `kSpec_EffectSlot` |
| `180061480` | 3 | 0x10 | 8 | inlined + `INC [str-4]` | `*fill`, same path | `0x180005930` | `kSpec_StringExt` |

`0x180059140` is the same 2-argument body **4× unrolled by the optimizer** — 233 instructions,
which is why lane D's instruction-count column made it look like an outlier. The unrolled
copies and the scalar remainder tail all do the identical seven-dword copy and identity fill.

`0x180008d60` and `0x1800764c0` are byte-identical to each other, as are `0x180076900` and
`0x18007a0d0`, and `0x18004f790` and `0x18004f8e0`.

## The allocator boundary

The whole cluster's only external dependency is the exported pair, and the port reaches it
through them rather than reproducing the inlined lock:

* `?MemAlloc@…SAPEAX_KH@Z` (`0x1800c7a50`) = `Enter; Allocate(_singleton, n, align); Leave`.
* `?MemFree@…SAXPEAX@Z` (`0x1800c7ad0`) — **disassembled for this lane** and found to be
  `if (p) { Enter; Free(_singleton, p); Leave; }`, **instruction for instruction the tail of
  every J1 body** plus the null guard J1 does itself. Calling it IS retail's body minus the
  inlining, and it keeps `<windows.h>` out of `runtime/src/` per PLAN §4.1 — which is the
  reason `runtime/src/picking/scene.cpp` had to leave one such free in the DLL.

Both are called **by address, not by name**, following `runtime/src/graphics/scene.cpp:206`:
Wave 2 registers ports under both mangled names behind `BANISHED_RT_ALLOC`, which carries a
standing NO-GO. Going by name would make these buffers come from whichever allocator the
switch selected while the DLL's own code keeps freeing them into the other one. **A heap
cannot be half-ours.**

> **LANE E DEPENDENCY.** At cutover `ResolveAddr(Module::Runtime, 0x1800c7a50)` resolves
> nothing. `rtcoll::MemAlloc` / `rtcoll::MemFree` must be repointed at lane E's allocator in
> the same commit that arms it. They are deliberately the only two places in `array.cpp` that
> name a Runtime address other than the 32 this lane deals in, and `tools/port_bypass_audit.py`
> will start flagging them the moment lane E registers the allocator by name — which is the
> intended alarm, not a false positive.

---

# 3. Oracle plan

## 3.1 Why the obvious oracle does not work, and what replaces it

A destructor and an in-place resize cannot be replayed at a live call site: ours then theirs
frees the same buffer twice, and for `SetSize` the second run starts from the state the first
left, so it is not the same experiment. Lane D proposed snapshot/restore.

**The replacement is a synthetic subject.** `SetSize` is a pure function of
(`_data` contents, `_size`, `_capacity`, `n`, `*fill`) and `~Array` is a pure function of the
16-byte header, so both can run on private Arrays: build two with identical headers and
identical buffer contents, run **ours** on one and the **shipped body** on the other, compare.
Two subjects, two buffers, two frees — no double free, no shared state, and the comparison is
against the DLL rather than against a restatement of our own rule. Same idea as lane S's two
independent sinks, and the shape
`reference_synthetic_subject_beats_a_fake.md` recommends.

**The shipped body is reached through `ResolveRuntimeRaw`, never `ResolveAddr`.**
`ResolveAddr` consults the port registry first
(`reference_resolveaddr_consults_port_registry_first.md`) and this lane registers all 27
addresses, so `ResolveAddr` would hand back our own function and the probe would compare ours
to ours and always agree.

**The probe self-schedules.** It is *armed* by `RtCollectionPortInit()` and *runs* on the first
call into any registered entry point. It must not run at init: `RuntimePortInit()` is called at
`standalone/main.cpp:1655`, right after `ResolveInit()`'s `LoadLibrary`, and the DLL's
`MemoryAllocator` is constructed but **not started** at that moment — `Engine::Startup` does
that later. The first real Array call is a moment we know is safe, because the caller is itself
in the middle of using an Array.

## 3.2 What is compared, and what is excluded

| Compared | Excluded, and why |
|---|---|
| `_size`, `_capacity` | `_data` — two independent allocations, two addresses |
| element bytes `[0, newSize)`, **minus the window of whichever arm wrote each element** (§3.4b) | `[newSize, capacity)` — never written by either side; capacity does not shrink |
| | `[oldSize, newSize)` for `kSpec_Bool` and `kSpec_I32` — retail's `else` arm is empty, so *no* arm wrote them |
| | `+0x34..+0x37` of every element the loop touched for `kSpec_Sprite38` (**assign** arm) |
| | `+0x2c..+0x2f` of every element the loop touched for `kSpec_EffectSlot` (**both** arms) |
| | the return value — `SetSize` leaves `RAX` unspecified at the `RET` |

Every exclusion in that right-hand column is itself checked by `VerifyUnwritten()` on
non-reallocating cases — see §3.4b. An exclusion is a claim about the shipped body, and this
lane makes the probe prove it rather than assert it.

For `~Array`, the compared extent is the whole 16-byte header. The **free** is not visible to
the comparator and is covered by pairing the run with `BANISHED_REDZONE=watch`.

## 3.3 The case matrix (9 cases × 16 J3 instantiations, 3 × 11 for J1)

| case | pre-state | what it pins |
|---|---|---|
| `noop(n==size)` | 0/0 → 0 | the exact-equal early return; counted as **ineffective** |
| `append-from-empty` | 0/0 → 1 | **capacity must become 1, not 3** — the `oldSize != 0` gate |
| `append-nonempty` | 1/1 → 2 | capacity trebles to 6 |
| `grow-within-capacity` | 2/6 → 5 | no realloc; only the tail is filled |
| `bulk-grow-realloc` | 5/6 → 20 | **capacity exactly 20**, because `20 != 5+1` |
| `append-at-capacity` | 20/20 → 21 | capacity 63 |
| `shrink` | 20/20 → 3 | no realloc, capacity kept, shrink-path destroy |
| `shrink-to-zero` | 5/6 → 0 | |
| `bulk-from-empty` | 0/0 → 4 | capacity exactly 4 |

J1: `populated` (4/8), `empty-allocated` (0/4), `null-buffer` (0/0 — must not reach `Free`).

## 3.4 Controls

**Effectiveness.** Every case reports, from the **pre-state** (so it does not depend on either
implementation being right), whether it reallocated, ran the copy loop, ran the fill loop and
freed. The summary prints `did-work` per address and a totals line; if `did-work` is zero the
probe prints

```
[RtArrayProbe] *** EFFECTIVENESS CONTROL FAILED: every comparison was a no-op.
This run proves NOTHING. ***
```

This is `reference_probe_needs_an_effectiveness_control.md` — Wave 8.5 had 64 green
comparisons of which zero did any work.

**Negative.** `BANISHED_RT_ARRAY_PROBE=neg` additionally runs a deliberately wrong body — the
growth rule with the `oldSize != 0` guard dropped, so an append to an empty array trebles —
through the same comparator. It must be flagged on `append-from-empty` and `bulk-from-empty`.
If it is not:

```
[RtArrayProbe] *** NEGATIVE CONTROL FAILED: a body with the growth rule broken
compared EQUAL. The comparator is broken, so the green result above is worthless. ***
```

Wave 8.5's negative control found a defect in the **probe**, not the port. This one is aimed at
the same place.

## 3.4a A defect the probe HAD, and the rule it produced

Fixed 2026-09-07 after the integration run. Recorded rather than quietly patched, because the
lesson is about diagnostics, not about arrays.

The first draft passed a **stack-local `rtcoll::Site`** from inside the probe's per-case loop
into `SetSizeCore`. `RtDiffRegister` keeps the **pointer** in `rt_diff.cpp`'s `g_sites[]` and
re-reads it at `RtDiffSummary()` time, so all 16 × 9 = **144 of them were dangling** by then.
The summary did not print rubbish; it printed

```
[RtDiff] ==== 144 of 416 sites were CORRUPTED — a stray write hit the harness's own statics ====
```

which is **the loudest diagnostic this harness has and names this project's worst failure
family** (`reference_internalfactory_swapremove_wild_write.md`,
`reference_load_wild_write_high_dword_7fc.md`). It cost the integrator most of an escalation.

**The tell, worth writing down because it generalises:** all 144 rows showed the *same*
address, the *same* name pointer and the *same* impossible counters, and that address was on
the **stack**. *N identical rows is one dangling pointer read N times, not N corrupted
statics* — corruption from a stray write is scattered, not uniform. The A/B settled it in one
run: without `BANISHED_RT_ARRAY_PROBE`, 271 sites and no corruption; with it, 416 and 144.

**The fix is that the probe registers nothing at all.** `SetSizeCore` and `ArrayDtorSited` now
accept a NULL `site` meaning "run the body, touch nothing in the diff registry", and the probe
passes NULL. A file-scope site would have fixed the dangling pointer and still been wrong: the
probe has its own counters, its `Compare()` is strictly stronger than the live checks, and its
traffic does not belong in a registry that reports on the *live* run.

**Two facts about `rt_diff.cpp` that this surfaced**, checked and worth carrying forward:

* Registration is an **unsynchronised** `g_sites[g_siteCount++]`. Anything registering sites in
  bulk at runtime is racing it.
* It is bounds-checked against `kMaxSites = 1024` by **silently dropping** anything past the
  cap. 416 observed sites is not near it and 27 static sites from this lane cannot move it —
  but 144 dynamic registrations per run were headed that way, and a silent overflow there would
  look exactly like corruption too.

**The rule, now stated in both source files:** *a `RtDiffSite` must have static storage
duration.* A harness that cries wild-write for a benign cause is worse than one that says
nothing.

## 3.4b Written extents belong to an ARM, not to a type — and the seed that hid it

Fixed 2026-09-07, on a latent defect lane D2 found while porting the destroy helpers this
cluster calls. **It had not bitten, and it would have** — the day anything seeded a non-zero
element, four bytes neither side writes would have been compared and the probe would have
reported a catastrophic-looking mismatch on a correct port.

### The defect

`0x180004220`, `kSpec_Sprite38`'s copy-assign, refcount-assigns the two `String` pointers at
`+0x00`/`+0x08`, copies nine dwords at `+0x10 … +0x30`, **and stops**. The element is `0x38`
bytes, so **`+0x34..+0x37` is never written**. The spec recorded no window.

That offset is not arbitrary: it is exactly `runtime/types/ui/SpriteSheet.h`'s **`_pad34`**.
Retail's own copy-assign does not copy the padding — which independently confirms the field
really is padding, and is a nice corroboration of a header that Wave 7 lane F had to correct
once already.

### Why the field was in the wrong place, which is the general lesson

The spec's window was documented as **the INIT arm's** extent, and this is **the ASSIGN arm**.
Those are two different code paths with two different written extents — precisely
`reference_never_written_needs_by_whom.md`, *a written-extent constant belongs to a WRITER,
not to a type*. And `kSpec_Sprite38` has **no init arm at all**: it is the three-argument
form, so the new tail goes through `assign` with `fill` too. Its window therefore had to be
excluded for **every element the loop touched**, not just `[oldSize, newSize)`.

`ArraySpec` now carries **two** windows, `assignUnwritten*` and `initUnwritten*`, and
`Compare()` decides per element which arm produced it:

| element | arm |
|---|---|
| `i < start` | untouched by the loop — compare in full (identical seed) |
| `i < oldSize` | **assign**, from the old buffer |
| `i >= oldSize`, a fill was passed | **assign**, from `*fill` |
| `i >= oldSize`, spec has an `init` | **init** |
| `i >= oldSize`, neither | **nothing written** — skip the whole element |

### Re-reading all 16 turned up a second one

Since the field was wrong, every spec's extent was re-derived rather than re-labelled.
`0x180051650` (`kSpec_EffectSlot`'s assign) was disassembled for this: dwords at `+0x00`,
`+0x04`, `+0x08`, `+0x0c`, qwords at `+0x10`, `+0x18`, dwords at `+0x20`, `+0x24`, `+0x28` —
then straight to `+0x30`. **It skips `+0x2c..+0x2f` too.**

So `EffectSlot`'s window is the same for both arms — but for two *independent* reasons, which
is now a fact rather than an assumption. (It finishes by zeroing `+0x30`/`+0x38` and calling
`0x180050d30` on the sub-object at `+0x30`, so the tail is fully initialised before that call
and has no window of its own.)

The other fourteen were checked and have none: every hand-written `assign` in the port covers
its full stride (`Assign1c` is 7 dwords = `0x1c`, `AssignStringExt` writes both qwords of
`0x10`), and every `init` except `InitEffectSlot` does too.

| spec | assign unwritten | init unwritten | evidence |
|---|---|---|---|
| `kSpec_Sprite38` | **`+0x34..+0x37`** | — (no init arm) | `0x180004220` stops at `+0x33`; `SpriteInfo::_pad34` |
| `kSpec_EffectSlot` | **`+0x2c..+0x2f`** | **`+0x2c..+0x2f`** | `0x180051650` and `InitEffectSlot`, read separately |
| all 14 others | none | none | the arm covers the full stride |

### The seed, and why all-zero was the problem

The reason this stayed latent is that the probe seeded the three non-trivial targets with
**all-zero elements** — the one seed under which an unwritten-bytes bug cannot be seen, because
both sides read zero in the window and agree by accident.

That blanket flag is gone. Which bytes must stay zero is a property of the **element type**, so
it now lives in `ArraySpec::seedZeroLo/Hi` and covers only the slots the element operations
actually dereference; everything else is seeded with a varying non-zero pattern based on `0xCD`:

| spec | seeded zero | why | seeded non-zero |
|---|---|---|---|
| `kSpec_Sprite38` | `+0x00..+0x0f` | both `String*`, refcounted by the assign and released by `0x1800059e0` | `+0x10..+0x37` — which is what makes the `+0x34` window testable |
| `kSpec_StringExt` | `+0x00..+0x07` | the `String*` | `+0x08`, the `ExternalPtr`, copied verbatim and never dereferenced |
| `kSpec_EffectSlot` | `+0x30..+0x3f` | the sub-object goes to `0x180050d30` and to `0x180052110`'s free | `+0x00..+0x2f` |

**A varying pattern, not a constant `0xCD`.** The idiom's value is that "`0xCD` surviving in
theirs proves retail never wrote there", but a *constant* fill cannot detect a copy that reads
the wrong element or the wrong offset — every byte looks the same. The pattern is
`0xCD + element*31 + offset*7`, which keeps both properties, and the fill argument uses element
index `0xFF` so a byte copied *from the fill* is distinguishable from one copied from the old
buffer.

### And the exclusions are now tested, not trusted

An exclusion that is *wrong* hides a real defect rather than a false one — the more dangerous
direction, and not something disassembly alone keeps honest as the port changes. So
`VerifyUnwritten()` closes the loop: on **non-reallocating** cases, where the destination *is*
the seeded buffer, every excluded byte must still hold its seed afterwards.

```
[RtArrayProbe] UNWRITTEN WINDOWS: <n> excluded windows verified as genuinely
               unwritten, <m> turned out to be written
[RtArrayProbe] *** A window excluded by an ArraySpec IS written by its arm. The spec
               is wrong, and every comparison of those bytes has been silently
               skipped. Fix the spec, do not widen the window. ***
```

Not checkable on a realloc: the destination is a fresh `MemAlloc`, so there is no seed to
survive. **This also means the `+0x34` and `+0x2c` windows are only positively verified on the
`grow-within-capacity` and `shrink` cases** — which is why those cases are in the matrix.

### Two lane-D2 corrections to §4, accepted

* `0x180052110` was filed here as *"PORT with the Audio holdbacks"*. **Wrong** — it is an
  ordinary destroy-range body, `offs{0x30}`, stride `0x40`, calling nothing but the allocator,
  and is not gated on Audio. Its sibling `0x180051650` genuinely is (it calls `0x180050d30`).
  §4 is corrected.
* *"The three String-releasing helpers are likely one body with three strides"* — right about
  the two that matter, wrong that all three are one body. One of the three is a different body.

## 3.5 Live instrumentation — a coverage counter, stated as such

Each of the 27 addresses owns its own `RtDiffSite`, so the `RtDiff` summary reports `calls=`
**per address**. That distinction is the whole risk of this lane: eleven addresses share one J1
body, so a run that exercises the body proves nothing about the other ten *registrations*.

Two checks run at a live call site:

* **Header algebra** — `_size == n` and `_capacity` equals what retail's rule predicts from the
  pre-state. **Weak by construction**: it restates the rule the function implements, so a port
  that got the rule wrong would agree with it. It is kept for its `calls=` value.
* **Copy prefix** — on a realloc, before the old buffer is freed, `memcmp` the copied prefix
  against the old buffer. This *is* independent: it compares against the source data rather
  than against our own rule. Gated on `spec->byteCopy`, so it is skipped for the two specs whose
  assignment is an opaque call.

**Do not read a green `[RtDiff]` summary as a verified port.** The independent oracle is
`[RtArrayProbe]`.

## 3.6 The runs

Each command assumes a debug or perf exe built by lane R and run from
`C:\GOG Games\Banished`; read `standalone_error.log` for `[RtArrayProbe]` and
`standalone_debug.log` for `[RtDiff]`.

| # | Command | What it covers |
|---|---|---|
| **Q1** | `BANISHED_RT_ARRAY_PROBE=neg BANISHED_RT_DIFF=1 BANISHED_AUTOSTART=1 BANISHED_AUTOQUIT=1 make sa-run` | **The oracle itself.** 9×16 + 3×11 = **177 comparisons** against the shipped bodies, with both controls. This is the run that verifies the two BODIES. Expect `mismatches=0`, `did-work` ≈ 150, and the negative control flagged in ≥ 2 cases per instantiation. `AUTOQUIT` quits at the **main menu** (WAVE9.md's recorded trap) — that is fine here, because the probe does not need gameplay, and the clean quit reaches the `~Engine` J1 sites. |
| **Q2** | `BANISHED_RT_DIFF=1 BANISHED_AUTOSTART=1 BANISHED_PAUSEMENU=900 BANISHED_PAUSEMENU_QUIT=120 make sa-run` | **Per-address `calls=`, through gameplay and a clean quit.** This is the run that says which REGISTRATIONS are live. |
| **Q3** | `BANISHED_RT_DIFF=1 BANISHED_AUTOTUTORIAL=1 make sa-run` | Scene teardown **mid-run** rather than only at exit: the tutorial chain reloads the world, which drives the `Graphics::Scene` / `Picking::Scene` / `Renderer` J1 dtors while the process is still alive. |
| **Q4** | `BANISHED_RT_DIFF=1 BANISHED_AUTOMENU_BUTTON=8 make sa-run-menu` | The Achievements front-end: `0x180008b90` / `d60` / `ff0` and `0x180004890`. |
| **Q5** | `BANISHED_RT_DIFF=1 BANISHED_REDZONE=watch` on Q2 | **The free half of the J1 oracle.** The comparator cannot see a wrong pointer handed to `Free`; the allocator watchdog can. |
| **Q6** | `make sa-stress` with `BANISHED_RT_DIFF=1` | Volume — `0x180076900`, `0x180059140`, `0x18001f990` are the hot ones. |
| **Q7** | `BANISHED_RT_STRICT=1` on Q2 | **The cutover check.** All 27 addresses must have LEFT `[RtResidue]`, and `rawaddr:` entries for the 5 new helpers plus `0x1800c7a50` / `0x1800c7ad0` must have JOINED it. |
| **Q8** | Q2 with `BANISHED_RT_COLLECTION=0`, and again with `BANISHED_RT_ARRAY_NONTRIVIAL=0` | The A/B. If Q2 shows a new failure, these two say whether it is this lane and whether it is the non-trivial three. |

## 3.7 Per-address coverage — what these runs would and would NOT reach

This is the part the brief asked to be stated plainly. **The probe (Q1) verifies the BODY for
all 27 by construction**, because it drives every instantiation's spec directly. What varies
per address is whether the **registration** is exercised by a real caller — which is what
`calls=` reports and what a `calls=0` line means.

| address | live caller | reached by |
|---|---|---|
| `18001f990` | `Geometry::Attach` | Q2, Q3, Q6 — hot |
| `180076900` | `Element::AddChild` | Q1 (menu UI), Q2, Q4, Q6 — hot |
| `180059140` | `Picking::Scene::AddMesh` scratch | Q2, Q3, Q6 — hot |
| `1800739e0` | `AddMesh` scratch free **and** `Picking::Scene` dtor | Q2, Q3, Q6 — hot |
| `180004e30` | `SpriteSheet::AddSpriteInfo` | Q1 — boot resource load |
| `180008b90`, `180008d60`, `180008ff0` | `AchievementManager::SetAchievements` | Q1 (boot), Q4 |
| `180051390` | `Audio::Scene` ctor, `SetSize(0x1000)` | Q1 — boot |
| `180061480` | `Material` texture array | Q1 — boot |
| `18007a0d0` | dialog pending-list push | Q1, Q2, Q4 |
| `18002fea0`, `1800764c0` | UI layout | Q1, Q2 |
| `180023830` | `Graphics::Scene::RemoveObject` | Q2, Q3 |
| `18004f790`, `18004f8e0` | `MainLoop` post-draw/post-update push | Q1, Q2 |
| `18007b960` | `Picking::Scene` ctor | Q2, Q3 |
| `18001bf50`, `18001c750`, `18005fc20`, `18001c9e0` | scene/renderer teardown | **Q3** primarily; Q2 only at exit |
| `1800737f0`, `180073bb0` | `Picking::Scene` dtor | **Q3** primarily; Q2 only at exit |
| `180004890` | `AchievementManager` teardown | Q4, or a world reload |
| `180051d80` | `Audio::Scene` teardown | Q2 at exit |
| `180033960`, `180033a40` | `~Engine` | **clean process exit ONLY** |

### The gaps, named

1. **`0x180033960` and `0x180033a40` are reached only by a clean shutdown.** A killed process
   gives `calls=0` on both, and that is the shape
   `reference_uncalled_port_is_not_verified.md` warns about. Q1 and Q2 both end with a real
   quit; if either is interrupted, those two lines are meaningless.
2. **Nine of the eleven J1 addresses are teardown-only.** Q3 is not optional — without a world
   reload, `calls=` on the scene dtors reflects one exit path, not use.
3. **The three non-trivial instantiations have their POINTER SLOTS seeded zero** — narrowed
   from "all-zero elements" in §3.4b, which is what let an unwritten-bytes window hide. Only
   the bytes the element operations dereference stay zero now (`+0x00..+0x0f` for
   `kSpec_Sprite38`, `+0x00..+0x07` for `kSpec_StringExt`, `+0x30..+0x3f` for
   `kSpec_EffectSlot`); everything else is seeded non-zero, so **the container algebra and the
   scalar half of the element semantics are both verified for all three.**

   What remains uncovered is the **refcount** half: the `String` slots stay null, so the
   `INC dword ptr [RAX-4]` bump inside `0x180061480`'s inlined assign and inside
   `0x180004220` is **not reached by the probe**. It is covered only indirectly, by
   `runtime/src/ui/sprite.cpp`'s existing refcount reasoning and by whether Q1/Q2 leak or
   double-free strings. Closing it means seeding a synthetic `str-8`/`str-4` refcounted block
   and asserting the count afterwards — specified, not implemented, and it is this lane's
   largest single remaining gap.
4. **The five new helper addresses are not verified by anything.** They are called by address
   and remain the DLL's, exactly as `ClearAll` and `RegisterInput` call `Collection::Set`
   internals. They are lane D rows now, not lane Q results.
5. **The header-algebra live check is not an oracle** (§3.5). If Q1 cannot run for some reason,
   a green Q2 is a coverage report, not a verification.

---

# 4. New lane-D rows created by this lane

Add to `wave9_internals.csv`:

| address | ins | job | created by | disposition |
|---|---|---|---|---|
| `0x180004220` | 30 | element copy-assign, 0x38 `SpriteSheet` entry. Refcounts two `String`s, copies nine dwords `+0x10..+0x30`, **leaves `+0x34..+0x37`** | lane Q `kSpec_Sprite38` | PORT with the SpriteSheet element type |
| `0x1800059e0` | 62 | destroy-range, same element (releases two `String` refs each) | lane Q `kSpec_Sprite38` | PORT with `System::String` release |
| `0x180051650` | 27 | element copy-assign, 0x40 `Audio::EffectSlot`. **Leaves `+0x2c..+0x2f`**, zeroes `+0x30`/`+0x38`, then calls `0x180050d30` (lane D's J10) on the sub-object | lane Q `kSpec_EffectSlot` | **PORT with the Audio holdbacks** — the J10 call is the gate |
| `0x180052110` | — | destroy-range, same element | lane Q `kSpec_EffectSlot` | **PORT — ordinary destroy-range** (corrected, see below) |
| `0x180005930` | 43 | destroy-range, 0x10 `{String*, ExternalPtr}` | lane Q `kSpec_StringExt` | PORT with `System::String` release |

**Two corrections from lane D2, accepted** (also in §3.4b):

* `0x180052110`'s "PORT with the Audio holdbacks" was **wrong**. It is an ordinary
  destroy-range body — `offs{0x30}`, stride `0x40`, calling nothing but the allocator — and is
  not gated on Audio. Its sibling `0x180051650` genuinely is, because it calls `0x180050d30`.
  Filing the pair together on the strength of the sibling was the error.
* "Three of the five are the same shape, so they are **likely one body with three strides**"
  was right about the two that matter and wrong that all three are one body: **one of the three
  is a different body.** The J1 result does not generalise for free.

## Rows lane Q did NOT take, and why

* **J2 / J2b** (`180010ab0`, `18001c7e0`, `18001c8c0`, `180033880`, `1800416d0`, `18005bf60`,
  `180073c90`) — `~Array` plus an element-destroy prologue. The container half is now written
  and reusable; what is missing is the six element-destroy helpers lane D named, plus the CRT
  `eh_vector_destructor_iterator` for J2b. Cheap to finish **on top of this lane**, and it
  creates 6 more rows, which is the trade lane D already priced.
* **J4** (`180003970`, `180010110`) — `operator=` / deep copy. Depends on J3, which now exists.
* **J5** (the `Collection::Set` AVL, 5 rows) — a different container.

## A row lane D's CSV does not have

`tools/runtime_census.py --unported --namespace internal` lists **`0x1800042a0`** and
**`0x180005dc0`**, both called from `runtime/src/achievement/goal_serialize.cpp`, and neither
appears in `wave9_internals.csv`'s 66. Given their addresses sit inside the same
`0x180004000`–`0x180006000` band as `0x180004220`/`0x1800059e0`, they are plausibly more of the
same element-helper family. **Not investigated by this lane** — flagged because the 66 is being
used as a work list and it is short by at least two.

---

# 5. Comments this lane made false, and corrected

Registering these 27 addresses changes what `ResolveAddr` returns at ~20 existing call sites.
Several carried comments asserting the opposite, and this project has just spent a commit
(`e9d81d9`) on exactly that class of drift. All are corrected in the same change:

| file | claim that became false |
|---|---|
| `runtime/src/ui/element.cpp` | *"Safe here — and only here — because **nothing registers 0x180076900**, so the port registry does not intercept it."* It does now. |
| `runtime/src/picking/scene.cpp` | *"All four are called here at their own addresses so the ported dtor is **the retail body and not a re-derivation of it**."* True of one of the four now (`0x180073c90`, still lane D's J2). |
| `runtime/src/graphics/scene.cpp` | the same sentence, for `0x18001c750` / `0x18005fc20` |
| `runtime/src/achievement/achievement_manager.cpp` | *"so **calling the DLL's copy** is not a heap split"* — the heap argument still holds, the "DLL's copy" does not |
| `runtime/src/ui/sprite.cpp` | *"Delegate the array mechanics to **retail's own unexported helper**"* |
| `runtime/src/graphics/renderer.cpp`, `runtime/src/engine/loops.cpp`, `runtime/src/ui/widgets_layout.cpp`, `runtime/src/audio/audio.cpp` | descriptions that read as "this call lands in the DLL" |

Each correction says what the sentence used to claim rather than silently deleting it, because
the old claim was load-bearing for a reader and a future revert of this lane must restore it.

---

# 6. What the integrator needs to know

1. **Run Q1 first.** It is the only run that verifies the bodies. If its negative control does
   not fire, stop — nothing else in this lane means anything.
2. **`RtCollectionPortInit()` is called last** in `RuntimePortInit()`
   (`standalone/resolve_standalone.cpp`), deliberately: it is the one init that changes what
   *other* waves' `ResolveAddr` calls return, and it happens at one identifiable point.
3. `RuntimePortInit()` runs **before** `EngineFuncsInit()` (`standalone/main.cpp:1655` vs
   `:1681`), so `engine/funcs/engine_funcs.cpp`'s eager caching of `0x18007a0d0`,
   `0x18004f790` and `0x18004f8e0` picks up these registrations. That ordering is already
   asserted in a comment at the call site; it is now load-bearing for this lane too.
4. **If something breaks, bisect with the two env vars** (`BANISHED_RT_ARRAY_NONTRIVIAL=0`
   first — it is 3 of 27 and carries all the element semantics — then
   `BANISHED_RT_COLLECTION=0`).
5. **The probe no longer registers anything with `rt_diff.cpp`** (§3.4a). A run with
   `BANISHED_RT_ARRAY_PROBE` set should now report the same site count as one without it, and
   **no `CORRUPTED` line**. That is the check that the fix landed; the probe's own
   `[RtArrayProbe]` lines are unchanged.
