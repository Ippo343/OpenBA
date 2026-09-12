# WAVE 28 — LANE MISC

Branch `wave28/misc`, cut from `wave28/base` at `5d2594de`.
Scope: the six leftover named bodies across five namespaces (WAVE28_KICKOFF.md §3).

Written as the lane went, not at the end.

---

## 0. HEADLINE

**Five rows ported; one row was already done; one row not ported. And every one
of the four stated blockers I was handed turned out to misdescribe reality — two
by overstating the work, two by misnaming the obstacle.** §6 is the finding.

- `?NextGroup@Random@System@@` had a byte-faithful BODY since Wave 2 and was
  missing only a `RegisterRuntimeExport` line.
- `?Serialize@StringTable@UI@@` was ported AND registered in full by Wave 27
  lane FRONTIER, after the residual that handed it to me was written.
- `?Update@Scene@Audio@@`'s twenty-wave-old blocker named the RNG. The RNG was
  never the obstacle; the audible side effect is. **Ported anyway, and it ran
  799 times in a clean in-game run.**
- `?SetupFileSystem@Modification@IO@@` was recorded as unverifiable because "the
  only unattended mod route is the broken one". **Measured false**: it is
  reached on the ORDINARY autostart path, through `Loader::Load`, with no mods
  env var set at all. Evidence file committed.

---

## 1. ROW-BY-ROW STATUS

| # | Row | Retail | Bytes | Outcome |
|---|---|---|---|---|
| 3 | `?NextGroup@Random@System@@AEAAXXZ` | `0x1800c7dd0` ord 3245 | 0xae | **PORTED** (registration only — body existed), and **live: ≥8 draws** through it in the §5.1 run |
| 4 | `?SetTransformScale@Context@Video@@…` | `0x18001a400` ord 3994 | 0x2a7 | **PORTED**, new body |
| 5a | `??1StringTable@UI@@UEAA@XZ` | `0x18011d8f0` ord 691 | 0x37 | **PORTED**, new body |
| 5b | `??3StringTable@UI@@SAXPEAX@Z` | `0x180002220` ord 1013 | 0x5d | **PORTED**, new body |
| 6 | `?Serialize@StringTable@UI@@UEAA_N…` | `0x18011d940` ord 3764 | 0xcc | **ALREADY PORTED** by Wave 27 FRONTIER — no work, stale residual |
| 1 | `?Update@Scene@Audio@@QEAA_N…` | `0x1800df6c0` ord 4502 | **0x718** | **PORTED**, new body, and **EXERCISED — 799 calls** (§5.1) |
| 2 | `?SetupFileSystem@Modification@IO@@QEAAXXZ` | `0x1800aebb0` ord 4030 | **0xa38** | **NOT PORTED** — but its stated blocker is **refuted by measurement** (§5.2) |

Every address, ordinal and extent above is MEASURED — `pefile` over the export
directory of `C:\GOG Games\Banished\Runtime-x64.dll` for name/RVA/ordinal, and
`objdump -d` over the same file on disk for the extents (first byte to the
`RET`, then `INT3` padding). Ghidra was not the source for any of them.

---

## 2. WHAT LANDED, AND WHERE

### 2.1 `?NextGroup@Random@System@@AEAAXXZ` — `runtime/src/system/value_types.cpp`

The MT19937 twist. **Its body has existed since Wave 2** as the file-local
`NextGroupImpl` at `value_types.cpp:83`, called by `RandImpl` and therefore by
every one of the six `Random` members this tree already ports. What did not
exist was a registration, so a callee census could not tell it from an absent
body and it stood on the board as a named unported System BODY row.

I verified the existing body against `objdump` instruction by instruction before
registering it, rather than trusting that a Wave 2 comment saying "confirmed
against the disassembly" still described these bytes:

- `r10d` starts `0x18d` (397), loop counter `r11 = 0x270` (624).
- The `UPPER_MASK | LOWER_MASK` splice is realised as
  `xor r9d,[r8]` / `btr r9d,0x1f` / `xor r9d,[r8]`, which computes
  `(mt[i] & 0x80000000) | (mt[i+1] & 0x7fffffff)` — the same value the port's
  explicit mask pair produces.
- `MATRIX_A` is `0x9908b0df` at `0x1800c7e5f`, applied on `test r9b,1`.
- Both `% 624` are signed magic-division idioms (`0xd20d20d3`, `sar 9`), which
  C's `%` on non-negative `int` reproduces.

The port is two lines: an `extern "C" RtSystem_Random_NextGroup` wrapper and a
`RegisterRuntimeExport` inside `RtSystemValueRegister()`. That register function
is reached through `RtWaveEnabled("BANISHED_RT_VALUE")`, which is a **kill
switch, default ON** — the registration happens in an ordinary run.

Deliberately NOT given a diff oracle. `NextGroup` returns void and its whole
effect is the 624-word state; `RtDiffRandomShadow`, which every other `Random`
member in this file already uses, `memcmp`s exactly that state on every `Rand*`
call that triggers a twist. A second oracle wrapped around the same bytes would
be the self-check shape (`reference_shadow_marker_is_not_an_oracle.md`), not a
new screen. Written into the body's comment, not only here.

