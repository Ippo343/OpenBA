# WAVE 20 — INTEGRATOR CORRECTIONS

Running log. Same convention as Wave 19: every entry names **what was claimed**,
**what is true**, **how it was caught**, and **what it changes**. Entries are appended,
never rewritten; a correction to a correction gets its own number.

---

## C1 — "14 duplicate registrations" is 7 names + 7 addresses, not 14 sites

**Claimed** (WAVE20_GUIDE §0c, WAVE19_CLOSED §6): *"14 pre-existing duplicate
registrations in compiled code."*

**True:** `tools/dup_runtime_export.py` on the wave tip, exit code read directly and
**not through a pipe**, reports:

```
1119 distinct RegisterRuntimeExport names,     7 duplicated
1158 distinct RegisterRuntimeAddr addresses,   7 duplicated
```

These are **two overlapping views of the same defects** — a body registered twice is
generally duplicated under *both* its name and its address. 14 is the sum of two
counts of one population, and a lane told to "triage 14" would be looking for sites
that do not exist.

**Caught by:** running the tool before relaying its number. It prints its own
`SUBJECT:` line and both denominators — the Wave 19 fix (C39) working exactly as
designed, and the only reason the shape of the number was visible at all.

**Changes:** lane TOOLS is tasked to triage **what is there**, not to reach 14.

---

## C2 — "5 of 62 vtables promoted" is **10 of 62**, and one of the wave's two named top candidates IS ALREADY PROMOTED

**This is the load-bearing one. It corrects a correction.**

**Claimed** (WAVE19_CLOSED §7.5a, quoted as the headline of the recommended subject in
WAVE20_GUIDE §2, and repeated in §4 items 3 and 4): *"The right figure is 5 OF 62
VTABLES PROMOTED, not 31 of 62"*, from lane FILE's count *"5 vtables, 25 typeInfo, ~1
storage = 31."* WAVE19_CLOSED §5.1 and WAVE20_GUIDE §2 then name
**`??_7VisitorStream@`** as one of the two *"still unpromoted"* strongest candidates
for Wave 20.

**True, counted from `standalone/reg_mint_promoted.cpp` on the wave tip:**

```
RegisterRuntimeExport("??_7...")  in reg_mint_promoted.cpp : 10
  UI/Video (Waves 10B/10D/18)  SpriteSheet  Element  DialogInstance  ToolTipDialog  ImageBuffer
  IO       (Wave 19, 9f2edbc3) BinaryReader  Visitor  MemoryStream  VisitorStream  BinaryWriter
live ++n rows in the file : 36   (37 written, 1 commented out)
denominator, re-derived   : rt_mint_gen.py --check -> UP TO DATE (62 vtables, 104 BaseTypeInfo, 39 storage), rc=0
```

> **It is 10 of 62 vtables, and `??_7VisitorStream@` — named in two documents as a
> top unpromoted Wave 20 candidate — was PROMOTED ON 2026-09-10 by lane IOBASE**, in
> commit `9f2edbc3` *"Lane IOBASE: five IO vtable rows PROMOTED, with the A/B that
> earns it."*

**The mechanism, which matters more than the number.** The two counts differ by
**exactly the five IO rows lane IOBASE promoted**:

```
pre-IOBASE vtable promotions : SpriteSheet Element DialogInstance ToolTipDialog ImageBuffer  = 5
IOBASE's tranche             : BinaryReader Visitor MemoryStream VisitorStream BinaryWriter  = 5
```

So **lane FILE's "5" was correct about the tree it could see, and stale about the
merged wave.** It counted a file its sibling lane was concurrently adding to. This is
the wave's own catalogued failure — *a number quoted about a subject that changed
since the number was written* — and it happened **inside the correction that was
issued to fix a different miscount of the same figure.** §7.5a was right that 31 was a
category error and wrong about what replaced it.

**Caught by:** not by care. By the integrator declining to relay the headline of a
recommended subject without counting it, then `git blame`-ing the lines to get a
mechanism rather than a discrepancy. A number and a mechanism are different
deliverables: the number alone would have read as a coin-flip between two counts.

**Changes, and this is the point:**

1. **The promotion opportunity is real but SMALLER than the guide claims.** §7.5a
   argued the corrected figure made the subject *"roughly four times what §4 implied"*.
   At 10 promoted it is **half of that argued expansion**.
2. **Do not scope a Phase 1 cell on `??_7VisitorStream@`.** It is done. Its warrant —
   *"7/7/0 and ATTENDED via `External::Add`, a proven door plus a complete slot set"* —
   was **already spent by lane IOBASE**, which is presumably *why* it was the row a
   lane picked.
3. **The other named candidates SURVIVE this check and are confirmed unpromoted**:
   `??_7BinaryFileStream@`, and the §7.5b NO-OP pair `??_7Stream@` and
   `??_7ReaderWriter@`. None appears in `reg_mint_promoted.cpp`. **The guide's
   recommendation to promote the NO-OP pair FIRST is unaffected** and is now the
   best-supported item on the Phase 1 board.
