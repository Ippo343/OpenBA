# WALKCACHE -- the slot-8 tree walk visits only the components that tick

Block WALKCACHE, 2026-10-12. ELPROF section 6, item 3. Faithful perf work in the style of the
TREE_SNAPSHOT block: no toggle, no registry entry, no settings line, no new flag. It rides the
snapshot walk, so `BANISHED_PERF_TREE_SNAPSHOT=0` (the existing kill-switch) also turns it off.

## 1. What changed

| file | change |
|---|---|
| `src/game/Entity/tick_list.h` (new) | `TickList::Retail` / `RetailFrom` = retail's 0x1400c2de0 loop (moved here verbatim, one copy); `TickList::Entry` (16 bytes); `Build`, `Cached<V>`, `IndexOf`, `StrictlyAscending`. Templated on the entity type so the unit tests drive it with plain structs. |
| `src/game/Entity/entitylist.cpp` | `ComponentEntity::TickComponents` = `TickList::Retail` with the slot-8 call (COMPPROF=2 arm unchanged, now `TickOneComponent`). Beside `s_walkSnap`: `s_tickList` (one `Entry` per snapshot entity) and `s_tickSorted`, reset with the snapshot. The snapshot walk calls `TickList::Cached<false>` (or `<true>` under `g_verifyOn`); the i+1 prefetch fetches only the listed components when the entry is built. `EntityList_TickListInvalidate(entity)`: binary search of the snapshot, mark the entry stale. |
| `src/game/Entity/componententity.cpp` | `RefreshEntityFlags` (0x1400c3470) calls `EntityList_TickListInvalidate(this)` first. |
| `src/game/Entity/EntityList.h` | the declaration. |
| `tests/unit/tick_list_test.cpp` (new) | 11 tests, section 5. |

How it works: an `Entry` holds the indices (at most 4) of the entity's components whose
`_flags` bit 2 was set when it was built, plus the component array's `(data, size)` as a key.
States: `kStale` (build after the next retail pass), `kCached`, `kWide` (more than 4 candidates,
more than 255 components, or a null slot: always retail). A visit:

1. the entity checks exactly as retail: `_entityFlags` bit 2 set, bit 10 clear, count > 0, the
   count read once;
2. stale entry or key mismatch: retail's loop (`RetailFrom(0, n)`), then `Build` from the flags as
   they are after the ticks (skipped when the entity got 0x400); `kWide` with a matching key:
   retail's loop, no build;
3. cached: for each listed index, re-read the slot, test the live bit 2, re-read the slot, tick;
   after a tick, break on 0x400; if the entry went stale or the key changed during the tick,
   `RetailFrom(ci + 1, n)` (n = the count read at entry, as retail) finishes the entity and the
   entry is rebuilt.

The snapshot is the update tree's in-order walk and the tree is keyed by the entity pointer
(retail compares it unsigned, `EntityList.h`), so `s_walkSnap` is sorted and the entity -> entry
lookup is a binary search; `StrictlyAscending` re-proves that at every rebuild, and if it ever
fails every entity takes the retail loop (`s_tickSorted = false`).

## 2. Every writer, and how each reaches the cache

### Component `_flags` (+0x20) writers after construction -- every one that can touch the low word follows retail's idiom
`old = _flags; _flags = new; if ((old ^ new) & 0xffff && _entity) _entity->RefreshEntityFlags();`
(found by grepping every `_flags =`, `|=`, `&=` in src/game, src/ext, src/app, src/diag, plus
`OrFlagsInCtor`, `Serialize(_flags`, `&_flags` and raw `+ 0x20) =` stores):

