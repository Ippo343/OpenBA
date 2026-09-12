# WAVE 23 INTEGRATOR LOG — open items, in flight

Base `wave23/base` at `a60351e3`. Ten lanes. **Nothing merged yet.**

## 1. CORRECTIONS PENDING AGAINST THE BOARD — do not quote 1314/1137/86.5% at close

Three separate corrections are landing. **The close figure must be re-measured on the merged
tip with the corrected tool, and must name which tool version produced it.**

| # | correction | source | direction |
|---|---|---|---|
| 1 | census **understated by 1** — `0x180009c30` registered unconditionally inside a file-level-gated file | W22 ALLOC | +1 ported |
| 2 | census **overstated by 2** — `?Allocate@`/`?Free@` registered ONLY in armed-only branches (a **fourth gating shape** no existing mechanism sees) | W22 ALLOC | −2 ported |
| 3 | `ordinal` holds **3 PHANTOM rows**, double-counted against exports that already read `ported=1` | W23 INTERNAL | board 177 → **174**, `Object` 10 → **11** |

**Integrator ruling on #2 (recorded deliberately):** the count MOVES. Five `MemoryAllocator`
rows are excluded from `ported` precisely because a gated registration is not a port; counting
`?Allocate@`/`?Free@` while excluding their five siblings is not conservatism, it is an
inconsistency that flatters the number. Verified independently before ruling —
`standalone/rt_alloc_swap.cpp:382` opens `if (!g_swapTaken) return;` (`g_swapTaken` false at
:112, set true only at :320), and `allocator.cpp:1371`/`:1428` are the cutover and ARMED
branches. ALLOC's stated reason to hold back does not apply: the Wave 23 lanes branched from
`a60351e3`, which predates its census change, so they were already on a different tool.

## 2. `kTypeInfoStorage` IS NEVER WRITTEN — a precondition on 104 rows, verified here

Lane ACHIEVE reported it; **the integrator verified the mechanism rather than relaying it**:

- `standalone/rt_data_mint_tables.inc:1607` — `static RtObject::BaseTypeInfo kTypeInfoStorage[104];`,
  zero-initialised.
- Every reference to it is a **read or an address-take**: `RtMintIsMintedTypeInfo` is a range
  test, `RtMintTypeInfoStorageProbe` is a probe positive control. **No writer anywhere.**
- `RtMintPromotionStorage` (`rt_data_mint.cpp:793-814`) validates the symbol and the VA, then
  `return r.storage;` — **it does not seed content.**

So **any `?_typeInfo@` promotion publishes a zeroed `BaseTypeInfo` (type id 0, name null)**, in
any namespace. This is a precondition on 104 rows and no wave doc names it.

**IT IS ALSO ALREADY LIVE, AND THAT IS THE PART TO RESOLVE, NOT ANNOUNCE.**
`standalone/reg_mint_promoted.cpp` already promotes `?_typeInfo@` rows (the ten UI rows from
Wave 10, plus others). If a zeroed node were harmful on a dispatched path, UI would be broken —
and it is not. The coherent reading, consistent with `runtime/src/object/factory_dispatch_probe.cpp`
existing at all (it measures "0 minted nodes dispatched") and with lane GFXVT's finding that
abstract tables measure attendance 0, is that **the promoted `?_typeInfo@` rows are inert —
promoted but never dispatched.** That is a hypothesis with good corroboration, **not a
measurement**, and it is recorded as such.

**This strengthens lane ACHIEVE's refusal** to port 15 slot bodies toward a promotion that is
harmful today — the Wave 20 §4 "safest only because it was pointless" mode.

**ADDRESSEE: the Wave 24 kickoff writer**, as a kickoff item and not a lane task. It must be
settled by measurement before any further `?_typeInfo@` promotion in any namespace.

## 3. A REAL BUG CAUGHT PRE-COMMIT IN THE CENSUS

`tools/runtime_census.py:1493` in lane ALLOC's tree: **`gated_func_spans` is not defined** — a
bare `NameError`, categorically different from the union-type noise ALLOC had already correctly
diffed against `wave22/base` and dismissed. ALLOC's end-to-end run returned 910 lines and exit
0 because **the line sits on a branch that run did not take.** "It ran clean end to end" does
not cover a line that did not run — the same shape as ALLOC's own best finding this wave, where
the perf binary could not say whether it had armed and a silent refusal read as a clean result.
Caught before commit and sent back.

## 4. CLOSED LANES — four of ten

| lane | rows moved | headline |
|---|---|---|
| **GFXDATA** | 0 of 7 | correct. **Promotion safety is governed by unported READERS, not writers** — two rows gained a ported writer and the verdict did not flip (`_singleton@AnimationManager` still has 18 unported readers) |
| **GFXVT** | 0 of 8 | **readiness and value are ANTI-CORRELATED**: both COMPLETE tables are abstract bases, overwritten by the derived ctor one statement later, so nothing dispatches them. The sorting axis is the **purecall count**, which the backlog already prints and nobody reads as a verdict |
| **OBJ** | 0 of 10 | correct — but **4 of 10 tags INVERT at cutover** and none said so. Scoped expiries 1 → 11, addressed 1 → 11, grandfathered 40 → 30 |
| **ACHIEVE** | **7 of 16** | Achievement 36 → 43 (branch-local). A **14-wave-old blocker was stale and its discharge was already in the same directory** in identical words |
| **INTERNAL** | n/a (instrument) | the 42 unknowns are **30 unexported functions + 12 statics, all already named in our own source comments** — the census reads names and a comment is not a name. Real work in the bucket: **8 bodies** |

## 5. CROSS-LANE RELAYS — one refuted, which is the point

The integrator relayed GFXDATA's claim that promoting `??_7AnimationEvent@`/`??_7Particle@`
would unblock attendance on three `?_typeInfo@` rows. **Lane GFXVT REFUTED it**: those tables
are abstract, so promotion would measure **attendance 0** — a zero that would then indict a
sound DATA row. GFXDATA's premise was necessary, not sufficient; only `??_7MaterialInstance@`
is the real unblocker.

**The relay was framed as evidence to weigh rather than an instruction, and the lane checked
it. That is the only reason it did not propagate.** Record the framing, not just the outcome.

## 6. INSTRUMENT FINDINGS THAT OUTLIVE THIS WAVE

1. **Runtime-x64.dll is `/GR-` — RTTI is OFF.** Any vtable-identification screen built on RTTI
   has no positive case in this binary and can only return 0, which reads as "none of these are
   vtables". Barred from driving a verdict. (INTERNAL)
2. **The `.pdata` trap bites the OPPOSITE way from how it is recorded.** The *entry* fragment is
   precisely the one reading `chained=False`, so a chained function looks complete.
   Understatement up to **20.4x** (13 bytes for a 265-byte body). Fix: union *forward*. (INTERNAL)
3. **52.9% of this DLL's `.text` exports have no `.pdata` at all** — leaf functions. A control
   failed; the control was wrong, not the screen. (INTERNAL)
4. **A build check cannot detect a misregistered port.** ACHIEVE's negative control (a
   well-formed phantom mangling) was caught by the census and **not** by the build, which stayed
   clean and still linked.
5. **Nothing auto-discovers gates.** A screen reporting only *declared* opt-in gates reads clean
   for every gate nobody declared. Two instances in two waves (`BANISHED_RT_ALLOC`,
   `BANISHED_RT_DLGFACTORY`) makes it a class.
6. **A blocker discharge does not propagate.** ACHIEVE found a 14-wave-old holdback whose
   discharge already sat in the same directory, in identical words, for two sibling functions.
