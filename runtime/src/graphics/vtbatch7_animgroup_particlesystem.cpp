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
//
// ---------------------------------------------------------------------------
// ENDGAME B.3.5 step 5, block H2 — what changed in the ParticleSystem half
// ---------------------------------------------------------------------------
// The file-local `struct ParticleSystem` is gone: the real
// `Graphics::ParticleSystem` (and its base `Graphics::Particle`) live in
// `runtime/types/graphics/Particle.h`, and all eight non-destructor slot bodies
// are MEMBERS. Every door above keeps its mangled name, its retail address and
// its registration and delegates with a QUALIFIED non-virtual call. The ctor
// door and the four hand-written vtable stamps STAY (B4b-2, B35_RULES section 11).
//
// *** THE SLOT ORDER IS RULE B5-16's ONE EXCEPTION IN THE WHOLE OF VIDEO AND
// GRAPHICS, AND IT IS WHY THIS CLASS HAS ITS OWN UNIT TEST. *** `Particle`
// introduces `CreateSystem` PURE at slot 8; `Construct` therefore enters the
// branch at slot 9, behind it. The slot list above is the byte-read order and it
// is the order the member declarations are in.
// The three hand-inlined int64 `ReaderWriter*` slot calls in `Serialize` became
// `IO::Serializer::Serialize(int64_t&)` member calls, closing this file's share
// of the nine sites B35_RULES section 11 deferred as "receiver-side work"
// (block H1 closed the fourth, in `AnimationGroup`'s clip loop).
// ============================================================================

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>

#include "../../funcs/rt_port.h"
#include "../../../standalone/resolve.h"
#include "../../types/collection/Collection.h"
#include "../../types/io/Serializer.h"
#include "../../types/object/External.h"
// ENDGAME B.3.5 step 5, block H1: the real `Graphics::AnimationGroup` and
// `Graphics::Animation`. The two file-local `struct AnimationGroup` /
// `struct AnimationClip` this file used to declare (batch 7's) are GONE —
// they moved into `runtime/types/graphics/AnimationGroup.h` and `Animation.h`
// under retail's own names, with the same offsets and the same asserts.
//
// GREP HAZARD (B4b-5 / B34_BRIEFS A.7), RESTATED FOR BLOCK H2: inside a
// `namespace Graphics { }` block in this file, a bare `Animation`,
// `AnimationGroup`, `AnimationEvent`, `Particle` OR `ParticleSystem` binds the
// REAL CLASS. Block H1's note said a bare `Particle` bound nothing; that was
// true only until block H2 added `Graphics::Particle` and
// `Graphics::ParticleSystem` to `Particle.h`. Every POD spelling inside such a
// block must be written `RtGraphics::`.
#include "../../types/graphics/AnimationGroup.h"
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

// The element type and the record are `Graphics::Animation` (0x70) and
// `Graphics::AnimationGroup` (0x58), declared in runtime/types/graphics/.
// Batch 7's two file-local structs used to sit here; they carried exactly these
// offsets and exactly these static_asserts, and the headers carry them now --
// plus the eleven retail accessor addresses block H1 disassembled in order to
// NAME each field instead of numbering it.
using AnimationClip  = ::Graphics::Animation;
using AnimationGroup = ::Graphics::AnimationGroup;

const uint8_t* AnimGroupTypeInfo() {
    return RT_DATA_SLOT(const uint8_t*, "?_typeInfo@AnimationGroup@Graphics@@0VBaseTypeInfo@Object@@A");
}

// ENDGAME B.3.9 block GFX: batch 7's `ConstructBody` (the Base / External /
// AnimationGroup stamps out of the MINT plus the field stores) is gone. The
// stores are the real `AnimationGroup()` (AnimationGroup.h) and the stamps the
// compiler's; Construct / Reconstruct placement-new the class.

// 0x1800594a0 — Object::ExternalPtr<AnimationEvent>::Serialize, ported as an
// appended thunk in external_ptr_serialize.cpp (shared 748-byte template).
extern "C" void RtObject_ExternalPtrSerialize_AnimationEvent(void** self, Serializer* ser);

