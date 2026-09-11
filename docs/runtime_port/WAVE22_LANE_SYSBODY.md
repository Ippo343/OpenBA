# WAVE 22 LANE SYSBODY — the six genuinely unported System bodies

Branch `wave22/sysbody`, cut from `wave22/base` at `37706651`. Closed 2026-09-11.

## 0. HEADLINE

**Five of six rows moved. The sixth — `??_7Thread@System@@6B@` — is NOT promoted, on
purpose, and the reason is route-scoped.**

> **BRANCH-LOCAL FIGURES — NOT WAVE NUMBERS.** System **78 → 83 ported** (gap 25 → 20),
> TOTAL **1129 → 1134**, **86.0% → 86.4%**. These were measured with
> `tools/runtime_census.py` on **this lane's isolated worktree tip, where only this
> lane's changes exist**. They describe this branch and nothing else. Lane ERRCFG
> independently measured 1314/1132 on *its* tip; both are honest about their own tree
> and **neither describes the tree that will exist after the merge**. That conflation
> was Wave 21's closing defect. **The integrator re-measures on the merged tip; quote
> that, not this.**

Two things this lane found that outlive it:

1. **`?StrCompare@String@System@@SAHPEBD0@Z` was never unported.** It has had a correct
   body since Wave 2 (`runtime/src/system/string.cpp:157`) and was simply never
   registered. It has read as an unported System `BODY` for twenty waves. The Task-0 grep
   caught it before a line was written. **Third recorded instance** of
   `reference_half_registered_port_reads_as_absent.md`.

   **And the body carried a latent defect that NO TEST WE CAN RUN WOULD CATCH.** Retail
   widens the two compared bytes with `MOVSX` (`1800c98ec`/`1800c98f1`); the Wave 2 body
   spelled that `static_cast<int>(a[i])` on a plain `const char*`. That is the same
   value **only while `char` happens to be signed** — which it is for MSVC and for MinGW
   on x86-64 Windows, i.e. for *every build this project produces*. No run, no sweep and
   no oracle on this toolchain can distinguish the two spellings, because the toolchain
   never produces the case where they differ. It was found by reading the disassembly
   and fixed by making the widening explicit (`signed char`); the sweep's `highBit`
   bucket (228 cases) is what now *exercises* it, and the negative control in §5.1
   confirms that bucket is live — but the defect itself was never discoverable by
   testing, only by reading.
2. **A note I wrote in this lane was FALSE, and my own negative control caught it.**
   The first draft of the Compress banner claimed the 32-bit rolling hash register's
   upper byte "CHANGES THE BUCKET" and that masking to 24 bits would be a silent defect.
   A control that injected exactly that mask reported **0 divergences over 40 cases**.
   The sweep was right and the note was wrong — the upper byte is provably unobservable
   (proof in the corrected note). This is
   `reference_control_must_pick_a_compared_subject.md` from the other side: a control
   that corrupts a value nothing observes reads as an indictment of a sound instrument.

## 1. TASK 0 — THE PRE-PORT GREP, BEFORE ANY PORTING

Denominator: `runtime/`, `engine/`, `code/`, `standalone/`, `dx11/` for `*.cpp *.h *.hpp
*.c *.inc`, each of the six symbols plus its plausible unmangled spellings.

| row | grep result | disposition |
|---|---|---|
| `?StrCompare@String@System@@SAHPEBD0@Z` | **BODY EXISTS** at `string.cpp:157`, declared in `string.h:33`, used by 6 in-tree callers. **NOT in the 20-entry registration list.** | **REGISTER ONLY** — no body written |
| `?Compress@Compression@System@@…` | No body. Three comment hits, one of them a registration block saying **"DELIBERATELY ABSENT"** with a stated reason | Holdback re-checked → **PORTED** |
| `??0Compression@System@@QEAA@XZ` | No body. Reached raw from `compressed_stream_write.cpp:714`, `saveload_meta.cpp:421` | **PORTED** |
| `?DateAsString@DateTime@System@@…` | No body. **A live caller already exists**: `value_types.cpp:347` resolves it by name through `RT_EXPORT_ONCE` | **PORTED** |
| `?TimeAsString@DateTime@System@@…` | Same, `value_types.cpp:349` | **PORTED** |
| `??_7Thread@System@@6B@` | Minted row exists; `RtSystemThreadVftable()` reads it through `RT_DATA_SLOT` | **NOT PROMOTED** — §4 |

