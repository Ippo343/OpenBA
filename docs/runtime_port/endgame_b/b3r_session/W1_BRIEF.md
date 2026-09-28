# W1 brief — reader blocks (read AFTER `BLOCK_BRIEF.md` and `../B3R_RULES.md`)

You convert EVERY door spelling in YOUR files to a direct reference (R2), delete the DEAD registration
lines and dead readers in your files (R3), and write STUB readers as their null arm (R4). Rows that
other blocks may read stay registered: W2 deletes them.

## Where the names are
| you have | look in |
|---|---|
| an exe address `0x14…` | `data/P0a_declared.tsv` (Action, Work), `P0b_declared.tsv` (Component), `P0c_declared.tsv` (the rest): address -> body name -> declaring header. Class and readers: `data/B_exe_rows.tsv` |
| an import slot `0x140151xxx / 0x140152xxx` | `data/B_exe_rows.tsv` column `runtime_symbol_if_slot`, then the Runtime symbol in `data/A_runtime_rows.tsv` |
| a mangled name or a `0x18…` address | `data/A_runtime_rows.tsv` (`target`, `direct_replacement`), declarations in `P0c_declared.tsv` |
| an `engine_funcs` global (`fn_*`, `g_p*`) | `data/E_engine_funcs.tsv`: `bound_to` is the key, `direct_replacement` the call. 19 globals are MISNAMED: trust the binding |
| a `.rdata` constant | `code/Game/Game/exe_constants.h` (`kRdata_<addr>`) |
| `fn_IsDerivedFrom(p, id)` | `Object::IsDerivedFromNullSafe(p, id)` (`runtime/types/object/TypeInfo.h`) — NOT `p->IsDerivedFrom(id)` |

The tables are maps. Before each edit, read the reader's BINDING in the code and the body's
DEFINITION; when they disagree with the table, the code wins and your hand-in says so.

## Bodies with internal linkage
A target that is `static` or in an anonymous namespace (P0 listed them) gets external linkage and a
declaration in the defining module's header, by the block that owns the DEFINING file. If the defining
file is not yours, leave the reader and list it (file:line, key, the owner block).

## The allocator doors — do them LAST
Block W0-A0 is giving `System::MemoryAllocator` its members and re-seating the REDZONE / ALLOC_STATS
instruments on a named hook. Until it has merged, do NOT convert an allocator door
(`fn_MemAlloc`, `fn_MemFree`, `fn_MemFreeStatic`, `g_pMemAllocSingleton`, slots 0x140152b60 / b68,
`?MemAlloc@` / `?MemFree@` / `?Allocate@` / `?Free@`, `InternalMemAlloc/Free` wrappers, per-file
`*MemAlloc*` / `*MemFree*` wrappers). Convert everything else first. Then `git merge main`: if
`runtime/types/system/MemoryAllocator.h` declares `MemAlloc` / `MemFree` as static members, convert
the allocator doors to them (the door decides which member: `?Allocate@` takes the singleton first
and is NOT `MemAlloc`); if it does not yet, hand in without them and say so — the coordinator sends
you back for them.
`g_pMemAllocSingleton` as an ARGUMENT disappears with the call that took it. Any OTHER read of it
becomes `System::MemoryAllocator::_singleton` read live; say where.

## Guards
`if (fn_X)`, `if (!fn_X) return …`, `fn_X ? fn_X(a) : fallback`, `if (!s_tried) { … }`, readiness
booleans: on a DIRECT row they are always true today. Delete the guard and the dead arm. Do not
delete a null check on an OBJECT or on a value the body returned.
A log line inside a dead arm goes with it; a log line on the live path stays.

## What must not change
The order of calls, the arguments, the values. If the direct call needs a cast to fit the body's
declared parameter type, cast at the call (`// B.3.x bridge` for a receiver). If it needs a
different NUMBER of arguments, stop: R2's argument-list rule.