| site | bits | refresh |
|---|---|---|
| `Component::ModifyFlags` growthcomponent.cpp:149 (callers animalcomponent.cpp:529, growthcomponent.cpp:535) | any | idiom |
| `NaturalResourceComponent::ResetGrowthState` growthcomponent.cpp:193 | set 21 | idiom |
| GrowthComponent growthcomponent.cpp:487 / :545 / :639 | set 4 / clear 3,4 / ... | idiom |
| `Component::FlagForRemoval` action_pf_ctors.cpp:121 | set 20 | idiom |
| `ModelComponent::Tick` modelcomponent.cpp:338 | **clear 2** | idiom |
| action_base_movedeps.cpp:115 (PlayAnimation) | **set 2** | idiom |
| action_base_cnr.cpp:160 / :240 | clear 4 / **set 2** | idiom |
| naturalresourcecomponent.cpp:57 | **clear 2** | idiom |
| particlecomponent.cpp:274 / :306 / :662 | **clear 2 / set 2 / clear 2** | idiom |
| statusiconcomponent.cpp:181 / :243 / :259 | **set 2 / clear 2 / set 2** | idiom |
| citizenai_handlers2.cpp:262 / :279 (status icon) | **clear 2 / set 2** | idiom |
| workplacecomponent.cpp:182 / :200 (status icon) | **clear 2 / set 2** | idiom |
| tool_base_render.cpp:471 / :611 (highlight) | **clear 2 / set 2** | idiom |
| work_act_clear.cpp:235, work_base_actionctor.cpp:297 | set 20 | idiom |
| removenaturalresource.cpp:146, removeanimal.cpp:124, pasturework.cpp:471 | clear 20 | idiom |
| keystone_a_map.cpp:380 / :417 | set / clear 20 | idiom |
| herdcomponent.cpp:215, pasturecomponent.cpp:113 / :159 | set / clear 22 | idiom |
| pasturecomponent_states.cpp:221 / :272 | clear 21 / set 23 | idiom |
| tradecomponent_livestock.cpp:195 | clear 22 | idiom |
| diseasecomponent.cpp:539 `natRes->_flags \|= 0x200000` | set 21 | **none** -- high word, cannot touch bit 2 |
| mapcomponent.cpp:136 `&= ~0x100000` | clear 20 | **none** -- high word |

So every write that can change bit 2 after construction is followed, synchronously, by
`RefreshEntityFlags` on the component's `_entity`, which stales that entity's entry.
`RefreshEntityFlags` has one body (the old second copy `NotifyEntityChanged` is gone).

### Construction and load
- `Component()` sets 0x80 and ~60 leaf ctors `OrFlagsInCtor(...)`: `_entity` is null and the
  component is in no entity's array yet.
- `ComponentEntity(ComponentDescription*)` (0x1400c3720) fills the array, then `RefreshEntityFlags`.
- `ComponentEntity::Persist` load: `Component::Persist` (component.cpp:54) reads `_flags`; the
  entity calls `RefreshEntityFlags` at the end only when it has a description.
- In every case the entity is new: it reaches the update tree through `stagingUpdate` ->
  `PreUpdate`'s insert, which marks the snapshot dirty, and a rebuilt snapshot starts with every
  entry `kStale`. An entity in the tree is never re-loaded or re-constructed.

### The `_components` array
Writers: the ctors (componententity.cpp SetSize / slot stores / the `_size` correction), the dtor
(nulls each slot), `Persist` (SetSize on load, each slot's `Serialize`, the `IsDerivedFrom`
null-out, the load-time insert with its shift). None runs on an entity during the walk; the ctor
and `Persist` end in `RefreshEntityFlags`, and the `(data, size)` key catches a move or a resize
even without a refresh (a unit test covers that path). Retail's count-read-once is kept: the
remainder after a mid-loop stale uses the entry count.

### Entity deletion
`MarkForRemoval` sets 0x400 (action_base_hunt.cpp:75); `RefreshEntityFlags` can drop it again
(it recomputes the word from the components, as retail does). Both are read live, never cached.
Removal from the tree (`DrainRemovals` / `PreUpdate`) marks the snapshot dirty, which resets the
side table; a freed entity's address cannot reappear in a live snapshot.

