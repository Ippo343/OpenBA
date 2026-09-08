# Lane RETSLOT — the 234 exe `.text` dispatch-slot typedefs

Closes the subclass of the hidden-return-slot ABI audit that neither
`tools/abi_retslot_audit.py` nor `tools/retslot_slot_join.py` can reach.

`engine/funcs/engine_funcs.cpp` has **254** `fn_* = … ResolveAddr(Module::Exe, 0x…)`
assignments. 20 are PE import slots (`0x14015xxxx`), which `retslot_slot_join.py`
resolves to a mangled name and already closed. The other **234 are direct exe
`.text` addresses** — the game's own code, which the exe does not export, so there
is no name to classify and the join answers "NOT AN IMPORT SLOT". Those 234 are
this directory's corpus.

## What a defect is

A function returning a class/struct **by value** takes a hidden pointer to the
return slot as an implicit leading argument (MS x64, which MinGW GCC follows). A
typedef that omits it makes every call write the returned object through whatever
happened to be in the next register. Both `SpriteContext::SetScissor` typedefs did
exactly that until lane FLUSH fixed them at `4281428`.

## The corpus is provably whole

Two independent methods, both in this directory, agree on 254 with an empty
symmetric difference:

* `extract.py` — regex anchored on `fn_`, DOTALL so multi-line assignments match.
  Every `ResolveAddr(Module::Exe` occurrence in the file is either inside a matched
  assignment or printed as residue (`-v`), so an invisible site cannot hide.
* `residue_check.py` — no `fn_` anchor at all: strip comments, split on `;`,
  bucket every statement by its left-hand side.

`extract.py` also resolves the **17 rows whose variable is declared with a SHARED
typedef under another name** (`extern fn_Array_Resize_t fn_Array_Resize_Stride08;`).
Looking only for `fn_NAME_t` reported those as "no typedef found", i.e. as a hole.

## Why there are two screens, and what each one misses

A hidden return slot is an extra leading pointer argument, so it cannot be
invisible to **arity**. But no single arity source here is trustworthy:

* **Screen 1** (`screen1_ghidra_nparams.py`) — Ghidra's parameter count. 231 of the
  234 signatures are `SignatureSource.ANALYSIS`, and Ghidra declared **zero**
  struct-by-value returns across the whole corpus, so "no STRUCT return ⇒ clean"
  would have been a sweep that says nothing. Its count is wrong in both directions:
  it over-counts (four rows where a scratch register or a home-slot spill was read
  as a parameter) and under-counts (seven dialog ctors called 1-parameter whose call
  sites set four registers and up to three stack arguments).
* **Screen 2** (`screen2_callsite_regs.py` + `AllArity.java`) — what retail's own
  compiler emitted: the highest MS-x64 argument register written on the run-up to
  each call site. It does not consult Ghidra's signature at all. It cannot see a
  missing **5th-or-later** argument, because those go on the stack and the register
  count does not move.

**Positive control.** Three correct typedefs had their return-slot argument deleted
in a throwaway copy of `engine_funcs.h` and the corpus was re-run:

| injected defect | screen 1 | screen 2 |
| --- | --- | --- |
| `fn_Building_GetOrderName` 2 → 1 | fires | fires |
| `fn_Entity_GetPosition` 5 → 4 | fires | **misses** (arg 5 is a stack arg) |
| `fn_ResidenceComponent_GetSpawnPoint` 5 → 4 | fires | **misses** (same) |

3/3 for the union, and no other row moved. The screens are run together for that
reason; neither is sufficient alone.

## Result at `fbf147a`

* Class (a), by-value class return with no hidden slot in the typedef: **0**.
  Not one of the 234 typedefs even declares a struct return, and the 40 rows whose
  Ghidra return type is a pointer to a named type (`Vector3 *`, `String *`,
  `Geometry *` … — what a by-value return looks like to an analyser that was not
  told about the slot) all already carry an explicit `retval`/`out…` argument.
* Class (b), other arity/register mismatch: **1**, `fn_Game_TriggerDisasterByType`.

The archived `corpus_*.csv`, `ghidra_dump_*.tsv` and `callsite_arity_*.tsv` are the
exact inputs behind those numbers.
