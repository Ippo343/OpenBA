// code/Game/Game/serialize_arrays.cpp
// Generic Geometry/Transform/Array serialize helpers used by MenuScene::Slot5.
//
// Decompiled from Application-x64.exe. These helpers are not MenuScene-
// specific — Geometry::Serialize and other scene serializers reuse them.
//
// Helper map (M3.5 / notes/M35_recon.md):
//   #1 Geometry init                — exe 0x1400b00e0  (no cascade)
//   #2 Transform::Serialize wrapper — exe 0x1400988c0  (no cascade)
//   #3 Array_Resize stride 0x20     — exe 0x14001baf0  (no cascade)
//   #4 Array_Resize stride 0x08     — exe 0x140001bb0  (no cascade)
//   #5 Array_Resize stride 0x14     — exe 0x14001bf10  (no cascade)
//   #6 Array_Serialize stride 0x20  — exe 0x14001bbf0  (no cascade)
//   #7 Array_Serialize_External GraphicsMesh — exe 0x14001bcc0  (CASCADE)
//   #8 Array_Serialize_External Particle     — exe 0x14001bff0  (CASCADE)
//
// Helpers #7/#8 share a byte-identical body that differs only in the
// BaseTypeInfo* slot it dereferences. We extract that body into a single
// generic implementation parameterized by typeIdPtr.
//
// **param_4 mystery (recon question 2):** the Ghidra decompile of #7 and #8
// shows `undefined8 param_4` threaded into the function but never read past
// `local_res20 = param_4` (which is then overwritten with GetReferenceId's
// return). The caller (MenuScene::Slot5 at 0x1400a3779/0x1400a3928) never
// sets R9 — only RCX/RDX/R8 are loaded before the CALL. **param_4 is
// decompiler noise** (R9 leftover from prior code), so our reimpl signature
// drops it entirely.

#include "engine_funcs.h"
#include "resolve.h"
#include "Object/factory_registration.h"
#include <cstdint>
#include <cstdio>
#include <cstring>

