# B.3.5 session kit (2026-09-19) — reference, not live tooling

Kept at the B.3.5 close so step 6 can reuse the shapes. The coordinator's gate, the resave DIFF
classifier, the Mods route and the warning A/B are still the ones in `../b34_session/` (copy them
into the new session's scratchpad; `gate.sh` takes its paths from its own location).

| file | what it is |
|---|---|
| `BLOCK_BRIEF.md` | the brief every class block and fragment pass ran under (class shape, process rules, static gate, runtime check, report format). The block's prompt adds only: class set, FILE set, branch name, who else is running. |
| `CALLER_BATCH_BRIEF.md` | the brief every caller batch ran under. Round 2 prompts added the exe-import-slot method and the list of members the fragment passes had defined since round 1. |
| `ghidra/*.java` | the question pass's read-only dump scripts (vtables + slot targets, parents by `_ParentTypeName`, hidden-retslot exports, per-class export lists). This Ghidra has no Python provider: scripts are Java. Change the namespace filter for UI. |
| `CLOSE_NUMBERS.md` | what the closing owner measured and HOW (units stated): cast lines per directory, bridge lines, remaining by-name sites, inlined slot calls, census rows, class list, declared-undefined members. The template for step 6's close. |
| `RENDER_CHECK.md` | the step's rendering verification by command stream (`tools/trace_oracle`): pre-step exe vs step exe, control pairs from both arms. |

Order that worked: question pass (one opus owner, retail bytes) + file map (sonnet) -> blocks by
disjoint FILE set -> coordinator merges and gates each -> fragment pass per side -> caller round 1
-> caller round 2 + the table/seam batch -> one closing owner -> step-close gate (clean rebuild,
Mods, stress, true release + `make clean`, trace oracle). Lessons are in `../NEXT_SESSION.md` 2d.
