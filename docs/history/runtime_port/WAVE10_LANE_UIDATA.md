# WAVE 10 — lane UIDATA: can the 13 minted UI DATA rows go live?

Branch `wave10/lane-uidata`, cut from `wave10/base` (3318690).
Worktree `C:\code\GIT\untitled_rev_eng_project\.claude\worktrees\agent-a707fd103374b0cdf`.

## The question

`tools/runtime_census.py --unported --namespace UI` reports 29 rows. Thirteen of
them are DATA with no body to reimplement:

| kind | symbols |
|---|---|
| 4 vtables | `??_7DialogController@UI@@6B@`, `??_7DialogInstance@UI@@6B@`, `??_7Element@UI@@6B@`, `??_7SpriteSheet@UI@@6B@` |
| 9 typeinfo | `?_typeInfo@` of `CheckElement`, `ClipRegionElement`, `Dialog`, `DialogInstance`, `ImageElement`, `LabelElement`, `NumberEditElement`, `ScrollElement`, `TextElement` |

All 13 are already minted in `standalone/rt_data_mint_tables.inc`. They are not
registered, because `RegisterMintedData()` runs only under `BANISHED_RT_MINT=1`,
and the census's own rule is that a gated registration is not a port.

**Verdict: NO. None of the 13 can be promoted today, and the reason is
per-symbol and measured, not a blanket "the mint is gated".**

## VERDICT SUMMARY

| rows | blocker | what unblocks them |
|---|---|---|
| 4 vtables | 27 of their 42 non-purecall slots are **abort traps** | port those 27 slot bodies (lane-B class work) |
| 9 typeinfo | our minted `typeId` is the ctor sentinel `0xFFFFFFFF`, forever | nothing short of cutover — see §4 |
| all 13 | `BANISHED_RT_MINT` is **one global switch** over 55 vtables + 104 BaseTypeInfo + 38 statics, and the switch does not boot | selective registration would have to be built first, and even then §2/§3 still bite |

## 1. What `BANISHED_RT_MINT=1` does today

`RtDataMintInit()` (`standalone/rt_data_mint.cpp`) always MINTS — lays out 55
vtables (527 slots), 104 `BaseTypeInfo` and 38 `.data` statics (11328 bytes) —
regardless of any flag. `BANISHED_RT_MINT=1` adds exactly one step,
`RegisterMintedData()`, which publishes **every** row through **both** doors:
`RegisterRuntimeExport(symbol, storage)` and
`RegisterRuntimeAddr(dllVa, storage)`.

There is no per-row and no per-namespace selectivity. It is one switch over all
197 rows. That matters for this lane's question: the 13 UI rows are not gated
because anybody assessed UI as unsafe — they are gated because they share a
switch with 184 other rows.

A slot in a minted vtable holds either our registered port or a **named trap
that calls `abort()`**. Never a DLL address, by design.

## 2. Running it: `BANISHED_RT_MINT=1` does not boot with the DLL present

Measured on the normal DLL-present path, debug build, this lane's exe, under
`tools/game_dir_lock.sh`.

**Run A — control, `BANISHED_RT_MINT_SHADOW=1` only (mint built, not registered).**
Boots, autostarts, plays, quits clean. Shadow report:

```
[RtMint] minted 55 vtables (527 slots: 195 ported, 184 unported-trap, 148 purecall-trap),
         104 BaseTypeInfo, 38 storage symbols (11328 bytes)
[RtMint] 15 of 55 vtables are COMPLETE
[RtMintShadow] ground truth : 527 slot comparisons vs the live DLL, 0 MISMATCH
[RtMintShadow] minted from OUR : 195 slots, 0 MISMATCH
[RtMintShadow] ==== clean: 0 problem(s) ====
```

So the harvest itself is sound: 527/527 slot targets and every `BaseTypeInfo`
string field agree with the loaded DLL. Nothing below is a defect in the mint's
*data*.

**Run B — `BANISHED_RT_MINT=1 BANISHED_RT_MINT_SHADOW=1`.**
Dies during boot, before the UI exists. `standalone_crash.log`:

```
Exception: 0xC0000005 (ACCESS_VIOLATION)
Fault: WRITE of 0000000000000648
RCX=0000000000000640   RBX=0000000000000004
#1  Runtime-x64.dll + 0xC7A8A     (= 0x1800c7a8a, MemoryAllocator, next door to
                                     ?MemFree@ at 0x1800c7ad0)
```

