# WAVE 22 LANE GEOM — discharging the addressee tag on `??_7Geometry@Graphics@@6B@`

Base: `wave22/base` at `37706651` (kickoff commit). Branch: `wave22/geom`.

## 1. What the tag said

`standalone/reg_mint_promoted.cpp` carried:

> `RT_UNPROMOTABLE: ??_7Geometry@Graphics@@6B@ | 6/6 (3 purecall) [CONV: M=NONPURECALL] |
> WAVE21_LANE_GFXVT.md §5.2 | ROUTE-SCOPED (DLL-PRESENT ONLY); voids when
> statusicon_geometry.cpp:341 reads `i < 9`; REQUIRED at cutover [SCOPE: DLL-PRESENT]
> [ADDRESSEE: Wave 22 lane GEOM, which owns the statusicon_geometry.cpp:341 cutover
> blocker -- WAVE21_CLOSED.md §7 item 1] | HARMFUL`

Wave 21's essay under that line (still preserved as history, see §3 below) explained the
whole chain: `code/Game/Component/statusicon_geometry.cpp:341` copies `i < 12` slots out of
a Geometry base vtable that is really 9 slots wide; today the extra 3 reads land on the
shipped DLL's own `.rdata` (dormant, because retail's own destination table is likewise 9
slots + a null pad + an unrelated class's table, so nothing dispatches past slot 8); at
cutover, promoting `??_7Geometry@Graphics@@6B@` (required, because `ResolveAddr` would
otherwise hand `Geometry`'s ctor a null vtable) would move that same 24-byte overread onto
our own 9-element `kVTable28` array — turning a dormant read of known bytes into an
unanalysed read of unrelated storage of ours.

## 2. Re-derivation — TRUE, independently re-measured, not re-quoted

I did not take the "9 and 9" claim on the strength of WAVE21_LANE_GFXVT.md §5.2a alone. I
read both binaries' raw bytes myself with `pefile` (no Ghidra needed for this — plain image
reads):

- **Runtime-x64.dll**, image VA `0x180154e60` (`??_7Geometry@Graphics@@6B@`): qwords
  0-8 are ordinary code pointers into `.text` (`0x18001____`-`0x1800a____`); qword 9 is
  `0x6675426567616d49`, which as little-endian bytes is `49 6d 61 67 65 42 75 66` = ASCII
  `"ImageBuf"` — the start of `"ImageBuffer\0Generated..."`. Confirmed: 9 real slots, then
  string data, all of it non-zero (so the old `baseVtbl[i]` truthiness test would pass and
  install string fragments as callable pointers).
- **Application-x64.exe**, image VA `0x14015f0c0` (retail's own billboard vtable): qwords
  0-8 are ordinary code pointers into `.text`; qword 9 is exactly `0` — a genuine NULL, not
  string bytes; qwords 10-18 are a second, unrelated 9-slot table (this part I did not
  re-derive from xrefs myself — I re-read WAVE21_LANE_GFXVT.md's xref evidence,
  `StatusIconComponent_Ctor` at `0x14012c0fe` stamping `0x14015f0c0` and
  `TradeWork_impl_ed50`/`FUN_1400b00e0` stamping `0x14015f110`, and did not re-run those
  xref queries against Ghidra this lane); qwords 19-20 are ASCII (`"Terrain..."` reversed).

So both extents really are **9**, confirmed on the actual shipped bytes, and the "12" in the
loop bound has no basis in either binary. The plan's claim re-derives true. The finding I can
add beyond re-confirming GFXVT's number: the DLL's slot 9 is *provably* string content (not
just "assumed non-function"), and the exe's slot 9 is *provably* a bare null word, which
sharpens residual 1 below from "probably a pad" to "measured, byte for byte."

## 3. The fix — one character, plus the comment that would have talked the next reader out of it

`code/Game/Component/statusicon_geometry.cpp:341`: `i < 12` → `i < 9`.

The surrounding comment (previously ~35 lines asserting "12 real slots... has no basis in
either binary" while the code below it still read 12 — a prescription written but not
applied, exactly the failure mode `reference_a_prescription_read_as_a_completion.md`
describes) is rewritten to: state the derivation as *this lane's own re-measurement* (with
the exact byte values), record that the fix has now landed, and carry both open residuals
forward explicitly rather than resolve them into a single tidy paragraph. See the file for
the full comment; the operative code is now:

```cpp
if (i < 9)
    s_siGeomVtable[i] = (baseVtbl && baseVtbl[i])
                             ? baseVtbl[i]
                             : reinterpret_cast<void*>(&SIGeom_StubRet0);
else
    s_siGeomVtable[i] = nullptr;
```

Slots 0-8 keep the pre-existing stub fallback for when `baseVtbl` itself is null/not yet
built (those are real, potentially-dispatched Geometry virtuals — dtor, bounds/type
getters — and a null there would break ordinary scene culling). Slots 9-15 — never real
Geometry slots in the first place, just headroom in our own 16-wide array — now get
`nullptr` instead of a live stub. See residual 1.

## 4. The two open residuals — carried forward, not smoothed

**Residual 1 — retail's slot 9 is `nullptr` where our stub was not.** Resolved this lane:
slots ≥9 now get `nullptr`, matching the measured retail value at slot 9 rather than
installing `SIGeom_StubRet0` as a live callable nobody asked for. Reasoning: nothing in
either binary dispatches these indices (see residual 2 for the status of that claim), so
the choice between "null" and "harmless stub" has no observed behavioural effect either
way *today*. But a null pointer in an unreachable slot fails loudly (crash) if some future
change ever did reach it — the same exposure retail itself carries at that address — while
a callable stub would quietly absorb such a call and hide the bug. That is the direction
this project's "don't guard a deref retail leaves unguarded" doctrine argues for: our
previous code was quietly guarding against a fault retail does not guard against, in a
region retail leaves genuinely unguarded (null). This is **not** a claim that it is now
"more correct" in the sense of matching retail's *function*, since slots 9-15 correspond to
nothing in either binary — 9 is retail's pad, 10-15 don't exist in retail's table at all.
It is a claim that the failure mode is now the honest one instead of a masked one.

**Residual 2 — the no-dispatch finding is DERIVED, not OBSERVED — still true, not upgraded.**
I confirmed (§2) that both classes present at destination address `0x14015f0c0` have
exactly 9 virtuals each, by reading raw bytes and by re-reading (not re-running) the xref
evidence from WAVE21_LANE_GFXVT.md that ties those two tables to their stampers. That is
still a *derivation*: "9 slots per class" implies "nothing dispatches index ≥9" only if
every caller respects the class's real vtable size, which I have not independently
observed at runtime. There is no attendance counter on `StatusIconGeometry_Vtable()`'s
call sites, and I did not add one — instrumenting every virtual call through this
particular vtable pointer to prove zero calls land at index ≥9 was out of scope for a
one-character-fix brief, and I am naming that as unchecked rather than upgrading the
derivation to an observation by restating it confidently. A lane that wants an
*observed* zero (not a derived one) needs to add that counter and run the game.

## 5. The tag itself

`standalone/reg_mint_promoted.cpp`'s `RT_UNPROMOTABLE:` line for
`??_7Geometry@Graphics@@6B@` is **deleted** (not merely edited) — its own expiry text said
"voids when statusicon_geometry.cpp:341 reads `i < 9`", and that line now reads `i < 9`, so
the condition fired. The essay underneath it is kept as history (Wave 21's stamper-census
numbers and byte findings are still true and useful to the next lane) but is prefixed with a
`RESOLVED by Wave 22 lane GEOM` note and its "EXPIRY, WITH AN ADDRESSEE" clause is rewritten
in the past tense to say the addressee acted and to be explicit that GEOM **did not**
promote the row.