The `StrCompare` finding is the one that mattered: had I not grepped, I would have
written a second body and the diff table would have caught it, exactly as it did three
times in a prior wave.

## 2. THE HOLDBACK ON `Compress`, DISCHARGED RATHER THAN DROPPED

`compression.cpp`'s registration block recorded Compress as deliberately absent because
the file's own `Decompress` probe **uses the shipped Compress as its input generator**,
and porting your test-input generator is how a shared misreading passes its own test.
That was the right call when written. Re-checked rather than inherited
(`reference_holdback_blockers_are_stale_check_them_first.md`), the premise is **false by
mechanism**:

```
runtime/funcs/rt_port.h:154  RT_ORIGINAL_ONCE(Fn,NAME) -> RtOriginal<Fn>(NAME)
runtime/funcs/rt_port.h:140  RtOriginal = ResolveExportRaw   // does NOT consult the registry
```

All three of the probe's Compression doors (ctor, Compress, dtor) are `RT_ORIGINAL_ONCE`.
Registering our encoder therefore **cannot** change what that probe calls; it keeps
generating its corpus with the shipped encoder. At cutover the probe is compiled out
entirely. The old block is replaced in-tree with this argument, not deleted.

**`??1Compression@System@@` stays on the DLL.** It is not a census row and not in scope.
Our ctor allocates through `RtMemAlloc` → `?MemAlloc@MemoryAllocator@System@@SAPEAX_KH@Z`,
which I decompiled and confirmed is *exactly* the `EnterCS(_singleton+0x640) / Allocate /
LeaveCS` sequence the ctor inlines.

> **CORRECTION.** This section first said "at cutover the dtor becomes a required row".
> **That was wrong**, and re-checking it while re-pointing the residuals is what found it.
> Retail does not destroy a Compression through its dtor at all on the live path: the
> teardown is **inlined into the already-ported `CompressedStream::Release`**, which
> frees the 0x40000 table and the object directly through `MemFree`. Full evidence in
> §7.1. The claim is corrected here rather than deleted so the retraction stays visible.

## 3. WHAT WAS READ FROM THE DISASSEMBLY, NOT THE DECOMPILER

- **`DateAsString` / `TimeAsString`**: Ghidra prints both as
  `void __thiscall f(DateTime *this)` — **no second parameter and no return**. The hidden
  return slot is a real ABI argument (`1800c56a0 MOV [RSP+0x10],RDX` … `1800c5773 MOV
  RAX,RBX / RET`), so the signature is `String* f(const DateTime*, String* ret)`.
  Transcribing the decompiler's rendering would have been
  `reference_hidden_retslot_wild_write.md`.
- The two bodies differ in **exactly two instructions**: `GetDateFormatW` flag 1
  (`DATE_SHORTDATE`) vs `GetTimeFormatW` flag 2 (`TIME_NOSECONDS`). They share one helper.
- **`StrCompare` returns a SIGN-EXTENDED difference** (`1800c98ec MOVSX`). The Wave 2 body
  spelled it `static_cast<int>(a[i])` on a plain `char`, which is only correct while
  `char` happens to be signed — true for this toolchain, and therefore invisible. Made
  explicit (`signed char`). The sweep's `highBit` bucket (228 cases) is what tests it.
- **`Compress`** is 287 instructions rendered by Ghidra as a 60-line ternary nest; the
  whole body was read from the disassembly. Six named hazards are documented at the body,
  including the strict `m_pos > src` guard and a **one-byte tail overrun that is retail's**
  (3 bytes reserved, up to 4 writable) — left unguarded, per project rule, because a guard
  changes the return value rather than protecting anything.

