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
// Dispatch pattern: try the original exe address first (DLL mode + standalone
// where 0x1400b04d0 / 0x1400af1a0 may be RegisterExeAddr-able). If null, fall
// through to the cleaned C body. Both functions self-resolve each Runtime
// dispatch slot lazily on first use.

#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cstdio>

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

typedef void  (*FnSerInt_t       )(void* serializer, int32_t* field, const char* name);
typedef void  (*FnSerByte_t      )(void* serializer, uint8_t* field, const char* name);
typedef void  (*FnSerBool_t      )(void* serializer, bool*    field, const char* name);
typedef void  (*FnSerFloat_t     )(void* serializer, float*   field, const char* name);
typedef int   (*FnGetVer_t       )(void* serializer);
typedef void  (*FnSerUShortArr_t )(void* serializer, int32_t count, uint16_t* data, const char* name);
typedef void  (*FnSerSizedRaw_t  )(void* serializer, int32_t bytes, void* ptr, const char* name);
typedef void  (*FnSerByteArr_t   )(void* serializer, int32_t count, void* ptr, const char* name);
typedef void  (*FnSerCtor_t      )(void* self, void* stream, int version);
typedef void  (*FnSerDtor_t      )(void* self);
typedef void  (*FnVector2Ser_t   )(void* self, void* serializer);
typedef bool  (*FnVertexCreate_t )(void* self, int sizeBytes, bool dyn, void* initialData);
typedef bool  (*FnIndexCreate_t  )(void* self, int sizeBytes, bool dyn, void* initialData);

// Slot map (verified via Ghidra MCP `disassemble_function 0x1400b04d0 CALL`):
constexpr uintptr_t kSlot_SerCtor          = 0x140152ad8;  // ??0Serializer@IO
constexpr uintptr_t kSlot_SerDtor          = 0x140152ad0;  // ??1Serializer@IO
constexpr uintptr_t kSlot_SerInt           = 0x140152ac8;
constexpr uintptr_t kSlot_SerByte          = 0x140152410;
constexpr uintptr_t kSlot_SerBool          = 0x140152a58;
constexpr uintptr_t kSlot_SerFloat         = 0x140152a60;
constexpr uintptr_t kSlot_GetVer           = 0x140152a48;
constexpr uintptr_t kSlot_SerUShortArr     = 0x140152488;  // bulk uint16 array
constexpr uintptr_t kSlot_SerSizedRaw_3F8  = 0x1401523f8;  // Serialize(int,short*,name)
constexpr uintptr_t kSlot_SerSizedRaw_400  = 0x140152400;  // Serialize(int,void*,name) variant
constexpr uintptr_t kSlot_SerByteArr_618   = 0x140152618;  // SerializeBytes(int,void*,name)
constexpr uintptr_t kSlot_Vector2Ser       = 0x1401526b8;  // Math::Vector2::Serialize
constexpr uintptr_t kSlot_VertexBufCreate  = 0x140152668;  // Video::VertexBuffer::Create
constexpr uintptr_t kSlot_IndexBufCreate   = 0x140152660;  // Video::IndexBuffer::Create

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

// Helper: read the inner `serializerInner` flag at *(int*)(*serializer + 8).
// Original disasm checks this to decide load (0) vs save (1) path.
inline int SerializerInner(void* serializer) {
    if (!serializer) return -1;
    void* vt = *reinterpret_cast<void**>(serializer);
    if (!vt) return -1;
    return *reinterpret_cast<int*>(reinterpret_cast<uint8_t*>(vt) + 8);
}

