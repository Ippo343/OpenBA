# WAVE 28 LANE SLOTS — the incomplete promotion-backlog tables

Branch `wave28/slots`, cut from `wave28/base` at `5d2594de`.

Scope as briefed, six rows of the PROMOTION BACKLOG:

```
0x180157c60                                0 / 7    <-- largest gap, UNNAMED
??_7MeshInstance@Graphics@@6B@             5 / 9
??_7EdgeMeshInstance@Graphics@@6B@         7 / 9
??_7TextFileStream@IO@@6B@                 8 / 10
??_7Stream@Audio@@6B@                      1 / 2
0x180155860                                1 / 2
```

Re-derived by this lane on `wave28/base` before any edit with
`tools/runtime_census.py`; all six figures reproduced exactly, so the brief was
not stale. Recorded because a re-derivation that AGREES still has to be reported
as having happened
(`reference_a_matching_number_can_match_because_it_is_stale.md`).

---

## 1. THE HEADLINE ANSWER — `0x180157c60` IS IDENTIFIED

**`0x180157c60` is the vtable of `IXAudio2VoiceCallback` as implemented by an
internal, unexported helper class embedded 64 times inside `Audio::Device`, one
per audio channel, at `Device+0xa98`, stride 0x20.**

**The class has no name in the shipped binary.** It is not exported, no
`??_7…@Audio@@6B@` points at it, and nothing in `.rdata` names it. The name
`VoiceCallback` used in the port is DESCRIPTIVE and chosen by this lane, exactly
as `encoding.cpp` chose `IsBinary`/`IsText`/`GetKind` for the other unexported
table. **The INTERFACE is identified; the CLASS NAME is UNIDENTIFIED and is not
recoverable from the retail file.**

The identification is a chain of MEASUREMENTS, each read out of
`C:\GOG Games\Banished\Runtime-x64.dll` on disk with `objdump` (true-VA
disassembly) and `pefile` (export directory, import directory, raw `.rdata`).
**No Ghidra call was made in this lane at all** (see §6).

1. `??0Device@Audio@@QEAA@XZ` (0x1800de3a0) stamps this pointer into 64 records
   at `Device+0xa98` stride 0x20 (`lea 0xa98(%rbx),%rax` /
   `lea 0x7985c(%rip),%rcx # 0x180157c60` / `mov %rcx,(%rax)`).
   An exhaustive scan of `.text` for RIP-relative references to 0x180157c60
   finds exactly TWO: that loop and the single-object constructor at
   0x1800501a0. No other reference of any kind exists.
2. `?PlayPlatform@Device@Audio@@AEAA…` (0x1800de7a0) hands that record to XAudio2
   as the `pCallback` argument of `CreateSourceVoice`:
   `shl $0x5,%rax` / `lea 0xa98(%r15),%rdx` / `add %rax,%rdx` /
   **`mov %rdx,0x28(%rsp)`** (argument 5) / `call *0x40(%rax)`.
3. The interface is pinned by two further COM vtable indices in the same
   listing: `call *0x50(%rax)` on the `IXAudio2` = index 10 =
   `CreateMasteringVoice`, and `call *0x90(%rax)` on a voice = index 18 =
   `IXAudio2Voice::DestroyVoice`. Index 8 (0x40) `CreateSourceVoice` and index
   10 (0x50) `CreateMasteringVoice` agreeing simultaneously is unique to the
   XAudio2 2.7 `IXAudio2` layout, and `IXAudio2VoiceCallback` has exactly SEVEN
   methods against this table's exactly seven slots.
4. The one import the table touches was read from the DLL's own import
   directory with `pefile`, not guessed: `0x18014b368` is
   `KERNEL32.dll!SetEvent`. (`audio_stream_win.cpp` independently names the same
   slot.)
5. The HANDLE that slot 4 signals has a WRITER, and it was read:
   `?Run@Stream@Audio@@` at `0x1800a114d` does
   `mov 0x48(%rsi),%rax ; mov %rax,0x18(%r14)` with
   `r14 = Device+0xa98 + channel*0x20` — i.e. `Audio::Stream`'s `CreateEventW`
   handle into `+0x18`. That is a measurement of the writer, not N readers
   agreeing on a shape
   (`reference_a_writer_beats_readers_agreeing_on_a_sign.md`).

There is no XAudio2 import in the DLL's import table (it is reached through
`ole32!CoCreateInstance`, which IS imported), so the identification rests on the
vtable-index arithmetic above and not on a linker record. Stated because it is
the one place a reader could expect a cheaper proof and not find one.

### The record layout, every field with a writer that was read

```
+0x00  vptr                   -> 0x180157c60                 Device ctor loop 2
+0x08  IXAudio2SourceVoice*                                  CreateSourceVoice out-param;
                                                             DestroyVoice'd by
                                                             FreeChannelPlatform at
                                                             Device+0xaa0+i*0x20
+0x10  AudioChannel*          -> &Device->channels[i]        Device ctor loop 3
+0x18  HANDLE                 -> Audio::Stream's +0x48 event Stream::Run 0x1800a114d
```

---

## 2. WHAT LANDED — MEASURED

`tools/runtime_census.py`, run by this lane on `wave28/base` before any edit and
again on the lane tip. Same tool, same two tips.

| backlog row | before | after | |
|---|---|---|---|
| `0x180157c60` | 0 / 7 | **7 / 7** | COMPLETE |
| `0x180155860` | 1 / 2 | **2 / 2** | COMPLETE |
| `??_7TextFileStream@IO@@6B@` | 8 / 10 | **10 / 10** | COMPLETE |
| `??_7EdgeMeshInstance@Graphics@@6B@` | 7 / 9 | **8 / 9** | |
| `??_7MeshInstance@Graphics@@6B@` | 5 / 9 | **6 / 9** | |
| `??_7Stream@Audio@@6B@` | 1 / 2 | 1 / 2 | NOT ATTEMPTED (§7) |

Backlog totals: **175 bodies / 34 COMPLETE -> 187 bodies / 37 COMPLETE.**
**Twelve slot bodies ported, three tables closed, nothing promoted.**

Plus one thing that is not a slot count and is worth more than several of them:
**the cutover manifest's `CONDITIONAL` bucket is now EMPTY** (§3.4).

### The census delta, attributed ROW BY ROW (kickoff warning 5)

| | `wave28/base` | +unnamed tables | +TextFileStream | lane tip | delta |
|---|---|---|---|---|---|
| called | 1430 | 1435 | 1439 | **1439** | **+9** |
| ported | 1277 | 1277 | 1277 | **1278** | **+1** |
| TOTAL % | 89.3% | 89.0% | 88.7% | **88.8%** | down |
| `internal` called | 222 | 227 | 231 | **231** | **+9** |
| Graphics ported | 98 | 98 | 98 | **99** | **+1** |

**Every one of the +9 is named, and the check was a SET DIFF of
`runtime_census.py --unported` at each tip, not a comparison of counts**
(`reference_diff_the_set_not_the_count.md`).