**Lane PARTSYS was told early** (message sent before any other row landed), with
the consequence spelled out: `runtime/src/graphics/wave27_lane_gfxpart.cpp:269`
resolves this symbol through `ResolveExport`, which is registry-aware, so after
this branch merges that call reaches our body instead of the DLL's.

### 2.2 `?SetTransformScale@Context@Video@@…` — `runtime/src/video/context.cpp`

**The name is a trap and the disassembly says so: there is not one `mulss` in
the body.** Nothing is scaled by anything. Retail builds a copy of the matrix
argument in which the **fourth column** (`m[3]`, `m[7]`, `m[11]`, `m[15]`; byte
offsets `0x0c/0x1c/0x2c/0x3c`) is replaced wholesale by the four components of
the `Vector4`, and then runs `SetTransform`'s tail verbatim.

This is kickoff warning 3's shape exactly — count the STORES, not the fields.
Sixteen `movss` land in the destination at `0x18001a5c4..0x18001a647`; **twelve
are sourced from `r8` (the matrix) and four from `r9` (the vector)**, at
precisely the four w-column offsets. The scratch matrix built earlier at
`0x18001a4c9..0x18001a5a7` was traced independently and agrees store for store,
including four writes that are issued and then overwritten by the vector
component at the same slot (`[rsp+0x64]`, `[rsp+0x34]`, `[rsp+0x44]`,
`[rsp+0x54]`). A "scale the basis rows" or "multiply by the vector" reading
passes every plausible account of the name and writes different bytes.

The tail is `SetTransform`'s, and the three details it carries were re-read here
rather than copied on faith:

- the change test is a call to `0x18001d850`, which `pefile` identifies as
  `??9Matrix@Math@@QEBA_NAEBV01@@Z` (`operator!=`, ord 1286) — the file's
  existing inlined `MatrixNotEqual` is that function;
- the dirty bit is `0x4000` (`or [rdi+0x10],0x4000`) = `kDirtyTransformConstants`;
- **the `_transformCount` high-water update is OUTSIDE the change test.** The
  `je` at `0x18001a5be` lands ON the `cmovg` at `0x18001a64e`, so the count
  rises even when the matrix is unchanged. Same asymmetry the sibling port
  already warns about, and this body would have lost it if the tail had been
  written from the sibling's C rather than from these bytes.

Registered by NAME **and** by ADDRESS (`0x18001a400`), name spelled as a literal
at the registration site for the reason the file's own banner gives (the census
cannot see through a named constant).

Gets the file's existing replay oracle for free: `ReplaySnapshot` +
`RT_ORIGINAL_ONCE` + `RT_DIFF_BYTES` over the whole `Context`, the same shape
its eleven siblings use. **Not yet exercised — see §4.**

### 2.3 + 2.4 `??1StringTable@UI@@` and `??3StringTable@UI@@` — NEW FILE `runtime/src/ui/stringtable_lifecycle.cpp`

The wave's only DTOR gap row and only OPNEWDEL gap row, both on the same class.

**A separate file, not `stringtable_factory.cpp`'s tail — purely a merge-hazard
decision.** Three lanes of this wave are on the factory family and that is the
family's file. Appending to its tail from a private worktree is how Wave 27
collected the merge hazard its own kickoff warning 10 is about. The new file's
banner says so and points back at `stringtable_factory.cpp` as required reading.

**The dtor** (0x37 bytes) is three steps: re-stamp our own vptr, call
`~Array<TextEntry>` on `this+0x20`, tail-JMP `~External`. All three callees
already exist in the tree, which is what made this row cheap:

| callee | status |
|---|---|
| `0x180154630` `??_7StringTable@UI@@6B@` | minted DATA row, taken through `RT_DATA_SLOT` — the same door the ctor uses, so ctor and dtor cannot drift |
| `0x180004a90` `~Array<TextEntry>` | **NOT exported** (pefile returns nothing at that RVA), and already **ported AND registered by address** — Wave 27 lane FRONTIER, `array_destroy.cpp:751`, spec `kD_Str2_10`. So this is not a new cutover door; the address is already satisfied. |
| `0x1800bc950` `??1External@Object@@UEAA@XZ` | exported, ord 588, ported Wave 9 lane B2. Reached BY NAME for that reason. |

The tail-JMP is reproduced as call-then-return. That is not a liberty: the two
differ only in the frame, and the function returns void.

**The operator delete** carries the lane's one genuinely new binary finding:

> `??3StringTable@UI@@SAXPEAX@Z` (`0x180002220`) is **instruction-for-instruction
> identical** to `?MemFree@MemoryAllocator@System@@SAXPEAX@Z` (`0x1800c7ad0`,
> ord 3228). Not "similar", not "the same shape" — the two listings were diffed
> line by line and every instruction, displacement and immediate agrees,
> including the `MOV [RSP+0x20],-2` SEH state store and the `REPZ RET`. MSVC
> emitted the class's `operator delete` as a duplicate of the global free.

That is what licenses the one-line body (delegate to `0x1800c7ad0` through
`ResolveAddr`, registry-first) instead of opening a second set of doors on the
allocator singleton `0x1801fc718`, its lock at `+0x640`, and the two
critical-section imports at `[0x18014b390]`/`[0x18014b388]`. The delegate
re-tests null itself, so retail's guard is not lost. This file makes **no**
allocator decision of its own: with `BANISHED_RT_ALLOC` unarmed the free lands
in retail's arena, exactly as retail's own bytes do, because retail's own bytes
reach the same singleton.

