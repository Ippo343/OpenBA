# Wave 2, lane C — `System::MemoryAllocator`

Ground-truthed 2026-09-06 on `wave/upgrade-path-complete`. Every address below came
from a live Ghidra query against `Runtime-x64.dll` or from `pefile` over the shipped
DLL; nothing here is inferred from a name.

**Bottom line: the port is written, compiles clean, and is LANDED SWITCHED OFF.
`RtSystemAllocatorRegister()` returns 0 and logs why. The shadow proof DID NOT RUN —
no numbers are claimed. The verdict on the atomic swap is NO-GO until it does.**

---

## 1. What the allocator actually is

Not what the plan assumed. PLAN.md §4.3 risk 2 calls it "bucketed free lists,
refcount at `str-4`, user pointer at `str-8`". Two corrections:

* It is a **single-arena boundary-tag allocator** with 192 **segregated first-fit
  bins**, not a bucketed pool. The bins index a search over variable-sized blocks;
  they do not own fixed-size cells. There is no separate large-allocation path and
  no CRT fallback — a 4-byte String and a 40 MB texture come out of the same arena.
* The `str-4` / `str-8` note in `standalone/filename_runtime_hook.cpp` is **correct
  but it describes `System::String`, not the allocator**. Verified against
  `String::SetLength` (`0x1800c8220`):

  ```
  U = MemoryAllocator::Allocate(_singleton, len + 1 + 8, 4);
  *(int*)(U+0) = len;   *(int*)(U+4) = 1 /*refcount*/;   this->_data = U + 8;
  ```

  So for a `String* s`: `s-4` is the refcount, `s-8` is the allocator user pointer,
  and **the allocator's own 8-byte header is at `s-0x10`**. The allocator header
  carries no refcount at all. Anyone reading that note as "the allocation header has
  a refcount at -4" is one offset out.

Full layout, tag bit fields, bin function and free-list encoding:
`runtime/types/system/MemoryAllocator.h` (long header comment, every offset
`static_assert`ed). Highlights:

| | |
|---|---|
| `sizeof(MemoryAllocator)` | **0x670** — pinned by `operator=` (`0x18007c0c0`) `memcpy`ing 0x670 |
| Singleton | `?_singleton@…` at `0x1801fc718`; **the allocator is embedded at offset 0 of `Engine::Engine`** (`Engine::GetMemoryAllocator` `0x18000c8b0` is `return this`) |
| Tag | 8 bytes at `user-8`. bits 0..40 size/4 · bits 41..43 prevSize/4 · bit 44 PREV_FREE · bit 45 THIS_FREE · bits 46..63 free-list prev-link high bits |
| Prev size overflow | when prevSize/4 > 7 the 3-bit field reads 0 and the size is a full qword **footer** at `tag-8` |
| Free list | **doubly linked, 41-bit SIGNED RELATIVE offsets**; next in the FreeTag qword bits 0..40, prev **split** — low 23 bits in the FreeTag bits 41..63, high 18 bits in the MemoryTag bits 46..63 |
| Bins | `bytes<=0x200 → bytes>>2` (0..128, exact), else `119 + floor(log2(bytes))` (128..191); 192 bins |
| `_minBin` (+0x628) | a search **floor**, not a hint — `GetFreeTagForSize` clamps its start bin **up** to it. Starts at 0xbf |
| Not linked | blocks under 16 bytes are free but unreachable holes until a neighbour coalesces them |
| Memory | `VirtualAlloc` MEM_RESERVE per range; pages committed lazily to the used high-water mark and **decommitted** when the tail block is freed |
| Limits | `Startup(minRangeMB, maxTotalMB)` — both `<< 20`. `maxTotalSize == 0` means unlimited |
| Statistics | `+0x630` allocatedBytes, `+0x638` live blocks, `+0x63c` linked free blocks, plus a file-static "largest size ever requested" at `0x1801ff300` that nothing reads |
| Debug/guard bytes | **none**. `Allocate` returns dirty memory |

