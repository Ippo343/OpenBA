# WAVE 24 LANE DIFF — one armed sweep, and the instrument measured

Branch `wave24/diff`, cut from `wave24/base` at `f5d70de2`.
All figures below were measured **in this lane's isolated worktree**. They are
lane figures, not board figures; the integrator re-derives on the merged tip.

---

## 0. HEADLINE

**The armed run found ZERO port defects. It found three instrument defects, one
of which is a 100%-failing dead oracle, and it put a number on the thing the
kickoff asked for: at most 39% of the harness is an oracle.**

| | |
|---|---|
| sites in the armed run | **630** |
| calls | **99,753,623** |
| divergences | **35, in 7 sites** |
| of those 7 sites: declared **negative controls** | 3 (14 divergences) |
| of those 7: **already-documented identity artefacts** | 2 (2 divergences) |
| of those 7: **newly named instrument defects** | 1 (18 divergences) |
| of those 7: **unexplained, needs a look** | 1 (1 divergence) |
| genuine port divergences | **0** |

---

## 1. WHAT I MEASURED, WITH THE COMMANDS THAT RE-DERIVE IT

### 1.1 The site census — the kickoff's 354 is 305

```
grep -rn "RT_DIFF_SHADOW" --include=*.cpp runtime/ code/ standalone/ engine/ dx11/ | wc -l
   -> 354
python tools/rt_diff_site_classify.py docs/runtime_port/wave24_diff/shadow_class.json
   -> 305 RT_DIFF_SHADOW occurrences IN CODE, 1 unscoped
```

**354 = 305 sites + 49 prose mentions.** The kickoff's number (correct as a
scoped *line* count) counts 49 lines where `RT_DIFF_SHADOW` appears inside a
comment — and the comments most likely to name the macro are precisely the ones
explaining that a site is a self-check, so the inflation is not random. The
classifier strips comments and string-literal contents before counting.

Re-derive the split: `grep ... | grep -cE ':[0-9]+: *(//|\*)'` -> 49.

### 1.2 The static classification (the deliverable)

`tools/rt_diff_site_classify.py` (added by this lane), output in
`docs/runtime_port/wave24_diff/site_classification.txt`:

```
  ORACLE-LINKED           34     EQUAL's value chain reaches a DLL door by assignment
  ORACLE-LINKED-OUTPARAM  71     ... only via an out-parameter (weaker: over-approximates)
  SELF-CHECK             124     no door to the shipped body ANYWHERE in the function
  ATTENDANCE              75     EQUAL is the literal `true` -- compares NOTHING
  TOTAL                  304
```

**The two SOUND populations are SELF-CHECK and ATTENDANCE: 199 of 304 sites
(65%) provably cannot consult the DLL.** That direction is sound because it is a
*necessary condition* — with no door in the function, the verdict cannot reach
retail. `ATTENDANCE` is sound for a different reason: the verdict is a compile-
time constant.

**105 is an UPPER BOUND on genuine oracles, not a count.** See §3.

Per area:

| area | ORAC-strict | ORAC-weak | SELF-CHECK | ATTEND | total |
|---|---|---|---|---|---|
| io | 8 | 15 | 25 | 5 | 53 |
| graphics | 1 | 15 | 20 | 4 | 40 |
| video | 2 | 4 | 16 | 16 | 38 |
| ui | 4 | 7 | 16 | 4 | 31 |
| audio | 12 | 2 | 0 | 12 | 26 |
| input | 0 | 0 | 9 | 17 | 26 |
| system | 1 | 17 | 5 | 0 | 23 |
| engine | 1 | 6 | 11 | 3 | 21 |
| application | 0 | 0 | 2 | 13 | 15 |
| achievement | 2 | 0 | 9 | 0 | 11 |
| object | 0 | 3 | 7 | 0 | 10 |
| picking | 3 | 1 | 2 | 0 | 6 |
| save | 0 | 1 | 2 | 1 | 4 |

### 1.3 The armed run

```
tools/game_dir_lock.sh acquire lane-DIFF 900      # bare, never piped
cp standalone/build/reimpl-x64.exe "C:/GOG Games/Banished/"   # md5 5a909b6d... verified both sides
cd "C:/GOG Games/Banished"
BANISHED_RT_DIFF=1 BANISHED_AUTOSTART=1 \
  BANISHED_PAUSEMENU=1200 BANISHED_PAUSEMENU_QUIT=120 BANISHED_PAUSEMENU_BUTTON=6 \
  ./reimpl-x64.exe          # exit 0, standalone_crash.log 0 bytes
tools/game_dir_lock.sh release lane-DIFF
```

