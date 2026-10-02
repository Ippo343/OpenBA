// code/Game/Game/serialize_resize_helpers.cpp
//
// M3.5 Path B session 5 — additional Geometry::Serialize resize helpers.
//
// Five small dynamic-array resize helpers used by Geometry::Serialize
// (TradeWork_impl_04d0 @ 0x1400b04d0) and TradeWork_impl_f1a0 (@ 0x1400af1a0).
// Each shares the standard 16-byte array header layout:
//   +0x00: void*  data       (heap buffer or NULL)
//   +0x08: int32  count      (current length, visible to caller)
//   +0x0C: int32  capacity   (allocated slot count)
//
// Helper map:
//   #1 stride 0x02, align 2  — TradeWork_impl_sub_c890       @ 0x14001c890
//                              (uint16 array — heightmap/index sub-arrays)
//   #2 stride 0x10, align 4  — TradeWork_impl_b260_sub_efd0  @ 0x14001efd0
//                              (int4 / Vec4 stride for arrays at +0x1C0/+0x1D0)
//   #3 stride 0x10, align 8  — TradeWork_impl_sub_f0a0       @ 0x14001f0a0
//                              (array-of-arrays — deep-clones inner via helper_ff00)
//   #4 stride 0x10, align 2  — FUN_14001ef10                 @ 0x14001ef10
//                              (mesh-vertex 16-byte struct)
//   #5 stride 0x04, align 4  — FUN_1400021d0                 @ 0x1400021d0
//                              (int array — Geometry +0x220)
//
// All five mirror the same shape as Reimpl_Array_Resize_Stride20 in
// serialize_arrays.cpp; the three mechanical ones go through a generic
// resize routine. The array-of-arrays helper is a thin variant that calls
// `DeepCloneInner_FF00` per outer element to copy the inner Vector2 array.
//
// **TradeWork_helper_ff00 (deep-clone)** is small (~30 instructions) and is
// only called from this file (xref_count=2 — once from sub_f0a0, once from
// inside resize loop). We inline it as a static helper rather than register
// it separately — no exe address surface in code/Game/ references it
// directly.

#include "system/MemoryAllocator.h"   // System::MemoryAllocator::MemAlloc / MemFree
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include "Game/exe_bodies.h"
#include "Action/PathSearch.h"   // RawArray<T>
#include "math/Math.h"
extern "C" { extern thread_local void* g_allocStatsCtx; }   // allocator_lock.cpp

// ============================================================
// Generic dynamic-array resize implementation (shared)
// ============================================================
//
// Mirrors `GenericArrayResize` in serialize_arrays.cpp but exposes only
// stride + alignment so it serves every stride below.
//
// B.4 GAME-b: the header is read as a RawArray (data, size, capacity) instead
// of by offset. LEFT: the C entry points receive each caller's header as void*
// (geometry, name database, plant-area, worldgen, ...), so the one cast below
// is where it is read as a byte-element header.
namespace {

RawArray<uint8_t>& ByteHeader(void* arrayHeader) {
    return *reinterpret_cast<RawArray<uint8_t>*>(arrayHeader);
}

void GenericArrayResize(void* arrayHeader, int newCount,
                        size_t stride, int alignment) {
    if (!arrayHeader) return;
    if (newCount < 0) {
        // Defensive: a negative count usually means the deserialize stream
        // was misaligned earlier and we read garbage. Bail out rather than
        // run the shrink-path which would free wild pointers.
        static bool s_warned = false;
        if (!s_warned) {
            printf("[Array::Resize<S5>] negative newCount=%d for header=%p stride=%zu — "
                   "stream misaligned upstream; leaving array unchanged\n",
                   newCount, arrayHeader, stride);
            fflush(stdout);
            s_warned = true;
        }
        return;
    }
    if (!System::MemoryAllocator::_singleton) {
        static bool s_warned = false;
        if (!s_warned) {
            printf("[Array::Resize<S5>] MemAlloc unavailable — leaving array unchanged "
                   "(header=%p newCount=%d stride=%zu)\n",
                   arrayHeader, newCount, stride);
            fflush(stdout);
            s_warned = true;
        }
        return;
    }

    RawArray<uint8_t>& hdr = ByteHeader(arrayHeader);

    int oldCount = hdr.size;
    if (newCount == oldCount) return;

    uint8_t* oldData = hdr.data;
    hdr.size = newCount;

    if (hdr.capacity < newCount) {
        hdr.capacity = newCount;
        if (newCount == oldCount + 1 && oldCount != 0) {
            hdr.capacity = newCount * 3;
        }
        size_t bytes = static_cast<size_t>(hdr.capacity) * stride;
        g_allocStatsCtx = arrayHeader;                         // ALLOCSTATS owner tag
        uint8_t* newBuf = static_cast<uint8_t*>(System::MemoryAllocator::MemAlloc(bytes, alignment));
        g_allocStatsCtx = nullptr;
        hdr.data = newBuf;
    }

    int copyStart = (oldData == hdr.data) ? oldCount : 0;
    uint8_t* dst = hdr.data;

    for (int slot = copyStart; slot < newCount; ++slot) {
        size_t off = static_cast<size_t>(slot) * stride;
        uint8_t* dstElem = dst + off;
        if (!dstElem) continue;
        if (slot < oldCount) {
            const uint8_t* srcElem = oldData + off;
            for (size_t i = 0; i < stride; ++i) {
                dstElem[i] = srcElem[i];
            }
        } else {
            // Zero EVERY new slot unconditionally (both realloc and in-place
            // grow) — matches the original exe resize functions. The engine
            // allocator returns DIRTY memory; on an in-place grow the new
            // slots retain stale bytes (potentially a freed-block pointer),
            // which a later serialize can treat as live and free → free-list
            // corruption. See the matching note in serialize_arrays.cpp
            // GenericArrayResize.
            for (size_t i = 0; i < stride; ++i) {
                dstElem[i] = 0;
            }
        }
    }

    if (oldData != hdr.data && oldData != nullptr) {
        System::MemoryAllocator::MemFree(oldData);
    }
}

} // anonymous namespace

