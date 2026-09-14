# actwork notes (COLL-COMPONENT step 2, Action + Work)

## Converted (GameColl::AsArray)
- 0x1400021d0 (I32 SetSize): action_base_move.cpp, action_pf_astar.cpp, action_pf_ctors.cpp
  (via Ptr8, see below -- ctors.cpp is actually 032e0), action_pf_region.cpp (7 sites, script-replaced),
  action_pf_road.cpp, gatherrawmaterials.cpp, gatherrawmaterials_ctor.cpp (x2),
  consumeproduce.cpp, fishingwork.cpp
- 0x1400032e0 (Ptr8 SetSize): action_pf_ctors.cpp, consumeproduce.cpp, fishingwork.cpp,
  work_act_clear.cpp, work_act_consume.cpp (x2)
- 0x140003110 (Ptr8 SetSize 3-arg fill): pickuprawmaterial.cpp, removeanimal.cpp
- 0x140044810 (I32 SetSize 3-arg fill): createrawmaterial.cpp -- derived row myself,
  reduces exactly to SetSize(n, fill) since the single new slot's write IS the fill.
- 0x14000a590 (U8 SetSize): harvestarea.cpp, harvestorchard.cpp, harvestorchard_ctor.cpp,
  plantarea.cpp, plantorchard.cpp, plantorchard_ctor.cpp
- 0x14001c890 (U16 SetSize) + 0x14001ee40/0x14001f8d0 (new local PfRawElem14 type,
  alignas(2) trivial 0x14-byte POD -- SpecFor<T> gives init=nullptr matching these two
  bodies' "leave new tail untouched" behaviour): all three in action_pf_road.cpp only.

## NOT converted (left alone, with reason)
- 0x14000da70 (consumerawmaterial.cpp, pickupstoredrawmaterial.cpp): body
  (ActRm_ArrayResize16_da70, action_base_rawmat.cpp) zeroes only 12 of its 16 stride
  bytes on the new-tail arm (dst[0]=0 then a uint32 store at +8..+0xc; bytes [0xc,0x10)
  untouched) -- not a plain full-zero element the GameColl types express safely without
  a bespoke type; left as resolved-pointer calls.
- 0x14001d830 (action_pf_region.cpp): new-tail arm writes a NON-zero pattern
  ([+8]=0xffffffff) -- same class of case as the documented "10.0f" non-zero-default
  caution; not converted.
- 0x14001d940, 0x14001e210, 0x14001fa80, 0x14000f9e0 (action_pf_astar.cpp,
  action_pf_road.cpp): each does a deep-copy of an embedded int-vector or a real
  element copy-ctor/dtor (Pf_VectorResize18Dup_d940, Pf_VectorResize30_e210,
  Pf_VectorResize60_fa80, Pf_IntArrayClone_f9e0) -- not SetSizeCore's algorithm.
- 0x14001f250 (action_pf_road.cpp -> Struct60B_Resize_Body in serialize_struct60b.cpp,
  not in my file set): new-tail arm writes a non-zero sentinel (+0x28 = -1) AND the
  element's assign is a real deep-copy (CopyCtor), not a byte copy -- outside GameColl's
  byteCopy=true assumption.
- action_pf_roots_drop.cpp / work_act_hunt.cpp: 0x1400032e0 resolved but never called
  anywhere in either file -- dead resolved-pointer declarations, deleted outright.

## Dead resolved-pointer decls deleted (never called, no conversion needed)
- action_pf_roots_drop.cpp: s_fn_grow (0x1400032e0)
- work_act_hunt.cpp: s_wah_ArrayGrow (0x1400032e0)

## Guessed / derived myself (flag for review)
- createrawmaterial.cpp's 0x140044810 site: derived that the whole body
  (ActRm_DynArrayPush4_44810 in action_base_rawmat.cpp, not mine to edit) reduces to
  `SetSize(newCount, fill)` because the single appended slot's write literally IS the
  fill value -- there is no "plainly separate" store beyond what SetSizeFill already
  does. Recipe named this function as one to double-check.
- PfRawElem14 (action_pf_road.cpp, local to that file): a new element type not in
  coll_bridge.h, used only there for 0x14001ee40/0x14001f8d0. `alignas(2)`,
  no NSDMI, verified trivially-default-constructible via static_assert so
  rtcoll::SpecFor<T> emits init=nullptr, matching both bodies' behavior of leaving
  the new tail's bytes as whatever the allocation contained.
- action_pf_ctors.cpp Pf_ActStackPush_54e0 and work_act_consume.cpp WCP_AppendInline:
  both pass a "stack" struct pointer whose caller-side field comments imply a larger
  layout (capacity/count fields beyond +0x10), but 0x1400032e0 (SetSizeCore) only ever
  touches bytes [0x00,0x10) at the pointer it's given, so the conversion is safe
  regardless of what the surrounding struct keeps further out. Called this out in-code.

## Counts
- Resolved-pointer declarations deleted: 15 (021d0 x6 files, 032e0 x5 files incl. 2
  dead, 44810 x1, da70 x0 [not converted], plus c890/ee40/f8d0 collapsed into one
  file's block comment).
- reinterpret_cast, code/Game/Action: 2210 occurrences / 39 files (current, post-edit).
- reinterpret_cast, code/Game/Work: 2688 occurrences / 53 files (current, post-edit).
  No pre-edit whole-directory baseline was captured (only touched-file diffs); the
  conversions net roughly flat -- deleted casts belonged to removed fn-ptr resolution,
  new casts appear in the bridge call sites' pointer-to-header casts.

All 25 assigned files compile individually with the required g++ invocation
(incl. -Wall -Wextra -Wno-cast-function-type per coordinator's correction),
exit 0, zero output.

## Pre-existing warnings silenced (unrelated to Collection work, found only
## because of the -Wall -Wextra re-check; fixed with [[maybe_unused]] / (void)
## casts, no behavior change)
- fishingwork.cpp: FishingWork_Slot6_Impl's unused param2/param3 -> (void) casts.
- harvestarea.cpp: dead diagnostic counters g_ha_nopass/g_ha_nopath/g_ha_ok, and
  an unused `cur` debug local in a stock-printing block -> [[maybe_unused]].
- harvestorchard.cpp: `cellFloat` is set (line 589) but genuinely never read
  (comment at line 543 confirms it's a leftover) -> [[maybe_unused]].
None of these touch the Collection/array bridge conversions.