// 0x18005a040 — `Array<Graphics::Animation>::Serialize`'s element loop, called
// by `AnimationGroup::Serialize` with (Serializer*, int count, Animation* data).
//
// IT STAYS A FREE FUNCTION. Retail 0x18005a040 is an unexported
// `Collection::Array<T>::Serialize` instantiation, not a member of either class,
// and runtime/src/collection/array.cpp owns that family. What changed in ENDGAME
// B.3.5 block H1 is the SPELLING: every `c->i30`-style position name is now the
// retail field name off `Graphics::Animation`, whose layout is pinned in its
// header against the eleven accessor bodies listed there.
extern "C" void RtGraphics_AnimationGroup_SerializeClips(Serializer* ser, int count, void* datav) {
    if (count <= 0) return;
    AnimationClip* c = static_cast<AnimationClip*>(datav);
    using SetSizeFn = void (*)(void* arr, int32_t n);
    using StrSerFn  = void (*)(void* str, Serializer* ser);
    for (int i = 0; i < count; ++i, ++c) {
        ReaderWriter* rw = ser->readerWriter;
        StrSerFn strSer = RtAddr<StrSerFn>(0x1800c9930ull);
        strSer(&c->_name, ser);
        strSer(&c->_meshName, ser);
        strSer(&c->_sourceName, ser);

        rw = ser->readerWriter;
        RwScalar(rw, rw_vtbl::kScalarInt, &c->_startFrame);
        rw = ser->readerWriter;
        RwScalar(rw, rw_vtbl::kScalarInt, &c->_endFrame);
        rw = ser->readerWriter;
        RwScalar(rw, rw_vtbl::kScalarFloat, &c->_rate);
        rw = ser->readerWriter;
        RwScalar(rw, rw_vtbl::kScalarBool, &c->_looping);
        rw = ser->readerWriter;
        RwScalar(rw, rw_vtbl::kScalarInt, &c->_loopFrame);
        rw = ser->readerWriter;
        RwScalar(rw, rw_vtbl::kScalarInt, &c->_instanceCount);
        rw = ser->readerWriter;
        RwScalar(rw, rw_vtbl::kScalarBool, &c->_instanced);

        // The nested Array<Animation::Event> at c->+0x00, stride 0x10.
        int32_t n = c->_events._size;
        rw = ser->readerWriter;
        RwScalar(rw, rw_vtbl::kScalarInt, &n);
        if (IsReading(ser->stream)) {
            static SetSizeFn setSize = nullptr;
            if (!setSize) setSize = RtAddr<SetSizeFn>(0x180059930ull);
            setSize(&c->_events, n);
        }
        uint8_t* e = c->_events._data;
        for (int32_t k = 0; k < n; ++k, e += 0x10) {
            rw = ser->readerWriter;
            RwScalar(rw, rw_vtbl::kScalarInt, e);
            RtObject_ExternalPtrSerialize_AnimationEvent(
                reinterpret_cast<void**>(e + 8), ser);
        }

        rw = ser->readerWriter;
        RwScalar(rw, rw_vtbl::kScalarInt, &c->_frameCount);
        rw = ser->readerWriter;
        RwScalar(rw, rw_vtbl::kScalarFloat, &c->_frameRate);
        rw = ser->readerWriter;
        RwScalar(rw, rw_vtbl::kScalarFloat, &c->_loopScale);
        rw = ser->readerWriter;
        RwScalar(rw, rw_vtbl::kScalarInt, &c->_boneCount);

        // Already-ported Array<Math::Transform>::Serialize (animationstate.cpp).
        RtGraphics_SerializeTransformArray(&c->_keys, ser);

        // ENDGAME B.3.5 block H1 — ONE OF THE NINE HAND-INLINED int64 SLOT CALLS
        // B35_RULES section 11 listed as "receiver-side work". Block H1 owns this
        // receiver (`Graphics::Animation::_duration`), and
        // `IO::Serializer::Serialize(int64_t&, const char*)` @ 0x180035000 is a
        // real member, so it is spelled as a member call. That member's own body
        // still calls the ReaderWriter's +0x90 slot (Serializer.h section 3.1:
        // the 22 members keep their raw slot calls until the minted tables
        // retire), so this is a spelling change and nothing else. The three int64
        // calls in `RtGraphics_ParticleSystem_Serialize` below are NOT touched:
        // that class is block H2's and its half of this file stays byte-identical.
        reinterpret_cast<IO::Serializer*>(ser)->Serialize(c->_duration, nullptr);  // B.3.5 bridge
    }
}

}  // namespace

// ############################################################################
// ENDGAME B.3.5 step 5, block H1 — `Graphics::AnimationGroup`'s member bodies.
//
// Their own `namespace Graphics { }` block, after the anonymous namespace above
// has closed, because a member definition has to appear in a namespace
// ENCLOSING the class and `Graphics` / `RtGraphics` are siblings (B34_BRIEFS
// section A.2). The anonymous namespace's helpers — `ConstructBody`,
// `CallSlot0`, `AnimGroupTypeInfo`, `MakeReaderWriter`, `RwScalar`, `RwFloats`,
// `IsReading`, `RtAddr` — are reachable from here unqualified: they are members
// of the global namespace's unnamed namespace, in this same translation unit.
//
// ALL EIGHT NON-DESTRUCTOR SLOT BODIES MOVED. ENDGAME B.3.9 block GFX: the
// class is concrete and every live AnimationGroup wears the compiler's table,
// so the eight slot doors that delegated into these members are gone with the
// minted `??_7AnimationGroup@Graphics@@6B@`.
// ############################################################################

