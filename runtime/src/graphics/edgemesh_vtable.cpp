// ============================================================================
// runtime/src/graphics/edgemesh_vtable.cpp — WAVE 27 lane GFXMESH
//
// THE EIGHT NON-SLOT-0 BODIES OF `??_7EdgeMesh@Graphics@@6B@` (DLL 0x180154c70).
//
// SUBJECT AND DENOMINATOR, STATED FIRST. The Wave 27 census
// (tools/runtime_census.py, re-run on wave27/base, PROMOTION BACKLOG) prints
//
//     ??_7EdgeMesh@Graphics@@6B@            0     9
//
// — nine non-purecall slots, NOT ONE of them ported anywhere in the tree. That
// zero was re-checked before a line was written, by mangled name and by
// address, across runtime/ code/ standalone/ engine/ dx11/
// (reference_grep_for_the_port_before_writing_it.md); it returned nothing.
//
// This file is the first body for slots 1..8. SLOT 0 IS NOT HERE: it is the
// unnamed vector deleting destructor at 0x1800134b0, which belongs to the
// 37-row family in runtime/src/object/deleting_dtor.cpp and is added THERE as a
// table row, because that file already holds the one correct body and a 45th
// hand-written copy is exactly what its banner exists to prevent.
//
// With this file plus that row, the table reaches 9/9 — COMPLETE. COMPLETE is
// the STATIC half of readiness ONLY (reference_a_green_readiness_screen_is_not
// _a_route.md); the promotion verdict is in WAVE27_LANE_GFXMESH.md §5 and this
// file does not promote anything.
//
// EVERY BODY BELOW WAS READ FROM THE DISASSEMBLY of the shipped RETAIL
// Runtime-x64.dll, never from Ghidra's decompiled C, which drops dereferences,
// prints MOVSXD as a zero extension and drops whole arguments at a mistyped
// vtable call (reference_ghidra_c_output_vs_disassembly.md).
//
// EVERY ADDRESS WAS CONFIRMED AGAINST THE EXPORT DIRECTORY BY VA, with pefile
// against `C:\GOG Games\Banished\Runtime-x64.dll` — not by name pattern, and
// not against the profile build, whose vtable offsets are known to differ
// (reference_encoding_vtable_offsets_differ_between_builds.md):
//
//     0x180012c90  4382  ?TypeName@EdgeMesh@Graphics@@UEBAPEBDXZ
//     0x180012c70  4175  ?Type@EdgeMesh@Graphics@@UEBAIXZ
//     0x1800a23d0  2622  ?GetFileVersion@EdgeMesh@Graphics@@UEBAHXZ
//     0x1800a21c0  1741  ?Compile@EdgeMesh@Graphics@@UEAA?AW4CompileResult@...
//     0x1800a21d0  3685  ?Serialize@EdgeMesh@Graphics@@UEAA_NAEAVStream@IO@@@Z
//     0x180012e10  3477  ?Reconstruct@EdgeMesh@Graphics@@UEAAPEAVExternal@Object@@XZ
//     0x180012e40  2172  ?Destruct@EdgeMesh@Graphics@@UEAAXXZ
//     0x180012e30  1817  ?Construct@EdgeMesh@Graphics@@UEAAPEAVExternal@Object@@XZ
//
// All eight are `UEBA`/`UEAA` — PUBLIC virtual. Nothing here needs an address
// door on its own account; the address doors that ARE opened are for the two
// unexported serialize helpers, and each is commented in the §4 #10 shape at
// its call site.
//
// KILL SWITCH: BANISHED_RT_GFXEDGEVT=0 leaves all eight on the DLL.
//
// ⚠ CALLS=0 IS THE EXPECTED READING, AND IT IS NOT VERIFICATION.
//   mesh.cpp's closing note already says it for the whole EdgeMesh family: the
//   selection-outline chain (standalone/address_registry.cpp:584,
//   RegisterPlayerRenderBatch) is the only consumer of EdgeMesh and it is gated
//   default-OFF on BANISHED_SELECTION_OUTLINE. A run that does not set that
//   variable exercises none of these eight, and a port with zero callers is not
//   verified no matter how clean the diff summary looks
//   (reference_uncalled_port_is_not_verified.md). Read the `calls=` column.
// ============================================================================

#include <cstddef>
#include <cstdint>
#include <cstring>

#include "../../funcs/rt_port.h"