// ============================================================
// Helper #1 — Geometry init (FUN_1400b00e0)
// ============================================================
//
// Constructs a 0x230-byte Graphics::Geometry-derived container ("TerrainGenerator"
// per the type-name string at exe 0x14015f158). Layout per Ghidra disasm of
// FUN_1400b00e0:
//   +0x000  Graphics::Geometry::Geometry(self)        — base ctor (0xb8 bytes)
//   +0x000  vtable = exe 0x14015f110 (derived 9-slot)
//   +0x0c0..+0x0f8  zeroed (mesh state slots)
//   +0x100  Video::VertexBuffer ctor (~0x20 bytes)
//   +0x120  Video::IndexBuffer  ctor (~0x30 bytes)
//   +0x150,+0x158  zeroed
//   +0x160  Video::VertexBuffer ctor
//   +0x180  Video::IndexBuffer  ctor
//   +0x1a0..+0x228  zeroed
//   +0x030  = 1  (state flag)
//
// DLL mode: fn_Geometry_Init holds the original exe code → passthrough.
// Standalone mode: fn_Geometry_Init resolves to ourselves via the address
//                  registry, so we run the cleaned body using Runtime DLL
//                  ctors directly (no recursion).
extern "C" void Reimpl_Geometry_Init(void* self) {
    if (!self) return;

#ifndef STANDALONE_EXE
    if (fn_Geometry_Init) {
        fn_Geometry_Init(self);
        return;
    }
#endif

    if (!fn_Graphics_Geometry_Ctor || !fn_Video_VertexBuffer_Ctor ||
        !fn_Video_IndexBuffer_Ctor) {
        static bool s_warned = false;
        if (!s_warned) {
            printf("[Geometry::Init] Runtime ctors unresolved "
                   "(GeomC=%p VB=%p IB=%p) — zero-fill fallback (self=%p)\n",
                   (void*)fn_Graphics_Geometry_Ctor,
                   (void*)fn_Video_VertexBuffer_Ctor,
                   (void*)fn_Video_IndexBuffer_Ctor, self);
            fflush(stdout);
            s_warned = true;
        }
        memset(self, 0, 0x230);
        return;
    }

    fn_Graphics_Geometry_Ctor(self);

    // Override base vtable with the derived "TerrainGenerator" 9-slot vtable.
    // The exe-resident vtable at 0x14015f110 holds 9 fn ptrs to exe code; in
    // standalone mode those code addresses may not be registered, but the
    // vtable pointer itself is needed so virtual dispatch through the
    // geometry has *something*. Callers in the Slot5 path (TradeWork_impl_04d0
    // / _f1a0) don't dispatch through this vtable, so leaving exe slot
    // contents unresolved is safe for the immediate menu render.
    uintptr_t vtbl = ResolveAddr(Module::Exe, 0x14015f110);
    if (vtbl) {
        *reinterpret_cast<uintptr_t*>(self) = vtbl;
    }

    auto* base = reinterpret_cast<uint8_t*>(self);
    *reinterpret_cast<uint64_t*>(base + 0xc0) = 0;
    *reinterpret_cast<uint64_t*>(base + 0xc8) = 0;
    *reinterpret_cast<uint32_t*>(base + 0xd0) = 0;
    *reinterpret_cast<uint64_t*>(base + 0xd8) = 0;
    *reinterpret_cast<uint64_t*>(base + 0xe0) = 0;
    *reinterpret_cast<uint64_t*>(base + 0xe8) = 0;
    *reinterpret_cast<uint32_t*>(base + 0xf0) = 0;
    *reinterpret_cast<uint64_t*>(base + 0xf8) = 0;

    fn_Video_VertexBuffer_Ctor(base + 0x100);
    fn_Video_IndexBuffer_Ctor(base + 0x120);

    *reinterpret_cast<uint64_t*>(base + 0x150) = 0;
    *reinterpret_cast<uint64_t*>(base + 0x158) = 0;

    fn_Video_VertexBuffer_Ctor(base + 0x160);
    fn_Video_IndexBuffer_Ctor(base + 0x180);

    for (int off = 0x1a0; off <= 0x228; off += 8) {
        *reinterpret_cast<uint64_t*>(base + off) = 0;
    }

    *reinterpret_cast<uint32_t*>(base + 0x30) = 1;
}

// ============================================================
// Standalone-only stub vtable for the derived Geometry / "TerrainGenerator"
// class. The 9-slot exe vtable lives at 0x14015f110; in DLL mode the original
// exe code backs each slot, but in standalone none of those exe addresses are
// registered, so the Geometry retained the Runtime base Graphics::Geometry
// vtable — whose Draw slot (+0x18) is _purecall. Renderer::Draw walks
// Scene+0x60 and dispatches vtable[+0x18] on every object, hitting MSVC's
// _purecall handler (a MessageBox).
//
// Until the 9 derived methods are reimplemented properly, register a stub
// vtable here. The slots are best-effort: dtor returns self, Draw is a void
// no-op (terrain just won't render), the rest return 0/null. The crucial
// property is that none of them is _purecall.
//
// Original exe vtable contents (from Ghidra read of 0x14015f110):
//   +0x00  0x14001ECC0   dtor
//   +0x08  0x1400AED40   ?
//   +0x10  0x1400AEA10   ?
//   +0x18  0x1400AE490   Draw
//   +0x20  0x14003AA7A   ?
//   +0x28  0x14003AAA4   ?
//   +0x30  0x14003AB22   ?
//   +0x38  0x14003AB1C   ?
//   +0x40  0x14003AAAA   ?
// ============================================================
#ifdef STANDALONE_EXE
// Real implementations of the 9 vtable slots live in
// code/Game/Game/geometry_draw.cpp. They're decompiled from the original exe
// at 0x14001ECC0 / 0x1400AED40 / 0x1400AEA10 / 0x1400AE490 / Runtime IAT
// thunks, and wired through engine_funcs.h fn_* pointers (resolved via
// ResolveExport from Runtime DLL exports).
extern "C" {
    void* GeomDraw_Dtor(void* self, uint32_t flags);
    void  GeomDraw_ChunkUpdate(void* self);
    void  GeomDraw_ItemDraw(void* self, void* context, void* p3, void* p4,
                            float param5, uint32_t param6);
    void  GeomDraw_CollectDraw(void* self, void* unused, void* renderer);
    void  GeomDraw_UpdateAttached(void* self);
    void* GeomDraw_GetSubTransform(void* self, void* retval, int idx);
    void* GeomDraw_GetInstanceTransform(void* self, void* retval, int idx, float t);
    int   GeomDraw_SetSharedInstanceData(void* self, void* context, int idx, float t);
    bool  GeomDraw_Clean(void* self);
}

