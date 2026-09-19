// code/Game/Game/geometry_serialize.cpp
//
// M3.5 Path B session 8 — final assembly.
//
// Two top-level reimpls that drive `Geometry::Serialize` end-to-end so that
// the standalone-exe MenuScene 3D milestone can deserialize terrain without
// the original exe code page being mapped:
//
//   `Reimpl_TradeWork_impl_04d0`  — exe 0x1400b04d0 (~700 bytes, 15 steps)
//                                    Geometry::Serialize body (master schema, version 6).
//   `Reimpl_TradeWork_impl_f1a0`  — exe 0x1400af1a0 (~196 bytes, 4 steps)
//                                    Version-gated extra serialize (parallel schema, version 4).
//
// Both walk the spec in `notes/M35_geom_serialize_findings.md`. Each step
// delegates to a previously-implemented session helper:
//
//   session 2 — Reimpl_Struct60B_Resize / _CopyCtor / _DtorRange
//   session 3 — Reimpl_ExternalPtr_Serialize_Typed_7140 (MaterialInstance)
//   session 4 — Reimpl_Struct60B_Serialize, Reimpl_Array_Serialize_External_MaterialInstance
//   session 5 — Reimpl_Array_Resize_Stride02/16A/16B/04/_ArrayOfArrays
//   session 6 — Reimpl_TradeWork_sub_c1e0 (heightmap mesh build)
//   session 7 — Reimpl_TradeWork_sub_c780 (heightmap → texture upload)
//   prior     — Reimpl_Array_Resize_Stride08 (stride 8) from serialize_arrays.cpp
//
// Cascade safety: every Runtime primitive (Serializer ctor/dtor, dispatch slot
// for Serialize_Int/Bool/Float/Byte/SerializeBytes, Vector2::Serialize, IndexBuffer/
// VertexBuffer::Create) is null-guarded with one-shot `printf` warnings; on
// missing primitives we soft-fail per-step rather than crashing.
//
// On first run in standalone we expect cascades on the 4 typed-ExternalPtr
// serialize call sites (MaterialInstance via FUN_140007140 at +0xF8, +0x150,
// +0x158 inside 04d0; FUN_140020150 at +0x210; FUN_14001ffa0's interior call
// for each 0x60B element). Session 3 + session 4 reimpls already log
// one-shot warnings in those branches and return early — so the menu
// shouldn't crash even with partial type registration. Document expected
// failure points inline so the user can iterate.
//
// Dispatch pattern: try the original exe address first (usable when
// 0x1400b04d0 / 0x1400af1a0 is RegisterExeAddr-able). If null, fall
// through to the cleaned C body. Both functions self-resolve each Runtime
// dispatch slot lazily on first use.

// ENDGAME B.3.5 batch K1: the runtime type header comes FIRST. video/Buffers.h reaches
// no Volume.h, but the house rule (a runtime type header ahead of resolve.h's
// <windows.h>, whose <wingdi.h> `Rectangle()` hides Volume.h's global
// `struct Rectangle`) is kept so a later include change cannot break it silently.
#include "video/Buffers.h"   // Video::{Vertex,Index}Buffer
#include "engine_funcs.h"
#include "resolve.h"
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch B
#include <new>               // ENDGAME B.3.4a closing: placement-new of IO::Serializer
#include <cstdint>
#include <cstdio>

// Heap-overflow checkpoint (standalone diagnostic; no-op when
// BANISHED_REDZONE is unset). Localizes which Geometry::Serialize step trips a
// tail-canary so the displaced free-list crash can be root-caused.
#include "redzone_alloc.h"
// Use the free-list LINK validator (not the tail-canary ValidateAll, which is a
// no-op at PAD=0) so a checkpoint catches the step that clobbers a freed block's
// forward link — the menu heap-crash signature. Aborts (0x124) naming the tag.
#define RZ_CHECK(tag) RedzoneAlloc_CheckLinks(tag)

// External reimpl entry points from sibling sessions.
extern "C" void  Reimpl_Struct60B_Resize(void* arrayHeader, unsigned int newCount);
extern "C" void  Reimpl_Struct60B_Serialize(void* serializer, int count, void* arrayBase);
extern "C" void  Reimpl_Array_Serialize_External_MaterialInstance(
    void* serializer, int count, void* arrayBase);
