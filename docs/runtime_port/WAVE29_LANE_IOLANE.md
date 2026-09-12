# WAVE 29 LANE IOLANE — IO's last four bodies and its two minted vtables

Branch `wave29/iolane`, cut from `wave29/base` at `c11511a1`.
Subject: IO's 4 BODY gap rows, IO's 2 MINTED vtable rows.

---

## ⚠ POST-REPORT CORRECTION — READ THIS FIRST

**I EDITED AFTER REPORTING. Kickoff warning 18 says to say so loudly; this is
that.** The edit is **DOCUMENTATION ONLY** — `WAVE29_LANE_IOLANE.md` and
`wave29_iolane_retslot_screen.py.txt`. **No file under `runtime/`, `standalone/`,
`code/`, `engine/` or `dx11/` was touched**, so the integrator's merge-time
`git diff --stat <base>...<branch> -- runtime/ standalone/ code/ engine/ dx11/`
is byte-identical to what it was at `ea8a2839`, and no measurement in this
document changes.

**WHAT WAS WRONG.** In §7.1 I wrote that AVBODY's typedef needs "the slot first",
and I sent the same prescription to AVBODY:

```
    WRONG:  using FnGetName = String* (*)(String* retSlot, const ShaderProgram* self, int index);
```

**Lane AVBODY refuted it, and they are right.** That is the FREE-FUNCTION rule.
For a **member** function returning a class by value, MSVC x64 leaves `this` in
RCX and puts the return pointer in RDX:

```
    RIGHT:  using FnGetName = String* (*)(const void* self, String* ret, int32_t index);
            // RCX = this, RDX = hidden return slot, R8D = index
```

I re-read `0x18004e4b0` myself rather than taking the correction on trust, and
the bytes settle it with an asymmetry that cannot be faked — an **object is
dereferenced**, a **slot is stored through**:

```
  18004e4cc:  shl  $0x4,%rax          ; R8D is the index, scaled by 16
  18004e4d0:  add  0x30(%rcx),%rax    ; RCX DEREFERENCED at +0x30  => RCX is `this`
  18004e4d4:  mov  %r9,(%rdx)         ; RDX STORED THROUGH         => RDX is the slot
  18004e4ec:  mov  %rdx,%rax          ; the slot is what comes back
```

**My prescription was wrong IN THE WILD-WRITE DIRECTION** — it would have put
`self` in RDX and the caller's `ret` in RCX, so the body would dereference the
caller's String slot as a `ShaderProgram` at +0x30 and write 8 bytes through
`self`. That is precisely the defect the screen exists to prevent. AVBODY also
notes independent agreement from Wave 28 lane GFXBODY's already-landed
`Material::GetConstantName` forwarder, which has used the correct order since
last wave.

**WHY MY CONTROLS COULD NOT CATCH IT — AVBODY's diagnosis, and it is the
transferable part.** M1 and M2 both answer *"IS there a hidden slot?"*. Neither
answers *"WHERE does it sit?"*, and my known-positive/known-negative pair only
exercises the first question. **A control that scores a binary present/absent
verdict cannot validate an ORDERING claim derived beside it.** The ordering rode
along on the screen's credibility — a measured number lending cover to an
asserted one, which is a named pattern in this project's ledger and which I
walked straight into while writing a message telling AVBODY to control their own
screen.

**FIXED, by adding the third column AVBODY asked for.** M3 reads, from the
bytes, which register is **dereferenced** (→ `this`) and which is **stored
through** (→ the slot). Re-run:

```
M1     M2     extent  M3 where           symbol
BYVAL  BYVAL  0x29    this=?   slot=rdx  ?GetSource@Stream@IO@@…     [M1/M2 positive; SLOT HALF of M3 only]
BYVAL  BYVAL  0x38    this=rcx slot=rdx  ?GetName@PackageFile@IO@@…  [M3 positive, BOTH halves]
ref    ref    0x5     n/a (no slot)      ?GetObjectName@TextFileStream@IO@@…  [negative]
ref    ref    0x5     n/a (no slot)      ?GetObjectType@TextFileStream@IO@@…
ref    ref    0x4     n/a (no slot)      ?GetObjectVersion@TextFileStream@IO@@…
BYVAL  BYVAL  0x44    this=rcx slot=rdx  ?GetConstantName@ShaderProgram@Video@@…  va=0x18004e4b0
BYVAL  BYVAL  0x44    this=rcx slot=rdx  ?GetTextureName@ShaderProgram@Video@@…   va=0x18004e420
```

**AND I ALMOST REPEATED THE MISTAKE INSIDE THE FIX.** M3's first run scored my
existing known positive as `this=? slot=rdx`, and I nearly read that as a pass.
It is not: `?GetSource@Stream@IO@@` is the **abstract base** — it writes a null
String into the slot and **never touches RCX at all** (verified at
`0x18000c6a0`), so `this=?` is a *true statement about that body* and that row
**cannot exercise M3's `this` half**. A control that cannot reach half the
mechanism it scores is the union-control defect again. I added
`?GetName@PackageFile@IO@@` (`0x180025310`, `mov 0x40(%rcx),%rax` **and**
`mov %rax,(%rdx)`) as M3's own positive, which reaches both halves and reads
`this=rcx slot=rdx`.

**None of this touches my own four rows**: they are `AEBV`/`H`, have no hidden
slot, and M3 correctly prints `n/a (no slot)` for all three. The error was
entirely in advice I gave about a *partner's* subject.

**Scored honestly: the pairing instruction worked exactly as the kickoff says it
does. This defect was found by my partner and by nobody on my side, and my own
controls were structurally incapable of finding it.**