namespace {
void* g_Geometry_Vtable_Standalone[9] = {
    reinterpret_cast<void*>(&GeomDraw_Dtor),                  // +0x00 dtor
    reinterpret_cast<void*>(&GeomDraw_ChunkUpdate),           // +0x08 chunk-streaming
    reinterpret_cast<void*>(&GeomDraw_ItemDraw),              // +0x10 per-item Draw
    reinterpret_cast<void*>(&GeomDraw_CollectDraw),           // +0x18 collect Draw
    reinterpret_cast<void*>(&GeomDraw_UpdateAttached),        // +0x20 UpdateAttached
    reinterpret_cast<void*>(&GeomDraw_GetSubTransform),       // +0x28 GetSubTransform
    reinterpret_cast<void*>(&GeomDraw_GetInstanceTransform),  // +0x30 GetInstanceTransform
    reinterpret_cast<void*>(&GeomDraw_SetSharedInstanceData), // +0x38 SetSharedInstanceData
    reinterpret_cast<void*>(&GeomDraw_Clean),                 // +0x40 Clean
};
}  // namespace

extern "C" void RegisterGeometryVtable() {
    extern void RegisterExeAddr(uintptr_t exeAddr, uintptr_t standaloneAddr);
    RegisterExeAddr(0x14015f110,
                    reinterpret_cast<uintptr_t>(&g_Geometry_Vtable_Standalone));
}
#endif

// ============================================================
// Helper #2 — Transform::Serialize wrapper (FUN_1400988c0)
// ============================================================
//
// Wraps a Stream in a Serializer, then delegates to Math::Transform::Serialize
// + Math::Vector3::Serialize at +0x1c + 3 IO::Serializer::Serialize(float)
// calls at +0x28/+0x2c/+0x30.
extern "C" void Reimpl_Transform_SerializeWrapper(void* transform, void* stream) {
    if (!transform || !stream) return;

#ifndef STANDALONE_EXE
    if (fn_Transform_SerializeWrapper) {
        fn_Transform_SerializeWrapper(transform, stream);
        return;
    }
#endif

    static bool s_warned = false;
    if (!s_warned) {
        printf("[Transform::SerializeWrapper] unresolved — Transform fields "
               "remain at default values (transform=%p stream=%p)\n",
               transform, stream);
        fflush(stdout);
        s_warned = true;
    }
}

