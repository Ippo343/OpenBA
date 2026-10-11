# B.4 second pass — block 0 inventory brief (READ-ONLY)

Repo: `C:/code/GIT/untitled_rev_eng_project` (Banished reimplementation, C++). The project is removing
every "dangerous" cast (a read by raw offset, or a `void*` / integer reinterpreted as a class). 8,950 are
left. The first pass left them because of a BLOCKER. Your job: for ONE region of the tree, say for every
remaining cast WHAT BLOCKS IT, so the next agents can be allocated one per blocker.

You change NOTHING in the repo. No builds, no game runs, no git commands that write, no `find /` or any
search outside the repo. Use Grep / Glob / Read scoped to the repo. Write only the two output files below.

## Inputs (in `<INV>` = the directory this brief is in)
- `slices/<REGION>.detail.txt` — every counted cast in your region: `file:line:col<TAB>spelling<TAB>snippet`
- `slices/<REGION>.files.tsv` — `count<TAB>file`, descending
- `slices/<REGION>.left.txt` — every `// LEFT: <reason>` comment the first pass wrote in your region
  (the reason names the owner / blocker; "as above" refers to the previous LEFT in the same file)
- Background (skim only): `docs/runtime_port/endgame_b/NEXT_SESSION.md` §2.3 (the known cross-module
  asks), `docs/runtime_port/endgame_b/B4_RULES.md` §2-§4.

## What to do
For every file in your region with >= 5 casts (smaller files: classify from the snippet and the LEFT
text alone, do not open them unless cheap): open the file at the cast sites, find what the operand IS
(a `void*` parameter, a `void*` / integer / raw-storage member of some class, the return of a C function,
a local computed as `base + 0x…`), find its DECLARATION (which header / which function signature), and
decide the blocker. One file usually has 1-4 blockers; split the file's count between them (estimates
are fine, they must sum to the file's count).

Blocker keys (use exactly these forms):
- `LOCAL:<Class>` — no outside blocker. The casts are raw offset reads (`*(int*)(p + 0x28)`) of an
  object of `<Class>` whose header already declares the fields (check the header exists and has the
  field at that offset), or whose header is in the same module; the first pass simply did not get to
  them. Say in the note whether the header already has the fields or needs them added.
- `NOCLASS:<short description>` — the thing read has no type in any header (a node, a record, a
  scratch struct); a layout must be settled first. Give its size / the retail function if the code says.
- `FIELD:<Class>::<member>` — a member declared `void*` / `uint64_t` / `int64_t` in `<header>`; typing
  it (`T*`) removes the casts. Name the T the readers cast it to.
- `H5:<Class>::<member>` — a member held as raw byte storage of a non-trivial type (String, Array,
  embedded object).
- `SIG:<function>` — a C function / member / virtual slot whose PARAMETER or RETURN is `void*` /
  integer; name the function and the header that declares it. (Use `SIG:Serialize-stream` for the
  `void* stream` parameter of any `Serialize` / `Persist` override; `SIG:Work-slots` for
  `Work::Execute/OnComplete/OnCancel/GetTarget/Slot5`; `SIG:Action-slots` for
  `Action::Execute/Enter/Exit/GetTarget/AbstractImpl`.)
- `FWD:<forwarder name>` — the cast exists inside / because of a `// B4 FORWARD` forwarder.
- `ERASED:<core>` — a type-erased container core (Array / Set / AVL helpers taking `void*` / `uint8_t*`).
- `CONST:FindOrLoad` — `const_cast` of a `FindOrLoad` / `FindNext` result (or other `CONST:<what>`).
- `POD:<Rt mirror>` — a bridge between an `Rt<Ns>::X` POD mirror and the real class.
- `FLOOR:<category>` — genuinely a legitimate reinterpretation that should just be marked
  `// CAST-OK: <category>` (categories: alloc_header, ptr_int, crash_walk, win32, float_bits,
  string_header, pe_walk, byte_io, elem_stride, gpu_map, com_iid, crt, hash, vslot, dx11_seam).
- `OTHER:<what>` — anything else.

Use the SAME key string for the same blocker across files (e.g. always `FIELD:GameUpdateLoop::m_game`),
because the keys are grouped afterwards.

## Outputs
1. `<INV>/out/<REGION>.tsv` — tab-separated, header row
   `file<TAB>file_casts<TAB>blocker<TAB>casts<TAB>declared_in<TAB>note`
   one row per (file, blocker). `declared_in` = the header / file that must CHANGE to remove the blocker
   (for LOCAL: the class header). `note` <= 120 chars.
2. `<INV>/out/<REGION>.md` — under 80 lines: the blockers of your region ordered by casts (key, casts,
   files, the header that changes, one line on what the change is); the 10 largest `LOCAL` files; the
   classes that have NO header yet; anything the brief got wrong.

Work through the big files first. Do not paste file contents into your final message; your final message
is under 15 lines: the two paths, total casts classified, the top 8 blockers with counts.
