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

| backlog row | before | after |
|---|---|---|
| `0x180157c60` | 0 / 7 | **7 / 7 COMPLETE** |
| `0x180155860` | 1 / 2 | **2 / 2 COMPLETE** |
| `??_7TextFileStream@IO@@6B@` | 8 / 10 | **10 / 10 COMPLETE** |

Backlog totals: **175 bodies / 34 COMPLETE -> 185 bodies / 37 COMPLETE.**
**Ten slot bodies ported, three tables closed, nothing promoted.**

`mingw32-make standalone-debug` exits 0 on the lane tip.

### The census delta, attributed ROW BY ROW (kickoff warning 5)

| | `wave28/base` | after the two unnamed tables | lane tip | delta |
|---|---|---|---|---|
| called | 1430 | 1435 | **1439** | **+9** |
| ported | 1277 | 1277 | **1277** | **0** |
| TOTAL % | 89.3% | 89.0% | **88.7%** | **down** |
| `internal` called | 222 | 227 | **231** | **+9** |

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

`ported` did not move at all, and **that is correct, not a disappointment** —
all ten new bodies are slots of UNPROMOTED vtables, a population the census
states in terms is "NOT counted above". **Score this lane on the three backlog
rows, never on the TOTAL.** The percentage going DOWN while ten bodies were
added is `reference_the_ceiling_percentage_is_a_moving_target.md` in its
plainest form.

A set diff also turned up two rows that look like new gap rows and are NOT:
`0x1800ba640` and `0x1800ba9f0` appear in `--unported`'s **"Ports with no caller
in our tree"** section, which lists addresses we REGISTERED that nothing in our
tree reaches by address. That is the expected state for a vtable slot the DLL
dispatches out of its own table, and it is a different list from the gap. Said
here because the two sections look alike in the output and reading one for the
other would have manufactured a finding.

**There is no unexplained residue. Every row of the delta is named.**

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

## 5. PROMOTION — NOTHING WAS PROMOTED, AND FOR THESE TWO ROWS THE QUESTION IS
   MALFORMED

**This lane promoted nothing.**

Both rows it closed are UNEXPORTED, address-keyed tables. A row enters the
census's `promoted` bucket only via a `??_7` symbol, so **neither of these two
can ever be promoted in that sense** — `encoding.cpp`'s registration banner
makes the identical point about 0x1801554a8. What COMPLETE buys here is
narrower and real: a dispatch that reaches the minted table runs our body
instead of aborting in `RtMintTrapStub_*`.

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

* All three backlog rows, before and after, same tool, same tips.
* The census delta above, and the five new `called` rows checked as a SET
  against `--unported` at both tips.
* `mingw32-make standalone-debug` exit 0.
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
* That the eight bodies are CORRECT. None carries a diff oracle and the refusals
  are structural, not convenience (§8).

**UNVERIFIED**

* **None of the eight bodies has been observed executing.** No run was taken, no
  game-directory lock acquired, no attendance measured. Every claim in this
  document is static or build-time.
* Whether every live `Audio::Device` is constructed through our registered
  `??0Device@Audio@@QEAA@XZ`, which is what would make the 0x180157c60 slots
  reachable today rather than only at cutover.

---

## 7. WHAT THIS LANE DID NOT DO

*(filled in at close — see §9)*

---

## 8. ORACLES — TWO REFUSALS, BOTH STRUCTURAL

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

`reference_uncalled_port_is_not_verified.md` applies to all eight.

---

## 9. RESIDUALS, EACH WITH AN ADDRESSEE

*(filled in at close)*