All 30 class functions are exported and all 30 are ported. Address map in the header
of `runtime/src/system/allocator.cpp`.

### Three things a "tidy" reimplementation gets wrong

1. `align8 - 8` is **sign-extended** before the add (`CDQE` at `0x1800c6ea1`). With
   `align == 0` that subtracts 8 from the request; it does not add 4 GB.
2. The alignment padding uses a **64-bit `DIV` but a 32-bit `IMUL`/`SUB`**
   (`0x1800c6f7d`..`0x1800c6f80`). The truncation is reproduced.
3. `Free` decrements `_allocCount` / `_allocatedBytes` **before** coalescing, using
   the size the user block had.

---

## 2. What landed

| File | What |
|---|---|
| `runtime/types/system/MemoryAllocator.h` | layouts + `static_assert`s + the full RE writeup + the `RtSysMemPlatform` seam |
| `runtime/src/system/allocator.cpp` | the byte-faithful port, portable half — no OS header |
| `runtime/src/system/allocator_win.cpp` | the six exported `Platform*` bodies + the Win32 seam table |

The type names `MemoryTag`, `FreeTag` and `MemoryRange` are the DLL's own — they
appear in the mangled signatures (`PEAUMemoryTag@12@`, `PEAUFreeTag@12@`,
`PEAUMemoryRange@12@`), so the header uses those and the port's signatures use them
in the positions retail does. A `FreeTag*` is the USER pointer of a free block
(`tag + 8`), not its header; getting that backwards is a one-qword error that reads
as a plausible pointer.

The split between the two source files is not one we invented: the class has six
exported `Platform*` members and every `VirtualAlloc` / `GetSystemInfo` /
`CRITICAL_SECTION` call in the allocator is one of them or an inlined copy of one.
The original authors drew the platform line exactly where PLAN.md §4.1 asks for it,
so `allocator_win.cpp` is that translation unit reconstructed rather than a wrapper.
`allocator.cpp` never sees an OS header; a `_posix`/`_sdl` twin supplies the same two
entry points (`RtSystemAllocatorInstallPlatform`,
`RtSystemAllocatorRegisterPlatformExports`).

`_singleton` is resolved by NAME (`?_singleton@MemoryAllocator@System@@0PEAV12@EA`,
an exported data symbol) through `ResolveExportRaw` — the hardcoded `0x1801fc718` is
recorded in the comments as documentation only and is never used to reach it.

Compile check, clean, zero warnings:

```
g++ -std=c++17 -fsyntax-only -Wall -Wextra -I. -Iengine/types -Iengine/funcs -Icode/Game \
    -Iruntime/types -Iruntime/funcs -Idx11/types -Idx11/funcs -Istandalone \
    -include standalone/env_cache.h runtime/src/system/allocator.cpp
```

No `<windows.h>` anywhere under `runtime/`. The allocator genuinely needs
`VirtualAlloc`/`VirtualFree`/`GetSystemInfo` and a recursive mutex, so those ten
operations are a function-pointer table (`rt::system::RtSysMemPlatform`) that the
host installs through `RtSysMemPlatformInstall()`. **Until it is installed the port
refuses to arm.** The Windows implementation belongs in
`runtime/src/system/MemoryAllocator_win.cpp` — a file this lane was not allowed to
create; §6 has its exact contents.

---

## 3. The shadow proof — design, and the honest status

`rt_diff.h` cannot diff an allocator the Wave 1 way: running both implementations
mutates the heap twice. `RT_DIFF_SHADOW`'s contract is "ours computes into a private
replica, theirs executes for real, and the port asserts the replica still matches".
Implemented as a **replay replica**:

* A **second, private arena**, reserved from the platform seam, driven only by our
  ported code and touched by nothing else in the process.
* Armed at `MemoryAllocator::Startup` with the **same** `(minRangeMB, maxTotalMB)`,
  so both arenas begin in the identical virgin state and every subsequent range is
  reserved in the same order at the same size.
