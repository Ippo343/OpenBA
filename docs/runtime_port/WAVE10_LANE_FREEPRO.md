# Wave 10 — Lane FREEPRO — is `InstallDetour`'s `ADD RAX,8` wrong for the allocator?

Worktree `C:/code/GIT/untitled_rev_eng_project/.claude/worktrees/agent-af3fd5c6d37172c3f`,
branch `wave10/lane-freepro` (off `wave10/base` @ `df37677`).

Lane UI1 handed over an unanswered question: it routed
`?Update@UserInterface@UI@@` to the **byte-copy** installer because
`InstallDetour`'s automatic `ADD RAX,8` would have corrupted RBX, and noted that
`standalone/rt_alloc_swap.cpp` detours `MemoryAllocator::Free` (0x1800c6c00) —
same `MOV RAX,RSP` — through the **correcting** path, untested.

**Answer, in one line: the `ADD RAX,8` is INERT for both allocator detours, so
no armed run has ever been corrupting — but the correction is WRONG IN EVERY
CONFIGURATION, and it was live and corrupting on a third subject in the very
same file.** Both facts are measured, not argued.

---

## 1. What the correction actually is, and where it can reach

`InstallDetour` never writes the correction into the patched function. It writes
it into the **trampoline copy** of the displaced prologue
(`standalone/runtime_detour.cpp`, both the blind-copy path and
`InstallDetourRelocating`). A trampoline that is never called is dead code, so
the correction can only matter for a subject whose driver actually calls
`info->trampoline`.

That makes the question two independent screens, and **a subject is only at risk
if it fails both**:

1. does the displaced prologue contain `MOV RAX,RSP` / `MOV R11,RSP`?
2. does the driver call the trampoline?

### The sweep

Prologue bytes read out of the shipped `Runtime-x64.dll` with `pefile` and
disassembled with `objdump` — not from Ghidra's C, which drops dereferences and
misprints MOVSXD (`reference_ghidra_c_output_vs_disassembly.md`). Trampoline use
established by grepping each driver for `.trampoline`, not from its comments.

| subject | addr | displaced | RSP capture? | trampoline called? | at risk |
|---|---|---|---|---|---|
| `ElementDescription::Serialize` | 0x180116b00 | 12 | no | yes | no |
| `ExternalFactory::ExpandRelativePath` | 0x1800bd360 | 12 | no | yes | no |
| `ExternalFactory::Find` | 0x1800be0e0 | 12 | no | yes | no |
| `MemoryAllocator::Allocate` | 0x1800c6e60 | 13 | **no** | no | no |
| `MemoryAllocator::Free` | 0x1800c6c00 | 12 | **yes**, at +9 | **no** | **no** |
| `Engine::Startup` | 0x1800db8a0 | 19 | **yes**, at +0 | **yes** | **YES** |
| `Audio::Device::Stop` | 0x1800de200 | 12 | no | yes | no |
| `System::Random::Rand` | 0x1800c8040 | 12 | no | yes | no |
| `TabElement::UpdateContainers` | 0x1800d9710 | 12 | no | yes | no |
| `UserInterface::Update` | 0x1801213c0 | 12 | **yes**, at +0 | yes | routed to byte-copy by UI1 |

Only three subjects have an RSP capture at all. Two of them are safe for
different reasons, and the third — which nobody had looked at — was live.

---

## 2. The two allocator detours: why the correction is inert

### `Allocate` (0x1800c6e60) — the correction never even fires

```
1800c6e60:  40 53              rex push rbx
1800c6e62:  57                 push   rdi
1800c6e63:  41 54              push   r12
1800c6e65:  41 56              push   r14
1800c6e67:  41 57              push   r15
1800c6e69:  48 83 ec 20        sub    rsp,0x20        <- exactly 13 bytes
```

There is no `MOV RAX,RSP` anywhere in the displaced 13 bytes, and no PC-relative
byte either (the RIP-relative `MOV RAX,[rip+0x13848c]` sits at +13, just past the
boundary), so this takes the blind-copy path and no correction is inserted.
`rt_alloc_swap.cpp`'s comment calling this an RSP-capture prologue is wrong; the
bytes it lists are right.

### `Free` (0x1800c6c00) — the correction fires, into a trampoline nobody calls

```
1800c6c00:  48 85 d2           test   rdx,rdx
1800c6c03:  0f 84 4f 02 00 00  je     0x1800c6e58     <- Jcc rel32
1800c6c09:  48 8b c4           mov    rax,rsp         <- exactly 12 bytes
```

The `Jcc rel32` makes `HasRelativeInstructions` true, so `Free` goes through
`InstallDetourRelocating` — which inserts the same `ADD RAX,8`. A live debug log
shows it doing exactly that:

```
[RtDetour] Detected relative instructions at 0x7ffca06e6c00, using relocating installer
[RtDetour] RSP correction: inserted ADD RAX,8
[RtDetour] Hook installed: 12 prologue bytes -> 39 trampoline bytes
```

And the hazard is real in principle — `Free` has precisely the save/restore
asymmetry UI1 described:

```
save     1800c6c12  mov QWORD PTR [rax+0x8],rbx     ; RAX-relative
restore  1800c6e4d  mov rbx,QWORD PTR [rsp+0x50]    ; RSP-relative
```

With RAX = E (entry RSP), and RSP = E−0x48 at the epilogue (`PUSH R14` then
`SUB RSP,0x40`), `[RSP+0x50]` and `[RAX+8]` name the same address E+8. Shifting
only RAX by 8 desynchronises them by exactly one slot.

**But it never runs.** `rt_alloc_swap.cpp`'s `SwapFree` and `SwapAllocate`
*replace* retail's body; neither reads `g_freeDetour.trampoline` or
`g_allocDetour.trampoline`. The only `.trampoline` read in the whole file is
`g_startupDetour.trampoline`. Retail's `MOV RAX,RSP` never executes on the
detoured path, retail's epilogue never executes, and RBX is preserved by our own
`SwapFree` under the ordinary ABI.

So: for `Free` the correction is **inert — neither correct nor corrupting, just
dead bytes in an unreachable trampoline.** The honest verdict for the armed
`BANISHED_RT_ALLOC=1` path is *not* "the correction is right here"; it is "the
question does not arise here."

---

## 3. What the green redzone stress run can and cannot see

Lane ALLOC's armed run (3,000 frames, 802 citizens, 23,705 entities,
`allocs=901685 frees=540750`, zero redzone violations) is real evidence about
heap integrity. It is **not** evidence about this question, and it would have
read exactly the same if the answer had been the opposite.

Ask the field the question it must be able to answer differently
(`reference_diagnostic_field_cannot_express_the_answer.md`): a redzone violation
is raised when a *heap block's guard bytes* are overwritten. A rotated callee-saved
register is a **register** event in a **stack** frame. For it to become a redzone
violation, the corrupted register would have to survive into a pointer used for a
heap write that lands on a guard — several contingent steps, none guaranteed.
The corruption's own footprint (the last displaced store landing 8 bytes past the
caller's 32-byte home space) is on the **stack**, which the redzone allocator
does not watch at all.

So `redzone=0` over 900k allocations cannot distinguish "no register corruption"
from "register corruption that did not happen to reach a guard byte". It is a
green run from a different instrument. The register-level answer needs a
register-level instrument, which is §5.

---

## 4. The real defect: `Engine::Startup` (0x1800db8a0)

Same file, same installer, and here the trampoline **is** called —
`ShadowStartup` does `orig(selfv, name, minMB, maxMB, noConfigure)`.

```
prologue (19 bytes displaced)
1800db8a0:  48 8b c4           mov  rax,rsp          RAX = E
1800db8a3:  55                 push rbp                   RSP = E-8
1800db8a4:  48 8b ec           mov  rbp,rsp
1800db8a7:  48 83 ec 60        sub  rsp,0x60              RSP = E-0x68
1800db8ab:  48 c7 45 c0 fe..   mov  QWORD PTR [rbp-0x40],-2
saves (RAX-relative)
1800db8b3:  48 89 58 10        mov  QWORD PTR [rax+0x10],rbx    -> E+0x10
1800db8b7:  48 89 70 18        mov  QWORD PTR [rax+0x18],rsi    -> E+0x18
1800db8bb:  48 89 78 20        mov  QWORD PTR [rax+0x20],rdi    -> E+0x20
epilogue (RSP-derived)
1800dbea2:  4c 8d 5c 24 60     lea  r11,[rsp+0x60]              R11 = E-8
1800dbea7:  49 8b 5b 18        mov  rbx,QWORD PTR [r11+0x18]    <- E+0x10
1800dbeab:  49 8b 73 20        mov  rsi,QWORD PTR [r11+0x20]    <- E+0x18
1800dbeaf:  49 8b 7b 28        mov  rdi,QWORD PTR [r11+0x28]    <- E+0x20
```

Function extent taken from the PE exception directory
(`RUNTIME_FUNCTION begin=0x1800db8a0 end=0x1800dbeb8`), so the epilogue is the
real one and not a mis-sliced window.

