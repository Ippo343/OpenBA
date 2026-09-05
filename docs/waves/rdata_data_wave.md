# `.rdata` / `.data` census wave — shared brief

Session 2026-09-04, branch `wave/upgrade-path-complete`, follows `ccadef7`.
**Read this fully before touching your lane.** Lane list:
`docs/waves/lanes/w2_lane<N>.txt`.

## What this wave is, and what it is NOT

The census stands at 55 unregistered addresses in four bands:

| band | count | status |
|---|---|---|
| exe `.text` functions | 18 | **fully triaged — do not re-open** |
| PE import slots | 20 | **fully triaged — do not re-open** |
| `.rdata` vtable/const | 12 | **THIS WAVE — never examined** |
| `.data` globals | 5 | **THIS WAVE — never examined** |

The `.text` and import bands persist in the census because a `dead` or
`deliberate-leave` verdict does not remove a row — the `ResolveAddr` still runs
and still misses. Their verdicts are in `docs/waves/code_fns_wave.md`. **If your
work leads you to one of those addresses, read the existing verdict rather than
re-deriving it.**

Your 17 addresses have never been triaged by anyone.

## These are NOT functions — that changes the method

A `.text` address is a function; you decompile it. **An `.rdata` or `.data`
address is storage.** `ResolveAddr` returning 0 means our code asked for the
address of a vtable, a constant, or a global and got null. The failure modes are
different and often quieter:

- **A vtable** (`.rdata`): our code reads `vt[N]` off a null base — an immediate
  fault, or worse, reads a slot from address `0 + N*8` if something offsets first.
- **A float/int constant** (`.rdata`): the consumer falls back to a hardcoded
  literal. **This is the dangerous one.** A wrong constant produces no crash and
  no trace — it just makes the game subtly wrong. A prior wave found
  `0x1401720f4` = 0.15f where the fallback used 1.0f: a **6.7x divisor error**,
  silent.
- **A type-id storage slot** (`.data`): reads as 0. A type id of 0 used as a
  FILTER matches nothing, so a count is a hard 0 while every visible mechanism
  looks healthy. Found twice before.
- **A singleton pointer** (`.data`): every read through it is null.

So for each address, the first question is **what KIND of storage is this**, and
the second is **what does our consumer do with a zero**.

## Method, per address

1. In Ghidra: `get_xrefs_to` the address to see how retail uses it, and
   `inspect_memory_content` / `analyze_data_region` / `list_data_items` to see
   what actually lives there. For a vtable, read the slots. For a constant, read
   the VALUE — you will likely need it.
2. **Grep the address across `code/`, `engine/`, `standalone/`, `runtime/`** and
   read every one of our consumers.
3. Classify what a zero does at OUR call site:
   - **hardcoded-literal fallback** → compare the literal against the real value
     in the binary. If they differ, that is a silent live defect — report the
     measured value.
   - **null-guard-and-SKIP** → registering restores retail.
   - **null-guard-and-FALLBACK** → registering CHANGES behaviour; argue for it.
   - **unguarded read** → a live fault or a silent zero. High priority.
4. Verdict: `register` / `deliberate — leave` / `dead` / `needs a run to decide`.

## Hard-won rules — every one of these cost this project real time

- **Weight a row by what its CALL SITE does, never by its census tag.**
  `NotImplementedStub_impl` fires inside `ResolveAddr` on a resolution MISS, not
  when the null is used. `first=load` means only "the one-time resolver that
  fetches this ran during load". `total=N` is a RESOLVE count, **not a call
  count**. This misled two lanes last wave.
- **Grep the ADDRESS, not our `fn_*`/`g_*` name.** Our names are frequently
  misnomers — last wave a "QuickSave handler" was `TakeScreenshot`, a
  "Toolbar_ToggleAll" was `OpenPauseMenu`, an "AmbientSound::Update" was
  `MusicPlayer::Update`. A name match is not evidence.
- **A vestigial resolve masquerades as a gap.** If the only thing touching an
  address is one `ResolveAddr` populating a pointer/global that is **never read**,
  it is dead: the resolve itself manufactures the census row. Verify with:
  exactly 3 source mentions (defn / resolve / decl), no dereference, single
  `ResolveAddr` site. Ruling: **zero consumers → recommend DELETE; any real
  consumer, including `_DEBUG`-only or feature-gated → REGISTER.**
- **An unregistered address is sometimes DELIBERATE**, so a dispatch-or-fallback
  path takes the fallback. Registering silently changes behaviour. Last wave 7 of
  ~18 leaves would have caused ACTIVE HARM if registered.
- **A lone `[DATA]` xref inside `.pdata` (`0x1401e1000`-`0x1401f33ff`) is DEAD
  CODE, not a caller** — it is the function's own unwind record.
- **A stub's "why this is stubbed" comment outlives the blocker it names.**
  Verify the claim; three stale comments were corrected in this branch this week.
- **Do not "fix" a faithful crash with a null check** (CLAUDE.md). The exception
  is where our standalone genuinely differs from retail — say so in the comment.

## Working rules — LANES SHARE ONE TREE

- **Do NOT build.** **Do NOT run the game.** The integrator does both centrally.
  (You may use `g++ -fsyntax-only` — it writes no object files.)
- **Own file only**: `standalone/reg_w2_lane<N>.cpp`, or
  `code/Game/<Module>/w2_lane<N>_*.cpp` for a reimpl. Tell the integrator the one
  line needed to call your init.
- **Do NOT edit shared files** (`standalone/address_registry.cpp`,
  `engine/funcs/engine_funcs.{h,cpp}`, existing `code/Game/` files). Describe the
  exact edit in your report; the integrator applies it serially.
- **Ghidra: do NOT call `switch_program`** — global state shared with the other
  lanes. The exe is current; pass `program="Runtime-x64.dll"` per call instead.

## Report back

A table: address, what the storage IS, verdict, the call site you read, and what
changes if registered. **For any constant, give the measured value AND the
fallback literal our code uses.** Flag what you could not settle rather than
guessing. Do not paste decompiler dumps.