// ============================================================
// Generic dynamic-array resize implementation
// ============================================================
//
// Layout of arrayHeader (16 bytes):
//   +0x00: void*  data        — buffer base (or NULL)
//   +0x08: int32  count       — current length (visible to caller)
//   +0x0C: int32  capacity    — allocated slots (>= count)
//
// Algorithm matches FUN_14001baf0 / FUN_140001bb0 / FUN_14001bf10:
//   if (newCount == count) return;
//   if (newCount > capacity) {
//       capacity = newCount;
//       if (newCount == count + 1 && count != 0) capacity = newCount * 3;
//       newBuf = MemAlloc(capacity * stride, alignment);
//       (caller continues below to copy)
//   }
//   for slot in [start..newCount):
//       if (slot < oldCount) memcpy elem from oldBuf
//       else if (newBuf was reallocated) zero-init elem
//   if (oldBuf != newBuf) MemFree(oldBuf)
//
// `start` = oldCount when buf didn't reallocate, else 0.
//
// Note: ALL variants zero every new slot (slot >= oldCount) on BOTH paths,
// matching the original exe resize functions. An earlier version assumed
// "MemAlloc returns zeroed memory" and only zeroed on the realloc path —
// that was WRONG (the engine boundary-tag allocator at Runtime 0x1800c6e60
// returns DIRTY memory), and on an in-place grow it left new slots holding
// stale bytes (a freed-block pointer) → free-list corruption when a later
// serialize treated the stale pointer as live. See the unconditional-zero
// branch below.
namespace {

void GenericArrayResize(void* arrayHeader, int newCount,
                        size_t stride, int alignment) {
    if (!arrayHeader) return;
    if (!fn_MemAlloc || !g_pMemAllocSingleton) {
        static bool s_warned = false;
        if (!s_warned) {
            printf("[Array::Resize] MemAlloc unavailable — leaving array unchanged "
                   "(header=%p newCount=%d stride=%zu)\n",
                   arrayHeader, newCount, stride);
            fflush(stdout);
            s_warned = true;
        }
        return;
    }

    auto* hdr = reinterpret_cast<uint8_t*>(arrayHeader);
    void** dataSlot = reinterpret_cast<void**>(hdr + 0x00);
    int32_t* countSlot = reinterpret_cast<int32_t*>(hdr + 0x08);
    int32_t* capSlot = reinterpret_cast<int32_t*>(hdr + 0x0C);

    int oldCount = *countSlot;
    if (newCount == oldCount) return;

    void* oldData = *dataSlot;
    *countSlot = newCount;

    if (*capSlot < newCount) {
        *capSlot = newCount;
        if (newCount == oldCount + 1 && oldCount != 0) {
            *capSlot = newCount * 3;
        }
        size_t bytes = static_cast<size_t>(*capSlot) * stride;
        void* newBuf = fn_MemAlloc(g_pMemAllocSingleton, bytes, alignment);
        *dataSlot = newBuf;
    }

    int copyStart = (oldData == *dataSlot) ? oldCount : 0;
    auto* dst = reinterpret_cast<uint8_t*>(*dataSlot);

    for (int slot = copyStart; slot < newCount; ++slot) {
        size_t off = static_cast<size_t>(slot) * stride;
        uint8_t* dstElem = dst + off;
        if (!dstElem) continue;
        if (slot < oldCount) {
            const uint8_t* srcElem = reinterpret_cast<const uint8_t*>(oldData) + off;
            // memcpy by stride
            for (size_t i = 0; i < stride; ++i) {
                dstElem[i] = srcElem[i];
            }
        } else {
            // Zero EVERY new slot (slot >= oldCount) on BOTH the realloc and
            // the in-place-grow path — matching the original exe resize
            // functions (e.g. FUN_140001bb0), which zero unconditionally.
            // The engine's boundary-tag allocator (Runtime 0x1800c6e60) hands
            // out DIRTY memory, and on an in-place grow the new slots are NOT
            // freshly allocated — they hold STALE bytes from whatever last
            // lived in that capacity. For the Struct60B element arrays
            // (stride 0x20 / 0x14) each slot embeds sub-array headers
            // {data*, count, cap}; a stale data pointer there is treated as a
            // live buffer by the subsequent Struct60B_Serialize resize and
            // gets freed/realloced → free-list corruption (same UAF class as
            // the stride-8 fix, one level deeper). Zeroing is faithful and
            // harmless for plain data arrays (they get fully overwritten).
            for (size_t i = 0; i < stride; ++i) {
                dstElem[i] = 0;
            }
        }
    }

    if (oldData != *dataSlot && oldData != nullptr) {
        if (fn_MemFreeStatic) fn_MemFreeStatic(oldData);
    }
}

} // namespace

// ============================================================
// Helpers #3-#5 — Array resize variants
// ============================================================
extern "C" void Reimpl_Array_Resize_Stride20(void* arrayHeader, int newCount) {
#ifndef STANDALONE_EXE
    if (fn_Array_Resize_Stride20) {
        fn_Array_Resize_Stride20(arrayHeader, newCount);
        return;
    }
#endif
    GenericArrayResize(arrayHeader, newCount, 0x20, 4);
}

