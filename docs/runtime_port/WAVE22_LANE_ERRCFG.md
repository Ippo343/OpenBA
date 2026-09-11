# WAVE 22 LANE ERRCFG — `System::Error`, `System::Configuration`, `System::Debug`

Branch `wave22/errcfg`, cut from `wave22/base` at `37706651`. Closed 2026-09-11.

---

## 0. HEADLINE

**Three bodies ported and registered; ZERO rows promoted, deliberately.**
`System 103 called / 78 ported (75.7%)` → **`103 / 81 (78.6%)`**.
`TOTAL 1313 / 1129 (86.0%)` → **`1314 / 1132 (86.1%)`**.

Two findings that outlive the lane:

1. **A single named body, `?LoadResources@UpdateLoop@Engine@@QEAAXXZ`
   (`0x1800dd7d0`), is the last blocker on `?_singleton@Configuration@System@@`.**
   It is one of only two unported in-DLL readers of that static, and the other
   (`?Get@Configuration@System@@SAPEAV12@XZ`) is an eight-byte accessor. Whoever
   wants that DATA row promotable needs exactly that one body. This is a
   *decomposed* blocker, not a namespace gap.

2. **TIMER's rule — "a static's unported in-DLL readers make it unpromotable" —
   is correct for two of my five rows and REFUTED for the other three, on their
   own reader/writer evidence.** The Error trio's whole unported reader set was
   `?Display@Error@System@@SAXXZ`, which this lane ported. Answering each row on
   its own evidence rather than by analogy was the instruction and it changed
   the answer. See §3.

**The denominator for everything below is FIVE DATA rows and THREE code rows,
not the eight-with-six-DATA the brief stated.** The kickoff's §1 list is right at
eight rows; the DATA subset is five (`_errorString`, `_hideDisplay`,
`_stopApplication`, `_singleton@Configuration`, `_disableHandler@Debug`). The
integrator has confirmed the correction.

---

## 1. TASK 0 — THE PRE-WORK GREP, BEFORE ANYTHING WAS WRITTEN

All eight symbols grepped across `runtime/`, `engine/`, `code/`, `standalone/`,
`tools/` and `docs/`. **This was the highest-value twenty minutes of the lane:**
it found that four of the eight were already substantially handled and that one
of my three "bodies" was thirteen lines rather than a Collection lane's work.

| row | found in the tree BEFORE this lane | consequence |
|---|---|---|
| `?Display@Error@System@@SAXXZ` | *not* ported. `runtime/src/system/error.cpp:293` **declines it by name** with a stated reason; `runtime/src/system/allocator.cpp:154` reaches it through a `RtOriginal<>` **raw door** | ported this lane; the raw door had to be converted in the same commit (§5) |
| `?_errorString@Error@System@@0PADA` | `error.cpp:112` `RT_DATA_SLOT`; minted at `rt_data_mint_tables.inc:2244`; read by `code/Game/Game/game_main.cpp:487` | minted, unpromoted |
| `?_hideDisplay@Error@System@@0_NA` | `engine_lifecycle.cpp:289` `RT_DATA_SLOT`; minted `:2251` | minted, unpromoted |
| `?_stopApplication@Error@System@@0_NA` | `error.cpp:115` `RT_DATA_SLOT`; minted `:2276` | minted, unpromoted |
| `?GetCommandLineString@Configuration@System@@…` | *not* ported. Two `RT_EXPORT_ONCE` **call sites** at `filesystem_setupdirs_win.cpp:240` and `:411` | ported this lane; those two call sites move to it |
| `??1Configuration@System@@QEAA@XZ` | *not* ported. One `RT_EXPORT_ONCE` call site at `engine_lifecycle.cpp:981`, inside the ported `Engine::Shutdown` | ported this lane |
| `?_singleton@Configuration@System@@0PEAV12@EA` | five `RT_DATA_SLOT` sites; minted `:2261`; **written by the already-ported `??0Configuration` (`engine_lifecycle.cpp:1099`)** | minted, unpromoted, BLOCKED |
| `?_disableHandler@Debug@System@@0_NA` | `enumerate_video_modes.cpp:112` `RT_DATA_SLOT`; minted `:2243` | minted, unpromoted, BLOCKED |