## 4. `??_7Thread@System@@6B@` — NOT PROMOTED, AND THE VERDICT IS ROUTE-SCOPED

The backlog's **1/1 COMPLETE is CORRECT**: the table has 2 slots, slot 0 is `_purecall`,
slot 1 is `OnCompleted`, and `OnCompleted` is ported. But COMPLETE says a promotion puts
no trap in a live table; it does not say the row is promotable
(`reference_a_green_readiness_screen_is_not_a_route.md`). Promotability turns on owning
what **stamps** the table, and I determined that:

- **We own one stamper**: `RtSystemThreadVftable()` (`thread_win.cpp:142`) is a real
  `RT_DATA_SLOT` door, feeding our ported Thread ctor/dtor. All three constructing call
  sites (`code/Game/Game/pathfinder_seed.cpp:143`,
  `runtime/src/engine/singleton_ctors.cpp:448`, `standalone/rt_data_mint.cpp:505`) reach
  us **by name**.
- **We do not own the other**: retail's own Thread ctor stamps `0x180155f20` with a
  RIP-relative LEA inside the DLL. Not a door, and never will be
  (`reference_a_ported_row_can_be_permanently_unpromotable.md`).

| route | verdict | why |
|---|---|---|
| **DLL present** | **DO NOT PROMOTE** | Puts two vptr values in play for one class where today there is one, and buys nothing: the only dispatched slot is slot 1 and both sides' `OnCompleted` are a bare `RET`. Low cost, **zero** benefit. |
| **Cutover** | **REQUIRED** | `RT_DATA_SLOT` has nothing to fall through to; our ctor would stamp a NULL vptr into every Thread. This is the attempt-14 failure one layer along. |

Opposite verdicts, the shape `reference_unpromotable_is_route_scoped_not_permanent.md`
names — neither may be quoted without its route. Recorded in `thread_win.cpp`.

**Also fixed there**: that comment block named the mint slot array `kVSlots42`. It is
`kVSlots53` today — generator-assigned indices renumber silently
(`reference_a_generated_file_goes_stale_with_no_detector.md`). The name is **dropped**
rather than corrected; the row is findable by its symbol, which does not drift.

## 5. VERIFICATION — METHOD, AND THE EFFECTIVENESS CONTROL

All three sweeps are **input sweeps against the shipped export**, gated on
`BANISHED_RT_DIFF`, with `ours` called as a direct C++ call and `theirs` through
`ResolveExportRaw`. Neither side reaches the other's body by address, so no probe is a
self-check. Run: `BANISHED_RT_DIFF=1 BANISHED_AUTOSTART=1 BANISHED_AUTOQUIT=1`, exit 0,
empty crash log, game directory lock held (`tools/game_dir_lock.sh`, bare, never piped).

```
[SYSBODY/StrCompare] sweep: 784 cases, 0 divergences (FULL int, not sign)
[SYSBODY/StrCompare] classes: equal=27/0 diffAt0=520/520 diffAtN=70/70 prefix=112/112
                              highBit=228/228 nulls=55/54
[SYSBODY/StrCompare] control: 64 distinct return values; ALL SIX CLASSES; DISCRIMINATE -- VERIFIED
[SYSBODY/Compress]   sweep: 40 cases, 0 length div, 0 byte div (accepted=32 rejected=8)
[SYSBODY/Compress]   independent round-trip (ours->ours): ok=32 bad=0
[SYSBODY/Compress]   branches: literal=38278 literalFlush=205 matchShort=16573
                               matchLong=3443 matchUnroll=19981 failDst=6 -- ALL SIX
[SYSBODY/Compress]   control: 32 distinct lengths; NOT UNIFORM -- VERIFIED
[SYSBODY/DateTime]   sweep: 12 cases (11 compared, 1 UNCOMPARABLE), 0 date div, 0 time div
[SYSBODY/DateTime]   control: 10 distinct dates, 11 distinct times -- VERIFIED
```