---

## 0. HEADLINE

**Three of four bodies ported, registered and ORACLE-verified against retail on a
live route. One of the two minted vtables PROMOTED, with a two-arm attendance
warrant and a working A/B kill switch. The other NOT promoted, on measured
grounds that are not about its readiness. `?SetupFileSystem@Modification@IO@@`
DECLINED as a size judgement, said as a size judgement.**

| row | kind | result |
|---|---|---|
| `?GetObjectName@TextFileStream@IO@@` | BODY | **PORTED**, oracle `calls=3 divergences=0` |
| `?GetObjectType@TextFileStream@IO@@` | BODY | **PORTED**, oracle `calls=3 divergences=0` |
| `?GetObjectVersion@TextFileStream@IO@@` | BODY | **PORTED**, oracle `calls=2 divergences=0` |
| `?SetupFileSystem@Modification@IO@@` | BODY | **NOT PORTED** — size judgement, §6 |
| `??_7PackageFile@IO@@6B@` | VTABLE | **PROMOTED**, §4 |
| `??_7TextFileStream@IO@@6B@` | VTABLE | **NOT PROMOTED**, §5 — stampers and attendance, not readiness |

**The most useful thing this lane produced is not in that table.** It is §4.4:
a claim I wrote into a promotion warrant with the words *"checked rather than
assumed"* was **assumed, and wrong**, and a `printf` that ran regardless of what
I believed caught it inside one run. The mechanism it exposed — **retail can
re-stamp ITS vtable over an object we built, through an in-DLL rel32 tail jump no
registration can intercept** — is the mirror image of the rule this project
already has, and it means a promotion has a *lifetime* and not only a *route*.

---

## 1. THE BOARD, RE-DERIVED BY ME ON THIS TIP

`python tools/runtime_census.py` (plain `python`, 3.12.10; the venv does not
exist in a worktree).

| | called | ported | % | IO gap | decomposition |
|---|---|---|---|---|---|
| `wave29/base` `c11511a1` | 1453 | 1294 | 89.1% | 12 | 4 BODY + 4 VTABLE + 4 DATA |
| after the three accessors | 1453 | 1297 | 89.3% | 9 | 1 BODY + 4 VTABLE + 4 DATA |
| **this branch, final** | **1459** | **1304** | **89.4%** | **8** | **1 BODY + 3 VTABLE + 4 DATA** |

IO itself: **234 called / 222 ported / 94.9% → 239 / 231 / 96.7%**, ceiling 97.9%
unchanged. Whole-board BODY gap **24 → 21**; VTABLE gap **38 → 37**.

**Re-derived and REPRODUCED the kickoff exactly** at base: `--unported
--namespace IO` prints the same four BODY rows and the same 4 + 4 + 4
decomposition §1.2 claims. Kickoff warning 1 discharged with no correction to
report on the numbers.

**DO NOT SCORE THIS LANE ON 89.1 → 89.4.** `called` grew by 6 *because of* the
promotion (a promoted vtable folds its slot rows into the population), so the
ratio moved for two unrelated reasons at once. The identity I ask to be scored on
is the one in §0's table: **rows CLOSED, each with the evidence that closed it**.

---

## 2. EVERY STATED BLOCKER ON MY ROWS, RE-CHECKED FIRST (kickoff warning 3)

Four were handed to me. **Three had expired. One had not, and it is the one
everybody would have guessed was softest.**

| blocker, as handed to me | verdict |
|---|---|
| `SetupFileSystem` is "NOT VERIFIABLE — the only unattended mod route is the broken `ModDialog_ToggleMod_Impl+0x43` one" (WAVE27_CLOSED §5) | **EXPIRED.** Wave 28 lane MISC measured it false; I **reproduced** the proof on my own build: `[RtResidue] 1 ... ?SetupFileSystem@Modification@IO@@QEAAXXZ` from a `BANISHED_RT_STRICT=1` autostart run with no mods env var of any kind. MISSING-CAPABILITY class. |
| `??_7PackageFile@IO@@6B@` is BLOCKED — "slots=9 ported=5", four traps (`wave19_phase0c/run2_attend/exit.tbl:39`) | **EXPIRED.** Waves 23/25/27 closed all four. Census prints 9/9 COMPLETE; `PromoteVtableGuarded` re-asks at runtime and passed. MISSING-CAPABILITY class. |
| `??_7TextFileStream@IO@@6B@` is 8/10, slots 2 and 8 deferred on size (WAVE27 §6) | **EXPIRED.** Wave 28 lane SLOTS closed both. 10/10 COMPLETE. MISSING-CAPABILITY class. |
| The three `TextFileStream` accessors are "not worth porting — their only caller is a probe" (WAVE27 §5) | **STILL TRUE, and I confirmed it rather than overturned it.** See §3.3. INTRINSIC class as long as no non-probe caller exists. |

**Three for three on the capability/intrinsic tell, and the fourth is the
interesting one:** the blocker that survived is the one phrased as a judgement
about VALUE, not about capability. That is a third category the tell does not
name, and it is the category a "cheap wins" list is made of.

---

## 3. THE THREE BODIES

### 3.1 What they are, read from the shipped bytes

`objdump` on `C:\GOG Games\Banished\Runtime-x64.dll`, not from Ghidra's C
(which drops dereferences and prints MOVSXD as a zero extension):

```
1800ba250:  8b 41 50        mov    0x50(%rcx),%eax    ?GetObjectVersion@  -> int
1800ba253:  c3              ret
1800ba260:  48 8d 41 60     lea    0x60(%rcx),%rax    ?GetObjectType@     -> &+0x60
1800ba264:  c3              ret
1800ba270:  48 8d 41 58     lea    0x58(%rcx),%rax    ?GetObjectName@     -> &+0x58
1800ba274:  c3              ret
```

