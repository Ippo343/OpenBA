# B9 block RT2 -- hand-in

Branch `b9-RT2`, base `fb7cfc339`, main merged at the end (`41427ae97`, SURV in; the only conflict was the root
`HANDIN.md` both branches added -- this file kept; SURV's lives on main). Static gate only, at the tip: debug /
perf / unit builds exit 0, 0 new warnings (debug, perf), unit 920 / 0, **castmetric 2**.

| item | ruling | commit | B11 (perf) | left / measurement |
|---|---|---|---|---|
| 1. row 880, `rtcoll::ArrayAssignCore` | MOVED: `[[gnu::always_inline]] inline` in `runtime/types/collection/Collection.h` (B7 D4 shape), deleted from array_destroy.cpp; same reads and stores (two dead locals not carried) | `21baddeb4` | see below | **the free is GONE.** Left: retail CALLs the assign out of line (0x1800dfd10 -> 0x180050d30); the tip inlines it |
| 2a. first reading (superseded) | re-tag + row 1230, before the coordinator authorized the element headers | `9182fa48c` | IDENTICAL 16484 (comments) | superseded by 2b |
| 2b. the three element-loop serializers | MOVED: ONE member template `IO::Serializer::Serialize<T>(int n, T* d, const char*)` = `for (i < n) d[i].Serialize(*this)`, `[[gnu::noinline]]`; element steps `CropSlot::Serialize`, `MenuSceneMesh::Serialize`, one-argument `InternalPtr<T>::Serialize` (T's own cell), `Collection::Array<T>::Serialize`; the three free functions + declarations deleted; callers by name | `b4914657c` | see below | row 1230 (rewritten): MenuScene's two ExternalPtr arrays (bare `T*` holders), the bucket instantiation's inner-loop CALL, EntityList::Serialize's open-coded +0x08 array |

## Item 1 B11, Scene::Update verbatim

Base = fb7cfc339's perf objects built in this worktree before the edit.

```
## runtime__src__audio__audio_scene_update.o: DIFF 1 IDENTICAL 11 ONLY-BASE 3
  DIFF Audio::Scene::Update
      calls: ... rtcoll::ArrayAssignCore | System::MemoryAllocator::MemFree | ... | ClearCallback | _Unwind_Resume
         ==> ... System::MemoryAllocator::MemFree | ...   (ArrayAssignCore, ClearCallback, _Unwind_Resume gone)
```
Base, after the assign:
```
50f: call rtcoll::ArrayAssignCore
514: mov 0x70(%rsp),%rcx
519: test %rcx,%rcx
51c: je 523
51e: call System::MemoryAllocator::MemFree
```
Tip: no stack local, no test, no free for the empty source. The one MemFree left is the DESTINATION's clear
(slot->positions):
```
49e: mov 0x30(%rsi),%rcx
4d0: test %rcx,%rcx
4d3: je 4fa
4e5: call System::MemoryAllocator::MemFree
4fa: movq $0x0,0x30(%rsi)
502: movq $0x0,0x38(%rsi)
```
Retail's two dead zero stores to its stack source (0x1800dfd19 / 0x1800dfd1d) are not emitted either (stack-only).
Every other DIFF is the core's call becoming its body or the inliner ripple from it: TOTAL DIFF 34 IDENTICAL
16390 ONLY-BASE 28 SAME 60 (e.g. goal.o `Achievement::GoalList::operator=` calls `rtcoll::ArrayAssignCore` ==> none).

## Item 2b B11 (vs the item-1 objects; b11pair.py, map old -> `IO::Serializer::Serialize<T>`)

- **IsDerivedFrom cells.** Each instantiation's cell is T::TypeId(), so no call spells one explicitly.
  `InternalPtr<ComponentEntity>` uses 0x1401c5d70 (0x140033724, 0x140021c24).
  `InternalPtr<NaturalResourceComponent>` uses 0x1401c7b30 (0x14003374d).
- **`Serialize<CropSlot>`: RENAMED-SAME** to the old `.part.0`. It has the same 71 instructions, with one
  `mov %r8,%rbx` scheduled two lines later.
- **`CropFieldComponent::Persist`: DIFF, toward retail.** The caller is now `slots.Serialize(serializer)`; retail
  0x14011aef7..0x14011af36 is that inlined Array step. The out-of-line `IO::Serializer::GetDirection` call is gone,
  replaced by the inline stream load retail has at 0x14011af11 / 0x14011af16. The base's caller-side `count > 0`
  test, left over from partial inlining, is gone, and the CALL is unconditional as in retail.
- **`MenuScene::Serialize`: SAME** (the callee is renamed). `Serialize<MenuSceneMesh>` against the old
  `Reimpl_Array_Serialize_Stride20` (cross-object, objdump-normalised) makes the same calls. Only the loop-end
  arithmetic differs, because the old count was `uint32_t` and the new one is `int`. Retail's count is signed (JLE).
- **`EntityList::Serialize`: SAME** (the callee is renamed; n = 5 is passed). `Serialize<UpdateBucket>` against the
  old `BucketTree_Serialize_21ba0` runs the same count / `Serialize(int&)` / inline direction / SetSizeCore
  sequence, but **the inner loop is a CALL** to `Serialize<InternalPtr<ComponentEntity>>`. That callee is a
  36-instruction `.isra` clone, the old inline loop verbatim. Retail inlines that loop (0x140021c05..0x140021c3e)
  and const-propagates n = 5. noinline applies per template, not per call site: without it, GCC open-coded all
  three caller CALLs, which is worse (three mismatches against one).
- **`Set<ComponentEntity*>::Clear` (entitylist.o): DIFF, an inlining-order ripple.** It has the same 166
  instructions and the same 27 calls, with a different subtree unrolled.
- **61 TUs: ONLY-TIP 7.** These are the inline BinaryReader / BinaryWriter members of Serializer.h, now reached
  through Collection.h's `#include "../io/Stream.h"` (needed for the inline direction read). They are COMDAT copies
  folded at link, and 815 of 935 base objects already carry them.
- **Everything else is IDENTICAL.**

## For the coordinator
- **Item 1 changes code generation in every Assign caller.** It is behaviour-identical by construction, and the
  route gate on merge is the check.
- **noinline, not noipa.** The template is `[[gnu::noinline]]`, which still allows the `.isra` clones that drop
  the unread name argument. Retail's call sites likewise leave R9 unset (0x14011af36, 0x1400c45b9). SURV used
  `[[gnu::noipa]]` on its templates, so say if you want one spelling.
- **Still open on another block's file.** `Entity/entitylist.cpp`'s +0x08 master-array block can become
  `masterArray.Serialize(serializer)` (row 1230 c). That is a caller edit in another block's file.
- **Stale comments.** Comments elsewhere still say "rtcoll::ArrayAssignCore (array_destroy.cpp)". Also,
  entitylist.cpp's `#include "Game/exe_bodies.h"` comment names the deleted BucketTree function. All of these are
  outside this block's files and were left.