**Promotion itself is left undone, on purpose and out of scope.** The tag's history explains
that promoting `??_7Geometry@Graphics@@6B@` (adding a
`RegisterRuntimeExport("??_7Geometry@Graphics@@6B@", ...)` call so `ResolveAddr` redirects
Runtime's own stamp to our `kVTable28`) is a **separate** decision from fixing the cutover
overrun — it was tagged `ROUTE-SCOPED (DLL-PRESENT ONLY)` HARMFUL specifically because, with
the DLL present, retail's own table already answers every real dispatch and promoting buys
nothing; it only becomes *required* at cutover, which this tree is not at. Now that the
overrun that made promotion harmful even at cutover is fixed, the row is eligible to be
re-scored and promoted by a future lane — its stamper census was already green (3 stampers,
2 ours-capable, 0 unattributed, 1 retail copy-ctor stamper that is moot once there is no
retail body left to run) — but doing that promotion was not part of discharging the named
debt (the cutover *blocker*), and I did not want to fold a second, larger behavioural change
into a brief that was framed as "one character."

**No re-pointing was needed.** The addressee (GEOM) ran and discharged the debt in full for
the piece it was asked to own (the cutover blocker). The row's *promotion* is now a fresh,
un-tagged opportunity, not an unresolved debt — there is nothing left pointing at an absent
addressee.

## 6. Checkers

Both ran clean before and after, on this worktree, from `wave22/base`:

```
tools/dispatch_slot_map.py --check
  before: checked 110 ResolveExport registrations, 0 name mismatch(es); 0 address(es) registered more than once
  after:  checked 110 ResolveExport registrations, 0 name mismatch(es); 0 address(es) registered more than once
  (unaffected by this change -- no dispatch-slot edits)

tools/unpromotable_slot_check.py
  before: CHECK 1: 21 of 21 scoreable tags fit, 0 STALE | CHECK 2: 21 numeric tags -- 2 declared, 19 grandfathered, 0 UNDECLARED
          CHECK 3: 42 tags with an expiry -- 2 scoped, 2 addressed, 40 grandfathered, 0 MISSING
  after:  CHECK 1: 20 of 20 scoreable tags fit, 0 STALE | CHECK 2: 20 numeric tags -- 1 declared, 19 grandfathered, 0 UNDECLARED
          CHECK 3: 41 tags with an expiry -- 1 scoped, 1 addressed, 40 grandfathered, 0 MISSING
  (tag count dropped by exactly 1, as expected from deleting exactly one RT_UNPROMOTABLE
  line; the pre-existing FREEZE UNMATCHED note on ??_7ReaderWriter@IO@@6B@ is unrelated and
  present identically before and after this change)
```

Build: `mingw32-make standalone-debug` — exit code 0, `standalone/build/reimpl-x64.exe`
produced, zero warnings on either edited file
(`code/Game/Component/statusicon_geometry.cpp`, `standalone/reg_mint_promoted.cpp`).

## 7. What I did NOT check

- **No runtime/in-game verification.** I did not run `sa-run` or exercise the
  `BANISHED_LIVE_STATUSICON_DRAW` path to observe the fixed loop executing, and I did not
  add or run an attendance counter on `StatusIconGeometry_Vtable()`'s dispatch (residual 2).
  This lane's fix narrows a copy window that was already reading dormant/unreachable data
  by the plan's and my own byte-level analysis, so I judged the risk of a silent behaviour
  change to be effectively zero, but that judgement is derived, not observed.
- **Did not re-run the xref queries** (`StatusIconComponent_Ctor`, `TradeWork_impl_ed50`,
  `FUN_1400b00e0`) against Ghidra myself; I re-read WAVE21_LANE_GFXVT.md's stated xref
  evidence for the *second* table at `0x14015f110` rather than re-deriving it independently.
  Everything I could check directly from raw bytes (the two vtable extents, the string
  content, the null word) I did re-derive myself; the xref-based claim about *whose* stamper
  owns which table I took on the prior lane's word.
- **Did not promote `??_7Geometry@Graphics@@6B@`** — see §5 for why that is a deliberate
  scope decision, not an oversight.
- **Did not run the game-directory-dependent checkers** (`game_dir_lock.sh` /
  `boot_state.sh`) — this lane's change touches no dispatch slots, no registry rows beyond
  removing a comment-only tag, and no runtime behaviour reachable without the fixed vtable
  ever being exercised; the two checkers that do apply (`dispatch_slot_map.py --check`,
  `unpromotable_slot_check.py`) do not need the game directory and both ran green.