Extent **DERIVED** from the instruction stream (first `int3` after the `ret`):
4, 5 and 5 bytes. **None of the three has a `.pdata` entry** — leaf, no prologue
— so every `.pdata`-driven instrument in `tools/` is blind to all three. That is
the *other* direction of this wave's `.pdata` finding: SLOTS measured a 70.3x
UNDERSTATEMENT; here the annotation does not exist at all, and a reader who takes
that silence for "no such function" concludes the slot is empty.

`RETAIL SHIPS EMPTY BODIES` checked, not assumed (warning 5): an empty body is
`ret` or `xor eax,eax ; ret`. All three dereference `this` at a named offset and
two take its address. Not empty.

### 3.2 THE BRIEF'S HEADLINE TRAP IS NOT PRESENT, AND THE BRIEF MISCOUNTED

My brief said *"three of your four bodies return `const String&`"* and warned at
length about the hidden RDX return slot. Measured:

* only **two** of the four return a `String` at all — `?GetObjectVersion@` is
  `…QEBAHXZ`, i.e. `H`, an **int**;
* both String returns are **`AEBV`**, reference-to-const, returned in RAX like
  any pointer. `?AV` is the by-value spelling and is the one that carries the
  hidden slot.

So **no hidden return slot exists on any of my four rows** and the correct
signature takes exactly one argument. Confirmed by a **second, name-independent**
mechanism: neither body references RDX at all, and a by-value body cannot avoid
it. Full screen and both controls: §7.1.

### 3.3 THE ORACLE, and what it does NOT establish

`RT_DIFF_SCALAR(ours, RtOriginal<…>(name)(self))` in each body. `RtOriginal` is
`ResolveExportRaw` = GetProcAddress against the shipped DLL, which never consults
the port registry — so this compares our body with **retail's**, not with itself,
and stays an oracle after registration, which is exactly when screens of this
shape normally go dark. It costs a GetProcAddress per call and that is only
acceptable because `RT_DIFF_SCALAR` evaluates nothing unless `BANISHED_RT_DIFF=1`
(the Wave 2 5.6-point perf breach was this call *outside* a diff guard).

Measured, `BANISHED_AUTOSTART=1 … BANISHED_RT_DIFF=1`, clean self-quit,
`standalone_crash.log` 0 bytes:

```
[RtDiff]   IO::TextFileStream::GetObjectName     calls=3  entered=3  class=ORACLE  divergences=0
[RtDiff]   IO::TextFileStream::GetObjectType     calls=3  entered=3  class=ORACLE  divergences=0
[RtDiff]   IO::TextFileStream::GetObjectVersion  calls=2  entered=2  class=ORACLE  divergences=0
```

**READ THE `calls=` COLUMN AND THEN READ THIS PARAGRAPH.** All eight of those
calls come from my own probe screen and **not one comes from the game**. I
counted them against the screen's source and they match exactly (3 + 3 + 2).
So the honest statement is:

> The three bodies are **byte-faithful and oracle-verified**. They have **ZERO
> callers on any game route.** The census's own `GAP(instrument)` tag — *"a
> probe's resolve is a real dependency and NOT a port obligation… named here so
> no lane is sent after one as body work"* — was **RIGHT**, and Wave 27 lane
> IOTEXT's decision not to port them was **RIGHT** for the reason it gave.

I ported them because my brief scoped them, they are four bytes each, and they
remove three rows that will otherwise be rediscovered every wave. **I am not
claiming they satisfy a dependency.** They move a number. Said plainly rather
than dressed up, because this project's record is that the item a handover calls
"safest" is often safest only because it is pointless.

### 3.4 Screen D — a real ours-vs-theirs comparison, with its own control

Added to `runtime/src/io/file_stream_probe.cpp`, which already had a retail side
(`RtResolveExportForProbe` → `ResolveExportRaw`). Ours side is a **direct call on
the C symbol**, the one spelling that cannot silently become retail. It compares
the **returned POINTER**, not the string: both bodies compute `this + <offset>`,
so equal pointers means the OFFSETS agree, whereas a string comparison would pass
with both offsets wrong in the same direction.

**Control 6** perturbs the OURS side only, and **not inside the ported body** —
substituting a wrong-offset stand-in at the call site proves the comparison is
live without putting a control knob in shipping code. The wrong offset is `+0x60`
for the NAME accessor: the value a lane that believed the field names would have
written, which is the defect `FileStream.h:274-287` exists to warn about.

```
ctl=0   checked=33 failed=0                         PASS
ctl=6   checked=33 failed=1 att=2   control CAUGHT
        FAIL ours GetObjectName() == retail's  got=0x…b70 want=0x…b68   (delta 8 = 0x60-0x58)
```

Exactly **one** check failed, and that is deliberate: the distinctness check
calls the unperturbed body directly, so it correctly stayed green. The control is
scored on the screen it perturbs, **not on a union** (warning 9).

I also widened the summary's `att` range from `1..5` to `1..6` in the same
commit. Had I not, control 6 would have scored `att=0` and printed **"CAUGHT BUT
NOT VIA ITS OWN MECHANISM — NOT BANKED"**: a DEFLATING misreport of a working
control. Recorded because the bound is invisible and nothing points at it.

---

## 4. `??_7PackageFile@IO@@6B@` — PROMOTED

### 4.1 Readiness (necessary, nowhere near sufficient)