The duplicate-body finding is about the **shipped retail binary only**; it was
not re-checked against `Runtime-x64-profile.dll`, and nothing here depends on
the profile build. Stated in the file too.

Both rows registered by NAME and by ADDRESS, behind `BANISHED_RT_UISTRTAB`
(shared with `stringtable_factory.cpp`, **default ON**) — deliberately the same
switch, because a switch that could arm the ctor without the dtor is the
half-armed state `rt_ui_port.cpp:159` records cutover attempt 28 hitting.

Wired at `rt_ui_port.cpp` as a fifth `StringTable` pass, after FRONTIER's, so it
still precedes `RtDataMintInit()`.

---

## 3. `deleting_dtor.cpp` INDICES CLAIMED

**NONE. Zero indices claimed. The file is untouched by this lane.**

Checked rather than assumed, because I was porting a dtor and an operator delete
and kickoff warning 10 is aimed at exactly that: `runtime/src/object/deleting_dtor.cpp`
contains **no `StringTable` row at all** (grep for `StringTable` over
`deleting_dtor.cpp`, `deleting_dtor_internal.h` and `deleting_dtor_probe.cpp`
returns nothing). `UI::StringTable`'s deleting destructor is vtable slot 0,
`0x180002740`, and it is hand-written as `RtUiStringTable_ScalarDelete` in
`stringtable_factory.cpp:718` — outside the positional `kSpecs` table entirely.
`kSpecCount` is still 50 on this branch and this lane did not move it.

Lane SLOTS and lane PARTSYS may append freely; MISC takes no index.

---

## 4. MEASURED vs INFERRED, and what is UNVERIFIED

### MEASURED

- Every address, ordinal, symbol spelling and function extent in §1 (pefile +
  objdump against the retail DLL on disk).
- Every store/branch/immediate claim in §2 (objdump listings, read here).
- `mingw32-make standalone-debug` **exits 0** on this branch, including the
  `check-log-strip` gate and the `cutover manifest gate: OK (60 rows)`, with the
  linked-image checks in §7b (exe size + mtime, no source newer, `nm` and
  `strings` counts, and a negative control at 0).
- `deleting_dtor.cpp` contains no `StringTable` row (§3).
- `?Serialize@StringTable@UI@@` is already ported and registered (§6.1).
- **`?Update@Scene@Audio@@` executed 799 times** in a clean in-game run, with
  the branch breakdown in §5.1, no crash and no `resolved NULL`; and **0 times**
  in a negative-control run with its switch off.
- **`?SetupFileSystem@Modification@IO@@` is reached on the ordinary unattended
  autostart path** — one residue row in a 141-row census, no mods env var set
  (§5.2, evidence file committed).
- Every export symbol in this lane re-verified in a lane-private directory
  after the scratchpad-clobber warning: 20 positives by name AND RVA AND reverse
  lookup, 4 negative controls, 0 failures (§7a).

### INFERRED, and labelled as such

- That registering `??1StringTable@UI@@` and `??3StringTable@UI@@` makes vtable
  slot 0 self-contained at cutover. This follows from how `ResolveAddr` and
  `RT_EXPORT_ONCE` are documented to consult the port registry first, and from
  reading `RtUiStringTable_ScalarDelete`'s two resolve sites. **It is not
  measured**, because nothing in a DLL-present run dispatches slot 0.
- That `NextGroup`'s existing body is exercised continuously through `RandImpl`.
  The call chain is in the source; the *count* is not measured by this lane.

### UNVERIFIED — in that word

- **`?SetTransformScale@Context@Video@@` is UNVERIFIED.** It is registered and
  it carries `context.cpp`'s replay oracle, but I have **no `calls=` count** for
  it — the oracle is off by default and this lane did not arm it. Reporting a
  registered body as verified is what WAVE28_KICKOFF.md §4 forbids, and this row
  is the one where I would most like a number and do not have one.
- **`??1StringTable@UI@@` and `??3StringTable@UI@@` are UNVERIFIED.** They
  printed no attendance block in any of the three runs, i.e. attendance 0, which
  is the EXPECTED reading with the DLL present for the reason
  `stringtable_factory.cpp`'s banner gives at length. Per lane GFXBODY's
  measurement this wave, that zero is **not** by itself evidence of a defect —
  and it is equally not evidence of health.
- **`?Update@Scene@Audio@@` is EXERCISED, NOT VERIFIED.** 799 calls says it RAN.
  Nothing says it is CORRECT, and nothing can: it has no oracle, and the reason
  is the audible side effect (§5.1). This is the lane's largest single piece of
  new debt and R5 is the bisection for it.
- **`?SetupFileSystem@Modification@IO@@` is NOT PORTED.** Not unverified —
  absent. §5.2 and R1.
- The `Audio::Device` and `Audio::SoundEffect` offsets in `audio_scene_update.cpp`
  are from a SINGLE READER and are **UNVERIFIED as a layout**, distinct from the
  body being exercised. R2 names who overrules them.
