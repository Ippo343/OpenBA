# Wave 10, lane DLG2 — `??0Dialog@UI@@` and `??1Dialog@UI@@`

**Scope:** the two UI census rows that ENTERED when lane UI3 closed the
`DialogInstance` family. `WAVE10_LANE_UI3.md` §7b named them and carried a recipe.

**Branch** `wave10/lane-dlg2`, cut from `wave10/base` at `bed7a3f`
("WAVE 10: merge lane UI3 — the four DialogInstance rows, and the Dialog.h fields
were named BACKWARDS").
**Worktree** `C:\code\GIT\untitled_rev_eng_project\.claude\worktrees\agent-aaa2b6fa4a7217b6e`.

**Verdict: both PORTED, REGISTERED, and VERIFIED against the shipped bodies by a
synthetic-subject differential — 6 rounds, 0 divergences, 340 bytes of measured
effectiveness, under both allocator settings.** Three of four negative controls
caught; the fourth is proved STRUCTURALLY UNCATCHABLE rather than excused (§5).
Both are LIVE, and `??1Dialog` turns out to have been on the element-creation path
of every dialog in the game since lane UIF (§6).

---

## 0. Why these two rows exist, and why that is progress

Neither name was in the census before 2026-09-08. Retail **inlines** both bodies
into the 6-argument `?AddController@DialogInstance@UI@@` — the stack temporary
there is a `UI::Dialog`, proved by the `??_7Dialog@UI@@6B@` stamp at `0x180112deb`
— and UI3's port calls the out-of-line exports by name instead of re-inlining
them.

So an **inlined, unnamed** dependency became a **named, scopeable** export. That is
`reference_porting_can_grow_the_cutover_surface.md` working as intended. This lane
closes both.

---

## 1. Both manglings, read out of the shipped export directory FIRST

`tools/pe_exports.py` against `C:\GOG Games\Banished\Runtime-x64.dll`
(4,926 exports), before a line of body was written
(`reference_resolveexport_phantom_names.md`):

| address | exported name | present |
|---|---|---|
| `0x180112db0` | `??0Dialog@UI@@QEAA@XZ` | yes |
| `0x180112e40` | `??1Dialog@UI@@UEAA@XZ` | yes |

**No phantom.** The two spellings differ and both are load-bearing: the ctor is
`QEAA` (public, non-virtual), the dtor is `UEAA` (virtual). `??1Dialog@UI@@QEAA@XZ`
is **not** in the table and would resolve NULL in silence
(`reference_phantom_dtor_mangling_qeaa_vs_ueaa.md`).

The three vtables the ctor stamps were checked the same way and are exported at
exactly the addresses the shipped `LEA`s name — `??_7Base@Object@@6B@` 0x180154788,
`??_7External@Object@@6B@` 0x1801547a0, `??_7Dialog@UI@@6B@` 0x1801562a0 — so they
are resolved BY NAME, never minted (the `??_7Geometry@Graphics@@6B@` lever).

**Grepped for the port before writing it** (`reference_grep_for_the_port_before_writing_it.md`),
by address AND by mangled name: both addresses appeared only in prose and in two
`ResolveExport` helper wrappers (`dialog_create.cpp:366/372`, `dialog.cpp:2030`).
No body existed.

---

## 2. `??0Dialog@UI@@QEAA@XZ` — three facts only the disassembly shows

`reference_ghidra_c_output_vs_disassembly.md`. The body is 0x2b instructions.

**(a) The first qword is written THREE times** — Base, External, then Dialog: the
MSVC base-ctor chain, fully inlined. The first two stores are dead in the shipped
binary and are **transcribed anyway**. "Provably overwritten in this build" is a
claim a port should not bake in.

**(b) `+0x44` is NEVER written.** `0x180112e0b` is `MOV dword [RCX+0x40],EDX` and
the next store is a QWORD at `+0x48`. A "tidy" port zeroing `+0x40` as a qword
would write four bytes retail leaves holding caller stack. The oracle EXCLUDES
`+0x44..+0x47`, counts the exclusion, and prints it — comparing bytes neither side
writes fails 100 % of calls and reads as a catastrophic defect
(`reference_oracle_must_not_compare_unwritten_bytes.md`). It also gets **its own
screen** (§5).

**(c) `+0x30` is `-1`, not 0.** The single most likely slip in a body that is
otherwise sixteen zero stores, and what negative control 1 corrupts.

**Written extent `+0x00..+0x87`.** `runtime/types/ui/Dialog.h` declared out to
`+0x7f`; this lane adds the one member `0x180112e2a` proves and **still adds no
`sizeof` assert** — a ctor's written extent is a LOWER BOUND on a class size, never
the size.

---

## 3. `??1Dialog@UI@@UEAA@XZ` — the asymmetric String release, and a WRONG RECIPE

Four `~Array` calls (`+0x78`, `+0x68`, `+0x58`, `+0x48`, in that descending order),
the `System::String` release at `+0x28`, then a **tail-JMP** to `~External`.

The release is retail's asymmetric one, transcribed rather than tidied:
* refcount hits zero → the FIELD is first rewritten to `data - 8`
  (`ADD qword [RBX+0x28],-0x8`) and THAT is freed. **The field is left holding it.**
* refcount still positive → the field IS nulled.

### 3a. UI3 §7b's recipe for the four array dtors is WRONG, and this lane measured it

WAVE10_LANE_UI3.md §7b says `0x180039360` and `0x180039280` "are the same family"
as the two registered `J1_ENTRY`s and suggests two more `J1_ENTRY` lines.
`reference_holdback_blockers_are_stale_check_them_first.md` says re-check a stated
blocker; the same applies to a stated recipe. All 115 leading bytes of each were
read with Ghidra `read_memory` and compared against the registered J1 reference
`0x180033960`:

| address | leading shape | verdict |
|---|---|---|
| `0x180009460` | `MOV RDI,RCX ; MOV RSI,[RCX] ; TEST` | J1 — free the block |
| `0x180039440` | same | J1 — free the block |
| `0x180039360` | `MOV RDI,RCX ; MOV R8D,[RCX+8] ; MOV RDX,[RCX] ; CALL rel32 →0x180039641` | **NOT J1** |
| `0x180039280` | same shape, `CALL rel32 →0x180039581` | **NOT J1** |

The last two are `~Array<T>` for a `T` that HAS a destructor: an extra
element-destroy pass before the free, through two DIFFERENT helpers. **Registering
them as J1 would have silently replaced a body that destroys every element with one
that only frees the block — a leak, landed under a comment claiming a byte-level
identity that does not hold.** They are called by address and recorded as this
port's owed residue (§7).

None of the four is exported (checked against all 4,926 names), so the address door
is a genuine unexported internal, not the "the comment said unexported and it was
merely private" mistake (`reference_aeaa_is_private_not_unexported.md`).

---

## 4. The allocator, in BOTH configurations

The dtor frees. `?MemFree@` is reached BY ADDRESS (`0x1800c7ad0`), the discipline
`array.cpp` set three waves ago: the DLL's allocator unarmed, ours armed, and when
armed lane ALLOC's swap has patched the DLL's own prologues so its internal frees
follow us too. **No configuration puts the two sides on different heaps.** Nothing
in the file reads `BANISHED_RT_ALLOC`; both settings were run anyway (§5 table),
because "structurally fine" is a prediction.

---

## 5. Verification — a synthetic-subject differential, and FOUR controls

Both bodies are pure functions of one buffer, so they do not need a live caller to
be verified — the argument `reference_pure_functions_verify_by_sweep.md` makes for
`GetAlignedPosition`. Two private subjects are poison-filled byte-identically, OURS
runs on one and **the shipped export** on the other (via `ResolveRuntimeRaw`, never
`ResolveAddr` — `reference_resolveaddr_consults_port_registry_first.md`), and the
whole `0x88` extent is compared.

Six cases: three ctor rounds (two poison seeds plus a re-construct-in-place), and
three dtor rounds — empty, four REAL heap array blocks with String refcount 1 (the
freeing branch), and the same with refcount 3 (the sharing branch). Heap pointers
are compared as null/not-null and by header, never by value.

### 5a. The headline numbers

```
[RtPort]   ??0/??1Dialog[pins]: 10 pins, 0 failed
[RtPort]   [DLG2 pin control] decoder DISCRIMINATES (wrong-value 1, absent 1, ambiguous 1 of 3)
[RtPort]   [DLG2 diff] rounds=6 divergences=0  effectiveness=340 bytes changed
                       (excluded=12 in the +0x44 hole)  hole screen: 3 checked / 0 failed
```

**`effectiveness=340`** is the number that makes `divergences=0` mean something: a
probe with nothing to probe reads green (`reference_probe_needs_an_effectiveness_control.md`).
340 compared bytes were actually changed from the poison across the six rounds.
`excluded=12` is three ctor rounds × the four hole bytes.

### 5b. The four negative controls — SCORED SEPARATELY, and one is unreachable

`BANISHED_DLG2_NEGCTRL=<n>`. Each was run as its own process.

| n | the defect | byte compare | hole screen | verdict |
|---|---|---|---|---|
| 1 | ctor `+0x30 = 0` instead of `-1` | **3 caught** | 0/3 | CAUGHT |
| 2 | ctor zeroes `+0x40` as a QWORD | 0 (blind by design) | **3/3 failed** | CAUGHT, by the OTHER screen |
| 3 | dtor nulls the String in BOTH branches | **1 caught** | 0/3 | CAUGHT |
| 4 | ctor drops the two dead base vtable stamps | 0 | 0/3 | **NOT CAUGHT — structurally** |

**Control 2 is why the hole gets its own screen.** The byte compare *skips*
`+0x44..+0x47`, so it is structurally blind to a write there. Scoring the two
together would have produced "3 of 3 caught" and hidden that one screen caught none
of them (`reference_union_control_hides_a_blind_screen.md` — a 3/3 on a union that
could not distinguish "both needed" from "one is blind"). Reported separately, the
picture is exact: the byte compare catches 1 and 3, the hole screen catches 2.

**Control 3 catches 1 of 6 rounds, and that is the correct number, not a partial.**
The String asymmetry is only observable on the freeing branch, which is case 4
alone: cases 0–2 are ctor rounds, case 3 has a null String, and case 5 takes the
sharing branch, which nulls the field anyway — the corrupted body and retail agree
there *because retail also nulls it*. 1 of 1 reachable.

**Control 4 was NOT caught, and this is the instrument being honest.** Fact 2(a):
the Base and External stamps at `+0x00` are immediately overwritten by the Dialog
stamp three instructions later. **A post-state comparison is structurally incapable
of expressing whether they happened** — the test
`reference_diagnostic_field_cannot_express_the_answer.md` prescribes ("could this
field express the answer if the answer were different?") returns NO. It was
included deliberately, to have one control the oracle *must* miss: a 4/4 here would
have meant the instrument was wrong. What covers those two stores instead is the
transcription itself and the `[pins]` screen; nothing verifies them dynamically,
and that is stated rather than papered over.

### 5c. The pin screen, and the pin that FAILED

Ten shipped-byte pins, located BY PATTERN and required to occur exactly once. On
the first run the line read **`10 pins, 1 failed`**: the pin for the freeing
branch's field rewrite guessed the encoding `48 83 6B 28 08` from the mnemonic —
reading the sign into the immediate and the opcode extension as `/5` (SUB). The
shipped bytes at `0x180112e9a` are `48 83 43 28 F8` (`ADD r/m64, imm8` is REX.W
`83 /0`). **A pin that fails is the instrument working**; it is the only reason
this was caught, and it is why the decoder carries its own control, which rejects
all three of a wrong value, an absent pattern and an ambiguous one (3/3).

### 5d. Run matrix

| run | config | pins | diff | controls |
|---|---|---|---|---|
| baseline | `AUTOSTART` | 10/1 failed | *never ran — see §5e* | — |
| probe | `AUTOSTART` | 10/0 | 6 rounds, 0 div, eff 340 | — |
| negctrl1..4 | `AUTOSTART` + `NEGCTRL=n` | 10/0 | see 5b | 3 caught, 1 unreachable |
| alloc_armed | `AUTOSTART` + `RT_ALLOC=1` | 10/0 | 6 rounds, 0 div, eff 340 | — |
| live_menu | Mods menu click feed | 10/0 | 6 rounds, 0 div, eff 340 | — |
| final_off | `RT_DLGCTOR=0` | (silent, correct) | — | kill switch works |

Every deploy went through `tools/game_dir_lock.sh run lane-DLG2`, with `verify`
immediately before each launch and the build/deployed md5 quoted on both sides
(`ca64f6e35bbabf74323c7989da06f56e` for the final binary).

### 5e. THE PROBE HAD NO ROUTE, AND THE FIRST RUN PROVED IT

On the first measured boot the differential **never executed**. Its gate fired on
the first call into either port; `??0Dialog` is reached ONLY from the 6-arg
`AddController` and only on the `if (desc)` branch, which a 20-second autostart
boot does not take. The body would have shipped with a green pin line and a
differential that had never run — the `calls=0`-reads-as-clean shape, in its
"armed but never triggered" variant
(`reference_uncalled_port_is_not_verified.md`).

The fix is not to wait for a caller. Both bodies are pure, so
`RtUiDialogCtorDtorProbeNow()` is exported and called from `dialog_create.cpp`'s
own `ProbeGate` — which the same run showed firing, and which by construction runs
after the DLL's allocator is up. **`grep the harness before declaring no route`**
generalises here: the route existed one file over.

---

## 6. Liveness — and `??1Dialog` was already on every dialog's element path

A verified body nothing calls is dead code that reads exactly like a landed port,
and this lane's differential deliberately does not need a live caller — so nothing
else in the file could tell the two apart. Two one-shot `[DLG2] FIRST LIVE CALL`
lines can, and they found something:

* `??1Dialog@UI@@` fires in **every** run, including the plain autostart boot.
* `??0Dialog@UI@@` fires only in the Mods-menu run.

The asymmetry is real and it is not ours. Lane UIF's
`?CreateInstance@ElementDescription@` (`runtime/src/ui/dialog.cpp:2030`) transcribes
the retail-inlined Dialog **ctor** in place (`DialogTempCtor`) but calls the
exported **dtor** through `RT_EXPORT_ONCE("??1Dialog@UI@@UEAA@XZ")`. `RtResolveCached`
→ `ResolveExport` → the port registry, so **our `~Dialog` now runs on the
element-stamping path of every dialog the game builds.** That is a large blast
radius and it is stated rather than minimised; it is also why the row has its own
kill switch, `BANISHED_RT_DLGCTOR=0`.

**Follow-up for another lane, not taken here:** `DialogTempCtor` in
`runtime/src/ui/dialog.cpp` is now a second transcription of a body this tree
exports. UIF chose to transcribe rather than call, deliberately, to add no callee;
converting it is UIF's call, not this lane's. Recorded so a later census does not
rediscover it as a surprise.

---

## 7. The census — as a SET DIFF, and the per-namespace view HIDES the entrants

`tools/runtime_census.py --unported`, A/B'd by moving
`runtime/src/ui/dialog_ctor_dtor.cpp` out of the tree and back, run from THIS
worktree (`reference_scanner_walks_cwd_not_script.md`).

**UI namespace: 22 → 20.**

```
LEFT (2 — this lane's entire brief):
  ??0Dialog@UI@@QEAA@XZ
  ??1Dialog@UI@@UEAA@XZ
ENTERED: (none)
```

**That view is incomplete, and the full census says so. Whole census: 228 → 228.**

```
LEFT (2):     ??0Dialog@UI@@QEAA@XZ, ??1Dialog@UI@@UEAA@XZ
ENTERED (2):  0x180039280, 0x180039360     (bucketed "addr / internal")
```

The two entrants are §3a's non-J1 array destructors. They are **not new work this
port created** — they always ran, inside retail's `~Dialog`; porting the caller is
what made them nameable. That is the same "grew the cutover surface, and that is
progress" shape as §0, one level down.

### 7a. A SEVENTH `runtime_census.py` blind spot, and one this lane nearly committed

Two separate findings, and the first was self-inflicted:

1. **The first draft routed all four array dtors through one
   `ArrayDtorAt(addr, member)` helper.** Tidier, and INVISIBLE: the scanner greps
   for a literal `ResolveAddr(Module::Runtime, 0x…)` and cannot see the same fact
   spelled through a variable, so the census reported this port as reaching nothing
   unported while it reached two unported bodies on every call. That is
   `reference_literal_grep_blind_to_variables.md` for the fifth time, committed
   *while porting a body whose whole risk is its callees*. The four call sites now
   spell the literal each, which is what produced the ENTERED rows above.

2. **`--namespace UI` cannot see an unexported callee at all.** Both entrants are
   filed under the pseudo-namespace `internal`, because an address with no export
   name has no namespace the tool can derive. **So a per-namespace census
   systematically under-reports a port's residue, in the direction that looks
   good**: the UI view reads "2 left, 0 entered" and the true picture is "2 left,
   2 entered". Any lane reporting a namespace census as its cutover number is
   reporting a lower bound and should say so. This is the mechanism, not a count:
   it applies to every namespace and every unexported callee.

---

## 8. `runtime/types/ui/Dialog.h` — the backwards fields, FIXED

The header named `DialogControllerConfigEntry`'s two `ExternalPtr` fields the wrong
way round, and its own warning comment carried the error ("the config ExternalPtr
at +0x08 is passed AFTER the element-description at +0x10"). Lane UI3 found it,
worked around it at its call site, and left the header alone. It is fixed here,
because a knowingly-wrong header is a trap set for the next lane, and because the
existing pins make the fix checkable rather than hopeful.

The shipped bytes settle it:

```
180114a13  MOV RAX,[R9+0x08] ; MOV [RSP+0x20],RAX   -> arg 5
180114a1c  MOV R9,[R9+0x10]                          -> arg 4
```

and the callee's mangled name gives arg 4 = `ExternalPtr<DialogControllerConfig>`,
arg 5 = `ExternalPtr<ElementDescription>`. **`+0x08` is the DESCRIPTION, `+0x10` is
the CONFIG.** Both are read as 8-byte VALUES, which is also what makes
`ExternalPtr<T>` register-passed here.

Changed: the two field names swapped, two `offsetof` static_asserts added, the
rationalising comment replaced by the disassembly, UI3's call site un-crossed, and
UI3's three pins re-pointed at the now-correctly-named fields. **The pins are the
control**: they decode the offsets out of the shipped stream, they passed
`divergences=0` after the swap, and they would fail loudly if anyone re-crossed
either side. Only two files reference these fields (`Dialog.h`, `dialog_create.cpp`)
— checked by grep, not assumed.

`Dialog.h` also gains `_unknown80` (+0x80) and a note that the ctor's extent is a
lower bound, not a size.

---

## 9. Registration and switch

`RtUiDialogCtorDtorRegister()` in `runtime/src/ui/dialog_ctor_dtor.cpp`, called from
`runtime/src/ui/rt_ui_port.cpp` behind **`BANISHED_RT_DLGCTOR`** (unset = ON, `=0`
kills it — verified: the run goes silent and boots clean).

Both rows under **both doors**, `RegisterRuntimeExport(<literal>)` and
`RegisterRuntimeAddr(<address>)`, because both are live in this tree and a body
registered under one is dead code under the other
(`reference_count_the_doors_not_the_calls.md`). Every mangled name is its own
string literal, never a named constant
(`reference_census_cannot_see_nonliteral_registrations.md`).

`tools/dispatch_slot_map.py --check`: 62 registrations, **0 mismatches, 0 doubles**.

---

## 10. OWED WORK, stated plainly

1. **`0x180039360` and `0x180039280` are unported and our `~Dialog` calls them on
   every invocation.** They are `~Array<T>` with an element-destroy pass
   (helpers `0x180039641` / `0x180039581`), not the J1 family, and they need real
   bodies — not two more `J1_ENTRY` lines. This is the whole of this port's cutover
   residue.
2. **The two dead base-vtable stamps in the ctor are verified only by
   transcription and by the pin screen**, never dynamically — §5b control 4 proves
   no post-state oracle can reach them.
3. **`DialogTempCtor` in `runtime/src/ui/dialog.cpp` is now a duplicate** of a body
   this tree exports (§6). Lane UIF's call.
4. **A namespace census is a lower bound on residue** (§7a.2). Not this lane's tool
   to fix, but every lane reading one should know.

---

## 11. Files

| file | change |
|---|---|
| `runtime/src/ui/dialog_ctor_dtor.cpp` | NEW — both bodies, the differential, the pins, four negative controls |
| `runtime/src/ui/rt_ui_port.cpp` | register the lane behind `BANISHED_RT_DLGCTOR` |
| `runtime/src/ui/dialog_create.cpp` | un-cross the AddController6 arguments, re-point three pins, drive the DLG2 probe from `ProbeGate` |
| `runtime/types/ui/Dialog.h` | swap the two backwards fields + asserts, add `_unknown80`, record the `+0x44` hole |
| `tools/dlg2_run.sh`, `tools/dlg2_matrix.sh`, `tools/dlg2_final.sh` | the run harness, archived beside the numbers (`reference_same_tool_both_sides_is_not_enough.md`) |