9 slots, 9 ours, COMPLETE. `PromoteVtableGuarded` re-asks the trap question at
RUNTIME against the live registry, so this does not rest on the census.
**And the guard's own effectiveness was tested on this row rather than assumed** —
its normal output says the refusal path is `UNTESTED` on every run:

```
$ BANISHED_RT_MINT_TRAP_CONTROL='??_7PackageFile@IO@@6B@' … ./reimpl-x64.exe
[RtMint] TRAP CONTROL: reporting a synthetic trap count of 99 for ??_7PackageFile@IO@@6B@
[RtMint] REFUSING to promote ??_7PackageFile@IO@@6B@: RtMintVtableTrapCount()=99 (expected 0) …
[RtMint] promoted 49 minted DATA row(s)        <- 50 on a normal run
[RtMint] vtable promotion guard: 19 evaluated, 1 refused, 1 synthetic trap injection(s)
```

### 4.2 Attendance — MEASURED IN TWO ARMS, AND THE SECOND ARM IS NOT A FORMALITY

`BANISHED_W17_ATTEND=1`, ordinary autostart, clean self-quit, control
`?_singleton@MainLoop@Engine@@` GREEN (`init=1 post=3`) in both arms:

| row | arm A (probe + RT_DIFF + RT_STRICT) | arm B (nothing else armed) |
|---|---|---|
| `??_7PackageFile@IO@@6B@` | ATTENDED post=1 | **ATTENDED post=1** |
| `??_7TextFileStream@IO@@6B@` | never post=0 | never post=0 |
| `?_typeInfo@PackageFile@IO@@` | ATTENDED post=1 | **never post=0** |

**THE THIRD ROW IS THE FINDING.** `?_typeInfo@PackageFile@IO@@` has sat
ROUTE-UNDECIDED since Wave 26 lane PROMOIO with an explicit written condition:
*"promote this row when ONE run reads it post>0 on a route THE GAME DRIVES"*.
Arm A satisfies that sentence. **Arm B proves arm A's post=1 was manufactured by
my own instrumentation** — precisely the failure `reg_mint_promoted.cpp`'s Wave 26
retraction says reads *indistinguishably from the real thing* in the log. Without
arm B I would have reported that condition satisfied and recommended a promotion
on an instrument measuring itself. **The row is NOT promoted here and the
condition is NOT satisfied.**

`post=1` is NOT "ran once" — consumers cache in a function-local static. The
dispatch evidence is separate and far larger: `IO::PackageFile::PackageFile`
`calls=28 entered=14 worked=14 divergences=0`, and the per-entry accessors
5836–9471 calls each, all `divergences=0`, over 4 real mounted packages.

### 4.3 Stamper ownership — 3 of 4, and the fourth is named

`python tools/mint_stamper_census.py --rows="??_7PackageFile@IO@@6B@"`:

```
STAMPERS 4 ; ours-capable 3 ; retail 1 ; unattributed 0
  ?RefreshList@Modification@IO@@     0x1800af846   OURS
  ??0PackageFile@IO@@QEAA@XZ         0x1800b447f   OURS
  ??1PackageFile@IO@@UEAA@XZ         0x1800b455b   OURS
  ??0PackageFile@IO@@QEAA@AEBV01@@Z  0x18002562c   RETAIL  (copy ctor, UNPORTED)
```

**ROUTE SCOPE, not a bare verdict:** the live population of PackageFiles is
MIXED. One built by retail's copy constructor carries the DLL's table; one built
by any of the other three carries ours. That is not a hazard — both tables hold
the same nine behaviours and all nine are oracle-verified at 0 divergences — but
it is the truthful scope. At cutover the retail copy ctor does not exist, so the
mixture is a DLL-present phenomenon only. `ours-capable` is an **upper bound**
(the tool says so) and is not read here as "every caller uses it".

### 4.4 THE COST — AND THE CLAIM I GOT WRONG

Promotion changes what `PackageFileVtable()` (`package_file.cpp:318`, an
`RT_DATA_SLOT`, registry-first) hands our constructor, so the ctor's 240-byte
`RT_DIFF_BYTES` oracle would go permanently red at the vtable field. I handled
that, and I wrote into the warrant, in these words:

> *"The Destruct and Reconstruct memcmp oracles in the same file are NOT
> affected and this was checked rather than assumed: both build BOTH sides with
> `RtIo_PackageFile_Ctor`."*

**Destruct was unaffected. RECONSTRUCT DIVERGED on the very first promoted run.**

```
[PKG2] Destruct oracle: MATCH (240 bytes compared)
[PKG2] Reconstruct oracle: DIVERGED (240 bytes compared), return-is-this ours=1 theirs=1
```

The mechanism, and it is worth more than the fix: retail's `?Reconstruct@`
destructs in place through the object's OWN vtable slot 0 — with our table
installed that is OUR body, correct — and then **TAIL-JUMPS retail's constructor
at `0x1800b4440` by an in-DLL rel32**. **An in-DLL rel32 is not a door.** No
registration can intercept it. So retail's constructor re-stamps the DLL's table
over an object we built.

Two things generalise, neither about PackageFile:

* `reference_a_ported_row_can_be_permanently_unpromotable.md` is read as "we
  cannot get our table IN". **This is the mirror: retail can put ITS table BACK,
  over an object of ours, at any time, through a call our tree cannot see.**
  A promotion has a **LIFETIME** as well as a route.
* The claim was written as *"checked rather than assumed"* and was assumed. It
  was caught by a `printf` that ran regardless of what I believed — not by
  review, not by care. The warrant now carries the correction in place, naming
  the false sentence, rather than a quiet deletion.