namespace Graphics {

// [1] ?TypeName@AnimationGroup@Graphics@@UEBAPEBDXZ  0x180016310
//     reads `?_typeInfo@AnimationGroup@Graphics@@` + 8 (BaseTypeInfo::name).
const char* AnimationGroup::TypeName() const {
    const char* v = nullptr; std::memcpy(&v, AnimGroupTypeInfo() + 8, sizeof v); return v;
}

// [2] ?Type@AnimationGroup@Graphics@@UEBAIXZ  0x1800162f0 — the record's +0x00.
uint32_t AnimationGroup::Type() const {
    uint32_t v = 0; std::memcpy(&v, AnimGroupTypeInfo(), sizeof v); return v;
}

// [3] ?GetFileVersion@AnimationGroup@Graphics@@UEBAHXZ  0x1800fb680
//     `MOV EAX,8; RET`. EIGHT — the version every AnimationGroup in every save
//     was written with. B35_RULES section 6.1: `VertexLayout::GetFileVersion`
//     returning 1 instead of retail's 3 silently dropped every draw item once.
int32_t AnimationGroup::GetFileVersion() const { return 8; }

// [4] ?Compile@AnimationGroup@Graphics@@UEAA?AW4CompileResult@External@Object@@
//     AEBVFilename@System@@@Z  0x1800fb690. `MOV EAX,1; RET` — a no-op that
//     answers CompileResult::Success: an AnimationGroup loads pre-baked and has
//     no separate compile step. `path` is genuinely dead (RDX is never read).
//     The four-byte `?AW4` enum comes back in EAX with NO hidden return slot.
AnimationGroup::CompileResult AnimationGroup::Compile(const System::Filename& /*path*/) {
    return 1u;
}

// [8] ?Construct@AnimationGroup@Graphics@@UEAAPEAVExternal@Object@@XZ 0x1800165d0
//     the inlined constructor, i.e. placement construction. (Retail's null test
//     sat in the retired door; a virtual call always has a `this`.) `::new`:
//     the class declares its own, undefined, placement operator new.
Object::External* AnimationGroup::Construct() {
    return ::new (this) AnimationGroup();
}

// [6] ?Reconstruct@AnimationGroup@Graphics@@UEAAPEAVExternal@Object@@XZ 0x180016550
//     `vtable[0](this, 0)` then the same in-place construction. Slot 0 is called
//     VIRTUALLY on purpose — retail dispatches it, and a subclass's deleting
//     destructor is what has to run.
Object::External* AnimationGroup::Reconstruct() {
    DeletingDtor(0);
    return ::new (this) AnimationGroup();
}

// [7] ?Destruct@AnimationGroup@Graphics@@UEAAXXZ  0x180016650 -- `MOV RAX,[RCX]
//     ; XOR EDX,EDX ; JMP [RAX]`: slot 0, dispatched.
void AnimationGroup::Destruct() { DeletingDtor(0); }

// [5] ?Serialize@AnimationGroup@Graphics@@UEAA_NAEAVStream@IO@@@Z  0x1800fb6a0
//
// *** THE EMISSION ORDER IS RETAIL'S AND IT IS NOT THE DECLARATION ORDER. ***
// The +0x30 float goes out AFTER the tag array (see the comment at that call).
// Getting it wrong presents as a HANG, not a crash.
bool AnimationGroup::Serialize(void* streamv) {
    Stream* stream = static_cast<Stream*>(streamv);
    Serializer ser{};
    ReaderWriter* rw = MakeReaderWriter(&ser, stream);

    // +0x20 Array<Graphics::Animation>
    int32_t n = _clips._size;
    RwScalar(rw, rw_vtbl::kScalarInt, &n);
    if (IsReading(stream)) {
        using SetSizeFn = void (*)(void*, int32_t);
        RtAddr<SetSizeFn>(0x180059e70ull)(&_clips, n);
    }
    RtGraphics_AnimationGroup_SerializeClips(&ser, n, _clips._data);

    // +0x38 Array<{String,String,float}>, stride 0x18.
    n = _tags._size;
    rw = ser.readerWriter;
    RwScalar(rw, rw_vtbl::kScalarInt, &n);
    if (IsReading(stream)) {
        using SetSizeFn = void (*)(void*, int32_t);
        RtAddr<SetSizeFn>(0x18005a220ull)(&_tags, n);
    }
    {
        using StrSerFn = void (*)(void* str, Serializer* ser);
        StrSerFn strSer = RtAddr<StrSerFn>(0x1800c9930ull);
        uint8_t* p = _tags._data;
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
    RwScalar(rw, rw_vtbl::kScalarFloat, &_defaultBlend);

    // +0x48 Array<float>, bulk kArrayFloat.
    n = _blendTimes._size;
    rw = ser.readerWriter;
    RwScalar(rw, rw_vtbl::kScalarInt, &n);
    if (IsReading(stream)) {
        using SetSizeFn = void (*)(void*, int32_t);
        RtAddr<SetSizeFn>(0x18005a410ull)(&_blendTimes, n);
    }
    rw = ser.readerWriter;
    RwFloats(rw, n, _blendTimes._data);

    DestroyReaderWriter(ser.readerWriter);
    return true;
}

// ??1AnimationGroup@Graphics@@UEAA@XZ  0x1800fb620 -- retail's body, moved here
// from the retired `RtGraphics_AnimationGroup_Dtor` door
// (deleting_dtor_leaves.cpp, ENDGAME B.3.9 block GFX). The entry restamp is the
// compiler's, and so is the tail into ~External. The three Arrays go in
// retail's order, each through its own ~Array<T> instantiation BY ADDRESS
// (registry-first, the ported bodies in runtime/src/collection/):
//   0x18001c3b0(+0x48)  ~Array<float>                         (J1)
//   0x18001c2d0(+0x38)  ~Array<{String,String,float}>         (J2, two Strings each)
//   0x18001c1e0(+0x20)  ~Array<Graphics::Animation>           (each clip's ~Animation)
// Each frees AND zeroes its header, so the compiler-emitted ~Array<uint8_t> of
// the three members that runs after this body sees an empty header: no
// element destroy and MemFree(null) -- a no-op.
AnimationGroup::~AnimationGroup() {
    using ArrayDtorFn = void* (*)(void*);
    RtAddr<ArrayDtorFn>(0x18001c3b0ull)(&_blendTimes);
    RtAddr<ArrayDtorFn>(0x18001c2d0ull)(&_tags);
    RtAddr<ArrayDtorFn>(0x18001c1e0ull)(&_clips);
}

// ?TypeId@AnimationGroup@Graphics@@SAIXZ 0x180016300 -- the static twin of
// slot 2: the minted record's +0x00 until the W3 fold (B39_RULES D6 step 1).
uint32_t AnimationGroup::TypeId() {
    uint32_t v = 0; std::memcpy(&v, AnimGroupTypeInfo(), sizeof v); return v;
}

// ?CreateNewObject@AnimationGroup@Graphics@@SAPEAVBase@Object@@XZ 0x180016400
// -- `?MemAlloc@MemoryAllocator` (0x1800c7a50) of 0x58, align 8; retail's null
// branch; the inlined ctor. Replaces the FACTREST row (B39_RULES D2).
Object::Base* AnimationGroup::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(AnimationGroup), 8);
    if (p == nullptr) return nullptr;
    return ::new (p) AnimationGroup();
}
// ?CreateInPlaceObject@AnimationGroup@Graphics@@SAPEAVBase@Object@@PEAX@Z 0x1800164d0
Object::Base* AnimationGroup::CreateInPlaceObject(void* place) {
    if (place == nullptr) return nullptr;
    return ::new (place) AnimationGroup();
}

}  // namespace Graphics