- **`?NextGroup@Random@System@@`'s door is verified; nothing about the body is
  newly verified.** The §5.1 run drove at least 8 draws through it (4 started
  samples x 2 draws each), so the registration has a live route. The body itself
  is Wave 2's and was already under continuous comparison.

---

## 5. THE TWO LARGE ROWS

### 5.1 `?Update@Scene@Audio@@` — PORTED, and the only body in this lane with a MEASURED execution count

New file `runtime/src/audio/audio_scene_update.cpp`. 1816 bytes
(`0x1800df6c0..0x1800dfdd7`, `RET` then `INT3`, measured with objdump — not from
`.pdata`, which is only a lower bound).

**The stated blocker was dead, and this lane killed it with its own other row.**
See §6.3. What actually blocks an oracle is different and is now written into
the file: the body starts and stops AUDIBLE sound through `Device::PlayPlatform`
and mutates the `Device` singleton, so a replay plays every sound twice. The
conclusion "no oracle" was right; the *licence* offered for it was wrong.
Repairing the licence and not the sentence is warning 13.

What the port found that a tidy rewrite loses — each read off the listing:

- **The `EffectSlot` clear is `0x2c` bytes, not `0x30`.** Retail's last store
  in that block is `MOV DWORD PTR [RCX+0x28],0x3f800000` — a FOUR-byte store —
  so `+0x2c..+0x2f` (`Audio.h`'s `_pad2c`) are never written. A `memset(...,
  0x30)` writes bytes retail does not.
- **Both loop bounds are RE-READ from `effect+0x38`, not cached** (`CMP
  R10D,[RDI+0x38]` at `0x1800dfc53`, `CMP R11D,[RDI+0x38]` at `0x1800dfc68`).
  It matters at a real boundary: a `SoundEffect` with ZERO samples skips the
  inner loop, leaves `endedCount` at 0, satisfies `0 == 0`, and **recycles its
  channel on the first Update**.
- **The "not started" window floor is `0.0 / _secondsPerTick` truncated, not
  `0`.** Identical unless the divisor is 0, where it is NaN and `CVTTSD2SI`
  gives `INT64_MIN`. Transcribed as the division.
- Constants LIFTED, not recognised (warning 4): `0x42c80000` = 100.0f (the
  config-volume divisor) and `0x2f800000` = 2^-32 (the RNG unit scale), both read
  with `objdump -s`. The MT19937 masks are the shipped `0xff3a58ad` /
  `0xffffdf8c`, and the two inlined draw sites were **set-differenced against
  each other** rather than assumed equal — they are byte-identical.
- Six of seven direct callees are exported, so six are NAME doors. The seventh,
  `0x180050d30` (`Array<T16>::operator=`), is an unexported internal **already
  ported and registered by address** by lane Q, so this port opens **no new
  cutover door**.

Layout this body pins — `Audio::Device` and `Audio::SoundEffect`, neither in
`Audio.h` — is from a SINGLE READER and is labelled as such in the file, kept as
named byte offsets with the producing instruction rather than promoted to a
struct. `+0xa88` is the one exception: `audio.cpp:1171` already records that
exact guard from a different body.

**MEASURED, on a clean in-game run** (33,410,905-byte debug exe built 13:15,
deployed, `BANISHED_AUTOSTART=1 BANISHED_PAUSEMENU=600 BANISHED_PAUSEMENU_QUIT=120`,
clean self-quit, `standalone_crash.log` 0 bytes, no `resolved NULL` line):

```
[RtPort] Audio::Scene::Update ATTENDANCE: calls=799 effects=2885
  (play=2 stop=1 expire=0) channels=2035
  (startAttempt=4 placed=4 deviceFull=0 volume=2031 freed=0)