* Every `Allocate` the DLL performs is replayed with the same `(size, align)`; every
  `Free` is replayed on the replica pointer corresponding to the freed DLL pointer
  (`theirs → ours` map in its own reservation; it never touches either heap).
* The invariant asserted **per call** is
  `(rangeIndex, offsetWithinRange, allocatedBytes, allocCount, freeTagCount, minBin)`.

Absolute addresses cannot match — the arenas are at different `VirtualAlloc`
addresses — but the **offset within the range must**, because the two arenas are
byte-identical in layout. That single comparison exercises the whole machine: bin
selection, the first-fit walk, the alignment split, the tail split, forward and
backward coalescing, and the free-list encoding. Any divergence desynchronises the
offsets on a later call and never recovers, so it cannot hide.

Entry points (`extern "C"`, declared in `MemoryAllocator.h`):
`RtAllocShadowStartup`, `RtAllocShadowAlloc`, `RtAllocShadowFree`,
`RtAllocShadowReport`.

### Status: NOT RUN

The machinery is written and compiles. It has **not executed once**. It needs two
host-side pieces that do not exist in the tree:

1. `runtime/src/system/MemoryAllocator_win.cpp` (§6) plus a
   `RtSysMemPlatformInstall()` call from `standalone/main.cpp`.
2. A driver in `standalone/` that detours `MemoryAllocator::Startup`
   (`0x1800c7620`), `Allocate` and `Free` in **observe-only** mode: call the
   (`0x1800c7620`), `Allocate` and `Free` in **observe-only** mode: call the
   original, then hand the result to `RtAllocShadow*`. This is the existing
   `allocator_lock.cpp` shape with two extra calls — see §4.

**No shadow numbers are reported anywhere in this lane. Do not read the presence of
the code as evidence the port is verified.** To produce them, once §4 and §6 land:

```
BANISHED_RT_DIFF=1 BANISHED_RT_ALLOC_SHADOW=1 BANISHED_AUTOSTART=1 \
  make sa-run          # then read [RtDiff] lines in standalone_error.log
BANISHED_RT_DIFF=1 BANISHED_RT_ALLOC_SHADOW=1 make sa-stress
```

The pass bar is the Wave 1 bar: **zero divergences over a full autostart game and a
full `sa-stress` run**, with a `calls=` column in the millions. A green summary with
a small `calls=` is the Wave 1 defect #4 trap and must be rejected.

---

## 4. Interaction with `standalone/allocator_lock.cpp` — REQUIRED CHANGES

This is the PLAN.md §2.2 hazard A case, and it is live: `allocator_lock.cpp` already
patches the exact two prologues this lane ports.

| | Address | Patch |
|---|---|---|
| `MemoryAllocator::Allocate` | `0x1800c6e60` | 13 bytes, `allocator_lock.cpp:292` |
| `MemoryAllocator::Free` | `0x1800c6c00` | 12 bytes, `allocator_lock.cpp:363` |

### What those patches carry that must not be lost

`AllocateLocked` / `FreeLocked` do four things, not one:

1. `EnterCriticalSection(self + 0x640)` around the body. **This is not decoration.**
   Retail's `Allocate`/`Free` take no lock — only the static `MemAlloc`/`MemFree`
   wrappers do, and Runtime-internal code calls the raw `Free` directly, bypassing
   them (`allocator_lock.cpp:251-268`). Without the patch the free list races.
2. `RedzoneAlloc_TeardownOp("teardown-alloc"/"teardown-free")`.
3. The `BANISHED_ALLOC_STATS` accounting (`StatsRecordAlloc`/`StatsRecordFree`,
   `AllocStats_Lookup`, which `redzone_alloc.cpp` reads).
4. Free's wrapper reproduces the `TEST RDX,RDX; JZ` null early-out that its
   trampoline deliberately drops.

### Retired, not carried — and the ordering is enforced by the code