**Consequence for the promotion: none for correctness.** Both tables hold the
same nine verified behaviours, so a re-stamped object is not broken — it simply
stops dispatching OUR bodies. A COVERAGE fact, not a hazard; and at cutover
`0x1800b4440` is our body anyway.

### 4.5 The fix, and why it is not a weakening

Both oracles now excuse a vtable slot **only** if it differs **AND** ours holds
exactly what OUR door returned **AND** theirs holds exactly what the RAW door
returned. **The offset set is DERIVED at run time, never written down** — a
literal `+0x00` would be a second copy of the layout nothing keeps in step, and
PackageFile stamps more than one table (`BaseVtable()`, `ExternalVtable()`). Any
*other* differing slot is still reported, which is the direction that matters: a
window that silently swallows a real difference is the DEFLATING failure.

Measured in both arms of the kill switch — this is the A/B, and it shows the
exclusion does exactly and only what is claimed:

```
default                              PROMOTED   1 slot excused   232/240 compared   MATCH
BANISHED_RT_PROMOTE_PKGFILE=0        on the DLL 0 slots excused  240/240 compared   MATCH
                                     both arms: IO::PackageFile::PackageFile divergences=0
```

For comparison, four already-promoted rows in this tree (`SpriteSheet`,
`AnimationEvent`, `Geometry`, `UpdateLoop`) still print a bare unexplained
`+0x0:` divergence every diff run. PackageFile now prints none and says why.
**ADDRESSEE: §8 R3.**

---

## 5. `??_7TextFileStream@IO@@6B@` — COMPLETE, AND NOT PROMOTED

Not an oversight, and **not a readiness problem**: 10/10 COMPLETE on this tip.
Both halves of the case are measured and neither is about slots.

* **ATTENDANCE `never post=0`, in BOTH arms**, in runs whose control was GREEN
  and in which 40 of 67 subject rows DID read post>0. A real absence, not a dead
  instrument. Reproduces Wave 19 phase-0c exactly.
* **STAMPERS `8 ; ours-capable 3 ; retail 5`** — the weakest ratio of any IO
  vtable. The five in the DLL are `??0TextFileStream@…@AEBV01@@Z`,
  `??0TextFileStream@…@XZ`, and `TextFile`'s `Close` / `Create` / `Open`. Wave 19
  §5.1's criterion puts this at 3/8, where `??_7CompressedFileStream@` at 0/6 drew
  *"promote none"*.

---

## 6. `?SetupFileSystem@Modification@IO@@` — DECLINED, AND THIS IS A SIZE JUDGEMENT

**Stated as a size judgement, not dressed as a blocker.** Its blocker expired and
I say so in §2; it has a live, reproduced route; I still did not write it.

**Extent DERIVED, not taken:** `objdump` to the first `int3` after the final
`ret` (`c3` at `0x1800af5e8`) gives `0x1800aebb0..0x1800af5e9` = **0xA39 = 2617
bytes, 714 instructions**. Wave 28 lane MISC's 0xa38 = 2616 differs by one byte,
an inclusive/exclusive convention, and both describe the same body. **Not a
`.pdata` figure**, so this wave's 70.3x `.pdata` warning does not apply — the
figure is materially the size advertised, which is itself worth recording since
the instruction was to expect otherwise.

**What I add that Wave 28 lane MISC did not have.** MISC enumerated 18 call
targets. I asked a different question — *which of them does our tree already
have?* — and **three are ABSENT**, so porting this body creates at least three
new unsatisfied rows (`reference_porting_can_grow_the_cutover_surface.md`):

| callee | state in our tree |
|---|---|
| `?ForceUnload@ExternalFactory@Object@@QEAAXAEBVFilename@System@@@Z` `0x1800beec0` | **ABSENT — an EXPORT nobody has ported** |
| `0x180027970` | **ABSENT — unexported internal** |
| `0x180027380` | **ABSENT — unexported internal** |
| the other 15 | present; 9 of them carry a `RegisterRuntimeAddr` |

The three reasons I stopped are MISC's, re-read and agreed with rather than
re-invented: (1) `0x1800c6c00` (`MemoryAllocator::Free`) appears **16 times**
(I counted it myself), i.e. roughly half the body is inlined `System::String`
release, where under-release leaks and over-release frees a live object and does
not reproduce on short runs; (2) the inlined AVL walk with an inlined `strcmp` is
the exact shape of `reference_uniform_population_is_not_a_verification.md`;
(3) it tears down and remounts the package filesystem on the live loader path, so
a wrong port is a silently wrong resource mount.

**I had budget. I spent it on the promotion instead**, and I think that was the
better trade because the promotion had a decision in it and this has only volume.
A reader may disagree; the trade is stated so it can be disagreed with.

---

## 7. THE PAIRING (kickoff §2.1)

**Lane AVBODY had not reported when I needed it, so I self-controlled and say so.**
I sent AVBODY two messages: an opening exchange with the `AEBV`/`?AV`
discriminator and a request for their screen plus a known positive, and then the
result below. Nothing had come back by the time I stopped editing.

### 7.1 My screen, run against AVBODY's subject — and its own defect

`docs/runtime_port/wave29_iolane_retslot_screen.py.txt` (preserved beside this
doc as `.txt` rather than added to `tools/`, following the Wave 28 precedent).
Two independent mechanisms, scored separately:
**M1** the mangled return spelling; **M2** does the body's instruction stream
reference RDX at all — name-independent.

