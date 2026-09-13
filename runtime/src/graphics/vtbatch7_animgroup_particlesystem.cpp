// ============================================================================
// runtime/src/graphics/vtbatch7_animgroup_particlesystem.cpp
//   VTABLE BATCH 7 — Graphics::AnimationGroup and Graphics::ParticleSystem
// ============================================================================
//
// Both classes are Object::External subclasses (no CompiledExternal step --
// their ctors stamp Base/External then their OWN final vtable directly, unlike
// Video::VertexLayout's three-level chain in vertexlayout.cpp). All addresses
// below are RETAIL Runtime-x64.dll (read with objdump against the shipped
// file, not Ghidra's decompile -- reference_ghidra_c_output_vs_disassembly.md).
//
// ---------------------------------------------------------------------------
// Graphics::AnimationGroup — ??_7AnimationGroup@Graphics@@6B@, 0x58 bytes
// ---------------------------------------------------------------------------
//   slot0 deleting dtor   0x1800167f0  runtime/src/object/deleting_dtor.cpp
//   slot1 TypeName        0x180016310  here (reads ?_typeInfo@AnimationGroup@Graphics@@+8)
//   slot2 Type            0x1800162f0  here (reads ?_typeInfo@AnimationGroup@Graphics@@+0)
//   slot3 GetFileVersion  0x1800fb680  here (returns 8)
//   slot4 Compile         0x1800fb690  here (returns CompileResult::Success == 1,
//                                       no-op -- AnimationGroup has no separate
//                                       compile step, it loads pre-baked)
//   slot5 Serialize       0x1800fb6a0  here
//   slot6 Reconstruct     0x180016550  here
//   slot7 Destruct        0x180016650  here
//   slot8 Construct       0x1800165d0  here
//
// Layout (ctor 0x180016550/0x1800165d0, both inline the SAME stores; the
// intermediate Base/External vtable stamps are folded away by MSVC and only
// the LAST store — AnimationGroup's own vtable — survives, exactly the
// AnimationEvent/Particle pattern documented in Particle.h):
//   +0x00 vtable = ??_7AnimationGroup@Graphics@@6B@
//   +0x08/+0x10/+0x18  External fields, zeroed
//   +0x20 Array<AnimationClip>   _clips     {data, size@+0x28, cap@+0x2c}
//   +0x30 float                  _f30       default 0.1f (0x3dcccccd)
//   +0x38 Array<{String,String,float}> _tags {data, size@+0x40, cap@+0x44}
//         (stride 0x18: two System::String refs then a float; ctor's
//          `imul $0x70` helper at 0x180016660 pins the CLIP stride, not this
//          one -- this one's stride is read off Serialize's element loop,
//          see below)
//   +0x48 Array<float>           _floats    {data, size@+0x50, cap@+0x54}
// Total 0x58, matches the deleting dtor's measured elemSize.
//
// AnimationClip (0x70 bytes, the +0x20 array's element -- pinned by
// 0x180016660's `imul $0x70` GetElement helper AND by Serialize's element loop
// 0x18005a040, which is what supplies every field offset below):
//   +0x00 Array<{int32 tag, ExternalPtr<AnimationEvent>}> _events (0x10 header)
//   +0x10 String  _str1     (semantics unread -- byte position only)
//   +0x18 String  _str2
//   +0x20 String  _str3
//   +0x28 int32   _i28
//   +0x2c bool    _b2c
//   +0x30 int32   _i30
//   +0x34 int32   _i34
//   +0x38 float   _f38
//   +0x3c bool    _b3c
//   +0x40 int32   _i40
//   +0x44 float   _f44
//   +0x48 float   _f48
//   +0x4c int32   _i4c
//   +0x50 int32   _i50
//   +0x58 int64   _i58
//   +0x60 Array<Math::Transform> _pose (0x10 header)
// Total 0x70.
//
// Non-slot helpers this Serialize calls, and how each is reached:
//   0x1800c9930  System::String::Serialize             ALREADY PORTED (string.cpp)
//   0x180059820  Array<Math::Transform>::Serialize      ALREADY PORTED
//                (runtime/src/graphics/animationstate.cpp,
//                 RtGraphics_SerializeTransformArray)
//   0x1800594a0  Object::ExternalPtr<AnimationEvent>::Serialize
//                PORTED IN THIS BATCH, appended to
//                runtime/src/object/external_ptr_serialize.cpp (the shared
//                748-byte ExternalPtr<T>::Serialize template, one more
//                instantiation; typeInfo = ?_typeInfo@AnimationEvent@Graphics@@)
//   0x180059e70  Array<AnimationClip>::SetSize          *** ALL FOUR ARE NOW
//   0x18005a220  Array<{String,String,float}>::SetSize  PORTED AND REGISTERED
//   0x18005a410  Array<float>::SetSize                  (VTABLE BATCH 7b) ***
//   0x180059930  Array<{int32,ExternalPtr}>::SetSize    as J3 rows in
//                runtime/src/collection/array.cpp: kSpec_AnimClip70,
//                kSpec_TagStrStrF18, kSpec_I32 and kSpec_IdExtPtr10. The call
//                sites below are UNCHANGED -- they already went through
//                ResolveAddr, which consults the port registry first
//                (reference_resolveaddr_consults_port_registry_first.md), so
//                registering the four addresses is what moved them from the
//                DLL to our bodies.
//                (This banner used to end "adding four more ArraySpec rows ...
//                is out of this batch's scope", which was true of batch 7.)
//
// ---------------------------------------------------------------------------
// Graphics::ParticleSystem — ??_7ParticleSystem@Graphics@@6B@, 0x108 bytes
// ---------------------------------------------------------------------------
//   slot0 deleting dtor   0x180020a70  runtime/src/object/deleting_dtor.cpp
//   slot1 TypeName        0x1800203e0  here
//   slot2 Type            0x1800203c0  here
//   slot3 GetFileVersion  0x1800a4d10  here (returns 0x1001)
//   slot5 Serialize       0x1800a4d20  here
//   slot6 Reconstruct     0x180020560  here (calls vtable[0](this,0), then the
//                                       real ctor 0x1800a5320, reached by
//                                       address -- see below)
//   slot7 Destruct        0x180020590  here (tail-calls vtable[0](this,0))
//   slot8 CreateSystem    0x1800a5140  here (thin forward -- see below)
//   slot9 Construct       0x180020580  here
//
// ParticleSystem extends Graphics::Particle (0x48 bytes, runtime/types/graphics/
// Particle.h); its OWN fields start at +0x48 and were all independently
// confirmed here as a WRITER (Serialize), agreeing field-for-field with the
// kPS_* offsets wave28_lane_partsys.cpp already measured as a READER
// (ParticleSystemInstance::Update) -- two independent witnesses.
//
// *** VTABLE BATCH 7b PORTED BOTH OF THE BODIES BATCH 7 THUNKED. ***
// `??0ParticleSystem@Graphics@@QEAA@XZ` (the real ctor, 0x1800a5320, ord 302)
// and `?CreateSystem@ParticleSystem@Graphics@@` (0x1800a5140, ord 2134, which
// allocates a ParticleInstance of size 0xe0 and calls ITS ctor 0x1800a55a0) are
// both written out below, and Construct/Reconstruct/CreateSystem now call OUR
// bodies rather than forwarding through ResolveAddr. The ctor is registered at
// its own export name and address as well, so a caller that reaches it directly
// (Object::External's factory path does) gets ours too.
// (This banner used to say both "are NOT reimplemented here ... ported as THIN
// FORWARDING THUNKS", which was true of batch 7.)
// 0x1800a55a0, the ParticleInstance ctor, is still reached by address: it is a
// different class's row (wave28_lane_partsys.cpp owns that class).
// ============================================================================

