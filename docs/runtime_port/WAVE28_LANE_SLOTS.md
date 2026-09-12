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

Backlog totals: **175 bodies / 34 COMPLETE -> 183 bodies / 36 COMPLETE.**

`mingw32-make standalone-debug` exits 0 on the lane tip.

### The census delta, attributed ROW BY ROW (kickoff warning 5)

| | `wave28/base` | lane tip | delta |
|---|---|---|---|
| called | 1430 | 1435 | **+5** |
| ported | 1277 | 1277 | 0 |
| TOTAL % | 89.3% | 89.0% | **down** |
| `internal` called | 222 | 227 | **+5** |

**The +5 is fully attributed, and it was checked as a SET, not as a count**
(`reference_diff_the_set_not_the_count.md`). `runtime_census.py --unported` at
the lane tip contains exactly these five rows, and none of them was in the
`wave28/base` set:

```
  addr     internal     0x1800b6290      Persistence GET-SAVE-LIST job
  addr     internal     0x1800b65b0      Persistence GET-SAVE-INFO job
  addr     internal     0x1800b69c0      Persistence DELETE job
  addr     internal     0x1800b6aa0      Persistence LOAD job
  addr     internal     0x1800b6e00      Persistence SAVE job
```

They are the five arms of the dispatcher this lane ported (§3.2). Nothing else
moved: `ported` did not move at all, and **that is correct, not a
disappointment** — all eight new bodies are slots of UNPROMOTED vtables, a
population the census states in terms is "NOT counted above". **Score this lane
on the two backlog rows, never on the TOTAL.** The percentage going DOWN while
eight bodies were added is `reference_the_ceiling_percentage_is_a_moving_target.md`
in its plainest form.

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

### 3.3 `??_7TextFileStream@IO@@6B@` — THE SCOPE QUESTION THE BRIEF ASKED

*(see §4)*

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

**MEASURED**

* Both backlog rows, before and after, same tool, same two tips.
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
