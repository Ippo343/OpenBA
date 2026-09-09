# WAVE 10 — lane MINTSEL: per-row promotion of minted DATA

Branch `wave10/lane-mintsel`, cut from `wave10/base` (`bed7a3f`).
Worktree `C:\code\GIT\untitled_rev_eng_project\.claude\worktrees\agent-a1a05adf15927af9d`.

## Headline

**Ten rows promoted. Five not.** `tools/runtime_census.py --unported --namespace UI`
goes **22 → 12**; the UI `ported` column goes **148 → 158** of 170 called
(87.1% → 92.9%).

| what | outcome |
|---|---|
| per-row selectivity | **built** — three doors, see §1 |
| the 10 UI `?_typeInfo@` rows | **PROMOTED**, live on every run |
| the 4 minted UI `??_7` vtables | **BLOCKED** — 20 abort traps between them, each now NAMED |
| `??_7Dialog@UI@@6B@` | **not minted at all** — a generator gap, not a port gap |

Lane UIDATA's verdict on the typeinfo half — *"no condition short of cutover"* —
was **wrong, and this lane fixed the underlying defect rather than working
around it**. Its verdict on the vtable half was right and is re-measured here.

---

## 1. Per-row selectivity (the enabling change)

`RegisterMintedData()` was one switch over all 197 rows. There are now three
doors, in increasing order of commitment:

| door | what it does | who it is for |
|---|---|---|
| `RtMintRegisterRow(sym)` | one row, **both** doors (name + address), idempotent | the primitive; everything else goes through it, so there is one copy of the "both doors" rule |
| `BANISHED_RT_MINT_ROWS=<csv>` | registers exactly the named rows (`Foo*` = prefix) | **the measuring instrument** — runs a candidate in a real boot *without* the storage rows that make the global switch fatal |
| `standalone/reg_mint_promoted.cpp` | rows registered **unconditionally, every run** | promotion. These are ports. |

`BANISHED_RT_MINT=1` keeps its old meaning (all 197) and remains a hazard, not
a route. `BANISHED_RT_MINT_PROMOTED=0` is the kill switch for the promoted set.

Two design points that are load-bearing:

* **A token matching no row aborts.** A typo that silently registers nothing
  looks exactly like "the row was tested and found safe" — the failure mode
  this lane exists to prevent.
* **The promotion file spells each symbol as a literal at its
  `RegisterRuntimeExport` call site.** `tools/runtime_census.py` excludes
  `MINT_FILE` from the *ported* side entirely, and its non-literal scan cannot
  see a name arriving as `t.symbol` from a table walk. A promotion registered
  from inside the mint would be live in the process and invisible to the census
  — `reference_half_registered_port_reads_as_absent.md` exactly. The checking
  half (`RtMintPromotionStorage(sym, va)`) aborts if the symbol is not a minted
  row or if the VA disagrees with the generated table, so the duplicated token
  cannot drift.

**No census change was needed to see the ten promoted rows.** That was the
point of the literal.

---

## 2. The `_typeInfo` typeId: SOLVED, not worked around

### What UIDATA found, and what it concluded

The mint stamped `BaseTypeInfo::typeId` with the ctor sentinel `0xFFFFFFFF`.
Because our nodes hang off `g_mintTypeInfoHead` — a chain no `Factory::Startup`
walks — that was not a temporary state, it was the final value. Every consumer
of a `?_typeInfo@` in this tree reads exactly that dword. UIDATA concluded:
*"no condition short of cutover."*

### Why that was wrong

The id is not information the DLL holds that we cannot recover. It is a pure
function of `name`, and **we already own that function**: retail's private
`?GetTypeFromNameCRC@Factory@Object@@AEBAIPEBD@Z` is transcribed in
`runtime/src/object/type_system.cpp`, and `Factory::Startup`'s own line is
`info->typeId = TypeNameCRC(info->name)`.