**Nothing was re-written that the tree already had.** No body among the eight
had an existing implementation; the four DATA rows that looked "done" are
*minted and resolved*, which is not registration — see §2.

**The grep's biggest single payoff.** `??1Configuration`'s two callees,
`0x18000f7d0` and `0x180010ab0`, are **already ported and registered by address**
in `runtime/src/collection/set_clear.cpp:212` and
`runtime/src/collection/array_destroy.cpp:902`. Before the grep this row looked
like six unexported internals plus the allocator lock — the most expensive item
on my board. After it, thirteen lines. *Re-check a holdback's stated blocker
before pricing the work.*

---

## 2. WHY `ported = 0` ON THE FIVE DATA ROWS IS THE CENSUS BEING RIGHT

Recorded because the opposite conclusion is the obvious one and it is wrong.

All five rows are **minted** (`standalone/rt_data_mint_tables.inc`) and all five
are **resolved** by live code through `RT_DATA_SLOT`. Neither is registration.

* `RT_DATA_SLOT` expands to `RtResolveCached(name, &slot)` — a **resolve door**.
  Unpromoted it finds no registry entry, falls through to `GetProcAddress`, and
  returns **the real address inside Runtime-x64.dll**. Our writes therefore land
  in retail's own storage, which is why unported DLL internals reading the same
  address stay correct *by construction*.
* `ported` is `called ∩ registered`, and a minted row only becomes registered
  when it is **promoted** — a `RtMintPromotionStorage(...)` +
  `RegisterRuntimeExport` + `RegisterRuntimeAddr` triple in
  `standalone/reg_mint_promoted.cpp`. None of my five has one.

So `ported=0` here is not a census blind spot. I did **not** find and am **not**
reporting any census defect on these rows.

---

## 3. TASK 1 — THE FIVE DATA ROWS: READER / WRITER EVIDENCE AND VERDICTS

Method: `get_xrefs_to` on each retail address **in Runtime-x64.dll**, full
reference set, READ and WRITE columns separated. Plus a `pefile` import scan of
all four shipped binaries to catch any out-of-DLL consumer. Every verdict is
**route-scoped**.

| row | addr | WRITERS | READERS | DLL-present | cutover |
|---|---|---|---|---|---|
| `?_errorString@Error@System@@0PADA` | `0x1801fc720` | `Error::SetText` **PORTED**, door | `SetText` **PORTED**; `Error::Display` **PORTED BY THIS LANE** | **eligible — conditional, see below** | required |
| `?_stopApplication@Error@System@@0_NA` | `0x1801fc70a` | `SetText` ×2 **PORTED**, door | `Display` **PORTED BY THIS LANE** | **eligible — conditional** | required |
| `?_hideDisplay@Error@System@@0_NA` | `0x1801fc709` | `Engine::Startup` **PORTED**, door; `?HideErrorDisplay@Error@System@@SAX_N@Z` **UNPORTED but DEAD** (§4) | `Display` **PORTED BY THIS LANE** | **eligible — conditional, and rests on the §4 deadness proof** | required |
| `?_singleton@Configuration@System@@0PEAV12@EA` | `0x1801fc6f8` | `??0Configuration` **PORTED**, door; `Engine::Startup` **PORTED**, door | `FileSystem::Startup` **PORTED**; `FileSystem::SetupDirectories` **PORTED**; **`UpdateLoop::LoadResources` UNPORTED**; **`Configuration::Get` UNPORTED** | **NOT promotable — HARMFUL** | required |
| `?_disableHandler@Debug@System@@0_NA` | `0x1801fc708` | `EnumerateVideoModes` ×2 **PORTED**, door; **`Debug::DisableExceptions` UNPORTED** | **`Debug::ExceptionHandler` UNPORTED** (also writes) | **NOT promotable — HARMFUL** | required |

### 3.1 The two HARMFUL rows, and why HARMFUL not NO-OP

The distinction matters (`reference_unpromotable_is_route_scoped_not_permanent.md`):
a row can be pointless to promote, or actively damaging. Both of mine are the
latter, and the damage is silent.