// NOTE (heap-corruption fix): the stride-0x08 array holds raw 8-byte POINTERS
// (e.g. the Geometry +0x210 External*/MaterialInstance ref array). The original
// FUN_140001bb0 zeroes EVERY new slot (slot >= oldCount) on BOTH the in-place
// grow and realloc paths — see Ghidra decompile, the `else if (puVar1) *puVar1=0`
// branch runs unconditionally. This is essential because
// System::MemoryAllocator::Allocate (Runtime 0x1800c6e60) returns DIRTY memory
// (boundary-tag allocator, no zero-fill); a non-zeroed new pointer slot would
// retain a STALE pointer from a previously-freed block. A later serialize pass
// (sub_0150 / ArraySerializeExternalGeneric) then treats that stale pointer as a
// live External* and runs External::GetPath / Filename refcount ops on the
// already-freed System::String — incrementing the refcount slot at (strPtr-4),
// which is exactly (freedBlock+4), i.e. the high dword of the allocator's
// free-list link → the deterministic +0x100000000 bit-32 flip → wild allocator
// write → menu heap crash.
//
// The shared GenericArrayResize only zeroes new slots on the realloc path
// (`else if oldData != *dataSlot`), leaving in-place-grow new slots dirty — the
// divergence from the original. We therefore implement stride-8 with its own
// faithful body that zeroes new slots unconditionally.
extern "C" void Reimpl_Array_Resize_Stride08(void* arrayHeader, int newCount) {
#ifndef STANDALONE_EXE
    if (fn_Array_Resize_Stride08) {
        fn_Array_Resize_Stride08(arrayHeader, newCount);
        return;
    }
#endif
    if (!arrayHeader) return;
    if (!fn_MemAlloc || !g_pMemAllocSingleton) {
        static bool s_warned = false;
        if (!s_warned) {
            printf("[Array::Resize<8>] MemAlloc unavailable — leaving array unchanged "
                   "(header=%p newCount=%d)\n", arrayHeader, newCount);
            fflush(stdout);
            s_warned = true;
        }
        return;
    }

    auto* hdr = reinterpret_cast<uint8_t*>(arrayHeader);
    void** dataSlot = reinterpret_cast<void**>(hdr + 0x00);
    int32_t* countSlot = reinterpret_cast<int32_t*>(hdr + 0x08);
    int32_t* capSlot = reinterpret_cast<int32_t*>(hdr + 0x0C);

    int oldCount = *countSlot;
    if (newCount == oldCount) return;

    void* oldData = *dataSlot;
    *countSlot = newCount;

    if (*capSlot < newCount) {
        *capSlot = newCount;
        if (newCount == oldCount + 1 && oldCount != 0) {
            *capSlot = newCount * 3;
        }
        size_t bytes = static_cast<size_t>(*capSlot) * 0x08;
        *dataSlot = fn_MemAlloc(g_pMemAllocSingleton, bytes, 8);
    }

    int copyStart = (oldData == *dataSlot) ? oldCount : 0;
    auto* dst = reinterpret_cast<uint64_t*>(*dataSlot);
    if (dst) {
        for (int slot = copyStart; slot < newCount; ++slot) {
            if (slot < oldCount) {
                // Copy live pointer from old buffer.
                dst[slot] = reinterpret_cast<const uint64_t*>(oldData)[slot];
            } else {
                // Zero new slot (matches FUN_140001bb0 — prevents stale-pointer
                // use-after-free; the allocator does NOT zero-fill).
                dst[slot] = 0;
            }
        }
    }

    if (oldData != *dataSlot && oldData != nullptr) {
        if (fn_MemFreeStatic) fn_MemFreeStatic(oldData);
    }
}

extern "C" void Reimpl_Array_Resize_Stride14(void* arrayHeader, int newCount) {
#ifndef STANDALONE_EXE
    if (fn_Array_Resize_Stride14) {
        fn_Array_Resize_Stride14(arrayHeader, newCount);
        return;
    }
#endif
    GenericArrayResize(arrayHeader, newCount, 0x14, 4);
}

