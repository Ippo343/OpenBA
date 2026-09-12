# WAVE 26 LANE PROMOIO — the IO / Video / System promotion backlog

Branch `wave26/promoio` off `wave26/base` (`7de42dd1`).
Written as the lane ran. Every claim is marked **MEASURED** or **INFERRED**.

---

## 0. HEADLINE

Four rows promoted, all four **MEASURED live in a clean boot**. The lane's more
durable output is three things it found on the way:

1. **`tools/mint_stamper_census.py`'s `UNATTRIBUTED … NOT OURS` bucket is a
   false negative in 45 of 47 cases**, and 24 of those 47 stamp sites are ours.
   It deflates, so it makes work look *impossible* rather than cheap.
2. **15 of the 16 PROMOTED vtables sit in the census's 148-row gap as "called
   but not ported"** — they are live ports. Handed to lane DOORS (below);
   deliberately not fixed here.
3. **Two of the three vptr identity checks in `runtime/src/io/file_stream_probe.cpp`
   have been structurally FALSE since Waves 19 and 20.** Fixed in this commit.

And one negative result worth as much as the promotions: **20 of the 29
"COMPLETE" rows the kickoff pointed this lane and PROMOGFX at were already
adjudicated PERMANENTLY UNPROMOTABLE in earlier waves**, including all twelve
`??_7*Data@Video@@6B@`. The `PROMOTION BACKLOG` table prints `COMPLETE` and does
not print the tag; the kickoff was built from that column. This lane
re-derived the Video verdict independently **before** finding the tag and
reached the same answer, then checked the tag's own re-open condition at the one
place no previous wave had looked — see §3.1. Nothing here overturns it.

---

## 1. BOARD, RE-DERIVED ON THIS TREE (MEASURED)

`tools/runtime_census.py`, run from this worktree (it derives its subject from
its own path, so this is this tree, not the main checkout).

**At `wave26/base`, before this lane:**

```
TOTAL   4632 total / 1333 called / 1185 ported / 88.9%      gap 148
IO       407 / 216 / 197   gap 19    ceiling 97.7%
Video    573 / 159 / 140   gap 19    ceiling 88.7%
System   262 / 103 /  84   gap 19    ceiling 89.3%
gap kinds  IO  1 BODY 14 VTABLE  4 DATA
           Video      13 VTABLE  6 DATA
           System  7 BODY  1 VTABLE 11 DATA
PROMOTION BACKLOG  194 bodies / 49 unpromoted vtables / 29 COMPLETE
```

Identical to the kickoff. No correction needed.

**After this lane (MEASURED, same command):**

```
TOTAL   4632 total / 1346 called / 1198 ported / 89.0%      gap 148
IO       407 / 225 / 206
PROMOTION BACKLOG  159 bodies / 45 unpromoted vtables / 25 COMPLETE
```

**READ THE GAP CAREFULLY. It is still 148 and that is not a null result** — the
35 slot bodies the four promotions unlocked joined *both* columns, and the four
`??_7` rows themselves stayed in the gap for the reason in §4. The honest
one-line summary of this lane's effect on the board is **"the backlog fell by
35 bodies and 4 vtables"**, not "the percentage moved 0.1".

The System 7 BODY rows are the allocator, WRITTEN and held behind
`BANISHED_RT_ALLOC`. **Not touched**, per brief.

---

## 2. THE INSTRUMENT DEFECT: `UNATTRIBUTED` IS NOT `NOT OURS` (MEASURED)

`tools/mint_stamper_census.py` was re-run on **this** tree rather than quoted
from Wave 20. Its own controls: decoder 3 of 3, ownership positive and negative
both green.

It buckets a stamp site as `UNATTRIBUTED (no .pdata) -- NOT OURS` when the
`.pdata`-bounded-function lookup fails, then prints a "nearest-export HINT" and
discards it. **MSVC emits no `.pdata` entry for a leaf function with no stack
frame** — which is the shape of every one of these three-instruction ctors and
dtors.

Screen (`unattr.py`, `own.py`; the registration scan carries the same positive
control `??0BinaryFileStream@IO@@QEAA@XZ` = REGISTERED and a synthetic negative
control the tool itself uses):