Saves through RAX, restores through RSP. Add 8 to RAX only and the three saves
move up one slot while the three restores do not: RBX comes back from a slot
nothing wrote, RSI receives RBX's value, RDI receives RSI's — and the last save
lands at E+0x28, which is **past** the 32-byte home space (E+8..E+0x27) the
caller owns, i.e. a stray 8-byte write into the wrapper's own frame.

Gated behind `BANISHED_RT_DIFF` + `BANISHED_RT_ALLOC_SHADOW` (and it refuses to
arm when `BANISHED_RT_ALLOC=1`), so it is *not* on the armed allocator path — but
it is the path lane E2/E3 took their shadow measurements on.

---

## 5. Why the correction is wrong everywhere, and the control that proves it

The comment justifying it reads "the CALL into the trampoline shifts RSP by 8, so
the captured value is 8 low". Follow both frames:

```
retail      caller RSP = C at the CALL; the CALL pushes the return address, so
            entry RSP = E = C-8, MOV RAX,RSP gives RAX = E, and the caller's
            32-byte home space is at [E+8].
trampoline  our wrapper's RSP = C' at ITS call; entry RSP = R = C'-8,
            MOV RAX,RSP gives RAX = R, and OUR home space is at [R+8].
```

Same shape, with E := R. Nothing is shifted, so nothing needs correcting. A
JMP-reached trampoline (`InstallDetourRSPSafe`) needs no correction either,
because there RSP is caller-level already — and that installer correctly inserts
nothing. **There is no configuration in which the +8 was right.**

### The instrument

`standalone/rt_selftest.cpp` gains **Test 2b**. Test 2 could not answer this:
its subject `Random::Rand` has prologue `40 53 48 83 EC 20 …` with no RSP
capture, so the correction never fires on it and Test 2 passes either way — a
field structurally incapable of expressing the answer.

Test 2b's subject is **synthetic** (Engine::Startup runs once, at boot, and boots
the whole engine; calling it twice is not a test). Its bytes reproduce the one
property that matters: nonvolatiles saved through the captured RAX and restored
through an RSP-derived base. The harness loads RBX/RSI/RDI with sentinels, CALLs
through the patched address, and reads the registers back. It carries its own
baseline — the undetoured subject must preserve them first, or the cell reports
"concludes nothing" rather than blaming the installer.

### Results — the discriminating before/after

Taken with a standalone micro-harness linking only `runtime_detour.cpp` + hde64
(no game, no shared game directory), on two subjects because the installer has
two copy paths and the allocator's two targets take different ones:

| subject | path | installer as merged (`6d8f44a`) | after the fix |
|---|---|---|---|
| A — `Engine::Startup`-shaped, 19-byte prologue | blind copy | **CORRUPTING** | **CLEAN** |
| B — `MemoryAllocator::Free`-shaped, Jcc at +3 | relocating | **CORRUPTING** | **CLEAN** |

Subject A pre-fix, with the predicted signature spelled out by the harness:

```
undetoured  rbx=1111111111111111 rsi=2222222222222222 rdi=3333333333333333 rax=42
[RtDetour] RSP correction: inserted ADD RAX,8 at trampoline offset 3
detoured    rbx=00007ffcf77b8556 rsi=1111111111111111 rdi=2222222222222222 rax=42
RESULT: CORRUPTING -- callee-saved registers did not survive
  mechanism: shifted by exactly one 8-byte slot (rsi holds rbx's value, rdi holds rsi's)
```

Both subjects come back CLEAN after the change, and both restore byte-for-byte on
`RemoveDetour`.

### The fix

`standalone/runtime_detour.cpp`: the `ADD RAX,8` / `ADD R11,8` insertion is
removed from **both** copy paths and replaced with the frame argument above. The
`Bug Class 3` header note and `runtime_detour.h`'s contract are corrected to say
the installer reproduces an RSP capture verbatim.

Blast radius is exactly the three subjects in §1's table: `Free` (inert either
way), `UserInterface::Update` (already on the byte-copy installer, untouched),
and `Engine::Startup` (fixed). Nothing else in the tree ever had a correction
inserted, so nothing else changes.

---

## 6. The rule, so nobody re-derives it

**An RSP capture in the prologue is no longer a reason to choose one installer
over another.** All of them now reproduce `MOV RAX,RSP` / `MOV R11,RSP`
verbatim, which is correct whether the trampoline is entered by CALL or by JMP.
Choose on these grounds instead:

| use | when |
|---|---|
| `InstallDetour` / `InstallRuntimeDetour*` | the default. The **only** option when a displaced byte is PC-relative (CALL/JMP/Jcc rel, RIP-relative ModR/M) — it is the only one that relocates. |
| `RuntimeDetourInstall` (byte-copy) | you have read the prologue and want the strictest guard: verbatim copy, nothing inserted, refused on any byte mismatch. Requires no PC-relative displaced byte. |
| `InstallDetourRSPSafe` | your detour tail-JMPs into the trampoline instead of CALLing it. |
| `InstallDetourVTableAt` | the prologue cannot be displaced at all (SEH-cookie family, Bug Class 8). |