// ============================================================
// Helper #6 — Array_Serialize stride 0x20 (FUN_14001bbf0)
// ============================================================
//
// Per element (0x20 bytes = 7 floats + 2 shorts):
//   +0x00, +0x04, +0x08, +0x0c, +0x10, +0x14, +0x18 — 7 floats
//   +0x1c, +0x1e — 2 shorts
//
// In the decompile pfVar1 is offset to (base+8) so the relative writes are
// pfVar1[-2..+4] = 7 floats and pfVar1[+5] / +0x16 = the two shorts.
extern "C" void Reimpl_Array_Serialize_Stride20(void* serializer,
                                                uint32_t count,
                                                void* arrayBase) {
    if (!serializer || !arrayBase || count == 0) return;

#ifndef STANDALONE_EXE
    if (fn_Array_Serialize_Stride20) {
        fn_Array_Serialize_Stride20(serializer, count, arrayBase);
        return;
    }
#endif
    if (!g_fn_Serialize_Bool /* unused, just confirms serialize plumbing */) {
        // Soft-fail: we have no Serializer_Serialize_* primitives wired.
        // Returning early at least prevents memory corruption.
        static bool s_warned = false;
        if (!s_warned) {
            printf("[Array::Serialize_S20] no float-serialize primitive — skipping %u elems\n",
                   count);
            fflush(stdout);
            s_warned = true;
        }
        return;
    }

    auto* base = reinterpret_cast<uint8_t*>(arrayBase);
    for (uint32_t i = 0; i < count; ++i) {
        uint8_t* elem = base + static_cast<size_t>(i) * 0x20;
        // Use Serialize_Uint with a float* cast — both serialize 4-byte fields
        // through the same encoder-vtable slot for raw types. Original passes
        // them as floats, but the wire format is identical for (uint32, float).
        for (int off = 0; off < 7; ++off) {
            if (fn_Serializer_Serialize_Uint) {
                fn_Serializer_Serialize_Uint(serializer,
                    reinterpret_cast<uint32_t*>(elem + off * 4),
                    nullptr);
            }
        }
        // Two shorts at +0x1c and +0x1e — Serializer has a short overload
        // (slot vtable+0x80 in the original). For lack of a dedicated wired
        // primitive, treat them as a single uint32 for now. This is a TODO
        // if tests reveal endianness issues.
        if (fn_Serializer_Serialize_Uint) {
            fn_Serializer_Serialize_Uint(serializer,
                reinterpret_cast<uint32_t*>(elem + 0x1c),
                nullptr);
        }
    }
}

// ============================================================
// Generic shared body for ExternalPtr-array serialize (#7 / #8)
// ============================================================
//
// Identical to the single-ref helper (in serialize_externals.cpp) but loops
// `count` times, treating param_3 as an array of External* at stride 8.
//
// Cascade safety: when typeIdPtr is null OR the underlying typeId is 0 (Runtime
// hasn't registered the type yet), we skip the FindOrLoad call and the
// IsDerivedFrom validation. This prevents NullFactory crashes during early
// load and matches the "graceful no-op" pattern from
// feedback_factory_cascade_strategy.md.
namespace {

void ArraySerializeExternalGeneric(void* serializer, int count, void* arrayBase,
                                   const uint32_t* typeIdPtr,
                                   const char* typeName) {
    if (!serializer || count <= 0 || !arrayBase) return;

    // Required Runtime primitives. If any are missing, log once and bail.
    if (!fn_Serializer_GetEncoding || !fn_ExternalFactory_FindOrLoad ||
        !fn_IsDerivedFrom || !fn_ExternalFactory_IsReferenceModeEnabled ||
        !fn_External_GetPath || !fn_Filename_Ctor || !fn_Filename_Dtor ||
        !fn_Filename_AssignOp || !g_fn_String_Serialize ||
        !fn_Serializer_Serialize_Uint || !g_fn_Serialize_Bool ||
        !g_pExternalFactorySingleton || !*g_pExternalFactorySingleton) {
        static bool s_warned = false;
        if (!s_warned) {
            printf("[Array::Serialize_External<%s>] runtime primitives unwired — "
                   "skipping %d elements (cascade-safe no-op)\n",
                   typeName ? typeName : "?", count);
            fflush(stdout);
            s_warned = true;
        }
        return;
    }

    void* factory = *g_pExternalFactorySingleton;

    // Resolve typeId — null/zero means "type not yet registered". Skip
    // IsDerivedFrom validation in that case.
    uint32_t typeId = 0;
    bool typeIdResolved = false;
    if (typeIdPtr) {
        typeId = *typeIdPtr;
        typeIdResolved = (typeId != 0);
    }
    // Lazy resolution path — see serialize_externals.cpp for rationale.
    if (!typeIdResolved && typeName) {
        const char* simple = typeName;
        for (const char* p = typeName; *p; ++p) {
            if (p[0] == ':' && p[1] == ':') simple = p + 2;
        }
        typeId = ::Object::ResolveTypeIdByNameLazy(simple);
        typeIdResolved = (typeId != 0);
    }
    if (!typeIdResolved) {
        static bool s_typeWarned = false;
        if (!s_typeWarned) {
            printf("[Array::Serialize_External<%s>] typeId not yet registered — "
                   "loading without type validation (count=%d)\n",
                   typeName ? typeName : "?", count);
            fflush(stdout);
            s_typeWarned = true;
        }
    }

    // Encoding state. Slot vtable+0x10 returns 3 for "saving in
    // export-by-path" mode where we walk existing pointers through GetPath +
    // FindOrLoad without serializing anything.
    void* encoding = fn_Serializer_GetEncoding(serializer);
    if (!encoding) return;
    using EncodingStateFn = int (*)(void*);
    EncodingStateFn getStateFn = nullptr;
    {
        void* vtable = *reinterpret_cast<void**>(encoding);
        getStateFn = reinterpret_cast<EncodingStateFn>(
            *reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(vtable) + 0x10));
    }
    int encState = getStateFn ? getStateFn(encoding) : 0;