```
M1     M2     extent  agree  symbol
BYVAL  BYVAL  0x29    yes    ?GetSource@Stream@IO@@UEBA?AVString@System@@XZ     [KNOWN POSITIVE]
ref    ref    0x5     yes    ?GetObjectName@TextFileStream@IO@@QEBAAEBV…        [KNOWN NEGATIVE]
ref    ref    0x5     yes    ?GetObjectType@TextFileStream@IO@@QEBAAEBV…        [mine]
ref    ref    0x4     yes    ?GetObjectVersion@TextFileStream@IO@@QEBAHXZ       [mine]
BYVAL  BYVAL  0x44    yes    ?GetConstantName@ShaderProgram@Video@@…            [AVBODY]  va=0x18004e4b0
BYVAL  BYVAL  0x44    yes    ?GetTextureName@ShaderProgram@Video@@…             [AVBODY]  va=0x18004e420
```

Result handed to AVBODY: both their rows carry a hidden RDX return slot,
confirmed twice.

> ⚠ **THE NEXT CLAUSE OF THIS PARAGRAPH WAS WRONG AND IS CORRECTED AT THE TOP OF
> THIS DOCUMENT.** It read *"the typedef needs three parameters with the slot
> first"*. It is `(self /*RCX*/, ret /*RDX*/, index /*R8D*/)` — `this` stays in
> RCX for a member function. Refuted by lane AVBODY, re-verified by me from
> `0x18004e4b0`'s bytes, and the screen now carries an M3 column that measures
> the ordering instead of asserting it. The false sentence is left visible rather
> than silently deleted.

I also told AVBODY that 0x44 bytes was "far too big for a bare accessor, so they
construct a String". **AVBODY's caution on that is correct and I accept it**:
~0x20 of the 0x44 is MSVC EH-state bookkeeping (`MOV [RSP+8],-2`,
`MOV [RSP],0`/`1`) with no observable effect, and the real work is 9
instructions. The conclusion was true but **size alone was a weak argument**; the
store through RDX is the strong one. A correct conclusion reached by a weak
argument is still a weak argument, and it is the argument that gets reused.

**THE SCREEN SHIPPED A DEFLATING BUG AND ONLY THE CONTROL FOUND IT.** M1's first
version fell back to `sym[sym.rfind("QEBA"):]`, which for a `UEBA` symbol is
`-1` and therefore inspected the **last character of the name**. It reported the
by-value `?GetSource@Stream@IO@@UEBA?AVString@System@@XZ` as a **reference** —
i.e. "no hidden slot here" when there is one, exactly the wild write the screen
exists to prevent. Every other row it was asked about happened to be right, so
nothing but the known positive could have caught it. The fix carries the defect
in a comment at the fix site.

### 7.2 The other direction — AVBODY reported after I did, and it mattered

At the time I first reported, AVBODY had not published an instrument, so I ran
the **previous wave's** equivalent — the `file_stream_probe.cpp` ctor/vptr
screens already in the tree — against my rows, and added screen D and control 6
to them (§3.4). That found nothing my own screens had not, and I recorded it as a
substitute for a partner's eyes rather than a replacement.

**AVBODY then replied, and refuted the central prescription I had sent them.**
See the correction at the top. The score for the pairing instruction is
unambiguous and it is the kickoff's own claim, reproduced:

* defects my controls found in my work: **2** (§9 rows 1 and 2), both by a
  control firing, neither by review;
* defects **AVBODY** found in my work: **2** — the argument ORDER (wild-write
  direction) and the weak size argument;
* defects **I** found in AVBODY's work: **0**. My screen CONFIRMED their subject
  and my extent and BYVAL figures reproduced theirs independently, which is
  useful, but it is corroboration and not a defect found.

**What they got from me that reproduced**: extent 0x44 on both rows (they derived
the same from inter-function `CC` padding, a different method, and neither is
`.pdata`); BYVAL on both; FACT 1 that the hidden-slot trap is theirs alone; and
my M1 self-reported defect, which they cite.

### 7.3 The String idiom — one half corroborated, one half NOT

I asked AVBODY to compare transcriptions of the inlined `System::String`
refcount idiom, since half of the body I declined (§6) is that idiom.

**ADOPT half — CORROBORATED, from a different function in a different
namespace.** AVBODY's `0x18004e4d4..e4e2` matches my transcription from
`?SetObject@TextFileStream@IO@@` (`0x1800ba280`) exactly: unconditional QWORD
null-store into the destination, load candidate, `TEST`/`JZ`, store, then
`INC dword [p-0x4]` — same `[ptr-4]`, same DWORD width.

**RELEASE half — NOT CORROBORATED, and I say so rather than letting the adopt
half stand in for it.** AVBODY reports that **none** of their five bodies
contains an inlined String release, so they could not check it. What it does have
is a **third, independent** source: `runtime/types/system/String.h` documents the
block as `{int32 length @ -8, int32 refCount @ -4, chars @ 0}`, which is why the
freed pointer is `p-8` and not `p`. That is a header agreeing with my read of the
bytes, not a second reader of the bytes. **The release half of the idiom still
rests on one reading**, and it is exactly the half where an error is a
use-after-free that does not reproduce on short runs — which is reason #1 in §6
for declining `SetupFileSystem`, now slightly stronger than when I wrote it.

---

## 8. WHAT THIS DOES NOT CLAIM

1. **The three accessors are NOT claimed to satisfy any dependency.** Every one
   of their 8 oracle calls came from my own probe. They have **zero game
   callers**, the census's `GAP(instrument)` tag is correct about them, and Wave
   27's decision not to port them was correct for the reason it gave. This is
   three census rows closed, and I am not calling it anything else.
2. **`divergences=0` on those three is NOT a claim about coverage.** The bodies
   have no branches, so there is nothing for a branch-coverage caveat to bite on
   — but the oracle only ever saw a TextFileStream the probe built.