| | count |
|---|---|
| UNATTRIBUTED stamp sites in the full 63-row run | 47 |
| whose stamp address is **exactly** an export-directory entry (offset `+0x0`) | **45** |
| which are, further, **registered in our tree by NAME AND ADDRESS** | **24** |
| genuinely unattributed (`??4BinaryFileStream@` +224, `??4VideoProgram@` +4768) | 2 |

The tool printed `NOT OURS` for all 47.

**The concrete consequence for this lane's own subject:** the twelve
`??_7*Data@Video@@6B@` rows read `ours-capable 0` in the tool's table. The true
figure is **2 of 5 per row** — `??0<Class>Data@Video@@QEAA@PEAV<Owner>@1@@Z` and
`??1<Class>Data@Video@@UEAA@XZ`, both ported in
`runtime/src/video/companion_data.cpp` since Wave 9 lane B2, both registered by
name *and* address, and both reaching the table through
`ResolveExport("??_7…@Video@@6B@")` — a name door, the strongest kind.

This is `reference_a_screen_that_cannot_name_its_subject.md`: a screen printed a
verdict its evidence could not support. It is the **deflating** variety, which is
the one a lane is least likely to challenge, because it makes work look
impossible rather than cheap.

> **[ADDRESSEE: the next lane that runs `tools/mint_stamper_census.py`.]**
> **[CONDITION: before its output is used to score ownership again.]**
> Teach it to resolve an UNATTRIBUTED stamp site against the export directory
> before printing NOT OURS — the check is four lines of `pefile` and 45 of 47
> sites resolve exactly. Until that lands, treat its UNATTRIBUTED rows as
> **UNSCORED**, never as retail.

---

## 3. THE 19 COMPLETE ROWS IN SCOPE, TRIAGED

### 3.1 Twenty of the twenty-nine were already adjudicated (MEASURED)

`grep -o "RT_UNPROMOTABLE: ??_7[^ ]*" standalone/reg_mint_promoted.cpp` returns
**20 symbols, and every one of them is in the 29-row COMPLETE backlog list.**
That includes all twelve `??_7*Data@Video@@6B@` (Wave 17 lane VIDVT, re-measured
by Wave 23 lane VIDVT, tagged `UNPROMOTABLE(NO-OP)` `[SCOPE: PERMANENT]`),
`??_7ReaderWriter@IO@@6B@` and `??_7Stream@IO@@6B@`.

I re-derived the Video verdict independently before finding the tag, and it
agrees: each of those tables has **one non-purecall slot and N−1 `_purecall`
slots in the shipped bytes**, i.e. the class is abstract, so the table exists
only in the window between the base constructor and the derived one and no
completed object can dispatch through it.

**AND THE TAG'S OWN RE-OPEN CONDITION WAS CHECKED, not assumed.** The Wave 17 /
Wave 23 tag carries `[ADDRESSEE: nobody need act -- ... a lane that adds a
virtual call on a companion BASE between the two stamps must re-open it]`. That
condition is about the *derived* construction site, which is in `dx11/`, and
neither Wave 17 nor Wave 23 is recorded as having looked there — both read the
shipped Runtime bytes. It is `dx11/src/devicedata.cpp:1943`:

```cpp
T* AllocData(size_t size, void* (*baseCtor)(void*, void*), void* owner, Vt vt, const Imports* im) {
    T* p = static_cast<T*>(im->MemAlloc(size, 4));
    if (!p) return nullptr;
    baseCtor(p, owner);                 // stamp 1 — the Runtime companion base table
    p->vptr = Dx11VtableAddress(vt);    // stamp 2 — our dx11 derived table
    return p;
}
```

**Two adjacent statements, no call of any kind between the two stamps**, and
every one of the twelve `Factory_*Data` functions goes through this one helper.
The re-open condition does not hold. So the verdict now rests on **three
independent derivations** — the shipped Runtime bytes (Waves 17, 23), this
lane's purecall-count reading, and the derived construction site in `dx11/`,
which nobody had checked.

