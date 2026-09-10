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

---

## C5 — do NOT quote "23 complete tables"; and an 8-table gap between two instruments is OPEN

**Claimed** (WAVE19_CLOSED §7.5a, carried into WAVE20_GUIDE §2): *"23 complete tables
are minted and unregistered."*

**Measured** by lane CENSUS at `efb2b5a`: **26 complete unpromoted tables.**

**These are not the same measurement and neither should be quoted as the other.** The
*definition* is identical (`missing == 0` over non-purecall slots). Everything else
differs — **both the tip and the instrument**:

| | Wave 19's 23 | Wave 20's 26 |
|---|---|---|
| instrument | **live registry lookup** in a running process (`RtPortLookupExport`) | **static source scan** of `RegisterRuntimeExport`/`RegisterRuntimeAddr` |
| arithmetic | 28 complete − **5** promoted | 36 complete − **10** promoted |

**The comparable pair is 28 vs 36 overall-complete — an 8-table gap that is NOT
EXPLAINED.** Lane CENSUS reported it as unexplained rather than reconciling it with a
plausible story, which is the right call: a divergence between what a *live registry*
holds and what a *source scan* finds is the exact shape of
`reference_half_registered_port_reads_as_absent` and of the gated-registration trap,
and a guess would retire the flag without earning it.

> **STANDING INSTRUCTION: quote "26 complete unpromoted tables, static source scan,
> `efb2b5a`" — WITH the instrument named — or quote nothing.** Five figures decayed in
> transmission in Wave 19, four of them through the integrator, and the fix is naming
> the instrument and the tip beside the number.

**OPEN ITEM for the next `BANISHED_RT_MINT_SHADOW=1` run:** reconcile 28 vs 36. This is
a TODO with a named route, not a blocker.

---

## C6 — the fold-in's symmetry is FORCED, not lucky (a check that could have flattered us, and does not)

I asked lane CENSUS whether any namespace gained more `ported` than `called`, because
that is the shape that would make the ceiling identity **easier to satisfy with no work
done** — an instrument fix that quietly manufactures progress.

**Answer: none, anywhere.**

```
IO +78/+78    UI +17/+17    internal +10/+10    Video +6/+6    TOTAL +111/+111
every other namespace 0/0
```

**And the reason is structural, not incidental**, which is what makes it trustworthy:
all 111 newly-visible rows are `PORTED`, so each adds 1 to **both** sides. An unported
slot of a promoted table **does** fold in as called-and-not-ported, so the asymmetry can
only ever go the **safe** direction. It reads clean today only because all 10 promoted
tables happen to be COMPLETE.

**No namespace changed its ceiling VERDICT.** Video / Object / Application read AT
CEILING before and after. Video's *printed* ceiling moved **88.2% → 88.6%** while its
verdict did not — a live instance of the standing rule that **the ceiling percentage is
a moving target with `called` in both halves, so score the IDENTITY and never the
percentage.**

---

## C7 — the "unexported" banner was NOT the recorded trap: a REAL check whose SUBJECT was one symbol and whose CONCLUSION was written as covering two

I filed lane TOOLS' `0x1800e0c80` finding as a third instance of the recorded
`AEAA means PRIVATE, not unexported` trap and asked the lane a narrow question: is the
banner's claim **false**, or was it **true of a different symbol when written**? Those
call for different fixes.

**The answer is neither, and it is more useful than either.**

The banner in `input_config_startup.cpp` says the name *"was checked against
Runtime-x64.dll's export directory with pefile before a line was written."*

> **That check was REAL, and it was RUN. Its subject was
> `?Startup@Configuration@Input@@QEAA_NXZ` (ordinal 4085, re-verified by the lane).
> The lane then extended the conclusion to the SECOND function it happened to port in
> the same file, without running the check again.** `0x1800e0c80` is ordinal 3199 — a
> public `QEAA` export.

So it is **not a fabricated verification** and **not a decayed one**. It is a genuine
verification **whose subject was one symbol and whose conclusion was written as though
it covered two.** And it cannot be decayed: `Runtime-x64.dll` is a fixed 2017 artefact,
an export table does not change under us, so the claim was **false the day it was
typed**. Nor did it drift onto the wrong address — the table row, the section header and
the registration comment all say `0x1800e0c80`.

**The fix therefore differs from the recorded trap's**, which is why the distinction
earns its place:

| | recorded trap | this |
|---|---|---|
| failure | inferring "unexported" from the `AEAA` mangling | a real check silently scope-widened |
| fix | stop inferring unexported from the mangling | **a check NAMES THE SYMBOL IT EXAMINED, and a second symbol needs a second check** |

This is the same family as Wave 18's *"a screen that cannot name its subject"* and Wave
19's `--namespace IO` diff — **a sound instrument, correctly run, whose result is
recorded against a wider population than it examined.** It is the *authoring* form of
that defect rather than the *tooling* form: no tool was wrong; the sentence was.

**Scope limit on the refutation, self-reported:** the lane checked **retail only** and
did not open the profile DLL, so the *"either build"* half is not independently
falsified — but *"either"* is a universal over both builds, and **retail alone falsifies
it.** Correct logic, and it is recorded because the lane bounded its own claim without
being asked.

---

## C8 — TWO BEHAVIOURAL DELTAS from lane TOOLS, stated rather than buried

Recorded because both change what a future A/B run measures, and a kill-switch that
silently changes meaning is how a control stops being a control:

1. **The RenderBuffer registration now rides `BANISHED_RT_VIDBUFFERS`** — and **that
   kill switch previously FAILED TO KILL.** So an earlier A/B that set it and read no
   change was not measuring what it thought.