3. **The PackageFile promotion is NOT claimed to make our nine bodies run on
   every PackageFile.** One of four stampers is retail's copy constructor, and
   §4.4 shows retail's `Reconstruct` can put the DLL's table BACK over an object
   of ours through a call no registration can intercept. The scope is "objects
   built through our three stampers, until something re-stamps them".
4. **`post=1` is not a frequency.** It is "the door opened at least once after
   init". The dispatch evidence is the `calls=` columns, and it is separate.
5. **Arm B does not prove `?_typeInfo@PackageFile@IO@@` is unreachable.** It
   proves the post=1 I saw was mine. `post=0` cannot separate "the route never
   reaches it" from "the route reaches those objects only through in-DLL calls
   that are not doors" — lane ATTEND's caveat, kept rather than dropped.
6. **My registration check's concat branch was UNEXERCISED.** It found ZERO
   `Register*` sites opening with an adjacent-string seam in 1581 files, so the
   branch built against lane SLOTS' defect proved nothing on this run. Reported
   as unexercised, NOT as a pass.
7. **I did not verify the three accessors at cutover.** No cutover boot was run
   by this lane; `RtOriginal` resolves to nothing with the DLL absent, so the
   oracle is a DLL-present instrument by construction.
8. **I did not touch `SetupFileSystem`, `??_7ReaderWriter@`, `??_7Stream@`, or
   any of IO's 4 DATA rows.** IO's gap of 8 is 1 BODY + 3 VTABLE (2 permanently
   unpromotable, 1 measured-not-promotable) + 4 DATA.
9. **The board figures in §1 are mine, on my branch, pre-merge.** They must be
   re-derived by the integrator; `called` moved for two reasons at once.
10. **The retail `System::String` RELEASE idiom transcribed in §7.3 still rests
    on ONE reading of the bytes.** AVBODY has no body containing an inlined
    release and could not corroborate it; the ADOPT half is corroborated
    independently, and `String.h` agrees about the block layout, but a header is
    not a second reader. Anyone porting `SetupFileSystem` should treat the
    release half as unconfirmed.
11. **The post-report correction re-verified ONE thing and inherited the rest.**
    I re-read `0x18004e4b0` and `0x18000c6a0` and `0x180025310` myself. I did
    NOT re-run the game, the census, the oracles, or the promotion A/B after the
    doc edit — nothing in the edit could affect them, because no code file
    changed.

---

## 9. INSTRUMENT DEFECTS, EACH WITH THE DIRECTION IT ERRS

Deflating outranks inflating (warning 8).

| # | instrument | defect | direction | state |
|---|---|---|---|---|
| 1 | my own retslot screen | M1 fell back to `rfind` of the wrong convention token; reported a by-value return as a reference | **DEFLATING** — says "no hidden slot" when there is one | **FIXED**, defect recorded at the fix site; found by the known positive, not by review |
| 2 | `file_stream_probe.cpp` verdict line | the `att` range was hard-bounded `1..5`; a new control scores `att=0` and prints "CAUGHT BUT NOT VIA ITS OWN MECHANISM — NOT BANKED" | **DEFLATING** — a working control reads as a broken one | **FIXED** in the same commit as control 6; the bound is invisible and nothing points at it |
| 3 | `RtIo_PackageFile_Ctor` / `Reconstruct` oracles | compared the vtable field, which a promotion makes differ by construction | would have become a permanent unexplained red line | **FIXED** with a derived exclusion + A/B, §4.5 |
| 4 | `PromoteVtableGuarded` | its own printout says the refusal path is `UNTESTED` on every normal run | inflating-ish: "0 refused" from a guard never handed a bad row | **NOT fixed** — exercised on my row instead (§4.1). The tool already names the remedy. |
| 1b | my own retslot screen, **again** | M1+M2 answer only "IS there a slot?"; I derived an ORDERING prescription beside them that no control could reach, and got it **backwards** | **WILD-WRITE** — the prescription would deref the caller's slot as an object and write through `self` | **FIXED** post-report: M3 column measures deref-vs-store-through. **Found by lane AVBODY. Not findable by my controls, by construction.** |
| 1c | M3's first control | scored `?GetSource@Stream@IO@@` `this=? slot=rdx` and I nearly read it as a pass — that body is the abstract base and **never touches RCX**, so it cannot exercise M3's `this` half | **DEFLATING** (a half-blind control reading as a whole one) | **FIXED** in the same edit: `?GetName@PackageFile@IO@@` added as M3's own positive, reaching both halves |
| 5 | my registration check | concat branch never handed a positive | unknown; cannot be scored | **RECORDED, not cleared** (§8.6) |
| 6 | `.pdata` for my three accessors | **does not exist at all** (leaf, no prologue) | DEFLATING — silence reads as "no such function" | pre-existing; the vtable and the export directory are the authorities here |

---

## 10. RESIDUALS, EACH WITH AN ADDRESSEE

**R1 — `?SetupFileSystem@Modification@IO@@` is unported, and now better
conditioned than Wave 28 left it.**
ADDRESSEE: **a named Wave 30 lane that owns only this row.** It inherits: a
DERIVED extent (0xA39 / 714 instructions), a reproduced live route, MISC's
callee decomposition, **and the new fact that three of its callees are ABSENT
from our tree** — `?ForceUnload@ExternalFactory@Object@@` `0x1800beec0` (an
unported export) and the unexported internals `0x180027970`, `0x180027380`.
Budget those three as part of the row, not as a surprise.