* **`?_singleton@Configuration@System@@`** — `?LoadResources@UpdateLoop@Engine@@`
  (`0x1800dd7d0`) reads it at `0x1800ddbe0` through an in-DLL RIP-relative load,
  which **is not a door and cannot be intercepted**. Promoting sends our ported
  `??0Configuration`'s write into mint storage and leaves that reader on a DLL
  slot nothing writes. `LoadResources` is on the **live game-load path**, so the
  result is a null configuration pointer during a real load, not a theoretical
  one. `?Get@Configuration@System@@SAPEAV12@XZ` (`0x18000da70`, eight bytes)
  has the same shape.
* **`?_disableHandler@Debug@System@@`** — `?ExceptionHandler@Debug@System@@`
  reads it at `0x1800c5b6f` and writes it at `0x1800c5b9b`/`0x1800c5bee`, and
  `?DisableExceptions@Debug@System@@SAX_N@Z` writes it at `0x1800c5870`. Both
  unported, both in-DLL RIP-relative. Promoting splits the flag: our
  `EnumerateVideoModes` suppression would write our copy while retail's
  top-level handler kept reading its own, **silently un-suppressing the handler
  our port had asked to disable**. Nothing would report it.

This is exactly TIMER's finding on its five `Timer` statics, reached
independently on different symbols.

### 3.2 The three Error rows — WHERE TIMER'S RULE DOES NOT REACH, AND THE PREMISE THIS RESTS ON

> ⚠ **THIS IS A CONDITIONAL CLAIM AND ITS PREMISE IS MY OWN WORK.**
> The eligibility of `_errorString`, `_stopApplication` and `_hideDisplay`
> rests entirely on **`?Display@Error@System@@SAXXZ` being ported, registered,
> and reading/writing exactly the three statics enumerated above and nothing
> else.** That body is §5 of this document. The lane that supplies the premise
> is the lane scoring the conclusion, which is a self-check shape, so:
>
> **WHAT FALSIFIES IT.** (a) `?Display@` turns out to read or write a fourth
> exported static I did not enumerate — my enumeration is `get_xrefs_from` on
> `0x1800c6060`'s body plus a full read of its 906 bytes, and a reviewer should
> redo it rather than trust it. (b) `?Display@` is reverted or its registration
> is gated off. (c) The `?HideErrorDisplay@` deadness proof in §4 is wrong —
> that one affects `_hideDisplay` only. **If any of those holds, these three
> verdicts revert to NOT promotable and this section is the record of why.**

With that stated: for `_errorString` and `_stopApplication` the complete in-DLL
reference set is `SetText` and `Display`. Both now reach the static through
`RT_DATA_SLOT`, i.e. **through a door our registry can move**. There is no
in-DLL RIP-relative reader left to strand. That is a genuinely different
situation from TIMER's rows and from my own other two, and it was only visible
because each row was answered on its own xrefs.

**I did not promote them anyway.** See §7.

### 3.3 `game_main.cpp:487` — which storage that read lands on

The brief asked this specifically. `code/Game/Game/game_main.cpp:487` reads

```cpp
const char* err = reinterpret_cast<const char*>(
    ResolveAddr(Module::Runtime, 0x1801fc720));
```

`ResolveAddr(Module::Runtime, …)` **consults the port registry before the DLL**
(`reference_resolveaddr_consults_port_registry_first.md`). Unpromoted there is
no registration at `0x1801fc720`, so **today it falls through and lands on the
DLL's real 0x2000-byte buffer** — the same storage `SetText` writes through
`RT_DATA_SLOT`. The two agree today.

It is also **not** the "minted copy nothing publishes" hazard: the promotion
triple includes `RegisterRuntimeAddr(0x1801fc720, st)`, so this reader *follows*
a promotion rather than being stranded by it. The address door is the thing that
makes it safe, and it is already there.

### 3.4 A STALE BLOCKING WARNING, DISCHARGED

`runtime/src/system/error.cpp` §2 carried a ⛔ header: *"This port must not be
armed at cutover until lane C2 fixes one table row"*, because
`rt_data_mint_tables.inc` minted `?_errorString@` at **8 bytes** for a
**0x2000-byte** buffer. **That fix landed.** The row today reads
`8192` with a description naming the array (`rt_data_mint_tables.inc:2244`).

The warning has been rewritten in place to say it is discharged, by whom, on
what date, and quoting the current row — not deleted, because the reasoning is
the evidence for the fix. `BANISHED_RT_ERROR=0` is no longer required for that
reason.

