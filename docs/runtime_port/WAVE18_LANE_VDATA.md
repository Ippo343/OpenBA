# WAVE 18 — lane VDATA: the seven Video DATA rows

Worktree `C:/code/GIT/wt-w18-vdata`, branch `wave18/vdata`, cut from `d12e8be`.

Subject: `??_7ImageBuffer@Video@@6B@` (7 abort-trap slots to close) and the six
`?_typeInfo@…@Video@@` rows CEIL classed *blocked on attendance*, plus the four
Graphics `?_typeInfo@` rows that ride the same consumer.

---

## 0. PRE-REGISTERED PREDICTIONS

**Written and committed BEFORE any code was written and before any run.**
Scored in §6.

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

### 1.4 A predicate that could not fail, caught before it shipped

The `Destruct` shadow was first written `RT_DIFF_SHADOW(a || b ? true : true,
…)`. That is a predicate nobody evaluates
(`reference_a_predicate_nobody_evaluates.md`) and it would have printed green
forever. It is now one term that can actually fail: after the deleting
destructor runs, `self->vtable` must no longer be the value it held on entry,
because the destructor's last act before `~External` is to stamp
`??_7CompiledExternal@Object@@6B@`. The term is written by the destructor, not
by this function's own arithmetic, which is what stops the shadow from restating
the code above it.

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

*(Sections 3 onward — the attendance and promotion measurements — follow the run
batch.)*