**That agreement is the finding, not the verdict.** What it says about the
instrument is: *the `PROMOTION BACKLOG` table prints `COMPLETE` without printing
that the row already carries a PERMANENT `RT_UNPROMOTABLE` tag*, so a kickoff
reading that table pointed two lanes at 29 rows of which **20 were closed
business**. In this lane's scope the genuinely-open COMPLETE rows were **five**,
not nineteen.

> **[ADDRESSEE: lane DOORS, alongside the fold in §4 — same table, same print
> pass.]** **[CONDITION: when the corrected per-namespace VTABLE decomposition is
> made a printed output.]** Print the `RT_UNPROMOTABLE` tag in the
> `PROMOTION BACKLOG` row. The data is already parsed — `runtime_census.py`
> reads those lines for the ceiling; it just does not join them to this table.

There is also one row nothing can ever promote for a structural reason worth
stating once: **`0x1801554a8` (5 slots, COMPLETE) is address-keyed.** Every
`RtMintPromotionStorage` call names a symbol, and this row has none. It is not
pending work.

### 3.2 `??_7Thread@System@@6B@` — verdict re-checked, HELD (not re-derived)

Wave 22 lane SYSBODY already adjudicated this row at
`runtime/src/system/thread_win.cpp:150`, route-scoped and correctly written:
**DO NOT PROMOTE while the DLL is present** (two stampers, we own one; retail's
`??0Thread@System@@` stamps by in-DLL RIP-relative LEA, which is not a door; the
only dispatched slot is `OnCompleted` and both sides are a bare `RET` — *small
cost, zero benefit*), **REQUIRED at cutover**.

This lane re-checked that rather than re-deriving it, and adds one fact SYSBODY
could not have: **the row was not in the attendance instrument's subject list**,
so no run in any wave had ever produced a figure for it. That is **UNMEASURED,
which is not `post=0`** — the same shape as Wave 19's `??_7Visitor@IO@@6B@` and
Wave 23's `?_typeInfo@AnimationEvent@Graphics@@`.

I added it to `standalone/wave17_door_attend.cpp` and measured it in §5:
**ATTENDED, `init=0 post=1`.** The door opens. That does not overturn SYSBODY's
verdict — the verdict is about stamper ownership and benefit, not attendance —
but "buys nothing" now sits beside a measurement instead of a blank.

### 3.3 The five genuinely-open rows

| row | slots | disposition |
|---|---|---|
| `??_7CompressedFileStream@IO@@6B@` | 9/9 | **PROMOTED** |
| `??_7CompressedMemoryStream@IO@@6B@` | 9/9 | **PROMOTED** |
| `??_7CompressedStream@IO@@6B@` | 9/9 | **PROMOTED** |
| `??_7FileStream@IO@@6B@` | 8/8 | **PROMOTED** (weakest warrant; stated as such) |
| `??_7Thread@System@@6B@` | 1/1 + 1 purecall | HELD — §3.2 |

The warrants are written in full beside the code in
`standalone/reg_mint_promoted.cpp`, not summarised here, because that is where
the next person reads them. What follows is only what does not fit there.

**Why these four and not the abstract ones.** These are the only tables in this
lane's scope that are the **FINAL** table of a live object. The strongest is
`CompressedFileStream`: `Object::External::Load` (ours, ported, the object-load
path) builds one as a most-derived live object, stamps this table through the
door at `external_load.cpp:278`, hands `&cfs.base.base` to a `Serializer`, and
the `BinaryReader`/`Encoding` on the other side then dispatches
`Read` / `GetCurrentPos` / `GetSize` / `SetCurrentPos` **virtually through
exactly this table**. That is a dispatch route, not a door count.

**`FileStream`'s warrant is deliberately the weakest in the set** and is the
`??_7BinaryWriter@` precedent: TRAP=0 plus a door argument, no dispatch
observation. Every stamp of it our tree makes is a transient base-class stamp the
derived ctor overwrites, or a destructor-chain stamp on a dying object;
`??0FileStream@IO@@QEAA@XZ`, the only ctor that could leave it final, is retail
and unported. **This promotion probably changes nothing** (INFERRED), and is made
anyway on the recorded grounds that leaving a row unpromoted does not verify it.