**This is the mirror of the defect lane GEOM is paying off this wave:** there a
*prescription* was left reading as a *completion*; here a *completion* was left
reading as a *prescription*. A blocking warning that no longer applies is worse
than no warning, because the next lane obeys it and stops.

---

## 4. `?HideErrorDisplay@Error@System@@SAX_N@Z` IS DEAD — FOUR SCREENS, EACH WITH A POSITIVE CONTROL

`_hideDisplay`'s one unported writer. `0x1800c5f90`, seven bytes
(`MOV byte [rip],CL ; RET`), a public export.

A screen looking for the **absence** of a shape has no natural positive case, so
"0 hits" and "dead parser" are indistinguishable
(`reference_absence_screen_must_be_handed_a_shape.md`). Every screen below was
therefore run against a **known-live control** in the same invocation.

| screen | SUBJECT `HideErrorDisplay` | CONTROL `?SetText@` (live) | CONTROL `?Display@` (live) |
|---|---|---|---|
| Ghidra `get_xrefs_to` on the entry | export-directory entry + entry-point sweep, **nothing else** | real call sites | 2 call sites + 5 data refs |
| `pefile` import scan, all 4 binaries | 0 of 899 exe / 0 of 163 DX11 / 0 of 146 DX9 | present in DX11 + DX9 | present in **all three** |
| pointer-valued byte scan (8-byte VA and 4-byte RVA, raw file, decoder-independent) | **1** RVA hit in the DLL = its export-address-table entry; 0 everywhere else | 2 in the DLL | 5 in the DLL |
| mangled-name string scan (the `GetProcAddress`-by-name route) | **1**, the DLL's own export-name table; 0 in exe/DX11/DX9 | — | **1 in each of the four** |

Every control returned a nonzero, differentiated answer, so no screen was a dead
parser.

**COVERAGE — what this proves and what it does NOT.**
* It covers: direct `rel32` calls (screen 1), PE imports (2), function pointers
  stored in vtables, jump tables or any other data as a VA or an RVA (3 — and
  screen 3 is decoder-independent, so it does not depend on Ghidra having
  disassembled anything correctly), and runtime lookup by name (4).
* It does **not** cover: an address computed at runtime from a base plus a
  non-literal offset, or an ordinal-only `GetProcAddress`
  (`GetProcAddress(h, MAKEINTRESOURCE(n))`) where the ordinal never appears as a
  recognisable constant. I did not screen for either. I judge both implausible
  for a one-instruction setter in a C++ engine, but that is a judgement, not a
  measurement, and it is the gap in this proof.

Note this is a stronger position than a source grep can reach: a source grep
cannot tell an unexported internal from an export nobody has ported yet
(`reference_grep_cannot_tell_internal_from_unported_export.md`). These four
screens run against the **shipped binaries**, so that ambiguity does not arise.

`?HideErrorDisplay@` was **not** ported: porting a dead export grows `called`
for nothing.

---

## 5. TASK 2 — THE THREE BODIES

### 5.1 `??1Configuration@System@@QEAA@XZ` — `0x1800c5170`, ord 556

Thirteen instructions. **Name verified with `pefile` against the export
directory before writing:** `??1` is the destructor and **`QEAA` is the
NON-VIRTUAL spelling**; `??1Configuration@System@@UEAA@XZ` does not exist in
this DLL and would have resolved NULL in silence
(`reference_phantom_dtor_mangling_qeaa_vs_ueaa.md`).

```
1800c5186  ADD  RCX,0x10
1800c518f  CALL 0x18000f7d0     ; Set<Pair<String,String>>::Clear(this+0x10)
1800c5195  MOV  RCX,RBX
1800c519d  JMP  0x180010ab0     ; ~Array(+elem)(this)       -- TAIL CALL
```

**Both callees were opened and read, not taken from the recipe.**
`reference_a_written_recipe_can_be_wrong.md` records a wave that ported two
dtors exactly as prescribed and swapped a body that destroys **every** element
for one that destroys only the **first**. So:

* `0x18000f7d0` loads `[this]`, and on non-null calls `0x18000f890` **twice** —
  once with `[node+0x18]` and once with `[node+0x20]` — i.e. it recurses down
  **both** subtrees, then frees the node and zeroes `{ptr,int}`. Whole
  container.