**`BANISHED_RT_ALLOC=1` must RETIRE both patches, and the two must never coexist.**
Carrying them is not merely untidy, it is broken in both directions:

* If `allocator_lock` patches first and we then detour the same prologue, we
  overwrite its `MOV RAX,imm64; JMP RAX` and its trampoline's saved bytes become the
  lock stub's bytes, not retail's. The lock silently disappears **and** the
  trampoline is garbage.
* If we detour first, `allocator_lock`'s `memcmp` against its hardcoded `expected[]`
  fails and it **refuses and returns false** (`allocator_lock.cpp:303`, `:374`) —
  which is safe, but leaves our detour running with **no lock at all**, and the
  free-list race is exactly the intermittent menu crash it was written to fix.

The second case is the one that happens today, because the current order in
`standalone/main.cpp` is `RuntimePortInit()` at **:1599** and
`InstallAllocatorLock()` / `InstallFreeLock()` at **:1660** / **:1664** — the port
init runs *first*. So the failure mode is "unlocked ported allocator", silent apart
from one refusal line.

### The guard that actually landed (2026-09-06, lane C's file)

`RtSystemAllocatorRegister()` now **hard-refuses** rather than proceeding. Before it
calls the swap installer it compares BOTH prologues against the bytes the shipped
DLL actually has (13 at `Allocate`, 12 at `Free`, taken from memory — this build
emits redundant REX prefixes, so `PUSH RBX` is `40 53`). Any mismatch prints which
entry point is already owned and returns 0 without touching anything.

That guard is deliberately broader than "is the allocator lock installed": it catches
**any** foreign patch on those two addresses, including one nobody has written yet,
and it needs no cooperation from the patching file. It closes the silent path — the
unlocked ported allocator is now unreachable without someone first retiring the lock.

**Recommendation, NOT a prerequisite for landing this wave:** whoever takes the
allocator live should still move `InstallAllocatorLock()` / `InstallFreeLock()` above
`RuntimePortInit()` in `standalone/main.cpp`, and add
`AllocatorLockRetireForPort()` below. The reason is not safety any more — the guard
handles that — it is that with the current order the guard fires on **every**
`BANISHED_RT_ALLOC=1` run, because at `:1599` the lock has not been installed yet, so
the prologues are virgin, the port arms, and then `InstallAllocatorLock()` at `:1660`
refuses instead. In other words the current order inverts which of the two wins, and
the guard cannot see that because it runs too early to know the lock is coming. Two
consequences to plan for:

* With the order unchanged, arming the port means the lock never installs, so the
  swap stub in this section MUST take `self + 0x640` itself — which the proposed
  stub does. That is sufficient, but it makes the lock's `ALLOCSTATS` accounting and
  `RedzoneAlloc_TeardownOp` disappear unless the stub carries them too.
* With the order changed plus `AllocatorLockRetireForPort()`, there is exactly one
  sequence and one owner at every instant, and the guard becomes a belt on top of
  braces rather than the only thing standing between us and an unlocked heap.

The reorder is a change to the default configuration, which every run uses, in
service of a path currently used by nobody; deferring it until the shadow numbers
exist is the right call.

**Proposed change to `standalone/allocator_lock.cpp` (this lane did not make it):**

1. Add `bool AllocatorLockRetireForPort();` — removes both patches (restore the
   saved 13/12 bytes, `FlushInstructionCache`) and returns true, or returns false if
   they were never installed. Called by the swap installer before it patches.
2. Move `InstallAllocatorLock()` / `InstallFreeLock()` in `main.cpp` to **before**
   `RuntimePortInit()`, so there is exactly one order and the retire path is the
   only way the two ever interact. (Today they are after, which is the bad order.)
3. Export the three behaviours the port must keep, so the new detour stub can call
   them instead of re-implementing them: `RedzoneAlloc_TeardownOp` is already
   `extern "C"`; add `extern "C" void AllocStats_RecordAlloc(void*, uint64_t, void*)`
   and `extern "C" void AllocStats_RecordFree(void*)` around the existing statics.