`RCX = 0x640` with a write to `0x648` is `EnterCriticalSection(self + 0x640)` on
a **null** `self`. `+0x640` is confirmed as the allocator's critical section by
this same run's own line
`[AllocLock] MemoryAllocator::Free is now thread-safe (takes self+0x640)`.

The null is `?_singleton@MemoryAllocator@System@@0PEAV12@EA`. The mint registered
our **zeroed** storage for it, so the first caller that resolved the allocator
singleton by name or address got our zero instead of the DLL's live pointer. This
is precisely the split-brain `rt_data_mint.h` predicts in prose — now measured:

> "While the DLL is loaded, its own code keeps using its own singletons, so
> handing our callers a second, empty set would split the process in half."

**Consequence for this lane:** the global switch is not a route to the 13 UI
rows at all. Its blocker is in the 38 storage rows, which have nothing to do with
UI. Answering "can these 13 go live" therefore required asking about the 13
directly, which is §3 and §4.

## 3. The 4 vtables: BLOCKED, 27 abort traps between them

Registering a UI vtable is not a passive act. Our ported constructors **stamp**
it into objects — `runtime/src/ui/element.cpp:291` is explicit about why:

```cpp
// The exported data symbol the ctor stamps. Resolved by NAME, so our objects
// and the DLL's carry the identical vtable pointer.
void* ElementVTable() { static void* vt = ResolveExport("??_7Element@UI@@6B@"); return vt; }
```

`ResolveExport` is exactly the door `RegisterRuntimeExport` intercepts. Register
the mint and that sentence stops being true: our Elements get OUR table, the
DLL's get the DLL's, and the DLL's own `UserInterface::Update` then dispatches
through ours on every element every frame. Same shape at
`runtime/src/ui/sprite.cpp:316` (`SpriteSheet` ctor),
`runtime/src/ui/dialog.cpp:1180` (`DialogController` ctor) and
`dialog.cpp:543` (`DialogInstance`, via `RT_DATA_SLOT`).

So the question is how many slots of each table are traps. Measured **twice, by
two independent screens that were scored separately**:

* a source grep for `RegisterRuntimeExport("<slot name>")` / `RegisterRuntimeAddr(<va>)`;
* a **live** screen added by this lane — the per-vtable readiness table now
  printed by `RtDataMintShadowReport`, which asks the port registry itself
  through the same `OurPort()` the mint fills with, so it sees registrations
  whose name arrives through a variable (the blindness in
  `reference_literal_grep_blind_to_variables.md`).

Both screens returned the same four rows. That agreement is a result, not an
assumption, and it is only a result because both were run.

```
[RtMintShadow]   BLOCKED  ??_7DialogController@UI@@6B@   slots=11 ported= 2 TRAP= 9 purecall= 0
[RtMintShadow]   BLOCKED  ??_7DialogInstance@UI@@6B@     slots=10 ported= 8 TRAP= 2 purecall= 0
[RtMintShadow]   BLOCKED  ??_7Element@UI@@6B@            slots=12 ported= 3 TRAP= 8 purecall= 1
[RtMintShadow]   BLOCKED  ??_7SpriteSheet@UI@@6B@        slots= 9 ported= 1 TRAP= 8 purecall= 0
```

**27 traps across 42 non-purecall slots.** Among them `?Update@Element@UI@@UEAA…`
and `?Type@`/`?TypeName@` on all four classes — the hottest virtuals in the UI.
Registering any one of these four rows today converts a working frame into an
`abort()` with a `[RtMint] FATAL` banner.

Zero of the 15 READY vtables in the whole mint are UI (they are 11 DX11 `*Data`
tables plus `BinaryWriter`, `Geometry`, `Thread`).

## 4. The 9 typeinfos: BLOCKED by a silent wrong value, which is worse

These have no traps to hit. They fail differently, and the failure makes no
noise.

