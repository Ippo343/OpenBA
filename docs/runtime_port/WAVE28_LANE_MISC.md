# WAVE 28 — LANE MISC

Branch `wave28/misc`, cut from `wave28/base` at `5d2594de`.
Scope: the six leftover named bodies across five namespaces (WAVE28_KICKOFF.md §3).

Written as the lane went, not at the end.

---

## 0. HEADLINE

**Two of the six rows were not work. They were already done, and the handover
said otherwise in both directions.** One row (`?NextGroup@Random@System@@`) had a
byte-faithful BODY since Wave 2 and was missing only a `RegisterRuntimeExport`
line; one row (`?Serialize@StringTable@UI@@`) was ported AND registered in full
by Wave 27 lane FRONTIER, three weeks after the residual that handed it to me
was written. Details in §6.

---

## 1. ROW-BY-ROW STATUS

| # | Row | Retail | Bytes | Outcome |
|---|---|---|---|---|
| 3 | `?NextGroup@Random@System@@AEAAXXZ` | `0x1800c7dd0` ord 3245 | 0xae | **PORTED** (registration only — body existed) |
| 4 | `?SetTransformScale@Context@Video@@…` | `0x18001a400` ord 3994 | 0x2a7 | **PORTED**, new body |
| 5a | `??1StringTable@UI@@UEAA@XZ` | `0x18011d8f0` ord 691 | 0x37 | **PORTED**, new body |
| 5b | `??3StringTable@UI@@SAXPEAX@Z` | `0x180002220` ord 1013 | 0x5d | **PORTED**, new body |
| 6 | `?Serialize@StringTable@UI@@UEAA_N…` | `0x18011d940` ord 3764 | 0xcc | **ALREADY PORTED** by Wave 27 FRONTIER — no work, stale residual |
| 1 | `?Update@Scene@Audio@@QEAA_N…` | `0x1800df6c0` ord 4502 | **0x718** | see §5 |
| 2 | `?SetupFileSystem@Modification@IO@@QEAAXXZ` | `0x1800aebb0` ord 4030 | **0xa38** | see §5 |

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
  `check-log-strip` gate and the `cutover manifest gate: OK (60 rows)`.
- `deleting_dtor.cpp` contains no `StringTable` row (§3).
- `?Serialize@StringTable@UI@@` is already ported and registered (§6).

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
  it carries `context.cpp`'s replay oracle, but this lane did not run the game
  and therefore has **no `calls=` count** for it. Reporting a registered body as
  verified is precisely what WAVE28_KICKOFF.md §4 forbids.
- **`??1StringTable@UI@@` and `??3StringTable@UI@@` are UNVERIFIED**, and are
  expected to show attendance 0 with the DLL present for the reason
  `stringtable_factory.cpp`'s banner gives at length. A zero here is the
  expected reading, not a result.
- **`?NextGroup@Random@System@@` is UNVERIFIED as a registration.** The body is
  old and continuously exercised; what is new is the door, and this lane did not
  observe a call arriving through it.

---

## 5. THE TWO LARGE ROWS

*(filled in below as the lane reached them)*

---

## 6. STATED BLOCKERS RE-CHECKED — the finding most likely to outlive this lane

Four of the six rows arrived with a written reason they were not already done.
**Two of the four reasons were false on contact.** Both failures are the same
shape, and it is not the shape the handover warns about.

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

### 6.3 `?Update@Scene@Audio@@` — "out of budget, NOT newly blocked". TRUE.

Wave 27 §5's wording was accurate and Wave 24's "NOT A WALL" re-check still
holds. The specific technical blocker Wave 8 stated — *"a replay that draws
twice from `System::Random` diverges by construction; what it needs is a
snapshot/restore of the Random state"* — is now **completely retired**, and by
this lane's own row 3: `value_types.cpp`'s `RtDiffRandomShadow` is exactly a
snapshot/replay-on-a-scratch-copy of `System::Random`, has been since Wave 2,
and `Random` is a flat 2500-byte POD so the snapshot is a `memcpy`. The body
also reaches `System::Random` **only** through the two inlined draws at
`0x1800df99b` and `0x1800dfa15`, both of which call `0x1800c7dd0` — the function
this lane just registered.

So the stated blocker is dead. What remains is size, which is an honest reason
and a different one. §5 records where this lane actually got to.

### 6.4 `?SetupFileSystem@Modification@IO@@` — "writable but NOT VERIFIABLE". TRUE, and understated.

The verification claim holds: the only unattended mod route is the broken
`ModDialog_ToggleMod_Impl+0x43` one, and this lane did not touch it (correctly —
the fix is an upstream null, it is not this lane's, and the crash evidence is in
`wave17_modsroute_crash.txt`).

What the word "writable" understates is the size. **`0x1800aebb0..0x1800af5e8`
is `0xa38` = 2616 bytes** — larger than `?Update@ParticleSystemInstance@`'s
prologue-to-epilogue span was described as when it was given *an entire lane of
its own*. A row described as "writable but not verifiable" reads as small-and-
cheap-but-unmeasurable; this one is neither small nor cheap. §5 records the
outcome.

### 6.5 The pattern under 6.1 and 6.2

The kickoff's warning is that a handover's *cheap-wins list* is its stalest
section. Both failures here are the mirror image: **the stale claims were in the
"still open, here is why" section, and they overstated the work rather than
understating it.** A wave brief is trusted to know what is NOT done just as much
as what is, and nothing re-derives that half. The census could not have caught
6.1 either — `?Serialize@StringTable@UI@@` is registered as a literal and shows
as ported; the staleness was in a *residual* that nobody re-ran against the
census before promoting it into a lane scope.

**Actionable form, for the Wave 29 integrator:** a residual that names a symbol
should be re-checked against the board at kickoff time with one grep, not
carried into a lane brief on the strength of the wave that wrote it. Two of my
six rows would not have been written into §3 of the kickoff had that grep been
run.

---

## 7. INSTRUMENT DEFECT OBSERVED, NOT FIXED

*(filled in below)*

---

## 8. RESIDUALS, each with an addressee

*(filled in below)*