**Proposed swap installer** (new `standalone/rt_alloc_swap.cpp`, host side, may
include `<windows.h>`):

```cpp
static void* g_ourAllocate; static void* g_ourFree;

// Composed stub: lock + diagnostics + OUR body. The port itself stays
// byte-faithful and takes no lock, exactly as retail's Allocate/Free do.
static void* SwapAllocate(void* self, uint64_t size, uint64_t align) {
    auto cs = (LPCRITICAL_SECTION)((uint8_t*)self + 0x640);
    EnterCriticalSection(cs);
    RedzoneAlloc_TeardownOp("teardown-alloc");
    void* r = ((void*(*)(void*,uint64_t,int))g_ourAllocate)(self, size, (int)align);
    AllocStats_RecordAlloc(r, size, __builtin_return_address(0));
    LeaveCriticalSection(cs);
    return r;
}
static void SwapFree(void* self, void* p) {
    if (!p) return;                       // retail's TEST RDX,RDX; JZ
    auto cs = (LPCRITICAL_SECTION)((uint8_t*)self + 0x640);
    EnterCriticalSection(cs);
    RedzoneAlloc_TeardownOp("teardown-free");
    AllocStats_RecordFree(p);
    ((void(*)(void*,void*))g_ourFree)(self, p);
    LeaveCriticalSection(cs);
}

extern "C" bool RtAllocSwapInstall(void* ourAllocate, void* ourFree) {
    g_ourAllocate = ourAllocate; g_ourFree = ourFree;
    AllocatorLockRetireForPort();          // hazard A: RETIRE, do not carry
    static RtDetour a, f;
    // No trampolines are needed: our body REPLACES retail's, it does not wrap it.
    if (!InstallRuntimeDetour("MemoryAllocator::Allocate[port]", 0x1800c6e60,
                              (void*)&SwapAllocate, &a, 13)) return false;
    if (!InstallRuntimeDetour("MemoryAllocator::Free[port]", 0x1800c6c00,
                              (void*)&SwapFree, &f, 12)) { RemoveDetour(&a); return false; }
    return true;
}
```

and in `main.cpp`, before `RuntimePortInit()`:
`RtSystemAllocatorSetDetourInstaller(&RtAllocSwapInstall);`

`RtSystemAllocatorRegister()` calls that installer and **registers nothing at all if
it returns false**, so a failed detour can never leave the exports pointing at a port
the DLL does not use. That is the atomicity guarantee, and it is enforced in code
(`allocator.cpp`, `RtSystemAllocatorRegister`), not by convention.

**No change was made to `allocator_lock.cpp`, `rt_port.h`, `rt_system_port.cpp`,
`resolve_standalone.cpp` or the Makefile by this lane.**

---

## 5. Go / no-go on the atomic swap

> **SUPERSEDED 2026-09-07 by [WAVE9_LANE_E2.md](WAVE9_LANE_E2.md).** Gate step 2's
> measurement has now been taken: 12,750,873 replayed operations, 0 divergences, all
> health counters zero, and a negative control proved to go red. The "port has never
> executed a single instruction" line below is CLOSED. Two things below are also wrong
> and are corrected there: `MemoryAllocator::Startup` (§3) is **never called** — MSVC
> inlined it into `Engine::Startup` — and the `minBin` negative control step 2
> prescribes **cannot fail**, because `LinkFreeTag` overwrites it before the first
> comparison. One new gap is open: the arena-growth path never executed (ranges=1).
> The verdict below is kept verbatim as the record of what was believed at Wave 2.

### VERDICT: NO-GO today. GO-able, and less dangerous than PLAN.md assumed.