**R2 — `?_typeInfo@PackageFile@IO@@` is still ROUTE-UNDECIDED, and I have
narrowed it.**
ADDRESSEE: **the next lane that runs the mint promotion filter.** Wave 26's
condition is *unchanged and unsatisfied*; what is NEW is that an armed run makes
it look satisfied (§4.2). Any future run offered as evidence for this row **must
publish its unarmed arm beside it**, or it is measuring itself.

**R3 — four promoted rows print a bare `+0x0:` divergence on every diff run.**
`UI::SpriteSheet::Construct`, `Graphics::AnimationEvent::AnimationEvent()`,
`Graphics::Geometry::Geometry()`, `Engine::UpdateLoop::ctor`. Each is the vtable
identity and none says so at the site. ADDRESSEE: **the lane that next promotes
any `??_7` row** — the derived-exclusion idiom in `package_file.cpp` (§4.5) is
copy-pasteable and comes with an A/B that proves it excuses only what it claims.

**R4 — `??_7TextFileStream@IO@@6B@` needs STAMPERS, not slots.**
ADDRESSEE: **the lane that ports `??0TextFileStream@IO@@QEAA@XZ` (0x1800ba530)
and `?Open@TextFile@IO@@QEAA_NAEBVString@System@@@Z` (0x1800b9fc0)** — those two
are on the `?SaveText@External@` path, the one route that could give this row a
post>0. **Do NOT re-derive the attendance; it is measured twice in §5.**

---

## 11. FILES TOUCHED

| file | what |
|---|---|
| `runtime/src/io/text_file_stream_win.cpp` | the three accessor bodies, their oracles, their registrations by name AND address; the log line corrected |
| `runtime/src/io/file_stream_probe.cpp` | screen D (ours vs theirs), control 6, the `att` bound widened |
| `runtime/src/io/package_file.cpp` | derived vtable-field exclusion in the ctor AND Reconstruct oracles, with the wrong-prediction post-mortem at the Reconstruct site |
| `standalone/reg_mint_promoted.cpp` | the `??_7PackageFile@IO@@6B@` promotion + warrant + `BANISHED_RT_PROMOTE_PKGFILE` A/B; the `??_7TextFileStream@` non-promotion, with its measured grounds |
| `docs/runtime_port/wave29_iolane_retslot_screen.py.txt` | **new** — the hidden-retslot screen, both controls, the defect recorded at the fix site |
| `docs/runtime_port/wave29_iolane_regcheck.py.txt` | **new** — the already-registered check, comment-stripping and concat-aware |
| `docs/runtime_port/WAVE29_LANE_IOLANE.md` | this file |

No tool in `tools/` was written or modified.

---

## 12. GATES

* `mingw32-make standalone-debug` — **FOREGROUND**, exit 0 at every build.
  §0.2 gate run each time: size + mtime recorded, the `find … -newer` list
  **EMPTY**, `nm` found all three bodies in the linked image
  (`RtIo_TextFileStream_GetObject{Name,Type,Version}`), `strings` found the
  mangled names and `BANISHED_RT_PROMOTE_PKGFILE`.
* `tools/game_dir_lock.sh acquire IOLANE 900` — **not piped, not backgrounded**.
  **Wait: 0 seconds** (the lock was free on first ask). `assert-held IOLANE
  <my exe>` run before **every** launch, so no measurement was taken against a
  sibling's binary. Released at the end of the lane.
* `tools/boot_state.sh` not needed: every run self-quit via
  `BANISHED_PAUSEMENU_QUIT` and returned exit 0. No `grep -q` polling loop was
  used anywhere.
* **Six runs**, every one with `standalone_crash.log` at **0 bytes**.
* Census, stamper census and the attendance instrument all re-derived by me on
  this tip; no figure in this document is copied from a prior wave's prose
  without being re-run, and where a prior figure is quoted it is named as
  reproduced.

---

## 13. MY OWN ERRORS

1. **I wrote "checked rather than assumed" about something I had assumed**, in a
   promotion warrant, and it was false (§4.4). A `printf` caught it inside one
   run. The sentence is preserved in place with the correction, not deleted.
2. **My retslot screen shipped a deflating bug** and only its known positive
   found it (§7.1) — in the same hour I wrote a message to AVBODY telling them to
   hand their screen a known positive.
3. **I nearly reported Wave 26's `?_typeInfo@PackageFile@IO@@` route condition as
   satisfied** (§4.2). The only reason I did not is that I ran an unarmed arm,
   and I ran it out of routine rather than suspicion.
4. **I widened the probe's `att` bound only after writing control 6** — for a few
   minutes the tree held a control that would have reported itself unbanked.
5. My first registration check compared an uppercase verb against a lowercased
   string, so its BY-ADDRESS branch could never fire. The by-name branch masked
   it, which is a union-scored control in miniature.
6. **I prescribed a wild-write argument order to my partner** and put it in a
   committed document (see the correction at the top). I applied the
   free-function return-slot rule to a member function. Two things make this
   worse than an ordinary mistake and both are mine: I derived an **ordering**
   claim from a **present/absent** screen and let the screen's credibility carry
   it; and I did it **in the same message** in which I was telling AVBODY to hand
   their screen a known positive. I could not have caught it — but I could have
   noticed that I was asserting something my instrument did not measure, which is
   the check this project keeps writing down.
7. **I nearly repeated it inside the fix**, reading a half-blind M3 control as a
   pass (§9 row 1c). The only reason I did not is that I disassembled the control
   subject to ask *why* it printed `this=?` instead of accepting the shape.
8. I edited after reporting. It is documentation-only and the top of this file
   says so, but it is still a violation of warning 18's default and the
   integrator should treat the `ea8a2839` code tree, not this prose, as the
   measured artefact.