4. **A standing instruction for every Phase 1 lane:** *re-derive the promoted set from
   `reg_mint_promoted.cpp` at your own commit before claiming a row is unpromoted.*
   This figure has now been wrong twice in two documents, in opposite directions.

**What this does NOT claim:** I have not verified that the ten registrations are
*effective* at runtime (`RtMintRegisterPromotedRows` has an env kill-switch at
`reg_mint_promoted.cpp:173` and returns 0 when set to `"0"`), nor re-derived how many
of the 62 tables are COMPLETE. `mint_slot_sweep.py` at HEAD reports **62 vtables, 578
slots, 307 ported, 143 legitimate purecall, 128 unported named traps** — I quote that
with its denominators and leave the completeness count to the lane that needs it,
rather than relaying "28 complete / 23 unregistered" unchecked.

---

## C3 — the 13 save games are NOT under the game directory

Recorded in [WAVE20.md](WAVE20.md) §1.2 and repeated here so the corrections log is
self-contained. `C:/GOG Games/Banished/Save` **does not exist**; the 13 `.sav` files
are under **`%USERPROFILE%\Documents\Banished\Save`**. A check aimed at the game
directory returns zero and is **correct about the wrong directory**. Correction C14 of
Wave 19 was the integrator relaying that zero as *"no save games at all"*, which cost
a lane its verification route.

---

## C4 — CORROBORATED-EXACT rests on an unstated premise; 30 of 31 rows have it ASSUMED

**Not a correction of an error. A correction of a CONFIDENCE**, and it is Phase 0's
most valuable output. Self-reported by lane EXTENT **after it had already reported a
clean 31 of 31**, against its own headline, in answer to a question about a *different*
limitation.

**The claim as it reads:** `CORROBORATED-EXACT` = *"the next address any instruction in
the image names is exactly this row's end"* — presented as strictly stronger than the
`adjacent-export` bound it replaces, which it is.

**The premise nobody had written down:**

> **P3 — the address named at `va + 8n` is a DISTINCT OBJECT'S START, not an INTERIOR
> address of the same table that some code took the address of.**

A vtable cannot overlap a *distinct* object — that is what makes the bound work. But
nothing stops code from `LEA`-ing an address *within* a table it already owns: a
partial dispatch, a cached sub-range, a pointer to `vtable+0x50` handed to a helper. If
the address at `va + 8n` is such an interior address, then the true extent `m > n`, the
row is **UNDER-SIZED**, and — per Wave 19 §5.2's asymmetry — **there is no symptom.**

**Status: 1 of 31 CHECKED, 30 ASSUMED.** The one checked is
`??_7DeviceData@Video@@` (`0x180158cd0` is `LEA`'d by two constructors at `0x180058f7e`
and `0x1800fa966`, each followed by a `mov %rax,(%rdi)` vptr store — a genuine table
start).

**Why this matters more than it looks.** §0d exists so that **no Wave 20 DATA lane is
scoped against a slot count not re-derived from the writer.** Lane PROMOTE is being
scoped against these counts now, and a `COMPLETE` verdict from lane CENSUS means *"every
non-purecall slot IN THE RECORDED COUNT is registered."* **If a row is under-sized there
are slots beyond the recorded end, neither counted nor registered** — so promoting it
publishes a table that is complete over the visible part and **traps past it**. So
`COMPLETE` is conditional on the extent being right, and for 30 rows that condition is
assumed.

**What makes this a good outcome rather than a bad one:** the premise is **checkable
with machinery already in the tool.** The store-shape test the audit already parses and
prints for row-*start* writers, applied at `va + 8n`, settles all 31. Re-tasked to lane
EXTENT as a clearly-marked **post-report tranche** — sealing forbids altering a reported
result, not adding a marked one, so `296140e` stays quotable as the state at report.

**The tiering instruction issued with it:** report **PREMISE-CHECKED** vs
**PREMISE-ASSUMED** as distinct, never collapsed, and do not silently upgrade
`CORROBORATED-EXACT` for rows that pass. A uniform 31-of-31 PASS is itself to be looked
at hard — *one hand-check passing is not evidence that thirty will.* `COULD-NOT-DERIVE`
remains first-class: **"24 checked, 7 unreachable" beats 31 confident verdicts.**

**How it was caught, which is the transferable part.** Not by a screen — no screen has a
superset of "premises I did not state". It came from the integrator asking a lane to
finish a sentence that had been truncated in transmission, about a limitation it had
already volunteered. That is Wave 19 §7.6's standing finding holding again: **the
detector is integrator review of a specific claim, not peer review and not a tool.** And
it is the third time this wave that *asking a lane which of two mechanisms a limitation
is* has produced a finding neither the lane nor the tool would have surfaced.

**Relayed immediately to lane PROMOTE**, with instructions to keep working — its §5.1
stamper gate is independent of extent — but to record `PREMISE-ASSUMED` in any
promotion warrant covering a row whose premise the tranche has not cleared.
