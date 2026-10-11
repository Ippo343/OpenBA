# B9 block RT -- hand-in

Branch `b9-RT`, base `cf62fb35a`, main merged (`b0d547ff7`, docs only). Static gate only, at the
tip: debug / perf / unit builds exit 0, 0 new warnings (debug, perf), unit 920 / 0, **castmetric 2**.

| item | ruling | commit | B11 (perf) | left / measurement |
|---|---|---|---|---|
| A. introsort escape (D4), `rtsort::Array16Assign` 0x180050d30 | MOVED: deleted; the callers use `Collection::Array<EmitterInstance>::operator=` (StopEffect3D, Scene::Update, EffectSlotCopy); the `int64_t empty[2]` is a typed `const Collection::Array<EmitterInstance>` local in a block. `Assign` matches 0x180050d30 in effect (self test, J1 clear, size/treble rule, MemAlloc(cap*16, 4), computed-address test; the four DWORD MOVs are one 16-byte move of the same bytes) | `2f815c78b` | audio_scene_update DIFF 1 (Scene::Update), audio DIFF 2 (StopEffect3D callee rename; Scene::Scene EH pad calls the out-of-line ClearCallback), introsort IDENTICAL 8 SAME 1 | **LEFT, new measurement:** the free is NOT gone. After `call rtcoll::ArrayAssignCore` GCC keeps `mov 0x70(%rsp),%rcx / test / je / call System::MemoryAllocator::MemFree` (never taken) where retail has the bare stores 0x1800dfd19 / 0x1800dfd1d. `Assign` is inline, but its core `rtcoll::ArrayAssignCore` (array_destroy.cpp) is out of line, so `&empty` still escapes. Owed: that core inline. Row 880 status updated. |
| B. row 1163 `RangeResize` | FIXED: both calls are `RawArraySetSize(*range, n)` (SetSizeCore, GeometryDrawRange's trait row); RangeResize deleted | `c2edd2e5d` | water_contour_mesh DIFF 2 (callee RangeResize -> SetSizeCore, the expected ones), SAME 2, ONLY-BASE 1 | -- (behaviour: treble on a +1 grow, plain MemAlloc, retail's copy and free) |
| C. row 1164 `s_tidCompEnt &&` | FIXED: dropped | `d1f2280b4` | gameupdateloop_serialize SAME 1 (the `test/setne` + `test/je` pair gone) | -- |
| `QwordArraySetSize` 0x140003110 | MOVED: 19 callers on `Array<T>::SetSize(n, x)` / `SetSizeFill(n, &x)` (all holders are kSpec_PtrZero arrays = the body); body + 2 wrappers deleted; tool_harness's `%p` print of the core dropped (C6) | `f918346a2` | b11pair: all callers IDENTICAL / RENAMED-IDENTICAL / SAME (the 4th core argument `lea kSpec_PtrZero`); tool_harness DIFF 1 = the dropped printf | -- |
| `BuildComponent_FindInQueue` 0x140017550 | MOVED: `Array<T>::Find(const T&)` over the erased core `rtcoll::ArrayFind8` (array.cpp) | `78e861a83` | ArrayFind8 IDENTICAL to the old body (objdump); BuildComponent::OnRemove DIFF: it CALLs the core where base had it inlined -- retail CALLs 0x140017550 (0x140108ce1) | -- |
| `CemeteryListErase_Reimpl` 0x140002cc0 | MOVED: `Array<T>::Remove(const T&)` over `rtcoll::ArrayRemove8`; 29 call sites (the commit body's "26" miscounts the 27 regex sites + livestock's 2); healer's local `extern "C"` deleted | `78e861a83` | callers IDENTICAL / RENAMED-IDENTICAL / SAME; SetPasture's compare count delta is an operand spelling (`cmp (%r12)` -> `cmp 0x0(%rbp)`); ArrayRemove8 SAME as the old body (size read as a dword, as retail) | -- |
| `Reimpl_Array_Resize_Stride08` 0x140001bb0 | `if (!arrayHeader) return;` DROPPED (behaviour:); STAYS FREE, re-tagged | `937e3529c`, `46d968629` | serialize_arrays SAME 1 (the test gone) | holders: WorldGeometryArray / RawArray headers, AmbientEmitterDescription's raw triple; the Collection::Array<T*> holders (MenuScene, Tutorial, TutorialStep) sit on the trait row (tail unwritten) where retail zeroes it -- moving needs them typed on kSpec_Ptr8Zero (headers outside RT's files) |
| `ActRm_ArrayResize16_da70` 0x14000da70 | STAYS FREE, re-tagged | `46d968629` | IDENTICAL (comment) | holders: 3 POD triples (Consume / PickupStored EntryList, MarketWork::CarryList) + TrackerComponent::EntryList (kSpec_Fill16). No row has da70's init arm (qword + dword zero, +0x0c unwritten); kSpec_Fill16 has none -- unobservable today (nobody calls TrackerComponent's own SetSize) |
| `ActRm_VectorClone_d9d0` 0x14000d9d0 | STAYS FREE, re-tagged | `46d968629` | IDENTICAL | same holders; TrackerComponent's `Assign` over ArrayAssignCore IS this body |
| `PathPairs_Clear_25b30` 0x140025b30 | STAYS FREE, re-tagged; body banner corrected | `46d968629` | IDENTICAL | retail is SetSize(0) CONSTANT-FOLDED, one argument (0x140025b3a); the old "Ghidra dropped EDX" note was wrong. Holder: PathGraph's raw triple (CitizenBehavior.h); it is that Array's `Clear()` once typed |
| the three element-loop serializers (`CropFieldComponent_SerializeEntityList` 0x1400336c0, `BucketTree_Serialize_21ba0` 0x140021ba0, `Reimpl_Array_Serialize_Stride20` 0x14001bbf0) | ONE template in retail, but NOT `Array<T>::Serialize`: `(RCX serializer, EDX count, R8 data, R9 name)`, the DLL scalar exports' shape `Serializer::Serialize(int n, T* d, const char*)`; 0x140021ba0 is n = 5 const-propagated; MenuScene_Slot5 calls two more instantiations (0x14001bcc0 / 0x14001bff0, which home R9). The Array header is never handed in, count + SetSize stay open-coded at callers: **hazard 6 stands**. STAY FREE, re-tagged | `46d968629` | IDENTICAL | owed: a member template `IO::Serializer::Serialize<T>(int, T*, const char*)` in src/engine/io/Serializer.h (not RT's file) with these as instantiations |

