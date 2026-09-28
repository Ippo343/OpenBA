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
// NOTE: This file is the PUBLIC-INTERFACE-ONLY reimpl. The dispatch logic
// (delegate to original-via-fn-ptr if resolved, else execute the cleaned C
// path inline) follows the same pattern as serialize_arrays.cpp /
// serialize_externals.cpp. The cleaned-C path mirrors the original
// instruction-for-instruction so a future A/B trace can verify equivalence.

#include "system/MemoryAllocator.h"   // System::MemoryAllocator::MemAlloc / MemFree
#include "resolve.h"
#include <cstdint>
#include <cstdio>
#include <cstring>
#include "Game/exe_bodies.h"

namespace {

constexpr size_t kStruct60B_Size = 0x60;

// Layout of the 16-byte array header used by every Struct60B-managed buffer.
struct ArrayHeader16 {
    void*   data;
    int32_t count;
    int32_t capacity;
};

// Sub-array deep copy (stride-0x14 / stride-0x02 variants). Used by the copy
// ctor for the embedded sub-arrays at +0x08 and +0x18.
//
// Mirrors FUN_14001fdb0 / FUN_14001fe60 — both have identical structure,
// only the stride and per-element copy size differ.
void SubArrayDeepCopy(void* dstHdrPtr, const void* srcHdrPtr,
                      size_t stride, size_t copySize, int alignment) {
    if (!dstHdrPtr || !srcHdrPtr || dstHdrPtr == srcHdrPtr) return;
    if (!System::MemoryAllocator::_singleton) {
        static bool s_warned = false;
        if (!s_warned) {
            printf("[Struct60B::SubArrayDeepCopy] MemAlloc/Free unavailable — "
                   "leaving dst untouched (stride=%zu)\n", stride);
            fflush(stdout);
            s_warned = true;
        }
        return;
    }

    auto* dst = reinterpret_cast<ArrayHeader16*>(dstHdrPtr);
    const auto* src = reinterpret_cast<const ArrayHeader16*>(srcHdrPtr);

    // Free any existing buffer in dst.
    if (dst->data) {
        System::MemoryAllocator::MemFree(dst->data);
    }
    dst->data = nullptr;
    // Retail's `param_1[1] = 0` is ONE qword store over offsets 0x08..0x0f, so it
    // clears count AND capacity together; only the count was cleared here, which
    // left a stale capacity behind whenever the source array was empty.
    dst->count = 0;
    dst->capacity = 0;

    int srcCount = src->count;
    dst->count = srcCount;
    if (srcCount > 0) {
        dst->capacity = srcCount;
        size_t bytes = static_cast<size_t>(srcCount) * stride;
        dst->data = System::MemoryAllocator::MemAlloc(bytes, alignment);
    }

    if (dst->count <= 0 || !dst->data || !src->data) return;

    // Deep-copy element bytes. Original copies `copySize` bytes per element
    // (which may be < stride if the element has trailing padding the original
    // doesn't bother to copy — for stride 0x14 the original copies 0x14, for
    // stride 0x02 it copies 0x02, so they coincide here).
    auto* dstBytes = reinterpret_cast<uint8_t*>(dst->data);
    const auto* srcBytes = reinterpret_cast<const uint8_t*>(src->data);
    for (int i = 0; i < dst->count; ++i) {
        size_t off = static_cast<size_t>(i) * stride;
        std::memcpy(dstBytes + off, srcBytes + off, copySize);
    }
}

// ============================================================
// Helper #2 — Copy-ctor for one 0x60B element (FUN_14001f5e0)
// ============================================================
//
// The decompile shows a fixed sequence:
//   +0x00 qword raw-copy
//   +0x08 sub-array deep-copy via fdb0 (after zeroing dst's header first)
//   +0x18 sub-array deep-copy via fe60 (after zeroing dst's header first)
//   +0x28..+0x5d field-by-field raw copy (6 dwords + 1 byte + 1 dword + 1 byte
//                                          + 5 dwords + 2 bytes)
//
// Returns dst.
void* Struct60B_CopyCtor_Body(void* dst, const void* src) {
    if (!dst || !src) return dst;

    auto* d = reinterpret_cast<uint8_t*>(dst);
    const auto* s = reinterpret_cast<const uint8_t*>(src);

    // +0x00 : qword raw-copy
    *reinterpret_cast<uint64_t*>(d + 0x00) = *reinterpret_cast<const uint64_t*>(s + 0x00);

    // +0x08 : sub-array (stride 0x14, alignment 2). Zero the dst header
    // first so SubArrayDeepCopy sees an empty destination (the original
    // doesn't free dst's prior buffer here — the caller passes us
    // freshly-zeroed memory from the Resize allocation path).
    *reinterpret_cast<uint64_t*>(d + 0x08) = 0;
    *reinterpret_cast<uint64_t*>(d + 0x10) = 0;
    Reimpl_Struct60B_SubArrayCopyStride14(d + 0x08, s + 0x08);

    // +0x18 : sub-array (stride 0x02, alignment 2).
    *reinterpret_cast<uint64_t*>(d + 0x18) = 0;
    *reinterpret_cast<uint64_t*>(d + 0x20) = 0;
    Reimpl_Struct60B_SubArrayCopyStride02(d + 0x18, s + 0x18);

    // +0x28..+0x5d : raw byte copy of the trailing scalar fields.
    // The decompile lists each field individually but the net effect is a
    // straight 0x36-byte memcpy. We mirror the original's per-field cadence
    // for clarity (matches FUN_14001f5e0 ordering exactly).
    *reinterpret_cast<uint32_t*>(d + 0x28) = *reinterpret_cast<const uint32_t*>(s + 0x28);
    *reinterpret_cast<uint32_t*>(d + 0x2c) = *reinterpret_cast<const uint32_t*>(s + 0x2c);
    *reinterpret_cast<uint32_t*>(d + 0x30) = *reinterpret_cast<const uint32_t*>(s + 0x30);
    *reinterpret_cast<uint32_t*>(d + 0x34) = *reinterpret_cast<const uint32_t*>(s + 0x34);
    *reinterpret_cast<uint32_t*>(d + 0x38) = *reinterpret_cast<const uint32_t*>(s + 0x38);
    *reinterpret_cast<uint8_t* >(d + 0x3c) = *reinterpret_cast<const uint8_t* >(s + 0x3c);
    *reinterpret_cast<uint32_t*>(d + 0x40) = *reinterpret_cast<const uint32_t*>(s + 0x40);
    *reinterpret_cast<uint8_t* >(d + 0x44) = *reinterpret_cast<const uint8_t* >(s + 0x44);
    *reinterpret_cast<uint32_t*>(d + 0x48) = *reinterpret_cast<const uint32_t*>(s + 0x48);
    *reinterpret_cast<uint32_t*>(d + 0x4c) = *reinterpret_cast<const uint32_t*>(s + 0x4c);
    *reinterpret_cast<uint32_t*>(d + 0x50) = *reinterpret_cast<const uint32_t*>(s + 0x50);
    *reinterpret_cast<uint32_t*>(d + 0x54) = *reinterpret_cast<const uint32_t*>(s + 0x54);
    *reinterpret_cast<uint32_t*>(d + 0x58) = *reinterpret_cast<const uint32_t*>(s + 0x58);
    *reinterpret_cast<uint8_t* >(d + 0x5c) = *reinterpret_cast<const uint8_t* >(s + 0x5c);
    *reinterpret_cast<uint8_t* >(d + 0x5d) = *reinterpret_cast<const uint8_t* >(s + 0x5d);

    return dst;
}

// ============================================================
// Helper #3 — Per-element destructor loop (FUN_14001fb80)
// ============================================================
//
// Walks `count` elements starting at `first`, releasing each element's two
// embedded sub-array buffers (the +0x08 and +0x18 owned heap pointers) and
// zeroing the headers. Does NOT free the parent array buffer — the caller
// (Resize) handles that.
//
// Original derefs param_2 + 0x24 as the iterator base; relative offsets
// of -0xc / -0x4 / -0x1c / -0x14 yield element offsets +0x18 / +0x20 /
// +0x08 / +0x10 — i.e., this releases the +0x08 (data) and +0x18 (data)
// pointers and zeros both 16-byte headers. Note the order: +0x18 is freed
// FIRST, then +0x08 (matches the disassembly's `MemFree(lVar1-0xc)` then
// `MemFree(lVar1-0x1c)` cadence).
void Struct60B_DtorRange_Body(void* /*unused*/, void* first, int count) {
    if (count <= 0 || !first) return;

    auto* p = reinterpret_cast<uint8_t*>(first);
    for (int i = 0; i < count; ++i) {
        uint8_t* elem = p + static_cast<size_t>(i) * kStruct60B_Size;

        // Release embedded sub-array at +0x18 (stride-2 array).
        void** subData18 = reinterpret_cast<void**>(elem + 0x18);
        if (*subData18) System::MemoryAllocator::MemFree(*subData18);
        *reinterpret_cast<uint64_t*>(elem + 0x18) = 0;
        *reinterpret_cast<uint64_t*>(elem + 0x20) = 0;

        // Release embedded sub-array at +0x08 (stride-0x14 array).
        void** subData08 = reinterpret_cast<void**>(elem + 0x08);
        if (*subData08) System::MemoryAllocator::MemFree(*subData08);
        *reinterpret_cast<uint64_t*>(elem + 0x08) = 0;
        *reinterpret_cast<uint64_t*>(elem + 0x10) = 0;
    }
}

// ============================================================
// Helper #1 — Resize the 0x60B struct array (FUN_14001f250)
// ============================================================
//
// Algorithm (mirroring the decompile line-for-line):
//
//   oldCount = hdr->count
//   if (newCount == oldCount) return
//
//   oldData = hdr->data
//   hdr->count = newCount
//
//   if (hdr->capacity < newCount) {
//       hdr->capacity = newCount
//       if (newCount == oldCount + 1 && oldCount != 0)
//           hdr->capacity = newCount * 3      // amortised growth
//       hdr->data = MemAlloc(capacity * 0x60, alignment=8)
//   }
//
//   // copyStart = oldCount if data didn't realloc, else 0
//   copyStart = (oldData == hdr->data) ? oldCount : 0
//
//   for (slot = copyStart; slot < newCount; ++slot) {
//       elem = hdr->data + slot * 0x60
//       if (slot < oldCount) {
//           // copy old element into new slot (deep copy for sub-arrays)
//           CopyCtor(elem, oldData + slot * 0x60)
//       } else {
//           // zero-init: most fields are 0, but +0x28 (param 5) is -1
//           // and the count fields default to 0
//           memset(elem, 0, 0x60); elem[+0x28] = 0xFFFFFFFF
//       }
//   }
//
//   if (oldData == hdr->data) {
//       // shrink in place — destruct the trailing elements
//       if (newCount < oldCount)
//           DtorRange(_, oldData + newCount*0x60, oldCount - newCount)
//   } else {
//       // realloc happened — destruct the entire old buffer + free it
//       DtorRange(_, oldData, oldCount)
//       MemFree(oldData)
//   }
void Struct60B_Resize_Body(void* arrayHeaderPtr, unsigned int newCount) {
    if (!arrayHeaderPtr) return;
    // Defensive: when the deserialize stream is misaligned, we may receive a
    // garbage count (typically a negative int reinterpreted as a huge unsigned).
    // Without this, oldCount - newCount underflows to ~65k and DtorRange iterates
    // freeing wild pointers.
    if (static_cast<int32_t>(newCount) < 0) {
        static bool s_warned = false;
        if (!s_warned) {
            printf("[Struct60B::Resize] negative-as-unsigned newCount=%u (signed=%d) "
                   "for header=%p — stream misaligned upstream; leaving array unchanged\n",
                   newCount, static_cast<int32_t>(newCount), arrayHeaderPtr);
            fflush(stdout);
            s_warned = true;
        }
        return;
    }
    if (!System::MemoryAllocator::_singleton) {
        static bool s_warned = false;
        if (!s_warned) {
            printf("[Struct60B::Resize] MemAlloc unavailable — leaving array unchanged "
                   "(header=%p newCount=%u)\n", arrayHeaderPtr, newCount);
            fflush(stdout);
            s_warned = true;
        }
        return;
    }

    auto* hdr = reinterpret_cast<ArrayHeader16*>(arrayHeaderPtr);

    unsigned int oldCount = static_cast<unsigned int>(hdr->count);
    if (newCount == oldCount) return;

    void* oldData = hdr->data;
    hdr->count = static_cast<int32_t>(newCount);

    if (hdr->capacity < static_cast<int32_t>(newCount)) {
        hdr->capacity = static_cast<int32_t>(newCount);
        if (newCount == oldCount + 1 && oldCount != 0) {
            hdr->capacity = static_cast<int32_t>(newCount) * 3;
        }
        size_t bytes = static_cast<size_t>(hdr->capacity) * kStruct60B_Size;
        hdr->data = System::MemoryAllocator::MemAlloc(bytes, /*align*/ 8);
    }

    // copyStart = oldCount if buffer NOT reallocated, else 0
    unsigned int copyStart = (oldData == hdr->data) ? oldCount : 0u;

    if (static_cast<int>(copyStart) < static_cast<int>(newCount)) {
        for (unsigned int slot = copyStart; slot < newCount; ++slot) {
            size_t off = static_cast<size_t>(slot) * kStruct60B_Size;
            uint8_t* elem = reinterpret_cast<uint8_t*>(hdr->data) + off;
            if (!elem) continue;

            if (static_cast<int>(slot) < static_cast<int>(oldCount)) {
                // Copy from old buffer (sub-array deep copy needed).
                const uint8_t* srcElem =
                    reinterpret_cast<const uint8_t*>(oldData) + off;
                Reimpl_Struct60B_CopyCtor(elem, srcElem);
            } else {
                // Zero-init new slot. Original sets +0x28 to 0xFFFFFFFF
                // and zeros all other fields.
                std::memset(elem, 0, kStruct60B_Size);
                *reinterpret_cast<uint32_t*>(elem + 0x28) = 0xFFFFFFFFu;
            }
        }
    }

    if (oldData == hdr->data) {
        // Shrink-in-place — dtor trailing elements only.
        if (static_cast<int>(newCount) < static_cast<int>(oldCount)) {
            uint8_t* tail =
                reinterpret_cast<uint8_t*>(oldData) +
                static_cast<size_t>(newCount) * kStruct60B_Size;
            Reimpl_Struct60B_DtorRange(nullptr, tail,
                                       static_cast<int>(oldCount - newCount));
        }
    } else {
        // Realloc happened — dtor the entire old buffer + free it.
        if (oldData) {
            Reimpl_Struct60B_DtorRange(nullptr, oldData,
                                       static_cast<int>(oldCount));
            System::MemoryAllocator::MemFree(oldData);
        }
    }
}

} // namespace