#include <cstddef>
#include <cstdint>
#include <cstring>

#include "../../funcs/rt_port.h"
#include "../../../standalone/resolve.h"
#include "../../types/collection/Collection.h"
#include "../../types/io/Serializer.h"
#include "../../types/object/External.h"
#include "../../types/graphics/Particle.h"
#include "../object/deleting_dtor_internal.h"   // rtcoll::MemAlloc

using rt::io::ReaderWriter;
using rt::io::Serializer;
using rt::io::Stream;
namespace rw_vtbl = rt::io::rw_vtbl;

// 0x180059820 — Array<Math::Transform>::Serialize, already ported and
// registered by runtime/src/graphics/animationstate.cpp. `extern "C"`
// (matching that definition's linkage) and declared at NAMESPACE SCOPE, not
// inside the anonymous namespace below or a function body: an `extern`
// re-declaration inside this file's own anonymous namespace would pick up
// C++ (mangled) linkage and silently bind to nothing, rather than to that
// file's C-linkage symbol.
extern "C" void RtGraphics_SerializeTransformArray(void* arrayv, Serializer* ser);

namespace {

template <typename Fn>
Fn RtAddr(uintptr_t ghidra) { return reinterpret_cast<Fn>(ResolveAddr(Module::Runtime, ghidra)); }

using ScalarFn = void (*)(ReaderWriter*, void*, const char*);
using ArrayFn  = void (*)(ReaderWriter*, int, void*, const char*);

void RwScalar(ReaderWriter* rw, int slot, void* p) {
    reinterpret_cast<ScalarFn>(rw->vtable[slot / 8])(rw, p, nullptr);
}
void RwFloats(ReaderWriter* rw, int count, void* p) {
    reinterpret_cast<ArrayFn>(rw->vtable[rw_vtbl::kArrayFloat / 8])(rw, count, p, nullptr);
}
bool IsReading(const Stream* s) { return s->direction == rt::io::kDirectionReading; }

// Build the local {stream,rw} pair the same way every External::Serialize in
// this tree does (vertexlayout.cpp's ConstructBody banner is the reference
// shape); returns the ReaderWriter*, which the caller must destroy(1) if
// non-null.
ReaderWriter* MakeReaderWriter(Serializer* ser, Stream* stream) {
    ser->stream = stream;
    ser->readerWriter = nullptr;
    void** encVt = *reinterpret_cast<void***>(stream->encoding);
    const int slot = IsReading(stream) ? rt::io::encoding_vtbl::kCreateReader
                                       : rt::io::encoding_vtbl::kCreateWriter;
    using MakeRwFn = ReaderWriter* (*)(void*, Serializer*, int);
    ReaderWriter* rw = reinterpret_cast<MakeRwFn>(encVt[slot / 8])(stream->encoding, ser, 0);
    ser->readerWriter = rw;
    return rw;
}
void DestroyReaderWriter(ReaderWriter* rw) {
    if (!rw) return;
    using DtorFn = void (*)(ReaderWriter*, int);
    reinterpret_cast<DtorFn>(rw->vtable[0])(rw, 1);
}

void* BaseVTable()     { return RT_DATA_SLOT(void*, "??_7Base@Object@@6B@"); }
void* ExternalVTable() { return RT_DATA_SLOT(void*, "??_7External@Object@@6B@"); }

using Slot0Fn = void* (*)(void* self, int flag);
void CallSlot0(void* self, int flag) {
    void** vt = *reinterpret_cast<void***>(self);
    reinterpret_cast<Slot0Fn>(vt[0])(self, flag);
}

}  // namespace