// Same local declaration mesh.cpp, array.cpp and deleting_dtor.cpp carry, and
// for the same reason: standalone/resolve.h drags in <windows.h>, which
// PLAN.md §4.1 keeps out of runtime/src/. The linker checks the two spellings
// match.
enum class Module : int { Exe = 0, Runtime = 1, VideoDX11 = 2, COUNT = 3 };
uintptr_t ResolveAddr(Module mod, uintptr_t ghidra_addr);

extern "C" const char* RtGraphics_EdgeMesh_TypeNameStr(const void*);
extern "C" uint32_t    RtGraphics_EdgeMesh_Type(const void*);
extern "C" int32_t     RtGraphics_EdgeMesh_GetFileVersion(const void*);
extern "C" int32_t     RtGraphics_EdgeMesh_Compile(void*, const void*);
extern "C" bool        RtGraphics_EdgeMesh_Serialize(void*, void*);
extern "C" void*       RtGraphics_EdgeMesh_Reconstruct(void*);
extern "C" void        RtGraphics_EdgeMesh_Destruct(void*);
extern "C" void*       RtGraphics_EdgeMesh_Construct(void*);

namespace {

// ---------------------------------------------------------------------------
// SLOTS 2 and 1 — the BaseTypeInfo node readers.
//
//   0x180012c70  8b 05 ..   MOV EAX,[0x1801f99b0]   RET   -> node + 0x00, DWORD
//   0x180012c90  48 8b 05.. MOV RAX,[0x1801f99b8]   RET   -> node + 0x08, QWORD
//
// Neither instruction has an RCX operand: `this` is not read by either body.
// That is not an inference from a decompile, it is what the encodings contain.
//
// 0x1801f99b0 is cross-checked, not merely decoded: it is the VA of the minted
// `?_typeInfo@EdgeMesh@Graphics@@0VBaseTypeInfo@Object@@A` row in
// standalone/rt_data_mint_tables.inc (line 1973) and of the promotion-storage
// registration at standalone/reg_mint_promoted.cpp:931. So this file reads the
// node through RT_DATA_SLOT rather than pinning the DLL's address, which is
// what makes these two FOLLOW a promotion of that `?_typeInfo@` row — exactly
// the arrangement graphics_typeinfo_accessors.cpp established for the
// AnimationEvent / MaterialInstance / Particle nodes, and cutover needs.
//
// `0V…A` is the PRIVATE static spelling and this DLL exports its private
// members, so a NAME door is the correct door here and no address door is
// needed on the node's account (reference_aeaa_is_private_not_unexported.md).
// ---------------------------------------------------------------------------
constexpr size_t kTypeInfoTypeIdOffset = 0x00;
constexpr size_t kTypeInfoNameOffset   = 0x08;
static_assert(kTypeInfoTypeIdOffset == 0, "BaseTypeInfo::typeId is at +0x00");
static_assert(kTypeInfoNameOffset == 8,   "BaseTypeInfo::name is at +0x08");

const uint8_t* EdgeMeshTypeInfo() {
    return RT_DATA_SLOT(const uint8_t*,
                        "?_typeInfo@EdgeMesh@Graphics@@0VBaseTypeInfo@Object@@A");
}

uint32_t ReadTypeId(const uint8_t* node) {
    if (!node) return 0;
    uint32_t v = 0;
    std::memcpy(&v, node + kTypeInfoTypeIdOffset, sizeof v);
    return v;
}
const char* ReadTypeName(const uint8_t* node) {
    if (!node) return nullptr;
    const char* v = nullptr;
    std::memcpy(&v, node + kTypeInfoNameOffset, sizeof v);
    return v;
}

typedef uint32_t    (*U32ThisFn)(const void*);
typedef const char* (*CStrThisFn)(const void*);
typedef int32_t     (*I32ThisFn)(const void*);
typedef int32_t     (*CompileFn)(void*, const void*);

// Vtable slot 0 of Object::Base is the deleting destructor:
//   flag 0 = destroy in place, flag 1 = destroy AND free.
using Slot0Fn = void* (*)(void* self, unsigned flag);

// ??0EdgeMesh@Graphics@@QEAA@XZ (0x1800a2830, ordinal 161). Reached through
// RT_EXPORT_ONCE, NOT by address and NOT as a direct call into mesh.cpp's
// static: ResolveExport consults the port registry FIRST
// (reference_resolveaddr_consults_port_registry_first.md), so this picks up
// mesh.cpp's ported `RtGraphics_EdgeMesh_Ctor` whenever BANISHED_RT_GFXMESH is
// on, and the DLL's otherwise. Retail's Construct/Reconstruct both reach the
// ctor by a plain rel32 call to the same body, so one door for both is faithful.
using CtorFn = void* (*)(void*);
CtorFn EdgeMeshCtor() {
    return RT_EXPORT_ONCE(CtorFn, "??0EdgeMesh@Graphics@@QEAA@XZ");
}

// ---------------------------------------------------------------------------
// THE SERIALIZE CONTEXT, read out of the shipped frame rather than assumed.
//
// ?Serialize@EdgeMesh@ builds a TWO-QWORD local at [RBP-0x18] and passes its
// ADDRESS (`LEA RDX,[RBP-0x18]`) to every helper:
//     [RBP-0x18] = RDX          the incoming IO::Stream& / Serializer&
//     [RBP-0x10] = 0, then the Encoding* the factory hands back
// and every helper dereferences it the same way — 0x180014520 does
// `MOV RDI,RDX ; MOV RAX,[RDX]` (the stream) and later `MOV RCX,[RDI+8]`
// (the encoding) before every virtual call. So the pair is a context struct,
// not two separate arguments, and it is reproduced with the same layout so the
// helpers this file calls by address see exactly the bytes retail gives them.
// ---------------------------------------------------------------------------
struct SerializeCtx {
    void* stream;    // +0x00  the IO::Stream& this slot was handed
    void* encoding;  // +0x08  the Encoding the stream's factory produced
};
static_assert(sizeof(SerializeCtx) == 0x10, "the shipped context is two qwords");
static_assert(offsetof(SerializeCtx, encoding) == 8, "encoding is the second qword");

// The stream's mode discriminator, `[stream+0x8]`, tested three times in this
// body (`CMP dword ptr [RDX+0x8],R8D` / `CMP dword ptr [RAX+0x8],0x0`).
// ZERO selects the READ arm — it is the arm that RESIZES the two arrays before
// streaming them, which only a reader does. Non-zero is the write arm.
constexpr size_t kStreamModeOffset    = 0x08;
constexpr size_t kStreamFactoryOffset = 0x10;

// Encoding vtable byte offsets, READ FROM RETAIL. The profile build's Encoding
// table uses DIFFERENT offsets for the same operations
// (reference_encoding_vtable_offsets_differ_between_builds.md), so these were
// taken from `C:\GOG Games\Banished\Runtime-x64.dll` and from nowhere else. A
// port written from the profile decompile would desync every stream while
// looking correct.
constexpr size_t kEncSerializeDword = 0xa0;   // CALL [RAX+0xa0], one 4-byte value
constexpr size_t kEncSerializeArray = 0x28;   // CALL [RAX+0x28], (count, void*, 0)

// The stream factory's vtable byte offsets — the two that produce an Encoding.
//     CMP [ser+8],0 ; JNZ  -> CALL [RAX+0x20]     (index 4)
//                      else -> CALL [RAX+0x18]     (index 3)
constexpr size_t kFactoryMakeReadEncoding  = 0x18;
constexpr size_t kFactoryMakeWriteEncoding = 0x20;

// Video buffer slot: `MOV RCX,[this+X] ; MOV RAX,[RCX] ; CALL [RAX+0x30]`.
constexpr size_t kVideoBufferSerialize = 0x30;

// ---------------------------------------------------------------------------
// THE FOUR UNEXPORTED HELPERS THIS BODY CALLS, AND WHY BY ADDRESS.
//
// SYMBOL:      none. All four are in-DLL internals with no export-directory
//              entry — checked by VA against the export table with pefile, not
//              inferred from an `AEAA` in a mangled name
//              (reference_aeaa_is_private_not_unexported.md), because these
//              have no mangled name at all.
// CONDITION:   they are reached with ResolveAddr(Module::Runtime, <VA>), which
//              consults the port registry FIRST, so each becomes OUR body the
//              moment any lane registers that address — no change here.
// CONSEQUENCE: until then they are the DLL's, and this file makes four
//              addresses newly `called`
//              (reference_porting_can_grow_the_cutover_surface.md). They are
//              NOT claimed as ported by this lane and they are listed in
//              WAVE27_LANE_GFXMESH.md §6 as inherited cutover work, so the next
//              census does not have to rediscover them.
//
//   0x180014230  SerializeExternalPtrBase(ExternalPtr*, SerializeCtx*)
//                the +0x20 slot. Its type check reads the typeId at
//                0x1801fa708 — `?_typeInfo@GraphicsMesh@Graphics@@`.
//   0x180014520  SerializeExternalPtrMaterial(ExternalPtr*, SerializeCtx*)
//                the four material slots. Identical body except its typeId is
//                0x1801fa778 — `?_typeInfo@MaterialInstance@Graphics@@`. That
//                ONE difference is why retail emits two helpers and why this
//                file must not collapse them into one.
//   0x180014850  SerializeTriangleListArray(SerializeCtx*, int n, void* data)
//                six DWORDs per element at +0,+4,+8,+0xc,+0x10,+0x14, stride
//                0x18, through the encoding's +0xa0 slot. `TEST EDX,EDX ; JLE`
//                — a count of zero does nothing, including no virtual call.
//   0x1800136f0  the TriangleList Array<>::SetSize. ALREADY reached by this
//                tree: mesh.cpp:201 resolves the same address as
//                `RtTriListArrayResize()`. Deliberately resolved the same way
//                here rather than re-derived, so there is ONE place to repoint.
// ---------------------------------------------------------------------------
using ExtPtrSerializeFn = void (*)(void* extPtrSlot, SerializeCtx* ctx);
using TriListArrayFn    = void (*)(SerializeCtx* ctx, int count, void* data);
using ArrayResizeFn     = void (*)(void* array, int newSize);

template <typename Fn>
Fn RtAddrOnce(uintptr_t va, void** cache) {
    if (!*cache) *cache = reinterpret_cast<void*>(ResolveAddr(Module::Runtime, va));
    return reinterpret_cast<Fn>(*cache);
}

}  // namespace