    // Each element of arrayBase is an External* slot (8 bytes).
    auto* slots = reinterpret_cast<void**>(arrayBase);
    for (int i = 0; i < count; ++i) {
        void** slot = &slots[i];

        if (encState == 3) {
            // Path-export mode: only walk existing pointers, never write.
            if (!*slot) continue;
            uint8_t pathBuf[8] = {0};
            void* pathFn = *slot;
            void* path = fn_External_GetPath(pathFn, pathBuf);
            void* found = fn_ExternalFactory_FindOrLoad(factory, path);
            if (found && typeIdResolved) {
                ::Object::IsDerivedFromOrTrust(found, typeId);
            }
            // Filename returned by GetPath is a stack-local Filename; it owns
            // a System::String reference that we must release via dtor.
            // Original calls Filename::~Filename on local_70 here.
            fn_Filename_Dtor(pathBuf);
            continue;
        }

        // Load/save mode. Build a temporary Filename used to round-trip the
        // path through the serializer.
        uint8_t fnameBuf[8] = {0};
        fn_Filename_Ctor(fnameBuf);

        bool refMode = fn_ExternalFactory_IsReferenceModeEnabled(factory);

        if (refMode) {
            // Reference-mode: serialize a uint reference id.
            uint32_t refId = 0xFFFFFFFFu;
            if (*slot) {
                refId = fn_ExternalFactory_GetReferenceId(factory, *slot);
            }

            // Branch decision matches original's:
            //   if (slot==NULL || *(int*)(*serializer + 8) != 0 || refId != UINT_MAX)
            //       Serialize(refId)
            //       if (refId != UINT_MAX) {
            //           if loading: GetPathForReferenceId + Filename::operator=
            //           goto loadPathBranch
            //       }
            //   else Serialize(refId)  // also UINT_MAX
            int serializerInner = *reinterpret_cast<int*>(
                reinterpret_cast<uint8_t*>(*reinterpret_cast<void**>(serializer)) + 8);

            bool needsPathSerialize = false;
            if (!*slot || serializerInner != 0 || refId != 0xFFFFFFFFu) {
                fn_Serializer_Serialize_Uint(serializer, &refId, nullptr);
                if (refId != 0xFFFFFFFFu) {
                    if (serializerInner == 0) {
                        // Loading path: ask the factory for the actual path
                        // and assign into our temp Filename.
                        uint8_t tmpFn[8] = {0};
                        // Member function with hidden retval: RCX=this, RDX=retbuf, R8=refId.
                        fn_ExternalFactory_GetPathForReferenceId(factory, tmpFn, refId);
                        fn_Filename_AssignOp(fnameBuf, tmpFn);
                        fn_Filename_Dtor(tmpFn);
                    }
                    needsPathSerialize = true;
                }
            } else {
                fn_Serializer_Serialize_Uint(serializer, &refId, nullptr);
            }

            if (needsPathSerialize) {
                int innerCheck = *reinterpret_cast<int*>(
                    reinterpret_cast<uint8_t*>(*reinterpret_cast<void**>(serializer)) + 8);
                if (innerCheck == 0) {
                    void* loaded = fn_ExternalFactory_FindOrLoad(factory, fnameBuf);
                    void* result = nullptr;
                    if (loaded && (!typeIdResolved || ::Object::IsDerivedFromOrTrust(loaded, typeId))) {
                        result = loaded;
                    }
                    *slot = result;
                }
            }
        } else {
            // Plain mode: serialize a bool flag, then a string path if true.
            bool hasRef = (*slot != nullptr);
            g_fn_Serialize_Bool(serializer, &hasRef, nullptr);
            if (hasRef) {
                if (*slot) {
                    uint8_t pathBuf[8] = {0};
                    void* path = fn_External_GetPath(*slot, pathBuf);
                    fn_Filename_AssignOp(fnameBuf, path);
                    fn_Filename_Dtor(pathBuf);
                }
                g_fn_String_Serialize(fnameBuf, serializer);

                int innerCheck = *reinterpret_cast<int*>(
                    reinterpret_cast<uint8_t*>(*reinterpret_cast<void**>(serializer)) + 8);
                if (innerCheck == 0) {
                    void* loaded = fn_ExternalFactory_FindOrLoad(factory, fnameBuf);
                    void* result = nullptr;
                    if (loaded && (!typeIdResolved || ::Object::IsDerivedFromOrTrust(loaded, typeId))) {
                        result = loaded;
                    }
                    *slot = result;
                }
            }
        }

        fn_Filename_Dtor(fnameBuf);
    }
}

} // namespace