2. **With `BANISHED_RT_INPUTCFGSTART=0` the NAME door now reaches lane ENG's body**
   rather than `input.cpp`'s.

---

## C9 — the PlatformDestruct equivalence is OURS-vs-OURS, and the lane said so unprompted

Asked to state the strength of its equivalence claim precisely, lane TOOLS gave **both
tiers separately** rather than the stronger one:

* **Full reading**, transitively — `context_lifecycle`'s open-coded body in full, and
  because `buffers.cpp` delegates, also `PlatformDestructBody` and `ListUnlink` in full,
  with `kRenderLinks {next 0x18, prev 0x20}` checked against `RenderBuffer`'s
  `static_assert`s **rather than trusting the literal**.
* **Differences-cannot-affect-behaviour**, three found and argued individually: a
  redundant inner null test the caller already guards, a deleting-dtor function pointer
  typed `void` vs `void*` (same register, return ignored), and RT_DIFF instrumentation
  inert unless `rt_diff_on__`.

**And the limit that matters, volunteered:**

> **That equivalence is OURS-vs-OURS, not ours-vs-retail. A shared misunderstanding of
> `0x1800f4400` would pass it invisibly.**

That is the standing `reference_shadow_marker_is_not_an_oracle` finding applied by a
lane to its own result. The ours-vs-retail evidence is separate, covers **only the
surviving body**, and is thin: `calls=3 divergences=0`. **Quoted as thin.**

Also self-reported: the lane **did not measure `calls=0` for the RenderBuffer body
pre-fix** — that is an inference from registration order, which it verified statically.

---

## C10 — `?SaveText@` was ported NINE WAVES AGO; and THREE of the guide's named items have now failed, which is a PATTERN with a mechanism

### The fact

`WAVE20_GUIDE.md` §3 opens with ***"`?SaveText@External@Object@@` is the
highest-leverage single item left"***, and `WAVE19_CLOSED.md` §8 item 5 repeats it. It
was **already ported**, in **Wave 11f lane IOSTREAM**:
`runtime/src/object/external_savetext_win.cpp`, 407 lines, a full transcription
including the dead BOM branch and the inlined `~FileInfo`, **registered by name AND by
address** (`0x1800bb9d0`), with a three-screen oracle.

**Verified by me on the wave tip**, not in the reporting lane's tree, because it retires
the wave's top-billed item: registration block at `external_savetext_win.cpp:397-407`,
dated by git to `1b4980a` / `16acde4` *"Wave 11f lane IOSTREAM CLOSED"*.

Fifth payout in two waves of **grep before you port**.

### The pattern, which is the part worth keeping

**Three separate items the guide named as cheap, high-leverage or do-this-first have
now failed on contact, all in one wave:**

| guide's claim | reality | found by |
|---|---|---|
| `??_7VisitorStream@` — "still unpromoted", a top candidate | **already promoted** by lane IOBASE on Wave 19's last day | integrator (C2) |
| promote the NO-OP pair **first**, "lowest-risk items on the board" | safest **because pointless** — both already tagged `RT_UNPROMOTABLE \| NONE, EVER`; promoting deletes tags and lowers the printed ceiling for zero behaviour change | lane PROMOTE |
| `?SaveText@` — "the highest-leverage single item left" | **ported nine waves earlier** | lane SAVETEXT |

Meanwhile the guide's **measured** §1 table was *right*, in the sense that it was honest
about the tool that produced it — and its §0 instrument diagnoses were **all three
correct and worth the whole of Phase 0**.

> **So the defect is not "the guide was unreliable". It is CONCENTRATED IN ONE SECTION
> KIND, and that section is the one lanes act on FIRST.**

### The mechanism, stated so it can be defended against

A closing integrator's **"named, specified, cheap — take these opportunistically"** list
is assembled at close, from **lane reports**, about **territory the writer did not
personally work**, at the exact moment when **nobody is re-measuring anything**. Every
one of those properties pushes toward staleness:

* it is *about other people's work*, so the writer has no working knowledge to
  contradict it;
* it is *cheap by construction*, so it gets no scrutiny — nobody audits a five-minute
  item;
* it is written *at close*, when the wave's own lanes have just changed the very
  facts it describes (`??_7VisitorStream@` was promoted **the same day** the guide
  called it unpromoted);
* and being cheap and specific, **it is the first thing the next wave picks up.**

**The measured sections carry an instrument and a denominator. The opportunistic list
carries neither** — it is prose describing measurements, which
`reference_a_generated_file_goes_stale_with_no_detector` says decays silently because
nothing regenerates a comment.

### The rule extracted

> **A "cheap wins" list is the STALEST section of any handover and the FIRST one acted
> on. Every entry needs a re-derivation step before a lane is scoped on it — and the
> cheapest entries need it MOST, because their cheapness is what exempts them from
> scrutiny.**

Concretely, for Wave 21's handover: **every item in an opportunistic list gets a
one-line "re-derive this by running X" beside it**, or it does not go in the list. Two
of this wave's three failures would have been caught by a single `grep`, and the third
by reading a file the claim was about.

### The cost, honestly

Near zero — and *that* is why the pattern survived long enough to be visible three
times. Each stale item cost a lane minutes, not hours, because **every lane found it
immediately on being told to grep first.** The damage is not wasted work; it is
**misdirected scoping**: `?SaveText@` was billed as the wave's highest-leverage item and
would have been a lane's whole subject had the brief not also said *read the harness
first and grep before you port*.