// ---- slot 2  ?Type@EdgeMesh@Graphics@@UEBAIXZ  0x180012c70 -----------------

extern "C" uint32_t RtGraphics_EdgeMesh_Type(const void* self) {
    RT_DIFF_BEGIN("Graphics::EdgeMesh::Type");
    const uint32_t ours = ReadTypeId(EdgeMeshTypeInfo());
    RT_DIFF_SCALAR(ours, RT_ORIGINAL_ONCE(U32ThisFn,
                       "?Type@EdgeMesh@Graphics@@UEBAIXZ")(self));
    RT_DIFF_WORK(true);
    return ours;
}

// ---- slot 1  ?TypeName@EdgeMesh@Graphics@@UEBAPEBDXZ  0x180012c90 ----------
//
// ⚠ A POINTER DIVERGENCE IS EXPECTED, NOT A REGRESSION, once the
//   `?_typeInfo@EdgeMesh@` row is promoted: our node lives in our image and the
//   shipped export returns the DLL's — same characters, different object.
//   The comparison is deliberately left as a POINTER rather than relaxed to a
//   strcmp, because it is the one screen that can still see identity move, and
//   the identity question is retired in both shipped binaries (zero consumers
//   of any node's +0x08 — reference_basetypeinfo_name_is_content_only.md).
extern "C" const char* RtGraphics_EdgeMesh_TypeNameStr(const void* self) {
    RT_DIFF_BEGIN("Graphics::EdgeMesh::TypeName");
    const char* const ours = ReadTypeName(EdgeMeshTypeInfo());
    RT_DIFF_SCALAR(static_cast<const void*>(ours),
                   static_cast<const void*>(
                       RT_ORIGINAL_ONCE(CStrThisFn,
                           "?TypeName@EdgeMesh@Graphics@@UEBAPEBDXZ")(self)));
    RT_DIFF_WORK(true);
    return ours;
}