// ============================================================================
// Graphics::AnimationGroup
// ============================================================================
namespace {

struct AnimationClip {
    void*    eventsData;  int32_t eventsSize; int32_t eventsCap;  // +0x00
    void*    str1;                                                 // +0x10
    void*    str2;                                                 // +0x18
    void*    str3;                                                 // +0x20
    int32_t  i28;                                                  // +0x28
    uint8_t  b2c; uint8_t pad2d[3];                                 // +0x2c
    int32_t  i30;                                                   // +0x30
    int32_t  i34;                                                   // +0x34
    float    f38;                                                   // +0x38
    uint8_t  b3c; uint8_t pad3d[3];                                  // +0x3c
    int32_t  i40;                                                    // +0x40
    float    f44;                                                    // +0x44
    float    f48;                                                    // +0x48
    int32_t  i4c;                                                     // +0x4c
    int32_t  i50;                                                     // +0x50
    uint8_t  pad54[4];                                                 // +0x54
    int64_t  i58;                                                       // +0x58
    void*    poseData; int32_t poseSize; int32_t poseCap;                // +0x60
};
static_assert(sizeof(AnimationClip) == 0x70, "AnimationGroup::Clip must be 0x70 bytes");
static_assert(offsetof(AnimationClip, str1) == 0x10, "+0x10");
static_assert(offsetof(AnimationClip, i58)  == 0x58, "+0x58");
static_assert(offsetof(AnimationClip, poseData) == 0x60, "+0x60");

struct AnimationGroup {
    RtObject::External base;                        // +0x00 (0x20)
    void*   clipData;    int32_t clipSize; int32_t clipCap;   // +0x20
    float   f30;                                                // +0x30
    void*   tagData;     int32_t tagSize;  int32_t tagCap;      // +0x38
    void*   floatData;   int32_t floatSize; int32_t floatCap;   // +0x48
};
static_assert(sizeof(AnimationGroup) == 0x58, "Graphics::AnimationGroup must be 0x58 bytes");
static_assert(offsetof(AnimationGroup, clipData)  == 0x20, "+0x20");
static_assert(offsetof(AnimationGroup, f30)       == 0x30, "+0x30");
static_assert(offsetof(AnimationGroup, tagData)   == 0x38, "+0x38");
static_assert(offsetof(AnimationGroup, floatData) == 0x48, "+0x48");

void* AnimGroupVTable() { return RT_DATA_SLOT(void*, "??_7AnimationGroup@Graphics@@6B@"); }
const uint8_t* AnimGroupTypeInfo() {
    return RT_DATA_SLOT(const uint8_t*, "?_typeInfo@AnimationGroup@Graphics@@0VBaseTypeInfo@Object@@A");
}

void ConstructBody(AnimationGroup* self) {
    void** vp = reinterpret_cast<void**>(self);
    *vp = BaseVTable();
    *vp = ExternalVTable();
    self->base._path.data      = nullptr;
    self->base._refs._data     = nullptr;
    self->base._refs._size     = 0;
    self->base._refs._capacity = 0;
    *vp = AnimGroupVTable();
    self->clipData = nullptr; self->clipSize = 0; self->clipCap = 0;
    self->f30 = 0.1f;
    self->tagData = nullptr; self->tagSize = 0; self->tagCap = 0;
    self->floatData = nullptr; self->floatSize = 0; self->floatCap = 0;
}

// 0x1800594a0 — Object::ExternalPtr<AnimationEvent>::Serialize, ported as an
// appended thunk in external_ptr_serialize.cpp (shared 748-byte template).
extern "C" void RtObject_ExternalPtrSerialize_AnimationEvent(void** self, Serializer* ser);

// 0x18005a040 — the AnimationClip element loop, called by AnimationGroup's own
// Serialize with (Serializer*, int count, AnimationClip* data).
extern "C" void RtGraphics_AnimationGroup_SerializeClips(Serializer* ser, int count, void* datav) {
    if (count <= 0) return;
    AnimationClip* c = static_cast<AnimationClip*>(datav);
    using SetSizeFn = void (*)(void* arr, int32_t n);
    using StrSerFn  = void (*)(void* str, Serializer* ser);
    for (int i = 0; i < count; ++i, ++c) {
        ReaderWriter* rw = ser->readerWriter;
        StrSerFn strSer = RtAddr<StrSerFn>(0x1800c9930ull);
        strSer(&c->str1, ser);
        strSer(&c->str2, ser);
        strSer(&c->str3, ser);

        rw = ser->readerWriter;
        RwScalar(rw, rw_vtbl::kScalarInt, &c->i30);
        rw = ser->readerWriter;
        RwScalar(rw, rw_vtbl::kScalarInt, &c->i34);
        rw = ser->readerWriter;
        RwScalar(rw, rw_vtbl::kScalarFloat, &c->f38);
        rw = ser->readerWriter;
        RwScalar(rw, rw_vtbl::kScalarBool, &c->b3c);
        rw = ser->readerWriter;
        RwScalar(rw, rw_vtbl::kScalarInt, &c->i40);
        rw = ser->readerWriter;
        RwScalar(rw, rw_vtbl::kScalarInt, &c->i28);
        rw = ser->readerWriter;
        RwScalar(rw, rw_vtbl::kScalarBool, &c->b2c);

        // The nested Array<{int32,ExternalPtr<AnimationEvent>}> at c->+0x00.
        int32_t n = c->eventsSize;
        rw = ser->readerWriter;
        RwScalar(rw, rw_vtbl::kScalarInt, &n);
        if (IsReading(ser->stream)) {
            static SetSizeFn setSize = nullptr;
            if (!setSize) setSize = RtAddr<SetSizeFn>(0x180059930ull);
            setSize(&c->eventsData, n);
        }
        uint8_t* e = static_cast<uint8_t*>(c->eventsData);
        for (int32_t k = 0; k < n; ++k, e += 0x10) {
            rw = ser->readerWriter;
            RwScalar(rw, rw_vtbl::kScalarInt, e);
            RtObject_ExternalPtrSerialize_AnimationEvent(
                reinterpret_cast<void**>(e + 8), ser);
        }

        rw = ser->readerWriter;
        RwScalar(rw, rw_vtbl::kScalarInt, &c->i50);
        rw = ser->readerWriter;
        RwScalar(rw, rw_vtbl::kScalarFloat, &c->f44);
        rw = ser->readerWriter;
        RwScalar(rw, rw_vtbl::kScalarFloat, &c->f48);
        rw = ser->readerWriter;
        RwScalar(rw, rw_vtbl::kScalarInt, &c->i4c);

        // Already-ported Array<Math::Transform>::Serialize (animationstate.cpp).
        RtGraphics_SerializeTransformArray(&c->poseData, ser);

        rw = ser->readerWriter;
        RwScalar(rw, rw_vtbl::kScalarInt64, &c->i58);
    }
}

}  // namespace

