// code/Game/Game/serialize_resize_helpers.cpp
//
// kSpec_ContourOutline -- the row of retail 0x14001f0a0, Collection::Array<T>::SetSize for
// WorldGeometry +0x1E0 (_contourOutlines: an array of Array<Math::Vector2>, stride 0x10, align 8).
// The SetSize body is rtcoll::SetSizeCore (Collection.h); this row is the element's three
// operations, transcribed from 0x14001f0a0 and the inner operator= 0x14001ff00:
//   copy (a slot below the old size, after a reallocation; 0x14001f14d-0x14001f15b): the header
//     zeroed (two qword stores), then 0x14001ff00 from the old slot;
//   init (a new slot; 0x14001f169-0x14001f16c): the header zeroed (two qword stores);
//   destroy (every old slot after a reallocation, 0x14001f1a2; the dropped slots of an in-place
//     shrink, 0x14001f1e2): MemFree of the inner buffer, then data and size|capacity zeroed.
// Callers: WorldGeometry::Serialize (0x1400b093f) and BuildFromHeightfield (0x1400ac594), through
// RawArraySetSize(a, n, &kSpec_ContourOutline) (Action/PathSearch.h). B8 RT2 (row 1161): this
// replaces Reimpl_Array_Resize_ArrayOfArrays and its DeepCloneInner_FF00 / FreeOutline helpers.

#include "system/MemoryAllocator.h"   // System::MemoryAllocator::MemAlloc / MemFree
#include <cstdint>
#include "Action/PathSearch.h"        // RawArray<T>
#include "collection/Collection.h"    // rtcoll::ArraySpec, AssignAs / InitAs / DestroyAs
#include "Game/WorldGeometryElements.h"
#include "math/Math.h"

namespace {

using Outline = RawArray<Math::Vector2>;

// 0x14001ff00 -- Array<Math::Vector2>::operator=. Its only test is the self-assignment one;
// MemFree is called on the old buffer whatever it holds (MemFree tests for null itself).
void OutlineAssign(Outline* dst, const Outline* src) {
    if (dst == src) return;
    System::MemoryAllocator::MemFree(dst->data);
    dst->size = 0;            // retail: one qword store over size | capacity (H7: two named stores)
    dst->capacity = 0;
    dst->data = nullptr;
    dst->size = src->size;
    if (dst->size > 0) {
        dst->capacity = dst->size;
        dst->data = static_cast<Math::Vector2*>(
            System::MemoryAllocator::MemAlloc(static_cast<uint64_t>(dst->size) * 8, 4));
    }
    for (int i = 0; i < dst->size; ++i) {
        Math::Vector2* d = dst->data + i;
        if (d == nullptr) continue;   // retail tests the computed address (0x14001ff69)
        *d = src->data[i];            // one qword
    }
}

void OutlineZero(Outline* o) {
    o->data = nullptr;
    o->size = 0;              // retail: one qword store over size | capacity (H7: two named stores)
    o->capacity = 0;
}

void OutlineCopy(Outline* dst, const Outline* src) {
    OutlineZero(dst);
    OutlineAssign(dst, src);
}

void OutlineFree(Outline* o, int count) {
    for (int i = 0; i < count; ++i) {
        System::MemoryAllocator::MemFree(o[i].data);
        OutlineZero(&o[i]);
    }
}

}  // namespace

const rtcoll::ArraySpec kSpec_ContourOutline = {          // exe 0x14001f0a0
    "Array<Array<Vector2>>", 0x10, 8,
    &rtcoll::AssignAs<Outline, &OutlineCopy>, &rtcoll::InitAs<Outline, &OutlineZero>,
    &rtcoll::DestroyAs<Outline, &OutlineFree>, false,
    /*assign unwritten*/ 0, 0,
    /*init   unwritten*/ 0, 0,
    /*seed zero       */ 0, 0 };