* `0x180010ab0` reads the **count** from `[this+8]` and the base from `[this]`,
  hands both to `0x180011010` (the per-element loop), frees the block under the
  allocator lock, zeroes `{ptr,ptr}`. Whole array.

Both are already ported and registered by address in this tree, so
`ResolveAddr(Module::Runtime, …)` reaches **our** bodies today and at cutover.
**This port adds no new dependency on the DLL.**

**A retail leak, reproduced.** `??0Configuration` initialises **three**
`{pointer,int}` pairs — `+0x00`, `+0x10`, `+0x20`. This dtor destroys `+0x10`
and `+0x00` and **leaves `+0x20` alone**. Same family as the two leaks already
flagged inside `Engine::Shutdown`. Not tidied: freeing a block retail still
considers live is strictly worse than the leak.

**Oracle: none, and none manufactured.** A destructor's whole output is that its
subject is gone; a replay is a double free, not a comparison. It gets the
call-site counter instead — `Engine::Shutdown` reaches it through
`RT_EXPORT_ONCE`, a door, so registration moves that call to this body.

### 5.2 `?GetCommandLineString@Configuration@System@@QEBA?AVString@2@AEBV32@@Z` — `0x1800c5300`, ord 2495

**The hidden return slot is a real ABI parameter and retail says so on its first
three instructions.** `RCX = this`, **`RDX = String* ret`**, `R8 = const
String& key`; `MOV [RSP+0x10],RDX` / `MOV RSI,RDX` / `MOV RDX,R8`, and the
function returns `RAX = RSI`. An `fn_*` typedef omitting the slot would put
`key` in RDX, hand the find a `String*` that is really a `String**`, and write
the answer through whatever the caller left in R8 — **a wild write that still
returns the right answer**, because RAX is correct either way
(`reference_hidden_retslot_wild_write.md`). The typedef spells all three.

Semantics: find the node, take a reference on its value into a temporary, zero
the return slot, adopt the value with a second reference, release the temporary.
Net `+1`. The transient `+2` is written out rather than folded, because those
are the two arms retail compiled.

Retail's destructor-state byte at `[RSP+0x20]` makes each arm load the *other*
arm's uninitialised stack slot into RCX/RBX before the common tail. That is not
a bug and is **not** transcribed — the guarding state bit is clear in the arm
that did not create the temporary, so the garbage is never dereferenced.
Modelled as one `temp`, which is what the state machine computes.

New fact recorded in the tree: **`MapNode+0x08` is the VALUE's `String::data`**,
pinned by `MOV RCX,[RAX+0x8]` at `0x1800c533b` immediately followed by
`INC dword [RCX-0x4]` at `0x1800c534c` — a String refcount bump, so the qword is
a `String::data`, not an arbitrary pointer. Added to `MapNode` with that
`static_assert` citation; the remaining eight bytes of the node head stay
undescribed.

**Its one non-trivial callee was already ported**: the map find `0x18002ace0`
(`RtSystem_ConfigMap_Find`, registered in the same file).

### 5.3 `?Display@Error@System@@SAXXZ` — `0x1800c6060`, ord 2221

906 bytes, read instruction by instruction. `_hideDisplay` guard → zero a
`char[0x800]` and a `uint16_t[0x800]` → two clamped appends (the fixed prefix,
then `_errorString`) → UTF-8→UTF-16 → nine-byte caption `"Banished"` →
UTF-8→UTF-16 → `MessageBoxW(nullptr, wtext, wcap, MB_ICONERROR)` →
`if (_stopApplication) exit(-1)`.

Both literals were **read out of the binary with `read_memory`**, not retyped
from a decompiler rendering: `0x180155dc0` is the 64-byte
`"A fatal error has occurred and the application cannot continue.\n"`, and the
caption is the qword at `0x180154858` plus the single byte at `0x180154860` —
retail copies exactly nine bytes, not a `strcpy`. `0x18014b540` resolves to
`EXTERNAL MessageBoxW` via `get_xrefs_from`.

**The clamp unit is BYTES and it is not the buffer size.** Both conversion loops
accumulate the encoder's byte count and stop at `0x800` / `0x40` while the
destinations hold `0x1000` / `0x80` bytes. Retail is conservative by 2× and that
is reproduced, not "corrected" — the bound is observable as the point a long
error string truncates. The terminator is written only when `total + 4` is still
inside the bound, so a message that exactly fills the bound is left
**unterminated in a buffer that was zeroed up front**, which is why the zeroing
is load-bearing and is not elided.

