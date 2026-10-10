# WAVE 27 KICKOFF — 2026-09-12

Base: `wave27/base` (cut from `wave26/base` at `f5fb839a`). **Seven lanes, every
one a DECOMPILATION lane.** No lane in this wave is scoped to build, fix or
extend a tool. That is a deliberate reversal: Wave 26 was scoped as three
porting lanes and became an instruments wave, and the project now has more
instruments than it has ported bodies behind them.

## 1. BOARD, RE-DERIVED ON THIS TIP

`tools/runtime_census.py` run by the integrator on `f5fb839a` (not quoted from
WAVE26_CLOSED.md; the figures agree):

```
TOTAL   1372 called / 1237 ported / 90.2%      gap 135
```

```
  gap 135 = 18 BODY + 2 DTOR + 42 VTABLE + 46 DATA + 27 UNKNOWN(address-keyed)
```

**Do not scope anything on "135 rows".** The decomposition that matters:

- **7 of the 18 BODY rows are the allocator** — WRITTEN, held behind
  `BANISHED_RT_ALLOC`. Not this wave's work, not any lane's scope here.
- **20 of the 42 VTABLE rows are tagged UNPROMOTABLE.** Real vtable work is **22**.
- **46 DATA rows have no body to write.** Not decompilation. Not in scope.

So the honest scope of this wave is:

```
  11 unwritten bodies + 2 dtors
+ 22 promotable vtables, holding 156 registered bodies across 44 tables
+ the unported SLOTS of those 22 tables  <-- THE ACTUAL WORK
+  1 cutover-frontier body (address-keyed)
```

The lever is the **PROMOTION BACKLOG**: 156 bodies across 44 unpromoted tables,
23 COMPLETE. An incomplete table's missing slots are exactly "decompile this
function", and they are the largest well-defined body surface left in the DLL.

**COMPLETE is the STATIC half of readiness only.** It says promoting would put no
trap in a live table. It does NOT say the row is promotable — that turns on owning
the function that STAMPS the table. A green readiness screen is not a route.

## 2. THE UNPROMOTABLE FILTER WAS APPLIED BEFORE THESE BRIEFS WERE WRITTEN

Wave 26's closing correction demanded it, having been violated in the document
that stated it. The check that was run:

```
grep -o 'RT_UNPROMOTABLE: [^ |]*' standalone/reg_mint_promoted.cpp | sort -u
```

Result: no lane below is handed a table tagged UNPROMOTABLE. Video's entire
12-row vtable gap is UNPROMOTABLE and **Video is not in this wave at all.**
Also excluded: `??_7Base/CompiledExternal/External/Internal@Object@@`,
`??_7Dialog/DialogController@UI@@`, `??_7ReaderWriter/Stream@IO@@`.

**One tag names a lane in this wave as its addressee.** See lane GFXMESH.

## 3. LANES

| Lane | Branch | Scope |
|---|---|---|
| FRONTIER | `wave27/frontier` | The live cutover wall + StringTable's last 2 slots |
| GFXMESH  | `wave27/gfxmesh`  | `EdgeMesh` 0/9, `MeshInstance` 2/9, `EdgeMeshInstance` 4/9 |
| GFXPART  | `wave27/gfxpart`  | `Particle` 5/6, `ParticleInstance` 5/9, `ParticleSystemInstance` 5/10, `Light` 5/6, `SunLight` 5/11 |
| GFXBIG   | `wave27/gfxbig`   | The three large Graphics BODY rows (Renderer::Draw, BuildParameters, AnimationState::Update) |
| LOOPS    | `wave27/loops`    | `LoaderLoop` 6/12, `DialogLoaderLoopT` 8/12, `DialogLoopT` 10/12, `Loader` 1/2, `MainLoop::Draw` |
| ACHIEVE  | `wave27/achieve`  | Achievement's 4 tables, `GoalList::Initialize`, `Audio::Stream` 1/2, `Audio::Scene::Update` |
| IOTEXT   | `wave27/iotext`   | `TextFileStream` 8/10 + 2 dtors + 3 accessors, `PackageFile` 6/9, `Modification::SetupFileSystem`, `ElementDescription` 3/6 |

Every promotable vtable and every non-allocator body row is owned by **exactly
one** lane. No lane may touch another lane's table.

## 4. STANDING WARNINGS FOR EVERY LANE

1. **NO TOOLING.** If you find an instrument defect, **write it down and keep
   porting.** Record it in your lane doc under a heading `INSTRUMENT DEFECT
   OBSERVED, NOT FIXED`, name the file and line, and move on. Fixing it is a
   Wave 28 decision. This rule exists because it has been broken in two
   consecutive waves by lanes that were scoped exactly as you are.
2. **Don't "fix" a faithful crash with a null check.** If a byte-faithful body
   crashes on an unguarded deref, retail is not wrong — something upstream handed
   you a null. Find that. A guard does not restore the behaviour, it skips it.
3. **Grep for the port before writing it.** Three times in one wave a lane ported
   what the tree already had. A body PORTED but never REGISTERED is
   indistinguishable from an ABSENT one to a callee census.
4. **`AEAA` means PRIVATE, not unexported.** This DLL exports its private members.
   An address door justified by the word "unexported" in a comment is usually a
   plain name door.
5. **Read the census `calls=` column, not just `divergences=`.** A port with zero
   callers reports clean. `calls=0` with `worked>0` does not mean unexercised.
6. **Report what you MEASURED separately from what you INFER.** If you did not run
   it, say so. Every closed wave in this project has a defect of the form "an
   asserted number sat beside a measured one and inherited its credibility".
7. **Never `find /`.** Grep/Glob scoped to your worktree.
8. **A residual needs an ADDRESSEE, not just a condition.** "The next lane that
   touches this" waits forever. Name a lane, a wave, or a person.
9. **Put a correction on the surface the next person will REACH** — beside the row,
   in the file they will run, in a tool's printed output. A commit message is a
   permanently uncorrectable artefact; a lane doc has the weakness it describes.
10. **The shape to imitate** is Wave 26 §3.4: the comment at
    `external_load.cpp:147` that named its SYMBOL, its CONDITION and its
    CONSEQUENCE, sat at the call site, and cost the next lane nothing to find.
    Every address door you open must be commented that way.

## 5. DELIVERABLE

Each lane writes `docs/runtime_port/WAVE27_LANE_<NAME>.md` and commits on its own
branch, in its own private worktree. Write the doc **as you go**, not at the end.
The integrator merges and runs cutover attempt 30.