// ============================================================
// Public reimpl entry points
// ============================================================
//
// Each entry tries the original exe address first (usable when the exe code
// page is mapped). If that pointer is null (no exe code), we fall through to
// our cleaned C body. The cleaned body mirrors the original
// instruction-for-instruction so a future A/B trace can verify equivalence.

extern "C" void Reimpl_Struct60B_Resize(void* arrayHeader, unsigned int newCount) {
    Struct60B_Resize_Body(arrayHeader, newCount);
}

extern "C" void* Reimpl_Struct60B_CopyCtor(void* dst, const void* src) {
    return Struct60B_CopyCtor_Body(dst, src);
}

extern "C" void Reimpl_Struct60B_DtorRange(void* unused, void* first, int count) {
    Struct60B_DtorRange_Body(unused, first, count);
}

// ============================================================
// The two sub-array deep-copy leaves, exported by address (lane D close-out).
// ============================================================
// Retail 0x14001fdb0 (stride 0x14) and 0x14001fe60 (stride 0x02) are the two
// helpers Struct60B's copy ctor calls; both were unregistered, so
// fn_Struct60B_SubArrayCopyStride14/02 (engine_funcs.cpp:4947,4949) were null
// forever and only the inline fallback above ever ran. Registering them makes
// those two pointers truthful without changing which code executes — both
// entry points are the same body. Retail returns dst in RAX.
extern "C" void* Reimpl_Struct60B_SubArrayCopyStride14(void* dst, const void* src) {
    SubArrayDeepCopy(dst, src, /*stride*/ 0x14, /*copy*/ 0x14, /*align*/ 2);
    return dst;
}

extern "C" void* Reimpl_Struct60B_SubArrayCopyStride02(void* dst, const void* src) {
    SubArrayDeepCopy(dst, src, /*stride*/ 0x02, /*copy*/ 0x02, /*align*/ 2);
    return dst;
}