// Hard-pinned dispatch via shared globals — when the slot itself isn't
// available, fall back to the global Serializer ctor/dtor pointers from
// engine_funcs.cpp (resolved via ResolveExport).
void CallSerializerCtor(void* localSer, void* stream, int version) {
    static FnSerCtor_t cached = nullptr;
    static bool tried = false;
    FnSerCtor_t fn = ResolveSlot(kSlot_SerCtor, cached, tried, "Serializer::ctor");
    if (fn) { fn(localSer, stream, version); return; }
    if (g_fn_Serializer_Ctor) { g_fn_Serializer_Ctor(localSer, stream, version); return; }
    static bool s_warned = false;
    if (!s_warned) {
        printf("[Geometry::Serialize] no Serializer::ctor — local serializer "
               "stays uninitialized (downstream calls may misbehave)\n");
        fflush(stdout);
        s_warned = true;
    }
}

void CallSerializerDtor(void* localSer) {
    static FnSerDtor_t cached = nullptr;
    static bool tried = false;
    FnSerDtor_t fn = ResolveSlot(kSlot_SerDtor, cached, tried, "Serializer::dtor");
    if (fn) { fn(localSer); return; }
    if (g_fn_Serializer_Dtor) { g_fn_Serializer_Dtor(localSer); return; }
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

    // Resolve all dispatch slots up front — cached statics, one-shot warnings.
    static FnSerInt_t        s_serInt = nullptr;        static bool t_serInt = false;
    static FnSerBool_t       s_serBool = nullptr;       static bool t_serBool = false;
    static FnSerFloat_t      s_serFloat = nullptr;      static bool t_serFloat = false;
    static FnGetVer_t        s_getVer = nullptr;        static bool t_getVer = false;
    static FnSerUShortArr_t  s_serUSArr = nullptr;      static bool t_serUSArr = false;
    static FnSerByteArr_t    s_serByteArr = nullptr;    static bool t_serByteArr = false;
    static FnVector2Ser_t    s_vec2Ser = nullptr;       static bool t_vec2Ser = false;
    static FnVertexCreate_t  s_vbCreate = nullptr;      static bool t_vbCreate = false;
    static FnIndexCreate_t   s_ibCreate = nullptr;      static bool t_ibCreate = false;

    FnSerInt_t       serInt    = ResolveSlot(kSlot_SerInt,           s_serInt,    t_serInt,    "Serialize_Int");
    FnSerBool_t      serBool   = ResolveSlot(kSlot_SerBool,          s_serBool,   t_serBool,   "Serialize_Bool");
    FnSerFloat_t     serFloat  = ResolveSlot(kSlot_SerFloat,         s_serFloat,  t_serFloat,  "Serialize_Float");
    FnGetVer_t       getVer    = ResolveSlot(kSlot_GetVer,           s_getVer,    t_getVer,    "GetVersion");
    FnSerUShortArr_t serUSArr  = ResolveSlot(kSlot_SerUShortArr,     s_serUSArr,  t_serUSArr,  "Serialize_UShortArr");
    FnSerByteArr_t   serByteArr= ResolveSlot(kSlot_SerByteArr_618,   s_serByteArr,t_serByteArr,"SerializeBytes_618");
    FnVector2Ser_t   vec2Ser   = ResolveSlot(kSlot_Vector2Ser,       s_vec2Ser,   t_vec2Ser,   "Vector2::Serialize");
    FnVertexCreate_t vbCreate  = ResolveSlot(kSlot_VertexBufCreate,  s_vbCreate,  t_vbCreate,  "VertexBuffer::Create");
    FnIndexCreate_t  ibCreate  = ResolveSlot(kSlot_IndexBufCreate,   s_ibCreate,  t_ibCreate,  "IndexBuffer::Create");

    auto serIntField = [&](void* field) {
        if (serInt) serInt(serializer, reinterpret_cast<int32_t*>(field), nullptr);
    };

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

    // ---- Step 3: Serialize uint16 array at +0xD8 (count=+0xE0). ----
    int32_t local_count = *reinterpret_cast<int32_t*>(selfBytes + 0xE0);
    serIntField(&local_count);
    if (SerializerInner(serializer) == 0) {
        // Loading: resize the uint16 array (stride 0x02).
        Reimpl_Array_Resize_Stride02(selfBytes + 0xD8, local_count >> 1);
        // NOTE: original passes (count >> 1) — uint16 stride means
        // local_count is a byte count. Verified at 0x1400b06c2.
    }
    if (serUSArr) {
        uint16_t* arr16 = *reinterpret_cast<uint16_t**>(selfBytes + 0xD8);
        // Original at 0x1400b05ce uses slot 0x1401523f8 (raw bytes) — but
        // the count semantics there are byte-count, not element-count. Use the
        // sized-raw slot instead.
        // Fall through; we mimic with serUSArr only if it's the right shape.
        // The original disasm:
        //   IO::Serializer::Serialize(serializer, count, ushort*, name);
        // is the bulk uint16 form (slot 0x140152488).
        if (arr16) serUSArr(serializer, local_count, arr16, nullptr);
    }

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

    // The original checks `*(int*)(stream + 8) == 0` (stream's load flag),
    // not the local serializer's. The stream itself carries this state.
    int streamInner = -1;
    if (stream) {
        void* svt = *reinterpret_cast<void**>(stream);
        if (svt) streamInner = *reinterpret_cast<int*>(reinterpret_cast<uint8_t*>(svt) + 8);
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
    // Serialize raw heightmap bytes.
    if (serByteArr && heightBuf && byteCount > 0) {
        serByteArr(serializer, byteCount, heightBuf, nullptr);
    } else if (fn_Serializer_SerializeBulk && heightBuf && byteCount > 0) {
        // Fallback to the SerializeBulk variant from engine_funcs.
        fn_Serializer_SerializeBulk(serializer, byteCount, heightBuf, nullptr);
    }

    // ---- Step 6: Version-gated mesh-vert/idx buffer block. ----
    int version = getVer ? getVer(serializer) : 6;
    if (version < 3) {
        // OLD format: bool + uint count + raw bytes for both vert + idx buffers.
        bool flag = false;
        int32_t cnt = 0;
        if (serBool) serBool(serializer, &flag, nullptr);
        serIntField(&cnt);
        // Mesh-vertex resize (16-byte stride): cnt >> 4.
        Reimpl_Array_Resize_Stride16B(selfBytes + 0x1A0, cnt >> 4);
        if (serByteArr) {
            void* vertData = *reinterpret_cast<void**>(selfBytes + 0x1A0);
            if (vertData) serByteArr(serializer, cnt, vertData, nullptr);
        }
        if (serBool) serBool(serializer, &flag, nullptr);
        serIntField(&cnt);
        // Index buffer resize (uint16 stride): cnt >> 1.
        Reimpl_Array_Resize_Stride02(selfBytes + 0x1B0, cnt >> 1);
        if (serByteArr) {
            void* idxData = *reinterpret_cast<void**>(selfBytes + 0x1B0);
            if (idxData) serByteArr(serializer, cnt, idxData, nullptr);
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
            static FnSerSizedRaw_t s_rawA = nullptr; static bool t_rawA = false;
            static FnSerSizedRaw_t s_rawB = nullptr; static bool t_rawB = false;
            FnSerSizedRaw_t rawA = ResolveSlot(kSlot_SerSizedRaw_3F8, s_rawA, t_rawA, "Serialize(int,short*)_3f8");
            FnSerSizedRaw_t rawB = ResolveSlot(kSlot_SerSizedRaw_400, s_rawB, t_rawB, "Serialize(int,void*)_400");
            uint8_t* vbase = *reinterpret_cast<uint8_t**>(selfBytes + 0x1A0);
            if (vbase) {
                uint8_t* p = vbase + 0xC;
                for (int i = 0; i < vcount; ++i) {
                    if (rawA) {
                        rawA(serializer, 4, p - 0xC, nullptr);
                        rawA(serializer, 2, p - 4, nullptr);
                    }
                    if (rawB) {
                        rawB(serializer, 4, p, nullptr);
                    }
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
        if (serUSArr) {
            uint16_t* iarr = *reinterpret_cast<uint16_t**>(selfBytes + 0x1B0);
            if (iarr) serUSArr(serializer, icount, iarr, nullptr);
        }
    }

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

    // ---- Step 11: Version 4+ array at +0x220 (count +0x228), stride 0x04. ----
    if (version >= 4) {
        int32_t c = *reinterpret_cast<int32_t*>(selfBytes + 0x228);
        serIntField(&c);
        if (SerializerInner(serializer) == 0) {
            Reimpl_Array_Resize_Stride04(selfBytes + 0x220, c);
        }
        if (serUSArr) {
            // Note: original at 0x1400b0a3f calls FUN_1400021d0 (resize),
            // then does Serialize(serializer, count, int*, name) via slot
            // 0x140152488 (kSlot_SerUShortArr per disassembly). The slot at
            // 0x140152488 is the bulk uint16 array — but for ints the same
            // slot apparently works (serializing 4-byte chunks). The original
            // call shape passes the slot 4-byte ints despite the ushort
            // typing; we mirror.
            int32_t* iarr = *reinterpret_cast<int32_t**>(selfBytes + 0x220);
            if (iarr) {
                serUSArr(serializer, c, reinterpret_cast<uint16_t*>(iarr), nullptr);
            }
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
        if (ibCreate) {
            ibCreate(selfBytes + 0x180, idxCount * 2, false, idxData);
        }
        if (vbCreate) {
            vbCreate(selfBytes + 0x160, vertCount * 0x10, false, vertData);
        }
        // Image-buffer heightmap upload (CASCADE: requires Runtime exports).
        Reimpl_TradeWork_sub_c780(self);

        int32_t c218 = *reinterpret_cast<int32_t*>(selfBytes + 0x218);
        int32_t c228 = *reinterpret_cast<int32_t*>(selfBytes + 0x228);
        if (c218 != c228 && fn_Array_Resize_Stride08) {
            fn_Array_Resize_Stride08(selfBytes + 0x210, c228);
        }
    }

    // ---- Step 15: Local Serializer dtor. ----
    CallSerializerDtor(serializer);
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

    static FnSerInt_t s_serInt = nullptr;        static bool t_serInt = false;
    static FnGetVer_t s_getVer = nullptr;        static bool t_getVer = false;
    FnSerInt_t serInt = ResolveSlot(kSlot_SerInt, s_serInt, t_serInt, "Serialize_Int");
    FnGetVer_t getVer = ResolveSlot(kSlot_GetVer, s_getVer, t_getVer, "GetVersion");

    auto serIntField = [&](void* field) {
        if (serInt) serInt(serializer, reinterpret_cast<int32_t*>(field), nullptr);
    };

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
    int version = getVer ? getVer(serializer) : 4;
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

    // ---- Step 6: Local Serializer dtor. ----
    CallSerializerDtor(serializer);
}

} // anonymous namespace

// ============================================================
// Public reimpl entry points
// ============================================================
//
// Each entry tries the original exe address first (DLL mode + standalone
// when the exe code page is mapped). If that pointer is null (standalone
// mode, no exe code), we fall through to our cleaned C body.

extern "C" void Reimpl_TradeWork_impl_04d0(void* self, void* stream) {
    if (fn_TradeWork_impl_04d0) {
        fn_TradeWork_impl_04d0(self, stream);
        return;
    }
    TradeWork_impl_04d0_Body(self, stream);
}

extern "C" void Reimpl_TradeWork_impl_f1a0(void* self, void* stream) {
    if (fn_TradeWork_impl_f1a0) {
        fn_TradeWork_impl_f1a0(self, stream);
        return;
    }
    TradeWork_impl_f1a0_Body(self, stream);
}