extern "C" uint32_t RtGraphics_AnimationGroup_Type(const void*) {
    uint32_t v = 0; std::memcpy(&v, AnimGroupTypeInfo(), sizeof v); return v;
}
extern "C" const char* RtGraphics_AnimationGroup_TypeName(const void*) {
    const char* v = nullptr; std::memcpy(&v, AnimGroupTypeInfo() + 8, sizeof v); return v;
}
extern "C" int32_t RtGraphics_AnimationGroup_GetFileVersion(const void*) { return 8; }
extern "C" int32_t RtGraphics_AnimationGroup_Compile(void*, const void*) { return 1; /* CompileResult::Success */ }

extern "C" void* RtGraphics_AnimationGroup_Construct(void* selfv) {
    if (!selfv) return nullptr;
    ConstructBody(static_cast<AnimationGroup*>(selfv));
    return selfv;
}
extern "C" void* RtGraphics_AnimationGroup_Reconstruct(void* selfv) {
    CallSlot0(selfv, 0);
    ConstructBody(static_cast<AnimationGroup*>(selfv));
    return selfv;
}
extern "C" void RtGraphics_AnimationGroup_Destruct(void* selfv) { CallSlot0(selfv, 0); }

extern "C" bool RtGraphics_AnimationGroup_Serialize(void* selfv, void* streamv) {
    AnimationGroup* self = static_cast<AnimationGroup*>(selfv);
    Stream* stream = static_cast<Stream*>(streamv);
    Serializer ser{};
    ReaderWriter* rw = MakeReaderWriter(&ser, stream);

    // +0x20 Array<AnimationClip>
    int32_t n = self->clipSize;
    RwScalar(rw, rw_vtbl::kScalarInt, &n);
    if (IsReading(stream)) {
        using SetSizeFn = void (*)(void*, int32_t);
        RtAddr<SetSizeFn>(0x180059e70ull)(&self->clipData, n);
    }
    RtGraphics_AnimationGroup_SerializeClips(&ser, n, self->clipData);

    // +0x38 Array<{String,String,float}>, stride 0x18.
    n = self->tagSize;
    rw = ser.readerWriter;
    RwScalar(rw, rw_vtbl::kScalarInt, &n);
    if (IsReading(stream)) {
        using SetSizeFn = void (*)(void*, int32_t);
        RtAddr<SetSizeFn>(0x18005a220ull)(&self->tagData, n);
    }
    {
        using StrSerFn = void (*)(void* str, Serializer* ser);
        StrSerFn strSer = RtAddr<StrSerFn>(0x1800c9930ull);
        uint8_t* p = static_cast<uint8_t*>(self->tagData);
        for (int32_t i = 0; i < n; ++i, p += 0x18) {
            strSer(p + 0x00, &ser);
            strSer(p + 0x08, &ser);
            rw = ser.readerWriter;
            RwScalar(rw, rw_vtbl::kScalarFloat, p + 0x10);
        }
    }

    // +0x30 scalar float -- AFTER the tag array, not before it. Retail
    // 0x1800fb6a0 orders the emissions clips, tags, +0x30, floats (the
    // `(*rw+0x80)(this+0x30)` call sits between the tag loop and the third
    // count). This body was written with +0x30 before the tags, and because
    // it is reached only through the minted vtable it never ran DLL-present:
    // the first cutover boot to get this far read the tag count out of the
    // float's bytes (count=1288519116), asked CompressedStream::Read for bytes
    // past the end of the file, and spun forever in Read's chunk loop.
    rw = ser.readerWriter;
    RwScalar(rw, rw_vtbl::kScalarFloat, &self->f30);

    // +0x48 Array<float>, bulk kArrayFloat.
    n = self->floatSize;
    rw = ser.readerWriter;
    RwScalar(rw, rw_vtbl::kScalarInt, &n);
    if (IsReading(stream)) {
        using SetSizeFn = void (*)(void*, int32_t);
        RtAddr<SetSizeFn>(0x18005a410ull)(&self->floatData, n);
    }
    rw = ser.readerWriter;
    RwFloats(rw, n, self->floatData);

    DestroyReaderWriter(ser.readerWriter);
    return true;
}