// ---- slot 3  ?GetFileVersion@EdgeMesh@Graphics@@UEBAHXZ  0x1800a23d0 -------
//
//   b8 02 00 00 00 c3     MOV EAX,2 ; RET
//
// A LITERAL, not a read: the on-disk serialised format version of a
// Graphics::EdgeMesh. Faithful means returning 2 and NOT deriving it from
// anything in our tree — a derived spelling would silently track OUR format if
// EdgeMesh::Serialize below is ever revised, and retail's does not. It is also
// NOT the same constant as MaterialInstance's 4 or Particle's 0x1000, which is
// why each of the three is written as its own literal in its own file.
extern "C" int32_t RtGraphics_EdgeMesh_GetFileVersion(const void* self) {
    RT_DIFF_BEGIN("Graphics::EdgeMesh::GetFileVersion");
    const int32_t ours = 2;
    RT_DIFF_SCALAR(ours, RT_ORIGINAL_ONCE(I32ThisFn,
                       "?GetFileVersion@EdgeMesh@Graphics@@UEBAHXZ")(self));
    RT_DIFF_WORK(true);
    return ours;
}

// ---- slot 4  ?Compile@EdgeMesh@Graphics@@UEAA...  0x1800a21c0 --------------
//
//   b8 01 00 00 00 c3     MOV EAX,1 ; RET
//
// Six bytes. It reads NEITHER `this` NOR the Filename& — the encoding has no
// RCX and no RDX operand — so `Object::External::CompileResult` = 1 is what
// every EdgeMesh compile returns, unconditionally. Whatever 1 means in that
// enum, this body does not decide it and neither does the port: the constant is
// transcribed, not interpreted.
//
// The Filename& parameter is kept in the signature even though the body never
// touches it, because the vtable slot is still CALLED with it in RDX and a
// narrower signature would be a different ABI on any future by-pointer use.
extern "C" int32_t RtGraphics_EdgeMesh_Compile(void* self, const void* filename) {
    RT_DIFF_BEGIN("Graphics::EdgeMesh::Compile");
    const int32_t ours = 1;
    RT_DIFF_SCALAR(ours, RT_ORIGINAL_ONCE(CompileFn,
                       "?Compile@EdgeMesh@Graphics@@UEAA?AW4CompileResult@External@"
                       "Object@@AEBVFilename@System@@@Z")(self, filename));
    RT_DIFF_WORK(true);
    return ours;
}