**Not lifted from retail:** the two codec helpers. The DLL's dispatchers
(`0x1800c9fc0` decode, `0x1800c9b60` encode) are jump tables Ghidra cannot
recover as functions ("Too many branches") — the same wall Wave 2 and Wave 9 hit
on the same pair — so they are written from the Unicode specification, exactly
as `runtime/src/system/widestring.cpp` already discloses, and copied verbatim
from that canonical version with the disclosure attached.

---

## 6. TASK 3 — VERIFICATION: WHAT WAS DONE, AND WHAT WAS NOT

**Done.**
* `mingw32-make standalone-debug` — **clean, exit 0**, `reimpl-x64.exe` linked.
* `tools/runtime_census.py` — System `103/78` → `103/81`; TOTAL `1313/1129` →
  `1314/1132`.
* `tools/dispatch_slot_map.py --check` — **110 registrations, 0 mismatches, 0
  double-registered addresses**.
* `tools/port_bypass_audit.py` — 33 `BYPASS` lines, **none from
  `allocator.cpp`, `error.cpp` or `configuration.cpp`**. Checked by grepping for
  the SET (`?Display@Error`, and each of my three files by name), not by
  comparing the count — `reference_diff_the_set_not_the_count.md`.
* `tools/export_name_audit.py` — 3 phantom names, all three **pre-existing** and
  in files I did not touch (`reg_mint_promoted.cpp` ×2,
  `rt_resolve_cache.cpp`). All three of my new mangled names were verified
  against the DLL export directory with `pefile` **before** a line was written.

**`called` grew by 1, and that is mine.** `??1Configuration` reaches retail's
CRT `exit` at `0x18007f8a8` by address, which is a new address-keyed door in the
`internal` bucket. *Porting can grow the cutover surface*
(`reference_porting_can_grow_the_cutover_surface.md`) — flagged rather than left
for someone to find as an unexplained `+1`.

**NOT DONE — say it plainly.**
* **NOTHING WAS RUN.** No `sa-run`, no `sa-stress`, no diff-oracle execution,
  no `calls=` column read. ALLOC and GEOM owned the game directory this wave and
  the plan explicitly queues my verification behind them. `tools/game_dir_lock.sh`
  was therefore never acquired and never piped.
* Consequently **all three bodies are TRANSCRIBED AND UNVERIFIED AT RUNTIME.**
  A clean build and a green census are not a verification and a green readiness
  screen is not a route.
* Two of the three have **real differential oracles already written into the
  source**, waiting only for a process:
  * `GetCommandLineString` — a pure read of the map, so **no snapshot is needed**
    (unlike `ParseValue`, which has an aliased in/out `pos`, and unlike
    `SetCommandLine`, where a replay is a double insert). The shipped body is
    run into a second `String` and the two `data` **pointers** are compared —
    as pointers, not as text, which is sharper here precisely because neither
    side allocates: both must return the identical buffer from the map. A text
    comparison would pass on two different nodes holding equal strings, which is
    the exact failure the ported `TreeStrCmp` had once already. An effectiveness
    control counts the **hits**, so a run where every lookup missed reports
    `work=0` rather than reading green.
  * `Display` — described below, **not built**.
  * `??1Configuration` — no oracle is possible and none was manufactured.
* **A `Display` oracle is possible and I did not build it.** Set `_errorString`
  via `SetText`, call ours with `MessageBoxW` intercepted, capture
  `(wtext, wcap)`, call the shipped body, capture, compare the two wide buffers
  byte for byte. That is a genuine oracle; it needs a process and an intercept
  point. Recorded as buildable work, **not** as "no oracle possible" — which has
  been wrong four times in this project.
* I did **not** verify that `?Display@`'s in-DLL reference set is limited to the
  three statics I enumerated by any route other than reading its 906 bytes and
  `get_xrefs_from`. §3.2 names this as the falsifier.
* I did **not** examine `?LoadResources@UpdateLoop@Engine@@` beyond establishing
  that it reads `_singleton@Configuration` and is unported. I have not costed it.
* I did **not** run `find /` or any whole-filesystem scan; all searching was
  `Grep`/`grep` scoped to the worktree, or `pefile` against the four named
  binaries.