// ============================================================================
// Graphics::ParticleSystem
// ============================================================================
namespace {

using RtGraphics::Particle;

// Fields are declared in ASCENDING MEMORY ORDER (offsetof requires it); the
// SERIALIZE ORDER retail actually uses is different (f78 before spreadX/Y/Z)
// and is reproduced explicitly in RtGraphics_ParticleSystem_Serialize's call
// sequence below, not by struct declaration order.
struct ParticleSystem {
    Particle base;                                    // +0x00 (0x48)
    float f48, f4c, f50;                              // +0x48
    int64_t i58;                                       // +0x58  kPS_DurationTicks
    float emitDirX, emitDirY, emitDirZ;                // +0x60  kPS_EmitDir
    float spreadX, spreadY, spreadZ;                    // +0x6c
    float f78;                                           // +0x78  kPS_SpreadScale (SERIALIZED before spreadX/Y/Z)
    float f7c, f80, f84, f88, f8c, f90, f94, f98,
          f9c, fa0, fa4, fa8, fac, fb0, fb4;              // +0x7c..+0xb4 (15 floats)
    float gravX, gravY, gravZ;                            // +0xb8
    float fc4, fc8, fcc, fd0;                              // +0xc4..+0xd0
    float colorT0, colorT1;                                 // +0xd4, +0xd8
    uint32_t color0, color1;                                 // +0xdc, +0xe0 (packed bytes, kScalarUInt)
    int64_t fadeInTicks, fadeOutTicks;                        // +0xe8, +0xf0
    uint8_t tail[0x108 - 0xf8];                                // +0xf8.. (dynamic pool array, not modelled)
};
static_assert(offsetof(ParticleSystem, i58) == 0x58, "+0x58");
static_assert(offsetof(ParticleSystem, emitDirX) == 0x60, "+0x60");
static_assert(offsetof(ParticleSystem, spreadX) == 0x6c, "+0x6c");
static_assert(offsetof(ParticleSystem, f78) == 0x78, "+0x78");
static_assert(offsetof(ParticleSystem, gravX) == 0xb8, "+0xb8");
static_assert(offsetof(ParticleSystem, colorT0) == 0xd4, "+0xd4");
static_assert(offsetof(ParticleSystem, color0) == 0xdc, "+0xdc");
static_assert(offsetof(ParticleSystem, fadeInTicks) == 0xe8, "+0xe8");
static_assert(sizeof(ParticleSystem) == 0x108, "Graphics::ParticleSystem must be 0x108 bytes");

const uint8_t* ParticleSystemTypeInfo() {
    return RT_DATA_SLOT(const uint8_t*, "?_typeInfo@ParticleSystem@Graphics@@0VBaseTypeInfo@Object@@A");
}

}  // namespace

extern "C" uint32_t RtGraphics_ParticleSystem_Type(const void*) {
    uint32_t v = 0; std::memcpy(&v, ParticleSystemTypeInfo(), sizeof v); return v;
}
extern "C" const char* RtGraphics_ParticleSystem_TypeName(const void*) {
    const char* v = nullptr; std::memcpy(&v, ParticleSystemTypeInfo() + 8, sizeof v); return v;
}
extern "C" int32_t RtGraphics_ParticleSystem_GetFileVersion(const void*) { return 0x1001; }

// ============================================================================
// ??0ParticleSystem@Graphics@@QEAA@XZ — retail 0x1800a5320, ord 302.
//
// BATCH 7 LEFT THIS AS A THUNK ("stays on the DLL / port registry"); batch 7b
// ports the body. It is 60 stores and one RET, no calls, and every value below
// is transcribed from the disassembly — including MSVC's merged 8-byte stores
// at misaligned offsets (+0x74, +0x7c, +0x84, +0xac, +0xb4, +0xc4, +0xd0,
// +0xdc), which are written here as the two 4-byte fields they cover.
//
// FOUR vtable stamps, in retail's order (Object::Base, Object::External,
// Graphics::Particle, Graphics::ParticleSystem). Retail stamps each with a
// RIP-relative LEA, which is NOT a door; porting the ctor converts all four
// into RT_DATA_SLOT lookups that resolve to the mint the day it has rows
// (reference_a_ported_row_can_be_permanently_unpromotable.md).
//
// THE TWO .rdata DOUBLES WERE READ, NOT RECOGNISED (pefile, raw section bytes):
//     0x180161760 = 00 00 00 00 00 00 f0 bf = -1.0
//     0x1801616b8 = 00 00 00 00 00 00 d0 3f =  0.25
// so +0x58 is `(int64)(-1.0 / _secondsPerTick)` — a NEGATIVE tick count, i.e.
// the "no explicit duration" sentinel — and +0xe8/+0xf0 are both
// `(int64)(0.25 / _secondsPerTick)`, computed by two separate DIVSDs off one
// loaded XMM1 (0x1800a5470 and 0x1800a5484).
// ============================================================================
namespace {

void* ParticleSystemVTable() { return RT_DATA_SLOT(void*, "??_7ParticleSystem@Graphics@@6B@"); }
void* ParticleVTable()       { return RT_DATA_SLOT(void*, "??_7Particle@Graphics@@6B@"); }

double SecondsPerTick() {
    double* slot = RT_DATA_SLOT(double*, "?_secondsPerTick@Timer@System@@0NA");
    return slot ? *slot : 1.0;
}

void ParticleSystemConstructBody(ParticleSystem* self) {
    void** vp = reinterpret_cast<void**>(self);
    *vp = BaseVTable();                                   // 1800a5338  0x180154788
    *vp = ExternalVTable();                               // 1800a5342  0x1801547a0
    self->base._f08 = 0;                                  // 1800a5350
    self->base._f10 = 0;                                  // 1800a5353
    self->base._f18 = 0;                                  // 1800a5357
    *vp = ParticleVTable();                               // 1800a5362  0x180155130
    self->base._active = 1;                               // 1800a5365  byte
    self->base._f24    = 1;                               // 1800a5369
    self->base._f28    = 0xf;                             // 1800a5370
    self->base._f30    = 0;                               // 1800a5377
    self->base._f38    = 0;                               // 1800a537b
    self->base._f40    = 0;                               // 1800a537f
    *vp = ParticleSystemVTable();                         // 1800a538a  0x180155090

    const double spt = SecondsPerTick();

    self->f48 = 5.0f;  self->f4c = 0.0f;                  // 1800a538d  one qword
    self->f50 = 1.0f;                                     // 1800a5395
    self->i58 = static_cast<int64_t>(-1.0 / spt);         // 1800a539c..b1
    self->emitDirX = 0.0f; self->emitDirY = 0.0f;         // 1800a53b5  one qword
    self->emitDirZ = 1.0f;                                // 1800a53b9
    self->spreadX  = 1.0f;                                // 1800a53c0
    self->spreadY  = 1.0f;                                // 1800a53c7
    self->spreadZ  = 1.0f; self->f78 = 0.0f;              // 1800a53ce  one qword
    self->f7c = 0.0f; self->f80 = 0.0f;                   // 1800a53d6  one qword
    self->f84 = 1.0f; self->f88 = 0.0f;                   // 1800a53da  one qword
    self->f8c = 0.0f;                                     // 1800a53e5
    self->f90 = 1.0f; self->f94 = 0.0f;                   // 1800a53eb  one qword
    self->f98 = 0.0f; self->f9c = 0.0f;                   // 1800a53f6  one qword
    self->fa0 = 0.0f; self->fa4 = 0.0f;                   // 1800a53fd  one qword
    self->fa8 = 1.0f;                                     // 1800a5404
    self->fac = 3.0f; self->fb0 = 0.0f;                   // 1800a540e  one qword
    self->fb4 = 1.0f; self->gravX = 0.0f;                 // 1800a5419  one qword
    self->gravY = 0.0f;                                   // 1800a5424
    self->gravZ = -9.8f;                                  // 1800a542a  0xc11ccccd
    self->fc4 = 0.5f; self->fc8 = 0.0f;                   // 1800a5434  one qword
    self->fcc = 0.0f;                                     // 1800a543f
    self->fd0 = 1.0f; self->colorT0 = 0.0f;               // 1800a5445  one qword
    self->colorT1 = 1.0f;                                 // 1800a5450
    self->color0 = 0xffffffffu;                           // 1800a545a  one qword
    self->color1 = 0xffffffffu;                           //            of -1
    self->fadeInTicks  = static_cast<int64_t>(0.25 / spt);// 1800a5465..7d
    self->fadeOutTicks = static_cast<int64_t>(0.25 / spt);// 1800a5484..91
    std::memset(self->tail, 0, sizeof self->tail);        // 1800a5498/9f  +0xf8,+0x100
}

}  // namespace

