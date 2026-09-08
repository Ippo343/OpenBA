# Wave 9 — Lane D2: the rest of the unnamed DLL internals

Produced 2026-09-07 on `wave/upgrade-path-complete`, from `ef22d4a`. **Nothing was built and
the game was not run** — the integrator owns the exe this round. What was run: Ghidra MCP,
`g++ -fsyntax-only` on the three new files (writes nothing, touches no object directory),
`tools/runtime_census.py`, `tools/port_bypass_audit.py`, `tools/export_name_audit.py`,
`tools/resolve_fallback_audit.py`.

## Result

**12 addresses retired for four bodies, and ZERO new lane-D rows created.**

```
tools/runtime_census.py, before:   internal  69 called, 27 ported (39.1%)
tools/runtime_census.py, after:    internal  70 called, 39 ported (55.7%)
TOTAL                              843 called, 585 -> 586 ported
tools/port_bypass_audit.py         0 bypasses (both doors)
tools/export_name_audit.py         0 phantom names
tools/resolve_fallback_audit.py    0 defective fallbacks
"Ports with no caller in our tree"  0 addresses  (see §4)
```

The `called` column moving 69 → 70 is **not** this lane: it is `0x180010760`, a row lane V's
`runtime/src/video/companion_data.cpp` created concurrently. This lane created none, which is
the headline and is the opposite of what lane D priced.

## Files

| File | What |
|---|---|
| `runtime/src/collection/array_destroy.cpp` | four bodies, the 11-row parameterization table, 12 registered entry points, `RtCollectionDestroyPortInit()` |
| `runtime/src/collection/array_destroy_internal.h` | what the port and its probe must agree on: `DestroySpec`, the rows, the body signatures |
| `runtime/src/collection/array_destroy_probe.cpp` | the oracle: synthetic-subject replay against the shipped bodies, effectiveness + negative controls |
| `runtime/funcs/rt_port.h`, `standalone/resolve_standalone.cpp` | `RtCollectionDestroyPortInit()` declared and called after lane Q's |
| `runtime/src/collection/array.cpp` | two comment corrections lane Q's row table needed (§5) |
| `docs/runtime_port/wave9_internals.csv` | 66 → 74 rows; dispositions updated for all 43 addresses now closed |