// ============================================================
// Helper #1 — stride 0x02 uint16 array (TradeWork_impl_sub_c890)
// ============================================================
//
// MemAlloc(cap*2, 2). Per element: 2-byte copy. No special teardown.
extern "C" void Reimpl_Array_Resize_Stride02(void* arrayHeader, int newCount) {
    GenericArrayResize(arrayHeader, newCount, 0x02, 2);
}

// ============================================================
// Helper #2 — stride 0x10 int4 array (TradeWork_impl_b260_sub_efd0)
// ============================================================
//
// MemAlloc(cap<<4, 4). Per element: copy 4 dwords (16 bytes total).
extern "C" void Reimpl_Array_Resize_Stride16A(void* arrayHeader, int newCount) {
    GenericArrayResize(arrayHeader, newCount, 0x10, 4);
}

// ============================================================
// Helper #4 — stride 0x10 mesh-vertex array (FUN_14001ef10)
// ============================================================
//
// MemAlloc(cap<<4, 2). Per element: copy 2 qwords (16 bytes total).
// Same byte size as Helper #2 but different alignment (2 vs 4) — kept
// separate because the original exe also keeps these distinct functions.
extern "C" void Reimpl_Array_Resize_Stride16B(void* arrayHeader, int newCount) {
    GenericArrayResize(arrayHeader, newCount, 0x10, 2);
}

// ============================================================
// Helper #5 — stride 0x04 int array (FUN_1400021d0)
// ============================================================
//
// MemAlloc(cap<<2, 4). Per element: copy 1 dword. Used for Geometry +0x220
// (version 4+ array, count at +0x228).
extern "C" void Reimpl_Array_Resize_Stride04(void* arrayHeader, int newCount) {
    GenericArrayResize(arrayHeader, newCount, 0x04, 4);
}

// ============================================================
// Helper #6 — stride 0x0C 3-int array (FUN_14001c7c0)
// ============================================================
//
// MemAlloc(cap*0xc, 4). Per element: copy 3 dwords (12 bytes). Used by
// NameDatabase::Serialize for its +0x20 syllable-table array.
extern "C" void Reimpl_Array_Resize_Stride0C(void* arrayHeader, int newCount) {
    GenericArrayResize(arrayHeader, newCount, 0x0C, 4);
}

// ============================================================
// Helper #7 — stride 0x01 byte buffer (FUN_14000a590)
// ============================================================
//
// MemAlloc(cap, 1). Per element: copy 1 byte. Used by NameDatabase::Serialize
// for its +0x40 packed-character buffer (and by PlantArea work in-game).
extern "C" void Reimpl_Array_Resize_Byte(void* arrayHeader, int newCount) {
    GenericArrayResize(arrayHeader, newCount, 0x01, 1);
}

// ============================================================
// Inlined: TradeWork_helper_ff00 — deep-clone inner Vector2 array
// ============================================================
//
// Used only by Helper #3 below. Decompiled from FUN_14001ff00:
//   if (param_1 != param_2) {
//       MemFree(*param_1);
//       *param_1 = 0; param_1[1] = 0;  (count = 0)
//       int n = (int)param_2[1];
//       *(int*)((char*)param_1 + 8) = n;
//       if (n > 0) {
//           *(int*)((char*)param_1 + 0xc) = n;
//           *param_1 = MemAlloc(n * 8, 4);
//       }
//       for (int i = 0; i < (int)param_1[1]; ++i)
//           ((qword*)*param_1)[i] = ((qword*)*param_2)[i];
//   }
//
// Each outer element is a RawArray of Vector2 (8-byte elements); MemAlloc
// alignment is 4.
namespace {

using Outline = RawArray<Math::Vector2>;

void DeepCloneInner_FF00(Outline& dst, const Outline& src) {
    if (&dst == &src) return;
    if (!System::MemoryAllocator::_singleton) return;

    // Free the dst's existing buffer, zero the data and the count.
    if (dst.data) System::MemoryAllocator::MemFree(dst.data);
    dst.data = nullptr;
    dst.size = 0;
    // capacity is left in the existing state then overwritten below if n>0

    int n = src.size;
    dst.size = n;
    if (n > 0) {
        dst.capacity = n;
        dst.data = static_cast<Math::Vector2*>(
            System::MemoryAllocator::MemAlloc(static_cast<size_t>(n) * 8, 4));
    }

    if (dst.data && src.data) {
        for (int i = 0; i < dst.size; ++i) {
            dst.data[i] = src.data[i];
        }
    }
}

// Retail's free-and-zero of one outer element: MemFree(data); data = 0; one
// qword over size + capacity = 0 (H7: two named stores).
void FreeOutline(Outline& o) {
    if (o.data) System::MemoryAllocator::MemFree(o.data);
    o.data = nullptr;
    o.size = 0;
    o.capacity = 0;
}

} // anonymous namespace