extern "C" void  Reimpl_ExternalPtr_Serialize_Typed_7140(void* refField, void* serializer);
extern "C" void  Reimpl_TradeWork_sub_c1e0(void* self);
extern "C" void  Reimpl_TradeWork_sub_c780(void* self);
extern "C" void  Reimpl_Array_Resize_Stride02(void* arrayHeader, int newCount);
extern "C" void  Reimpl_Array_Resize_Stride16A(void* arrayHeader, int newCount);
extern "C" void  Reimpl_Array_Resize_Stride16B(void* arrayHeader, int newCount);
extern "C" void  Reimpl_Array_Resize_Stride04(void* arrayHeader, int newCount);
extern "C" void  Reimpl_Array_Resize_ArrayOfArrays(void* arrayHeader, int newCount);

namespace {

// ============================================================
// Dispatch-slot typedefs (lazy-resolved per call)
// ============================================================
//
// All slots in the 0x14015xxxx range. We resolve them via ResolveAddr at
// first use and cache. This pattern matches session 4 / session 6 etc.

typedef void  (*FnVector2Ser_t   )(void* self, void* serializer);
// ENDGAME B.3.5 batch K1: the two buffer-Create typedefs went with their caches.

// Slot map (verified via Ghidra MCP `disassemble_function 0x1400b04d0 CALL`):
// ENDGAME B.3.4a closing: every Serializer row is gone with its cache — the
// calls are real IO::Serializer members. The rows below are the non-Serializer
// doors this body still goes through. The PE import slots themselves stay
// registered in standalone/address_registry.cpp (B34_RULES §6.5(g)).
constexpr uintptr_t kSlot_Vector2Ser       = 0x1401526b8;  // Math::Vector2::Serialize
// 0x140152668 (Video::VertexBuffer::Create) and 0x140152660 (Video::IndexBuffer::Create)
// were here until ENDGAME B.3.5 batch K1; both are members now. The PE import slots
// themselves stay registered in standalone/address_registry.cpp (B34_RULES 6.5(g)).

template <typename Fn>
Fn ResolveSlot(uintptr_t exeAddr, Fn& cache, bool& triedOnce, const char* tag) {
    if (cache) return cache;
    if (triedOnce) return nullptr;
    triedOnce = true;
    void** slot = reinterpret_cast<void**>(ResolveAddr(Module::Exe, exeAddr));
    if (slot && *slot) {
        cache = reinterpret_cast<Fn>(*slot);
    } else {
        printf("[Geometry::Serialize] dispatch slot 0x%llx (%s) unresolved — "
               "step will soft-fail\n",
               static_cast<unsigned long long>(exeAddr), tag ? tag : "?");
        fflush(stdout);
    }
    return cache;
}

// Helper: read the inner load/save flag.
//
// IO::Serializer ctor (Runtime 0x1800b9960) stores layout:
//   [0]: Stream* (the wrapped stream)
//   [8]: encoding (returned by stream encoding factory virtual call)
// So `local_50[0]` is the stream pointer. The original decompile reads
// `*(int*)(local_50[0] + 8) == 0` — this is offset 8 of the **stream**, which
// holds the IsLoading flag (0 = loading, 1 = saving). Earlier we mis-read
// vtable+8 because we assumed local_50[0] was a vtable.
inline int SerializerInner(void* serializer) {
    if (!serializer) return -1;
    void* streamPtr = *reinterpret_cast<void**>(serializer);
    if (!streamPtr) return -1;
    return static_cast<int>(reinterpret_cast<IO::Serializer*>(serializer)->GetDirection());   // B.3.4 bridge  (IO::Serializer::GetDirection, retail 0x180006210)
}

// ENDGAME B.3.4a closing: these two were a two-stage door — the 0x140152ad8 /
// 0x140152ad0 dispatch slot first, then the g_fn_Serializer_Ctor/Dtor globals,
// then a one-shot "local serializer stays uninitialized" warning if BOTH were
// null. `IO::Serializer` is a real class now, so there is no door left to be
// null and no third branch to take: the ctor warning and its `s_warned` latch
// are deleted because the state they reported cannot occur, not because the
// warning was noisy. The dtor's fallback chain had no else arm at all.
void CallSerializerCtor(void* localSer, void* stream, int version) {
    ::new (localSer) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), version);   // B.3.4 bridge
}

void CallSerializerDtor(void* localSer) {
    reinterpret_cast<IO::Serializer*>(localSer)->~Serializer();   // B.3.4 bridge
}