**Route actually covered** (not assumed — read back out of the log): boot ->
main menu -> autostart New Game -> **1,321 in-game frames** -> toolbar pause
menu -> Quit Game -> clean shutdown. `RtDiffSummary()` runs from
`standalone/main.cpp:2041` at shutdown, so **a killed run prints nothing** —
the clean exit is what makes this report exist. Logs snapshotted to
`docs/runtime_port/wave24_diff/run1_{error,debug}.log`.

### 1.4 The run split by what each site can prove (`tools/rt_diff_run_join.py`)

```
python tools/rt_diff_run_join.py . docs/runtime_port/wave24_diff/run1_error.log \
       docs/runtime_port/wave24_diff/shadow_class.json
```
```
population        sites          calls diverg  calls=0 c=0,work>0
ORACLE              246       73529747     20        2          0
SELF-CHECK          115        2261416      8        8          0
ATTENDANCE           38        6746214      0        0          0
NO-COMPARISON       133       16701570      7       68         48
NO-SOURCE-MATCH      98         514676      0        0          0
TOTAL               630       99753623     35       78         48
```

`ORACLE` here is wider than §1.2's: it includes every site using
`RT_DIFF_SCALAR` / `RT_DIFF_FLOAT` / `RT_DIFF_BYTES`, which *do* evaluate the
original, plus the 101 hand-rolled `static RtDiffSite s = {"name",...}` sites
whose function holds a DLL door.

**`NO-COMPARISON` (133 sites, 16.7M calls) is a population nobody has named
before:** a site with `RT_DIFF_BEGIN` and **no comparison macro at all**. It
registers, it prints `divergences=0` in the summary, and it is structurally
incapable of ever printing anything else. Read as a green row it is a false
reassurance for 133 rows.

---

## 2. THE TWO TRAPS

### 2.1 Trap 1 — `calls=` vs `worked=`. **48 sites read `calls=0 worked>0`.**

```
calls=0                      : 78 of 630
  ...of which worked>0       : 48   <- the body ran EVERY time; the oracle covers NONE of it
worked UNDECLARED (`?`)      : 458  <- coverage UNKNOWN, not zero
worked=0 declared            : 16
```

The largest are not marginal:

| site | calls | worked |
|---|---|---|
| `Graphics::MaterialInstance::RenderTextures` | 0 | **201,103** |
| `Graphics::Scene grid reindex (0x18005ee30)` | 0 | 34,461 |
| `Graphics::Scene grid thread (0x18005f4b0)` | 0 | 34,461 |
| `Graphics::Scene grid add (0x18005e5e0)` | 0 | 34,395 |
| `IO::CompressedStream::Read` | 0 | 23,641 |
| `IO::FileSystem::ReadFile` | 0 | 18,963 |
| `Object::ExternalFactory::Add` | 0 | 7,872 |

Every one of these reports `divergences=0`. **None of those zeros is evidence.**
This is the `FlushDeferred` shape from Wave 21's memory, reproduced here 48
times in a single run — and it is the single most misreadable column in the
report. Full list: `docs/runtime_port/wave24_diff/run1_sites.txt`.

### 2.2 Trap 2 — oracles vs self-checks, reported separately

Answered twice, statically (§1.2) and on the run (§1.4). The two numbers are
deliberately never added:

* **Static, over the 304 shadow sites: 199 (65%) provably cannot consult the DLL.**
* **On the run, over all 630 sites: at most 246 (39%) are oracles; at least 286
  (45%) — SELF-CHECK + ATTENDANCE + NO-COMPARISON — cannot catch a shared
  misunderstanding of retail.** The remaining 98 are unresolved residue (§4).

---

## 3. THE 7 DIVERGENT SITES, EACH NAMED

| site | population | calls | div | verdict |
|---|---|---|---|---|
| `UI3 pin negative control (sink)` | NO-COMPARISON | 3 | 3 | **declared negative control — correct** |
| `DLG2 pin negative control (sink)` | NO-COMPARISON | 3 | 3 | **declared negative control — correct** |
| `UI::DialogInstance::StartTimer[negative control]` | SELF-CHECK | 9 | 8 | **declared negative control — correct** |
| `UI::SpriteSheet::TypeName` | ORACLE | 1 | 1 | already documented in-source; pointer identity, harmless |
| `UI::SpriteSheet::Construct` | ORACLE | 3 | 1 | already documented; vtable identity |
| **`IO::MemoryStream::MemoryStream`** | ORACLE | 18 | **18** | **DEAD ORACLE — new finding, §3.1** |
| `Object::Factory::Startup [probe, not diffable]` | NO-COMPARISON | 1 | 1 | **unexplained, §3.2** |

