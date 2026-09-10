# WAVE 15 LANE DOOR — per-row door facts for the 20 `_singleton` rows

**Worktree** `C:/code/GIT/wt-wave15-door`, branch `wave15/door`. Document-only:
no registrations changed, no source touched. `tools/dispatch_slot_map.py --check`
read **0 mismatches before and after** (62 `ResolveExport` registrations checked,
0 shadowed) — this lane moved it zero, as instructed.

## Result up front

`WAVE15_KICKOFF_SINGLETONS.md`'s `exe door` column and its "**15 of 20**" claim
are **CONFIRMED IN FULL**, cell by cell, against `Application-x64.exe`'s real PE
import table via `pefile`. No cell was wrong. The claimed method-3 ordinal
closure is also confirmed, and extended to all three binaries. See "What was
verified, and how" below for the exact routes and why a second route could
actually have disagreed with the first (the two-checks-one-premise trap).

Two facts this lane adds that the kickoff did not have:

1. **Three of the 15 door rows have doors in THREE binaries, not one.**
   `Application@1`, `Configuration@Video`, and `Device@Video` are imported by
   name in `Application-x64.exe`, `VideoDX11-x64.dll`, AND `VideoDX9-x64.dll`
   independently, each with its own IAT slot VA. A promotion strategy for
   these three has three doors to intercept, not one.
2. **8 of the 15 door rows already have a `RegisterDispatchSlot` in the tree**,
   but reading the actual registration lines shows most are **not promotions**
   — several are explicitly commented "no-op today ... registered for census
   hygiene" because they just `ResolveExport` the same DLL address the slot
   already held. Only `SaveLoad` carries an actual promotion (Wave 14's mint
   storage, a separate mechanism from this slot registration). This
   distinction matters for whoever scopes promotion work next: "registered"
   is not "promoted," and this lane's `registered` column should not be read
   as a to-do list already half-done.

## Door table

| symbol | exe door | dx11 door | dx9 door | exe .text sites (get_xrefs_to) | registered? | accessor imported? | verdict |
|---|---|---|---|---|---|---|---|
| `AchievementManager@Achievement` | yes `0x1401514f0` | no | no | 9 | yes (`reg_unreg_lane1.cpp:206`, passthrough) | no | DOOR |
| `AnimationManager@Graphics` | **no** | no | no | n/a | no | no | **NO-DOOR** |
| `Application@1` | yes `0x140152368` | yes `0x180013310` | yes `0x1800122f8` | 6 | yes (`address_registry.cpp:1536`) | no | DOOR — `UNPROMOTABLE \| HARMFUL` (Wave 12) |
| `Configuration@Audio` | yes `0x140151b78` | no | no | 9 | no | no | DOOR (unregistered) |
| `Configuration@Input` | yes `0x140151738` | no | no | 31 | yes (`address_registry.cpp:1606`, special-cased to `&g_StandaloneKeyMapMid`, not a passthrough) | no | DOOR |
| `Configuration@System` | **no** | no | no | n/a | no | no | **NO-DOOR** |
| `Configuration@Video` | yes `0x140151b70` | yes `0x180013480` | yes `0x1800123d0` | 6 | no | no | DOOR (unregistered; 3-binary door) |
| `Device@Audio` | **no** | no | no | n/a | no | no | **NO-DOOR** |
| `Device@Input` | yes `0x140151b80` | no | no | 6 | no | no | DOOR (unregistered) |
| `Device@Video` | yes `0x140151a80` | yes `0x1800134b8` | yes `0x180012460` | 53 | yes (`address_registry.cpp:1403`, passthrough) | no | DOOR (unregistered in dx11/dx9; 3-binary door) |
| `Engine@1` | **no** | no | no | n/a | no | no | **NO-DOOR** |
| `ExternalFactory@Object` | yes `0x1401529d8` | no | no | 177 | no | no | DOOR (unregistered; largest reader count of all 20) |
| `Factory@Object` | yes `0x140152760` | no | no | 8 | yes (`address_registry.cpp:1544`) | no | DOOR |
| `FileSystem@IO` | yes `0x140151d18` | no | no | 5 | yes (`reg_unreg_lane1.cpp:177`, passthrough) | no | DOOR |
| `InternalFactory@Object` | yes `0x140151ef8` | no | no | 2 | no | no | DOOR (unregistered) |
| `MainLoop@Engine` | yes `0x140151440` | no | no | 12 | yes (`reg_unreg_lane1.cpp:190`, passthrough) | no | DOOR |
| `MemoryAllocator@System` | **no** | no | no | n/a | no | no | **NO-DOOR** (also behind `BANISHED_RT_ALLOC`, its own wave) |
| `Modification@IO` | yes `0x1401519e8` | no | no | 12 | no | no | DOOR (unregistered) |
| `Persistence@IO` | yes `0x140151b88` | no | no | 14 | no | no | DOOR (unregistered) |
| `SaveLoad@Save` | yes `0x140151610` | no | no | 19 | yes (`reg_unreg_lane1.cpp:168` passthrough + Wave 14 mint-storage promotion) | no | DOOR — **PROMOTED**, Wave 14 ceiling |

`exe .text sites` is an **exact `get_xrefs_to` count** (Ghidra's own analysis of
the current `Application-x64.exe`), not the kickoff's crude disp32-scan
estimate. Where a comparison is possible it matches within noise: `SaveLoad`
scans 20 in the kickoff's method, this lane's xref count is 19 (one-off,
plausibly the scan's known overcount factor at this small a size — not
investigated further since the kickoff already warned `~real` is not a
measurement). `ExternalFactory` (kickoff ~188 estimated) counted 177 exact —
same tier, consistent ranking.