**Ownership is partial for all four and that is not the gate** (MEASURED,
stamper census re-run on this tree):

```
??_7FileStream@IO@@6B@              17 stampers,  6 ours, 11 retail
??_7CompressedStream@IO@@6B@         9 stampers,  5 ours,  4 retail
??_7CompressedMemoryStream@IO@@6B@   7 stampers,  5 ours,  2 retail
??_7CompressedFileStream@IO@@6B@     8 stampers,  1 ours,  7 retail
```

Retail's in-DLL stampers keep retail's table; both tables are the same eight or
nine methods. A mixed population is a coverage limit, not a hazard — the position
Wave 25 lane VTGFX took for `MaterialInstance`, restated rather than re-argued.

### 3.4 The three hazard screens, run against all four (MEASURED)

These are the three failure modes this file has already paid for:

* **(a) fixed-bound slot copy** (the `??_7Geometry@Graphics@` shape). Grepped
  `runtime/ code/ standalone/ engine/ dx11/` for a `memcpy` of any IO stream
  table: **zero**.
* **(b) identity-token consumer** (the `standalone/scene_watch.cpp` shape).
  **This screen found a live defect — see §6.** None of the four rows has such a
  consumer once that defect is fixed.
* **(c) a ported ctor whose oracle byte-compares the vptr.** Three of the four
  are already immune: `runtime/src/io/compressedstream.cpp` normalises `+0x00`
  on both sides with `TagVptrIdentity`, and the comment at
  `compressedstream.cpp:1096` **predicted this exact promotion in as many
  words**. The fourth (`CompressedFileStream`) has no ported ctor at all, so
  there is no oracle to redden.

**The same screen (c) is why the twelve Video rows must stay held even if their
tag were ever lifted:** `RT_COMPANION_CTOR` in `companion_data.cpp` does
`RT_DIFF_BYTES(self, &theirs, sizeof(CompanionData))` against a retail replay,
with **no `TagVptrIdentity` equivalent**. Promoting any of the twelve turns that
site permanently red, twelve times over, and it would read as a port defect.
Stated here so a future lane does not discover it by arming the diff.

---

## 4. THE CENSUS UNDERSTATES BY THE PROMOTED ROWS (MEASURED, HANDED TO DOORS)

**15 of the 16 promoted vtables are printed by `runtime_census.py --unported` as
"called but not ported".** They are live, unconditional ports.

*Mechanism.* `mint_promoted_symbols()` correctly sees both promotion doors since
the Wave 25 integration fix, and `promoted_syms` folds a promoted table's SLOTS
into the population. But the `??_7X` **symbol row itself** is a separate entity
from the resolve-door scan, and its ported flag comes only from
`d["ported_exports"]` — the literal `RegisterRuntimeExport` scan.
`PromoteVtableGuarded` registers with `RegisterRuntimeExport(symbol, st)`, a
**variable**, so the row never enters that set.

*The control was already in the data.* `??_7MaterialInstance@Graphics@@6B@` is
the one promoted vtable **not** in the gap, and it is the one row whose block
still spells `RegisterRuntimeExport("??_7MaterialInstance@Graphics@@6B@", st)`
as a literal at the call site rather than going through the guard. 15 blind, 1
seen, and the difference is exactly the literal.

**THE PART THAT OUTLIVES THE WAVE IS THE DECOMPOSITION, NOT THE PERCENTAGE.**
Measured, per namespace, at `wave26/base`:

| namespace | VTABLE gap rows | already promoted & live | tagged UNPROMOTABLE | genuinely open |
|---|---|---|---|---|
| **UI** | 6 | 4 | 2 | **0** |
| **IO** | 14 | 6 | 2 | **6** |
| **Video** | 13 | 1 | 12 | **0** |
| **System** | 1 | 0 | 0 (verdict in source, §3.2) | 1 |