Confirmed offline before a line was written — CRC-32 (reflected, poly
`0xEDB88320`) of the name widened to **UTF-16LE**, against the nine real ids
UIDATA had printed: 9/9.

The fix is three lines of substance:

* `TypeNameCRC` gets one exported spelling, `RtObject::TypeIdFromName`
  (`runtime/types/object/TypeInfo.h`). The faithful body stays where it was
  written; there is deliberately **no second copy of a CRC**.
* `MintTypeInfos()` stamps `TypeIdFromName(name)` instead of the sentinel.
  This is not a new claim about retail: it is Startup's own line, run earlier,
  on a node Startup will never reach, writing the value Startup would write.
  If our chain is ever walked, the walk is idempotent.
* The shadow's typeId line stops being an unconditional `INFO` and becomes a
  **real comparison against the loaded DLL**.

### The measurement

```
[RtMintShadow] BaseTypeInfo : 624 field comparisons, 0 MISMATCH,
                              0 typeId not-yet-comparable
[RtMintShadow]   typeinfo rows: 104 READY, 0 BLOCKED (of 104)
```

**All 104 minted `BaseTypeInfo` typeIds equal the loaded DLL's**, at phase=exit.

Three things make that a result rather than a green light:

1. **The instrument can fail.** A CRC over plain ASCII — the obvious wrong
   implementation — produces 104 MISMATCH lines, not 104 silent passes. The
   negative control was built into the problem and did not have to be invented.
2. **An un-run comparison is never scored as a pass.** The DLL's own node holds
   `0xFFFFFFFF` until *its* `Factory::Startup` runs, so at `phase=init` the
   report says `104 typeId not-yet-comparable` and every typeinfo row reads
   `BLOCKED … idcmp=NO`. Only the exit report has a comparison to make. Folding
   the two together is how a wrong id would hide behind an early report.
3. **An end-to-end check on a default-on consumer.**
   `standalone/reg_element_typeids.cpp` publishes six of the ten into 21 exe
   `.rdata` slots and logs the pointer *and* the id for each. Across the A/B:

   | | control (`_PROMOTED=0`) | promoted |
   |---|---|---|
   | `ImageElement` | `typeInfo=00007ffcab68ac48` (DLL) | `typeInfo=00007ff7b334da28` (**our storage**) |
   | id | `893464662` | `893464662` |

   Six pointers move into our image, fifteen stay on the DLL, and a diff of all
   21 lines with the pointer column masked is **empty**. Same id, different
   storage — which is precisely the claim.

### Why not all 104

The other 94 are equally READY on the instrument. Promoting them would be a
port with no caller: nothing in this tree resolves them, so nothing would
exercise them and the census would gain 94 rows no run can verify
(`reference_uncalled_port_is_not_verified.md`). The ten have live consumers.
That is the whole difference.

### What is still a trap, stated rather than hidden

Every minted `BaseTypeInfo`'s `factoryFn` / `inPlaceFactoryFn` is a named abort
trap — for all 104 rows; no mint factory is ported. Those pointers are reachable
only through an `Object::Factory` tree, and a minted node is in no tree: our
`Factory::Startup` resolves `?_firstTypeInfo@` by name, that row is **not**
promoted, so it still walks the DLL's chain. Every consumer of these ten in this
tree reads the first dword (`dialog.cpp` and `spritesheet_vtable.cpp` also read
`+0x08 name`, which the shadow compares and which agrees). The readiness table
prints `fac=2traps` on every row rather than collapsing it into the verdict word
— "safe for the consumers we have" and "safe for any consumer" are different
claims and the report makes both visible.

---

## 3. The vtables: BLOCKED, and now the blocker has a name

Re-measured live this lane, not quoted from UIDATA:

```
BLOCKED  ??_7SpriteSheet@UI@@6B@        slots= 9 ported= 8 TRAP= 1 purecall= 0
           TRAP slot  5  ?Serialize@SpriteSheet@UI@@UEAA_NAEAVStream@IO@@@Z
BLOCKED  ??_7DialogInstance@UI@@6B@     slots=10 ported= 8 TRAP= 2 purecall= 0
           TRAP slot  2  ?Type@DialogInstance@UI@@UEBAIXZ
           TRAP slot  4  ?OnOpened@DialogInstance@UI@@UEAAXXZ
BLOCKED  ??_7Element@UI@@6B@            slots=12 ported= 3 TRAP= 8 purecall= 1
           TRAP slot  1  ?TypeName@Element@UI@@UEBAPEBDXZ
           TRAP slot  2  ?Type@Element@UI@@UEBAIXZ
           TRAP slot  3  ?Persist@Element@UI@@UEAAXAEAVStream@IO@@@Z
           TRAP slot  7  ?Update@Element@UI@@UEAAXAEBVTime@System@@… (+4 more)
BLOCKED  ??_7DialogController@UI@@6B@   slots=11 ported= 2 TRAP= 9 purecall= 0
           TRAP slot  1  ?TypeName@DialogController@UI@@UEBAPEBDXZ
           TRAP slot  2  ?Type@DialogController@UI@@UEBAIXZ
           TRAP slot  4  ?OnCreated@DialogController@UI@@UEAAXXZ
           TRAP slot  5  ?OnOpened@DialogController@UI@@UEAAXXZ  (+5 more)
```

