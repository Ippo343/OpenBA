# WAVE 17 — lane CEIL: the ceilings of Video, IO and Graphics

Worktree `C:/code/GIT/wt-w17-ceil`, branch `wave17/ceil`, cut from `3da1503`.
Every number below was measured on this branch, debug build, DLL present, under
`tools/game_dir_lock.sh acquire ceil` (held for the whole run batch;
`assert-held ceil` gated every deploy, never piped).

**Nothing was ported. Nothing was promoted.** This lane produces a denominator.

## 0. Headline

| namespace | called | ported | today | **ceiling** | rows proved unpromotable |
|---|---|---|---|---|---|
| Video | 150 | 109 | 72.7% | **88.0%** | 18 (12 NO-OP + 6 HARMFUL) |
| IO | 120 | 83 | 69.2% | **97.5%** | 3 (0 NO-OP + 3 HARMFUL) |
| Graphics | 99 | 66 | 66.7% | **96.0%** | 4 (0 NO-OP + 4 HARMFUL) |

**The brief's hypothesis was right for Video and wrong for IO and Graphics.**
Video really is much closer to done than 72.7% says — 18 of its 41 unported rows
can never be promoted, so the real remaining work is 23 rows, not 41. IO and
Graphics are *not*: their data rows are overwhelmingly **BLOCKED on unported
vtable slots**, which is ordinary port work, not a ceiling. Ranking the three
namespaces by "gap" was giving Video too much credit and IO/Graphics too little.

## 1. The subject, and why it is 62 and not 55

The brief said 55 rows (Video 25, IO 17, Graphics 17). The strict count of
`??_7` + `?_typeInfo@` + `?_singleton@` + `.data` statics in the three unported
sets is **62**: Video 25, IO 17, **Graphics 20**. The brief's Graphics 17 omits
the three plain statics (`?_frameIndex@ParticleInstance@`, `?_rng@Particle@`,
`?_rootOnlyUpdates@MeshInstance@`) while its Video 25 *includes* the four
equivalent `?_first*@Video@` statics. All seven are the same kind of row and all
seven are triaged here; three of the seven turn out to be HARMFUL, so leaving
them out would have understated Graphics' ceiling.

**Five IO vtables have no minted row at all** — a fact nothing in the tree
records. `??_7BinaryFileStream@` (0x1801554d0), `??_7CompressedFileStream@`
(0x1801555f0), `??_7FileStream@` (0x180154960), `??_7TextFileStream@`
(0x180155980) and `??_7VisitorStream@IO@@6B@` (0x180155a20) are all exported and
all resolved by this tree, and none of those five VAs appears anywhere in
`standalone/rt_data_mint_tables.inc`. They appear in that file only as
*`adjacent-export:` labels on other rows*, which is why a name grep looks like a
hit. They are a harvest gap, not a ceiling.

## 2. The instruments, each with its control

### 2.1 Readiness — `BANISHED_RT_MINT_SHADOW=1`

Existing screen, exit phase, autostart→gameplay→pause-menu quit, `rc=0`,
0-byte crash log. `[RtMintShadow] ==== clean: 0 problem(s) ====`; 546 slot
comparisons vs the live DLL, 0 MISMATCH; 249 slots minted from our own code.
Per-row `slots/ported/TRAP/purecall` for all 29 subject vtable rows is in §4.

### 2.2 Attendance — `BANISHED_W17_ATTEND=1` (new, `standalone/wave17_door_attend.cpp`)

A green readiness screen says SAFE TO STAMP, not that anything dispatches
(`reference_a_green_readiness_screen_is_not_a_route.md`). The 62 rows are read
through a dozen call sites in five subsystems, so rather than 62 edits to bodies
this lane may not touch, the counter sits on the **door**: every one of them
reaches its row through `ResolveExport(<mangled>)`, directly or via
`RT_DATA_SLOT → RtResolveCached`. The counter is first in `ResolveExport`, before
the registry short-circuit.