A lane scoped on "UI has 6 vtable rows to do" is scoped against **zero**. Video's
13 are 1 promoted plus the 12 abstract bases. IO's 6 open rows are
`CompressedFileStream`, `CompressedMemoryStream`, `CompressedStream`,
`FileStream` (all four promoted by this lane) plus `PackageFile` and
`TextFileStream` — and **those last two are the only real IO vtable work left**,
with their missing slots named in §5.

This project's standing lesson is that *a namespace gap of N is not N bodies*,
and every wave rediscovers it because the census prints no decomposition.

> **NOT FIXED HERE, DELIBERATELY.** The one-line fix (fold `promoted_syms` into
> `ported_exports`) flips 15 rows and moves a figure every other lane is quoting
> mid-wave. Routed to **lane DOORS**, which owns board honesty and is landing it
> this wave. Its invariant is **the SET, not the count** — PROMOGFX measured
> 13-of-14 and this lane 15-of-16 on the same defect, because each counted after
> its own promotions landed on its own branch.

---

## 5. THE RUN (MEASURED)

One boot, on this branch, under the game-dir lock held by `lane-PROMOIO`.

```
exe md5 (build)   a4f5607664f4c61d3f2a3bba16af472b
exe md5 (deployed) a4f5607664f4c61d3f2a3bba16af472b      <- MANDATORY per-run check, matched
env    BANISHED_W17_ATTEND=1 BANISHED_AUTOSTART=1
       BANISHED_PAUSEMENU=600 BANISHED_PAUSEMENU_QUIT=120
       BANISHED_RT_MINT_SHADOW=1
result exit 0, standalone_crash.log 0 bytes, full autostart -> pausemenu -> quit
logs   docs/runtime_port/wave26_promoio/run1_{debug,error}.log
```

**The promotions are live in-process, not just in source:**

```
[RtMint] promoted 44 minted DATA row(s) -- live on every run          (was 40)
[RtMint] vtable promotion guard: 15 row(s) evaluated, 0 refused       (was 11)

[RtMintShadow] READY ??_7CompressedFileStream@IO@@6B@   slots=9 ported=9 TRAP=0 purecall=0  [PROMOTED]
[RtMintShadow] READY ??_7CompressedMemoryStream@IO@@6B@ slots=9 ported=9 TRAP=0 purecall=0  [PROMOTED]
[RtMintShadow] READY ??_7CompressedStream@IO@@6B@       slots=9 ported=9 TRAP=0 purecall=0  [PROMOTED]
[RtMintShadow] READY ??_7FileStream@IO@@6B@             slots=8 ported=8 TRAP=0 purecall=0  [PROMOTED]
```

`PromoteVtableGuarded` re-checks TRAP==0 against the **live registry**, so
`0 refused` is a runtime statement about the real thing, not a source claim.
Its own effectiveness counter reads `0 synthetic trap injection(s)`, i.e. the
refusal path was **not** exercised on this run — stated rather than glossed.

**Attendance** (`CONTROL ?_singleton@MainLoop@Engine@@ init=1 post=2 -> GREEN`,
so the zeros are findings and not a dead instrument):

```
ATTENDED post=2   ??_7CompressedFileStream@IO@@6B@
ATTENDED post=2   ??_7CompressedMemoryStream@IO@@6B@
ATTENDED post=2   ??_7CompressedStream@IO@@6B@
ATTENDED post=2   ??_7FileStream@IO@@6B@
ATTENDED post=6   ??_7Stream@IO@@6B@
ATTENDED post=1   ??_7PackageFile@IO@@6B@
ATTENDED post=1   ??_7Thread@System@@6B@        <- FIRST EVER figure for this row
never             ??_7ReaderWriter@IO@@6B@
never             ??_7TextFileStream@IO@@6B@
never             ?_typeInfo@PackageFile@IO@@0VBaseTypeInfo@Object@@A
40 of 67 subject rows had their door opened AFTER init; 27 never did.
```

This **independently reproduces lane ATTEND's figures** on a different exe
(different md5, same route), which is worth more than either run alone.

`post` counts DOOR OPENS, not dispatches, and `post==2` is **not** "ran twice" —
every consumer caches in a function-local static. So this proves a consumer of
each symbol RAN. It does not prove a slot body was dispatched; that is a separate
tier and is not claimed.