**20 traps across 41 non-purecall slots** (UIDATA measured 27; lane UI4 closed
seven of `SpriteSheet`'s). Registering any of these four converts a working
frame into `abort()`, because our ported ctors *stamp* the table and the DLL's
own `UserInterface::Update` then dispatches through it. One trap is
disqualifying, so **`SpriteSheet` is one slot away and still a no**.

The readiness table now **names** each trapped slot, from the live port registry
(`OurPort()`), never from a source grep — a slot registered under a name held in
a variable is invisible to a grep and would be reported as work that is already
done. That naming is the difference between "9 TRAP" and a worklist.

### `??_7Dialog@UI@@6B@` is a fifth row, and it is not a port problem

It is **not in `standalone/rt_data_mint_tables.inc` at all**. It became a census
row when `runtime/src/ui/dialog.cpp` started resolving it, after the tables were
last generated. There is nothing to promote. It expires by re-running
`tools/rt_mint_gen.py` — the generator's lane, not a UI lane. This is
`reference_mint_population_rule_blind_to_its_own_output.md` in its plainest
form: the generator mints what our source named *at generation time*.

---

## 4. Verdict per row

| row | verdict | surviving blocker / what expires it |
|---|---|---|
| `?_typeInfo@CheckElement@UI@@…` | **PROMOTED** | — |
| `?_typeInfo@ClipRegionElement@UI@@…` | **PROMOTED** | — |
| `?_typeInfo@Dialog@UI@@…` | **PROMOTED** | — |
| `?_typeInfo@DialogInstance@UI@@…` | **PROMOTED** | — |
| `?_typeInfo@ImageElement@UI@@…` | **PROMOTED** | — |
| `?_typeInfo@LabelElement@UI@@…` | **PROMOTED** | — |
| `?_typeInfo@NumberEditElement@UI@@…` | **PROMOTED** | — |
| `?_typeInfo@ScrollElement@UI@@…` | **PROMOTED** | — |
| `?_typeInfo@SpriteSheet@UI@@…` | **PROMOTED** | — |
| `?_typeInfo@TextElement@UI@@…` | **PROMOTED** | — |
| `??_7SpriteSheet@UI@@6B@` | BLOCKED | 1 trap: `?Serialize@SpriteSheet@UI@@`. Port that one slot. |
| `??_7DialogInstance@UI@@6B@` | BLOCKED | 2 traps: `?Type@`, `?OnOpened@` |
| `??_7Element@UI@@6B@` | BLOCKED | 8 traps incl. `?Update@`, `?Type@`, `?TypeName@`, `?Persist@` |
| `??_7DialogController@UI@@6B@` | BLOCKED | 9 traps incl. `?Type@`, `?TypeName@`, `?OnCreated@`, `?OnOpened@` |
| `??_7Dialog@UI@@6B@` | BLOCKED | **not minted** — re-run `tools/rt_mint_gen.py` |

---

## 5. The census note, corrected

The printed note used to end *"they therefore cannot move the columns above at
all, in either direction"*. That was wrong in both directions and the wrongness
cost a lane:

* A minted row **always** moved the *called* side — `MINT_FILE` is excluded from
  the ported scan but not the called scan, so an unregistered minted row our
  tree resolves has always appeared under `--unported`. That accounting is
  correct; the note denied it, which is why the 13 rows UIDATA was sent after
  looked like UI work when no UI work would move them.
* A **promoted** row now moves the *ported* side too, counted by the ordinary
  scan rather than by the mint's own line.

The note now says both, plus the count of promoted rows, and the one thing that
is still true (a minted row has no *body*, so it is not a shortfall in the
function port).

**Control on the tool change: the only line that moved is the note.**
`diff census_before_note.txt census_after_note.txt` touches nothing but that
block — every namespace row, every total, and the `TOTAL 4632 1076 858 6 79.7%`
line are byte-identical. The `22 → 12` was produced by the **source** change and
measured with the **pre-note** tool.

The script as run is archived beside the numbers:
`perf/mintsel/runtime_census_at_MINTSEL.py`.

---

## 6. Runs

Both under `tools/game_dir_lock.sh run lane-MINTSEL 900`, debug build, DLL
present, `BANISHED_AUTOSTART=1 BANISHED_RT_MINT_SHADOW=1 BANISHED_PAUSEMENU=420
BANISHED_PAUSEMENU_QUIT=120`. md5 of the build and of the deployed exe quoted
and compared immediately before each launch; boot polled with
`tools/boot_state.sh --wait 300`.

| run | md5 (build == deployed) | outcome |
|---|---|---|
| control, `BANISHED_RT_MINT_PROMOTED=0` | `b463dc457afb5e1b2208461fec6d4c90` | boots, autostarts, builds the world, pause menu at frame 721, Quit, clean. Shadow clean at init and exit. |
| promoted (default) | `6122a1e9c97531533c0ec937f8deb2d2` | identical: pause menu at frame 721, Quit, clean. `[RtMint] promoted 10 minted DATA row(s)`. **Zero trap entries.** Shadow clean at init and exit. |

Evidence: `perf/mintsel/{control,promoted}/shadow_report.txt` and `elemtid.txt`.

---

## 7. What this does NOT claim

* **It says nothing about `BANISHED_RT_MINT=1`.** That switch still does not
  boot, for the reason UIDATA measured — our zeroed
  `?_singleton@MemoryAllocator@System@@` over the DLL's live one. Nothing here
  touched a storage row. Per-row selectivity is what makes that blocker
  *irrelevant* to the UI rows rather than what fixes it.
* **It does not claim the 94 unpromoted typeinfo rows are ports.** They are
  READY on the instrument and have no caller; promoting them would be
  unverifiable.
* **It does not claim `fac=2traps` is harmless in general** — only that it is
  unreachable for the consumers this tree has, and that if that changes the trap
  aborts with the symbol in its first line instead of failing silently.

## Files

* `runtime/types/object/TypeInfo.h`, `runtime/src/object/type_system.cpp` —
  `RtObject::TypeIdFromName`, one exported spelling of the existing hash.
* `standalone/rt_data_mint.cpp` — real typeId; per-row registration;
  `BANISHED_RT_MINT_ROWS`; promotion bookkeeping; per-typeinfo readiness table;
  trapped slots named.
* `standalone/rt_data_mint.h` — the selectivity API.
* `standalone/reg_mint_promoted.cpp` — **new**, the ten promoted rows and the
  warrant for each, plus the five not promoted and why.
* `tools/runtime_census.py` — the corrected note.
