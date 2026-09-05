# Unported exe `.text` functions — shared brief

Session 2026-09-04, branch `wave/upgrade-path-complete`, follows `f119f3b`.
**Read this fully before touching your lane.** Your lane list is
`docs/waves/lanes/code_lane<N>.txt`.

## What this wave is

49 addresses in the exe's `.text` range that `ResolveAddr(Module::Exe, addr)`
still misses, so every call to them lands on `NotImplementedStub_impl` and
**returns 0**. Unlike the dispatch-slot band we just closed, these have no PE
import name — each one needs a Ghidra decompile to identify.

The list is the union of `docs/waves/lanes/held_back_code_fns.txt` (49) and the
`BANISHED_AUTOLOAD=Auto` census (48, a clean subset). Addresses tagged
`LOAD-WINDOW` are resolved during load/postload and are the highest-value ones:
a null returned there can silently shorten the save stream. `not-hit-on-the-autoload-run`
means the older tutorial census saw it but this run did not.

## Your deliverable is a TRIAGE REPORT first, code second

The single most important lesson from the previous two waves on this list:

> Of 18 rows audited, only 6 were real gaps; **5 would have caused ACTIVE HARM
> if implemented as written.**

So do not assume an unregistered address is a bug. For each address in your lane,
decide and report one of:

- **`port`** — a real gap. Our callers want this function and get 0 instead.
- **`deliberate — leave`** — a caller null-guards and takes a fallback path on
  purpose, or the function is genuinely a no-op for us. Say which call site.
- **`dead`** — nothing actually calls it (see the `.pdata` trap below).
- **`needs a run to decide`** — you cannot settle it statically. Say what run.

A well-argued `leave` with the call site quoted is worth as much as a port. A
guessed port is worth less than nothing.

## Method, per address

1. `get_function_by_address(address="0x...")` then `decompile_function`.
   The exe is already the current Ghidra program — see the concurrency rule below.
2. `get_function_xrefs` / `get_xrefs_to` to find retail's callers. That tells you
   what the function is FOR, which is usually more informative than its body.
3. **Grep the ADDRESS across `code/`, `engine/`, `standalone/`, `runtime/`** to
   find OUR consumers. Read every one before deciding.
4. Classify the consumer:
   - **null-guard-and-SKIP** → registering RESTORES retail. Usually `port`.
   - **null-guard-and-FALLBACK** → registering CHANGES behaviour. Argue for it or
     leave it.
   - **unguarded call** → currently an EXEC-of-0 waiting to happen. High priority.
5. If you port it, write the reimpl in **your own file** (below) and say exactly
   which one-line `RegisterExeAddr` the integrator must add.

## Traps that have burned this project before

- **Grep the retail ADDRESS, not our `fn_*` name.** 8 of 18 of our own `fn_*`
  names were found to be misnomers. A name match is not evidence.
- **A lone `[DATA]` xref inside `.pdata` (`0x1401e1000`-`0x1401f33ff`) means DEAD
  CODE, not "called from a table".** That xref is the function's own unwind
  record. "It has a real body" is not evidence anything calls it.
- **An unregistered address is sometimes DELIBERATE.** Prior sessions left slots
  null so a dispatch-or-fallback path takes the fallback. Registering silently
  changes behaviour.
- **A skipped serializer call is a stream desync our own save/load round trip can
  NEVER detect**, because both legs skip the same bytes. If your function is
  reached from a Serialize path, treat a `leave` verdict with extra suspicion.
- **A stub's "why this is stubbed" comment outlives the blocker it names.** If you
  find a comment explaining why something is skipped, verify the claim still
  holds rather than trusting it. Two stale comments were corrected in this branch
  this session alone.
- **Do not "fix" a faithful crash with a null check.** If a byte-faithful port
  crashes on an unguarded deref, something upstream handed us a null — find that.
  See CLAUDE.md.

## Working rules — SIX LANES SHARE ONE TREE

- **Do NOT build.** One object dir; a parallel `mingw32-make` corrupts it. The
  integrator builds centrally.
- **Do NOT run the game.** One Banished directory, one process, shared logs.
- **Put all your code in your own new file** `code/Game/<Module>/unreg_lane<N>_*.cpp`
  or, for registration-only work, `standalone/reg_code_lane<N>.cpp`. Tell the
  integrator the one line needed to call your init.
- **Do NOT edit shared files** — `standalone/address_registry.cpp`,
  `engine/funcs/engine_funcs.{h,cpp}`, anything already under `code/Game/` that
  you did not create. If your fix needs one, describe the exact edit in your
  report and the integrator applies it serially.
- **Ghidra concurrency: do NOT call `switch_program`.** It changes global state
  for all six lanes at once. Pass `program="Runtime-x64.dll"` as a parameter on
  the individual call instead. The exe is the current program; leave it that way.
- Comment every registration the way the existing ones do: what was broken with
  it unregistered, and the evidence. A bare address+name line is not enough.

## Report back

A table: address, what the function IS (name it), verdict, the call site you
read, and what changes if it is registered. Then the diff summary of your own
file. **Flag anything you could not settle rather than guessing.**