**What the number can and cannot say, stated before it was read.** Nearly every
consumer caches in a function-local static, so `post >= 1` proves the consumer
RAN and `post == 0` proves it never did — but `post == 1` says nothing about
frequency and is not "ran once". The file prints nothing that could be read as
frequency.

**Control 1 (wiring).** `?_singleton@MainLoop@Engine@@1PEAV12@EA` must read
`post > 0`. It read `post=2..3` on every route. If it had read 0 the table is
**suppressed** rather than printed, because a page of plausible zeros from an
unwired instrument is the failure this project keeps paying for.

**Control 2 (the phase boundary), and it caught a real error in this lane.**
A resolve during registration is not a consumer running. The marker was first
placed at the end of `RuntimePortInit()` — and that is **wrong**, because
`AddressRegistryInit()` runs afterwards and `standalone/reg_unreg_lane3.cpp`
resolves four `?_typeInfo@…@Graphics@@` rows *by name* to fill dispatch slots.
With the marker in the wrong place the table read **39 attended**; moved to
after `AddressRegistryInit()` it reads **35**, and exactly the four lane-3 rows
plus `?_rootOnlyUpdates@` moved from ATTENDED to `init=1 post=0`. The quantity
VARIES with the thing the boundary separates, which is what makes it a gate and
not a tally. Uncorrected, this lane would have handed four Graphics rows a route
that is a registration.

**Corroboration from an instrument this lane did not write.** `[Encoding]
ATTENDANCE: none of the four bodies was entered in this run` agrees with
`post=0` for `??_7BinaryReader@`, `??_7BinaryWriter@` and `??_7ReaderWriter@`.

### 2.3 Four routes, and the union

| route | env |
|---|---|
| gameplay | `BANISHED_AUTOSTART=1 BANISHED_PAUSEMENU=600 BANISHED_PAUSEMENU_QUIT=120` |
| save | `… BANISHED_SAVE_TEST=900 BANISHED_PAUSEMENU=1400` |
| load | `BANISHED_AUTOLOAD=1 …` (`[AutoStart] AUTOLOAD sequence fired`) |
| mods menu | `BANISHED_AUTOMENU_BUTTON=7 BANISHED_AUTOMENU_CLICKS=m6` |

Union: **36 of 62 attended, 26 never on any route**
(`docs/runtime_port/wave17_lane_ceil/union.py`, which refuses to report a union
if any input file parses to zero rows — it was written after the first version
silently read no files at all and printed a confident empty answer, because the
Windows interpreter cannot see MSYS `/tmp`).

The routes are not interchangeable: `??_7MaterialInstance@Graphics@@6B@` is
`post=0` on gameplay and `post>0` on save/load. Where a row is `post=0` this
report says "never on these four routes", never "no route exists".

### 2.4 Does any binary hold the row's address as a value? (`tools/wave17_qword_scan.py`)

For all 62 VAs, across all four shipped images, honouring each image's preferred
base: **zero hits**. Control: the scanner is asked for slot 0 of
`??_7Stream@IO@@6B@` (0x18000c610), which the DLL does store, and finds it →
GREEN. The exe imports **13** of the 62 (5 singletons, 3 Graphics statics, 5
Graphics typeinfos) and **zero `??_7`** of any kind, which reproduces
`reference_a_ported_row_can_be_permanently_unpromotable.md` on this subject.

### 2.5 In-DLL reference kinds (Ghidra, `getReferencesTo`)

The discriminating measurement, with its own two-row control:

| row class | READ | WRITE | DATA |
|---|---|---|---|
| all 26 `??_7` rows | **0** | **0** | 5–118 (LEA stamps) |
| all 15 `?_typeInfo@` rows | 2–9 | **0** | 2 |
| all 13 storage rows | 2–163 | **1–12** | 1–15 |

