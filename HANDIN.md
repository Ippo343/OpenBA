# B9 block RT2 -- hand-in

Branch `b9-RT2`, base `fb7cfc339`, main merged (already up to date). Static gate only, at the tip:
debug / perf / unit builds exit 0, 0 new warnings (debug, perf), unit 920 / 0, **castmetric 2**.

| item | ruling | commit | B11 (perf) | left / measurement |
|---|---|---|---|---|
| 1. row 880, `rtcoll::ArrayAssignCore` | MOVED: the body is `[[gnu::always_inline]] inline` in `runtime/types/collection/Collection.h` (B7 D4 shape), deleted from array_destroy.cpp. Same reads and stores; its two dead locals (`realloced`, `copied`) are not carried | `21baddeb4` | see below | **the free is GONE.** Left: retail CALLs the assign out of line (0x1800dfd10 -> 0x180050d30); the tip inlines it |
| 2. the three element-loop serializers (0x1400336c0, 0x140021ba0, 0x14001bbf0; plus 0x14001bcc0 / 0x14001bff0) | STAY FREE, re-tagged (B9 RT2); new row **1230** | `9182fa48c` | TOTAL IDENTICAL 16484 (comments only) | ONE retail template `Serializer::Serialize<T>(int n, T* d, const char*)`: `TEST EDX,EDX / JLE`, then `d[i].Serialize(*this)` inlined. R9 is never read (0x14001bcc0 and the DLL's 0x18005d4a0 only home it), and inner calls pass R8 = 0. 0x140021ba0 is n = 5 over UpdateBucket, whose element step is an inlined `Array<T>::Serialize` (count, `if reading` SetSize 0x1400032e0, the template over InternalPtr<ComponentEntity>). Owed element members sit outside this block's files: `CropSlot::Serialize` (CropFieldComponent.h), `MenuSceneMesh::Serialize` (MenuScene.h), a one-argument `InternalPtr<T>::Serialize` (Internal.h), `Collection::Array<T>::Serialize` (Collection.h; hazard 6 is a DLL-row fact). |

## Item 1 B11, Scene::Update verbatim

Base = fb7cfc339's perf objects built in this worktree before the edit.

```
## runtime__src__audio__audio_scene_update.o: DIFF 1 IDENTICAL 11 ONLY-BASE 3
  ONLY-BASE ...Array<Audio::Scene::EmitterInstance>...ClearCallback
  DIFF _ZN5Audio5Scene6UpdateEN6System4TimeES2_RNS1_6RandomERKN4Math7Vector3Ef
      calls: ... rtcoll::ArrayAssignCore | System::MemoryAllocator::MemFree | ... | ClearCallback | _Unwind_Resume
         ==> ... System::MemoryAllocator::MemFree | ...   (ArrayAssignCore, ClearCallback, _Unwind_Resume gone)
```

Base asm after the assign:
```
50f: call rtcoll::ArrayAssignCore
514: mov 0x70(%rsp),%rcx
519: test %rcx,%rcx
51c: je 523
51e: call System::MemoryAllocator::MemFree
```
Tip asm: no stack local, no test and no free for the empty source. The one MemFree left is the destination's
clear (slot->positions, ClearCallback inlined):
```
49e: mov 0x30(%rsi),%rcx
...  (the EffectSlot field stores)
4d0: test %rcx,%rcx
4d3: je 4fa
4e5: call System::MemoryAllocator::MemFree
4fa: movq $0x0,0x30(%rsi)
502: movq $0x0,0x38(%rsi)
```
Retail's two dead zero stores to its stack source (0x1800dfd19 / 0x1800dfd1d) are not emitted either; they are
stack-only and unobservable.

Every other DIFF is the core call becoming its body, or the inliner ripple from it (DIFF TUs: citizenlist_serialize 2,
serialize_struct60b 1, achievement_manager 1, achievement_state 1, goal 2, audio 2, array_destroy 13,
array_graphics_anim 4, introsort 1, engine/loops 1, io/modification 4, modification_refresh 1). TOTAL DIFF 34
IDENTICAL 16390 ONLY-BASE 28 SAME 60. Example: goal.o `Achievement::GoalList::operator=` calls
`rtcoll::ArrayAssignCore` ==> none.

## For the coordinator
- Item 1 changes code generation in every Assign caller (the core is now inline). It is behaviour-identical by
  construction, but the route gate on merge is the check.
- Item 2 needs CropFieldComponent.h, MenuScene.h and runtime/types/object/Internal.h to finish (row 1230).
- Stale comments elsewhere still say "rtcoll::ArrayAssignCore (array_destroy.cpp)": array.cpp,
  array_graphics_anim.cpp, closure_batch2b.cpp (graphics, save), wave9_lane_gfxd.cpp, modification.cpp,
  gameupdateloop_load.cpp, audio.cpp. They are outside this block's files and were left.