**Three declared negative controls fired.** That is this run's effectiveness
control for the whole harness: it proves the reporting path can emit a
divergence, so `divergences=0` elsewhere is a measurement and not a dead pipe.

### 3.1 `IO::MemoryStream::MemoryStream` is a dead oracle (18/18)

`runtime/src/io/compressedstream.cpp:948-970`. The oracle carefully neutralises
the `Encoding` pointer at `+0x10` and three gaps retail never writes
(`+0x0c`, `+0x21..23`, `+0x2c`) — and then **does not neutralise the vptr at
`+0x00`**, which *must* differ: ours is `VtblMemoryStream()` in our image,
theirs is the DLL's.

`RtDiffReportBytes` does `memcmp(ours, theirs, n)` over the whole buffer and
emits **one** verdict (`rt_diff.cpp:281`). Since byte 0 always differs, that
verdict is `false` on every call regardless of the other 0x30 bytes. So the site:

* reports `divergences=18` out of `calls=18` — a 100% failure rate that reads
  like a catastrophic port defect, and
* **checks nothing**: no difference anywhere else in the struct could change its
  output.

This is the mirror of the Wave 23 rule already in memory — *a control that
corrupts a byte that should not be compared exonerates a broken instrument*.
Here the byte that must differ is compared, so the instrument is permanently red.
**Fix: tag `+0x00` the way `+0x10` is already tagged**, i.e. add a
`TagVptr(a, 0)` / `TagVptr(b, 0)` beside the existing `TagEncodingPointer` calls.
I have deliberately **not** made that edit — it is a port-owner change and this
lane reports rather than repairs.

`RT_UNPROMOTABLE` tags touched: **none.**

### 3.2 `Object::Factory::Startup` — one unexplained row

```
ours=chain=416 nodes=415 badCrc=0   theirs=EXPECTED (no DLL run): nodes==chain==416, badCrc==0
```
The site states its own expected invariant and the run missed it **by one node**
(415 vs 416). This is a self-check, so it is not evidence about retail — but it
is a self-check failing its own stated post-condition, which is worth one look.
I did not chase it; it is not this lane's row.

---

## 4. MY PROBE SHAPE (§1c), AND ITS CONTROLS

**Shape: "shadow/diff site exercised by my run"** — this lane exists to *be* the
run for the four no-lock lanes. The other four shapes do not apply: there is no
single pure function to input-sweep (the subject is the harness itself), nothing
to synthesise an oracle for, no write path to replay, and a two-sided
disassembly check cannot answer "does this site consult the DLL at runtime".

Controls carried, with effectiveness counters:

1. **Classifier controls, 7 synthetic sites, one per outcome — 7/7 pass.**
   They are not decoration: the first version scored **5/7** and the two failures
   were a genuine dead parser (the function body still carried its own outer
   braces, so every `;` sat at depth >= 1 and the *whole function read as one
   statement*). A green-looking 105/105 "all linked" was the symptom.
2. **Effectiveness counters**: const-EQUAL test fired on 75 sites, no-door test
   on 124, dataflow ran on 105 and examined **4,100** statements. A dataflow
   examining 0 statements would be a dead parser and would print the same
   verdict.
3. **Run-level effectiveness**: 3 declared negative controls fired 14
   divergences — the harness can report.
4. **Triage screen controls**: an absent name matches 0; a positive shape
   **taken from the run itself** matches 1. My *first* positive control
   (`Persistence::GetSavePath`) scored 0 — not because the matcher was dead but
   because that site never registered. **A guessed positive control cannot
   distinguish "absent subject" from "broken screen"**; I replaced it with one
   drawn from the run's own output.

### 4.1 Hand-check while the tool agreed (instrument rule 1)

I hand-read 6 sites the classifier had already placed. Four were right
(`Element::GetAlignedPosition`, `Picking::Scene::Intersect`,
`Audio::Scene::AddEmitter`, `Video::ImageBuffer::TypeName`). **Two were wrong,
and both were wrong in the inflating direction** — they were called ORACLE and
are not:

* `runtime/src/engine/singleton_ctors.cpp:938` — `published` is
  `*slot == g_ours`, a property of **our own** object. Never consults retail.
* `runtime/src/io/filesystem_setupdirs_win.cpp:550` — the `else` arm taken when
  there is no DLL to lend; `endsSep(s) && endsSep(d) && endsSep(p)` is a pure
  post-condition on our own output.