// ============================================================
// 04d0 — Geometry::Serialize body (15 steps)
// ============================================================
//
// Spec: notes/M35_geom_serialize_findings.md lines 42-112.
// param_1 = self (Geometry*)
// param_2 = stream (Stream*)
//
// Local serializer is on stack: 24 bytes (3 qwords) — matches `local_50 [3]`
// in the original decompile. We use a uint64_t[3] buffer so the Runtime ctor
// has the expected scratch space.
void TradeWork_impl_04d0_Body(void* self, void* stream) {
    if (!self || !stream) return;

    uint8_t* selfBytes = reinterpret_cast<uint8_t*>(self);

    // Local Serializer scratch (24 bytes).
    uint64_t local_ser[3] = {0, 0, 0};
    void* serializer = static_cast<void*>(local_ser);

    // Resolve the remaining non-Serializer dispatch slots — cached statics,
    // one-shot warnings.
    static FnVector2Ser_t    s_vec2Ser = nullptr;       static bool t_vec2Ser = false;
    // ENDGAME B.3.5 batch K1: the VertexBuffer::Create / IndexBuffer::Create caches,
    // their typedefs, their two `kSlot_*` constants and their ResolveSlot lines went
    // with their last callers in Step 14, which call the members.
    FnVector2Ser_t   vec2Ser   = ResolveSlot(kSlot_Vector2Ser,       s_vec2Ser,   t_vec2Ser,   "Vector2::Serialize");

    auto serIntField = [&](void* field) {
        reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<int*>(field), nullptr);   // B.3.4 bridge
    };

    RZ_CHECK("geom 04d0 entry");

    // ---- Step 1: Construct local Serializer with version 6. ----
    CallSerializerCtor(serializer, stream, 6);

    // ---- Step 2: Serialize 6 ints + 1 packed dim at +0xC0 .. +0xE0. ----
    // Disassembly @ 0x1400b051f .. 0x1400b059e: 6 calls to slot 0x140152ac8
    // for fields +0xC0, +0xC4, +0xC8, +0xCC, +0xD0, +0xD4.
    int32_t* gridSizePtr = reinterpret_cast<int32_t*>(selfBytes + 0xC0);
    serIntField(selfBytes + 0xC0);
    serIntField(selfBytes + 0xC4);
    serIntField(selfBytes + 0xC8);
    serIntField(selfBytes + 0xCC);
    serIntField(selfBytes + 0xD0);
    serIntField(selfBytes + 0xD4);

    // ---- Step 3: Serialize 2-byte-element array at +0xD8 (count=+0xE0). ----
    // Verified original disasm @ 0x1400b0589..0x1400b05ce: local_count is the
    // ELEMENT count (no shift). Both resize and serialize use it directly.
    // The original calls slot 0x1401523f8 (F = short*); slot 0x140152488
    // (G = ushort*) reads identical bytes, so we route through the ushort overload.
    int32_t local_count = *reinterpret_cast<int32_t*>(selfBytes + 0xE0);
    serIntField(&local_count);
    if (SerializerInner(serializer) == 0) {
        Reimpl_Array_Resize_Stride02(selfBytes + 0xD8, local_count);
    }
    {
        uint16_t* arr16 = *reinterpret_cast<uint16_t**>(selfBytes + 0xD8);
        if (arr16) reinterpret_cast<IO::Serializer*>(serializer)->Serialize(local_count, arr16, nullptr);   // B.3.4 bridge
    }

    RZ_CHECK("geom 04d0 after step3 (+0xD8 uint16 heightmap arr)");

    // ---- Step 4: Typed-ExternalPtr serialize at +0xF8 (CASCADE). ----
    // CASCADE: MaterialInstance (slot 0x140151350). Session 3 reimpl logs
    // one-shot warning if typeId not registered; menu still loads.
    Reimpl_ExternalPtr_Serialize_Typed_7140(selfBytes + 0xF8, serializer);

    // ---- Step 5: Heightmap raw bytes at +0x140 (stream-direct, 4 bytes/cell). ----
    // On loading: build mesh first via sub_c1e0, then alloc heightmap buffer,
    // then serialize bytes. On saving: serialize from existing pointer.
    int gridSize = (gridSizePtr ? *gridSizePtr : 0);
    int byteCount = gridSize * gridSize * 4;
    void* heightBuf = nullptr;

    // The original checks `*(int*)(stream + 8) == 0` (stream's load flag).
    // This is offset 8 of the stream object directly — IsLoading=0, IsSaving=1.
    int streamInner = -1;
    if (stream) {
        streamInner = *reinterpret_cast<int*>(reinterpret_cast<uint8_t*>(stream) + 8);
    }

    if (streamInner == 0) {
        // Loading path: build mesh + allocate heightmap.
        Reimpl_TradeWork_sub_c1e0(self);
        if (fn_MemAlloc && g_pMemAllocSingleton && byteCount > 0) {
            heightBuf = fn_MemAlloc(g_pMemAllocSingleton, byteCount, 4);
            *reinterpret_cast<void**>(selfBytes + 0x140) = heightBuf;
        }
    } else {
        heightBuf = *reinterpret_cast<void**>(selfBytes + 0x140);
    }
    // Serialize raw heightmap bytes. ENDGAME B.3.4a closing: the `else if` arm
    // was the SerializeBulk (slot 0x140152400) fallback for "the 0x140152618
    // SerializeBytes door did not resolve". Retail calls the 618 door; with the
    // door gone that state cannot occur, so the compensating arm goes with it.
    if (heightBuf && byteCount > 0) {
        reinterpret_cast<IO::Serializer*>(serializer)->SerializeBytes(byteCount, heightBuf, nullptr);   // B.3.4 bridge
    }

    RZ_CHECK("geom 04d0 after step5 (sub_c1e0 mesh build + heightBuf)");

    // ---- Step 6: Version-gated mesh-vert/idx buffer block. ----
    int version = reinterpret_cast<IO::Serializer*>(serializer)->GetVersion();   // B.3.4 bridge
    if (version < 3) {
        // OLD format: bool + uint count + raw bytes for both vert + idx buffers.
        bool flag = false;
        int32_t cnt = 0;
        reinterpret_cast<IO::Serializer*>(serializer)->Serialize(flag, nullptr);   // B.3.4 bridge
        serIntField(&cnt);
        // Mesh-vertex resize (16-byte stride): cnt >> 4.
        Reimpl_Array_Resize_Stride16B(selfBytes + 0x1A0, cnt >> 4);
        {
            void* vertData = *reinterpret_cast<void**>(selfBytes + 0x1A0);
            if (vertData) reinterpret_cast<IO::Serializer*>(serializer)->SerializeBytes(cnt, vertData, nullptr);   // B.3.4 bridge
        }
        reinterpret_cast<IO::Serializer*>(serializer)->Serialize(flag, nullptr);   // B.3.4 bridge
        serIntField(&cnt);
        // Index buffer resize (uint16 stride): cnt >> 1.
        Reimpl_Array_Resize_Stride02(selfBytes + 0x1B0, cnt >> 1);
        {
            void* idxData = *reinterpret_cast<void**>(selfBytes + 0x1B0);
            if (idxData) reinterpret_cast<IO::Serializer*>(serializer)->SerializeBytes(cnt, idxData, nullptr);   // B.3.4 bridge
        }
    } else {
        // NEW format: per-element loops.
        // Vertex array at +0x1A0 (count +0x1A8), stride 0x10 (4 ints + 1 short + 4 bytes).
        int32_t vcount = *reinterpret_cast<int32_t*>(selfBytes + 0x1A8);
        serIntField(&vcount);
        if (SerializerInner(serializer) == 0) {
            Reimpl_Array_Resize_Stride16B(selfBytes + 0x1A0, vcount);
        }
        // Per-element loop (matches 0x1400b073f .. 0x1400b0768):
        //   foreach: Serialize(4, elem-0xC, name); Serialize(2, elem-4, name);
        //            Serialize(4, elem+0, name);
        // Element pointer walks from (vertdata + 0xC) i.e., 12 into each 16-byte vert.
        if (vcount > 0) {
            uint8_t* vbase = *reinterpret_cast<uint8_t**>(selfBytes + 0x1A0);
            if (vbase) {
                uint8_t* p = vbase + 0xC;
                for (int i = 0; i < vcount; ++i) {
                    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(4, reinterpret_cast<short*>(p - 0xC), nullptr);   // B.3.4 bridge
                    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(2, reinterpret_cast<short*>(p - 4), nullptr);     // B.3.4 bridge
                    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(4, reinterpret_cast<unsigned char*>(p), nullptr);   // B.3.4 bridge
                    p += 0x10;
                }
            }
        }
        // Index array at +0x1B0 (count +0x1B8), stride 0x02.
        int32_t icount = *reinterpret_cast<int32_t*>(selfBytes + 0x1B8);
        serIntField(&icount);
        if (SerializerInner(serializer) == 0) {
            Reimpl_Array_Resize_Stride02(selfBytes + 0x1B0, icount);
        }
        {
            uint16_t* iarr = *reinterpret_cast<uint16_t**>(selfBytes + 0x1B0);
            if (iarr) reinterpret_cast<IO::Serializer*>(serializer)->Serialize(icount, iarr, nullptr);   // B.3.4 bridge
        }
    }

    RZ_CHECK("geom 04d0 after step6 (vertex/index arrays)");

    // ---- Step 7: Two more typed-ExternalPtr serializes at +0x150 / +0x158 (CASCADE x2). ----
    // CASCADE: MaterialInstance again (same typeInfo slot as step 4).
    Reimpl_ExternalPtr_Serialize_Typed_7140(selfBytes + 0x150, serializer);
    Reimpl_ExternalPtr_Serialize_Typed_7140(selfBytes + 0x158, serializer);

    // ---- Step 8: Two parallel int4-array fields at +0x1C0 and +0x1D0. ----
    // Each: count int, resize stride 0x10 (align 4), per-element 4 ints.
    {
        int32_t c = *reinterpret_cast<int32_t*>(selfBytes + 0x1C8);
        serIntField(&c);
        if (SerializerInner(serializer) == 0) {
            Reimpl_Array_Resize_Stride16A(selfBytes + 0x1C0, c);
        }
        if (c > 0) {
            uint8_t* base = *reinterpret_cast<uint8_t**>(selfBytes + 0x1C0);
            if (base) {
                int32_t* p = reinterpret_cast<int32_t*>(base + 8);
                for (int i = 0; i < c; ++i) {
                    serIntField(p - 2);
                    serIntField(p - 1);
                    serIntField(p);
                    serIntField(p + 1);
                    p += 4;
                }
            }
        }
    }
    {
        int32_t c = *reinterpret_cast<int32_t*>(selfBytes + 0x1D8);
        serIntField(&c);
        if (SerializerInner(serializer) == 0) {
            Reimpl_Array_Resize_Stride16A(selfBytes + 0x1D0, c);
        }
        if (c > 0) {
            uint8_t* base = *reinterpret_cast<uint8_t**>(selfBytes + 0x1D0);
            if (base) {
                int32_t* p = reinterpret_cast<int32_t*>(base + 8);
                for (int i = 0; i < c; ++i) {
                    serIntField(p - 2);
                    serIntField(p - 1);
                    serIntField(p);
                    serIntField(p + 1);
                    p += 4;
                }
            }
        }
    }

    RZ_CHECK("geom 04d0 after step8 (int4 arrays +0x1C0/+0x1D0)");

    // ---- Step 9: Array-of-arrays at +0x1E0 (count +0x1E8). ----
    // Outer 16-byte stride; inner = Vector2 (8B/elem).
    {
        int32_t outerCount = *reinterpret_cast<int32_t*>(selfBytes + 0x1E8);
        serIntField(&outerCount);
        if (SerializerInner(serializer) == 0) {
            Reimpl_Array_Resize_ArrayOfArrays(selfBytes + 0x1E0, outerCount);
        }
        if (outerCount > 0) {
            // Outer base = *(qword*)(self+0x1E0) — array of {data,count} pairs (stride 0x10).
            uint64_t* outerBase = *reinterpret_cast<uint64_t**>(selfBytes + 0x1E0);
            if (outerBase) {
                for (int i = 0; i < outerCount; ++i) {
                    uint64_t* outerElem = outerBase + (i * 2);  // stride 0x10 = 2 qwords
                    int32_t innerCount = static_cast<int32_t>(outerElem[1]);
                    serIntField(&innerCount);
                    if (SerializerInner(serializer) == 0) {
                        // FUN_1400203a0 — 8-byte Vector2 stride resize. Use the
                        // session-prior stride-08 wrapper.
                        if (fn_Array_Resize_Stride08) {
                            fn_Array_Resize_Stride08(outerElem, innerCount);
                        }
                    }
                    if (vec2Ser && innerCount > 0) {
                        void* innerData = reinterpret_cast<void*>(outerElem[0]);
                        if (innerData) {
                            uint8_t* vec2 = reinterpret_cast<uint8_t*>(innerData);
                            for (int j = 0; j < innerCount; ++j) {
                                // Original disasm increments `this` by 8 (one Vector2)
                                // BUT the decompile says `this + 8` which Ghidra mis-renders;
                                // the actual increment is 8 bytes per Math::Vector2 element.
                                vec2Ser(vec2, serializer);
                                vec2 += 8;
                            }
                        }
                    }
                    // outerElem already advanced by stride; loop variable handles it.
                }
            }
        }
    }

    RZ_CHECK("geom 04d0 after step9 (array-of-arrays +0x1E0)");

    // ---- Step 10: Complex 0x60B struct array at +0x200 (count +0x208) (CASCADE x N). ----
    // Resize via session 2 helper (handles copy-ctor/dtor of embedded sub-arrays);
    // per-element serialize via session 4 helper.
    {
        int32_t c = *reinterpret_cast<int32_t*>(selfBytes + 0x208);
        serIntField(&c);
        if (SerializerInner(serializer) == 0) {
            Reimpl_Struct60B_Resize(selfBytes + 0x200, static_cast<unsigned>(c));
        }
        // Per-element serialize. CASCADE: each element's +0x00 is a typed
        // ExternalPtr (MaterialInstance). Soft-fails per-element on cascade.
        void* arrBase = *reinterpret_cast<void**>(selfBytes + 0x200);
        Reimpl_Struct60B_Serialize(serializer, c, arrBase);
    }

    RZ_CHECK("geom 04d0 after step10 (Struct60B array +0x200)");

    // ---- Step 11: Version 4+ array at +0x220 (count +0x228), stride 0x04. ----
    if (version >= 4) {
        int32_t c = *reinterpret_cast<int32_t*>(selfBytes + 0x228);
        serIntField(&c);
        if (SerializerInner(serializer) == 0) {
            Reimpl_Array_Resize_Stride04(selfBytes + 0x220, c);
        }
        // Original at 0x1400b0a55 calls slot 0x140151278 (NOT 0x140152488).
        // Slot 0x140151278 = `IO::Serializer::Serialize(int count, int* data, name)` —
        // bulk INT array (4 bytes per element). Slot 0x140152488 is the bulk
        // uint16 variant — wrong stride for stride-04 int array.
        {
            int32_t* iarr = *reinterpret_cast<int32_t**>(selfBytes + 0x220);
            if (iarr) reinterpret_cast<IO::Serializer*>(serializer)->Serialize(c, iarr, nullptr);   // B.3.4 bridge
        }
    }

    // ---- Step 12: Version 5- backfill loop into +0x210 from +0x200 elements. ----
    if (version < 5 && SerializerInner(serializer) == 0) {
        // FUN_140001bb0 — resize stride-08 array. Use prior session helper.
        int32_t srcCount = *reinterpret_cast<int32_t*>(selfBytes + 0x208);
        if (fn_Array_Resize_Stride08) {
            fn_Array_Resize_Stride08(selfBytes + 0x210, srcCount);
        }
        int32_t dstCount = *reinterpret_cast<int32_t*>(selfBytes + 0x218);
        if (dstCount > 0) {
            void* srcBase = *reinterpret_cast<void**>(selfBytes + 0x200);
            void* dstBase = *reinterpret_cast<void**>(selfBytes + 0x210);
            int32_t gv = (gridSizePtr ? *gridSizePtr : 0);
            if (srcBase && dstBase) {
                uint8_t* src = reinterpret_cast<uint8_t*>(srcBase);
                uint8_t* dst = reinterpret_cast<uint8_t*>(dstBase);
                for (int i = 0; i < dstCount; ++i) {
                    // Copy src+0x00 (qword) into dst+0x00 (stride 0x08).
                    *reinterpret_cast<uint64_t*>(dst + i * 8) =
                        *reinterpret_cast<const uint64_t*>(src + i * 0x60);
                    // Zero src+0x2c, src+0x30; backfill src+0x34, src+0x38 with grid size.
                    *reinterpret_cast<uint32_t*>(src + i * 0x60 + 0x2C) = 0;
                    *reinterpret_cast<uint32_t*>(src + i * 0x60 + 0x30) = 0;
                    *reinterpret_cast<uint32_t*>(src + i * 0x60 + 0x34) = gv;
                    *reinterpret_cast<uint32_t*>(src + i * 0x60 + 0x38) = gv;
                }
            }
        }
    }

    // ---- Step 13: Version 6+ array at +0x210 (count +0x218) (CASCADE). ----
    // CASCADE: MaterialInstance via FUN_140020150 (session 4 reimpl).
    if (version >= 6) {
        int32_t c = *reinterpret_cast<int32_t*>(selfBytes + 0x218);
        serIntField(&c);
        if (SerializerInner(serializer) == 0) {
            if (fn_Array_Resize_Stride08) {
                fn_Array_Resize_Stride08(selfBytes + 0x210, c);
            }
        }
        void* arrBase = *reinterpret_cast<void**>(selfBytes + 0x210);
        // CASCADE: MaterialInstance — session 4 reimpl handles typeInfo null.
        Reimpl_Array_Serialize_External_MaterialInstance(serializer, c, arrBase);
    }

    // ---- Step 14: Final IsLoading-only block. ----
    // Original tests `*(int*)(stream + 8) == 0` (stream load flag).
    if (streamInner == 0) {
        // 14a: Video::IndexBuffer::Create at +0x180 with idxCount*2 bytes.
        // 14b: Video::VertexBuffer::Create at +0x160 with vertCount*16 bytes.
        // 14c: TradeWork_impl_sub_c780 — image-buffer heightmap upload.
        // 14d: Final fixup — if +0x218 != +0x228, resize +0x210 to +0x228.
        int32_t idxCount   = *reinterpret_cast<int32_t*>(selfBytes + 0x1B8);
        int32_t vertCount  = *reinterpret_cast<int32_t*>(selfBytes + 0x1A8);
        void*   idxData    = *reinterpret_cast<void**>(selfBytes + 0x1B0);
        void*   vertData   = *reinterpret_cast<void**>(selfBytes + 0x1A0);
        // B.3.5 bridge — both `Create`s are members of block D's real classes now. The
        // first `int` parameter is named `count` in Buffers.h and `sizeBytes` in the old
        // game-side typedef; POSITIONAL order is unchanged by this commit, so behaviour
        // is identical and the naming disagreement is pre-existing (batch I1 reported it).
        reinterpret_cast<Video::IndexBuffer*>(selfBytes + 0x180)
            ->Create(idxCount * 2, false, idxData);          // B.3.5 bridge
        reinterpret_cast<Video::VertexBuffer*>(selfBytes + 0x160)
            ->Create(vertCount * 0x10, false, vertData);     // B.3.5 bridge
        // Image-buffer heightmap upload (CASCADE: requires Runtime exports).
        Reimpl_TradeWork_sub_c780(self);

        int32_t c218 = *reinterpret_cast<int32_t*>(selfBytes + 0x218);
        int32_t c228 = *reinterpret_cast<int32_t*>(selfBytes + 0x228);
        if (c218 != c228 && fn_Array_Resize_Stride08) {
            fn_Array_Resize_Stride08(selfBytes + 0x210, c228);
        }
    }

    RZ_CHECK("geom 04d0 after step14 (VB/IB create + image upload)");

    // ---- Step 15: Local Serializer dtor. ----
    CallSerializerDtor(serializer);
    RZ_CHECK("geom 04d0 exit");
}

