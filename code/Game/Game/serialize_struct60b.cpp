// code/Game/Game/serialize_struct60b.cpp
//
// M3.5 Path B session 2 — lifecycle helpers for the 0x60-byte struct array
// at MenuScene::geometry +0x200 (count +0x208). Used by Geometry::Serialize
// (FUN_1400b04d0) and the version-gated extra FUN_1400af1a0 to grow/shrink
// the array of trade-related (likely "merchant order"/"trade slot") structs.
//
// Helper map:
//   sub_f250  — 0x14001f250  (resize)        — Reimpl_Struct60B_Resize
//   sub_f5e0  — 0x14001f5e0  (copy ctor)     — Reimpl_Struct60B_CopyCtor
//   sub_fb80  — 0x14001fb80  (dtor range)    — Reimpl_Struct60B_DtorRange
//
// 0x60-byte element layout (from FUN_14001f5e0 decompile):
//   +0x00 : qword  (8B)              — copied raw (likely typed ExternalPtr,
//                                        FUN_140007140 cascade — but only on
//                                        SERIALIZE, not on copy/dtor)
//   +0x08 : array<stride 0x14>       — { void* data; int32 count; int32 cap }
//                                        owned heap; deep-copy via fdb0,
//                                        free on dtor
//   +0x18 : array<stride 0x02>       — { void* data; int32 count; int32 cap }
//                                        owned heap; deep-copy via fe60,
//                                        free on dtor
//   +0x28..+0x44                     — 6 dwords + 1 byte + 1 dword + 1 byte
//   +0x48..+0x5d                     — 5 floats + 2 bytes
//
// The two embedded sub-arrays are the only fields that own heap memory.
// Everything else is plain bytes.
//
// B.4 GAME-b: the element is GeometryBatch (WorldGeometryElements.h) and the
// bodies are typed functions; the C entry points forward ACT-a's untyped callers.
//
// NOTE: This file is the PUBLIC-INTERFACE-ONLY reimpl. The dispatch logic
// (delegate to original-via-fn-ptr if resolved, else execute the cleaned C
// path inline) follows the same pattern as serialize_arrays.cpp /
// serialize_externals.cpp. The cleaned-C path mirrors the original
// instruction-for-instruction so a future A/B trace can verify equivalence.

#include "system/MemoryAllocator.h"   // System::MemoryAllocator::MemAlloc / MemFree
#include <cstdint>
#include <cstdio>
#include <cstring>
#include "Game/exe_bodies.h"
#include "Game/WorldGeometryElements.h"

namespace {

inline void MemFree(void* p) { System::MemoryAllocator::MemFree(p); }

// Sub-array deep copy — FUN_14001fdb0 (GeometryBatchVertex, stride 0x14) and
// FUN_14001fe60 (uint16 index, stride 0x02): identical bodies but the element.
template <class T>
void CopySubArray(RawArray<T>& dst, const RawArray<T>& src) {
    if (&dst == &src) return;

    if (!System::MemoryAllocator::_singleton) {
        static bool s_warned = false;
        if (!s_warned) {
            printf("[Struct60B::SubArrayDeepCopy] MemAlloc/Free unavailable — "
                   "leaving dst untouched (stride=%zu)\n", sizeof(T));
            fflush(stdout);
            s_warned = true;
        }
        return;
    }

    // Free any existing buffer in dst.
    if (dst.data) MemFree(dst.data);
    dst.data = nullptr;
    // Retail's `param_1[1] = 0` is ONE qword store over offsets 0x08..0x0f, so it
    // clears size AND capacity together (H7: two named stores).
    dst.size = 0;
    dst.capacity = 0;

    int srcCount = src.size;
    dst.size = srcCount;
    if (srcCount > 0) {
        dst.capacity = srcCount;
        dst.data = static_cast<T*>(System::MemoryAllocator::MemAlloc(
            static_cast<size_t>(srcCount) * sizeof(T), /*align*/ 2));
    }
    if (dst.size <= 0 || !dst.data || !src.data) return;

    for (int i = 0; i < dst.size; ++i) dst.data[i] = src.data[i];
}

} // namespace

// ============================================================
// 0x14001f5e0 — GeometryBatch copy constructor.
// ============================================================
// The two sub-array headers are zeroed first (the caller hands us fresh memory
// from the Resize allocation) and deep-copied; the trailing scalars are copied
// field by field in retail's order (the padding is never written).
void GeometryBatch_CopyConstruct(GeometryBatch& d, const GeometryBatch& s) {
    d.material = s.material;                           // +0x00 qword raw copy

    d.vertices.data = nullptr;                         // +0x08 header (two qwords)
    d.vertices.size = 0;
    d.vertices.capacity = 0;
    CopySubArray(d.vertices, s.vertices);              // 0x14001fdb0

    d.indices.data = nullptr;                          // +0x18 header (two qwords)
    d.indices.size = 0;
    d.indices.capacity = 0;
    CopySubArray(d.indices, s.indices);                // 0x14001fe60

    d.slot       = s.slot;                             // +0x28
    d.minX       = s.minX;                             // +0x2c
    d.minY       = s.minY;                             // +0x30
    d.maxX       = s.maxX;                             // +0x34
    d.maxY       = s.maxY;                             // +0x38
    d.field_0x3C = s.field_0x3C;                       // +0x3c byte
    d.field_0x40 = s.field_0x40;                       // +0x40
    d.field_0x44 = s.field_0x44;                       // +0x44 byte
    for (int i = 0; i < 5; ++i)                        // +0x48 .. +0x58
        d.field_0x48[i] = s.field_0x48[i];
    d.field_0x5C   = s.field_0x5C;                     // +0x5c byte
    d.alternateKey = s.alternateKey;                   // +0x5d byte
}