// ---- slot 5  ?Serialize@EdgeMesh@Graphics@@UEAA_NAEAVStream@IO@@@Z ---------
//                                                            0x1800a21d0
//
// NO ORACLE, AND THE REASON IS THE ONE THE TREE ALREADY WROTE DOWN ONCE FOR
// THIS FAMILY. mesh.cpp's HELD BACK note (item 2) refuses
// ?Serialize@MeshInstance@ on exactly this ground and the same ground holds
// here: a differential run would consume or emit the record TWICE and desync
// the stream, and a stream desync does not surface where it happens — it
// surfaces later, on an innocent record. Wave 5 held four functions back for
// the same reason. RT_DIFF_WORK is recorded so the body's ATTENDANCE is
// visible; no RT_DIFF_SCALAR / RT_DIFF_BYTES is emitted, and the absence is
// deliberate rather than an omission.
//
// WHAT MAKES PORTING IT SAFE ANYWAY, and it is a narrow claim: this body
// performs NO ARITHMETIC OF ITS OWN. Every line below is a transcription of a
// call — same helper, same arguments, same order, same vtable byte offsets read
// from the same retail image. The two places it does branch are both a test of
// `stream->mode`, and both are reproduced from the shipped compare rather than
// reasoned about. There is nothing here for a misunderstanding of the format to
// hide in; the format lives entirely in the four helpers, which are NOT ported.
//
// THE SHIPPED ORDER, transcribed line for line from 0x1800a21d0:
//   1. ctx = { stream, nullptr }
//   2. factory = stream->+0x10;
//      ctx.encoding = factory->vt[ stream->mode ? +0x20 : +0x18 ](factory, &ctx, 0)
//   3. ExtPtrBase    (&this->+0x20, &ctx)
//   4. ExtPtrMaterial(&this->_material0 .. _material3, &ctx)   << in slot order
//   5. n = this->_triangleLists._size;
//      enc->vt[0xa0](enc, &n, 0);
//      if (stream->mode == 0) Array_SetSize(&this->_triangleLists, n);
//      SerializeTriangleListArray(&ctx, n, this->_triangleLists._data)
//   6. the same five lines for _edgeLists   (+0x160 size, +0x158 data)
//   7. eight Video-buffer virtual calls, `buf->vt[0x30](buf, &ctx)`, in
//      retail's order: +0x70, +0x50, +0xb0, +0x90, +0xf0, +0xd0, +0x130, +0x110
//      — INDEX buffer first in each pair, and the pointer is at header+0x08
//      (what PlatformConstruct wrote), which is why these are 8 past the
//      header offsets in the type header.
//   8. enc->vt[0x28](enc, 3, &this->_bounds.min, 0)  and the same for +0x174
//   9. if (enc) enc->vt[0](enc, 1)      <- destroy AND free
//  10. return true.   `MOV AL,1` — this slot has no failure path at all.
//
// STEP 5 IS WRITTEN IN THE SHIPPED ORDER AND THAT ORDER MATTERS: the count is
// serialised through a LOCAL (`MOV EAX,[RBX+0x150] ; MOV [RBP+0x10],EAX`), the
// resize is driven from THAT LOCAL, and the element walk is driven from it too.
// On the read arm the encoding OVERWRITES the local, so the resize and the walk
// both use the value that came off the stream, not the value the array had.
// Reading `_size` again after the resize would be the same number by accident
// on the write arm and a silent truncation on the read arm.
extern "C" bool RtGraphics_EdgeMesh_Serialize(void* selfv, void* stream) {
    RT_DIFF_BEGIN("Graphics::EdgeMesh::Serialize[NO-ORACLE]");

    static void* s_extBase = nullptr;
    static void* s_extMat  = nullptr;
    static void* s_triList = nullptr;
    static void* s_resize  = nullptr;

    uint8_t* const self = reinterpret_cast<uint8_t*>(selfv);
    uint8_t* const st   = reinterpret_cast<uint8_t*>(stream);

    SerializeCtx ctx;
    ctx.stream   = stream;
    ctx.encoding = nullptr;

    const int32_t mode = *reinterpret_cast<const int32_t*>(st + kStreamModeOffset);
    void* const factory = *reinterpret_cast<void**>(st + kStreamFactoryOffset);
    {
        void** const fvt = *reinterpret_cast<void***>(factory);
        using MakeEncFn = void* (*)(void*, SerializeCtx*, int);
        const size_t off = (mode == 0) ? kFactoryMakeReadEncoding
                                       : kFactoryMakeWriteEncoding;
        MakeEncFn make = *reinterpret_cast<MakeEncFn*>(
            reinterpret_cast<uint8_t*>(fvt) + off);
        ctx.encoding = make(factory, &ctx, 0);
    }

    ExtPtrSerializeFn extBase =
        RtAddrOnce<ExtPtrSerializeFn>(0x180014230ull, &s_extBase);
    ExtPtrSerializeFn extMat =
        RtAddrOnce<ExtPtrSerializeFn>(0x180014520ull, &s_extMat);
    TriListArrayFn triArray =
        RtAddrOnce<TriListArrayFn>(0x180014850ull, &s_triList);
    ArrayResizeFn arrayResize =
        RtAddrOnce<ArrayResizeFn>(0x1800136f0ull, &s_resize);

    extBase(self + 0x20, &ctx);
    extMat (self + 0x28, &ctx);
    extMat (self + 0x30, &ctx);
    extMat (self + 0x38, &ctx);
    extMat (self + 0x40, &ctx);

    void** const evt = *reinterpret_cast<void***>(ctx.encoding);
    using EncDwordFn = void (*)(void*, void*, int);
    using EncArrayFn = void (*)(void*, int, void*, int);
    EncDwordFn encDword = *reinterpret_cast<EncDwordFn*>(
        reinterpret_cast<uint8_t*>(evt) + kEncSerializeDword);
    EncArrayFn encArray = *reinterpret_cast<EncArrayFn*>(
        reinterpret_cast<uint8_t*>(evt) + kEncSerializeArray);

    {   // step 5 — the triangle lists, +0x148 data / +0x150 size
        int32_t n = *reinterpret_cast<const int32_t*>(self + 0x150);
        encDword(ctx.encoding, &n, 0);
        if (*reinterpret_cast<const int32_t*>(st + kStreamModeOffset) == 0)
            arrayResize(self + 0x148, n);
        triArray(&ctx, n, *reinterpret_cast<void**>(self + 0x148));
    }
    {   // step 6 — the edge lists, +0x158 data / +0x160 size
        int32_t n = *reinterpret_cast<const int32_t*>(self + 0x160);
        encDword(ctx.encoding, &n, 0);
        if (*reinterpret_cast<const int32_t*>(st + kStreamModeOffset) == 0)
            arrayResize(self + 0x158, n);
        triArray(&ctx, n, *reinterpret_cast<void**>(self + 0x158));
    }

    {   // step 7 — eight Video buffers, index before vertex, retail's order
        static const size_t kBufferSlots[8] = {
            0x70, 0x50, 0xb0, 0x90, 0xf0, 0xd0, 0x130, 0x110
        };
        using BufSerializeFn = void (*)(void*, SerializeCtx*);
        for (size_t i = 0; i < 8; ++i) {
            void* const buf = *reinterpret_cast<void**>(self + kBufferSlots[i]);
            void** const bvt = *reinterpret_cast<void***>(buf);
            BufSerializeFn f = *reinterpret_cast<BufSerializeFn*>(
                reinterpret_cast<uint8_t*>(bvt) + kVideoBufferSerialize);
            f(buf, &ctx);
        }
    }

    // step 8 — the bounds box, as two runs of THREE floats. Retail passes the
    // literal 3 in EDX (`LEA EDX,[R9+0x3]` with R9 already zeroed), and the two
    // runs are +0x168 and +0x174, not one run of six: the Box is {min, max} and
    // the encoding is told so.
    encArray(ctx.encoding, 3, self + 0x168, 0);
    encArray(ctx.encoding, 3, self + 0x174, 0);

    // step 9 — `if (enc) enc->vt[0](enc, 1)`. Flag 1 is destroy AND free; the
    // Encoding the factory made in step 2 is owned by this frame.
    if (ctx.encoding) {
        void** const vt = *reinterpret_cast<void***>(ctx.encoding);
        reinterpret_cast<Slot0Fn>(vt[0])(ctx.encoding, 1);
    }

    RT_DIFF_WORK(true);
    return true;   // MOV AL,1 — no failure path exists in the shipped body.
}