Controls `??_7Stream@IO@@6B@` (0/0/118) and `??_7DeviceData@Video@@6B@` (0/0/11)
sit in the same table as the storage rows and read differently, so the screen
demonstrably discriminates mutable storage from a table that is only stamped.

One apparent exception was checked rather than reported: Ghidra lists **1 READ**
of `??_7MaterialInstance@Graphics@@6B@` at 0x1800c3a88. The disassembly there is
`CALL qword ptr [RAX]` — a virtual dispatch on an object Ghidra's type
propagation assigned that table, not a read of the table address as a value
(`reference_ghidra_c_output_vs_disassembly.md`).

### 2.6 The factory-trap question, re-measured for this subject

`[FactoryDispatch]` (lane TINFO's probe, default-on) on this branch:
`External::Load 2847`, `Factory::CreateObject 103404`, `AddControllerTab 0 — SITE
DID NOT RUN`; **VERDICT: 0 of 106,251 dispatches went through a MINTED
BaseTypeInfo**, with C1 (predicate says TRUE for a minted node and FALSE for the
DLL's) and C2 (liveness) both GREEN. The mechanism is structural and applies to
*any* minted `?_typeInfo@` row, mine included: all three dispatch sites reach
`info` by walking `?_singleton@Factory@Object@@`'s AVL tree, which is built from
the DLL's `_firstTypeInfo` chain; our minted nodes hang off `g_mintTypeInfoHead`,
which no `Factory::Startup` walks. So a `?_typeInfo@` row's `fac=2traps` is not a
blocker for any row in this subject.

## 3. The verdicts

62 rows: **12 NO-OP · 13 HARMFUL · 2 PROMOTABLE · 35 BLOCKED.**

### 3.1 UNPROMOTABLE — NO-OP (12): the Video companion-base vtables

`??_7{ConstantBuffer,Context,Device,ImageBuffer,IndexBuffer,PixelProgram,RenderBuffer,RenderState,VertexBuffer,VertexLayout,VertexProgram,VideoProgram}Data@Video@@6B@`

All twelve are READY with `TRAP=0` (1 ported slot, the rest purecall), all twelve
are ATTENDED on every route, none is stored as a qword in any shipped image, none
has an in-DLL READ or WRITE, and the exe imports no `??_7` at all.

They are attended because **our own ported code is the only writer**:
`runtime/src/video/companion_data.cpp` stamps `ResolveExport("??_7XData@Video@@6B@")`
in each of the twelve base constructors and ten base destructors, through a
registry-first door. That is a genuinely live stamp — and it is still a NO-OP,
because of what happens next:

* **ctor.** `dx11/src/devicedata.cpp` slots 9–19 are all the same five steps:
  `MemAlloc` → `Video::<Class>Data::<Class>Data(p, owner)` → **`p->vptr = <the DX11
  vtable>`** → zero the private tail → return. The base stamp is overwritten by
  the next statement, with no virtual call in between.
* **dtor.** `DeviceData_Dtor` (and its ten siblings) stamps the DX11 table, does
  its cleanup, calls `im->DeviceDataBaseDtor(self)` — which stamps the base table
  — and then `MemFree(self)`. The base stamp is the object's last value and is
  never read.

This is the `??_7DialogController@UI@@6B@` shape exactly: **written and
overwritten, or written and freed, UNREAD.** Promotion cannot change behaviour on
any route that could exist. Expiry: none — it would take a virtual dispatch on a
companion base between the two stamps, which would be a change to our own dx11
code and to retail's class layout at once.

`??_7ImageBuffer@Video@@6B@` is **not** in this set: it is the engine class, not a
companion, and it is BLOCKED with 7 trap slots.

### 3.2 UNPROMOTABLE — HARMFUL (13): every mutable-storage row

| row | in-DLL R/W | exe imports it | 
|---|---|---|
| `?_singleton@Device@Video@@` | 163 / 2 | yes |
| `?_singleton@FileSystem@IO@@` | 85 / 2 | yes |
| `?_singleton@Configuration@Video@@` | 55 / 7 | yes |
| `?_singleton@Persistence@IO@@` | 23 / 1 | yes |
| `?_singleton@AnimationManager@Graphics@@` | 17 / 2 | no |
| `?_rng@Particle@Graphics@@` | 20 / 12 | yes |
| `?_firstSurface@ImageBuffer@Video@@` | 8 / 3 | no |
| `?_firstBuffer@IndexBuffer@Video@@` | 8 / 3 | no |
| `?_firstRenderBuffer@RenderBuffer@Video@@` | 8 / 3 | no |
| `?_firstBuffer@VertexBuffer@Video@@` | 8 / 3 | no |
| `?_singleton@Modification@IO@@` | 6 / 5 | yes |
| `?_frameIndex@ParticleInstance@Graphics@@` | 2 / 1 (READ_WRITE) | yes |
| `?_rootOnlyUpdates@MeshInstance@Graphics@@` | 2 / 1 | yes |

Every one is the `?_singleton@Application@1@0PEAV11@EA` case, already tagged
HARMFUL by lane OBJDATA/WAVE12: mutable storage whose readers and writers include
in-DLL RIP-relative accesses. **We intercept only at doors and a RIP-relative MOV
is not one**, so minting our own storage puts our readers on our copy and those
78–165 sites on theirs, with the writer publishing into one of the two. The
correct arrangement is the current one — resolve the exported NAME so we read the
very slot retail writes.

Nine of the thirteen are ATTENDED, which makes the hazard live rather than
theoretical: something in this tree reads each of them today.

Expiry: **none, ever.** A future lane that promotes any of these introduces a
defect. This is why the class field exists: eleven of these thirteen sit in the
same namespaces as the twelve NO-OPs above and calling both "unpromotable" would
say the Video work is finished when four of its six unpromotable rows are
deliberately left on the DLL.

### 3.3 PROMOTABLE (2) — left for a porting lane, not taken here

| row | evidence |
|---|---|
| `?_typeInfo@GeneratedImageBuffer@Graphics@@0VBaseTypeInfo@2@A` | READY; `idcmp=yes` (the exit shadow compares all 104 typeIds against the live DLL, 624 field comparisons 0 MISMATCH); ATTENDED `post=1` from `runtime/src/graphics/wave9_lane_gfx.cpp`; factory trap unreachable (§2.6) |
| `?_typeInfo@Material@Graphics@@0VBaseTypeInfo@2@A` | same, ATTENDED `post=1` **beyond** its `init=1` registration in `reg_unreg_lane3.cpp` — the consumer is `code/Game/Game/gameupdateloop_load.cpp` |

Both are the shape the UI namespace promoted eleven of in Wave 10. Promoting them
moves the numerator by 2; it does not move the ceiling, which is why this lane
did not do it.

### 3.4 BLOCKED (35)

**Blocked on unported vtable slots (15).** Real port work, not a ceiling. The
`TRAP` count is what a porting lane would have to close:

| row | slots / ported / TRAP / purecall |
|---|---|
| `??_7CompressedStream@IO@@6B@` | 9 / 1 / **8** / 0 |
| `??_7ImageBuffer@Video@@6B@` | 10 / 3 / **7** / 0 |
| `??_7MaterialInstance@Graphics@@6B@` | 9 / 2 / **7** / 0 |
| `??_7CompressedMemoryStream@IO@@6B@` | 9 / 3 / **6** / 0 |
| `??_7SunLight@Graphics@@6B@` | 11 / 5 / **6** / 0 |
| `??_7ParticleSystemInstance@Graphics@@6B@` | 10 / 5 / **5** / 0 |
| `??_7PackageFile@IO@@6B@` | 9 / 5 / **4** / 0 |
| `??_7ReaderWriter@IO@@6B@` | 36 / 2 / **4** / 30 |
| `??_7Particle@Graphics@@6B@` | 9 / 2 / **4** / 3 |
| `??_7ParticleInstance@Graphics@@6B@` | 10 / 5 / **4** / 1 |
| `??_7MemoryStream@IO@@6B@` | 8 / 6 / **2** / 0 |
| `??_7AnimationEvent@Graphics@@6B@` | 9 / 2 / **2** / 5 |
| `??_7BinaryReader@IO@@6B@` | 26 / 25 / **1** / 0 |
| `??_7Stream@IO@@6B@` | 7 / 1 / **1** / 5 |
| `??_7Light@Graphics@@6B@` | 11 / 5 / **1** / 5 |

`??_7BinaryReader@` at 25 of 26 and `??_7Light@`/`??_7Stream@` at one trap each
are the cheapest rows in the whole subject.

**Blocked because no minted row exists (5).** The five IO vtables of §1. Two of
them (`??_7CompressedFileStream@`, `??_7VisitorStream@`) are ATTENDED, so this is
not dead surface — it is a gap in `tools/rt_mint_gen.py`'s harvest.

**Blocked on attendance (13).** READY (or value-neutral) but nothing this lane can
drive enters the consumer:

* **The ten `Engine::MainLoop::OnVideoError` typeinfos** — `?_typeInfo@` for
  `ImageBuffer/PixelProgram/RenderState/VertexLayout/VertexProgram/VideoProgram@Video@@`
  and `EdgeMesh/GraphicsMesh/ReflectionImageBuffer/FrameCopyImageBuffer@Graphics@@`.
  All ten have **exactly one consumer in the whole tree**: the ten-element `ids[]`
  at `runtime/src/engine/mainloop_video.cpp:487-499`, inside
  `RtEngine_MainLoop_OnVideoError`. That is the device-lost recovery path; it does
  not run in a normal session, and `post=0` on all four routes agrees. These are
  **not** NO-OPs — the route exists (a real device-lost event reaches it), it is
  simply not one this harness drives. A lane that wants them should synthesise an
  `OnVideoError` call (`reference_no_route_is_not_no_oracle.md`); the readiness and
  value evidence for all ten is already green.
* `??_7BinaryWriter@IO@@6B@` — READY at **26/26, TRAP=0**, the only fully complete
  row in the subject, and `post=0` on all four routes, corroborated by
  `[Encoding] ATTENDANCE: none of the four bodies was entered`.
* `??_7Geometry@Graphics@@6B@` — READY (9 slots, 6 ported, 3 purecall, TRAP=0),
  `post=0` on all four routes.
* `?_typeInfo@PackageFile@IO@@`, `?_typeInfo@MaterialInstance@Graphics@@`,
  `?_typeInfo@Particle@Graphics@@` — READY and value-neutral, `post=0`. Note
  `reg_mint_promoted.cpp`'s banner records `IO::PackageFile::TypeName calls=4` from
  an earlier wave, so PackageFile's consumer demonstrably runs on *some* route;
  it did not run on these four.

**Blocked, with a NO-OP hypothesis this lane declined to tag (2).**
`??_7ReaderWriter@IO@@6B@` is 30-of-36 purecall — an abstract base, so no leaf
instance can exist and the base table can only ever be the transient `+0x00`,
which is lane OBJDATA's exact proof for `??_7Base@Object@@6B@`. `encoding.cpp`
states in a comment that both Encoding constructors "stamp `??_7ReaderWriter@` FIRST
and then overwrite it with the concrete table". If that is right the row is a NO-OP
and the IO ceiling rises. **It is not tagged**, because the claim currently rests on
a comment rather than on this lane's own reading of the shipped bytes, and
`reference_a_written_recipe_can_be_wrong.md` is precisely a good lane's written
recipe being wrong. The same reservation applies to `??_7Stream@IO@@6B@` (5 of 7
purecall). Both are named in §5 as the highest-value follow-up.

## 4. Prediction vs outcome — scored honestly

Pre-registered at `9e6de0d`, before any measurement:

| bucket | predicted | actual | |
|---|---|---|---|
| NO-OP | 40 | **12** | badly over |
| HARMFUL | 13 | **13** | exact — and right on mechanism for all 13 |
| PROMOTABLE | 4 | **2** | over |
| BLOCKED | 5 | **35** | badly under |

**One bucket right out of four, and the count that was right was right for the
right reason.** All thirteen HARMFUL rows are the ones I predicted (every
`?_singleton@` and every `.data` static), for the mechanism I predicted (in-DLL
RIP-relative readers and writers, no door). That prediction was cheap because
the shape was already tagged for `?_singleton@Application@1@`.

**Where I was wrong, and why.** I predicted 40 NO-OPs on the assumption that a
`??_7` row's stamps are all in-DLL LEAs, so promotion would be inert. That was
right about *reachability* and completely wrong about *readiness*: **22 of the 26
vtable rows are not READY at all** — they carry 1 to 8 abort-trap slots, because
their classes are only partly ported. A trap-carrying row is not "proved
unpromotable", it is unfinished, and it belongs in the gap. I had reasoned about
whether promotion would *matter* without first asking whether promotion was
*possible*, which is the readiness screen's whole job and which I had already
decided to run. The BLOCKED bucket absorbed almost exactly the error.

I also predicted the wrong headline: "Video and Graphics within ~2 points of
their ceiling, IO keeping the largest gap." The truth is the reverse for two of
the three — Video's ceiling is 88.0% (gap 23) while IO's is 97.5% (gap 34) and
Graphics' 96.0% (gap 29).

## 5. What this leaves for the next wave

1. **`??_7ReaderWriter@IO@@6B@` and `??_7Stream@IO@@6B@`** — settle the abstract-base
   NO-OP question from the shipped bytes rather than from a comment. If both hold,
   IO's ceiling falls to 95.8% and two rows leave the worklist permanently. One
   afternoon; read the two Encoding constructors as disassembly.
2. **The five unminted IO vtables** — a `tools/rt_mint_gen.py` harvest gap, not
   port work. Two of the five are ATTENDED today.
3. **The three cheapest trap rows**: `??_7BinaryReader@IO@@6B@` (1 trap of 26),
   `??_7Light@Graphics@@6B@` (1 of 11), `??_7Stream@IO@@6B@` (1 of 7).
4. **The ten OnVideoError typeinfos** — one synthesised `OnVideoError` call turns
   ten BLOCKED rows into ten decided ones in a single run.
5. **The two PROMOTABLE Graphics typeinfos**, for a lane that wants +2.

## 6. Contradicting the brief

* **62 rows, not 55** (§1), and the Graphics undercount omitted three rows that
  turned out to be HARMFUL.
* **The brief's ranking premise is half wrong.** Video/IO/Graphics were being
  ranked by gap size; measured, Video's gap is the one that shrinks and IO's and
  Graphics' barely move.
* **The factory-trap hazard the brief flagged does not bite this subject.** It
  told me to re-measure rather than inherit lane TINFO's result, which was right —
  and the re-measurement (§2.6) says the mechanism is structural, so it exonerates
  every `?_typeInfo@` row in all three namespaces, not just the two Object ones.
* **The mods route is broken on `3da1503`, independently of this lane.**
  `BANISHED_AUTOMENU_BUTTON=7 BANISHED_AUTOMENU_CLICKS=1001,…` — the example in
  CLAUDE.md — dies after 5s with `ACCESS_VIOLATION` reading `null+0xd0` in
  `ModDialog_ToggleMod_Impl+0x43` (`0x140144575`), called from `ModDialog_Slot8` ←
  `ModDialog_VOnElementEvent` ← `AutoStart::TickMenu`. **Control: the identical
  crash, same address, reproduces with `BANISHED_W17_ATTEND` unset**, so it is not
  this lane's instrument. The mods-toggle route was therefore unavailable; the
  open-and-close route (`…_CLICKS=m6`) was used instead.