// ENDGAME B.3.9 block GFX: AnimationGroup's eight slot doors
// (`RtGraphics_AnimationGroup_Type` .. `_Serialize`) are gone -- their only
// reader was the minted table. The Array<Animation>::Serialize helper
// 0x18005a040 stays registered (a by-address callee, not a slot).

// ============================================================================
// Graphics::ParticleSystem
// ============================================================================
namespace {

// ENDGAME B.3.5 step 5, block H2: batch 7's file-local `struct ParticleSystem`
// USED TO SIT HERE. It is GONE — it moved into
// `runtime/types/graphics/Particle.h` as the real `Graphics::ParticleSystem`,
// under retail's own name, with the same offsets, the same `static_assert`s and
// every position name (`f4c`, `f78`, `fac`, …) replaced by the name
// `wave28_lane_partsys.cpp` measured for the same offset as a READER. The only
// fields that keep a position name are the ones no reader has identified. See
// that header's banner for the two-witness argument.
using ParticleSystem = ::Graphics::ParticleSystem;

const uint8_t* ParticleSystemTypeInfo() {
    return RT_DATA_SLOT(const uint8_t*, "?_typeInfo@ParticleSystem@Graphics@@0VBaseTypeInfo@Object@@A");
}

}  // namespace

// ENDGAME B.3.5 block H2: ALL EIGHT NON-DESTRUCTOR SLOT BODIES MOVED onto the
// members further down. Every registered door below keeps its mangled name AND
// its retail address and is now a one-line QUALIFIED, non-virtual delegation.
// THE QUALIFICATION IS LOAD-BEARING (B34_BRIEFS A.1): a live `ParticleSystem`
// wears the MINTED `??_7ParticleSystem@Graphics@@6B@`, whose slots resolve back
// to these very doors, so an unqualified virtual call inside a door would
// dispatch into itself instead of delegating.
extern "C" uint32_t RtGraphics_ParticleSystem_Type(const void* selfv) {
    return reinterpret_cast<const ::Graphics::ParticleSystem*>(selfv)     // B.3.5 bridge
               ->::Graphics::ParticleSystem::Type();
}
extern "C" const char* RtGraphics_ParticleSystem_TypeName(const void* selfv) {
    return reinterpret_cast<const ::Graphics::ParticleSystem*>(selfv)     // B.3.5 bridge
               ->::Graphics::ParticleSystem::TypeName();
}
extern "C" int32_t RtGraphics_ParticleSystem_GetFileVersion(const void* selfv) {
    return reinterpret_cast<const ::Graphics::ParticleSystem*>(selfv)     // B.3.5 bridge
               ->::Graphics::ParticleSystem::GetFileVersion();
}

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