// ---- slot 7  ?Destruct@EdgeMesh@Graphics@@UEAAXXZ  0x180012e40 -------------
//
//   180012e40  MOV RAX,[RCX] ; XOR EDX,EDX ; JMP [RAX]
//
// SEVEN BYTES, then padding. Same shape and the same consequence as
// ?Destruct@MaterialInstance@ (wave24_lane_gfxmat.cpp:399): the body is far
// smaller than the twelve bytes the smallest inline detour in this tree needs,
// so this row can NEVER be detoured — a registration is its only door, and with
// rel32 = 0 a registration reaches every caller there is.
//
// No null test. Retail has none; destructing a null faults here exactly as it
// faults in the DLL (CLAUDE.md, "Don't fix a faithful crash with a null check").
extern "C" void RtGraphics_EdgeMesh_Destruct(void* selfv) {
    RT_DIFF_BEGIN("Graphics::EdgeMesh::Destruct");
    void** const vp = reinterpret_cast<void**>(selfv);
    reinterpret_cast<Slot0Fn>(static_cast<void**>(*vp)[0])(selfv, 0);
    RT_DIFF_WORK(true);
}

// ---- slot 8  ?Construct@EdgeMesh@Graphics@@UEAAPEAVExternal@Object@@XZ -----
//                                                            0x180012e30
//
//   180012e30  TEST RCX,RCX ; JNZ 0x1800a2830 ; XOR EAX,EAX ; RET
//
// TWELVE BYTES. Unlike MaterialInstance::Construct — which INLINES its whole
// constructor — EdgeMesh::Construct is a guarded TAIL JUMP to the real ctor at
// 0x1800a2830. The null test is retail's own and returns null; it is kept.
//
// The ctor is reached by NAME (see EdgeMeshCtor above), so this body follows
// mesh.cpp's ported ctor whenever BANISHED_RT_GFXMESH is on. A tail jump and a
// call differ only in the stack frame, which no caller of this slot observes.
extern "C" void* RtGraphics_EdgeMesh_Construct(void* selfv) {
    RT_DIFF_BEGIN("Graphics::EdgeMesh::Construct");
    if (!selfv) { RT_DIFF_WORK(true); return nullptr; }
    CtorFn ctor = EdgeMeshCtor();
    void* const ours = ctor ? ctor(selfv) : nullptr;
    RT_DIFF_WORK(true);
    return ours;
}