```

Why this is worth more than a bare attendance number: the counters say **which
branches ran**, and the ones that ran hardest are exactly the ones a wrong
layout would fault in. `volume=2031` is 2031 traversals of
`device+0xa88` -> `device+0xaa0 + idx*0x20` -> a **virtual call through
`vtbl+0x60`** on a platform-channel object — the single most speculative thing
in the port, exercised two thousand times without a fault. `placed=4` of
`startAttempt=4` means the device-slot walk (stride `0x28`, 64 entries) found a
free slot and the six-store stamp landed, four times.

The report is printed by a **static destructor** (`UpdateReport`), which runs on
normal process exit — not by a `*Summary()` function nothing calls. That
distinction was circulated mid-wave and is checked here: the block is present in
the armed run's log and absent in the control's.

**NEGATIVE CONTROL, and it is a real one.** Same exe, same command line, plus
`BANISHED_RT_AUDIOSCENEUPDATE=0`: the attendance block is **absent** (grep count
0), the run still reaches the built world and self-quits with an empty crash
log. So the 799 is gated on the port actually being registered — it is not a
counter that increments regardless — and the DLL path and our path both reach
the same gameplay milestones.

**What the 799 does NOT establish, in the file and here:** that the body is
CORRECT. Nothing compares its output to retail's and nothing can. Lane GFXBODY's
measurement this wave cuts the other way too — a non-zero attendance is not
health any more than a zero is a defect. This body is **EXERCISED, NOT
VERIFIED.**

### 5.2 `?SetupFileSystem@Modification@IO@@` — NOT PORTED. But its blocker is refuted by measurement.

**I did not port this row, and I am not going to dress that up.** What I did
instead is worth more to Wave 29 than a risky body would have been.

**The measurement.** WAVE27_CLOSED.md §5 records it as *"writable but NOT
VERIFIABLE: the only unattended mod route is the broken
`ModDialog_ToggleMod_Impl+0x43` one."* That is **false**, and the evidence is
committed at `docs/runtime_port/wave28_misc_setupfilesystem_residue.txt` with a
re-derivation recipe:

```
[RtResidue] 1        7ff6123a91be     ?SetupFileSystem@Modification@IO@@QEAAXXZ
```

from a `BANISHED_RT_STRICT=1` autostart run with **no mods env var of any kind**.
The only `ResolveExport` of that symbol in the tree is a function-local static
(`loops.cpp:214`), which initialises lazily, and its only call site
(`loops.cpp:712`, inside our ported `?Load@Loader@Engine@@`, retail
`0x1800dc050`) is written as `ModificationSetupFileSystem()(mod);` — one
expression. A residue row therefore proves the call site EXECUTED, and the run
completed normally after it. `Loader::Load` runs on **every loop transition**,
which autostart performs going menu -> New Game -> GameUpdateLoop. The mods menu
is not involved.

Instrument positive control: the same run printed **141** residue rows, so an
empty result would have been a real absence, not a dead census.

**The decomposition, so the next lane does not re-derive it.** Extent
`0x1800aebb0..0x1800af5e8` = **0xa38 = 2616 bytes**, measured. But the logic is
far smaller than that:

- **16 of the 18 direct call targets are one repeated shape.** `0x1800c6c00`
  (`MemoryAllocator::Free`) appears 16 times, each inside an inlined
  Enter/Free/Leave triple around `*(0x1801fc718)+0x640` with
  `[0x18014b390]`/`[0x18014b388]`. That is ~0x500 bytes — roughly **half the
  function is inlined `System::String` release epilogues.**
- The remaining callees, all identified by pefile RVA, and **12 of 18 are
  exported**: `?GetPath@External@Object@@` x5, `?GetCompiledPath@ExternalFactory@`
  x4, `?FindNext@ExternalFactory@Object@@` x4, `?AdjustDelimiters@Filename@System@@`
  x3, `?GetPackfileId@FileSystem@IO@@` x2, `??0WideString@System@@`,
  `?ForceUnload@ExternalFactory@Object@@`, `?Load@External@Object@@`,
  `?ClosePackages@FileSystem@IO@@`, `?LoadPackages@FileSystem@IO@@`,
  `?ExistsInPackfile@FileSystem@IO@@`, `?MemFree@MemoryAllocator@System@@`.
  Five are unexported internals: `0x180027970`, `0x180028cd0`, `0x180027380`,
  `0x180026880`, `0x1800098d0`.
- Globals: `?_singleton@FileSystem@IO@@` (`0x1801fc6a8`),
  `?_singleton@ExternalFactory@Object@@` (`0x1801fc6c8`),
  `?_singleton@Factory@Object@@` (`0x1801fc6e8`),
  `?_typeInfo@External@Object@@` (`0x1801f9b38`).
- One Win32 indirect at `[0x18014b0d0]`, reached right after a
  `System::WideString` conversion of a filename.
- `0x1800aee50..0x1800aeefd` is an **inlined AVL/BST walk with an inlined
  `strcmp`**, descending `[rbx+0x18]` / `[rbx+0x20]`.

**Why I stopped.** Not budget — I had it. Three things about this specific body:
(1) half of it is refcount release, and warning 6's asymmetry bites hardest
here — under-release leaks, over-release frees a live `System::String`;
(2) the inlined `strcmp` is the exact shape of
`reference_uniform_population_is_not_a_verification.md`, where a ported inline
`strcmp` returned 0 for every pair sharing a first byte and nothing caught it;
(3) the body **tears down and remounts the package filesystem** on the live
loader path, so a wrong port's failure mode is a silently wrong resource mount
or heap corruption, in a wave with no verification lane. Registering that
default-ON on the strength of a hand-read would have been the wrong trade. The
route I found makes it a much better-conditioned job for a lane that owns only
it.

---

## 6. STATED BLOCKERS RE-CHECKED — the finding most likely to outlive this lane

Four of the six rows arrived with a written reason they were not already done.
**All four reasons misdescribed reality.** Two overstated the work (the row was
already done, or nearly); two named the wrong obstacle (the real one was
elsewhere, and in both cases the row was more tractable than advertised). Not
one of the four was simply correct.

| row | stated reason | verdict |
|---|---|---|
| `?Serialize@StringTable@UI@@` | "unowned — this lane did not read it"; "one of the last two traps on a table otherwise closed" | **FALSE.** Ported, registered, row 9/9, by Wave 27 FRONTIER |
| `?NextGroup@Random@System@@` | "a named unported BODY row" | **HALF FALSE.** Body since Wave 2; only the registration was missing |
| `?Update@Scene@Audio@@` | "needs a snapshot/restore of the Random state" | **WRONG OBSTACLE.** That has existed since Wave 2. The real one is the audible side effect |
| `?SetupFileSystem@Modification@IO@@` | "NOT VERIFIABLE: the only unattended mod route is the broken one" | **MEASURED FALSE.** Reached on the ordinary autostart path via `Loader::Load` |

### 6.1 `?Serialize@StringTable@UI@@` — "unowned, this lane did not read it". FALSE.

Wave 26 lane FACTORY residual #5 handed this row on because it had not read it;
my brief added that it is "one of the last two traps on a table that is
otherwise closed".

**It is neither.** `?Serialize@StringTable@UI@@UEAA_NAEAVStream@IO@@@Z` is
ported in full as `RtUiStringTable_Serialize` at
`runtime/src/ui/stringtable_factory.cpp:757`, registered by name and by address
at `:809-810`, with its own attendance counters and a transcription of
`0x18011d940` that names a line number for every step. Wave 27 lane FRONTIER did
it, together with slot 0, and `rt_ui_port.cpp:167` records the result in the
present tense: *"Takes the row to 9/9 on the static screen."*

The table was **already closed** when this wave's brief called it "otherwise
closed but for two traps".

### 6.2 `?NextGroup@Random@System@@` — "a named unported BODY row". HALF FALSE.

True as a *census* statement and false as a *work* statement. The body has
existed and been correct since Wave 2; only the registration was missing. This
is `reference_half_registered_port_reads_as_absent.md` recurring for at least
the third time on this project, and the cost each time is a lane budgeting for a
decompilation it does not have to do.

### 6.3 `?Update@Scene@Audio@@` — the blocker named the WRONG OBSTACLE.

Wave 27 §5's *"out of budget, NOT newly blocked"* was honest as far as it went,
and Wave 24's "NOT A WALL" re-check still holds. But the *technical* blocker
Wave 8 wrote into `audio.cpp`'s header, and which three waves then re-confirmed
by reading rather than by checking, was this:

> *"a replay that draws twice from `System::Random` diverges by construction.
> What it needs is a snapshot/restore of the Random state as well, which is a
> real and bounded next step rather than a missing tool."*

**That step has existed since Wave 2.** `value_types.cpp`'s `RtDiffRandomShadow`
snapshots the live `Random`, lets ours mutate it, and replays the original on an
independent scratch copy — precisely the described shape. `Random` is a flat
2500-byte POD, so the snapshot is a `memcpy`. And the RNG is not even a large
part of this body: it is reached through exactly **two** inlined draws
(`0x1800df992`, `0x1800dfa0c`), both calling `0x1800c7dd0`, the function this
lane registered in the previous commit.

**What actually blocks an oracle is the audible side effect**, and that is a
different fact with a different consequence: it cannot be retired by tooling at
all. Wave 8.5 already knew this shape — it is why the teardown family needed a
synthetic scene — but the Device here is loaded from its singleton in the
prologue and written unconditionally, so no synthetic subject removes it.

So the conclusion "no oracle" was always right and the licence offered for it
was always wrong. **Repairing the licence rather than the sentence** (warning
13) is what unblocked the row: once the obstacle is named correctly, "port it
and measure execution, and say in those words that execution is not
correctness" is obviously available, and it is what §5.1 did.

### 6.4 `?SetupFileSystem@Modification@IO@@` — "NOT VERIFIABLE". MEASURED FALSE.

The stated reason was *"the only unattended mod route is the broken
`ModDialog_ToggleMod_Impl+0x43` one."* That sentence is true **about the mods
menu** and it was silently generalised into a claim about the FUNCTION.

The function is also called from `?Load@Loader@Engine@@` (`0x1800dc050`), our
own ported body at `runtime/src/engine/loops.cpp:712`, which runs on **every
loop transition** — including the menu → New Game → GameUpdateLoop chain an
ordinary autostart performs. I did not argue this from the call graph; I
measured it. A `BANISHED_RT_STRICT=1` autostart run with **no mods env var of
any kind** produced the residue row, in a 141-row census, and the resolve sits
in a lazily-initialised function-local static whose only consumer is that one
call expression. Full evidence and a re-derivation recipe:
`docs/runtime_port/wave28_misc_setupfilesystem_residue.txt`.

The size was also understated. **`0x1800aebb0..0x1800af5e8` = `0xa38` = 2616
bytes.** "Writable but not verifiable" reads as small-and-cheap-but-unmeasurable.
It is the opposite on both halves: large, and measurable. I did not port it, for
reasons given in §5.2 that are about blast radius rather than about routes.

I did **not** attempt the mods route, deliberately: the fix there is an upstream
null, it is not this lane's, and the crash evidence is in
`wave17_modsroute_crash.txt`.

### 6.5 The pattern, and it is NOT the one the kickoff warns about

The kickoff's warning is that a handover's **cheap-wins** list is its stalest
section and the first acted on. That is a real finding and it is not what
happened here. Every one of my four failures was in the opposite section:

> **The "still open, and here is why" list is at least as stale as the
> cheap-wins list, and nothing in this project re-derives it.**

`tools/runtime_census.py` re-derives what IS ported, every wave, and the kickoff
quotes it. Nothing re-derives what is NOT — the *reasons* rows are open get
copied forward verbatim, wave after wave, and they rot in two distinct ways this
lane saw both of:

1. **The row closed and the reason outlived it** (6.1, 6.2). The census would
   have caught 6.1 instantly — `?Serialize@StringTable@UI@@` is registered as a
   string literal and shows as ported — but nobody ran the census *against the
   residual*. The residual was promoted into a lane scope on the authority of
   the wave that wrote it.
2. **The row stayed open and the reason was never the obstacle** (6.3, 6.4).
   This one is worse, because it is invisible to any tool. A blocker sentence is
   prose; re-reading it is not re-deriving it, and three waves re-confirmed
   Wave 8's RNG sentence by reading it. **6.3 had been "re-checked" twice and
   the check each time was that the sentence still said what it said.**

Type 2 has a specific tell, and it is worth naming because it is cheap to look
for: **a stated blocker that names a MISSING CAPABILITY** ("what it needs is a
snapshot/restore", "needs an attended run with real keyboard and mouse", "the
only route is the broken one") is a claim about the tree, and claims about the
tree expire. A stated blocker that names an INTRINSIC property ("it starts
audible sound, so a replay plays it twice") does not. Wave 8 wrote the first
kind; the truth was the second kind; and nobody noticed for twenty waves because
the sentence was never wrong about the *conclusion*.

**Actionable form, for the Wave 29 kickoff author — three items, in order of
cost:**

- A residual that names a symbol costs **one grep** to re-check against the
  board before it enters a lane brief. Two of my six rows would not have been
  in §3 had that grep been run.
- A stated blocker that names a missing capability costs **one grep for that
  capability**. `RtDiffRandomShadow` has been greppable since Wave 2.
- A stated blocker that says "no unattended route" costs **one
  `BANISHED_RT_STRICT=1` run**, which is ~3 minutes and already exists. It
  refuted 6.4 outright.

Three lanes this wave (MISC, FACTUI2, FACTREST) independently found rows already
done, and FACTREST found five of twelve. That is not three coincidences; it is
one missing step in the kickoff pipeline.

---

## 7. INSTRUMENT DEFECT OBSERVED, NOT FIXED

**1. The shared session scratchpad has no lane namespacing, and it silently
clobbers.**
Not one file — the convention itself. The integrator circulated this mid-wave
after a sibling lane lost its analysis files to another lane writing the same
basename. I had already written `exp.py` at the scratchpad ROOT, one of the
exact names named. **Not fixed, and re-derivation is what cleared it:** I re-ran
every symbol check from scratch in a lane-private directory
(`<scratchpad>/misc/misc_exp_recheck.py`), with a parser self-test line and two
negative controls. Result in §7a.

**2. `runtime/src/ui/rt_ui_port.cpp:163` — a comment that is now false, left
false on purpose.**
It reads *"The row is 7/9 after these; slot 0 (the deleting dtor) and Serialize
are NOT ported and are named in WAVE26_LANE_FACTORY.md."* Wave 27 lane FRONTIER
ported both, and the very next comment block (`:166-170`) says so and says the
row is 9/9. So the file contradicts itself four lines apart, and **the stale
half is the half a reader hits first** — which is how this reached my lane brief
as a live scope item. **Not fixed**: editing that block is a merge hazard with
three factory lanes live this wave. ADDRESSEE in §8 R3.

**3. `standalone/main.cpp:1991` (`DumpRuntimeResidue`) — the residue census runs
only on the normal exit path, and nothing says so where you use it.**
A run ended with `taskkill` prints no residue, which is indistinguishable from
"the symbol never resolved". I hit this once before switching to
`BANISHED_PAUSEMENU_QUIT`. **Not fixed**; written into the re-derivation recipe
at the bottom of `wave28_misc_setupfilesystem_residue.txt` instead, which is the
surface the next person actually reaches (warning 15).

**4. `runtime/src/audio/audio.cpp:17-27` — the held-back list still states a
blocker this lane measured as dead.**
The `?Update@Scene@Audio@@` entry names the RNG snapshot as the missing step.
That step has existed since Wave 2 and the RNG was never the obstacle. **Fixed
where it matters and not where it does not**: the correction is written in full
at the top of the NEW file (`audio_scene_update.cpp`), which is where the next
reader of this row lands. `audio.cpp`'s header was left alone — a merge hazard
for no behavioural gain, and lane PARTSYS made the same call this wave for the
same reason.

### 7a. The re-derivation, since it is the only thing that clears a clobber

`<scratchpad>/misc/misc_exp_recheck.py`, run against
`C:\GOG Games\Banished\Runtime-x64.dll`:

```
SELF-TEST: parsed 4926 exported symbols
-- POSITIVE (name must exist AND its RVA must match AND the reverse lookup must agree)
   20 rows, all ok
