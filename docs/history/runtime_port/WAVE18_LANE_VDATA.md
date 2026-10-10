# WAVE 18 — lane VDATA: the seven Video DATA rows

Worktree `C:/code/GIT/wt-w18-vdata`, branch `wave18/vdata`, cut from `d12e8be`.

Subject: `??_7ImageBuffer@Video@@6B@` (7 abort-trap slots to close) and the six
`?_typeInfo@…@Video@@` rows CEIL classed *blocked on attendance*, plus the four
Graphics `?_typeInfo@` rows that ride the same consumer.

---

## 0. PRE-REGISTERED PREDICTIONS

**Written and committed BEFORE any code was written and before any run.**
Commit `88f90b3`. Scored honestly in §5.

### 0.1 The route for the ten `OnVideoError` typeinfos

CEIL §3.4 and the Wave 18 kickoff both say the ten rows are blocked because
"no harness route drives" `Engine::MainLoop::OnVideoError`, and CEIL §5.4
prescribes **synthesising** a call.

**P1 — I predict no synthesis is needed: a real driver already exists.**
`BANISHED_FORCE_DEVICE_LOST=<frame>` (`dx11/src/contextdata.cpp:594-628`,
Wave 9 lane ENGIO) injects one synthetic `DXGI_ERROR_DEVICE_RESET` into a
`Present` result and lets the real teardown run. `WAVE9_LANE_VIDL.md` §4.2
records it firing and reaching the ported body: `Engine::MainLoop::OnVideoError
calls=1`. That is the exact consumer of the ten-element `ids[]`.

Confidence: high. This is `reference_grep_the_harness_before_declaring_no_route.md`
and `reference_holdback_blockers_are_stale_check_them_first.md` for a third time.
The risk is that the driver has rotted since Wave 9 on this tip.

**P2 — attendance under that route.** All ten rows read `post >= 1`, and
`post == 1` exactly, because each `TypeIdX()` in `mainloop_video.cpp` caches in
its own function-local static and `OnVideoError` runs once per injected event.

**P3 — the split between `init` and `post`.** Four rows (the Graphics ones
`reg_unreg_lane3.cpp` resolves eagerly) also carry `init=1`; the six Video rows
read `init=0`. CEIL's phase-boundary correction is what makes this checkable.

**P4 — the control.** `?_singleton@MainLoop@Engine@@1PEAV12@EA` reads `post>0`
(GREEN), as it did on all four of CEIL's routes.

### 0.2 The seven `??_7ImageBuffer@Video@@6B@` trap slots

**P5 — all seven are cheap.** Six of the seven are 1–5 instructions
(`TypeName`, `Type`, `GetFileVersion`, `Reconstruct`, `Destruct`, `Construct`);
only `Serialize` (0x1800f26f0) has a body worth the name, and it is the same
shape as `Achievement::Goal::Serialize`, already ported in
`runtime/src/achievement/goal_serialize.cpp`.

**P6 — closing them makes the row READY with `TRAP=0`,** and
`BANISHED_RT_MINT_SHADOW=1` reports `10 slots / 10 ported / 0 TRAP / 0 purecall`.

**P7 — the row is then PROMOTABLE, and promoting it is behaviour-neutral,**
because every slot holds a faithful port of the body the DLL's own table points
at, and because our ported `ImageBuffer` ctor/dtor already stamp the row through
a `ResolveExport` door (`buffers.cpp:108`), so the stamp follows the promotion.

**P8 — attendance for `??_7ImageBuffer@Video@@6B@` is `post>0` on the ordinary
gameplay route**, because `buffers.cpp:108`'s `ImageBufferVtable()` is on the
ctor path and images are constructed during a normal load.

### 0.3 The census

**P9 — the seven slot bodies do NOT move Video's `called` column.**
`tools/runtime_census.py`'s `mint_symbol_count` deliberately excludes vtable
SLOT TARGET names in the `.inc` from the minted-symbol count, and the current
`--unported` listing for Video contains the vtable row but none of its seven
slot targets. So they are not in the denominator today.

**P10 — Video's numerator moves by 7: +6 typeinfo rows +1 vtable row,**
109 → 116 of 150 = **77.3%**, if P1/P2 and P7 both hold. That is +4.6 points
of the wave's +15.3.

**P11 — the ceiling does NOT move.** 88.0% (132/150) already counts all seven
of my rows as promotable; my lane converts BLOCKED into promoted, which is
numerator work. A ceiling move would require finding a row CEIL called
promotable that is not, or vice versa.

### 0.4 One prediction I expect to be wrong

**P12 — I expect at least one of these twelve to fail,** and the most likely is
P7: a `??_7` row whose only consumers are in-DLL `LEA` stamps is exactly the
shape `reference_a_ported_row_can_be_permanently_unpromotable.md` describes, and
CEIL measured 0 READ / 0 WRITE / 5–118 DATA for all 26 vtable rows including
this one. If our own ctor's `ResolveExport` stamp turns out to be the ONLY door,
the row is promotable but the promotion may be a NO-OP for every object the DLL
constructs itself.

---