**Kill switches**: `BANISHED_RT_ARRAY_DESTROY=0` (this lane's 12), inside
`BANISHED_RT_COLLECTION=0` (lane Q's 27 as well). `BANISHED_RT_ARRAY_DESTROY_PROBE=1|neg`.

---

# 1. The re-derived work list

The brief said to re-derive from the census rather than trust the round-1 CSV. Doing so found
**42 open addresses, not the ~39 the CSV implied** — and the difference is in both directions.

```
tools/ghidra_mcp/.venv/Scripts/python.exe tools/runtime_census.py --unported --namespace internal
```

| | count | |
|---|---|---|
| in the CSV's 66 | 66 | |
| − retired by lane Q | 27 | |
| − **J13, already closed** | 4 | the four dead fallback branches are **deleted from the source**, not pending |
| + **absent from the CSV** | 7 | lane Q's five new rows, plus `0x1800042a0` and `0x180005dc0` |
| = open at lane D2's start | **42** | |

**J13 is closed and nobody recorded it.** Lane D recommended deleting four dead fallback
branches and explicitly deferred the edit ("Dead-but-latent waits one round"). Somebody did it:
`0x18006a3e0`, `0x18008f530` and `0x18008f880` survive only inside comments explaining why they
were wrong, and `kRVA_Dtor` is gone from `tool_base_toolui.cpp` with a note in its place.
`tools/resolve_fallback_audit.py` now reports **163 sites, 0 defective** (was 166/5). The CSV
still carried them as open work; those four rows are marked CLOSED.

## What lane D2 did with the 42

| Job | Addrs | Disposition |
|---|---|---|
| **J2** `~Array` + element-destroy prologue | 6 | **PORTED** |
| **J2b** `~Array` via `eh_vector_destructor_iterator` | 1 | **PORTED** — and it is not a CRT dependency (§2) |
| **J4** `Array<T*>::operator=`, the `0x180003970` half | 1 | **PORTED** |
| lane Q's destroy rows `0x180052110` `0x180005930` `0x1800059e0` | 3 | **PORTED** — retired, not deepened |
| lane Q's copy-assign row `0x180004220` | 1 | **PORTED** — 37 ins, zero callees |
| **J12** `_beginthreadex` | 1 | CONFIRMED CRT swap, no port (§6) |
| **J14** `.data` globals | 3 | CONFIRMED lane C's, no port (§6) |
| **J6** allocator free-under-lock | 1 | lane E's, untouched (standing NO-GO) |
| **J11** `Volume::Box` transform | 1 | **HELD BACK with a reason** (§7) |
| **J5** `Collection::Set`/`Map` AVL | 5 | triaged, not ported (§7) |
| **J7** Graphics grid / **J8** Picking grid | 10 | untouched, still lane D's ranking |
| **J9** `IO::Mod` (+ the other J4, `0x180010110`) | 4 | untouched, HARD |
| **J10** Audio (+ `0x180051650`) | 3 | gated on lane A |
| **J15** `ExternalBasePtr::Serialize` ×2 | 2 | **NEW rows, and the brief's question answered** (§3) |
| lane V's `0x180010760` | 1 | not this lane's |

---

# 2. The finding: eleven helper addresses are three bodies

Lane D priced J2 at "**+6 new lane-D rows**", because each of its six `~Array` bodies opens
with a `CALL` to an unexported per-element destroy helper, and J2b at a seventh. Lane Q then
created five more rows for the same reason, three of them destroy helpers. Following all of
those to their own depth-2 callees gives **eleven destroy-helper addresses**.

Hashing the **mnemonic sequence** of all eleven — lane D's own method, and the only one that
works here, since byte hashes differ purely in RIP-relative displacements — says they are
**three bodies**, and ten of the eleven are two of them.

## Body A — free the inner `Array<T>` at element+off, clear its 16-byte header

| address | offsets, in retail's order | stride |
|---|---|---|
| `0x18001ce90` | `{0x100}` | `0x110` |
| `0x1800419b0` | `{0x008}` | `0x018` |
| `0x180052110` | `{0x030}` | `0x040` |
| `0x180074100` | `{0x020}` | `0x048` |
| `0x18005bce0` | `{0x038, 0x028, 0x008}` | `0x048` |
| *(J2b's inlined `eh_vector` loop)* | `{0x1e0, 0x1d0, 0x1c0}` | `0x1f0` |

The header clear is `[e+off] = 0` and `[e+off+8] = 0` — one qword each, the second covering
`_size` **and** `_capacity` together, exactly as J1 does — and it happens **unconditionally**,
including when the buffer was already null.

`0x180074100`'s mnemonic hash differs from the other three single-offset members (44
instructions vs 43) for one redundant `LEA RAX,[RBX-0x8]` unwind spill that has no effect on
the object. Same body.

## Body B — release one `System::String` reference per (element, offset)

| address | offsets | stride |
|---|---|---|
| `0x180011010` | `{0x000}` | `0x008` |
| `0x180005930` | `{0x000}` | `0x010` |
| `0x180014a40` | `{0x000}` | `0x048` |
| `0x180034440` | `{0x010}` | `0x018` |
| `0x1800059e0` | `{0x008, 0x000}` | `0x038` |

`0x180011010`'s hash differs (`3000b22572` vs `154da96279`) because MSVC indexed it —
`MOV RAX,[R14+RDI*8]` with an INC/CMP counter — instead of bumping a pointer by the stride.
With offset 0 and stride 8 the two addressings compute identical addresses; every store, every
branch and every immediate is otherwise the same.

**Body B's asymmetry is the whole risk of this lane, and the only thing a comparator can see:**

```
RAX = *(char**)(e+off)
if (RAX) {
    DEC dword [RAX-4]                  ; System::String's refcount
    CMP dword [RAX-4],0
    JNZ  ->  MOV [e+off], 0            ; STILL SHARED: slot becomes NULL
    ADD qword [e+off], -8              ; LAST REF:     slot becomes str-8 ...
    MOV RBP,[e+off]
    JZ  -> skip                        ; ... flags from the ADD, i.e. str == 8
    Enter; Free(_singleton, RBP); Leave
}                                      ; and the slot KEEPS str-8
```

The two arms leave the slot holding **different things** — `NULL` when the string survives,
`str-8` (a dangling, just-freed pointer) when it does not. Retail never clears it on the
freeing path. That is not a bug to fix: the object is being destroyed and the caller's next act
is to free the whole buffer, so a helpful `= nullptr` would hide a use-after-free that today is
loud. The probe's negative control is aimed exactly here.

## The eleventh: `0x18005c550` is neither

Lane D filed it as one more J2 helper. It is a **recursive** destroy: each of its elements holds
two nested Arrays whose own elements need destroying first, so it calls one of each body before
freeing.

```
for each element e (stride 0x30):
    0x18005bce0(ignored, *(void**)(e+0x20), *(int32*)(e+0x28));   // Body A x3
    free *(void**)(e+0x20);   e+0x20 = 0;  e+0x28 = 0;
    0x180014a40(ignored, *(void**)(e+0x10), *(int32*)(e+0x18));   // Body B
    free *(void**)(e+0x10);   e+0x10 = 0;  e+0x18 = 0;
```

Both sub-helpers are bodies this lane wrote, so it still creates no new row.

## J2b is Body A, not a CRT dependency

Lane D priced `0x18001c8c0` as "J1 + CRT `eh_vector_destructor_iterator` + a per-element dtor".
Three hops say otherwise, and each one needed a different tool:

* `LEA R9,[0x18001b960]` names the per-element dtor. **`0x18001b960` is not a function in
  Ghidra's Runtime program** — `getFunctionAt` returns null and `disassemble_function` errors.
  Read as raw memory it is five bytes, `E9 EB 0C 00 00`: an incremental-link **thunk** to
  `0x18001c650`.
* `0x18001c650` is the **J1 body again** — a twelfth copy of `Array<T>::~Array` for a trivially
  destructible `T`, the shape lane Q proved eleven times.
* `0x18007ec98` (`eh_vector_destructor_iterator`, 32 instructions) is
  `p = base + count*size; while (--count >= 0) { p -= size; dtor(p); }` — **reverse order**.

So the whole of J2b is: per element (stride `0x1f0`), destroy three inner Arrays at `+0x1c0`,
`+0x1d0`, `+0x1e0` **descending**, then the J1 tail. Body A with a descending offset list. No
CRT dependency survives and the per-element dtor is not a new row.

`runtime/src/graphics/renderer.cpp:196` independently pins the same stride ("The stride 0x1f0 in
its loop is what pins sizeof(DrawGroup)"), read by a different lane from a different direction.

## A leaf worth taking: `0x180004220`

37 instructions, **zero callees**, no allocation — the one of lane Q's five new rows that is a
self-contained leaf. It is the SpriteSheet element's `operator=`, and it corroborates the
destroy side: it assigns exactly the two `System::String` slots at `+0x00` and `+0x08` that
`0x1800059e0` releases, in the same two places. Two independently-read bodies agreeing on a
layout is worth more than either reading alone.

**It also exposes an oracle defect in lane Q's spec — see §5.**

---

# 3. `0x1800042a0` / `0x180005dc0` — the brief's question, answered

These are the two rows lane Q found missing from the CSV. Both are reached from
`runtime/src/achievement/goal_serialize.cpp` (`Goal::Serialize` and `GoalInstance::Persist`).

**They are ONE body written twice.** Mnemonic hash `411b4433bd`, 215 instructions each,
identical call lists down to the order of the twenty-one call sites. One port covers both — the
same result as J1/J3/Body A/Body B, at a fourth scale.

**The brief asked whether they are write-path-only, because lane S's whole result rests on a
write-path `Serialize` being a pure read of the object. The answer is: NOT ON STATIC EVIDENCE,
and the body says why.** Its twelfth instruction is a direction switch:

```
1800042bc:  MOV RAX,[RDX]              ; RDX = Serializer*
1800042bf:  MOV RCX,[RAX+0x10]         ; ser->_encoding
1800042c3:  MOV RAX,[RCX]
1800042c6:  CALL qword ptr [RAX+0x10]  ; Encoding vtable +0x10 -> a mode
1800042c9:  CMP EAX,3
1800042cc:  JNZ 0x18000433e            ; the other arm
```

`FindOrLoad` (`0x1800bf320`), `IsDerivedFrom`, `GetReferenceId`, `GetPathForReferenceId` and
`AdjustDelimiters` all sit **inside the `mode == 3` arm**. WAVE9.md rows 27-28 record
`FindOrLoad` as having **no possible oracle** — it rewrites the one shared `ExternalFactory` AVL
tree, so ours-then-theirs mutates it twice.

So this is **not** the shape lane S relies on: a write-path `Serialize` that is a pure read of
the object. This body is direction-switched, and whether the reachable arm on a save is the
`FindOrLoad` one is a **measurement, not a reading** — one that costs a single run:

> Register a shadow (not a port) on `0x1800042a0`, log the value returned by the
> `[Encoding_vtable+0x10]` call, and run `BANISHED_AUTOSTART=1` with a save and no load. If the
> mode is never 3, lane S's premise transfers and one replayable port covers both addresses. If
> it is 3 on the write path too, these two join WAVE9.md rows 27-28 and stay on the seam.

Also worth noting against `reference_encoding_vtable_offsets_differ_between_builds.md`: this
call is at **retail's** `+0x10`, and the profile build's Encoding vtable offsets differ from
retail's. Whoever ports this must not take the offset from the profile DLL.

---

# 4. Registration policy — and why eight ported bodies are deliberately unregistered

Of the eleven destroy-helper addresses, exactly **three** are reached through
`ResolveAddr(Module::Runtime, …)` anywhere in this tree: `0x180052110`, `0x180005930` and
`0x1800059e0`, the three lane Q's non-trivial specs call. The other eight are reached only by
**retail's own bodies**, which this lane replaces. Verified by grep across `code/`, `engine/`,
`runtime/`, `standalone/`, `dx11/`: the single hit on `0x18001ce90` is a comment in
`renderer.cpp`, not a call.

They are ported all the same — the J2 entry points inline exactly the body and the spec each one
contains — and **not registered**, because `tools/runtime_census.py` reports a registration with
no caller under *"Ports with no caller in our tree — check each is not a vestigial resolve"*.
Eight permanent entries there would dilute the one signal this wave has for finding a dead port
(`reference_uncalled_port_is_not_verified.md`). Registering them buys nothing at cutover either:
with the DLL gone, only *our* callers exist, and ours go through the J2 entry points.

**The first draft did register all of them, and the census caught it** — eight new orphan rows
appeared in that table within one run. The list, with the spec that already covers each, is in
`array_destroy.cpp` above the `A_ENTRY` block, so a future caller is one line away and needs no
re-derivation.

The 12 registered addresses are therefore exactly the 12 the census counts as retired.

---

# 5. Corrections to lane D's and lane Q's triage

| # | Claim | Correction |
|---|---|---|
| 1 | lane D: J2 costs **+6 new rows**, J2b needs the CRT | **Zero new rows.** The six helpers are two bodies, and J2b's "CRT + per-element dtor" is Body A behind a 5-byte JMP thunk. |
| 2 | lane Q: `0x180052110` is "destroy-range for Audio's `EffectSlot` — **PORT WITH THE AUDIO HOLDBACKS**" | **Not Audio at all.** Body A, `offs {0x30}`, stride `0x40`, calls nothing but the allocator, not gated on lane A. Ported here. Its sibling `0x180051650` genuinely *is* Audio (it calls `0x180050d30`) and stays gated. |
| 3 | lane Q: the three String-releasing helpers "are likely one body with three strides" | Half right, and the halves matter: `0x1800059e0` and `0x180005930` are Body B (2 offsets and 1), but `0x180052110` is **Body A**, a different body entirely. Grouping by "these look alike" would have given one of the three the wrong stride *and* the wrong operation. |
| 4 | lane Q: `kSpec_Sprite38` has **no unwritten window** (`unwrittenLo = unwrittenHi = 0`) | **Wrong, in the direction that fails 100% of comparisons.** `0x180004220` writes `+0x00..+0x33` and stops; the element is `0x38` bytes, so **`+0x34..+0x37` is never written by the assign**. It has not bitten because lane Q's probe seeds all-zero elements, so those four bytes agree by accident. This is `reference_oracle_must_not_compare_unwritten_bytes.md` for the eighth time in this project. |
| 5 | lane D: `0x1800059e0` is 62 instructions, `0x180005930` is 43 | 70 and 47. Cosmetic, but the CSV is a work list. |
| 6 | lane D: J13's four dead fallbacks await deletion | **Already deleted.** `resolve_fallback_audit.py`: 166/5 → 163/0. |

**Correction 4 is deliberately NOT applied to lane Q's spec.** `unwritten*` is documented in
`array_internal.h` as the window retail's **default (init)** arm leaves alone, and this window is
on the **assign** arm. Widening that field's meaning from another lane, with no ability to run
lane Q's probe, is precisely how an oracle quietly stops comparing bytes it should compare. A
comment at the row states the finding and names this document; the change is lane Q's to make.

Two comments in `array.cpp` that this lane made false are corrected in the same change (the tree
has just spent commit `e9d81d9` on that class of drift): the "five new rows" list now says which
three no longer exist, and the reason those `ResolveAddr` sites still reach our code without an
init-ordering requirement (they are function-local statics that resolve on **first use**, not at
init) is written down where it is load-bearing.

---

# 6. Three jobs closed WITHOUT porting

**J12 — `0x18007fbfc` is MSVC's statically-linked `_beginthreadex`.** Confirmed as lane D read
it, and lane D's warning stands and is the important half: it is reached through
`ResolveRuntimeRaw` (`runtime/src/system/thread_win.cpp:129`), which **neither consults the port
registry nor records residue**. Its absence from every residue list is a tool blind spot, not a
clearance, and lane 0 must not score it as one. At cutover it is satisfied by calling
MinGW/UCRT's own `_beginthreadex`. **No port. One call-site swap, in lane F/G.**

**J14 — three `.data` addresses, none of them code.** `0x1801fe7c0` and `0x1801fe7e0` are
`_loader+0x08` and `_loader+0x28` on the exported `?_loader@LoaderLoop@Engine@@1VLoader@2@A`
(`0x1801fe7b8`), exactly as `engine/funcs/engine_funcs.h:377` already documents; `0x1801fc6d0`
is the 8-byte Modification-DLL `HMODULE` slot wedged between the two `Object` factory
singletons. **Lane C mints 8 bytes ×3. No port.**

**J13 — four dead fallback branches.** Already deleted (§1). **No port, no pending work.**

**J6 — `0x180009c30`, the allocator free-under-lock.** Lane E's, untouched. Noted here because
`0x180023130` (J5) calls it, so J5 cannot be ported without either lane E or one by-address call.

---

# 7. What lane D2 did NOT do, and why

**J11 `0x18001f470` (`Volume::Box` transform) — HELD BACK, and this is a real holdback, not a
stale one.** Lane D called it "the cheapest fully-verified port in the lane" and that is still
true: 151 instructions, pure math, no allocation, and its one callee `AsTransform3`
(`0x18001fcd0`) **is already ported** by lane P (`runtime/src/math/wave9_lane_p.cpp:172`), so its
frontier is empty and an ordinary `RT_DIFF` replay oracle works.

It is held back because **151 instructions of hand-transcribed vector math is the highest-risk
thing a lane that cannot compile or run could write**, and it is the one job here whose
correctness a `static_assert` cannot touch. Every body this lane did write is pointer arithmetic
with a shape a compile catches. That asymmetry is the whole reason, and it makes J11 the **ideal
first job for the next lane that owns a build** — clean frontier, replayable oracle, one address,
no coordination.

**J5 (the `Collection::Set`/`Map` AVL, 5 addresses) — triaged, not ported.** Same mnemonic-hash
sweep, and it collapses the way the Array cluster did, which is worth recording before somebody
re-derives it:

| addresses | ins | note |
|---|---|---|
| `0x180023170` **=** `0x18002a310` | 71 | **one body**, hash `9d8e760f97`, two instantiations |
| `0x1800236a0` **=** `0x180023570` | 90 | **one body**, hash `604cbba085`, zero callees each |
| `0x180023060` | 43 | node alloc; calls the exported `Allocate` |
| `0x180023130` | 18 | recursive free; calls itself twice and **J6's `0x180009c30`** |
| `0x18002a090` `Map_Insert` | 46 | calls `0x18002a170` (117 ins) + `MemFree` ×2 |
| `0x18002ace0` `Map_Find` | 67 | **zero callees** — a pure lookup, the cheapest of the nine |

So J5's nine addresses are **five distinct bodies**, not nine. Not ported here because its
frontier is genuinely open in a way J2's was not: `0x18002a170` reaches `0x18000f970`,
`0x18002a310` reaches `0x18002a590`/`0x18002a6c0`, and `0x180023130` reaches lane E's
`0x180009c30` — three more levels to price, and lane D's oracle note (no replay; both sides
mutate one tree) means the verification is bespoke. **It is the next lane's biggest single win
and the sweep above is the head start.**

**J7 / J8 / J9 / J10** — untouched, dispositions unchanged from lane D's ranking.

---

# 8. The oracle

`runtime/src/collection/array_destroy_probe.cpp`. Synthetic-subject replay, lane Q's shape:
build two private byte-identical subjects, run **ours** on one and the **shipped body** on the
other, compare. Two subjects means two buffers and two frees — a destructor cannot be replayed on
one subject without double-freeing it.

The shipped body is reached through **`ResolveRuntimeRaw`, never `ResolveAddr`**
(`reference_resolveaddr_consults_port_registry_first.md`): this lane registers these addresses,
so `ResolveAddr` would hand back our own function and every comparison would be ours-against-ours
and always green.

## 8.1 What is compared

| body | comparator | excluded, and why |
|---|---|---|
| **A** | plain `memcmp` of the whole `count * stride` buffer | nothing — every slot ends at zero on both sides, and everything else must be **untouched**. The buffer is seeded with a byte pattern so a wrong stride or offset writes its zeroes where the pattern notices. |
| **B** | per-slot **classification** (`NULL` / this side's own block base / unchanged / other), + the **refcount** of every surviving block, + raw bytes of everything that is not a slot word | the raw slot words: retail's freeing arm leaves `str-8`, a per-side allocation that will never agree between subjects. A `memcmp` here would fail 100% of cases. |
| `0x180003970` | `_size`, `_capacity`, and the `_size * 8` prefix | `_data` (two independent allocations); the extent past `_size` (allocator fill, never written) |

**Body B's refcount comparison closes lane Q's largest stated gap.** Lane Q's §"gaps" item 3 says
its three non-trivial instantiations were probed with all-zero elements only, so the
`INC dword [RAX-4]` refcount path "is **not reached by the probe**" and is "this lane's largest
single gap". This probe's subjects carry real synthetic `str-8` blocks with real refcounts, both
arms are driven, and the count afterwards is an assertion — the thing lane Q specified and did
not implement.

## 8.2 Controls

**Effectiveness** (`reference_probe_needs_an_effectiveness_control.md` — Wave 8.5 had 64 green
comparisons of which zero did any work). Every case reports how many slots it actually freed,
how many refcounts it decremented, how many reached zero and how many survived. The `n == 0` case
is counted **ineffective on purpose**: it drives the `TEST R8D,R8D / JLE` early return, and a
count that includes it would make `calls=` mean "was entered" rather than "did work". Two banners
fire:

```
*** EFFECTIVENESS CONTROL FAILED: every comparison was a no-op. This run proves NOTHING. ***
*** EFFECTIVENESS CONTROL FAILED: an ARM was never driven (freed=%d reached-zero=%d survived=%d).
    The String asymmetry is the only thing this comparator can see, and a run that does not
    enter both arms has not seen it. ***
```

The second is the one that matters: a green Body-B result with `reached-zero = 0` would be a run
that never entered the freeing arm — exactly the shape of lane Q's gap, and it now says so.

**Negative** (`=neg`). Three deliberately wrong bodies through the same comparator:

| | wrong how | must be caught by |
|---|---|---|
| A | drops the `slot+8 = 0` store | the raw `memcmp` |
| B | **swaps the two arms** | the classification |
| `=` | always trebles the capacity | `_capacity` |

If any compares EQUAL the probe prints `*** NEGATIVE CONTROL FAILED … the green result above is
worthless ***`. Wave 8.5's negative control found a defect in the *probe*; this one is aimed at
the same place. When `neg` is not selected the probe says so, in the summary, rather than letting
a green line be read as a verification.

**A defect the probe had, and the rule.** The first draft freed the probe's own `String` blocks
by inspecting the post-state — "if the slot is not the block base, the body did not free it". The
**swapped** negative body frees the block and *then* stores NULL, so that cleanup would have
double-freed every refcount-1 slot the instant the negative control ran: a harness manufacturing
its own crash, the shape `reference_longjmp_unwind_runs_retail_cleanup.md` is about. Cleanup is
now keyed on the **seed**, which is known before either body ran. *Never key a probe's cleanup on
a post-state the body under test is allowed to get wrong.*

**A control this probe cannot provide, stated as such.** The **order** in which slots are freed.
`0x1800059e0` releases `+0x08` before `+0x00`, `0x18005bce0` walks `0x38/0x28/0x08`, J2b runs
`0x1e0/0x1d0/0x1c0` backwards. The slots do not alias, so no comparator on the element buffer can
see the order — only the allocator can. The order is reproduced from the disassembly, asserted at
**compile time** to be descending (`static_assert`, so "sorted them while tidying up" cannot land
silently), and checked at runtime only by `BANISHED_REDZONE=watch`.

## 8.3 The runs

Each assumes a debug or perf exe built by the integrator and run from `C:\GOG Games\Banished`.
`[RtArrayDestroyProbe]` goes to `standalone_error.log`, `[RtDiff]` to `standalone_debug.log`.

| # | Command | What it covers |
|---|---|---|
| **D1** | `BANISHED_RT_ARRAY_DESTROY_PROBE=neg BANISHED_RT_DIFF=1 BANISHED_AUTOSTART=1 BANISHED_AUTOQUIT=1 make sa-run` | **The oracle.** 4 counts × 11 helper cases + 6 assign cases = **50 comparisons** against the shipped bodies, both controls. **This is the only run that verifies the bodies.** Expect `mismatches=0`, `did-work ≈ 38`, `reached-zero > 0`, `survived > 0`, and `negative: run=5 caught=5`. `AUTOQUIT` quits at the **main menu** (WAVE9.md's recorded trap) — fine here, the probe needs no gameplay. |
| **D2** | `BANISHED_RT_DIFF=1 BANISHED_AUTOSTART=1 BANISHED_PAUSEMENU=900 BANISHED_PAUSEMENU_QUIT=120 make sa-run` | Per-address `calls=` through gameplay **and a clean quit**. This is the run that says which REGISTRATIONS are live. |
| **D3** | `BANISHED_RT_DIFF=1 BANISHED_AUTOTUTORIAL=1 make sa-run` | **Not optional.** Scene teardown mid-run: the tutorial chain reloads the world, driving the `Renderer` / `Animation` / `Picking::Scene` dtors while the process is alive. Without it, `calls=` on nine of the twelve reflects one exit path. |
| **D4** | `BANISHED_RT_DIFF=1 BANISHED_AUTOMENU_BUTTON=8 make sa-run-menu` | The Achievements front-end: `0x180003970` (`AchievementManager`'s `operator=`). |
| **D5** | D2 with `BANISHED_REDZONE=watch` | **The half the comparator cannot see**: a wrong pointer handed to `Free`, and the free ORDER. |
| **D6** | `BANISHED_RT_STRICT=1` on D2 | The cutover check. All 12 must have LEFT `[RtResidue]`; nothing new should join it, because this lane adds no Runtime address it does not register. |
| **D7** | D2 with `BANISHED_RT_ARRAY_DESTROY=0`, then with `BANISHED_RT_COLLECTION=0` | The A/B. **In that order** — the two are not independent: with `BANISHED_RT_ARRAY_NONTRIVIAL=0`, lane Q's three specs stop calling `0x1800059e0`/`0x180052110`/`0x180005930` at all, so that switch masks three of this lane's registrations rather than isolating them. |

## 8.4 Per-address coverage — what these runs would and would NOT reach

**The probe (D1) verifies the BODY for the ten helper parameterizations and for
`0x180003970` by construction**, because it drives every spec directly. What varies per address
is whether the **registration** is exercised by a real caller — which is what `calls=` reports.

| address | live caller | reached by |
|---|---|---|
| `0x180010ab0` | `IO::Mod` list cleanup (`engine_funcs.cpp`, `loops.cpp`) | D2, D3 |
| `0x18001c7e0`, `0x18001c8c0` | `Graphics::Renderer` teardown | **D3** primarily; D2 only at exit |
| `0x180033880` | `Engine` teardown | clean process exit |
| `0x1800416d0` | UI widgets layout | D2, D4 |
| `0x18005bf60` | `Graphics::Animation` teardown | **D3** primarily |
| `0x180073c90` | `Picking::Scene` dtor | **D3** primarily; D2 only at exit |
| `0x180003970` | `AchievementManager` | D1 (boot), D4 |
| `0x180052110` | lane Q's `kSpec_EffectSlot` shrink path | D2 — **only if an `Array<EffectSlot>` ever SHRINKS** |
| `0x1800059e0` | lane Q's `kSpec_Sprite38` shrink path | D1 (boot resource load) — same caveat |
| `0x180005930` | lane Q's `kSpec_StringExt` shrink path | D2 — same caveat |
| `0x180004220` | lane Q's `kSpec_Sprite38` element assign | D1 — boot resource load, hot |

### The gaps, named

1. **Nine of the twelve are teardown-only.** D3 is not optional; without a world reload,
   `calls=` on the scene and renderer dtors reflects one exit path rather than use. An
   interrupted D1 or D2 makes `0x180033880`'s line meaningless
   (`reference_uncalled_port_is_not_verified.md`).
2. **The three shrink-path helpers may legitimately report `calls=0`.** Lane Q's specs call them
   only when an Array *shrinks*, and a shrink is rare. A zero there is not evidence of a dead
   port and must not be read as one — the body is verified by D1 regardless, and D1 is the run
   that matters.
3. **The seven OUTER bodies are not replayed, by construction.** An outer body's last act is to
   free the element buffer, so afterwards there is nothing to compare: reading the buffer is a
   use-after-free and the 16-byte header is all zeroes on both sides. They are covered by
   **composition** — each is exactly `helper(this, _data, _size)` then `ArrayDtorImpl`, the helper
   is replayed and `ArrayDtorImpl` is lane Q's already-replayed J1 body — plus the live post-state
   check. That is weaker than a replay and is not counted as one.
4. **`0x18005c550`'s nesting is not replayed.** Its two sub-helpers are; the recursion is not,
   because a wrong inner count is a wild free that this comparator cannot see and
   `BANISHED_REDZONE=watch` can. D5 is where that is checked, or nowhere.
5. **The eight unregistered helper bodies have no `calls=` line at all**, by design (§4). D1
   exercises seven of their eight parameterizations against the shipped body; the eighth is
   `0x18005c550`, item 4.
6. **The live `[RtDiff]` checks in `array_destroy.cpp` are a coverage counter, not an oracle.**
   The post-state check restates the rule the function implements. The one genuinely independent
   live check in this lane is `0x180004220`'s `memcmp(dst, src, 0x34)` and `0x180003970`'s prefix
   compare, both of which compare against data we did not write.

---

# 9. What lane D2 asks of the integrator

1. **Run D1 first, with `=neg`.** It is the only run that verifies the bodies. If the negative
   control does not fire, stop — nothing else in this lane means anything.
2. **`RtCollectionDestroyPortInit()` is called immediately after `RtCollectionPortInit()`** in
   `RuntimePortInit()`. The ordering matters for **one** reason: this lane arms a probe that
   drives lane Q's `ArrayDtorImpl`. The *registrations* are order-independent, because
   `array.cpp` reaches this lane's three shared addresses through function-local
   `static Fn fn = ResolveAddr(...)`, which resolves on first **use** — a real Array call, long
   after either init.
3. **Bisect in the order in §8.3 D7.** `BANISHED_RT_ARRAY_DESTROY=0` isolates this lane;
   `BANISHED_RT_ARRAY_NONTRIVIAL=0` does **not**, it masks three of its registrations.
4. **Nothing here touches the heap.** Every free goes through lane Q's `rtcoll::MemFree`
   (the exported `0x1800c7ad0`). `BANISHED_RT_ALLOC` remains lane E's, under its standing NO-GO,
   and lane Q's LANE E DEPENDENCY note in `array.cpp` covers this file unchanged.
5. **If this lane is reverted, revert `array.cpp`'s two comment corrections with it** — the old
   claims are load-bearing for a reader when the registrations are gone.

# 10. Handoff, in priority order

| | Job | Why it is next |
|---|---|---|
| 1 | **J11 `0x18001f470`** | Empty frontier (`AsTransform3` is lane P's port), ordinary `RT_DIFF` replay, one address. Held back here only because this lane could not compile it. |
| 2 | **J5, the AVL** | 9 addresses, **5 bodies** (§7 has the sweep). Biggest remaining win. Needs lane E's `0x180009c30` or one by-address call. |
| 3 | **`0x1800042a0` / `0x180005dc0`** | One body, two addresses. Blocked on **one measurement**, spelled out in §3 — not on a port. |
| 4 | **Correction 4** | Lane Q's `kSpec_Sprite38` unwritten window. A live oracle defect, waiting on the lane that owns the field's meaning. |
| 5 | J7 / J8 / J9 / J10 | Unchanged from lane D's ranking. |