// ============================================================
// Helpers #7 / #8 — Array_Serialize_External (CASCADE)
// ============================================================
//
// #7 GraphicsMesh exref array (typeInfo @ slot 0x140151f20)
extern "C" void Reimpl_Array_Serialize_External_GraphicsMesh(
    void* serializer, int count, void* arrayBase) {

#ifndef STANDALONE_EXE
    if (fn_Array_Serialize_External_GraphicsMesh) {
        fn_Array_Serialize_External_GraphicsMesh(serializer, count, arrayBase);
        return;
    }
#endif

    const uint32_t* typeIdPtr = nullptr;
    if (g_ppTypeInfo_GraphicsMesh && *g_ppTypeInfo_GraphicsMesh) {
        typeIdPtr = *g_ppTypeInfo_GraphicsMesh;  // BaseTypeInfo*[0] == typeId
    }
    ArraySerializeExternalGeneric(serializer, count, arrayBase,
                                  typeIdPtr, "Graphics::GraphicsMesh");
}

// #8 Particle exref array (typeInfo @ slot 0x140151420)
extern "C" void Reimpl_Array_Serialize_External_Particle(
    void* serializer, int count, void* arrayBase) {

#ifndef STANDALONE_EXE
    if (fn_Array_Serialize_External_Particle) {
        fn_Array_Serialize_External_Particle(serializer, count, arrayBase);
        return;
    }
#endif

    const uint32_t* typeIdPtr = nullptr;
    if (g_ppTypeInfo_Particle && *g_ppTypeInfo_Particle) {
        typeIdPtr = *g_ppTypeInfo_Particle;
    }
    ArraySerializeExternalGeneric(serializer, count, arrayBase,
                                  typeIdPtr, "Graphics::Particle");
}

// ============================================================
// STANDALONE_EXE: address registry hooks
// ============================================================
//
// In standalone we do NOT have the original exe's code mapped. The
// fn_Geometry_Init / fn_Array_Resize_* / etc. function pointers above stay
// null when ResolveAddr can't find them, and the Reimpl_* fallbacks above
// kick in. To wire the registry up, see standalone/address_registry.cpp.
