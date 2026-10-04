# B.4 second pass — wave X block 0 (READ-ONLY inventory of what is left)

Repo: `C:/code/GIT/untitled_rev_eng_project` (Banished reimplementation, C++). The project removes every
"dangerous" cast (a read by raw offset, a `void*` / integer reinterpreted as a class). The file-by-file
waves are done: about 3,200 casts are left, and almost every one now carries a `// LEFT: <reason>` comment
on its line (or the line above) written by the block that left it. What is left waits on something that
crosses files: a C function signature taking `void*` / integers that many files call, a forwarder, a POD
mirror type, a raw-storage member, a parameter type of a virtual slot. Your job: for ONE region, turn the
remaining casts into a table keyed by the DECLARATION that has to change, with every file that must change
with it, so the next agents can each own one declaration family and all its callers.

You change NOTHING in the repo. No builds, no game runs, no writing git commands, no search outside the
repo (never `find /`). Use Grep / Glob / Read scoped to the repo. Write only the output files below.

## Inputs (in `<INVX>` = the directory this brief is in)
- `detail_<REGION>.txt` — every counted cast of your region: `file:line:col<TAB>spelling<TAB>snippet`
- `left_<REGION>.txt` — every `// LEFT:` line of your region (grep output `file:line:text`)
- Background: `docs/runtime_port/endgame_b/b4_session/PASS2_LEDGER.md` (what blocks asked of each other; its
  "Wave X additions" lists name most of the functions), `data/pass2/SIG_WORK.tsv`, `data/pass2/SIG_ACT.tsv`
  (typed signatures two blocks already proposed), `data/pass2/FORWARDERS.tsv` (the first forwarder
  inventory: many of its rows are gone now).

## What to produce
For every remaining cast of your region decide its FAMILY key, using exactly these forms:
- `SIG:<function>` — waits on the signature of a free / `extern "C"` function or a member (parameter or
  return is `void*` / integer). ONE key per function. Record: where it is DECLARED (header:line, or
  "local extern in <files>" when there is no header), where it is DEFINED (file:line), its current
  signature, the typed signature it should get (from SIG_WORK / SIG_ACT, or from what the body does: say
  which), whether it is a pure one-line forwarder to a member (name the member).
- `SLOT:<Class>::<virtual>` — waits on a virtual slot's parameter type (`Component::Update(void* time)`,
  the Controller slots' `void* stateMachine` / `void* time`, `Action` slots [4]-[7], `Tool` slots ...).
- `H5:<Class>::<member>` — raw-storage member of a non-trivial type.
- `FIELD:<Class>::<member>` — a member still `void*` / integer / wrong type in a header.
- `POD:<mirror type or bridge>` — a POD mirror (`RtSystem::String`, `RtObject::External`, `RtIo::…`) or a
  bridge function between mirror and real class; for the ExternalPtr serialize thunks use
  `POD:ExternalPtr-thunk`.
- `ERASED:<core entry point>` — a type-erased container entry point taking `void*` (`RtColl_Array*`,
  `rtsetavl::…`, the hand-written `SetSize` / resize bodies).
- `CONST:FindOrLoad` — the `const_cast` of a `FindOrLoad` / `FindNext` / `Find` result.
- `FLOOR:<category>` — a genuine reinterpretation that should simply carry `// CAST-OK: <category>`
  (alloc_header, ptr_int, crash_walk, win32, float_bits, string_header, pe_walk, byte_io, elem_stride,
  gpu_map, com_iid, crt, hash, vslot, dx11_seam).
- `H8:<what>` — a byte / enum serialized through a `bool&` / `uint&` overload that must stay.
- `LOCAL:<what>` — nothing outside the file blocks it (a block simply did not get to it).
- `OTHER:<what>`.

## Outputs
1. `<INVX>/out/<REGION>.tsv`, tab-separated, header row
   `file<TAB>line<TAB>family<TAB>note` — ONE ROW PER CAST LINE (take the lines from detail_<REGION>.txt;
   several casts on one line = one row with the count in `note`).
2. `<INVX>/out/<REGION>_families.tsv`, header row
   `family<TAB>casts<TAB>declared_at<TAB>defined_at<TAB>current_signature<TAB>typed_signature<TAB>forwarder_to<TAB>all_caller_files`
   — one row per `SIG:` / `SLOT:` / `ERASED:` / `POD:` family that appears in your region.
   `all_caller_files` = EVERY file in the WHOLE TREE (`code runtime standalone dx11 engine tests`) that
   calls / overrides / declares the function (Grep its name; a local `extern` declaration counts; comments
   and strings do not), as `path(n_sites)` separated by `; ` — not only files of your region. This column
   is what the next wave's file ownership is computed from: be complete.
3. `<INVX>/out/<REGION>.md`, under 60 lines: families ordered by casts; functions that are pure forwarders
   with ZERO callers left (deletable now); anything a `// LEFT` comment claims that the code contradicts.

Your final message: under 12 lines — the three paths, casts classified, the ten largest families with counts.
Do not paste file contents.