**Why `calls=` is not the metric here.** `Compress` has no caller in this tree or in the
game (only the ToolKit writes packages), and the DateTime pair is reached only from
save-file metadata an autostart run may not touch. Waiting for a caller would leave all
three at `calls=0` forever; for a function that is pure given its object, a sweep is
strictly the better instrument (`reference_project_pure_functions_verify_by_sweep.md`).

**`StrCompare` is NOT verified by a uniform population.** The named hazard on this row is
a prior inline strcmp that returned 0 for *every* distinct pair sharing a first byte while
a shared BST find still "worked". The sweep therefore classifies **on the inputs, never on
the answer**, and requires every class to be non-empty *and* every non-equal class to be
100% non-zero. `diffAtN` (70/70) is precisely that defect class. Comparison is on the
**full int**, not the sign: sign-only agreement would pass a body that got the magnitude
wrong, which is the specific error the Wave 2 loop-ordering comment exists to prevent.

### 5.1 The negative control — three injected defects, one build

| defect | subject it corrupts | expected | result |
|---|---|---|---|
| `StrCompare` return widened `MOVZX` not `MOVSX` | the `highBit` class (228 compared cases) | CAUGHT | **CAUGHT** — 220 divergences, verdict flipped to `*** NOT a verification ***` |
| `GetTimeFormatW` flag 2 → 0 | the time string | CAUGHT | **CAUGHT** — `ours='8:00:00 PM' theirs='8:00 PM'` on every comparable case |
| `dv` masked to 24 bits | *(believed: the hash bucket)* | CAUGHT | **NOT CAUGHT — and the instrument was right.** See below |

The third is the finding. `hash = (uint16)((dv>>8) - 5*dv)`; writing `dv = M + (U<<24)`,
`(dv>>8)`'s low 16 bits are `M>>8` (U sits at bit 16+), `5*dv`'s low 16 bits are `5*M`'s
(`5*(U<<24)` is all bits 24+), and subtraction borrows propagate upward only — so the
hash is **independent of U**, and `dv` feeds nothing else. The mask is a provable no-op.
The control corrupted a value nothing observes; `NOT CAUGHT` was the correct answer and
**my source comment was the thing that was wrong**. Corrected in place, with the proof.

Re-run with a control that *does* pick a compared subject — dropping one of the two
post-match hash-table backfills (hazard note 5) — gives:

```
sweep: 40 cases, 21 length divergences ... -- *** NOT a verification ***
independent round-trip (ours->ours, DLL not involved): ok=11 bad=0
```

**The round-trip passed 11/11 on the defective build while the byte/length comparison
caught it 21 times.** That is the concrete demonstration that the round-trip is the weak
oracle on this row and the comparison against the shipped encoder is what does the work.

### 5.2 What the DateTime sweep CAUGHT

The first draft wrote `FILETIME local{}; SYSTEMTIME st{};`. The sweep reported a
divergence at FILETIME 0 (the 1601 epoch): `ours='㕀烏翶' theirs='僝᳇'`. Retail's prologue
zeroes none of those slots and **ignores both `BOOL` results**, so on a rejected FILETIME
it formats from an uninitialised stack buffer. Zero-initialising is a behaviour change on
the failure path — invisible on the success path, and exactly the tidy-up the project's
rules forbid. The locals are now uninitialised (faithful), and the sweep **re-runs the two
conversions itself** to bucket rejected inputs as `UNCOMPARABLE`, printed with the reason,
rather than scoring a comparison that is meaningless on both sides. Classification is on
the input, never the output.

## 6. WHAT I DID **NOT** CHECK

- **Locale.** The DateTime sweep ran under one locale, so it cannot distinguish our
  `GetDateFormatW` flag `1` from any other flag that renders identically there. The flags
  are read off the disassembly instead, which is the stronger source — but no
  multi-locale run was done.
