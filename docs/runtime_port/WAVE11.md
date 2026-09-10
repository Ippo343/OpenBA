# WAVE 11 — the Object namespace to its ceiling

**Dispatched 2026-09-09 from `wave10/base` @ `fa4fa86`.**
Goal stated by the user: drive the **Object** namespace to 100% of `called`, or
prove its ceiling.

## The scoping correction this wave opens with

`WAVE11_KICKOFF.md` framed the wave as **the object creation path — 202
`Create*Object` exports, zero ported**. That framing is not wrong about the
cutover, but it is **not the goal the user asked for, and it does not move the
Object census at all**: those 202 exports are `called = 0`. The census
denominator is the set our tree *calls*, so porting a factory nobody calls
changes no percentage. Its payoff is the `??_7Dialog` promotion, which is one
row in **UI**, not in Object.

The Object number the user asked about:

```
namespace   total   called   ported   covered
Object        191       56       32    57.1%
```

**24 rows are the whole gap.** They are enumerated below, measured with
`tools/runtime_census.py --namespace Object --unported` on `fa4fa86`.

## The decomposition — 24 rows, three classes

Counted, not asserted. 10 + 5 + 9 = 24.

### Class A — minted DATA rows, none promoted (10)

Every one of these is already generated into
`standalone/rt_data_mint_tables.inc`; **not one appears in
`standalone/reg_mint_promoted.cpp`.** So the work is *promotion under the Wave
10 MINTSEL protocol*, not authoring.

| row | kind |
|---|---|
| `??_7Base@Object@@6B@` | vtable |
| `??_7CompiledExternal@Object@@6B@` | vtable |
| `??_7External@Object@@6B@` | vtable |
| `??_7Internal@Object@@6B@` | vtable |
| `?_firstTypeInfo@BaseTypeInfo@Object@@2PEAV12@EA` | chain head (split brain) |
| `?_typeInfo@External@Object@@0VBaseTypeInfo@2@A` | BaseTypeInfo |
| `?_typeInfo@ExternalList@Object@@0VBaseTypeInfo@2@A` | BaseTypeInfo |
| `?_singleton@Factory@Object@@0PEAV12@EA` | storage |
| `?_singleton@ExternalFactory@Object@@0PEAV12@EA` | storage |
| `?_singleton@InternalFactory@Object@@0PEAV12@EA` | storage |

The three `_singleton@` rows are **Wave 15 phase 2 subjects** and inherit its
doctrine verbatim (`WAVE15_CLOSED.md` §"Phase 2"): UNKNOWN-OWNER counts as
unported; a promotion must register the **address as well as the name**;
`ExternalFactory@Object` is explicitly flagged **NOT cheap tier** (127 of 151
references are unknown-owner).

### Class B — lifecycle (5)

`??0Internal`, `??1Internal` (`UEAA`, virtual), `??1Factory`,
`??1ExternalFactory`, `??1InternalFactory`.

### Class C — real bodies (9)

`ExternalFactory`: `FindOrLoad`, `GetCompiledPath`, `PushPath`, `PopPath`,
`SerializeReferenceTable`.
`External`: `Load`, `SaveText`.
`InternalFactory`: `ClearPersistState`, `ResetPersistState`.

## Lanes

| lane | worktree | branch | subject | rows |
|---|---|---|---|---|
| OBJDATA | `C:/code/GIT/wt-wave11-objdata` | `wave11/objdata` | Class A, mint promotion | 10 |
| OBJLIFE | `C:/code/GIT/wt-wave11-objlife` | `wave11/objlife` | Class B, lifecycle | 5 |
| OBJEXT | `C:/code/GIT/wt-wave11-objext` | `wave11/objext` | Class C, External/ExternalFactory | 7 |
| OBJINT | `C:/code/GIT/wt-wave11-objint` | `wave11/objint` | Class C, InternalFactory + the Dialog-factory probe | 2 + probe |

## Standing rules for every lane

Inherited doctrine, restated because each has cost this project a wave:

1. **Grep for the port before writing it.** Three times in one wave a lane ported
   what the tree already had. Check both the mangled name *and* the address.
2. **Port the CHECK before the code.** A written recipe from a good lane can be
   wrong; a dtor that only frees the block instead of destroying every element
   is a leak under a false byte-identity claim.
3. **`calls=0` is four different findings.** Read the oracle's `calls=` column,
   and if it is zero say *which* of the four causes you established.
4. **A green readiness screen is not a route.** SAFE-to-stamp ≠ anything
   dispatches it. Only an attendance counter separates them.
5. **Do not "fix" a faithful crash with a null check.** Find the upstream null.
6. **The game directory is a mutex.** `tools/game_dir_lock.sh acquire <lane>` —
   **never piped**, a pipeline returns the last command's status and a timeout
   then reads as a grant.
7. **Report the set-diff, not the count.** A closure count moving the right
   direction by the wrong amount reads as success.
8. **Stay in your own worktree.** Never `git checkout`/`switch` a branch.