---

## 7. ZERO ROWS PROMOTED — A DECISION, NOT AN OMISSION

Two rows (`_errorString`, `_stopApplication`) became eligible during this lane
and a third (`_hideDisplay`) became eligible conditional on §4. **None was
promoted.**

The reason is the `?_singleton@SaveLoad@Save@@` precedent recorded at
`standalone/reg_mint_promoted.cpp:560-660`, which deferred an eligible promotion
for the length of its wave and was right to: *promoting mid-wave makes every
sibling lane carry a configuration resting on ONE lane's static proof, and if
that proof is wrong somebody else eats the crash and bisects onto this row, mid
wave.* My proof is static, it is fresh, and its premise (§3.2) is my own
unverified body. That is the worst possible moment to make four other lanes
carry it. Lane TIMER independently reached zero promotions on its own evidence.

### 7.1 HANDOVER ITEM — ADDRESSED BY NAME

> **[ADDRESSEE: the FIRST Wave 23 lane to be dealt any `System::Error` row, or —
> if Wave 23 deals none — the Wave 23 INTEGRATOR, who is to assign it
> explicitly.]**
>
> **ACTION.** Promote `?_errorString@Error@System@@0PADA` and
> `?_stopApplication@Error@System@@0_NA` in
> `standalone/reg_mint_promoted.cpp`, using the triple at that file's
> `?_singleton@SaveLoad@Save@@` row as the template
> (`RtMintPromotionStorage` + `RegisterRuntimeExport` + `RegisterRuntimeAddr`,
> addresses `0x1801fc720` and `0x1801fc70a`). Then consider
> `?_hideDisplay@Error@System@@0_NA` at `0x1801fc709`.
>
> **PRECONDITION, WHICH MUST BE CHECKED AND NOT ASSUMED.**
> `?Display@Error@System@@SAXXZ` must still be ported and registered in
> `runtime/src/system/error.cpp`, and a reviewer must re-run `get_xrefs_to` on
> each address to confirm no NEW unported in-DLL reader has appeared. If
> `?Display@` has been reverted or gated, **do not promote** — §3.2 lists the
> falsifiers and this item is void.
> For `_hideDisplay` additionally: §4's deadness proof for
> `?HideErrorDisplay@Error@System@@SAX_N@Z` must still hold.
>
> **EXPIRY.** This item expires when Wave 23 closes, whether or not it is acted
> on. If it is not acted on, the Wave 23 integrator **re-points it by name at a
> Wave 24 addressee in the closing document, or deletes it and says so.** It
> does not roll forward silently.

> **[ADDRESSEE: whichever Wave 23 lane is dealt `Engine`, or the Wave 23
> integrator if none is.]**
>
> `?LoadResources@UpdateLoop@Engine@@QEAAXXZ` (`0x1800dd7d0`) and
> `?Get@Configuration@System@@SAPEAV12@XZ` (`0x18000da70`, eight bytes) are the
> **complete** set of unported in-DLL readers of
> `?_singleton@Configuration@System@@0PEAV12@EA`. Port both and that DATA row
> becomes promotable; port neither and it stays HARMFUL. **Same expiry rule.**
>
> I have **not** costed `LoadResources`. It is a `UpdateLoop` method at
> `0x1800dd7d0`–`0x1800ddd1f` (1,360 bytes) and should be priced before being
> scheduled.

---

## 8. FILES CHANGED

| file | change |
|---|---|
| `runtime/src/system/configuration.cpp` | `+?GetCommandLineString@`, `+??1Configuration@`, `MapNode::value` at `+0x08` with its `static_assert` citation, both registrations (name **and** address) |
| `runtime/src/system/error.cpp` | `+?Display@Error@System@@SAXXZ` and its helpers; §2's stale ⛔ blocking warning **discharged in place** with date and evidence; the "deliberately absent" note for `?Display@` replaced and a new one added for the dead `?HideErrorDisplay@` |
| `runtime/src/system/allocator.cpp` | `?Display@` raw door `RtOriginal<>` → `ResolveExport` — **required by**, and committed with, the `?Display@` registration that would otherwise have made it a live bypass |
| `docs/runtime_port/WAVE22_LANE_ERRCFG.md` | this document |

No shared table, generated file or checker was edited.