New row: **1220** (`62299c80a`) -- Audio::Scene::Update's two `if (table)` tests around the 4096-slot
walks; retail reads the table untested (0x1800df730, 0x1800dfc7a). Body left.

For the coordinator before merging:
- Behaviour commits for the resave / route gate: `c2edd2e5d` (row 1163: water contour range-array
  capacity growth, world build), `d1f2280b4` (row 1164, no change while the type is registered),
  `937e3529c` (a null test no caller can reach).
- Line-local caller edits land in other blocks' files: Component/{attendance, cemetery, citizen_onremove,
  disease, happiness, healer, herd, livestock, marketlocation, pasturecomponent_states, population,
  residence, storagelocation, trade, tradecomponent_states, wildanimal, workplace, zone, destroycomponent_states,
  buildcomponent*}.cpp, Action/{action_base_cnr, action_base_hunt (body deleted), action_pf_roots_misc,
  action_pf_avl (comment), pickuprawmaterial, removeanimal}.cpp, Game/{town_storage_seed,
  town_workplace_registry}.cpp, Player/{fencedtool_calcpath, keystone_a_removalcalcpath,
  tool_base_drag}.cpp, src/diag/tool_harness.cpp (two calls and two printf fields). RES and SURV
  will meet these on their merge of main.
- Collection.h gains `Array<T>::Find` / `Remove` (8-byte element, static_assert) and two core declarations.