extern "C" void* RtGraphics_ParticleSystem_Ctor(void* selfv) {
    // Retail has NO null check (`MOV [RCX],RAX` on the first instruction).
    ParticleSystemConstructBody(static_cast<ParticleSystem*>(selfv));
    return selfv;                                          // 1800a54a6  MOV RAX,RCX
}

// Construct/Reconstruct are the vtable slots; each is retail's `JMP 0x1800a5320`
// (Construct) / `vtable[0](this,0)` then the same ctor (Reconstruct). They call
// OUR ctor directly now rather than round-tripping through ResolveAddr.
extern "C" void* RtGraphics_ParticleSystem_Construct(void* selfv) {
    if (!selfv) return nullptr;
    return RtGraphics_ParticleSystem_Ctor(selfv);
}
extern "C" void* RtGraphics_ParticleSystem_Reconstruct(void* selfv) {
    CallSlot0(selfv, 0);
    return RtGraphics_ParticleSystem_Ctor(selfv);
}
extern "C" void RtGraphics_ParticleSystem_Destruct(void* selfv) { CallSlot0(selfv, 0); }

// ============================================================================
// ?CreateSystem@ParticleSystem@Graphics@@ — retail 0x1800a5140, ord 2134.
//
// Also a batch-7 thunk, also ported here. Eleven instructions:
//
//   1800a5154  RBX = *0x1801fc718 + 0x640          the allocator's own CS
//   1800a516a  EnterCriticalSection(RBX)
//   1800a5183  MemAlloc(_singleton, 0xe0, 8)       0x1800c6e60
//   1800a518e  LeaveCriticalSection(RBX)
//   1800a519c  if (result) 0x1800a55a0(result)     ParticleInstance's ctor
//              else EAX = 0
//   1800a51b5  RET (the allocation, or the ctor's return, which is the same
//              pointer)
//
// `this` IS NEVER READ. Retail's CreateSystem ignores the ParticleSystem it is
// called on entirely — the returned instance is bound to its system later, by
// the caller. Reproduced, parameter deliberately unnamed.
//
// The Enter/Alloc/Leave triple goes through `rtcoll::MemAlloc` (array.cpp's
// accessor, declared in deleting_dtor_internal.h), which IS that triple on the
// same singleton, rather than re-opening four new address doors onto
// 0x1801fc718 / 0x18014b390 / 0x1800c6e60 / 0x18014b388 that array.cpp already
// owns. Same reasoning the Serialize tail below states for its own allocation.
//
// 0x1800a55a0 (`Graphics::ParticleInstance::ParticleInstance`) is NOT this
// batch's row and is reached BY ADDRESS — ResolveAddr consults the port
// registry first, so the day wave28_lane_partsys.cpp's class gets a ctor this
// call site picks it up with no edit here.
// ============================================================================
extern "C" void* RtGraphics_ParticleSystem_CreateSystem(void* /*self, never read*/) {
    void* inst = rtcoll::MemAlloc(0xe0, 8);                // 1800a5183
    if (!inst) return nullptr;                             // 1800a519c -> XOR EAX,EAX
    using CtorFn = void* (*)(void*);
    return RtAddr<CtorFn>(0x1800a55a0ull)(inst);           // 1800a51a1
}