// ============================================================
// Helper #3 — stride 0x10 array-of-arrays (TradeWork_impl_sub_f0a0)
// ============================================================
//
// MemAlloc(cap<<4, 8). Each outer element is a 16-byte array-header.
// On grow:
//   - existing-but-still-fits slots [0..oldCount): no-op (live in same buffer)
//   - new slots in same buffer [oldCount..newCount): zero header, then
//     deep-clone the source slot via DeepCloneInner_FF00 (effectively reading
//     from the same buffer's old position — this is intentional in the
//     original; the source pointer plVar9 walks alongside the dst loop)
//   - new buffer (realloc): all slots [0..newCount) get zeroed then deep-cloned
//     from the *old* buffer's corresponding source slot.
// On shrink:
//   - same buffer: free each freed slot's inner buffer, zero header
//   - reallocated buffer: free all old inner buffers + free the old outer
//     buffer (this branch in the decompile loops over ALL old outer elems)
//
// The decompile structure is unusual because the outer "shrink" path walks
// the OLD buffer, not the new one — we follow that exact pattern.
//
// LEFT: worldgen.cpp (GAME-c) passes the header as void*.
extern "C" void Reimpl_Array_Resize_ArrayOfArrays(void* arrayHeader, int newCount) {
    if (!arrayHeader) return;
    if (newCount < 0) {
        // Defensive: see GenericArrayResize note. The shrink-path here would
        // free wild pointers if oldCount > newCount and newCount is negative.
        static bool s_warned = false;
        if (!s_warned) {
            printf("[Array::Resize<AoA>] negative newCount=%d for header=%p — "
                   "stream misaligned upstream; leaving array unchanged\n",
                   newCount, arrayHeader);
            fflush(stdout);
            s_warned = true;
        }
        return;
    }
    if (!System::MemoryAllocator::_singleton) {
        static bool s_warned = false;
        if (!s_warned) {
            printf("[Array::Resize_ArrOfArr] MemAlloc unavailable — leaving array "
                   "unchanged (header=%p newCount=%d)\n", arrayHeader, newCount);
            fflush(stdout);
            s_warned = true;
        }
        return;
    }

    RawArray<Outline>& hdr = *reinterpret_cast<RawArray<Outline>*>(arrayHeader);

    int oldCount = hdr.size;
    if (newCount == oldCount) return;

    Outline* oldData = hdr.data;
    hdr.size = newCount;

    if (hdr.capacity < newCount) {
        hdr.capacity = newCount;
        if (newCount == oldCount + 1 && oldCount != 0) {
            hdr.capacity = newCount * 3;
        }
        size_t bytes = static_cast<size_t>(hdr.capacity) * 0x10;
        hdr.data = static_cast<Outline*>(System::MemoryAllocator::MemAlloc(bytes, 8));
    }

    // Per-element fill loop. Walks (newCount - copyStart) elements.
    int copyStart = (oldData == hdr.data) ? oldCount : 0;
    Outline* dstBase = hdr.data;

    if (copyStart < newCount) {
        // The original walks `plVar9 = puVar6 + lVar7 * 2` (i.e. old-buffer
        // starting at index copyStart). When the buffer didn't realloc,
        // copyStart = oldCount, so plVar9 walks past the end of valid data —
        // safe because the < oldCount branch is what triggers the deep-clone.
        for (int i = copyStart; i < newCount; ++i) {
            Outline* dstElem = dstBase + i;
            const Outline* srcElem = oldData + i;
            // zero the destination header first (two qwords)
            if (dstElem) {
                dstElem->data = nullptr;
                dstElem->size = 0;
                dstElem->capacity = 0;
                if (i < oldCount && srcElem) {
                    DeepCloneInner_FF00(*dstElem, *srcElem);
                }
            }
        }
    }

    if (oldData == hdr.data) {
        // Same buffer: shrink path — free each freed slot's inner array.
        if (newCount < oldCount) {
            for (int i = newCount; i < oldCount; ++i) FreeOutline(dstBase[i]);
        }
    } else {
        // Reallocated: free all OLD outer slots' inner buffers, then free
        // the old outer buffer itself.
        if (oldData && oldCount > 0) {
            for (int i = 0; i < oldCount; ++i) FreeOutline(oldData[i]);
        }
        if (oldData) System::MemoryAllocator::MemFree(oldData);
    }
}