That is why **105 is an upper bound**. On a 6-row hand sample the false-positive
rate was 2/6; extrapolating, the genuine-oracle count among the shadow sites is
plausibly ~70-105, and I did not narrow it further. The second case also names a
class the classifier cannot handle at all: **a site whose population depends on
which branch runs**, which no static screen can settle.

---

## 5. SECONDARY — caller-less ports converted to verdicts

```
python tools/rt_diff_triage_callerless.py <untriaged-list> docs/runtime_port/wave24_diff/run1_error.log
```
Output: `docs/runtime_port/wave24_diff/callerless_exercised.txt`.

Of the census's 422 UNTRIAGED caller-less rows, **204 are `export` rows** (the
rest are addr/ordinal rows this screen does not attempt). **45 of those 204
(22%) have a site in this run that demonstrably ran**, so they support an
**EXERCISED** verdict. Examples: `?Lerp@Transform@Math@@` (1,800,206 calls),
`?GetPathExtension@Filename@System@@` (11,074 worked),
`?GetFileInfo@PackageFile@IO@@` (9,471 worked).

**EXERCISED is not VERIFIED**, and the two must not be merged: the table prints
each row's site population beside it, and several of the 45 sit on SELF-CHECK or
NO-COMPARISON sites where `divergences=0` means nothing. Note also
`?Close@CompressedStream@IO@@` — `calls=0 worked=24`, trap 1 exactly.

I did **not** write these verdicts into the census; that is a change to
`tools/runtime_census.py`'s verdict table and belongs to whoever owns it. The
evidence file is the deliverable.

---

## 6. WHAT I DID **NOT** ESTABLISH

1. **I did not produce an exact oracle count.** 105 (static) / 246 (run) are
   **upper bounds** with a measured 2-of-6 hand-sample false-positive rate. The
   sound claims are the negative ones (199 static / 286 on the run).
2. **98 of 630 run sites are unresolved residue** — their names are built by
   local macros (`LANEX_PROBE`) or literal concatenation and no source scan of
   mine matched them. They are 514,676 calls and 0 divergences. I do not know
   what they can prove.
3. **458 of 630 sites never declared `RT_DIFF_WORK`.** Their coverage is
   UNKNOWN, not zero. That is 73% of the harness.
4. **One run, one route.** Boot + menu + 1,321 gameplay frames + pause-menu quit.
   **No save/load route, no stress route, no mods route** (the mods route is the
   known-broken one). Namespaces exercised only in play — deep Graphics,
   Audio 3D, Picking under load — got one pass, not a sweep. I released the lock
   after a single run because TRACE was queued behind me.
5. **I did not run `BANISHED_RT_DIFF_INVERT=1`.** Worth recording why it would
   have found nothing: **`RT_DIFF_REPLAY_PRE` / `RT_DIFF_REPLAY_POST` have ZERO
   adopters in the tree** (`grep -c` over the five roots = 0), so the invert mode
   documented at length in `rt_diff.h` is currently inert. That is a standing
   claim in the header that no site honours.
6. **I did not repair any defect I found.** §3.1's vptr fix is described, not
   applied.
7. **`Object::Factory::Startup`'s 415-vs-416** is named and not diagnosed.

---

## 7. HANDOVER

1. **`IO::MemoryStream::MemoryStream` is red on every call for a structural
   reason** and is checking nothing. One-line fix in §3.1. **Until it is fixed,
   anyone scanning for `divergences=` will see an 18/18 failure and chase a port
   that is fine.**
2. **`calls=0 worked>0` on 48 sites** — `docs/runtime_port/wave24_diff/run1_sites.txt`.
   Any lane quoting `divergences=0` for a Graphics or IO body should check this
   list first; `MaterialInstance::RenderTextures` worked 201,103 times under an
   oracle that covered none of it.
3. **`NO-COMPARISON`: 133 sites print `divergences=0` and cannot print anything
   else.** They are not oracles, self-checks or attendance markers — they are
   registrations. Consider making `RtDiffSummary` print a marker for a site that
   has never called `RtDiffReport`, so the column stops reading as green.
4. **`RT_DIFF_REPLAY_PRE/POST` have no adopters.** Either adopt or retire the
   prose.
5. **The 98-site name residue** needs either a name convention or a registry
   before any future run-vs-source join can claim completeness.
6. **Scope every grep** (kickoff §1a): the `354` -> `305` correction is the same
   shape one level down — a *scoped* grep that still counts comments.

**Tools added by this lane** (all derive their root from their own path, not the
CWD): `tools/rt_diff_site_classify.py`, `tools/rt_diff_run_join.py`,
`tools/rt_diff_triage_callerless.py`.