Two things that are still worth checking by hand for every new detour, because
no installer can check them for you:

* **`prologueSize` must land on an instruction boundary.** Count it in the
  disassembly. Half an instruction left behind kills the process somewhere else
  entirely.
* **Whether your driver calls the trampoline at all.** A driver that *replaces*
  the body (like `rt_alloc_swap.cpp`) makes every property of the trampoline
  irrelevant; a driver that *wraps* makes all of them load-bearing. Read the
  driver, not its comments — `rt_alloc_swap.cpp`'s own header describes
  `Allocate` as an RSP-capture prologue and it is not one.

And the generalisable one, which is what actually cost the time here:

> **A correction applied to a copy that nobody executes is not "safe", it is
> unmeasured.** The two allocator detours looked identical to the dangerous case
> on every screen except the one that mattered — does anything call the
> trampoline — and that screen also happened to be the one that exonerated them.
> Separate "is this transformation correct?" from "can this transformation be
> reached?" and answer both; a subject can pass for the second reason while the
> transformation is wrong, and the next subject will not be so lucky.

---

## 7. Corrections to other files' comments

* `standalone/rt_alloc_swap.cpp` header lists `Allocate 0x1800c6e60` under
  "PROLOGUES" correctly but the surrounding prose treats both allocator targets
  as RSP-capture prologues. Only `Free` is, and its capture is at **+9**, not at
  the start.
* `standalone/runtime_detour.cpp`'s Bug Class 3 note said the RSP-safe variant
  is "what `allocator_lock.cpp` does for `MemoryAllocator::Free` by hand".
  `allocator_lock.cpp` does hand-roll a Free trampoline, but it is reached by an
  ordinary CALL from `FreeLocked`, and it reproduces `MOV RAX,RSP` **verbatim**
  with no `+8` — i.e. it was always doing the thing this lane just made
  `InstallDetour` do. That hand-rolled code was the counter-example to the
  correction sitting in the tree the whole time.

---

## 8. Runs

All in-game cells ran through `tools/game_dir_lock.sh run lane-FREEPRO ... --
tools/freepro_cell.sh`, which re-`verify`s ownership and **md5s the deployed exe
against our own build immediately before launch**, refusing to run on a mismatch.
Both cells reported `md5 built = md5 deployed = bf3ea3d634c6b3a266c6dc56f56324b9`
and ownership still held after the run, so neither is measuring another lane's
binary.

| # | purpose | result |
|---|---|---|
| A | micro-harness, installer as merged (`6d8f44a`), subjects A and B | **CORRUPTING** on both |
| B | micro-harness, installer after the fix, subjects A and B | **CLEAN** on both |
| C | full debug exe, `BANISHED_RT_SELFTEST=1` + armed `BANISHED_RT_ALLOC=1`, boot to menu | Test 2b **pass** in situ; swap installed; exit 0, empty crash log |
| D | full debug exe, armed `BANISHED_RT_ALLOC=1`, autostart into gameplay, pause-menu quit at frame 721 | world built, played 721 frames, exit 0, empty crash log |

Cell C's own log confirms the fix is live and that `Free` still takes the
relocating path: `grep -c "RSP correction"` = **0**, while
`[RtDetour] Detected relative instructions at …6c00, using relocating installer`
is still present. Cell C also shows the armed swap taking both detours
(`ATOMIC SWAP INSTALLED`).

Cell C quits from the menu (`BANISHED_AUTOQUIT` clicks Quit after N *menu*
frames), so it is a boot-and-menu exercise, not a gameplay one — cell D is the
gameplay half, and both are needed: C is where Test 2b's verdict is read, D is
where the armed allocator actually carries a world.

**What these cells do NOT establish.** They are functional checks, not a
re-measurement of lane ALLOC's stress town, and they cannot show the installer
change improved anything on the armed path — by §2 that path never called the
affected trampolines, so cells C and D are expected to be indistinguishable from
the same runs before the fix. The evidence that the fix *does* something is cell
A vs cell B, at register level. The evidence that it *breaks* nothing is C and D.
`Engine::Startup`'s shadow driver (`BANISHED_RT_ALLOC_SHADOW`) is the path that
was actually corrupting and it is **not re-measured here** — its previous
measurements (lane E2/E3) were taken with three rotated callee-saved registers
and should be re-read before being trusted.
