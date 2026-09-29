# W1 brief — bodies by class (read after `BLOCK_BRIEF.md` and `../B4_RULES.md`, corrections §8 included)

Your block is a row of `B4_FILEMAP.md`; your files are the rows of `data/filemap.tsv` with your block
name (`grep -P '\t<BLOCK>\t' docs/runtime_port/endgame_b/b4_session/data/filemap.tsv`). You own those
files and the class headers among them. Everything else is read-only for you.

## What you do, per class in your files
1. **Header first** (H1-H8): every field your bodies read is declared with RETAIL's type and
   asserted; pointer members typed; guess names only where retail gives no evidence.
2. **Bodies** (B1-B13): a C body becomes the member retail compiled (class and slot from retail,
   never from the body's name); its receiver is `this`; every field read is by NAME; callers INSIDE
   your files change to `p->Member(…)`.
3. **Reads of other classes** in your bodies: if the class has a finished header (W0's twelve, the
   runtime classes, a class another block already converted on main), read by name through a typed
   pointer. The typed pointer comes from a typed SOURCE (a typed member, `FindComponent<T>()`, a typed
   parameter). Where the source is still `void*` / `uintptr_t` because its owner is another block:
   LEAVE the read as it is and list it. Do not add a cast to reach a name.
4. **Floor** casts in your files get `// CAST-OK: <category>` (M2, `FLOOR.md`); tier I is exempt by
   shape. Anything else that stays is LEFT and listed with its reason.
5. File-local offset helpers (`At`, `Field`, `P64`, `Rd`, …) die when their last call in the file
   is converted.

## Callers outside your files
A caller of your body in a file you do not own keeps compiling: while such callers exist the C
declaration STAYS in `exe_bodies.h` / its `*_funcs.h` as a one-line forwarder to the member, defined
in your file, marked `// B4 FORWARD: <n> outside caller(s)` and listed in your hand-in (the closing
wave rewires those callers and deletes the forwarders). This is the ONE exception to B2's "no
forwarding wrapper", and it exists so blocks stay disjoint. A forwarder takes the typed receiver
where its outside callers already hold one, else the old signature.

## Order of value
Convert what is CERTAIN first: a class whose header is finished, whose bodies read `this`. A body
whose class cannot be settled from retail in reasonable time is LEFT and listed — a wrong class is
worse than a cast. Aim for the whole block; hand in what is done when the remainder is only LEFT
items.

## Behaviour
No behaviour change outside `behaviour:` commits (retail address in the title's text). H5 members
(non-trivial types whose ctor / dtor would start to run) stay raw storage unless you make the
`behaviour:` commit yourself with retail's ctor / dtor read and quoted. Widths and signedness: the
field has retail's type; a site whose result changes is a `behaviour:` commit.

## Hand-in additions
- `castmetric.py <your files>`: base → tip per column; `--floor` summary (bad / stray markers 0).
- B4 FORWARD forwarders left, with their outside caller files.
- LEFT reads by reason (owner block named).
- B11 object comparison: identical / different, each difference explained.

## Shared C-body headers (added 2026-09-29)
`code/Game/<Module>/exe_bodies.h` and a `*_funcs.h` shared by several families belong to no single
block. A block edits ONLY the sections headed by its own defining files (`// ---- citizen*.cpp`):
it deletes the declarations whose bodies became members with no outside caller and keeps the
`B4 FORWARD` ones. No other line is touched, nothing is reordered or reformatted. The hand-in lists
the line ranges.