**The reason it is less dangerous is worth stating plainly, because it changes the
risk model.** The brief's nightmare — "we allocate from our heap while the DLL frees
into its own" — **cannot happen with this design**, because the port does not own a
heap. Every ported function takes the live `MemoryAllocator*` (`_singleton`, i.e. the
`Engine` object) and reads and writes the *same* boundary tags, the *same* 192 bins
and the *same* `VirtualAlloc` ranges the DLL built. There is one heap and one set of
metadata; the swap changes only which code manipulates it. A block allocated by
retail before the swap and freed by us after it is an ordinary free. That is why
detouring `Allocate` and `Free` alone is sufficient: `Startup`, `AddNewMemoryRange`
and the free-list helpers are only ever reached from inside those two, so replacing
the two bodies replaces the whole subsystem, and construction can stay retail's.

Evidence **for** go:

* All 30 functions decompiled, and the private helpers are exported, so nothing rests
  on a `FUN_` guess (PLAN.md §4.3 risk 4 does not bite this lane).
* Layout pinned three independent ways: `operator=`'s 0x670 `memcpy`, the ctor's
  field-by-field zeroing, and `PlatformConstruct`/`PlatformStartup` bracketing
  `+0x668` against the `CRITICAL_SECTION` at `+0x640`.
* Shared-state model above: no two-heap failure mode exists.
* The registration is genuinely atomic in code — detour first, register only on
  success, register nothing on failure.
* `allocator_lock.cpp` proves the two prologues are detourable: it has been patching
  them, default-on, for months.

Evidence **against**, and these are why it is no-go:

* **The port has never executed a single instruction.** Not once, not in shadow, not
  behind the switch. Wave 1's defect #4 (`Quaternion::operator*` wrong in z, survived
  the whole wave because nothing called it) is the standing warning.
* The shadow harness has never run either, so "it would agree" is a claim with no
  measurement behind it.
* The swap installer and the shadow driver do not exist; two `standalone/` files
  must land before anything can be measured.
* `allocator_lock.cpp` is default-on and currently installs **after**
  `RuntimePortInit`, so the very first `BANISHED_RT_ALLOC=1` run would silently lose
  the lock. §4 item 2 must land first.
* 1,778 inbound call edges and every other lane in Wave 2 sits on top of this. A
  wrong bit in the free-list prev split is a wild write with no proximate symptom —
  the `~InternalFactory` swap-remove family all over again.

**Gate to flip to GO**, in order:

1. ~~Land the platform seam~~ — done, `runtime/src/system/allocator_win.cpp`.
2. Land the observe-only shadow driver; run `sa-run` and `sa-stress` under
   `BANISHED_RT_DIFF=1 BANISHED_RT_ALLOC_SHADOW=1`. Require **0 divergences with
   `calls=` in the millions**, and check `calls=`, not just `divergences=`.
3. Land §4's `AllocatorLockRetireForPort()` and the `main.cpp` reorder.
4. Only then a first `BANISHED_RT_ALLOC=1` run, and even then behind the four
   Wave-boundary gates in PLAN.md §5 (tutorials 1–4, the achievement census,
   `Save9.sav`, `sim ms/iteration` within 5%).

---

## 6. What the host still has to supply

Nothing under `runtime/` is outstanding. `runtime/src/system/allocator_win.cpp`
defines `RtSystemAllocatorInstallPlatform()` and
`RtSystemAllocatorRegisterPlatformExports()`; the two files mutually resolve
(`ld -r` over both leaves no cross-file symbol undefined) and neither emits a static
initialiser, so nothing in this lane runs before `main`. Two `standalone/` files are
still outstanding:

* `standalone/rt_alloc_swap.cpp` — the composed detour stub in §4, plus
  `RtSystemAllocatorSetDetourInstaller(&RtAllocSwapInstall)` called before
  `RuntimePortInit()`.
* the observe-only shadow driver in §3, which may live in the same file.

Plus §4 items 1–2 inside `standalone/allocator_lock.cpp` and `standalone/main.cpp`.

## 7. Defects found

### 7.1 `?MemAlloc@MemoryAllocator@System@@SAPEAXII@Z` is a PHANTOM (pre-existing, exe-side)