// THE FOUR VTABLE STAMPS STAY HAND-WRITTEN AND THEY STAY POINTED AT THE MINT —
// the same decision, for the same reason, as `AnimGroupConstructBody` above
// (B34_RULES section 4.3): `Construct` and `Reconstruct` run on an object that
// ALREADY EXISTS and that the rest of the tree expects to be wearing
// `??_7ParticleSystem@Graphics@@6B@` out of `rt_data_mint_tables.inc`. A member
// constructor here would stamp GCC's tables instead, which is why B4b-2 keeps the
// ctor door. The fields are spelled through `Graphics::ParticleSystem` now; the
// STORES are byte for byte batch 7b's.
void ParticleSystemConstructBody(ParticleSystem* self) {
    void** vp = reinterpret_cast<void**>(self);
    *vp = BaseVTable();                                   // 1800a5338  0x180154788
    *vp = ExternalVTable();                               // 1800a5342  0x1801547a0
    self->_path.data      = nullptr;                      // 1800a5350  +0x08
    self->_refs._data     = nullptr;                      // 1800a5353  +0x10
    self->_refs._size     = 0;                            // 1800a5357  +0x18
    self->_refs._capacity = 0;                            //            +0x1c
    *vp = ParticleVTable();                               // 1800a5362  0x180155130
    self->_active     = 1;                                // 1800a5365  byte
    self->_recordCount = 1;                               // 1800a5369
    self->_subCount   = 0xf;                              // 1800a5370
    self->_material   = nullptr;                          // 1800a5377
    self->_records._data     = nullptr;                   // 1800a537b
    self->_records._size     = 0;                         // 1800a537f
    self->_records._capacity = 0;                         //            one qword with _size
    *vp = ParticleSystemVTable();                         // 1800a538a  0x180155090

    const double spt = SecondsPerTick();

    self->_f48 = 5.0f;  self->_rateAccelA = 0.0f;         // 1800a538d  one qword
    self->_rateAccelB = 1.0f;                             // 1800a5395
    self->_durationTicks = static_cast<int64_t>(-1.0 / spt); // 1800a539c..b1
    self->_emitDirX = 0.0f; self->_emitDirY = 0.0f;       // 1800a53b5  one qword
    self->_emitDirZ = 1.0f;                               // 1800a53b9
    self->_spreadX  = 1.0f;                               // 1800a53c0
    self->_spreadY  = 1.0f;                               // 1800a53c7
    self->_spreadZ  = 1.0f; self->_spreadScale = 0.0f;    // 1800a53ce  one qword
    self->_angleBRange = 0.0f; self->_angleARange = 0.0f; // 1800a53d6  one qword
    self->_speedBase = 1.0f; self->_speedRange = 0.0f;    // 1800a53da  one qword
    self->_accelScale = 0.0f;                             // 1800a53e5
    self->_damping = 1.0f; self->_rotBase = 0.0f;         // 1800a53eb  one qword
    self->_rotRange = 0.0f; self->_spinBase = 0.0f;       // 1800a53f6  one qword
    self->_spinRange = 0.0f; self->_spinAccel = 0.0f;     // 1800a53fd  one qword
    self->_spinDamp = 1.0f;                               // 1800a5404
    self->_lifeBase = 3.0f; self->_lifeRange = 0.0f;      // 1800a540e  one qword
    self->_gravScale = 1.0f; self->_gravX = 0.0f;         // 1800a5419  one qword
    self->_gravY = 0.0f;                                  // 1800a5424
    self->_gravZ = -9.8f;                                 // 1800a542a  0xc11ccccd
    self->_sizeBase = 0.5f; self->_sizeRange = 0.0f;      // 1800a5434  one qword
    self->_sizeAccel = 0.0f;                              // 1800a543f
    self->_sizeDamp = 1.0f; self->_colorT0 = 0.0f;        // 1800a5445  one qword
    self->_colorT1 = 1.0f;                                // 1800a5450
    self->_color0 = 0xffffffffu;                          // 1800a545a  one qword
    self->_color1 = 0xffffffffu;                          //            of -1
    self->_fadeInTicks  = static_cast<int64_t>(0.25 / spt); // 1800a5465..7d
    self->_fadeOutTicks = static_cast<int64_t>(0.25 / spt); // 1800a5484..91
    // 1800a5498 / 1800a549f — two qword stores over +0xf8 and +0x100, i.e. the
    // `_pool` Array header zeroed. Batch 7b spelled it `memset(tail, 0, 0x10)`
    // over an unmodelled byte array; the three named fields are the same bytes.
    self->_pool._data     = nullptr;                      // 1800a5498  +0xf8
    self->_pool._size     = 0;                            // 1800a549f  +0x100
    self->_pool._capacity = 0;                            //            +0x104
}

}  // namespace

extern "C" void* RtGraphics_ParticleSystem_Ctor(void* selfv) {
    // Retail has NO null check (`MOV [RCX],RAX` on the first instruction).
    ParticleSystemConstructBody(static_cast<ParticleSystem*>(selfv));
    return selfv;                                          // 1800a54a6  MOV RAX,RCX
}