// ---- slot 6  ?Reconstruct@EdgeMesh@Graphics@@UEAAPEAVExternal@Object@@XZ ---
//                                                            0x180012e10
//
//   180012e10  PUSH RBX ; SUB RSP,0x20
//              MOV RAX,[RCX] ; XOR EDX,EDX ; MOV RBX,RCX ; CALL [RAX]
//              MOV RCX,RBX ; ADD RSP,0x20 ; POP RBX ; JMP 0x1800a2830
//
// Destroy in place through vtable slot 0 with flag 0 — NOT flag 1: the storage
// is reused, not freed — then re-run the constructor on the same storage and
// return what the ctor returns, which is `this`.
//
// NO NULL TEST, unlike Construct one slot over. That asymmetry is retail's and
// is kept: reconstructing a null faults here exactly as it faults in the DLL.
// The identical asymmetry exists in the MaterialInstance pair and was kept
// there for the same reason (wave24_lane_gfxmat.cpp:440).
extern "C" void* RtGraphics_EdgeMesh_Reconstruct(void* selfv) {
    RT_DIFF_BEGIN("Graphics::EdgeMesh::Reconstruct");
    void** const vp = reinterpret_cast<void**>(selfv);
    reinterpret_cast<Slot0Fn>(static_cast<void**>(*vp)[0])(selfv, 0);
    CtorFn ctor = EdgeMeshCtor();
    void* const ours = ctor ? ctor(selfv) : nullptr;
    RT_DIFF_WORK(true);
    return ours;
}