It is not a deferral and it is not registered anywhere in this lane's files
(`grep -c SAPEAXII` over all three: 0, 0, 0). The census lists it as "called"; it
never has been.


The DLL exports exactly **two** `MemAlloc`-family symbols:
`?MemAlloc@MemoryAllocator@System@@SAPEAX_KH@Z` (`0x1800c7a50`) and
`?MemFree@MemoryAllocator@System@@SAXPEAX@Z` (`0x1800c7ad0`). The `SAPEAXII@Z`
("unsigned, unsigned") spelling circulating in this project's notes and used at
**six live call sites** is a mangling the DLL does not carry, so
`ResolveExport(...)` on it has returned null forever. Confirmed with `pefile` over
the shipped `Runtime-x64.dll` (32 `MemoryAllocator` exports listed; that name is not
among them). Wave 1 defect #3 is the same class. Lane B's `runtime/src/system/string.cpp:57`
independently reached the same conclusion this session.

**Impact today: none, and that is luck, not design.** All six sites read the exe
dispatch slot `0x140152b60` first, which IS registered
(`standalone/address_registry.cpp:1319`) and does resolve to the real `MemAlloc`.
The bad name is only the fallback:

| Site | Fallback if the slot were ever null |
|---|---|
| `code/Game/Action/action_pf_geom.cpp:82` | **null call** |
| `code/Game/Player/tool_base_highlight.cpp:59` | **null call** |
| `code/Game/Player/tool_base_trade.cpp:55` | **null call** |
| `code/Game/Work/pickup.cpp:383` | **null call** |
| `code/Game/Work/tradework_serialize.cpp:55` | **null call** |
| `code/Game/Component/keystone_a_restree.cpp:73` | safe — falls through to the `_KH` form at `:76` |

Fix: replace the string with `?MemAlloc@MemoryAllocator@System@@SAPEAX_KH@Z` at all
six (the ABI is identical — size in RCX, alignment in RDX). Not done here: those
files are outside this lane's ownership.

### 7.2 The allocator-lock ordering hazard

Described in §4. `RuntimePortInit()` (`standalone/main.cpp:1599`) runs **before**
`InstallAllocatorLock()` (`:1660`), so the first `BANISHED_RT_ALLOC=1` run would
detour the prologue first, `allocator_lock`'s `memcmp` guard would refuse, and the
allocator would run **unlocked** — reintroducing the free-list race that the lock was
written to fix, announced only by one refusal line in `standalone_error.log`. Not a
defect in code that exists today (nothing sets `BANISHED_RT_ALLOC`); it is a trap
armed for whoever flips the switch first. §4 items 1–2 disarm it.

### 7.3 PLAN.md §4.3 risk 2 is wrong about the shape

"Bucketed free lists, refcount at `str-4`, user pointer at `str-8`". It is
boundary-tag + segregated first-fit bins, and the refcount belongs to `String`. The
practical consequence: a reader who plans around "buckets" expects fixed-size cells
and no coalescing, and would not think to reproduce the forward/backward merge, the
prev-size 3-bit/footer split, or the 41-bit relative free-list links — which is the
majority of the code. Corrected in `runtime/types/system/MemoryAllocator.h`; PLAN.md
itself is outside this lane's ownership.

### 7.4 `+0x62c` is dead padding, and `Shutdown` leaks the last table

Two harmless observations recorded so the next reader does not go looking:
`+0x62c` is never written by any retail function (the ctor's `*(u64*)(this+0x638)=0`
covers `allocCount`+`freeTagCount` in one store, and `+0x628` is a lone dword).
`MemoryAllocator::Shutdown` (`0x1800c75b0`) releases every range and then the range
table itself, but `~MemoryAllocator` (`0x1800c7960`) does **not** call `Shutdown` —
it only nulls `_singleton` and deletes the critical section. Process teardown
reclaims it; it is not a defect, just not symmetric.