## 3. The identity argument

Retail reads, for each component in order: the slot, the component's `_flags`, and (when bit 2)
the slot again, the tick, then the entity's 0x400. Reads have no side effects (`Get()` is a load,
`CheckPtr` a bare RET). The cached walk differs only in the reads it skips: components not in the
list. Claim: each skipped component has bit 2 clear at the moment retail would have read it.

- At build time it had bit 2 clear (that is why it is not listed).
- Any later write that sets it is followed by `RefreshEntityFlags` on its entity (section 2),
  which marks the entry stale before the write's caller returns.
- Between two ticks of one entity no code runs, so nothing can change in between; within a tick,
  the entry state and the key are re-checked right after it returns.
- So at a visit with a cached entry, no skipped component has gained bit 2 since the build, and
  inside a visit, the first tick that could have armed one is followed by the hand-over to
  `RetailFrom(ci + 1, n)`, which is retail's loop from exactly the next component.

The listed components are still tested live, so a clear (ModelComponent's own) behaves as retail
whether or not a refresh follows.

**Deleted bit mid-loop.** Retail tests 0x400 after every component, ticked or not. The bit can
only change inside a tick, so testing it after every tick is the same: the walk breaks after the
same tick. It is tested before the stale check, so a tick that both marks the entity and stales
the entry stops the entity (as retail) instead of handing over.

**Flag cleared during a tick** (ModelComponent::Tick, ParticleComponent): the clear stales the
entry; if that component was the last listed, the walk returns as retail would after reading the
rest (all clear); otherwise `RetailFrom` reads them exactly as retail does. The next visit rebuilds.

**Flag set during a tick** on a later sibling: stale -> `RetailFrom(ci + 1)` ticks it in the same
pass, as retail. On an earlier sibling: next pass, as retail. On another entity: that entity's
entry is stale; its next visit is a retail pass.

## 4. Verification hook (COMPPROF=4)

`WalkUpdateTreeSnapshot` (entitylist.cpp) reads `g_verifyOn` once per walk; when set it calls
`TickList::Cached<true>`, which, for every cached visit, reads every skipped component's bit 2 at
the moment retail would have read it (before the next listed one, and the tail after the last)
and reports the visit once: `StressHarness_Verify("TickList", ok, detail)`, detail = the first
offending component index (-1 when ok). Off: one branch per walk, `Cached<false>` has no shadow
code. Stale / wide visits are retail passes and are not reported.

## 5. Unit coverage (`tests/unit/tick_list_test.cpp`)

Three copies of one scripted world: retail, cached, cached + shadow; logs of (iteration, entity,
component) must be equal, the shadow must report no mismatch, and the final flags and counts
must match. Scripts mutate from inside ticks through a mock `RefreshEntityFlags` (OR of the low
words, drops 0x400, stales the entry), exactly as the game's writers do:

- static mix (2 of 9, 1 of 3, 1 of 5, none, 6 of 6 = wide, stale entity word, no components);
- `Build` states (indices, key, wide, null slot);
- a component clearing its own bit on a tick (ModelComponent) and a sibling re-arming it;
- arming a later sibling (same pass) and an earlier one (next pass), plus a self clear;
- arming / clearing components of other entities, before and after in the walk;
- append (array moves), insert at the front (indices shift), remove in place (count - 1);
- an array change with no refresh (caught by the key alone);
- 0x400 mid-loop, on the last candidate, together with a stale, and un-marked by a later refresh;
- the entity word losing bit 2 and being re-armed;
- a deliberate bit-2 write WITHOUT a refresh: logs differ and `Cached<true>` reports it (the
  shadow works);
- `IndexOf` / `StrictlyAscending`.

Mutation check (done once, reverted): removing the mid-loop hand-over fails 3 tests; removing
the post-tick 0x400 break fails the deleted test.

## 6. Memory and cost

16 bytes per entity in the update tree (2,630 in the heavy town: 42 KB), parallel to the 21 KB
snapshot; nothing added to `ComponentEntity` (0x48, unchanged). `RefreshEntityFlags` gains one
binary search (~12 probes) of the snapshot. Expected effect (ELPROF): the walk stops loading the
non-ticking components (a character has ~10 components, 2 tick): about -5% of EntityList. Not
measured here: the coordinator runs the stress town (`BANISHED_STRESS_COMPPROF=4` for the
shadow, `TickList` counter in the [COMPPROF] report).

## 7. Doubts

- The invalidation goes to the component's `_entity`. If a component ever sat in the array of an
  entity other than its `_entity`, a write would stale the wrong entry. Every attach sets
  `_entity = this` (ctor, Persist); the shadow would catch a violation.
- `RefreshEntityFlags` is assumed main-thread (the walk is). No writer was found on a worker.
- `kMaxIdx = 4`: an entity with more than four slot-8 components always runs retail (correct,
  no gain). The profile says 1-2 is the norm.
- `TickComponents`' body now lives in `tick_list.h` (one copy shared with the cached path); the
  tree walk (kill-switch) still calls `TickComponents`, so its behaviour is unchanged, but its
  code moved.
- `Build` refuses a null slot (`kWide`); that is not a guard on a retail read: `Build` itself is
  extra reads, and it must not add a dereference retail's loop would not make.
- Not run in the game by this block (rules): first verification is the coordinator's COMPPROF=4
  stress run; expected `TickList` mismatches = 0.

## Benchmark (coordinator, 2026-10-12; idle machine, `machine load: CLEAN`; both PATHDIST and WALKCACHE live)

| town | run | sim ms/iter | Update ms/frame | EntityList | tree walk | buckets | CitizenList | PathDistance ms/frame |
|---|---|---|---|---|---|---|---|---|
| heavy (large, 1600, spread), before | `20261012_105902_pfh_faithful` | 0.32 | 3.09 | 2.62 | 2.05 | 0.62 | 0.21 | 0.44 |
| heavy, after | `20261012_144605_elperf_heavy` | 0.29 (-9.7%) | 2.87 | 2.51 | 1.96 | 0.53 | 0.08 | 0.17 (-61%) |
| compact (default town), before | `perf/baselines/current.csv` / `20261012_093810_pf_faithful` | 0.17 | 1.70 | 1.45 | 1.15 | 0.29 | 0.03 | - |
| compact, after | `20261012_144816_elperf_compact` | 0.16 (-5.5%) | 1.63 | 1.39 | 1.09 | 0.29 | 0.03 | 0.04 |

Attribution: the tree walk is WALKCACHE's (-4.7% heavy, -5.0% compact, as ELPROF estimated); the
bucket phase (-15%), CitizenList (-0.13 ms: `CitizenBehavior` calls PathDistance too) and the
PathDistance bracket are PATHDIST's. Shadows at COMPPROF=4 in the heavy town
(`20261012_143830_both_verify`): PathDistance 126,553 checks, TickList 155.7M checks, 0 mismatches.

### Speed 50, heavy town, RTSS stopped (coordinator, 2026-10-12; `20261012_150008_elperf50_heavy` vs `20261012_130927_pfh50_faithful`, rows 1-3 = before the town starves; CLEAN)

| window | citizens | Update ms/frame before -> after | ms/iter | fps before -> after |
|---|---|---|---|---|
| 1 | 1600 | 10.5 -> 9.1 | 0.21 -> 0.18 | 186 -> 225 |
| 2 | ~1900 | 12.7 -> 11.4 | 0.25 -> 0.23 | 128 -> 150 |
| 3 | ~1870 | 14.9 -> 13.5 | 0.30 -> 0.27 | 57 -> 85 |

The third window gains more fps than its 10% of Update: an Update that stays under the 16.7 ms step
no longer piles a second Update into the same Draw, so the accumulator effect amplifies every
millisecond cut near the step. One run each.