- **Cutover for every row.** All verification is **DLL-present**. Nothing here was run
  without Runtime-x64.dll, and the cutover statements in §2 and §4 are *derived* from how
  `RT_DATA_SLOT` / `ResolveExportRaw` behave, **not observed**.
- **`Compress` against a real ToolKit package.** The corpus is Runtime-x64.dll's own image
  bytes, synthetic repetitive patterns, PRNG bytes and degenerate lengths — 40 cases, not
  a `.pkg` round-trip. No package was written or re-read with our encoder.
- **Threading.** `Compress` was exercised single-threaded. The 0x40000 table is per-object
  and retail takes no lock around it, so this is believed irrelevant; it is not tested.
- **`Compress` inputs above 0x8000 bytes**, and inputs where `dstCap` sits exactly on the
  tail's 1-byte overrun boundary. The overrun is documented from the disassembly, not
  provoked.
- **`?StrCompare`'s two live consumers** (`ui/dialog_create.cpp:357`, `ui/element.cpp:215`)
  both cache the resolved pointer in a **plain function-local static**. **The sweep
  verifies the BODY; it does not verify those two ROUTES.** §7.2 argues from the
  initialisation order and from `port_bypass_audit.py` that both pick up our body, and I
  believe that argument — but it is **derived, not observed**. I did not run with
  `BANISHED_RT_STRICT` to confirm `StrCompare` is absent from the residue list, which is
  the measurement that would settle it.
- I ran no filesystem-wide scans; all searching was worktree-scoped.

## 7. RESIDUALS — RE-CHECKED, AND TWO OF FOUR DISSOLVED

The integrator returned the first draft of this section because its addressees read
*"the next lane to touch either file"* and *"the lane that next runs a cutover boot"*.
**Those are conditions, not addressees** — the form that satisfies a seal-time checklist
and then never fires, because no lane ever identifies itself as that lane
(`reference_expiry_clause_needs_an_addressee.md`; it is why lane GEOM had to exist this
wave to pay off a Wave 6 prescription that sat unapplied for fifteen waves).

Re-pointing them meant re-checking them first, and **two of the four turned out to be
false** — both of them mine, both one turn old. That is the same lesson as §2 and §5.1
for the third time in one lane: *re-check a stated blocker before acting on it, including
your own.*

### 7.1 RETIRED — `??1Compression@System@@` is NOT a cutover-required row

I claimed in §2 that porting the ctor makes the dtor cutover-required. **False.** The
Compression teardown is **INLINED into `?Release@CompressedStream@IO@@`**, which is
already ported and registered (`runtime/src/io/compressed_stream_release.cpp:104-113`,
retail `0x1800aa1f3..0x1800aa26f`):

```c
void** const comp = *reinterpret_cast<void***>(base + 0x60);
if (comp) {
    if (comp[0]) MemFree(comp[0]);   // <- the 0x40000 table OUR ctor allocates
    comp[0] = nullptr;
    MemFree(comp);
}
```