The five from `0x180155860` slot 0 (§3.2), none of which was in the base set:

```
  addr     internal     0x1800b6290      Persistence GET-SAVE-LIST job
  addr     internal     0x1800b65b0      Persistence GET-SAVE-INFO job
  addr     internal     0x1800b69c0      Persistence DELETE job
  addr     internal     0x1800b6aa0      Persistence LOAD job
  addr     internal     0x1800b6e00      Persistence SAVE job
```

The four from `??_7TextFileStream@IO@@6B@` slots 2 and 8 (§3.3):

```
  addr     internal     0x1800c9b60      EncodeOne dispatcher
  addr     internal     0x1800c9d60      DetectFormat
  addr     internal     0x1800c9fc0      DecodeOne dispatcher
  addr     internal     0x1800ca260      ConvertText
```

The `0x180157c60` row contributed **ZERO** new `called` rows: all seven of its
bodies are leaves (four are `ret`, one ORs a bit, one calls a KERNEL32 import,
which the census does not count as a Runtime row).

**`ported` moved by ONE while TWELVE bodies were written, and that is correct,
not a disappointment.** Eleven of the twelve are slots of UNPROMOTED vtables — a
population the census states in terms is "NOT counted above" — so they land in
the PROMOTION BACKLOG (175 -> 187) and move no column. The twelfth,
`?AddDrawItems@MeshInstance@`, was already a `called` row and therefore flipped.
**Score this lane on the five backlog rows, never on the TOTAL.** The TOTAL
percentage falling from 89.3% to 88.8% across the lane is
`reference_the_ceiling_percentage_is_a_moving_target.md` in its plainest form:
`called` sits in both halves of the ratio, so a lane that opens nine doors moves
it down even while it closes three tables.

A set diff also turned up two rows that look like new gap rows and are NOT:
`0x1800ba640` and `0x1800ba9f0` appear in `--unported`'s **"Ports with no caller
in our tree"** section, which lists addresses we REGISTERED that nothing in our
tree reaches by address. That is the expected state for a vtable slot the DLL
dispatches out of its own table, and it is a different list from the gap. Said
here because the two sections look alike in the output and reading one for the
other would have manufactured a finding.

The **+1 `ported`** is a single row and it was identified by a set diff of the
symbol names, not inferred: `?AddDrawItems@MeshInstance@Graphics@@` LEFT the
unported set. It was already a `called` row (the mint table names it), so
registering it flipped it rather than adding one — which is why `called` did not
move while `ported` did. Graphics went 98 -> 99 and its ceiling 96.0% -> 96.8%.

The same diff shows `?AddDrawItems@EdgeMeshInstance@Graphics@@` ENTERING the
`--unported` output, and **that is not a new gap row**: it lands in the
**"Ports with no caller in our tree"** section (179 exports, up from 178), which
is the expected state for a vtable slot the DLL dispatches out of its own table.
Two sections of one report that look alike; reading one for the other would have
manufactured a finding. (The same trap caught `0x1800ba640`/`0x1800ba9f0`
earlier in the lane.)

**There is no unexplained residue. Every row of the delta is named.**

### The build gate — all four checks, with numbers

Per the integrator's mid-lane correction: exit 0 alone cannot distinguish a
build that compiled everything from one that wrote nothing. **The closing build
was run in the FOREGROUND** (the earlier ones were backgrounded and are not the
gate).

```
mingw32-make standalone-debug                    MAKE-EXIT=0
ls -la standalone/build/reimpl-x64.exe           33,400,025 bytes, Sep 12 13:24
find <my 7 source files> -newer <the exe>        EMPTY
nm  <exe> | grep -c <my 4 register functions>    4
nm  <exe> | grep -c <my 12 body symbols>         36
strings <exe> | grep -cF each of 4 mangled names 2, 2, 2, 2
cutover manifest gate                            OK (59 rows)
```

The `strings` count of 2 per name is the registration literal plus the mint
table's literal — i.e. **the symbols reached the LINKED IMAGE**, not merely an
object file.

**THE FIRST FOREGROUND BUILD FAILED, AND THE FAILURE WAS A RESULT.**
`check-cutover-manifest` refused the build:

```
GATE FAIL: row in standalone\cutover_manifest.inc that a fresh derivation
does NOT produce: ?AddDrawItems@MeshInstance@Graphics@@...
GATE: 0 missing, 1 extra, 0 changed.
```