## 1. Part A — the seven `??_7ImageBuffer@Video@@6B@` trap slots

CEIL's readiness screen read `10 slots / 3 ported / 7 TRAP / 0 purecall` and
classed the row **BLOCKED**. Restating the kickoff's own rule, because it is the
whole reason this half of the lane exists: **BLOCKED is not a ceiling.** A
trap-carrying row is unfinished, and unfinishing it is ordinary port work.

### 1.1 What each of the seven was, and what closed it

All seven bodies are in `runtime/src/video/imagebuffer_vtable.cpp`, registered
by NAME **and** by ADDRESS behind `BANISHED_RT_VIDIMGVT` (its own switch, so the
row's promotion can be A/B'd with the same binary one flag apart). Every one was
transcribed from `disassemble_function` at its retail VA — not from Ghidra's
decompiled C.

| slot | symbol | retail VA | what it is | what closed it |
|---|---|---|---|---|
| 1 | `?TypeName@…UEBAPEBDXZ` | 0x18001e900 | `MOV RAX,[0x1801fa550]; RET` — one load of `_typeInfo + 8`, i.e. `BaseTypeInfo::name` | read the same field through `RT_DATA_SLOT` (port-registry-aware) rather than by address |
| 2 | `?Type@…UEBAIXZ` | 0x18001e8e0 | `MOV EAX,[0x1801fa548]; RET` — a 4-byte load at offset 0 of the same `BaseTypeInfo`, the typeId | same door |
| 3 | `?GetFileVersion@…UEBAHXZ` | 0x1800f26e0 | `MOV EAX,7; RET` | the constant `7` |
| 5 | `?Serialize@…UEAA_NAEAVStream@IO@@@Z` | 0x1800f26f0 | 10 scalar emissions + `String::Serialize` + `PlatformSerialize`, always returns `true` | full transcription; see §1.2 |
| 6 | `?Reconstruct@…PEAVExternal@Object@@XZ` | 0x18001ea80 | `vtable[0](this,0)` then tail-JMP the ctor — destroy in place, re-construct in place | two explicit steps |
| 7 | `?Destruct@…UEAAXXZ` | 0x18001eab0 | `MOV RAX,[RCX]; XOR EDX,EDX; JMP [RAX]` | the tail call, **with its `flags=0`** |
| 8 | `?Construct@…PEAVExternal@Object@@XZ` | 0x18001eaa0 | `TEST RCX,RCX; JNZ <ctor>; XOR EAX,EAX; RET` | the null test and the `this` return |

**0x1801fa550 is not a second symbol.** `tools/pe_exports.py` resolves it to
`?_typeInfo@ImageBuffer@Video@@…` **+8**. So slots 1 and 2 are the two readers
of the very `?_typeInfo@ImageBuffer@Video@@` row that Part B is about — the two
halves of this lane are the same object seen from two sides, which is why they
are one lane.

**Two of the seven are exactly the shapes Ghidra's C output mangles**
(`reference_ghidra_c_output_vs_disassembly.md`), and reading the listing was not
a formality:

* `Destruct` is a **tail JMP through a vtable slot** whose second argument is
  set by an `XOR EDX,EDX` in the caller. The decompiled C prints it as an
  argument-less call. That argument is the scalar-deleting-destructor flag:
  `0` means *destroy, do not free*. Passing `1` — which is what "no argument, so
  use the obvious one" would produce — frees an object the caller still owns.