-- NEGATIVE CONTROL
   ok  absent (as claimed)  ??1StringTable@UI@@QEAA@XZ     <- the WRONG dtor spelling
   ok  0x180004a90 not in the export directory (unexported internal)
   ok  0x180005570 not in the export directory (unexported internal)
   ok  0x180050d30 not in the export directory (unexported internal)
VERDICT: 0 failure(s)
```

The `UEAA`-vs-`QEAA` question my brief flagged is answered in both directions:
`UEAA` present at `0x18011d8f0` ord 691 by name AND by RVA; `QEAA` absent.

### 7b. Build gate — foreground, and checked four ways

```
standalone/build/reimpl-x64.exe   33,410,905 bytes   2026-09-12 13:15
find <my 6 touched sources> -newer <the exe>   ->   EMPTY
```

Every build in this lane was FOREGROUND (`make ... > log 2>&1` then `echo $?`),
never backgrounded. `mingw32-make standalone-debug` exits **0**, including the
`check-log-strip` gate and `cutover manifest gate: OK (60 rows)`.

Symbols in the **linked image**, not just the objects:

| check | count |
|---|---|
| `nm` `RtAudioSceneUpdateRegister` | 1 |
| `nm` `RtUiStringTableLifecycleDtorRegister` | 1 |
| `nm` `RtVideo_Context_SetTransformScale` | 5 |
| `nm` `RtSystem_Random_NextGroup` | 1 |
| `nm` `RtUiStringTable_Dtor` | 7 |
| `nm` `RtUiStringTable_OperatorDelete` | 2 |
| `nm` `RtAudio_Scene_Update` | 10 |
| `strings` `??1StringTable@UI@@UEAA@XZ` | 1 |
| `strings` `??3StringTable@UI@@SAXPEAX@Z` | 2 |
| `strings` `?NextGroup@Random@System@@AEAAXXZ` | 3 |
| `strings` `?SetTransformScale@Context@Video@@...` | 2 |
| `strings` `?Update@Scene@Audio@@...` | 4 |
| `strings` `??1StringTable@UI@@QEAA@XZ` (**negative control**) | **0** |

The last row is the one that matters: the wrong dtor mangling is not in the
image, so nothing registered it by accident.

### 7c. One check I was asked for and did NOT need

The integrator circulated a finding that a `Serialize` is a WRITER and therefore
outranks any field naming taken from readers. `?Serialize@StringTable@UI@@` is
not my work (§6.1) but I cross-checked it anyway, since it is the authority on
the layout my two new bodies use: it reads the count at `+0x28`, hands `+0x20`
to `Array<TextEntry>::SetSize`, and strides entries by `0x10` releasing `+0x00`
then `+0x08`. That is `runtime/types/ui/StringTable.h` exactly. **No
disagreement to repair** — recorded as a checked negative rather than silence.

---

## 8. RESIDUALS, each with an addressee

**R1 — `?SetupFileSystem@Modification@IO@@` is unported and now well-conditioned.**
ADDRESSEE: **a named Wave 29 lane that owns only this row.** Not "the next lane
that touches IO" — that is the no-addressee shape four expiry clauses in this
project have already failed on. Hand it §5.2's decomposition and
`wave28_misc_setupfilesystem_residue.txt`. It has a live attendance route and a
crash screen; it does **not** have a correctness oracle, and half of it is
`System::String` refcount release where over-release frees a live object.

**R2 — `?Update@Scene@Audio@@` is EXERCISED, NOT VERIFIED, and the `Device` /
`SoundEffect` offsets in it come from a SINGLE READER.**
ADDRESSEE: **the Wave 29 lane that ports `?Update@Device@Audio@@`
(`0x1800de520`) or `?FreeChannelPlatform@Device@Audio@@` (`0x1800de6f0`)** —
those are `Device`'s own writers, and a writer outranks my reader. If they
disagree with the offsets in `audio_scene_update.cpp`'s banner (`+0x000` slots
stride `0x28`, `+0xa88` gate, `+0xaa0` stride `0x20`), **they are right and I am
wrong**; correct them there, at the surface the next Audio reader reaches.

**R3 — `rt_ui_port.cpp:163`'s stale "7/9 … Serialize NOT ported" comment.**
ADDRESSEE: **the Wave 28 integrator, at merge**, when the three factory lanes'
edits to that file are already being reconciled and the hazard I avoided is
gone. One-line deletion. This is not cosmetic: that comment is the proximate
cause of `?Serialize@StringTable@UI@@` being written into this wave's kickoff §3
as live scope (§6.1).

**R4 — `??_7Stream@Audio@@6B@` slot 2 was NOT touched by this lane.**
My brief said to close it *only if lane SLOTS has not*, and that the table is
SLOTS'. I left it and made **no change to any Audio vtable**. ADDRESSEE: **lane
SLOTS** (its brief lists `Stream@Audio` 1/2 explicitly); failing that, the Wave
28 integrator should confirm at merge whether that slot moved.

**R5 — the `Audio::Scene::Update` port is default-ON and has no oracle.**
ADDRESSEE: **whoever runs cutover attempt 31.** If audio misbehaves or the boot
changes character, `BANISHED_RT_AUDIOSCENEUPDATE=0` is a one-variable bisection
back to the DLL body that does **not** disturb `audio.cpp`'s sixteen other Audio
ports. That separation is deliberate and is the whole reason the switch is its
own.

**R6 — the shared-scratchpad clobber convention.**
ADDRESSEE: **the Wave 29 kickoff author.** Three lanes lost or risked artefacts
to it this wave. The fix is one sentence in the standing warnings ("write to
`<scratchpad>/<lane>/`"); standing warning 1 forbids me building it.