// Construct/Reconstruct/Destruct are vtable slots [9]/[6]/[7]; each is retail's
// `JMP 0x1800a5320` (Construct) / `vtable[0](this,0)` then the same ctor
// (Reconstruct) / a tail call to `vtable[0](this,0)` (Destruct). ENDGAME B.3.5
// block H2: the bodies moved onto the members and each door is a one-line
// QUALIFIED delegation. The retail null test in `Construct` stays IN THE DOOR
// (B35 class-block brief).
extern "C" void* RtGraphics_ParticleSystem_Construct(void* selfv) {
    if (!selfv) return nullptr;
    return reinterpret_cast<::Graphics::ParticleSystem*>(selfv)           // B.3.5 bridge
               ->::Graphics::ParticleSystem::Construct();
}
extern "C" void* RtGraphics_ParticleSystem_Reconstruct(void* selfv) {
    return reinterpret_cast<::Graphics::ParticleSystem*>(selfv)           // B.3.5 bridge
               ->::Graphics::ParticleSystem::Reconstruct();
}
extern "C" void RtGraphics_ParticleSystem_Destruct(void* selfv) {
    reinterpret_cast<::Graphics::ParticleSystem*>(selfv)                  // B.3.5 bridge
        ->::Graphics::ParticleSystem::Destruct();
}

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
// ENDGAME B.3.5 block H2: BODY MOVED onto the member. *** THIS IS SLOT 8, NOT
// SLOT 9 — RULE B5-16's trap. *** `Particle` introduced `CreateSystem` PURE at
// slot 8, so `Construct` sits one slot further along, at 9, on this class alone.
// The return type is `ParticleInstance*`, NOT `Object::External*`; swapping the
// two slots would produce a `CreateSystem` that hands callers an `External*`
// through the right vtable index, which is silent.
extern "C" void* RtGraphics_ParticleSystem_CreateSystem(void* selfv) {
    return reinterpret_cast<const ::Graphics::ParticleSystem*>(selfv)      // B.3.5 bridge
               ->::Graphics::ParticleSystem::CreateSystem();
}

extern "C" bool RtGraphics_ParticleSystem_Serialize(void* selfv, void* streamv) {
    return reinterpret_cast<::Graphics::ParticleSystem*>(selfv)            // B.3.5 bridge
               ->::Graphics::ParticleSystem::Serialize(streamv);
}

// ############################################################################
// ENDGAME B.3.5 step 5, block H2 — `Graphics::ParticleSystem`'s member bodies.
//
// Their own `namespace Graphics { }` block, after the anonymous namespace above
// has closed, because a member definition has to appear in a namespace ENCLOSING
// the class and `Graphics` / `RtGraphics` are siblings (B34_BRIEFS section A.2).
// The anonymous namespace's helpers — `ParticleSystemConstructBody`, `CallSlot0`,
// `ParticleSystemTypeInfo`, `MakeReaderWriter`, `RwScalar`, `RwFloats`,
// `IsReading`, `RtAddr` — are reachable from here unqualified.
//
// `Graphics::Particle` GETS NO BODIES HERE. Its `Serialize` and its three
// type-info forwarders are in `wave27_lane_gfxpart.cpp` (also block H2's), which
// is where that slot has always lived.
// ############################################################################