// ============================================================
// 0x14001fb80 — destroy `count` batches: each one's index array (+0x18), then
// its vertex array (+0x08); each header's data freed, then data and the
// size|capacity qword zeroed.
// ============================================================
void GeometryBatch_DestroyRange(GeometryBatch* first, int count) {
    if (count <= 0 || !first) return;
    for (int i = 0; i < count; ++i) {
        GeometryBatch& b = first[i];
        if (b.indices.data) MemFree(b.indices.data);
        b.indices.data = nullptr;
        b.indices.size = 0;
        b.indices.capacity = 0;
        if (b.vertices.data) MemFree(b.vertices.data);
        b.vertices.data = nullptr;
        b.vertices.size = 0;
        b.vertices.capacity = 0;
    }
}

// ============================================================
// 0x14001f250 — resize a GeometryBatch array (Collection::Array<T>::SetSize as
// retail inlines it for this element: exact-equal early return, size stored
// first, the treble rule on a one-element grow of a non-empty array, align 8;
// the prefix is copy-constructed only when the block moved, new slots are zeroed
// with slot = -1, the truncated tail or the whole old block is destroyed).
// ============================================================
void GeometryBatch_Resize(RawArray<GeometryBatch>& hdr, unsigned int newCount) {
    // Defensive: when the deserialize stream is misaligned, we may receive a
    // garbage count (typically a negative int reinterpreted as a huge unsigned).
    // Without this, oldCount - newCount underflows to ~65k and DtorRange iterates
    // freeing wild pointers.
    if (static_cast<int32_t>(newCount) < 0) {
        static bool s_warned = false;
        if (!s_warned) {
            printf("[Struct60B::Resize] negative-as-unsigned newCount=%u (signed=%d) "
                   "for header=%p — stream misaligned upstream; leaving array unchanged\n",
                   newCount, static_cast<int32_t>(newCount), (void*)&hdr);
            fflush(stdout);
            s_warned = true;
        }
        return;
    }

    if (!System::MemoryAllocator::_singleton) {
        static bool s_warned = false;
        if (!s_warned) {
            printf("[Struct60B::Resize] MemAlloc unavailable — leaving array unchanged "
                   "(header=%p newCount=%u)\n", (void*)&hdr, newCount);
            fflush(stdout);
            s_warned = true;
        }
        return;
    }

    unsigned int oldCount = static_cast<unsigned int>(hdr.size);
    if (newCount == oldCount) return;

    GeometryBatch* oldData = hdr.data;
    hdr.size = static_cast<int32_t>(newCount);

    if (hdr.capacity < static_cast<int32_t>(newCount)) {
        hdr.capacity = static_cast<int32_t>(newCount);
        if (newCount == oldCount + 1 && oldCount != 0) {
            hdr.capacity = static_cast<int32_t>(newCount) * 3;
        }
        hdr.data = static_cast<GeometryBatch*>(System::MemoryAllocator::MemAlloc(
            static_cast<size_t>(hdr.capacity) * sizeof(GeometryBatch), /*align*/ 8));
    }

    // copyStart = oldCount if buffer NOT reallocated, else 0
    unsigned int copyStart = (oldData == hdr.data) ? oldCount : 0u;
    if (static_cast<int>(copyStart) < static_cast<int>(newCount)) {
        for (unsigned int slot = copyStart; slot < newCount; ++slot) {
            GeometryBatch* elem = hdr.data + slot;
            if (!elem) continue;
            if (static_cast<int>(slot) < static_cast<int>(oldCount)) {
                GeometryBatch_CopyConstruct(*elem, oldData[slot]);   // 0x14001f5e0
            } else {
                // New slot: zeroed, slot = -1.
                std::memset(elem, 0, sizeof(GeometryBatch));
                elem->slot = -1;
            }
        }
    }

    if (oldData == hdr.data) {
        // Shrink-in-place — destroy the trailing elements only.
        if (static_cast<int>(newCount) < static_cast<int>(oldCount)) {
            GeometryBatch_DestroyRange(oldData + newCount,
                                       static_cast<int>(oldCount - newCount));
        }
    } else {
        // Realloc happened — destroy the entire old block + free it.
        if (oldData) {
            GeometryBatch_DestroyRange(oldData, static_cast<int>(oldCount));
            MemFree(oldData);
        }
    }
}

// ============================================================
// The C entry points (B4 FORWARD): action_pf_pools.cpp / action_pf_road.cpp
// (ACT-a, finished) hold the headers and elements untyped.
// ============================================================
// LEFT: the void* arguments of the ACT-a callers.
extern "C" void Reimpl_Struct60B_Resize(void* arrayHeader, unsigned int newCount) {
    if (!arrayHeader) return;
    GeometryBatch_Resize(*reinterpret_cast<RawArray<GeometryBatch>*>(arrayHeader), newCount);
}

extern "C" void* Reimpl_Struct60B_CopyCtor(void* dst, const void* src) {
    if (!dst || !src) return dst;
    GeometryBatch_CopyConstruct(*reinterpret_cast<GeometryBatch*>(dst),
                                *reinterpret_cast<const GeometryBatch*>(src));
    return dst;
}

extern "C" void Reimpl_Struct60B_DtorRange(void* /*unused*/, void* first, int count) {
    GeometryBatch_DestroyRange(reinterpret_cast<GeometryBatch*>(first), count);
}
