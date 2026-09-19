# B.3.4 session kit (2026-09-19) — reference, not live tooling

Copied out of the coordinator's session scratchpad at the B.3.4 close so step 5 can reuse the
shapes instead of re-deriving them. Paths inside the scripts point at that (gone) scratchpad:
copy a script into the new session's scratchpad and fix `ROOT` / the run directory first.

| file | what it is |
|---|---|
| `gate.sh` | the coordinator's per-merge gate: 4 builds, new-warning SET diff vs a clean-rebuild baseline, unit, slot map / manifest / mint checks, three routes with per-route counters, pointer-free type table vs baseline, resave xN with `reftable_diff.py` on a DIFF. Edit with a file-writing tool only — a shell heredoc eats its backslashes. |
| `reftable_diff.py` | classifies a resave DIFF by WHICH BYTES differ (count field only / whole resident rows incl. duplicates / real). Exit 0 identical, 10 race-shaped, 20 real-shaped. Never turns a DIFF into a PASS. |
| `mods_route.sh` | the Mods route with the `.pkm` files mounted, restored on exit. |
| `CALLER_BATCH_BRIEF.md` | the brief every Serializer caller batch ran under. |
| `SYSTEM_CALLER_BRIEF.md` | the same for the System caller batches (eight binding spellings). |
| `CLOSING_4A_BRIEF.md` | the closing pass's four commits. |
| `GUARD_SWEEP_SPEC.md` | the closing owner's measured spec for removing guards and door caches together, by directory, globals last. |
| `sweep.py` `occ.py` `zero.py` `shapes.py` `bydir.py` | the closing pass's binding-resolving sweep (positional resolution of scratch locals). KNOWN BLIND SPOTS: a resolver call wrapped over two lines; caches bound through a file-local helper (`ResolveSlot(addr, cache, tried, tag)`, `RSlot`, `ResolveSlotPtr<T>`); handle-struct fields. Cross-check with a raw slot-literal grep. |
| `warn_ab2.sh` | per-file warning A/B with BOTH flag sets, base copy materialised beside the real file. Never run it during a build (its temp `__abbase_*.cpp` joins the source glob). |