* `Construct` is a bare `TEST RCX,RCX / JNZ` with a fallthrough that returns
  zero in EAX. It is MSVC's placement-new codegen (`return new (this)
  ImageBuffer();`). The null test looks unreachable for a virtual and is
  transcribed anyway; "obviously unreachable" is not a licence to drop a branch.

**Grepped before writing** (kickoff §4.1/§4.2): all seven retail VAs and all
seven mangled names, across `code/ engine/ runtime/ standalone/ dx11/`, by
symbol and by address. Zero hits outside the generated mint table. Nothing in
the tree already ported any of them, so this file duplicates nothing and
shadows nothing. **All seven are exported by name** — `tools/pe_exports.py`
resolves each at offset 0 of its own export — so not one needs an address door,
and none of the `AEAA`-means-unexported confusion applies.

### 1.2 `Serialize`, and why its oracle stops at 10

The field order here is the save-file layout for every image in the game, so the
ORDER and the TYPE of each emission is the whole port:

```
0x1800f2741  [+0xa0] &this+0x20   int    width        <- SIGNED slot
0x1800f2756  [+0xa0] &this+0x24   int    height       <- SIGNED slot
0x1800f276b  [+0x98] &this+0x28   uint   dimension
0x1800f2780  [+0x98] &this+0x2c   uint   kind
0x1800f2795  [+0x98] &this+0x30   uint   format
0x1800f27aa  [+0x98] &this+0x34   uint   flags
0x1800f27bf  [+0x98] &this+0x38   uint   addressU
0x1800f27d4  [+0x98] &this+0x3c   uint   addressV
0x1800f27e9  [+0x98] &this+0x40   uint   addressW
0x1800f27fe  [+0x98] &this+0x44   uint   filter
0x1800f280d  CALL 0x1800c9930  (this+0x48, &ser)   System::String::Serialize
0x1800f281a  CALL 0x1800f3130  (this,      &ser)   ImageBuffer::PlatformSerialize
0x1800f2834  MOV AL,1                              always true, no failure path
```

Width and height go through the **signed** slot (+0xa0) and the other eight
through the **unsigned** slot (+0x98). On a binary encoding both are four bytes
and the distinction is invisible; on any other it is not, and the slot number is
what the shipped bytes say. The third argument of every emitter is the field
NAME and retail passes NULL; a text encoding would write those names into the
file, so inventing them would change the format.

**The oracle is `RtSaveTrace::Compare` in the WRITING direction with a prefix
stop at 10 emissions, and the prefix is required rather than cautious.**
`PlatformSerialize` (0x1800f3130) calls the object's own vtable slot 9 —
`Create` — at 0x1800f3221 on **both** directions when `kind` is not 4/5, which
tears down and rebuilds the GPU texture. Replaying the whole body would do that
twice: exactly the shared-state trap the whole `Serialize` family was held back
for, and the reason `save_trace.h` grew a `stopAfter` at all. Ten is read off
the listing — the exact number of scalar emissions this body makes itself before
the first call into anything else.

**What that costs, stated rather than hidden:** `String::Serialize`'s emissions
and everything `PlatformSerialize` does are NOT compared.

### 1.3 Two things found on the way

**(a) `ImageBuffer +0x48` is a `System::String`, not a pixel buffer.**
`runtime/types/video/ImageBuffer.h` calls it `pixelData`, "refcounted pixel
buffer". `Serialize` hands `this+0x48` straight to
`?Serialize@String@System@@QEAAXAEAVSerializer@IO@@@Z`, which accepts nothing
else. Both descriptions cover the same eight bytes — a `System::String` IS one
pointer to a refcounted payload whose count sits at −4, which is why the
destructor in `buffers.cpp` is correct as written and needs no change — but the
NAME sends a reader looking for pixels in what is really a path. Corrected by
comment in the header rather than by rename, so no other lane's file has to move
this wave.

**(b) `?Serialize@String@System@@` is registered by ADDRESS ONLY.**
`string.cpp:1187` is a `RegisterRuntimeAddr`; there is no `RegisterRuntimeExport`
for that symbol anywhere in the tree, checked by grep. The first draft of this
file reached it by NAME, which would have resolved straight past our own Wave 2
port into the DLL — silently — while the census still counted the port
(`reference_converting_a_door_is_not_satisfying_it.md`). It now goes through
`ResolveAddr(Module::Runtime, 0x1800c9930)`, which is what `goal_serialize.cpp`
does and for the same stated reason.

### 1.4 TWO DEFECTS I PUT IN MY OWN CODE AND CAUGHT BEFORE SHIPPING

These sit at the same prominence as the results deliberately. Both are defect
classes this project already has a written reference for, which means neither was
a new idea — they are shapes I walked into anyway, and the transferable part is
*how* each was caught, not that it was.

**(a) A shadow predicate that could never fail.** The `Destruct` shadow was first
written `RT_DIFF_SHADOW(a || b ? true : true, …)`. That is a predicate nobody
evaluates (`reference_a_predicate_nobody_evaluates.md`); it would have printed
green forever, on every run, and its greenness would have been quoted.

*How it was caught:* re-reading my own diff before the first build and asking of
each shadow the question that reference exists to force — *could this term be
false?* It could not. It is now one term that can actually fail: after the
deleting destructor runs, `self->vtable` must no longer hold the value it had on
entry, because the destructor's last act before `~External` is to stamp
`??_7CompiledExternal@Object@@6B@`. That term is written by the destructor, not
by this function's own arithmetic, which is what stops the shadow from restating
the code above it.

**(b) A direct C++ call that bypassed the kill switch entirely — this project's
"off by default was not true" defect, exactly.** `Construct` and `Reconstruct`
both re-run `??0ImageBuffer@Video@@QEAA@XZ`. The first draft called
`Video_ImageBuffer_Ctor` **directly**, because it has external linkage and the
call compiles cleanly.

That is wrong for a reason no compiler or test could have surfaced:
`buffers.cpp`'s registration of that constructor is gated on
`BANISHED_RT_VIDBUFFERS`, and **a kill switch gates the REGISTRATION, not a C++
call.** With `BANISHED_RT_VIDBUFFERS=0` — the A/B half whose entire purpose is to
put `Video::ImageBuffer` back on the DLL — these two bodies would still have run
OUR constructor while every other caller in the process ran retail's. No switch
reaches it, and no counter reports it, so the A/B would have compared two states
that both contained our ctor and reported a clean difference of nothing. That is
`reference_off_by_default_was_not_true.md` reproduced almost line for line: there,
one wrapper called `rtalloc::MemFree` directly and an *unarmed* run mutated
retail's arena.

*How it was caught:* not by a test, and not by review of the body. It was caught
by asking a **gate-shaped** question about a line I had already written and was
happy with — *if someone sets this lane's kill switch to 0, does this line
change?* The answer was no, and a line that does not vary with the switch that is
supposed to gate it is not gated. This is the same test the project states for
instruments (`reference_the_gate_was_never_a_control.md`: test a gate by asking
whether its quantity VARIES with the thing it gates); it applies to ordinary
calls too.

Both bodies now go through `ResolveAddr(Module::Runtime, 0x1800f2580)`, which is
registry-first — our port when registered, the DLL's body when not, which is
exactly what retail's tail `JMP` to that address does. A follow-on commit also
retyped that door `void`-returning rather than `void*`, because the registry may
hand back either retail's body (which ends `MOV RAX,RBX` and returns `this`) or
our port (which returns `void`, leaving RAX undefined): declaring a return one of
the two callees never writes, and then using it, is
`reference_hidden_retslot_wild_write.md`. `Construct`/`Reconstruct` return `self`
themselves, which is correct against both.

### 1.5 A lock hazard, and the guard that earned its keep

`tools/game_dir_lock.sh`'s banner warns against **piping** `acquire`, because a
pipeline exits with the last command's status and a TIMEOUT then reads as a
GRANT. I hit the same hazard in a second costume: I wrote

```
tools/game_dir_lock.sh acquire vdata; echo "ACQUIRE_EXIT=$?"
```

— and the harness reported the **compound** command's status, `0`, while the
acquire had actually TIMED OUT after 900s with `gfxif` still holding the lock.

**What makes this worse than a pipe, and worth recording:** the echoed text was
*correct*. `$?` really was `acquire`'s status, so the output literally said
`ACQUIRE_EXIT=1` while the step's status said `0`. Two fields disagreed, and the
authoritative-looking one was the wrong one — and **the wrapper I added to make
the status MORE visible is what hid it.** Any trailing command discards the
refusal just as a pipe does: no pipe, no `;`, no trailing echo, no `&&` tail.

**`assert-held` is what stopped this becoming a clobber.** I gated the deploy on
`tools/game_dir_lock.sh assert-held vdata`, it returned `VERIFY FAILED: held by
'gfxif', not 'vdata'`, and nothing was written to the game directory. The cost was
about fifteen idle minutes instead of another lane's run measured against my
binary. Every subsequent run in this lane used the self-releasing
`game_dir_lock.sh run` form, which has no exit code to discard and cannot be held
unnoticed.

### 1.6 A third self-inflicted defect, and it is the same one twice

The commit that added §1.5 was written with `git commit -m "…"` in double quotes,
with backticks around the offending shell snippet. **The backticks ran command
substitution.** Git received a message in which the quoted snippet had been
*executed* and replaced by its output, and `$?` had been expanded — so the
committed text read `ACQUIRE_EXIT=127` and "`0` really was acquire's status".
Neither is true. The message describing a command-substitution hazard was
silently rewritten by command substitution, and it made two confident factual
claims that were artefacts of its own corruption.

`reference_heredoc_eats_backslashes.md` already states the rule — **use `Write`
plus `git commit -F`, never inline shell quoting** — and I had read it. The
commit was amended (`a22f0b0`) and the amendment says so in its own body.

Three things make this worth a section rather than a blush:

* **It is the same failure as §1.5, one layer up.** In §1.5 a wrapper meant to
  make a status *more* visible discarded it; here a quoting style meant to make a
  snippet *more* readable executed it. Both times the visible text looked right.
* **It would have been permanent.** A commit message is not re-run, so nothing
  would ever have contradicted `ACQUIRE_EXIT=127`. The next reader would have
  inherited a fabricated number attached to a real lesson — which is precisely
  the failure mode §5.1 is about.
* **It was caught by one line of incidental stderr**: `bash: line 41: acquire:
  command not found`, printed beside a successful commit. Had the snippet been a
  valid command, it would have run silently and the corruption would have been
  invisible. That is luck, not method, and the method is the rule I skipped.

---

## 2. The census effect of Part A — and it is NOT what I predicted

Measured on this branch, `tools/runtime_census.py`, before and after the seven
ports, with the set-diff taken rather than the count
(`reference_diff_the_set_not_the_count.md`):

| | called | ported | covered | ceiling |
|---|---|---|---|---|
| Video, before (`d12e8be`) | 150 | 109 | 72.7% | 88.0% |
| Video, after seven ports | **152** | **110** | **72.4%** | **88.2%** |

**Video's percentage went DOWN.** The set-diff names exactly why: the unported
list gained ONE row and lost none, and the row it gained is
`?PlatformSerialize@ImageBuffer@Video@@AEAAXAEAVSerializer@IO@@@Z` — the callee
`Serialize` reaches by name and nothing in the tree reached before. This is
`reference_porting_can_grow_the_cutover_surface.md` again: a faithful port that
calls a DLL body grows the `called` column.

**P9 holds, and it is why the numerator barely moved.** Six of the seven slot
bodies are NOT in `called` and never were: `runtime_census.py`'s
`mint_symbol_count` deliberately excludes vtable SLOT TARGET names in the `.inc`
from the minted-symbol count, so a slot target is neither called nor countable
as ported. The one that did move both columns is `Serialize` itself, and only
because its save-trace probe spells the mangled name as a LITERAL at a
`ResolveExportRaw` call site.

**So closing seven abort traps is worth exactly +1 to the census numerator, and
that is the honest number.** The value of the work is that the row can now be
promoted at all, which is §3's subject — not the +1.

### 2.1 A correction to the wave's headline: **the 88.0% ceiling is not a fixed
number**

CEIL's ceiling is `(called − unpromotable) / called`. `called` is not a
constant: it grows whenever any lane adds a named or address door. Adding one
row to `called` moved the printed ceiling from 88.0% to 88.2% with no triage
change whatsoever, and with Video's *covered* percentage falling in the same
step.

This matters for how Wave 18 is scored. "Video 72.7% → 88.0%" treats both
endpoints as fixed; the second one moves under three lanes all adding doors at
once, and the first one is already stale. **GATE should re-measure the ceiling on
the merged tree rather than compare against 88.0%,** and should quote the
numerator and denominator, not the percentage. P11 is therefore **WRONG as
stated** — the ceiling did move — though it was right about the mechanism
(nothing in my triage changed).

---

---

## 3. Part B — the route CEIL said did not exist

### 3.1 `BANISHED_FORCE_DEVICE_LOST` — no synthesis was needed

CEIL §3.4 and kickoff §1 both record the ten `?_typeInfo@` rows as *blocked on
attendance*: one consumer in the whole tree (the ten-element `ids[]` at
`runtime/src/engine/mainloop_video.cpp:487-499`, inside
`RtEngine_MainLoop_OnVideoError`), that consumer is the device-lost recovery
path, and `post=0` on all four of CEIL's routes. CEIL §5.4 therefore prescribes
**synthesising** an `OnVideoError` call.

**A real driver was already in the tree.** `BANISHED_FORCE_DEVICE_LOST=<frame>`
(`dx11/src/contextdata.cpp:594-628`, Wave 9 lane ENGIO) substitutes one
synthetic `DXGI_ERROR_DEVICE_RESET` for a `Present` result and lets the genuine
teardown and rebuild run. `WAVE9_LANE_VIDL.md` §4.2 records it firing end to end
and reaching the ported body with `calls=1`. It still works on this tip:

```
[DeviceLost] ARMED: a synthetic DXGI_ERROR_DEVICE_RESET will be injected on Present #400
[DeviceLost] INJECTING DXGI_ERROR_DEVICE_RESET (real Present returned 0x00000000)
[DeviceLost] hr=0x887a0007 onLost=... -> calling
[DeviceLost] recreate OK; onRestored=... -> calling
[DeviceLost] device-lost round trip COMPLETE
[RtDiff]   Engine::MainLoop::OnVideoError    calls=1  worked=?  divergences=0
[RtDiff]   Engine::MainLoop::OnVideoRestart  calls=1  worked=?  divergences=0
```

This is the **third stale stated blocker of this wave**
(`reference_holdback_blockers_are_stale_check_them_first.md`), and it cost
nothing to find: it is one grep of `dx11/` for a `BANISHED_*` variable, which is
what `reference_grep_the_harness_before_writing_unverifiable.md` already tells
this project to do before writing "no route".

### 3.2 The run batch

Debug build, DLL present, one control/armed pair against **one binary** under a
single `tools/game_dir_lock.sh run vdata` hold (never piped, and the batch script
`assert-held`s rather than acquires, so it cannot itself deadlock). Identical in
every respect but `BANISHED_RT_MINT_ROWS`. Env: `BANISHED_AUTOSTART=1
BANISHED_RT_DIFF=1 BANISHED_W17_ATTEND=1 BANISHED_RT_MINT_SHADOW=1
BANISHED_RT_SAVE_TRACE=1 BANISHED_FORCE_DEVICE_LOST=400 BANISHED_PAUSEMENU=900
BANISHED_PAUSEMENU_QUIT=120`. Both halves `rc=0`, both crash logs **0 bytes**,
both `[RtResolve] 0 misses`. Logs in `docs/runtime_port/wave18_lane_vdata/`.

### 3.3 Attendance — and what the instrument's subject set was

The counter is CEIL's, **unmodified**: `standalone/wave17_door_attend.cpp`, a
fixed **63-entry watch list spelled as literals** — 62 subject rows plus the
wiring control. That set is stated here because a screen that cannot name its own
subject invites a green to be cited for something it never looked at.

Both of CEIL's controls hold on this run:

* **Wiring.** `?_singleton@MainLoop@Engine@@1PEAV12@EA` reads `init=1 post=3` →
  GREEN. The instrument SUPPRESSES the whole table rather than print a page of
  plausible zeros if this reads 0.
* **Phase boundary.** The marker sits after `AddressRegistryInit()`, not at the
  end of `RuntimePortInit()`, so a resolve during registration is not counted as
  a consumer running.

| row | init | post | |
|---|---|---|---|
| `?_typeInfo@ImageBuffer@Video@@` | 0 | 1 | ATTENDED |
| `?_typeInfo@PixelProgram@Video@@` | 0 | 1 | ATTENDED |
| `?_typeInfo@RenderState@Video@@` | 0 | 1 | ATTENDED |
| `?_typeInfo@VertexLayout@Video@@` | 0 | 1 | ATTENDED |
| `?_typeInfo@VertexProgram@Video@@` | 0 | 1 | ATTENDED |
| `?_typeInfo@VideoProgram@Video@@` | 0 | 1 | ATTENDED |
| `?_typeInfo@EdgeMesh@Graphics@@` | 0 | 1 | ATTENDED |
| `?_typeInfo@FrameCopyImageBuffer@Graphics@@` | 0 | 1 | ATTENDED |
| `?_typeInfo@GraphicsMesh@Graphics@@` | **1** | 1 | ATTENDED |
| `?_typeInfo@ReflectionImageBuffer@Graphics@@` | 0 | 1 | ATTENDED |
| `??_7ImageBuffer@Video@@6B@` | 0 | 1 | ATTENDED |

**`post=1` means the consumer RAN. It does NOT mean "ran once"** — every
`TypeIdX()` caches in its own function-local static, so one read is all the
instrument can ever see no matter how often the body runs. The screen prints
nothing that could be read as frequency, and this report claims none.

**This single route attends 49 of the 62 subject rows, where CEIL's union of all
four routes attended 36.** The device-lost path is the most productive route
anyone has run at this subject.

### 3.4 Readiness and value

At exit, all ten typeinfo rows:

```
READY  ?_typeInfo@…  id=0x…  idcmp=yes  strs=0  ptr=2  fac=2traps
BaseTypeInfo: 624 field comparisons, 0 MISMATCH, 0 typeId not-yet-comparable
READY  ??_7ImageBuffer@Video@@6B@   slots=10 ported=10 TRAP= 0 purecall= 0
```

`idcmp=yes` is the load-bearing column: our minted `typeId` is compared against
the live DLL's own value and agrees. `ptr=2` is the pointer-identity column,
scored SEPARATELY from `strs=` and permanently harmless — `BaseTypeInfo::name`
is content-only in both shipped binaries. `fac=2traps` does not bite, for the
structural reason CEIL §2.6 measured on exactly this subject.

**The value check, and it is not a tautology.**
`Collection::Array::SetSize @0x18004f620` is the `_videoResources` sweep:
`OnVideoError` walks `ExternalFactory::FindNext` once per type id and appends
every live resource it finds. It reads **`calls=995` in BOTH halves.** In the
armed half those ten ids came from OUR minted nodes and in the control from the
DLL's statics; had any of the ten been wrong, `FindNext` would have matched a
different set and the count would have moved. 995 is also the number Wave 9 lane
VIDL measured independently.

### 3.5 The vtable's effectiveness control — and why the counts hid it

"The boot still works" is also what a promotion that did NOTHING reports. The raw
site counts are **601 control / 600 armed** and say nothing at all. The **set-diff
of the site NAMES** says everything (`reference_diff_the_set_not_the_count.md`):

```
only in armed:    Video::ImageBuffer                  calls=6  divergences=0
only in control:  Video::Context::SetInverseScale     calls=2383
only in control:  Math::Transform::Identity()         calls=1
```

`Video::ImageBuffer` is the scalar deleting destructor site
(`runtime/src/object/deleting_dtor.cpp`). It **does not exist in the control run
at all**, so real dispatch went through slot 0 of the MINTED table and came back
clean. A second counter nobody had to look for corroborates it independently:
`Video::ImageBuffer::PlatformDestruct` moves **4 → 10**, exactly those six extra
destructions. This is the `??_7SpriteSheet@UI@@6B@` shape reproduced.

**The two control-only sites were checked, not dismissed as jitter.**
`Video::Context::SetInverseScale`'s only caller in the tree is the weather
particle draw (`code/Game/Game/weather_particle_draw.cpp:274`), reached only when
weather is active with visible tiles; the neighbouring draw-volume counter
`Video::Context::SetColor` moves 195,598 → 170,022 in the same direction. Weather
is simulation state and the promotion cannot reach it.

### 3.6 The confirming boot — because "safe when ARMED" is a different claim

The A/B pair proves the rows are safe and live when armed by env var.
Registering them unconditionally is what every future run does, so that claim
got its own run, on the built exe, in one further lock hold:

```
[RtMint] promoted 31 minted DATA row(s) -- live on every run     (20 + this lane's 11)
[DeviceLost] device-lost round trip COMPLETE
[RtDiff]  Video::ImageBuffer                          calls=6   divergences=0
[RtDiff]  Engine::MainLoop::OnVideoError              calls=1   divergences=0
[RtDiff]  Collection::Array::SetSize @0x18004f620     calls=995 divergences=0
[RtMintShadow] ==== clean: 0 problem(s) ====      (both passes)
[RtResolve] 0 misses
```

`rc=0`, crash log **0 bytes**. The minted-slot-0 dispatch (`calls=6`) and the
995-resource sweep both reproduce the armed half exactly, with no env var set.

**The tree-wide divergence total is 17 in 6 sites, and it is the SAME SET in all
three runs** — checked rather than assumed, by diffing the divergent site names:
three deliberate negative controls (`DLG2 pin`, `UI3 pin`, `StartTimer`), the
pre-existing `Object::Factory::Startup` self-check, and `UI::SpriteSheet::
Construct` / `TypeName`, whose divergence is the documented vtable- and
string-identity of an earlier wave's promotion. **None is this lane's.**

---

## 4. Verdicts

**Eleven rows PROMOTED** — registered unconditionally in
`standalone/reg_mint_promoted.cpp`, each symbol spelled as a literal at its
`RegisterRuntimeExport` call site so `tools/runtime_census.py` can see it.

| row | verdict | evidence |
|---|---|---|
| `??_7ImageBuffer@Video@@6B@` | **PROMOTED** | §1 (7 traps closed → READY 10/10 TRAP=0), §3.5 (effectiveness GREEN by set-diff) |
| `?_typeInfo@ImageBuffer@Video@@` | **PROMOTED** | §3.3 ATTENDED, §3.4 `idcmp=yes` + the 995 sweep |
| `?_typeInfo@PixelProgram@Video@@` | **PROMOTED** | " |
| `?_typeInfo@RenderState@Video@@` | **PROMOTED** | " |
| `?_typeInfo@VertexLayout@Video@@` | **PROMOTED** | " |
| `?_typeInfo@VertexProgram@Video@@` | **PROMOTED** | " |
| `?_typeInfo@VideoProgram@Video@@` | **PROMOTED** | " |
| `?_typeInfo@EdgeMesh@Graphics@@` | **PROMOTED** | " (rides the same consumer) |
| `?_typeInfo@GraphicsMesh@Graphics@@` | **PROMOTED** | " |
| `?_typeInfo@ReflectionImageBuffer@Graphics@@` | **PROMOTED** | " |
| `?_typeInfo@FrameCopyImageBuffer@Graphics@@` | **PROMOTED** | " |

**Nothing in this lane's subject is left UNPROMOTABLE, and nothing is left
BLOCKED.** All seven rows of Tranche C are decided, plus four Graphics rows that
were the next wave's subject.

### 4.1 What is NOT claimed: seven slots are PORTED, NOT VERIFIED

Slots 1, 2, 3, 5, 6, 7, 8 (`TypeName`, `Type`, `GetFileVersion`, `Serialize`,
`Reconstruct`, `Destruct`, `Construct`) report **no `[RtDiff]` site in either
half** — a site is not registered until its body is entered, so absence is
`calls=0`. `Serialize`'s save-trace oracle reported no entry at all: the only
`[RtSaveTrace]` lines in either run are `Achievement::Goal::Serialize`. That is
the `calls=0` column and it is not a pass
(`reference_uncalled_port_is_not_verified.md`).

Only slot 0 (the deleting destructor) and slot 9 (`Create`, 6 → 9) were
dispatched. Promoting is still right, for the reason lane SS gave for
SpriteSheet slot 5: `TRAP=0` means those seven can no longer `abort()`, the
dispatched slots ARE clean, and leaving the row unpromoted would not verify the
seven either — it would only keep slot 0 unreachable too.

**Expiry / what would verify them:** a route that deserialises or reconstructs an
`ImageBuffer` built by OUR `ExternalFactory` rather than the DLL's. `Serialize`
and `Reconstruct` are both on the compiled-external load path; a lane that owns
that path gets all seven for free.

### 4.2 Census

| | called | ported | covered |
|---|---|---|---|
| Video, at `d12e8be` | 150 | 109 | 72.7% |
| Video, after Part A | 152 | 110 | 72.4% |
| **Video, after Part B** | **152** | **117** | **77.0%** |
| Graphics, at `d12e8be` | 99 | 66 | 66.7% |
| **Graphics, after Part B** | **99** | **70** | **70.7%** |
| TOTAL | 1166 | 950 | **81.5%** (from 80.6%) |

Video's `AT CEILING` identity is `ported >= called − unpromotable` =
`117 >= 152 − 18` = `117 >= 134`: **not yet**, 17 rows short, which is exactly
GFXIF's nine plus VRES's seven plus `?PlatformSerialize@` (the row this lane
added). Stated as the count identity rather than a percentage, per the advisor's
amendment.

---

## 5. Prediction scoring — 8 right, 3 wrong, 1 given no credit

Pre-registered at **`88f90b3`**, before the first line of the port and before any
run.

| # | prediction | outcome |
|---|---|---|
| P1 | a real driver exists; no synthesis needed | **RIGHT** — `BANISHED_FORCE_DEVICE_LOST`, fired |
| P2 | all ten `post>=1`, and `post==1` exactly | **RIGHT** — all ten `post=1` |
| P3 | four rows carry `init=1`, six read `init=0` | **WRONG** — exactly ONE (`GraphicsMesh`) |
| P4 | wiring control GREEN | **RIGHT** — `init=1 post=3` |
| P5 | six slots trivial, only `Serialize` substantial | **RIGHT** |
| P6 | row becomes READY `10/10 TRAP=0` | **RIGHT** — verbatim |
| P7 | row PROMOTABLE and promotion behaviour-neutral | **RIGHT** — and live, §3.5 |
| P8 | `??_7ImageBuffer@` attended on the gameplay route | **RIGHT** — `post=1` |
| P9 | the seven slot bodies do not move `called` | **RIGHT** — six do not; `Serialize` does, via its probe's literal |
| P10 | Video numerator +7 → 116/150 = 77.3% | **half right** — +7 exactly (110→117), but of 152, so **77.0%** |
| P11 | the ceiling does not move | **WRONG** — 88.0% → 88.2%, §2.1 |
| P12 | at least one prediction fails, most likely P7 | **RIGHT that one would fail, WRONG about which** |

### 5.1 P3: inheriting a count without checking what it was a count OF

**This is the error class, not a footnote about one prediction.** It is the same
class this wave has now hit repeatedly, and it is worth naming precisely because
the number I inherited was *correct*.

CEIL §2.2 says its phase-boundary correction moved "the four lane-3 rows" out of
ATTENDED. That statement is true. I read "four rows" as **four of MINE** and
predicted four of my ten would carry `init=1`. The four rows CEIL means are
`MaterialInstance`, `Material`, `GraphicsMesh` and `Particle` — and **three of
the four are outside my ten**. Exactly one of my rows (`GraphicsMesh`) carries
`init=1`; the other nine read `init=0`.

The failure was not arithmetic and not a misquotation. **I carried a count across
a boundary without checking what set it was counting over.** A count is only
meaningful with its denominator attached, and "four rows" borrowed from a
neighbouring lane's prose silently acquires *my* subject as its denominator the
moment I write it down.

The generalisation, and it is the one to carry forward: this is the same shape as
`reference_kind_count_read_as_a_portstate_count.md` (a count of KINDS read as a
count of PORT-STATES) and as this lane's own §2.1 (a *ceiling* percentage read as
a fixed target when its denominator moves). Three different manifestations, one
mechanism — **a number inherited from prose is a number nobody re-derived, and
the first thing to re-derive is not its value but its subject.**

I caught it by grep before the run rather than after, which is the only reason it
never became a claim. The grep took ten seconds; the assumption had already
survived being written into a pre-registered prediction and committed.

**P11 is the one that mattered**, and it is written up as §2.1 because it changes
how the wave is scored, not just my scoreboard.

**P12 deserves no credit.** "At least one of these will fail" is unfalsifiable in
the direction that counts, and naming P7 as the likely one was wrong. It is
recorded as a prediction because it was pre-registered, not because it was
useful.

---

## 6. What this lane's measurements CONTRADICT

1. **CEIL §3.4 and kickoff §1: "no harness route drives it", and CEIL §5.4's
   prescription to synthesise a call.** Both wrong. §3.1. The route was in
   `dx11/` the whole time and is documented in another wave's own lane doc.
2. **The 88.0% ceiling as a fixed target.** §2.1. It moved to 88.2% under my own
   lane with zero triage change, because `called` is not a constant.
3. **CEIL §2.2's "four lane-3 rows", read as four of the ten.** §5, P3. CEIL's
   own statement is correct; the misreading was mine, and it is recorded because
   the next reader of that sentence will be tempted the same way.
4. **The implicit worth of "close the traps".** Closing seven abort traps is
   worth **+1** to the census numerator, not +7 (§2). The row's promotability is
   the return, not the count.

### 6.1 One pre-existing defect found in passing, verified NOT this lane's

`tools/dispatch_slot_map.py --check` reports **one** name mismatch at
`0x140152870`, reading `?_typeInfo@External@…`. That string is a **COMMENT** at
`standalone/reg_mint_promoted.cpp:688` — the ellipsis is prose — and the
identical line exists at `d12e8be`, verified with `git show`. The checker has no
comment stripper and read prose as code
(`reference_uncommented_checker_manufactures_work.md`, whose whole point is that
such false positives cluster on dead things). It is not this lane's, and
`--check` is not at zero on the base commit either.

---

## 7. Files, worktree, commits

Worktree `C:/code/GIT/wt-w18-vdata`, branch `wave18/vdata`, cut from `d12e8be`.

| file | what |
|---|---|
| `runtime/src/video/imagebuffer_vtable.cpp` | NEW — the seven trap-slot bodies |
| `runtime/src/video/rt_video_port.cpp` | +`RtVideoImageBufferVtableRegister()` behind `BANISHED_RT_VIDIMGVT` |
| `runtime/types/video/ImageBuffer.h` | `+0x48` name correction (comment only) |
| `standalone/reg_mint_promoted.cpp` | the eleven promotions and their warrant |
| `tools/wave18_vdata_run.sh` | the control/armed batch (asserts the lock, never acquires) |
| `tools/wave18_vdata_confirm.sh` | the confirming boot with promotions unconditional |
| `tools/wave18_vdata_report.sh` | pulls the four screens from the SNAPSHOTTED logs |
| `docs/runtime_port/wave18_lane_vdata/` | control / armed / promoted logs |