extern "C" bool RtGraphics_ParticleSystem_Serialize(void* selfv, void* streamv) {
    ParticleSystem* self = static_cast<ParticleSystem*>(selfv);
    Stream* stream = static_cast<Stream*>(streamv);

    // Particle::Serialize base call (0x180103f00) -- already ported? Not by
    // this batch; reached by address (falls back to the DLL if unregistered).
    using BaseSerFn = void (*)(void*, void*);
    RtAddr<BaseSerFn>(0x180103f00ull)(self, stream);

    Serializer ser{};
    ReaderWriter* rw = MakeReaderWriter(&ser, stream);

    RwScalar(rw, rw_vtbl::kScalarFloat, &self->f48);
    rw = ser.readerWriter; RwScalar(rw, rw_vtbl::kScalarFloat, &self->f4c);
    rw = ser.readerWriter; RwScalar(rw, rw_vtbl::kScalarFloat, &self->f50);
    rw = ser.readerWriter; RwScalar(rw, rw_vtbl::kScalarInt64, &self->i58);
    rw = ser.readerWriter; RwFloats(rw, 3, &self->emitDirX);
    rw = ser.readerWriter; RwScalar(rw, rw_vtbl::kScalarFloat, &self->f78);   // retail's order: 0x78 before 0x6c
    rw = ser.readerWriter; RwFloats(rw, 3, &self->spreadX);

    float* f7cArray = &self->f7c;
    for (int i = 0; i < 15; ++i) {
        rw = ser.readerWriter;
        RwScalar(rw, rw_vtbl::kScalarFloat, &f7cArray[i]);
    }

    rw = ser.readerWriter; RwFloats(rw, 3, &self->gravX);

    rw = ser.readerWriter; RwScalar(rw, rw_vtbl::kScalarFloat, &self->fc4);
    rw = ser.readerWriter; RwScalar(rw, rw_vtbl::kScalarFloat, &self->fc8);
    rw = ser.readerWriter; RwScalar(rw, rw_vtbl::kScalarFloat, &self->fcc);
    rw = ser.readerWriter; RwScalar(rw, rw_vtbl::kScalarFloat, &self->fd0);

    rw = ser.readerWriter; RwScalar(rw, rw_vtbl::kScalarUInt, &self->color0);
    rw = ser.readerWriter; RwScalar(rw, rw_vtbl::kScalarUInt, &self->color1);
    rw = ser.readerWriter; RwScalar(rw, rw_vtbl::kScalarFloat, &self->colorT0);
    rw = ser.readerWriter; RwScalar(rw, rw_vtbl::kScalarFloat, &self->colorT1);
    rw = ser.readerWriter; RwScalar(rw, rw_vtbl::kScalarInt64, &self->fadeInTicks);
    rw = ser.readerWriter; RwScalar(rw, rw_vtbl::kScalarInt64, &self->fadeOutTicks);

    // READ-ONLY tail (retail 0x1800a505c..0x1800a5107): when reading and the
    // resource is active (Particle::_active, +0x20), rebuild the pool-pointer
    // array at self+0xf8 to Particle::_f24 entries and allocate one 0x18-byte
    // record per index.
    //
    // *** BATCH 7 GOT TWO THINGS WRONG HERE AND BATCH 7b FIXES BOTH, at the
    // source (PLAN.md §6 step 8). *** The old comment said 0x180020e90 is a
    // "record ctor"; it is not. Both addresses are Collection::Array<T>::SetSize
    // bodies of this tree's J3 family, and both are now PORTED AND REGISTERED
    // (runtime/src/collection/array.cpp: kSpec_Fill8 for the stride-8 pointer
    // array, kSpec_Rec14 for the record's own stride-0x14 inner array). So the
    // second call is `SetSize(&record->innerArray, ParticleSystem::_f28)`, and
    // these two ResolveAddr calls now land on OUR bodies rather than the DLL's.
    //
    // And the dword at record+0x10 is a RAW BIT COPY of self->+0x48
    // (0x1800a50df `MOV EAX,dword ptr [RSI+0x48]` / `MOV [RDI+0x10],EAX`), NOT
    // a float-to-int conversion. Batch 7 wrote `static_cast<int32_t>(self->f48)`,
    // which for the ctor's default 5.0f stores 5 where retail stores
    // 0x40a00000. Transcribed as a memcpy so the width and the bits both match.
    //
    // The allocation goes through `rtcoll::MemAlloc` (array.cpp's accessor,
    // declared in deleting_dtor_internal.h), which IS retail's
    // Enter/Allocate(0x18, align 8)/Leave triple on the same singleton, rather
    // than re-opening address doors onto 0x1801fc718+0x640 / 0x18014b390 /
    // 0x1800c6e60 / 0x18014b388 that array.cpp already owns.
    if (IsReading(stream) && self->base._active != 0) {
        using SetSizeFn = void (*)(void*, int32_t);
        RtAddr<SetSizeFn>(0x180021220ull)(reinterpret_cast<uint8_t*>(self) + 0xf8,
                                          self->base._f24);            // 1800a507a
        uint8_t** poolArr = *reinterpret_cast<uint8_t***>(
            reinterpret_cast<uint8_t*>(self) + 0xf8);                   // 1800a50f1
        SetSizeFn innerSetSize = RtAddr<SetSizeFn>(0x180020e90ull);
        for (int32_t i = 0; i < self->base._f24; ++i) {                 // 1800a5103
            uint8_t* rec = static_cast<uint8_t*>(rtcoll::MemAlloc(0x18, 8));
            if (rec) {                                                  // 1800a50d4
                std::memset(rec, 0, 0x10);                              // 1800a50d8/db
                std::memcpy(rec + 0x10,                                 // 1800a50df/e2
                            reinterpret_cast<const uint8_t*>(self) + 0x48, 4);
                innerSetSize(rec, self->base._f28);                     // 1800a50eb
            }
            // Retail stores RDI unconditionally, so a failed allocation parks a
            // null in the slot rather than skipping the index.
            poolArr[i] = rec;                                           // 1800a50f8
        }
    }

    DestroyReaderWriter(ser.readerWriter);
    return true;
}