Every reader of a `?_typeInfo@` takes its **first dword, the type id** — e.g.
`runtime/src/ui/element.cpp:319` (`ClipRegionTypeId`, `return p ? *p : 0u;`).
Our minted `BaseTypeInfo::typeId` is set to the ctor's sentinel `0xFFFFFFFF` and
is only ever overwritten by `Factory::Startup` walking the chain it is on. Ours
sit on `g_mintTypeInfoHead`, a chain nothing but the mint knows about. Run A's
own report says so for all nine:

```
[RtMintShadow] INFO ?_typeInfo@CheckElement@UI@@0…A.typeId:
                    minted 0xffffffff (ctor sentinel), DLL 0x1039fdd3
… ClipRegionElement 0x352b9ada · Dialog 0x4fcc106b · DialogInstance 0xbe12e2eb
… ImageElement 0x35413056 · LabelElement 0x4f8656b9 · NumberEditElement 0x25a24564
… ScrollElement 0x1c6afffa · TextElement 0x1a20cede
```

Register the nine and every reader gets `0xFFFFFFFF` where retail has a CRC. No
crash, no log line — just element lookups that never match.

Registering `?_firstTypeInfo@` as well does not rescue it. `Factory::Startup`
**is** ported and registered (`runtime/src/object/type_system.cpp:1206`), so ours
would then walk our 104-node chain and fill our ids — but the DLL's own internals
read their `_firstTypeInfo` through a direct RIP-relative load no registry can
intercept, so the DLL's chain would go un-Startup'd and the split-brain moves
rather than closes.

**The tree already knew this and wrote it down.** `standalone/reg_element_typeids.cpp`
— the file that supplies these very type ids to the exe's 21 `.rdata` slots, and
which is **default-on** (`BANISHED_LIVE_ELEMENT_TIDS` is in `kDefaultOnFlags`;
this run logged `[ElemTid] registered 21/21`) — says in its header:

> "Pointing the slot at the Runtime's own BaseTypeInfo export is both simpler and
> strictly more faithful: the Runtime fills the id inside its own Startup exactly
> as in retail, so there is **no window where the storage holds 0xFFFFFFFF**."

That file is itself a live consumer of six of the nine
(`ImageElement`, `TextElement`, `LabelElement`, `CheckElement`, `ScrollElement`,
`NumberEditElement`) and reaches them through `ResolveExport`. Registering the
mint would hand it our sentinel and quietly poison all 21 slots.

**This is the "minted vtable our own source does not name" blind spot in its
typeinfo form**, and it is worth stating exactly: the generator mints what our
source names, and here our source *does* name these nine — but it names them to
read a field (`typeId`) that the mint does not, and cannot, reproduce while the
DLL owns the chain.

## 5. Conditions under which the 13 could be promoted

* **The 4 vtables** — port the 27 trap slots listed by the new readiness table.
  Then each row flips to `READY` on its own, and can be registered independently
  of the storage rows *if* `RegisterMintedData()` is first given per-row
  selectivity, which it does not have today. `??_7DialogInstance@UI@@6B@` is the
  near one: 2 traps (`?Type@`, `?OnOpened@`).
* **The 9 typeinfos** — no condition short of cutover. Their value is correct
  only once the DLL is gone and our `Factory::Startup` owns the only chain there
  is. Until then the DLL's copy is strictly better than ours and the existing
  `reg_element_typeids.cpp` route already delivers it.

## 6. A census note for the integrator

`tools/runtime_census.py` prints, of the minted rows:

> "They therefore cannot move the columns above at all, in either direction — do
> not read this as a shortfall."

For the totals that is right. For `--unported` it is **not**: all 13 appear in
the "Called but not ported (29)" list, because our source calls `ResolveExport`
on them and nothing registers them. The census excludes `MINT_FILE` from the
*ported* side but not the *called* side, so a minted-but-unregistered row is
counted as an outstanding call. That is arguably the correct accounting — the
call really is unsatisfied by our code — but it contradicts the printed note,
and it is why these 13 look like UI work when no UI work would move them.

Suggested wording, not applied here (it is the integrator's tool): the note
should say the mint cannot move the *ported* columns, and that minted rows do
still appear under `--unported` until registration.

## Artifacts

* `standalone/rt_data_mint.cpp` — adds the per-vtable readiness table to
  `RtDataMintShadowReport`. Shadow-only, no behaviour change on the normal path.
* Census before and after this lane: **UI unported = 29, unchanged.** No rows
  were promoted, because none of the 13 is safe to promote.