Registering `AddDrawItems` removed the only `CUTOVER_CONDITIONAL` row in the
manifest — the row the generator describes as *"HARMFUL, scoped DLL-PRESENT, but
the tag never states an inversion — nobody has adjudicated this row FOR
cutover"*. **The manifest's CONDITIONAL bucket is now EMPTY: `CONDITIONAL 1 |
REQUIRED 58 | WITHHELD 1` became `REQUIRED 58 | WITHHELD 1`.** The `.inc` was
regenerated with the existing `tools/cutover_manifest.py --inc` (regenerating a
generated artefact is not building a tool) and the whole diff is two lines, both
this lane's and both attributed:

* the `AddDrawItems@MeshInstance@` CONDITIONAL row removed — the adjudication;
* `?_singleton@FileSystem@IO@@` door count `25 -> 26` — my new `Fs()` helper in
  `text_file_stream_win.cpp`, a cosmetic count.

> **[ADDRESSEE — `standalone/cutover_manifest.inc`]** the **Wave 28 integrator**,
> who already regenerates it on the merged tip (commit `db43287a`, "per Wave 26
> defect 4"). The copy on this branch is a MID-WAVE snapshot committed only
> because the gate is now a hard FAIL rather than the WARN lane IOTEXT could
> leave alone. Re-run `python tools/cutover_manifest.py --inc` after merging.

---

## 3. PER-ROW WORK

### 3.1 `0x180157c60` 0/7 -> 7/7 COMPLETE

New file `runtime/src/audio/audio_voice_callback_win.cpp`, wired into
`RtAudioPortInit()` behind `BANISHED_RT_AUDIOVOICECB` (kill switch, default ON).

| slot | VA | retail body | ours |
|---|---|---|---|
| 0 | 0x180050200 | `ret $0x0` | `OnVoiceProcessingPassStart` |
| 1 | 0x1800501f0 | `ret $0x0` | `OnVoiceProcessingPassEnd` |
| 2 | 0x1800501c0 | `mov 0x10(%rcx),%rax ; orl $0x2,(%rax) ; ret` | `OnStreamEnd` |
| 3 | 0x180050210 | `ret $0x0` | `OnBufferStart` |
| 4 | 0x1800501d0 | `mov 0x18(%rcx),%rcx ; test ; je ; jmp *[SetEvent]` | `OnBufferEnd` |
| 5 | 0x180050220 | `ret $0x0` | `OnLoopEnd` |
| 6 | 0x180050230 | `ret $0x0` | `OnVoiceError` |

Registered by ADDRESS only. **Not one of the seven is exported** — measured
against the retail export directory with `pefile` rather than inferred from a
mangling, because `AEAA` means PRIVATE and this DLL exports its private members
(`reference_aeaa_is_private_not_unexported.md`). An `RT_MINT_UNNAMED` slot
resolves through `RtPortLookupAddr` on its `dllVa`, so these seven
`RegisterRuntimeAddr` calls are exactly what turns `kVSlots2[0..6]` from
`RtMintTrapStub_2_*` aborts into our bodies — the same door `encoding.cpp` uses
for the five slots of 0x1801554a8.

**The slot ORDER in `rt_data_mint_tables.inc` was independently re-derived from
the raw `.rdata` bytes with pefile and agrees slot for slot.** Two sources, and
the second was not the generator that produced the first.

**Corroboration of the two non-empty slots, which is where a wrong slot->method
mapping could do harm.** Slot 2 raises bit 1 of the channel's `flags`, and
`audio_device.cpp`'s already-ported `Device::Update` tests exactly `flags & 2`
as "finished, reap me" and clears bits 0+1 in the reap. That reading was made in
Wave 9 from the READER; this lane read the WRITER. They agree with no shared
premise. Slot 4 signals the streaming thread's buffer event, which is the
textbook use of `OnBufferEnd` and matches the writer in `Stream::Run`. **The
five empty slots cannot be distinguished by behaviour at all, so for them the
method names are LABELS and nothing depends on them.**

`ret $0x0` rather than `ret` is MSVC giving five otherwise-identical empty
bodies five distinct addresses so COMDAT folding cannot merge them — and the
table does hold five distinct addresses, so the anti-folding worked. Five
separate C++ functions reproduce the observable property (five addresses, each
returning without touching anything); the `imm16` itself is not reproducible
from C++ and does not need to be.

**WHY THIS ROW IS UNLIKE EVERY OTHER BACKLOG ROW.** We already own the stamper:
`Ctor_AudioDevice` (the port of `??0Device@Audio@@QEAA@XZ`,
`runtime/src/engine/singleton_ctors.cpp`) reads
`ResolveAddr(Module::Runtime, 0x180157c60)` and stamps whatever that returns
into all 64 records. So a Device our constructor builds carries the minted table
and, with these registrations, the minted table holds our bodies. **That is NOT
a promotion and is not reported as one** — see §5.

### 3.2 `0x180155860` 1/2 -> 2/2 COMPLETE

Slot 0, `0x1800b7260`, 49 bytes, appended to
`runtime/src/io/persistence_worker_win.cpp` (that file already owns the other
four unexported bodies of the same embedded worker). Slot 1
(`?OnCompleted@Thread@System@@`) has been ours for waves.

**What the table is:** the vtable `??0Persistence@IO@@QEAA@XZ` (0x1800b8020)
stamps over the base `System::Thread` vtable of the save/load worker it embeds
at `Persistence+0x10`. Slot 0 of a Thread vtable is `Run` — established by
reading `??_7Stream@Audio@@6B@` (0x180154950), whose slot 0 IS the exported
`?Run@Stream@Audio@@UEAAXXZ` and whose slot 1 is the same
`?OnCompleted@Thread@System@@`.

**The body** is a five-way jump table on `worker+0x3c` and nothing else — no
allocation, no synchronisation, no state of its own, every arm a tail `JMP`:

```
command 0 -> 0x1800b6e00     command 1 -> 0x1800b6aa0     command 2 -> 0x1800b69c0
command 3 -> 0x1800b65b0     command 4 -> 0x1800b6290     default  -> repz ret
```

**The command encoding has an independent witness.** `worker+0x3c` is
`Persistence+0x4c`, and `runtime/types/io/Persistence.h` has said since Wave 19
lane PERSIST, from reading the ENQUEUE side, `1 load · 2 delete · 3 info ·
4 list`. Derived here from the CONSUMER, there from the PRODUCER, agreeing with
no shared premise. **Command 0 = SAVE is INFERRED, not measured**: it is the one
value the header does not list, and `RtIo_PersistenceWorker_EnqueueSave`
(0x1800b75c0) is the only enqueue path that writes no non-zero command. That is
a reading of an ABSENCE and is labelled as such at the site.

**NO NULL TEST on the resolved arm, on purpose.** Retail tail-JMPs with no
check. An `if (f)` would convert "this arm is unported at cutover" from a loud
fault at address 0 into a SILENTLY SKIPPED SAVE. The guard would not restore the
behaviour, it would hide its absence.

**THIS ROW GROWS THE CUTOVER SURFACE BY FIVE, DECLARED IN ADVANCE AT THE CALL
SITE, not discovered afterwards** (`reference_porting_can_grow_the_cutover_surface.md`).
The five arms are the +5 of §2. Each is spelled as a LITERAL inside its own
`ResolveAddr` call rather than routed through a shared helper, because both
`runtime_census.py` and `addr_door_audit.py` find doors by grepping for that
literal and a door reached through a variable is invisible to both
(`reference_literal_grep_blind_to_variables.md`; GFXMESH found a door hidden
this way in `mesh.cpp:201` for 21 waves). Each door carries a
SYMBOL / CONDITION / CONSEQUENCE comment naming what it is, when it is live and
what happens at cutover, per the brief.

### 3.3 `??_7TextFileStream@IO@@6B@` 8/10 -> 10/10 COMPLETE

The scope question the brief asked is answered in §4; this is the work.

Both remaining slots ported into `runtime/src/io/text_file_stream_win.cpp`, the
file that already owns five of the other eight:

| slot | symbol | VA | size |
|---|---|---|---|
| 2 | `?Write@TextFileStream@IO@@UEAA_NHPEBX@Z` | 0x1800ba9f0 | 462 B |
| 8 | `?Open@TextFileStream@IO@@UEAA_NAEBVFilename@System@@W4Format@Text@4@@Z` | 0x1800ba640 | 666 B |

**Both deferrals were re-checked before being acted on and NEITHER had
expired** — they had nothing to expire. Wave 20 lane SAVETEXT and Wave 27 lane
IOTEXT both recorded them, in terms, as "size judgements, not blockers, with no
premise that can expire". That is the honest form of a deferral and the reason
this lane could simply do the work: there was no stale claim to fall over
(`reference_holdback_blockers_are_stale_check_them_first.md` — the check came
back NOT STALE, which still has to be reported as having happened).

**The one thing a port written from the shape rather than the bytes gets wrong,
and it is in BOTH bodies.** Each transcodes in TWO PASSES — measure, then
convert — and the two passes encode into DIFFERENT scratch slots. Pass 1 encodes
into the codepoint slot it just read from, because it only wants the returned
LENGTH and throws the bytes away. Pass 2 (`Open` only) encodes into a separate
4-byte slot and `memmove`s exactly `n` bytes out of it. Collapsing the two onto
one scratch works by accident today and is not what the machine does.

Three smaller ones, recorded so they are not re-derived:

* **`Write`'s two arms return values that are not comparable.** The fast arm
  (`_requestedFormat == _activeEncoding`) compares bytes written against the
  CALLER'S length; the transcoding arm compares against the CONVERTED length.
  Hoisting one `return written == len` out of the branch is right on every
  same-encoding write and wrong on every UTF-8 -> UTF-16 one.
* **`Open` does NOT write `_requestedFormat` (+0x68).** Only `_activeEncoding`
  (+0x6c), and only at the very end. The stream keeps the constructor's 6 in
  +0x68, which is exactly what makes a later `Write` take its transcoding arm.
  An absence is as load-bearing as a presence.
* **`Open` calls `DetectFormat` TWICE on the same bytes** rather than carrying
  the first result in a register. Reproduced; it is retail's, and the second
  call is what pass 2 branches on.

**FOUR NEW ADDRESS DOORS, declared at the call site.** This lane does NOT
reimplement the codec tables. Both bodies pass a RUNTIME encoding id in 0..5 to
two jump-table dispatchers, so a port from spec would have to reproduce all six
entries of both tables; `string.cpp`, `widestring.cpp` and
`package_file_read_win.cpp` each wrote only the one or two entries THEIR caller
could reach and each said so. Here the id is a variable, so the honest shape is
a door. Each is spelled as a literal inside its own `ResolveAddr` call
(`reference_literal_grep_blind_to_variables.md`) and carries a
SYMBOL/CONDITION/CONSEQUENCE comment.

**`ConvertText`'s arg6 is never written by the caller, and that is CORRECT.**
`[rsp+0x28]` at the call site is left holding whatever was there. The callee
uses that slot (`0x88(%rsp)` in its frame) as its own codepoint SCRATCH, writing
`$0xffffffff` into it before every read — MSVC spilled a local into the incoming
argument area. A port that "fixed" it by zeroing would change nothing; a port
that OMITTED the slot would shift args 7, 8 and 9 and corrupt the call. This is
the kind of thing that is only visible in the disassembly of the CALLEE, and
reading only the caller would have produced an 8-argument signature that
compiles.

**ONE TABLE, READ FOUR TIMES, AGREEING.** Both bodies decide how many BOM bytes
to skip with the same ladder (3 -> 3, 4 -> 2, 5 -> 2, else 0). That is the
mirror of the table `?Create@TextFileStream@` WRITES (3 -> `EF BB BF`,
4 -> `FF FE`, 5 -> `FE FF`), of the one `?SaveText@External@Object@@` carries,
and of `DetectFormat`'s own returns (3 after matching `EF BB BF`, 4 after
matching `FF FE` — read out of its disassembly here). Four witnesses, one
written by a writer, two by skippers and one by a detector.

**NO ORACLE for either, and both refusals are structural.** `Write` ends in a
real `WriteFile` on a real handle; a replay emits every byte twice and advances
the cursor twice. `Open` reads the whole file, **CLOSES it**, and installs an
owned buffer at +0x70; a replay would run against a closed handle, leak the
first buffer, and could not read the same bytes. `RT_DIFF_WORK` records liveness
only. **UNVERIFIED: neither body has been observed executing.**

**A STALE LOG LINE CORRECTED ON THE SURFACE THE NEXT PERSON REACHES.**
`RtIoTextFileStreamSlotsRegister()` still printed
`"??_7TextFileStream@ 2/10 -> 7/10 (slots 0, 2, 8 still trap)"` — three waves
after lane ENC closed slot 0. A slot tally inside a `printf` is a comment on its
way to becoming a lie, and this one already was one. It now reports the count it
can actually state plus the current status, and it says NOT PROMOTED in those
words (kickoff warning 15).

---

### 3.4 The two `AddDrawItems` overrides — DERIVED TOGETHER, ON PURPOSE

`??_7MeshInstance@` 5/9 -> **6/9** and `??_7EdgeMeshInstance@` 7/9 -> **8/9**.
Both into `runtime/src/graphics/mesh.cpp`'s new Wave 28 section.

| slot | symbol | VA | ord | extent |
|---|---|---|---|---|
| 3 | `?AddDrawItems@MeshInstance@Graphics@@` | 0x180100f20 | 1550 | **720 B** |
| 3 | `?AddDrawItems@EdgeMeshInstance@Graphics@@` | 0x1800a2fd0 | 1549 | **373 B** |

**They were ported in one go because WAVE27_LANE_GFXMESH.md §7 said porting one
without the other "would leave the two overrides of one slot derived from
different readings of [`Renderer::AddDrawItem`'s] call", and its §4 is the
standing proof that this family goes wrong exactly there.**

**AND I DID NOT READ FOR THE DIFFERENCES — I SET-DIFFERENCED THEM.** The
integrator's `Camera::operator=` finding (a WRITE WIDTH that no amount of
careful reading surfaces) arrived mid-lane with the instruction to use the set
method rather than the eye. Every memory operand of both bodies was extracted as
a SET of `(class, mnemonic, base[+index*scale], displacement)` and the two sets
compared. The result, on the instance object alone:

* `MeshInstance::AddDrawItems` touches **19 distinct fields** of `this`:
  `+0x28`, the twelve bounds floats `+0x4c..+0x78`, `+0x7c`, `+0x80`, `+0xc0`,
  `+0xc8`, `+0xd8`, `+0xe0`, `+0xe8`, `+0x11c`.
* `EdgeMeshInstance::AddDrawItems` touches **exactly one**: `+0xc0`.

That is the difference stated as a measurement. It is also the check on my own
port: no bounds, no opacity, no `_sortBias`, no `_detailId`, no animation state
and no materials array may appear in the EdgeMesh body, and none does. **Seven
places the two disagree are itemised at the code**, including three the shape
does not suggest: the 16-bit `ADD %r9w` that WRAPS rather than promotes; the
`(level << 12)` vs `| 0x8000` packings; and `arg7` being an interpolated opacity
in one and the CONSTANT 1.0f in the other — both bodies load the same 1.0f from
0x18016169c and only one of them subtracts `frameAlpha` from it.

**THE READER WAS GIVEN A POSITIVE CONTROL BEFORE ITS SILENCE WAS BELIEVED.** The
integrator's second warning was that GFXBODY's re-verify script missed the
no-displacement `MOV [RCX],EAX` form and reported the same impossible hole in
BOTH bodies — consistent, and wrong. My extractor printed `UNPARSED: 0` on both
bodies, which is a CLAIM, not a result. It was then handed a synthetic listing
containing exactly that form (`mov %rax,(%rcx)`, `mov %eax,(%rdx)`) plus a
RIP-relative `lea` and an indirect `jmp`; it reported all four, the two
no-displacement writes as `mov[rcx] +0x0` and `mov[rdx] +0x0`. **The blind spot
GFXBODY hit is not present in this reader** — and that is now measured rather
than assumed. The control also exposed a real limitation I am declaring rather
than fixing: the reader classifies an indirect branch target (`jmp *0x…(%rip)`)
as a WRITE. It does not affect a field-set comparison, and no tool was changed.

**THE EXTENTS WERE PINNED BY A CHAIN WALK, NOT BY THE FIRST `.pdata` ENTRY** —
PARTSYS's warning that a set difference over the wrong window answers a
different question. Measured with a forward `UNW_FLAG_CHAININFO` walk:

| body | first `.pdata` entry | full chain | understatement |
|---|---|---|---|
| `?Update@ParticleSystemInstance@` (control) | 0x9d = 157 B | **0x147a = 5242 B** | **33x** |
| `AddDrawItems@MeshInstance@` | 83 B (6 fragments follow) | **720 B** | **8.7x** |
| `AddDrawItems@EdgeMeshInstance@` | 205 B (2 fragments) | **373 B** | 1.8x |

The first row is a POSITIVE CONTROL: the walker was pointed at PARTSYS's own
subject and reproduced its measured 0x9d -> 0x147a before I trusted it on mine.
An instrument whose only outputs are about the subject under test cannot be
distinguished from a broken one.

Both windows I actually disassembled (`0x180100f20..0x1801011f0` and
`0x1800a2fd0..0x1800a3145`) equal the union extent exactly, so the set
difference measured the bodies and not padding or neighbours.

**A SIZE CLAIM IN A LIVE TAG WAS WRONG AND IS CORRECTED IN PLACE.** The
`RT_UNPROMOTABLE` block at `standalone/reg_mint_promoted.cpp` says the body is
"**0xd0 bytes** of LOD selection". It is **0x2d0**; 0xd0 is roughly the
LOD-selection preamble alone. An understated extent makes work look cheap, and
this one sat in the tag that was telling the next lane what the job was.

**THE TAG ITSELF IS UPDATED WHERE THE NEXT PERSON REACHES IT** (kickoff warning
15). Its remaining clause — *"It remains unregistered only because THE BODY IS
NOT PORTED"* — is now false. Before registering, I did what its addressee clause
required and re-ran `grep -rn 0x180100f20 runtime/ code/ standalone/ engine/
dx11/`: the only live consumer is still `scene_watch.cpp`'s rebuilt two-token
test, which no longer goes through `ResolveAddr`. **No new identity-token
consumer has appeared. The debt GFXMESH paid is still paid.** The HARMFUL tag is
left standing, per GFXMESH's own reasoning that a refusal which vanishes teaches
nothing, with a WAVE 28 block beneath it saying both its clauses are now dead.

Both bodies are UNVERIFIED: neither has been observed executing, and both are
virtual slots the DLL dispatches out of its own table, so `calls=0` is the
expected reading and is not verification
(`reference_uncalled_port_is_not_verified.md`).

## 4. THE `TextFileStream` ACCESSOR QUESTION — DECIDED, WITH REASONING

The brief asked me to decide whether the real vtable SLOTS behind the census's
three instrument-only BODY rows (`?GetObjectName@`, `?GetObjectType@`,
`?GetObjectVersion@`, reached only by `runtime/src/io/file_stream_probe.cpp`)
are in scope, and to state the reasoning.

**DECISION: the three accessors are OUT of scope. They are not slots of this
table, or of any table.**

The reasoning is a measurement, not a judgement call. I dumped
`??_7TextFileStream@IO@@6B@` (0x180155980) directly out of the retail `.rdata`
with `pefile` — independently of `rt_data_mint_tables.inc`, which is a generated
file and therefore one source, not two:

```
[0] 0x18002ba90  (unnamed vector deleting dtor)     ours (deleting_dtor.cpp)
[1] 0x1800aab50  ?GetSource@FileStream@IO@@         ours
[2] 0x1800ba9f0  ?Write@TextFileStream@IO@@         NOT OURS
[3] 0x1800ba900  ?Read@TextFileStream@IO@@          ours
[4] 0x1800babd0  ?SetCurrentPos@TextFileStream@IO@@ ours
[5] 0x1800bac30  ?GetCurrentPos@TextFileStream@IO@@ ours
[6] 0x1800ba8e0  ?GetSize@TextFileStream@IO@@       ours
[7] 0x1800aac70  ?Open@FileStream@IO@@              ours
[8] 0x1800ba640  ?Open@TextFileStream@IO@@(Filename,Format)  NOT OURS
[9] 0x1800ba940  ?Create@TextFileStream@IO@@        ours
```

None of `GetObjectName` / `GetObjectType` / `GetObjectVersion` appears. They are
`QEBA` — non-virtual const members, a `LEA` and a `RET` each — and their only
caller in the tree is the probe. **The census's own note is right and I am not
overriding it.** Porting them would move three census rows in fifteen minutes
and satisfy nothing; this project's record is that the item a handover calls
"safest" is often safest only because it is pointless
(`reference_cheap_wins_list_is_the_stalest_section.md`). Lane IOTEXT reached the
same conclusion in Wave 27 and I re-derived it rather than inheriting it.

**The two genuine unported slots are 2 (`Write`, 462 B) and 8
(`Open(Filename, Text::Format)`, 666 B), and THOSE are in scope.** See §7 for
what happened to them.

---

## 5. PROMOTION — NOTHING WAS PROMOTED, AND FOR TWO OF THE THREE CLOSED ROWS
   THE QUESTION IS MALFORMED

**This lane promoted nothing, and it did not establish ownership of any stamping
function except where noted below.** Three tables reached COMPLETE. COMPLETE is
the STATIC half of readiness only: it says promoting would put no trap in a live
table, and nothing more.

**`0x180157c60` and `0x180155860` cannot be promoted in the census's sense at
all.** Both are UNEXPORTED, address-keyed tables, and a row enters the `promoted`
bucket only via a `??_7` symbol — `encoding.cpp`'s registration banner makes the
identical point about 0x1801554a8. What COMPLETE buys for these two is narrower
and real: a dispatch that reaches the minted table runs our body instead of
aborting in `RtMintTrapStub_*`.

**`??_7TextFileStream@IO@@6B@` IS exported and CAN be promoted, and this lane
deliberately did not.** Establishing that would mean owning and measuring the
stamper — which constructor stamps that table and whether every live
`TextFileStream` comes through our copy — and no source scan answers it
(`reference_a_green_readiness_screen_is_not_a_route.md`). Wave 27 had three
lanes reach COMPLETE and all three correctly declined; this is the fourth.
**Closing a table to COMPLETE does not license promoting it.** Residual 6.

For `0x180157c60` the ownership question *does* have an answer and it is worth
recording: `Ctor_AudioDevice` is ported and registered, and it is the stamper.
But **owning the stamper is not the same as every live object coming through
it**, that is a question about call sites, and this lane took no run
(`reference_a_green_readiness_screen_is_not_a_route.md`). Recorded as
"we own the stamper; attendance UNMEASURED".

---

## 6. MEASURED vs INFERRED

### 6.0 THE SHARED-SCRATCHPAD WARNING — EVERY LOAD-BEARING FACT RE-DERIVED

Mid-lane the integrator reported that the session scratchpad is shared between
all seven lanes and that sibling lanes had silently clobbered each other's
`pefile` helper scripts by basename. **Four of this lane's five helper scripts
lived in the shared root under exactly that kind of generic basename**
(`vt.py`, `ex.py`, `xref.py`, `imp.py`). They have been moved to a lane-private
`slots/` subdirectory, and — because a clobbered export dump would have produced
a CONFIDENTLY WRONG identification, which the brief says is worse than an honest
UNIDENTIFIED — every fact this lane rests on was **re-derived with `objdump`,
which reads the PE directly and touches no scratchpad file at all**:

| fact | re-derivation | result |
|---|---|---|
| the 7 slots of `0x180157c60` | `objdump -s -j .rdata` raw bytes at that VA | `180050200 1800501f0 1800501c0 180050210 1800501d0 180050220 180050230`, then non-pointer data — **identical, and the 7-slot extent confirmed by what follows it** |
| the 2 slots of `0x180155860` | same | `1800b7260 18000c590`, then the ASCII `"true"` — **identical, 2 slots** |
| the 10 slots of `??_7TextFileStream@IO@@6B@` | same | **identical to both the mint table and my earlier read** |
| `0x1800de3a0` is the `Audio::Device` ctor | `objdump -p` export address table + name table | index 133 -> RVA `de3a0`, name `??0Device@Audio@@QEAA@XZ` — **confirmed** |
| the 7 callback slots, the 5 Persistence jobs and the worker `Run` are UNEXPORTED | `objdump -p` export address table, grepped for each RVA | **0 hits for all 13** |
| the two `TextFileStream` mangled names | `objdump -p` name table at the ordinals the vtable VAs resolve to | ord 3308 -> `?Open@TextFileStream@IO@@UEAA_NAEBVFilename@System@@W4Format@Text@4@@Z`, ord 4556 -> `?Write@TextFileStream@IO@@UEAA_NHPEBX@Z` — **byte-identical to what was registered; no phantom name** |
| `0x18014b368` is `KERNEL32!SetEvent` | the IAT qword at that VA read raw with `objdump -s`, matched against `objdump -p`'s import listing | slot holds `0x1afd72`; `objdump -p` prints `1afd72 1127 SetEvent` — **confirmed**, and the neighbours `0x18014b360`/`0x18014b370` land on `CreateEventW`/`DeleteCriticalSection`, which is exactly what `audio_stream_win.cpp` recorded in Wave 24 from the other direction |

**Nothing changed.** The re-derivation is reported as having happened precisely
because it AGREED
(`reference_a_matching_number_can_match_because_it_is_stale.md`), and because
"my scripts were not clobbered" is not a claim a lane can make about a shared
directory — only re-deriving through a different mechanism settles it.

Note also point 1 of that correction, the census command: this lane used the
MAIN-checkout absolute venv path
(`C:\code\GIT\untitled_rev_eng_project\tools\ghidra_mcp\.venv\Scripts\python.exe`)
from its first invocation, because the worktree path errored immediately with
`No such file or directory` and an empty output file — a failure that is only
harmless because it is LOUD. Every census figure in this document came from that
absolute path.

**MEASURED**

* The two `AddDrawItems` extents and the seven callback slots' absence of any
  `.pdata` entry, by a forward `UNW_FLAG_CHAININFO` chain walk validated first
  against PARTSYS' own measured subject (0x9d -> 0x147a, reproduced exactly).
* The operand SET of both `AddDrawItems` bodies, with the reader handed a
  positive control for the blind spot that defeated GFXBODY's.
* All five backlog rows that moved, before and after, same tool, same tips.
* The census delta above, and every one of the nine new `called` rows plus the
  one `ported` flip, checked as SET DIFFS against `--unported` at each tip.
* The build gate: foreground `mingw32-make standalone-debug` exit 0, plus file
  size, mtime, a `find -newer` that came back empty, and `nm`/`strings` counts
  against the LINKED IMAGE (§2).
* Every byte, address, extent and slot in this lane, from `objdump` (true-VA
  disassembly of `C:\GOG Games\Banished\Runtime-x64.dll` on disk) and `pefile`
  (export directory, import directory, raw `.rdata` of the same file).
  **This lane made ZERO Ghidra calls.** Kickoff warning 2 says two Ghidra calls
  are one source; the cheapest way to comply was not to use it at all, since
  nothing in this lane needed a name Ghidra has and the PE does not.
* That none of the seven 0x180157c60 bodies, and none of the five Persistence
  job arms, has an export entry — checked individually against the export
  directory rather than inferred from a mangling.
* The 10-slot extent of `??_7TextFileStream@IO@@6B@`, dumped from `.rdata`
  independently of the generated mint table.

**INFERRED, and labelled**

* That `0x180157c60` is `IXAudio2VoiceCallback`. The chain in §1 is measured at
  every link, but the CONCLUSION is an identification, not a record read out of
  the file. The class NAME is UNIDENTIFIED and is not in the binary.
* The slot->method mapping for the five EMPTY slots of that table. They are
  behaviourally indistinguishable; the names are labels.
* That Persistence command 0 is SAVE (an absence-based reading, §3.2).
* That the twelve bodies are CORRECT. **Not one carries a diff oracle**, and
  every refusal is structural rather than convenience (§8). The operand-set
  comparison in §3.4 is a check on the two `AddDrawItems` ports against each
  OTHER and against retail's field set — it is not an oracle and does not test
  agreement with retail's behaviour.
* That the five voice-callback slots whose bodies are empty carry the method
  names given. Behaviourally indistinguishable; labels only.

**UNVERIFIED**

* **None of the twelve bodies has been observed executing.** No run was taken,
  no game-directory lock acquired, no attendance measured, no screenshot. Every
  claim in this document is static or build-time. The four attendance counters
  added for the two `AddDrawItems` bodies **have no caller in the tree**, and no
  number from them is quoted anywhere here (residual 4).
* Whether every live `Audio::Device` is constructed through our registered
  `??0Device@Audio@@QEAA@XZ`, which is what would make the 0x180157c60 slots
  reachable today rather than only at cutover.

---

## 6.5 THE SEVEN EMPTY-LOOKING SLOTS ARE PORTS, NOT GAPS — AND THEY ARE NOT
   THE FACTORY LANES' ABSTRACT SHAPE

Three lanes this wave have now found that a two-or-three-byte slot is a
legitimate body: FACTUI1 and FACTREST's `xor eax,eax ; ret` abstract factories,
and PARTSYS's bare-`RET` `SunLight` slots with no `.pdata` entry at all. Five of
my seven `0x180157c60` slots look exactly like that, so the distinction is
recorded rather than assumed:

* **They are NOT the abstract-factory shape.** `xor eax,eax ; ret` sets a return
  value to null, which is what says "this class is not constructible". These are
  `c2 00 00` — `ret imm16` with imm 0, setting nothing, in methods whose return
  type is `void`. **The empty body IS the implementation**: an
  `IXAudio2VoiceCallback` that does not care about `OnLoopEnd` implements it and
  returns. Porting a body into them is correct; recording them as retail-abstract
  would be wrong.
* **All seven, plus the Persistence `Run`, have NO `.pdata` ENTRY** — leaves with
  no prologue, the same shape PARTSYS measured on `SunLight`. That is *not*
  "length zero": the unwinder simply needs nothing. Their extents come from the
  disassembly, where each is followed by `INT3` padding to the next 0x10
  boundary, so no `CHAININFO` fragment is possible and nothing can have been
  silently truncated.
* `ret imm16` rather than `ret` is MSVC giving five otherwise-identical empty
  bodies five distinct addresses so COMDAT folding cannot merge them — and the
  table does hold five distinct addresses.

## 6.6 `deleting_dtor.cpp` — **NO INDICES CLAIMED**

Kickoff warning 10 requires every lane to state which `kSpecs[]` indices it
claimed. **This lane claimed NONE. `runtime/src/object/deleting_dtor.cpp` and
`deleting_dtor_internal.h` are UNTOUCHED, and `kSpecCount` is unchanged at 50.**
No table in my scope needed a deleting-destructor row: `0x180157c60`'s seven
slots are a COM callback interface with no destructor slot at all,
`0x180155860`'s two are `Run`/`OnCompleted`, and slot 0 of both
`??_7TextFileStream@`, `??_7MeshInstance@` and `??_7EdgeMeshInstance@` was
already a row (41, 46 and 45 respectively, from Waves 21 and 27).

`tools/rt_deleting_dtor_verify.py` was therefore never run and its known bare-
`else` defect never bit this lane.

## 7. WHAT THIS LANE DID NOT DO

* **`??_7Stream@Audio@@6B@` stays at 1/2 — NOT ATTEMPTED, and the reason is
  size, not a blocker.** Its one unported slot is
  `?Run@Stream@Audio@@UEAAXXZ` (0x1800a0f60). **Measured, not estimated** — the
  chain walk gives `0x1800a0f60..0x1800a1668`, a single unchained entry of
  **1800 bytes** — the XAudio2 streaming thread.
  MEASURED about it: it makes five virtual calls through the source voice at
  `callback+0x08`, it calls `MemFree` (0x1800c7ad0), and at `0x1800a114d` it is
  **the single writer of the event handle that slot 4 of `0x180157c60` — which
  this lane DID port — signals**. INFERRED and labelled: that those five voice
  calls are the submit/start/stop set; I did not decode the XAudio2 voice vtable
  indices, because nothing in this lane depended on it. Porting it would have
  taken the rest of the lane and it closes one 2-slot table. Stated as a size
  judgement with no premise that can expire — the honest form, and the one that
  let this lane simply do `TextFileStream`'s two deferred slots (§3.3).
* **Did not launch the game.** No run, no `game_dir_lock.sh` acquisition, no
  attendance measurement, no screenshot. Every claim here is static or
  build-time.
* **Did not promote any vtable row** (§5).
* **Did not port the three `TextFileStream` accessors** (§4, with reasoning).
* **Did not touch another lane's scope.** `ParticleSystemInstance`, `SunLight`,
  `ParticleInstance` and everything particle (PARTSYS); the named Graphics BODY
  rows (GFXBODY); the factory family (FACTUI1/FACTUI2/FACTREST); `StringTable`
  (MISC) are untouched. No table tagged `RT_UNPROMOTABLE` was touched. The one
  file edited outside my own bodies is
  `standalone/reg_mint_promoted.cpp`, where the `AddDrawItems` tag's stated
  reason became false the moment I registered the symbol and had to be corrected
  at the row rather than only in this document.
* **Did not build, fix or extend any tool.** The four helper scripts in
  `<scratchpad>/slots/` are lane-private analysis scaffolding, live outside the
  repo, and are not committed.
* **Did not fix `wave27_lane_gfxpart.cpp`'s split-literal `AddDrawItem` door**
  (§8.2) — another lane's file, and a tooling question.

---

## 8. ORACLES — FOUR REFUSALS, ALL STRUCTURAL

**No body in this lane carries a diff oracle, and not one of the refusals is a
convenience.** Each is a case where running the shipped body a second time is a
SECOND SIDE EFFECT rather than a comparison.

* **The seven XAudio2 callbacks.** Their caller is inside `XAudio2_*.dll`, on
  XAudio2's own audio thread. There is no call site in our tree and never will
  be one. A differential replay would set the finished bit twice and SIGNAL THE
  EVENT TWICE, handing the streaming thread a spurious wake-up.
* **The Persistence worker `Run`.** It is the thread entry of a worker blocked
  on an event for the whole of an ordinary run; calling it a second time "for
  comparison" would execute the pending save/load/delete AGAIN — writing a file
  twice or deleting an already-deleted one. A `RT_DIFF_SHADOW` would also be
  dishonest: the body stores nothing, so a self-check could only assert
  something it never wrote
  (`reference_oracle_must_not_compare_unwritten_bytes.md`). **No site was
  added.**
* **`TextFileStream::Write` and `::Open`.** `Write` ends in a real `WriteFile`
  on a real handle — a replay emits every byte TWICE and advances the cursor
  twice. `Open` reads the whole file, **CLOSES it**, and installs an owned
  buffer at `+0x70` — a replay would run against a closed handle, leak the first
  buffer, and could not read the same bytes because the first call closed the
  file.
* **Both `AddDrawItems`.** Their entire effect is an append into the live
  `Renderer`'s draw-item array, which the array helper may reallocate and free.
  This is the identical refusal `wave9_lane_gfx.cpp` already makes for
  `Renderer::AddDrawItem` itself, one level down, and for the same reason: a
  second run either shares `_data` with the real group or needs a deep copy of a
  0x1f0-byte group nothing in this tree owns.

`reference_uncalled_port_is_not_verified.md` applies to all twelve.

---

## 8.5 THE CONTROL ON THE IDENTIFICATION ITSELF

The integrator's warning that "a reader with a systematic blind spot produces a
confident, self-consistent, wrong answer" applies most sharply to §1, so the
identification carries a **falsifiable discriminator** rather than a
resemblance:

**`IXAudio2VoiceCallback` is the one XAudio2 interface that is NOT
`IUnknown`-derived.** Every other candidate — `IXAudio2`, `IXAudio2Voice`,
`IXAudio2EngineCallback` (which has 3 methods, not 7) — either begins with
`QueryInterface`/`AddRef`/`Release` or has a different method count. This table's
slots 0, 1 and 2 are `ret`, `ret`, and `or dword[this+0x10],2`: **none of them
sets EAX**, so none can be an `AddRef` or `Release` (both must return a `ULONG`)
and none can be a `QueryInterface` (which must return an `HRESULT` and write
through its third argument). **There is no `IUnknown` prefix, and the slot count
is exactly 7.** That pair is satisfied by one interface.

It is worth saying what would falsify it: a seven-slot non-`IUnknown` COM
interface, passed as argument 5 of a call through `IXAudio2` vtable index 8,
that is not `IXAudio2VoiceCallback`. I know of none, and if one exists this
identification is wrong in its NAME while every field offset and every body in
§3.1 stays correct — the port does not depend on the name.

## 9. INSTRUMENT DEFECTS OBSERVED, NOT FIXED

Kickoff warning 1: recorded with file and line, not repaired.

### 9.1 `tools/runtime_census.py` — two look-alike sections, no separator

`--unported` prints the called-but-unported GAP and the "Ports with no caller in
our tree" list in the same `addr`/`export` column format, hundreds of lines
apart, with no marker on the rows themselves. Three symbols in this lane
(`0x1800ba640`, `0x1800ba9f0`, `?AddDrawItems@EdgeMeshInstance@`) appeared as
"new `--unported` rows" in a set diff and are in the SECOND list — i.e. correct
registrations, not gap rows. Reading one section for the other manufactures a
finding, and a count-only diff cannot tell them apart at all. **Not fixed.**

> **[ADDRESSEE]** whoever scopes a Wave 29 instruments lane. The fix is one
> token per row (`gap` / `no-caller`), not a new section.

### 9.2 `wave27_lane_gfxpart.cpp:625` — an `AddDrawItem` door split across two
string literals

```cpp
add = reinterpret_cast<AddDrawItemFn>(ResolveExport(
    "?AddDrawItem@Renderer@Graphics@@QEAAXPEBVGeometry@2@PEBVMaterialInstance@2@"
    "GGMMHPEBXI@Z"));