**Minor instrument nit, for whoever next touches that file:** the header prints
`==== door attendance (62 subject rows + 1 control) ====` while the footer
correctly counts 67. The 62 is a stale literal in the banner.

**IO's remaining vtable work, named by the same run:**

```
BLOCKED ??_7TextFileStream@IO@@6B@  slots=10 ported=8 TRAP=2
          TRAP slot 2  ?Write@TextFileStream@IO@@UEAA_NHPEBX@Z
          TRAP slot 8  ?Open@TextFileStream@IO@@UEAA_NAEBVFilename@System@@W4Format@Text@4@@Z
BLOCKED ??_7PackageFile@IO@@6B@     slots= 9 ported=6 TRAP=3
          TRAP slot 5  ?Serialize@PackageFile@IO@@UEAA_NAEAVStream@2@@Z
          TRAP slot 6  ?Reconstruct@PackageFile@IO@@UEAAPEAVExternal@Object@@XZ
          TRAP slot 7  ?Destruct@PackageFile@IO@@UEAAXXZ
```

**Five function bodies.** That is the whole of IO's genuinely-open vtable work —
a much smaller and much more actionable statement than "IO gap 19".

### 5.1 A cutover consequence this lane did not set out to produce

Lane ATTEND measured (WAVE26_LANE_ATTEND.md §4) that in its cutover arms
`??_7FileStream@IO@@6B@` and `??_7CompressedFileStream@IO@@6B@` **resolve NULL
and kill the boot** inside `Game::LoadInputNameTables → ExternalFactory::FindOrLoad
→ External::Load` — but only when `BANISHED_RT_MINT=1` is absent, because they
were minted-but-unpromoted rows that nothing published except that blanket
switch, which integrator item C3 says cutover should not use.

Promoting them publishes both unconditionally. **Two of the four rows promoted
here are the two symbols ATTEND named.**

> This lane does **not** claim to have moved the cutover wall — only attempt 27
> can say that. **[ADDRESSEE: whoever runs cutover attempt 27.]**
> **[CONDITION: read the residue for those two symbols.]** If they no longer
> appear as NULL, this is why. If the boot dies somewhere new, that is the
> expected shape of progress, not a regression.

---

## 6. A PROBE THAT HAS BEEN LYING SINCE WAVE 19 (MEASURED, FIXED HERE)

`runtime/src/io/file_stream_probe.cpp` builds each subject with
`RtResolveExportForProbe` → `ResolveExportRaw`, i.e. **retail's constructor**,
which stamps retail's table with an absolute LEA. It then compared the result
against `RT_DATA_SLOT(...)`, which goes through `ResolveExport` and **consults
the port registry FIRST** (`rt_resolve_cache.cpp:331`).

So the moment a `??_7` row is promoted, the two sides can no longer be equal *by
construction*. That already happened twice:

| check | falsified by | since |
|---|---|---|
| `vptr(+0x68) == ??_7BinaryFileStream` | Wave 20 lane PROMOTE promoting that row | Wave 20 |
| `VisitorStream.vptr == ??_7VisitorStream` | Wave 19 lane IOBASE promoting that row | Wave 19 |
| `vptr(+0x00) == ??_7CompressedFileStream` | would have been falsified by **this lane** | — |

**The failure does not read as "a promotion happened". It reads as a LAYOUT
defect** — the one thing that probe exists to detect. Nothing noticed because the
probe is opt-in (`BANISHED_RT_IOSTREAM_PROBE`, armed from `external_add.cpp:266`)
and, as far as the tree records, nobody has armed it since Wave 19.

Nothing downstream is contaminated, precisely because nobody ran it. But **the
next person to arm it would have seen two red "layout defect" lines that are not
layout defects**, and this is a family this project has paid for before (a probe
whose ours-side reaches the body by address compares retail with itself; one such
was dark for five waves).

**Fixed in this commit:** all three checks now ask the DLL for the address via
`RtResolveExportForProbe`, which is what the object in front of them was actually
stamped with. The claim they make — that `+0x00` and `+0x68` are two separately
stamped tables in one object — is preserved exactly, and is now immune to any
future promotion.