namespace Graphics {

// [1] ?TypeName@ParticleSystem@Graphics@@UEBAPEBDXZ  0x1800203e0
//     reads `?_typeInfo@ParticleSystem@Graphics@@` + 8 (BaseTypeInfo::name).
const char* ParticleSystem::TypeName() const {
    const char* v = nullptr; std::memcpy(&v, ParticleSystemTypeInfo() + 8, sizeof v); return v;
}

// [2] ?Type@ParticleSystem@Graphics@@UEBAIXZ  0x1800203c0 — the record's +0x00.
uint32_t ParticleSystem::Type() const {
    uint32_t v = 0; std::memcpy(&v, ParticleSystemTypeInfo(), sizeof v); return v;
}

// [3] ?GetFileVersion@ParticleSystem@Graphics@@UEBAHXZ  0x1800a4d10.
//     0x1001 == 4097, and it is NOT `Particle`'s 0x1000 — the two were decoded
//     separately and the difference is why each is its own literal (B35_RULES
//     section 6.1: a wrong GetFileVersion silently drops content).
int32_t ParticleSystem::GetFileVersion() const { return 0x1001; }

// [9] ?Construct@ParticleSystem@Graphics@@UEAAPEAVExternal@Object@@XZ 0x180020580
//     `JMP 0x1800a5320` — the constructor body, in place. SLOT NINE.
Object::External* ParticleSystem::Construct() {
    ParticleSystemConstructBody(this);
    return this;
}

// [6] ?Reconstruct@ParticleSystem@Graphics@@UEAAPEAVExternal@Object@@XZ 0x180020560
//     `vtable[0](this, 0)` then the same in-place construction. Slot 0 is called
//     VIRTUALLY on purpose — retail dispatches it, and a subclass's deleting
//     destructor is what has to run.
Object::External* ParticleSystem::Reconstruct() {
    CallSlot0(this, 0);
    ParticleSystemConstructBody(this);
    return this;
}

// [7] ?Destruct@ParticleSystem@Graphics@@UEAAXXZ  0x180020590
void ParticleSystem::Destruct() { CallSlot0(this, 0); }

// ??1ParticleSystem@Graphics@@UEAA@XZ  0x1800a51c0 — NOT ported anywhere in the
// tree and not started here. EMPTY for `~AnimationGroup`'s reason: the retail
// body stamps the MINTED table, which is right for a minted object and wrong for
// a converted one, and a real destructor chain emits GCC's stamps for free.
ParticleSystem::~ParticleSystem() {}

// [8] ?CreateSystem@ParticleSystem@Graphics@@UEBAPEAVParticleInstance@2@XZ
//     retail 0x1800a5140. `this` IS NEVER READ (see the banner above the door):
//     retail ignores the ParticleSystem entirely and the returned instance is
//     bound to its system later, by the caller.
ParticleInstance* ParticleSystem::CreateSystem() const {
    void* inst = rtcoll::MemAlloc(0xe0, 8);                // 1800a5183
    if (!inst) return nullptr;                             // 1800a519c -> XOR EAX,EAX
    // 0x1800a55a0 (`Graphics::ParticleInstance::ParticleInstance`) is NOT this
    // block's row and is reached BY ADDRESS — ResolveAddr consults the port
    // registry first, so it lands on `animation.cpp`'s ported ctor door. The
    // result is reinterpreted rather than static_cast because `ParticleInstance`
    // is only FORWARD-DECLARED in `Particle.h`: including `ParticleInstance.h`
    // here would drag `Geometry.h` -> `Volume.h` into a translation unit that has
    // already seen `<windows.h>`, which is the `Rectangle` collision block E
    // measured.
    using CtorFn = void* (*)(void*);
    return reinterpret_cast<ParticleInstance*>(
        RtAddr<CtorFn>(0x1800a55a0ull)(inst));             // 1800a51a1
}

// [5] ?Serialize@ParticleSystem@Graphics@@UEAA_NAEAVStream@IO@@@Z  0x1800a4d20.
//
// *** THE EMISSION ORDER IS RETAIL'S AND IT IS NOT THE DECLARATION ORDER. ***
// `_spreadScale` (+0x78) goes out BEFORE `_spreadX/Y/Z` (+0x6c), which is why the
// header declares the fields in ascending memory order and the order lives here.
bool ParticleSystem::Serialize(void* streamv) {
    Stream* stream = static_cast<Stream*>(streamv);

    // `Particle::Serialize` (0x180103f00) — the base slot, reached BY ADDRESS
    // exactly as retail's `CALL` does. It IS ported now, as
    // `Graphics::Particle::Serialize` in `wave27_lane_gfxpart.cpp` (also block
    // H2's file), and ResolveAddr consults the port registry first
    // (reference_resolveaddr_consults_port_registry_first.md), so this call site
    // lands on that member's door with no edit here. It is deliberately NOT
    // rewritten as `Particle::Serialize(streamv)`: retail's own instruction is an
    // indirect call at that address, and a direct qualified call would bypass the
    // registry — the one place where the door, not the C++ base, decides.
    using BaseSerFn = void (*)(void*, void*);
    RtAddr<BaseSerFn>(0x180103f00ull)(this, stream);

    Serializer ser{};
    ReaderWriter* rw = MakeReaderWriter(&ser, stream);

    RwScalar(rw, rw_vtbl::kScalarFloat, &_f48);
    rw = ser.readerWriter; RwScalar(rw, rw_vtbl::kScalarFloat, &_rateAccelA);
    rw = ser.readerWriter; RwScalar(rw, rw_vtbl::kScalarFloat, &_rateAccelB);
    // ENDGAME B.3.5 block H2 — THE FIRST OF THE THREE HAND-INLINED int64 SLOT
    // CALLS B35_RULES section 11 listed in this file as "receiver-side work".
    // Block H2 owns this receiver, and `IO::Serializer::Serialize(int64_t&, const
    // char*)` @ 0x180035000 is a real member, so it is spelled as a member call.
    // That member's own body still calls the ReaderWriter's +0x90 slot
    // (Serializer.h section 3.1: the 22 members keep their raw slot calls until
    // the minted tables retire), so this is a spelling change and nothing else.
    // The cast is the bridge from the POD `rt::io::Serializer` this body builds on
    // the stack to the real class; block H1 converted its one site the same way.
    reinterpret_cast<IO::Serializer*>(&ser)->Serialize(_durationTicks, nullptr);  // B.3.5 bridge
    rw = ser.readerWriter; RwFloats(rw, 3, &_emitDirX);
    rw = ser.readerWriter; RwScalar(rw, rw_vtbl::kScalarFloat, &_spreadScale); // retail's order: 0x78 before 0x6c
    rw = ser.readerWriter; RwFloats(rw, 3, &_spreadX);

    // The fifteen consecutive float scalars, +0x7c..+0xb4. Written out one field
    // at a time rather than walked as `float*`: retail emits fifteen separate
    // scalar calls, the names are what a reader needs, and `Particle.h` carries a
    // `static_assert` that the run is still contiguous.
    rw = ser.readerWriter; RwScalar(rw, rw_vtbl::kScalarFloat, &_angleBRange);
    rw = ser.readerWriter; RwScalar(rw, rw_vtbl::kScalarFloat, &_angleARange);
    rw = ser.readerWriter; RwScalar(rw, rw_vtbl::kScalarFloat, &_speedBase);
    rw = ser.readerWriter; RwScalar(rw, rw_vtbl::kScalarFloat, &_speedRange);
    rw = ser.readerWriter; RwScalar(rw, rw_vtbl::kScalarFloat, &_accelScale);
    rw = ser.readerWriter; RwScalar(rw, rw_vtbl::kScalarFloat, &_damping);
    rw = ser.readerWriter; RwScalar(rw, rw_vtbl::kScalarFloat, &_rotBase);
    rw = ser.readerWriter; RwScalar(rw, rw_vtbl::kScalarFloat, &_rotRange);
    rw = ser.readerWriter; RwScalar(rw, rw_vtbl::kScalarFloat, &_spinBase);
    rw = ser.readerWriter; RwScalar(rw, rw_vtbl::kScalarFloat, &_spinRange);
    rw = ser.readerWriter; RwScalar(rw, rw_vtbl::kScalarFloat, &_spinAccel);
    rw = ser.readerWriter; RwScalar(rw, rw_vtbl::kScalarFloat, &_spinDamp);
    rw = ser.readerWriter; RwScalar(rw, rw_vtbl::kScalarFloat, &_lifeBase);
    rw = ser.readerWriter; RwScalar(rw, rw_vtbl::kScalarFloat, &_lifeRange);
    rw = ser.readerWriter; RwScalar(rw, rw_vtbl::kScalarFloat, &_gravScale);

    rw = ser.readerWriter; RwFloats(rw, 3, &_gravX);

    rw = ser.readerWriter; RwScalar(rw, rw_vtbl::kScalarFloat, &_sizeBase);
    rw = ser.readerWriter; RwScalar(rw, rw_vtbl::kScalarFloat, &_sizeRange);
    rw = ser.readerWriter; RwScalar(rw, rw_vtbl::kScalarFloat, &_sizeAccel);
    rw = ser.readerWriter; RwScalar(rw, rw_vtbl::kScalarFloat, &_sizeDamp);

    rw = ser.readerWriter; RwScalar(rw, rw_vtbl::kScalarUInt, &_color0);
    rw = ser.readerWriter; RwScalar(rw, rw_vtbl::kScalarUInt, &_color1);
    rw = ser.readerWriter; RwScalar(rw, rw_vtbl::kScalarFloat, &_colorT0);
    rw = ser.readerWriter; RwScalar(rw, rw_vtbl::kScalarFloat, &_colorT1);
    // The second and third int64 sites — same conversion, same reason.
    reinterpret_cast<IO::Serializer*>(&ser)->Serialize(_fadeInTicks, nullptr);   // B.3.5 bridge
    reinterpret_cast<IO::Serializer*>(&ser)->Serialize(_fadeOutTicks, nullptr);  // B.3.5 bridge

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
    if (IsReading(stream) && _active != 0) {
        using SetSizeFn = void (*)(void*, int32_t);
        RtAddr<SetSizeFn>(0x180021220ull)(&_pool, _recordCount);        // 1800a507a
        uint8_t** poolArr = reinterpret_cast<uint8_t**>(_pool._data);   // 1800a50f1
        SetSizeFn innerSetSize = RtAddr<SetSizeFn>(0x180020e90ull);
        for (int32_t i = 0; i < _recordCount; ++i) {                    // 1800a5103
            uint8_t* rec = static_cast<uint8_t*>(rtcoll::MemAlloc(0x18, 8));
            if (rec) {                                                  // 1800a50d4
                std::memset(rec, 0, 0x10);                              // 1800a50d8/db
                std::memcpy(rec + 0x10, &_f48, 4);                      // 1800a50df/e2
                innerSetSize(rec, _subCount);                           // 1800a50eb
            }
            // Retail stores RDI unconditionally, so a failed allocation parks a
            // null in the slot rather than skipping the index.
            poolArr[i] = rec;                                           // 1800a50f8
        }
    }

    DestroyReaderWriter(ser.readerWriter);
    return true;
}

}  // namespace Graphics

int RtGraphicsBatch7AnimGroupParticleSystemRegister() {
    int n = 0;
    // AnimationGroup: the factory pair (B39_RULES D2), the class's own statics,
    // by name AND address; the minted `?_typeInfo@AnimationGroup` record
    // resolves them. The slot doors are retired (above).
    RegisterRuntimeExport("?CreateNewObject@AnimationGroup@Graphics@@SAPEAVBase@Object@@XZ",
                          (void*)&::Graphics::AnimationGroup::CreateNewObject); ++n;
    RegisterRuntimeAddr  (0x180016400ull, (void*)&::Graphics::AnimationGroup::CreateNewObject);
    RegisterRuntimeExport("?CreateInPlaceObject@AnimationGroup@Graphics@@SAPEAVBase@Object@@PEAX@Z",
                          (void*)&::Graphics::AnimationGroup::CreateInPlaceObject); ++n;
    RegisterRuntimeAddr  (0x1800164d0ull, (void*)&::Graphics::AnimationGroup::CreateInPlaceObject);
    RegisterRuntimeAddr  (0x18005a040ull, (void*)&RtGraphics_AnimationGroup_SerializeClips); ++n;

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