// ============================================================
// f1a0 — Version-gated extra Geometry serialize (4 steps)
// ============================================================
//
// Spec: notes/M35_geom_serialize_findings.md lines 114-127.
// Constructs a separate IO::Serializer with version 4 (NOT version 6).
// Reads the OLD-schema +0x1F0 / +0x200 arrays before the new-schema +0x210.
void TradeWork_impl_f1a0_Body(void* self, void* stream) {
    if (!self || !stream) return;

    uint8_t* selfBytes = reinterpret_cast<uint8_t*>(self);
    uint64_t local_ser[3] = {0, 0, 0};
    void* serializer = static_cast<void*>(local_ser);

    auto serIntField = [&](void* field) {
        reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<int*>(field), nullptr);   // B.3.4 bridge
    };

    RZ_CHECK("geom f1a0 entry");

    // ---- Step 1: Construct local Serializer with version 4. ----
    CallSerializerCtor(serializer, stream, 4);

    // ---- Step 2 (was step 1 in spec): Serialize array at +0x1F0 (count +0x1F8). ----
    // Same complex 0x60-byte struct as +0x200. CASCADE.
    {
        int32_t c = *reinterpret_cast<int32_t*>(selfBytes + 0x1F8);
        serIntField(&c);
        if (SerializerInner(serializer) == 0) {
            Reimpl_Struct60B_Resize(selfBytes + 0x1F0, static_cast<unsigned>(c));
        }
        void* base = *reinterpret_cast<void**>(selfBytes + 0x1F0);
        Reimpl_Struct60B_Serialize(serializer, c, base);
    }

    // ---- Step 3 (was step 2 in spec): Serialize array at +0x200 (count +0x208). ----
    // Duplicate of the outer +0x200 schema. CASCADE.
    {
        int32_t c = *reinterpret_cast<int32_t*>(selfBytes + 0x208);
        serIntField(&c);
        if (SerializerInner(serializer) == 0) {
            Reimpl_Struct60B_Resize(selfBytes + 0x200, static_cast<unsigned>(c));
        }
        void* base = *reinterpret_cast<void**>(selfBytes + 0x200);
        Reimpl_Struct60B_Serialize(serializer, c, base);
    }

    // ---- Step 4: Version 0-2 backfill into +0x210 from +0x200. ----
    int version = reinterpret_cast<IO::Serializer*>(serializer)->GetVersion();   // B.3.4 bridge
    if (version < 3 && SerializerInner(serializer) == 0) {
        int32_t srcCount = *reinterpret_cast<int32_t*>(selfBytes + 0x208);
        if (fn_Array_Resize_Stride08) {
            fn_Array_Resize_Stride08(selfBytes + 0x210, srcCount);
        }
        int32_t dstCount = *reinterpret_cast<int32_t*>(selfBytes + 0x218);
        int32_t gridSize = *reinterpret_cast<int32_t*>(selfBytes + 0xC0);
        void* srcBase = *reinterpret_cast<void**>(selfBytes + 0x200);
        void* dstBase = *reinterpret_cast<void**>(selfBytes + 0x210);
        if (dstCount > 0 && srcBase && dstBase) {
            uint8_t* src = reinterpret_cast<uint8_t*>(srcBase);
            uint8_t* dst = reinterpret_cast<uint8_t*>(dstBase);
            for (int i = 0; i < dstCount; ++i) {
                *reinterpret_cast<uint64_t*>(dst + i * 8) =
                    *reinterpret_cast<const uint64_t*>(src + i * 0x60);
                *reinterpret_cast<uint32_t*>(src + i * 0x60 + 0x2C) = 0;
                *reinterpret_cast<uint32_t*>(src + i * 0x60 + 0x30) = 0;
                *reinterpret_cast<uint32_t*>(src + i * 0x60 + 0x34) = gridSize;
                *reinterpret_cast<uint32_t*>(src + i * 0x60 + 0x38) = gridSize;
            }
        }
    }

    // ---- Step 5 (was step 4 in spec): Version 3+ array at +0x210 (count +0x218). ----
    // CASCADE: MaterialInstance via FUN_140020150.
    if (version >= 3) {
        int32_t c = *reinterpret_cast<int32_t*>(selfBytes + 0x218);
        serIntField(&c);
        if (SerializerInner(serializer) == 0) {
            if (fn_Array_Resize_Stride08) {
                fn_Array_Resize_Stride08(selfBytes + 0x210, c);
            }
        }
        void* base = *reinterpret_cast<void**>(selfBytes + 0x210);
        Reimpl_Array_Serialize_External_MaterialInstance(serializer, c, base);
    }

    RZ_CHECK("geom f1a0 before dtor");

    // ---- Step 6: Local Serializer dtor. ----
    CallSerializerDtor(serializer);
}

} // anonymous namespace

// ============================================================
// Public reimpl entry points
// ============================================================
//
// Each entry tries the original exe address first (usable when the exe code
// page is mapped). If that pointer is null (no exe code), we fall through to
// our cleaned C body.

extern "C" void Reimpl_TradeWork_impl_04d0(void* self, void* stream) {
    TradeWork_impl_04d0_Body(self, stream);
}

extern "C" void Reimpl_TradeWork_impl_f1a0(void* self, void* stream) {
    TradeWork_impl_f1a0_Body(self, stream);
}