---

## 7. `?_typeInfo@PackageFile@IO@@` — the appended row: **ROUTE-UNDECIDED**

**NOT PROMOTED.** This is a third state and is neither "ready" nor "not ready".

ATTEND measured `init=0 post=0` — `never` — and, as asked, separated the two
readings. It reports **(b): the route never reached it**, with evidence rather
than assertion (the symbol's only consumers are the three reflection bodies at
`package_file.cpp:531/542/552`; `??_7PackageFile@IO@@6B@` reads ATTENDED
`post=1` in the *same* run, so PackageFile objects *are* built through our door
and nothing on that route asks one for its type; sibling `?_typeInfo@` rows read
`post>0` in the same run, so the door mechanism is not at fault; and the consumer
literal is byte-identical to the watch-table literal, excluding spelling drift).
**My own run reproduces the `never`.**

So the zero is **not evidence about the row**, and per my brief it is not read as
"not ready" and not silently dropped.

**Why not promote anyway, given the four vtables above went on weaker
attendance?** Because the cost is not symmetric. Promoting a `?_typeInfo@` row
makes `?TypeName@PackageFile@IO@@` hand out **our** node's `name` pointer, and
that body compares the pointer against the DLL's with `RT_DIFF_SCALAR` — it would
read `divergences=N` forever after, exactly as `?TypeName@SpriteSheet@UI@@`
already does. Lane DATA's instruction is that the annotation lands in the **same
commit** as the promotion. Writing a permanent red line into a body on the
strength of a measurement that is explicitly *not about the row* is the wrong
trade. The four vtable rows carry no such cost — their oracles are already immune
(§3.4).

> **[ADDRESSEE: the Wave 26 integrator.]**
> **[CONDITION — a ROUTE condition, not a readiness one: promote when ONE run
> reads this row `post>0` on ANY route.]** Two candidates, in cost order:
> 1. call `RtIo_PackageFile_TypeName` once from `standalone/tool_harness.cpp`,
>    which already holds drivers of exactly this shape — needs no new route;
> 2. the mods route would ask a PackageFile its type, and is blocked on the
>    `ModDialog_ToggleMod_Impl+0x43` crash CLAUDE.md records — blocked on that
>    crash, **not** on this row.
>
> **Do NOT re-derive the (a)/(b) question — it is measured.** What is missing is
> a route, not an analysis. The `RT_DIFF_SCALAR` annotation must land in the same
> commit as the promotion.

---

## 8. WHAT THIS LANE DID NOT REACH, BY NAME

* **The 4 IO DATA rows, the 6 Video DATA rows, and the 11 System DATA rows** in
  the gap. Not examined. The Video six are all already tagged
  `UNPROMOTABLE(HARMFUL)` / `REQUIRED at cutover` (`?_firstBuffer@IndexBuffer@`,
  `?_firstBuffer@VertexBuffer@`, `?_firstRenderBuffer@RenderBuffer@`,
  `?_firstSurface@ImageBuffer@`, `?_singleton@Configuration@Video@`,
  `?_singleton@Device@Video@`) — **MEASURED from the census output, but I did not
  re-check those verdicts.** The IO four and System eleven I did not enumerate.
* **IO's 1 BODY gap row** (`?SetupFileSystem@Modification@IO@@QEAAXXZ`,
  reached from `runtime/src/engine/loops.cpp`). Identified, **not ported**.
* **The five named TextFileStream / PackageFile slot bodies** in §5. They are the
  whole of IO's remaining vtable work and are the obvious Wave 27 input for this
  namespace.
* **No dispatch-tier verification of the 35 promoted slot bodies.** Attendance is
  door-opens. Whether each body agrees with retail's is a separate tier
  (`file_stream_slots_probe.cpp` and friends) and was not run.
* **The refusal path of `PromoteVtableGuarded` was not exercised** on my run
  (`0 synthetic trap injection(s)`). `BANISHED_RT_MINT_TRAP_CONTROL=*` tests it.