int RtGraphicsBatch7AnimGroupParticleSystemRegister() {
    int n = 0;
    RegisterRuntimeExport("?TypeName@AnimationGroup@Graphics@@UEBAPEBDXZ", (void*)&RtGraphics_AnimationGroup_TypeName); ++n;
    RegisterRuntimeAddr  (0x180016310ull, (void*)&RtGraphics_AnimationGroup_TypeName);
    RegisterRuntimeExport("?Type@AnimationGroup@Graphics@@UEBAIXZ", (void*)&RtGraphics_AnimationGroup_Type); ++n;
    RegisterRuntimeAddr  (0x1800162f0ull, (void*)&RtGraphics_AnimationGroup_Type);
    RegisterRuntimeExport("?GetFileVersion@AnimationGroup@Graphics@@UEBAHXZ", (void*)&RtGraphics_AnimationGroup_GetFileVersion); ++n;
    RegisterRuntimeAddr  (0x1800fb680ull, (void*)&RtGraphics_AnimationGroup_GetFileVersion);
    RegisterRuntimeExport("?Compile@AnimationGroup@Graphics@@UEAA?AW4CompileResult@External@Object@@AEBVFilename@System@@@Z",
                          (void*)&RtGraphics_AnimationGroup_Compile); ++n;
    RegisterRuntimeAddr  (0x1800fb690ull, (void*)&RtGraphics_AnimationGroup_Compile);
    RegisterRuntimeExport("?Serialize@AnimationGroup@Graphics@@UEAA_NAEAVStream@IO@@@Z", (void*)&RtGraphics_AnimationGroup_Serialize); ++n;
    RegisterRuntimeAddr  (0x1800fb6a0ull, (void*)&RtGraphics_AnimationGroup_Serialize);
    RegisterRuntimeAddr  (0x18005a040ull, (void*)&RtGraphics_AnimationGroup_SerializeClips); ++n;
    RegisterRuntimeExport("?Reconstruct@AnimationGroup@Graphics@@UEAAPEAVExternal@Object@@XZ", (void*)&RtGraphics_AnimationGroup_Reconstruct); ++n;
    RegisterRuntimeAddr  (0x180016550ull, (void*)&RtGraphics_AnimationGroup_Reconstruct);
    RegisterRuntimeExport("?Destruct@AnimationGroup@Graphics@@UEAAXXZ", (void*)&RtGraphics_AnimationGroup_Destruct); ++n;
    RegisterRuntimeAddr  (0x180016650ull, (void*)&RtGraphics_AnimationGroup_Destruct);
    RegisterRuntimeExport("?Construct@AnimationGroup@Graphics@@UEAAPEAVExternal@Object@@XZ", (void*)&RtGraphics_AnimationGroup_Construct); ++n;
    RegisterRuntimeAddr  (0x1800165d0ull, (void*)&RtGraphics_AnimationGroup_Construct);

    RegisterRuntimeExport("?TypeName@ParticleSystem@Graphics@@UEBAPEBDXZ", (void*)&RtGraphics_ParticleSystem_TypeName); ++n;
    RegisterRuntimeAddr  (0x1800203e0ull, (void*)&RtGraphics_ParticleSystem_TypeName);
    RegisterRuntimeExport("?Type@ParticleSystem@Graphics@@UEBAIXZ", (void*)&RtGraphics_ParticleSystem_Type); ++n;
    RegisterRuntimeAddr  (0x1800203c0ull, (void*)&RtGraphics_ParticleSystem_Type);
    RegisterRuntimeExport("?GetFileVersion@ParticleSystem@Graphics@@UEBAHXZ", (void*)&RtGraphics_ParticleSystem_GetFileVersion); ++n;
    RegisterRuntimeAddr  (0x1800a4d10ull, (void*)&RtGraphics_ParticleSystem_GetFileVersion);
    RegisterRuntimeExport("?Serialize@ParticleSystem@Graphics@@UEAA_NAEAVStream@IO@@@Z", (void*)&RtGraphics_ParticleSystem_Serialize); ++n;
    RegisterRuntimeAddr  (0x1800a4d20ull, (void*)&RtGraphics_ParticleSystem_Serialize);
    RegisterRuntimeExport("?Reconstruct@ParticleSystem@Graphics@@UEAAPEAVExternal@Object@@XZ", (void*)&RtGraphics_ParticleSystem_Reconstruct); ++n;
    RegisterRuntimeAddr  (0x180020560ull, (void*)&RtGraphics_ParticleSystem_Reconstruct);
    RegisterRuntimeExport("?Destruct@ParticleSystem@Graphics@@UEAAXXZ", (void*)&RtGraphics_ParticleSystem_Destruct); ++n;
    RegisterRuntimeAddr  (0x180020590ull, (void*)&RtGraphics_ParticleSystem_Destruct);
    RegisterRuntimeExport("?CreateSystem@ParticleSystem@Graphics@@UEBAPEAVParticleInstance@2@XZ", (void*)&RtGraphics_ParticleSystem_CreateSystem); ++n;
    RegisterRuntimeAddr  (0x1800a5140ull, (void*)&RtGraphics_ParticleSystem_CreateSystem);
    // VTABLE BATCH 7b — the real ctor, now a body rather than a thunk.
    RegisterRuntimeExport("??0ParticleSystem@Graphics@@QEAA@XZ", (void*)&RtGraphics_ParticleSystem_Ctor); ++n;
    RegisterRuntimeAddr  (0x1800a5320ull, (void*)&RtGraphics_ParticleSystem_Ctor);
    RegisterRuntimeExport("?Construct@ParticleSystem@Graphics@@UEAAPEAVExternal@Object@@XZ", (void*)&RtGraphics_ParticleSystem_Construct); ++n;
    RegisterRuntimeAddr  (0x180020580ull, (void*)&RtGraphics_ParticleSystem_Construct);
    return n;
}