## What was verified, and how (two independent routes)

**Route 1 — normal PE import table**, all three binaries, via `pefile`:
iterate `DIRECTORY_ENTRY_IMPORT`, filter `dll == Runtime-x64.dll`, match each
of the 20 mangled `?_singleton@...` names against the imported-by-name set.
This alone reproduces the kickoff's 15/20 exactly, cell for cell, including
which 5 are absent (`AnimationManager`, `Configuration@System`, `Device@Audio`,
`Engine@1`, `MemoryAllocator@System` — exactly the kickoff's named set).

**Route 2 — a route that could actually disagree**: raw byte-string search for
each of the 5 absent names' mangled text, across the full file content of all
three binaries (not just the import directory — the whole file, so a
`GetProcAddress`-by-name string sitting anywhere else would still be found).
Zero occurrences in all three binaries for all 5 names. This is the check the
lane doc's trap #5 (two checks sharing a hidden premise) asks for: route 1
could only ever say "not in the import table"; route 2 asks the different
question "does this string exist ANYWHERE in the file," and it agreeing is a
real second data point, not a restatement of route 1.

**Import-by-ordinal, closed, not just unfound**: `pefile`'s import entries
have `.name is None` exactly when an import is ordinal-only. Filtering for
that against `dll == Runtime-x64.dll` (or DX11/DX9's own Runtime import
tables) returns an **empty list for all three binaries** — zero Runtime
imports by ordinal exist anywhere, for any symbol, not just these 20. That
closes the route rather than failing to find an instance of it, per the
kickoff's own wording requirement (method 3).

**Accessor-imported check** (trap: check the accessor too, not just the slot):
searched every Runtime-imported name in `Application-x64.exe` for a
static-accessor shape (`SAPEAV<Class>...` / `SAAEAV<Class>...`, i.e. a static
member returning a pointer/reference to its own class with no args — the
`GetInstance()`/`Instance()` idiom). **Zero matches for all 20 classes.** The
exe does import many *other* instance methods of these classes (e.g.
`SaveLoad::GetSaveCount`, `Device::Video::Lock`), which is how it reaches
already-obtained instances without a separate accessor — but there is no
distinct singleton-accessor export to register a door on. So
`accessor_imported` is `no` for every row, and the
`NO-DOOR-BUT-ACCESSOR-DOOR` verdict never applies in this set.

## Route that could not be enumerated

`GetProcAddress(h, MAKEINTRESOURCE(ordinal))` has no string in the binary to
search for — an ordinal passed as an integer literal looks like any other
integer at the call site, and nothing distinguishes "this int is secretly an
ordinal for `GetProcAddress`" from any other numeric argument short of manually
auditing every `GetProcAddress` call site's second argument in all three
binaries and Runtime-x64.dll's own exports-by-ordinal table. **This route is
named as un-enumerable, not closed** — unlike the by-name ordinal-import route
above, which the import table closes outright.

## The NO-DOOR rows: is there anything to promote into?

For `AnimationManager@Graphics`, `Configuration@System`, `Device@Audio`, and
`Engine@1` (the 4 the kickoff names, membership confirmed above), plus
`MemoryAllocator@System` (no-door for a different, already-documented reason —
the allocator gate): **no import slot exists in any of the three binaries for
the `_singleton` symbol itself, and no accessor to it is imported either.**

That is not merely "not yet imported" — it is established from the binaries
that no consumer binary's import table, ordinal table, or GetProcAddress
name-string set references these five `_singleton` slots at all (the only
unclosed route, MAKEINTRESOURCE ordinals, is unclosed for literally every
symbol in every binary, so it carries no extra suspicion for these five in
particular). Every reader of these five rows is necessarily an in-DLL
RIP-relative load — reached only from inside `Runtime-x64.dll` itself, never
across a module boundary we can intercept.

A row with **zero cross-module readers** has no promotion semantics: there is
nothing outside the DLL to redirect, and nothing our port's own storage would
serve except the DLL's own (unreachable) internal code. This yields a clean
**NO-OP** classification by construction — not unfinished work, not a gap in
this wave's coverage, simply a row with nothing on the other side of the door.
(Whether the DLL-internal *readers* of these rows are ported bodies whose own
in-DLL loads got redirected via detour, or unported bodies that read the DLL's
value directly, is lane INSTR's question, not this lane's — see Out of scope.)

`AnimationManager` and `Engine@1` do have several *other* class methods
imported (ctor/dtor/Update/Serialize, `Startup`/`Shutdown`, etc. — see the
accessor-check output for the full import lists), so the type itself is very
much reachable at cutover; it is specifically the raw `_singleton` pointer
slot that has no cross-module door.

## Out of scope, honored

No promotion verdicts rendered beyond the NO-DOOR / nothing-to-promote-into
argument above. No registrations added or changed — `standalone/*.cpp` is
untouched (`git status` at the end of this lane is clean apart from these two
new files). DLL-internal reference counting and reader classification
(ported/unported/dead) is explicitly lane INSTR's job and was not duplicated
here.

## `dispatch_slot_map.py --check`

Before: `checked 62 ResolveExport registrations, 0 name mismatch(es); 0
address(es) registered more than once`. After (no source changes made): same
— 0 mismatches, moved zero.