```

`runtime_census.py`'s called-scan cannot see a mangled name split across a C++
string-literal concatenation (`reference_census_three_blind_spots.md`), so that
door may be invisible to the census today. **This lane's own identical door is
spelled on ONE line for exactly that reason**, and the contrast is written at my
call site. **Not fixed** — another lane's file, and this wave builds no tooling.

> **[ADDRESSEE]** the Wave 29 instruments lane, together with 9.1.

### 9.3 My own operand-set reader mis-classifies an indirect branch target

Declared rather than fixed because it is mine and it is load-bearing that a
reader announces its limits: `jmp *0x…(%rip)` is classified as a WRITE. It does
not affect a field-set comparison (§3.4) and no result here depends on it. The
script is lane-private scaffolding and is **not committed**.

## 10. RESIDUALS, EACH WITH AN ADDRESSEE

1. **`??_7Stream@Audio@@6B@` 1/2 — `?Run@Stream@Audio@@` (0x1800a0f60, 1800 B
   measured).** NOT ATTEMPTED, a size judgement (§7). **ADDRESSEE: a Wave 29
   Audio lane.** It is the natural partner of this lane's `0x180157c60` work —
   it is the writer of the event slot 4 signals — so whoever takes it inherits a
   record layout that is already pinned and a callback table that is already
   COMPLETE.
2. **The five Persistence job bodies** (0x1800b6290, 0x1800b65b0, 0x1800b69c0,
   0x1800b6aa0, 0x1800b6e00). **ADDRESSEE: the Wave 29 lane scoped to IO's
   remaining `internal` address rows, or the cutover lane that finds them in the
   `[RtRawResidue]` list, whichever comes first.**
3. **The four text-codec internals** (0x1800c9d60 `DetectFormat`, 0x1800c9fc0
   decode, 0x1800c9b60 encode, 0x1800ca260 `ConvertText`). **ADDRESSEE: the same
   Wave 29 IO lane**, together with lane IOTEXT's residual #1 (0x180029bb0) —
   same neighbourhood, same transcoder family. Roughly half of both jump tables
   is ALREADY WRITTEN DOWN in this tree (`package_file_read_win.cpp` §94 has
   decode entry 1 and encode entry 2; `widestring.cpp` has decode 2/4 and encode
   2/4 from spec) and needs collecting rather than deriving.
4. **The four `AddDrawItems` attendance counters have NO CALLER.** Per the
   integrator's finding that every lane's `*Summary()` is written and nothing
   invokes it, `RtGraphicsAddDrawItemsCounters()` is stated AT THE CODE to have
   no caller in the tree today, and **no number from it is quoted anywhere in
   this document.** **ADDRESSEE: the Wave 29 verification lane**, which kickoff
   §4 names as the wave that pays the "registered and never executed" debt.
5. **`standalone/cutover_manifest.inc` is a mid-wave snapshot on this branch.**
   **ADDRESSEE: the Wave 28 integrator** — re-run
   `python tools/cutover_manifest.py --inc` on the merged tip, as commit
   `db43287a` already establishes as practice.
6. **Three tables in my scope are COMPLETE and NONE is promoted** (§5).
   **ADDRESSEE: the Wave 29 lane scoped to promotion**, if there is one. For
   `0x180157c60` and `0x180155860` the question is malformed — they are
   address-keyed and cannot enter the `promoted` bucket at all. For
   `??_7TextFileStream@IO@@6B@` it is a live question this lane did not touch:
   it needs someone to establish who stamps that table and whether every live
   `TextFileStream` comes through our constructor.
7. **Twelve bodies, zero observed executing.** **ADDRESSEE: the Wave 29
   verification lane.** This lane grew that debt by twelve and says so in the
   word UNVERIFIED rather than reporting "ported + registered" as verified.

---

## 11. CLOSING GATE — RE-DERIVED AND QUOTED ON THE LANE TIP

`mingw32-make standalone-debug` → **MAKE-EXIT=0** (foreground),
`reimpl-x64.exe` **33,400,025 bytes, Sep 12 13:24**, `find … -newer` **empty**,
`cutover manifest gate: OK (59 rows)`.

`tools/ghidra_mcp/.venv/…/python.exe tools/runtime_census.py` on the lane tip:

```
TOTAL              4632     1439     1278        9   88.8%
Graphics            599      126       99        1   78.6%   96.8%
IO                  407      234      222        0   94.9%   97.9%

PROMOTION BACKLOG
  187 bodies across 42 unpromoted vtable(s), of which 37 are COMPLETE
      vtable                                                ours slots  complete
      0x180155860                                              2     2  COMPLETE
      0x180157c60                                              7     7  COMPLETE
      ??_7EdgeMeshInstance@Graphics@@6B@                       8     9
      ??_7MeshInstance@Graphics@@6B@                           6     9
      ??_7Stream@Audio@@6B@                                    1     2
      ??_7TextFileStream@IO@@6B@                              10    10  COMPLETE
```

Those six lines are this lane's whole scope. Five of the six moved; the sixth
(`??_7Stream@Audio@@6B@`) is §7's declared non-attempt.