Retail never calls `??1Compression@` on the live path; it frees the table and the object
directly, through the same `MemFree` door our ctor allocates through. The only two
in-tree references are inside diff-guarded probes that die with the oracle at cutover
(and this lane's own probe explicitly SKIPs on a null dtor). **Nothing to hand to
anybody.** The §2 sentence is corrected in place rather than deleted, so the retraction
is visible.

### 7.2 RETIRED — the two `StrCompare` consumers resolve AFTER registration

`ui/dialog_create.cpp:357` and `ui/element.cpp:215` are `static Fn fn =
ResolveExport(...)`, and I flagged them as possibly freezing the DLL's address. Two
facts close it:

- **They use the port-AWARE door** (`ResolveExport`, not `RtOriginal`), so they are not
  bypasses — `tools/port_bypass_audit.py`, re-run after this landing exactly as
  `value_types.cpp` instructs ("re-run after every LANDING, not after every edit"),
  reports **no row** for `StrCompare`, `dialog_create.cpp` or `element.cpp`.
- **Registration strictly precedes any UI.** `RuntimePortInit()` is `standalone/main.cpp:1736`,
  and the comment there states it must run *before* `AddressRegistryInit` and
  `EngineFuncsInit` precisely so no port is "resolved by nobody". Dialog and element
  construction cannot precede it.

**Caveat, stated rather than smoothed: this is DERIVED, not OBSERVED.** I did not run
with `BANISHED_RT_STRICT` to confirm `StrCompare` is absent from the residue list. The
argument is structural and I believe it; it is not a measurement. Carried into §6.

### 7.3 OPEN — `??_7Thread@System@@6B@` at cutover
**Owner: the integrator, in the Wave 22 close handover.** I cannot name a Wave 23 lane
honestly because no cutover lane is planned and inventing one would be the same defect in
a new coat.

- **Precondition that makes it live:** the first cutover boot that constructs a
  `System::Thread` — which is every cutover boot, because `ConstructLoader` builds one
  during `RtDataMintInit`.
- **Symptom if missed:** a null vptr stamped into every Thread; an `EXEC-of-0` or
  `VCALL-of-0` whose top frame will *not* name this row.
- **Action:** register the minted row at cutover. Do **not** promote it before then (§4).
- Recorded at the site (`runtime/src/system/thread_win.cpp`, the
  `RtSystemThreadVftable()` banner) as well as here, so a cutover lane reading the code
  finds it without reading this doc.

### 7.4 OPEN — a real package round-trip for `Compress`
**Owner: the integrator, to place in the Wave 22 close handover as a scoping item for
whichever Wave 23 lane takes IO packages** (the natural home; `PackageFile`/
`CompressedStream` work already lives there).

- **Precondition:** any lane that already has a `.pkg` read path stood up.
- **Why it is worth doing:** §5.1 demonstrated that the round-trip is the *weak* oracle
  on this row and the byte comparison is what does the work. A real package would add an
  independent *input distribution*, not an independent check — the corpus today is
  Runtime-x64.dll's own image bytes plus synthetic patterns.
- **It is an improvement, not a gap:** the encoder is verified as it stands (40 cases, 0
  length and 0 byte divergences, all six branches). If this is never done, nothing
  regresses.

## 7.5 WHAT THE INTEGRATOR IS BEING HANDED

Two items, both explicitly owned rather than left to a condition:

| # | item | precondition | why it cannot self-surface |
|---|---|---|---|
| §7.3 | register the minted `??_7Thread@System@@6B@` row at cutover; do **not** promote it before | the first cutover boot (every one — `ConstructLoader` builds a Thread) | the crash names an address, not this row |
| §7.4 | a real `.pkg` round-trip as a second input distribution for `Compress` | a lane with a `.pkg` read path already stood up | it is an improvement; nothing fails if it never happens |

§7.1 and §7.2 are **retired, not handed over** — both were my own claims and both were
false on inspection.

## 8. FILES CHANGED

```
runtime/src/system/string.cpp        StrCompare: signed-char widening made explicit;
                                     REGISTERED; RtSystem_StrCompareProbe() added
runtime/src/system/compression.cpp   CompressBody + ctor ported and registered; holdback
                                     block rewritten; branch counters; RtSystem_CompressProbe()
runtime/src/system/value_types.cpp   DateAsString/TimeAsString ported and registered;
                                     RtSystem_DateTimeProbe(); stale "unported" note corrected
runtime/src/system/thread_win.cpp    route-scoped promotion verdict; stale kVSlots42 dropped
runtime/src/system/rt_system_port.cpp    calls the pure StrCompare sweep
runtime/src/io/filesystem_packages.cpp   calls the two allocating sweeps (live-heap hook)
docs/runtime_port/WAVE22_LANE_SYSBODY.md
```

Build clean (`mingw32-make standalone-debug`). No merge to any other branch.