// ============================================================================
// REGISTRATION — by NAME and by ADDRESS, both, for every body.
//
// A body PORTED but never REGISTERED is indistinguishable from an ABSENT one to
// the census (reference_half_registered_port_reads_as_absent.md). The mint
// fills a slot by NAME for RT_MINT_NAMED and by ADDRESS otherwise; all eight of
// these are RT_MINT_NAMED in standalone/rt_data_mint_tables.inc (kVSlots22), so
// the name door is the one the table needs. The address door is registered
// anyway so a by-address call site reaches the same body
// (reference_resolveaddr_consults_port_registry_first.md).
//
// NOTHING IN THIS TREE USES ANY OF THESE EIGHT ADDRESSES AS AN IDENTITY TOKEN —
// grepped across runtime/ code/ standalone/ engine/ dx11/ before the address
// doors were opened, because that is precisely what makes
// ?AddDrawItems@MeshInstance@ unregisterable (standalone/scene_watch.cpp:401,
// and see WAVE27_LANE_GFXMESH.md §7). None of the eight appears anywhere except
// rt_data_mint_tables.inc, which STAMPS them rather than comparing them.
// ============================================================================
int RtGraphicsEdgeMeshVtableRegister() {
    int n = 0;

    RegisterRuntimeExport("?Type@EdgeMesh@Graphics@@UEBAIXZ",
                          (void*)&RtGraphics_EdgeMesh_Type); ++n;
    RegisterRuntimeAddr(0x180012c70ull, (void*)&RtGraphics_EdgeMesh_Type);

    RegisterRuntimeExport("?TypeName@EdgeMesh@Graphics@@UEBAPEBDXZ",
                          (void*)&RtGraphics_EdgeMesh_TypeNameStr); ++n;
    RegisterRuntimeAddr(0x180012c90ull, (void*)&RtGraphics_EdgeMesh_TypeNameStr);

    RegisterRuntimeExport("?GetFileVersion@EdgeMesh@Graphics@@UEBAHXZ",
                          (void*)&RtGraphics_EdgeMesh_GetFileVersion); ++n;
    RegisterRuntimeAddr(0x1800a23d0ull, (void*)&RtGraphics_EdgeMesh_GetFileVersion);

    RegisterRuntimeExport("?Compile@EdgeMesh@Graphics@@UEAA?AW4CompileResult@External@"
                          "Object@@AEBVFilename@System@@@Z",
                          (void*)&RtGraphics_EdgeMesh_Compile); ++n;
    RegisterRuntimeAddr(0x1800a21c0ull, (void*)&RtGraphics_EdgeMesh_Compile);

    RegisterRuntimeExport("?Serialize@EdgeMesh@Graphics@@UEAA_NAEAVStream@IO@@@Z",
                          (void*)&RtGraphics_EdgeMesh_Serialize); ++n;
    RegisterRuntimeAddr(0x1800a21d0ull, (void*)&RtGraphics_EdgeMesh_Serialize);

    RegisterRuntimeExport("?Reconstruct@EdgeMesh@Graphics@@UEAAPEAVExternal@Object@@XZ",
                          (void*)&RtGraphics_EdgeMesh_Reconstruct); ++n;
    RegisterRuntimeAddr(0x180012e10ull, (void*)&RtGraphics_EdgeMesh_Reconstruct);

    RegisterRuntimeExport("?Destruct@EdgeMesh@Graphics@@UEAAXXZ",
                          (void*)&RtGraphics_EdgeMesh_Destruct); ++n;
    RegisterRuntimeAddr(0x180012e40ull, (void*)&RtGraphics_EdgeMesh_Destruct);

    RegisterRuntimeExport("?Construct@EdgeMesh@Graphics@@UEAAPEAVExternal@Object@@XZ",
                          (void*)&RtGraphics_EdgeMesh_Construct); ++n;
    RegisterRuntimeAddr(0x180012e30ull, (void*)&RtGraphics_EdgeMesh_Construct);

    return n;   // 8
}
