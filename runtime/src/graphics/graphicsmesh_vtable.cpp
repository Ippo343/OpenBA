// ============================================================================
// runtime/src/graphics/graphicsmesh_vtable.cpp
// ??_7GraphicsMesh@Graphics@@6B@ (0x180154cc0) — all eight non-slot-0 slots,
// the constructor, the destructor, and the one nested Serialize underneath
// them.  Vtable batch 6.
// ============================================================================
//
//   slot 1  ?TypeName@         0x180012390    slot 6  ?Reconstruct@  0x180012510
//   slot 2  ?Type@             0x180012370    slot 7  ?Destruct@     0x180012540
//   slot 3  ?GetFileVersion@   0x1800ff3a0    slot 8  ?Construct@    0x180012530
//   slot 5  ?Serialize@        0x1800ff3b0
//   slot 0  the vector deleting destructor 0x180012bd0 — a ROW in
//           runtime/src/object/deleting_dtor.cpp, not a body here.
//
//   ??0GraphicsMesh@Graphics@@QEAA@XZ   0x1800ff170
//   ??1GraphicsMesh@Graphics@@UEAA@XZ   0x1800ff270
//   GraphicsMesh::TriangleList::Serialize  0x1800a4670  (unexported; see §T)
//
// Everything below was read from the LISTING at its retail VA. The four
// Collection::Array<T> instantiations this class needs are NOT here — they are
// runtime/src/collection/array_graphics_mesh.cpp's, so that all Array bodies
// stay in one place and share array.cpp's `SetSizeCore` / `ArrayDtorImpl`.
// ============================================================================

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <new>

#include "../../funcs/rt_port.h"
#include "../../types/io/Serializer.h"
#include "../../types/graphics/GraphicsMesh.h"   // the real Graphics::GraphicsMesh
#include "../../types/video/Device.h"            // ENDGAME B.3.9 SS1: Video::Device::_singleton
#include "../../types/graphics/AnimationState.h" // rtcoll::Rt_SetSize_18005aa30 / RtGraphics_SerializeMeshNodes
#include "../../types/graphics/Mesh.h"           // RtGraphics_EdgeMesh_SerializeTriangleListArray
#include "../../types/collection/Collection.h"   // rtcoll:: the array instantiations
#include "../../types/object/External.h"         // the ExternalPtr serializers
#include "../../types/system/String.h"
#include "../../types/system/MemoryAllocator.h"

using rt::io::Encoding;
using rt::io::ReaderWriter;
using rt::io::Serializer;
using rt::io::Stream;

namespace {

// ---------------------------------------------------------------------------
// GRAPHICSMESH'S LAYOUT — the eleven `constexpr uint32_t k*` offsets that used
// to live here MOVED to `runtime/types/graphics/GraphicsMesh.h` as the real
// `Graphics::GraphicsMesh` class's named fields (ENDGAME B.3.5 block G), and
// with them the `+0x50 Array<TriangleList>` MISNOMER this file carried.
//
// *** +0x50 IS NOT AN `Array<TriangleList>`. *** The element is 0xb0 bytes and
// it is the DETAIL LEVEL: eight exported getters index it by
// `IMUL R8,R8,0xb0` and name eight of its fields — a sub-mesh `String` at +0x00
// (`?GetSubMeshName@` 0x180012770), an `ExternalPtr<Video::ImageBuffer>` at
// +0x08 (`?GetImage@` 0x1800126e0), a distance at +0x10, FOUR
// `VideoBufferHeader`s at +0x18/+0x38/+0x58/+0x78 (`?Get{,Skin}{Vertex,Index}
// Buffer@`), an `Array<TriangleList>` at +0x98 (`?GetTriangleLists@`
// 0x180012640, whose mangled return type is what names the element) and a
// max-instance count at +0xa8. The `Array<TriangleList>` is INSIDE the detail
// level, not the +0x50 array itself, and the 0x18-byte `TriangleList` is the
// six-dword record `?Serialize@TriangleList@GraphicsMesh@Graphics@@`
// (0x1800ff300) emits. `Mesh.h`'s `GraphicsMeshView` had it right all along
// (`_detailLevels` at +0x50); this file's name for it did not. The header
// records both readings and the disassembly behind each.
//
// The four sub-objects `TriangleListPass` below dispatches through — +0x40,
// +0x20, +0x80, +0x60 of the record — are exactly the `_deviceData` qwords at
// `+0x08` of those same four buffers, which is the independent cross-check that
// fixes the whole 0xb0 layout.
// ---------------------------------------------------------------------------
using Graphics::GraphicsMesh;

// ENDGAME B.3.9 block GFX: the ctor's bit-pattern constants and the four
// minted vtable addresses the ctor / dtor doors stamped are gone with those
// doors; the real `GraphicsMesh()` (GraphicsMesh.h) writes the same bits and
// the compiler stamps the tables.

// ---------------------------------------------------------------------------
// `?_typeInfo@GraphicsMesh@Graphics@@0VBaseTypeInfo@Object@@A` — 0x1801fa708,
// MINTED and PROMOTED (standalone/reg_mint_promoted.cpp:936). Read through the
// name for the reason graphics_typeinfo_accessors.cpp gives.
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// Named dependencies — RT_EXPORT_ONCE, so a ported one is reached as ours.
// ---------------------------------------------------------------------------
using VoidThisFn  = void  (*)(void*);
using PtrThisFn   = void* (*)(void*);
using SerMemberFn = void  (*)(void*, Serializer*);

// `?Serialize@String@System@@QEAAXAEAVSerializer@IO@@@Z` — retail 0x1800c9930,
// the member System::String::Serialize. B.3.x bridge: the field is a String.
void StringSerialize(void* str, Serializer* ser) {
    static_cast<System::String*>(str)->Serialize(ser);
}

// `?PlatformSerialize@GraphicsMesh@Graphics@@QEAAXAEAVSerializer@IO@@@Z` —
// retail 0x1800a48e0. **PORTED** (runtime/src/graphics/batchc_gfx_leaves.cpp,
// BATCH C 2026-09-12) and reached BY NAME because it IS exported (`QEAA`,
// public). This comment used to say "NOT PORTED ... it stays on the DLL until
// that [Video] side is ported ... the one dependency in this file that the
// cutover still needs": the body is two device-interface arms of the shape
// runtime/src/video/shaderprogram.cpp already spelled out, and every helper it
// drives (0x1800136f0, 0x180014850) was already ported.
// RENAMED `PlatformSerializeDoor` by ENDGAME B.3.5 block G: the real class now
// DECLARES `Graphics::GraphicsMesh::PlatformSerialize`, so a helper of the same
// bare name inside `namespace Graphics`'s member bodies would bind the MEMBER
// silently — exactly the name-lookup trap B34_BRIEFS A.2 records. The door it
// opens is unchanged.
// ENDGAME B.3.5 caller batch: `Graphics::GraphicsMesh::PlatformSerialize` is a
// DEFINED member (batchc_gfx_leaves.cpp:789), so the by-name door is a member
// call. The name stays `PlatformSerializeDoor` for the lookup reason above.
void PlatformSerializeDoor(void* self, Serializer* ser) {
    reinterpret_cast<::Graphics::GraphicsMesh*>(self)
        ->::Graphics::GraphicsMesh::PlatformSerialize(
            *reinterpret_cast<IO::Serializer*>(ser));          // B.3.5 bridge
}

// ---------------------------------------------------------------------------
// The unexported Collection helpers this class's Serialize drives. All are
// template instantiations with no export name in either shipped build; each is
// called by its body name (B3R).
//
//   0x18005d840  Array<TriangleList>::SetSize(n)          array_graphics_mesh.cpp (THIS batch)
//   0x18005cda0  Array<Array<T>>::SetSize(n)              array_graphics_mesh.cpp (THIS batch)
//   0x18005aa30  Array<GraphicsMesh::Node>::SetSize(n)    array_graphics_anim.cpp (batch 3)
//   0x1800136f0  Array<T>::SetSize(n) for TriangleList's index array  (already ported)
//   0x18005d0f0  a per-member Serialize helper            (already ported)
//   0x18005cc60  Array<T>::SetSize(n) for the inner array (already ported)
//   0x18005d4a0  an array-of-T Serialize helper           (already ported)
//   0x18005c120  the Array<Node> Serialize helper         (already ported)
//   0x180014850  EdgeMesh triangle-list array Serialize   (closure batch 2b)
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// The two GraphicsMesh file-scope statics that carry context into
// `TriangleList::Serialize`.
//
//   0x1801fc69c  int   — a counter. `GraphicsMesh::Serialize` zeroes it before
//                        the sub-objects run; `TriangleList::Serialize`
//                        INCREMENTS it (0x1800a46aa, its only reader-writer);
//                        `GraphicsMesh::PlatformVersion` (0x1800a4cd0) also
//                        writes it.
//   0x1801fc6a0  void* — the GraphicsMesh currently being serialized. Written
//                        by the same two functions and by nothing else
//                        (get_xrefs_to, both addresses, retail).
//
// ENDGAME B.3.9 block GFX (B39_RULES D7, rows A6 / A7): FILE STATICS. Both
// were UNEXPORTED DATA reached by address through the mint's by-address
// storage rows; this file holds every site (`PlatformVersion`, the third
// retail writer, is unported and has no caller), so the two words live here
// and the two mint rows are retired (rt_mint_retired/gfx.txt). Zero-initialised
// by the language, as retail's .data words are. The lazy function-local
// ResolveAddr caches are gone with them.
// ---------------------------------------------------------------------------
int32_t g_serializeCounter     = 0;         // retail 0x1801fc69c
void*   g_serializeCurrentMesh = nullptr;   // retail 0x1801fc6a0
int32_t* SerializeCounter()     { return &g_serializeCounter; }
void**   SerializeCurrentMesh() { return &g_serializeCurrentMesh; }

// `?_singleton@Device@Video@@0PEAV12@EA` — 0x1801fe7a8, a POINTER to the
// Device, so reading +0xf8 is a double indirection.
const uint8_t* VideoDevice() {
    // ENDGAME B.3.9 SS1 (B39_RULES D7): the class static now, read fresh.
    return reinterpret_cast<const uint8_t*>(::Video::Device::_singleton);
}

// ---------------------------------------------------------------------------
// Serializer prologue/epilogue. IDENTICAL to imagebuffer_derived.cpp's, EXCEPT
// FOR ONE ARGUMENT that is the whole point of this class's format:
//
//     *** THE VERSION IS 1 HERE, NOT 0. ***
//
// `LEA R8D,[RBX+0x1]` at 0x1800ff3df (EBX has just been zeroed) puts 1 in the
// factory's third argument, where Video::ImageBuffer::Serialize puts 0. On the
// WRITING path BinaryWriter stores that byte and emits it, so a file this build
// writes declares version 1; on the READING path BinaryReader overwrites it
// with what the file says. Two branches below then key off
// `ReaderWriter::GetVersion()` — one runs the OLD inline triangle data when the
// version is 0, the other runs `PlatformSerialize` when it is >= 1. Passing 0
// here would make every file this build writes claim to be the old format.
// ---------------------------------------------------------------------------
ReaderWriter* MakeCodec(Stream* stream, Serializer* ser, int version) {
    ser->stream = stream;
    ser->readerWriter = nullptr;
    Encoding* enc = stream->encoding;
    void** encVt = enc->vtable;
    const int slot = (stream->direction == rt::io::kDirectionReading)
                         ? rt::io::encoding_vtbl::kCreateReader
                         : rt::io::encoding_vtbl::kCreateWriter;
    using FactoryFn = ReaderWriter* (*)(Encoding*, Serializer*, int);
    ReaderWriter* rw = reinterpret_cast<FactoryFn>(encVt[slot / 8])(enc, ser, version);
    ser->readerWriter = rw;
    return rw;
}

// B3X W0: the ReaderWriter members, by name on the real class (the
// `rt::io::ReaderWriter` POD and `IO::ReaderWriter` are asserted
// interchangeable in Serializer.h). `name` is nullptr, as retail passes.
IO::ReaderWriter* AsRw(ReaderWriter* rw) {
    return reinterpret_cast<IO::ReaderWriter*>(rw);   // B.3.x bridge
}

void DestroyCodec(ReaderWriter* rw) {
    if (!rw) return;
    delete AsRw(rw);
}

void EmitInt(ReaderWriter* rw, int32_t* p) {
    AsRw(rw)->SerializeInt(*p, nullptr);
}
void EmitFloat(ReaderWriter* rw, float* p) {
    AsRw(rw)->SerializeFloat(*p, nullptr);
}
void EmitFloatArray(ReaderWriter* rw, int n, float* p) {
    AsRw(rw)->SerializeFloatArray(n, p, nullptr);
}
int32_t CodecVersion(ReaderWriter* rw) {
    return AsRw(rw)->GetVersion();
}
void CodecSkip(ReaderWriter* rw) {
    AsRw(rw)->Skip();
}

// An Array<T> header, as the elements of the +0x70 array-of-arrays are shaped:
// data, size, capacity. Only `data` and `size` are read here.
struct ArrayHdrView { uint8_t* data; int32_t size; int32_t capacity; };
static_assert(sizeof(ArrayHdrView) == 0x10, "Array<T> header is 0x10 bytes");

// ============================================================================
// ??0GraphicsMesh@Graphics@@QEAA@XZ                  retail 0x1800ff170
// ============================================================================
// Four vptr stamps, three zeroed External qwords, and twenty-odd immediates.
// It open-codes the whole chain (`Object::Base`, `Object::External`,
// `Object::CompiledExternal`) as the four stamps plus three zero stores.
// ENDGAME B.3.9 block GFX: that IS the real `GraphicsMesh()` now (the stamps
// and External's stores the compiler's, the immediates in GraphicsMesh.h); the
// batch-6 `ConstructBody` and its ??0 door are gone.
// ============================================================================

}  // namespace

// ############################################################################
// `Graphics::GraphicsMesh`'s member bodies. Their own `namespace Graphics { }`
// block, after the anonymous namespace has closed, because a member definition
// has to appear in a namespace ENCLOSING the class (B34_BRIEFS A.2). The
// anonymous namespace's helpers are reachable unqualified from here: they are
// members of the global namespace's unnamed namespace, in this same TU.
// ############################################################################
namespace Graphics {

// [2] ?Type@GraphicsMesh@Graphics@@UEBAIXZ  0x180012370
//     `MOV EAX,dword ptr [0x1801fa708]` — the BaseTypeInfo record's +0x00.
// ENDGAME B.3.9 TI-REST: Type / TypeName / TypeId are DECLARE_TYPE's bodies over the class's own
// record (graphics_type_records.cpp).

// [1] ?TypeName@GraphicsMesh@Graphics@@UEBAPEBDXZ  0x180012390
//     `MOV RAX,qword ptr [0x1801fa710]` (= ?_typeInfo@… + 8).

// [3] ?GetFileVersion@GraphicsMesh@Graphics@@UEBAHXZ  0x1800ff3a0
//     `MOV EAX,0x3415 ; RET`. 13333 decimal; a LITERAL, and nothing in this tree
//     may derive it, for the reason graphics_typeinfo_accessors.cpp gives about
//     MaterialInstance's 4: a derived spelling would silently track OUR format if
//     this class's Serialize is ever revised, and retail's does not.
int32_t GraphicsMesh::GetFileVersion() const { return 0x3415; }

// [7] ?Destruct@GraphicsMesh@Graphics@@UEAAXXZ  0x180012540
//     `MOV RAX,[RCX]; XOR EDX,EDX; JMP [RAX]` — slot 0 with flags 0, dispatched
//     VIRTUALLY on purpose: a subclass's deleting destructor is what has to run.
void GraphicsMesh::Destruct() { this->~GraphicsMesh(); }

// [8] ?Construct@GraphicsMesh@Graphics@@UEAAPEAVExternal@Object@@XZ 0x180012530
//     `TEST RCX,RCX; JNZ 0x1800ff170; XOR EAX,EAX` -- the constructor in place.
//     (The null test sat in the retired door; a virtual call always has a
//     `this`.) `::new`: the class declares its own, undefined, placement new.
Object::External* GraphicsMesh::Construct() {
    return ::new (this) GraphicsMesh();
}

// [6] ?Reconstruct@GraphicsMesh@Graphics@@UEAAPEAVExternal@Object@@XZ 0x180012510
//     Slot 0 with flags 0 (destroy IN PLACE, not free), then the ctor on the
//     same storage.
Object::External* GraphicsMesh::Reconstruct() {
    this->~GraphicsMesh();
    return ::new (this) GraphicsMesh();
}

// ============================================================================
// ??1GraphicsMesh@Graphics@@UEAA@XZ                  retail 0x1800ff270
// ============================================================================
// Restamp to GraphicsMesh's own table, destroy the three arrays IN RETAIL'S
// ORDER (+0x70, then +0x60, then +0x50 — the REVERSE of declaration order, as a
// destructor should be, and the order is transcribed rather than re-derived),
// release the name String, restamp to CompiledExternal and tail-jump its dtor.
//
// ENDGAME B.3.9 block GFX: the body moved here from the retired
// `RtGraphics_GraphicsMesh_Dtor` door. The two restamps are the compiler's, and
// so is the tail into ~CompiledExternal / ~External. The three explicit
// `~Array` calls are retail's own decomposition; each frees AND zeroes its
// header, so the members' implicit ~Array that runs after this body is a
// no-op (no element destroy, MemFree(null)) -- nothing runs twice.
//
// THE STRING RELEASE IS THE `str-4` / `str-8` IDIOM and its two arms leave the
// slot holding DIFFERENT THINGS — the asymmetry array_destroy.cpp's Body B
// documents:
//     if (p) { if (--p[-1] == 0) { slot = p - 8; MemFree(slot); }
//              else               slot = nullptr; }
// Retail really does store `this+0x20 -= 8` and then free THAT (0x1800ff2c0 ..
// 0x1800ff2c9), and really does store 0 on the surviving arm (0x1800ff2d0).
// Writing `slot = nullptr` on both arms would leave a freed block's address
// nowhere and would not match; writing neither would leave a dangling pointer.
// ============================================================================
GraphicsMesh::~GraphicsMesh() {
    GraphicsMesh* const self = this;

    rtcoll::Rt_ArrayDtor_180014160(&self->_skinMaterials);     // ~Array<Array<T>>
    rtcoll::Rt_ArrayDtor_180014090(&self->_nodes);             // ~Array<Node>
    rtcoll::Rt_ArrayDtor_180013fb0(&self->_detailLevels);      // ~Array<DetailLevel>

    char* const name = self->_name.data;
    if (name != nullptr) {
        int32_t* const refs = reinterpret_cast<int32_t*>(name) - 1;
        --*refs;
        if (*refs == 0) {
            self->_name.data = name - 8;                       // ADD qword [RBX+0x20],-8
            System::MemoryAllocator::MemFree(self->_name.data);   // 0x1800c7ad0
        } else {
            self->_name.data = nullptr;                        // MOV qword [RBX+0x20],0
        }
    }
    // 0x1800ff2d8 restamp + `JMP 0x1800bc950`: the compiler's chain.
}

// ?TypeId@GraphicsMesh@Graphics@@SAIXZ 0x180012380 -- the static twin of slot
// 2: the minted record's +0x00 until the W3 fold (B39_RULES D6 step 1).

// ?CreateNewObject@GraphicsMesh@Graphics@@SAPEAVBase@Object@@XZ 0x180012480 --
// the FACTREST row it replaces called ??0GraphicsMesh after the locked
// MemAlloc(0x98, 8); retail's null branch (B39_RULES D2).
Object::Base* GraphicsMesh::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(GraphicsMesh), 8);
    if (p == nullptr) return nullptr;
    return ::new (p) GraphicsMesh();
}
// ?CreateInPlaceObject@GraphicsMesh@Graphics@@SAPEAVBase@Object@@PEAX@Z 0x180012500
Object::Base* GraphicsMesh::CreateInPlaceObject(void* place) {
    if (place == nullptr) return nullptr;
    return ::new (place) GraphicsMesh();
}

}  // namespace Graphics

// ENDGAME B.3.9 block GFX: the ctor door and the six slot doors (Type,
// TypeName, GetFileVersion, Destruct, Construct, Reconstruct) are gone -- their
// only readers were FACTREST, the minted `??_7GraphicsMesh@Graphics@@6B@` and its
// RT_DELDTOR row.

// ============================================================================
// §T  the DETAIL LEVEL's inline-geometry pass        retail 0x1800a4670
// ============================================================================
// *** THE DOOR IS NAMED `RtGraphics_GraphicsMeshTriangleList_Serialize` AND
// THAT NAME IS WRONG. IT IS KEPT ANYWAY. *** This body is NOT
// `?Serialize@TriangleList@GraphicsMesh@Graphics@@` — that export exists, at
// 0x1800ff300, and is a different function: six DWORD scalars at +0x00..+0x14 of
// a 0x18-byte record, looped over by the array helper at 0x180014850 which THIS
// body calls. What 0x1800a4670 serializes is the WHOLE 0xb0-byte DETAIL LEVEL's
// inline geometry — its `Array<TriangleList>` at +0x98, its four video buffers
// and its max-instance count at +0xa8 — which is why its argument is now spelled
// `GraphicsMesh::DetailLevel*`. The door keeps its name and its single ADDRESS
// registration (0x1800a4670) because renaming a registered door is a change to
// the cutover surface and B35_RULES section 11 keeps that surface still in step
// 5; the correction is recorded here and in `GraphicsMesh.h`'s banner instead.
//
// UNEXPORTED, and a NON-SLOT helper: `GraphicsMesh::Serialize` below calls it
// per detail level, so the class cannot round-trip without it.
//
// THE SHAPE, and it is not the obvious one. The body runs the SAME nine-step
// block TWICE, over two separately-created codecs on the same stream, under two
// DIFFERENT conditions on `Video::Device::_singleton->[0xf8]`:
//
//     pass 1 (0x1800a46f3):  [0xf8] == 0        -> emit,  else Skip()
//     pass 2 (0x1800a47ed):  [0xf8] == 1 or 2   -> emit,  else Skip()
//
// So the two passes are two SECTIONS of the file — one written when the device
// is in mode 0 and one when it is in mode 1 or 2 — and the reader takes the
// Skip() arm for whichever section its device does not want. Collapsing them
// into one pass, or hoisting the singleton read, would change how many bytes
// the function consumes. Both passes are transcribed; the shared block is one
// function so the two cannot drift apart.
//
// EACH PASS IS GUARDED, BEFORE ANY OF THAT, by `encoding->vtable[0](encoding)`,
// called TWICE with the counter increment between the two calls
// (0x1800a46a0 / 0x1800a46ba). Both calls are reproduced, in that order: slot 0
// of an Encoding is not known to be pure, and calling it once would be a
// different sequence of virtual calls on a live object.
// ============================================================================
namespace {

// The nine steps of one pass, 0x1800a470b .. 0x1800a479c (pass 1) and
// 0x1800a4813 .. 0x1800a48a8 (pass 2), which are instruction-for-instruction
// the same.
//
//   1  int n = this->[0xa0]        ; emitted through kScalarInt
//   2  if reading: SetSize(this+0x98, n)              (0x1800136f0)
//   3  SerializeTriangleArray(&ser, n, this->[0x98])  (0x180014850 — OURS,
//                                                      closure batch 2b)
//   4..7  four sub-objects, IN THIS ORDER: +0x40, +0x20, +0x80, +0x60 — NOT
//         ascending. Each is a POINTER, and each is called through ITS OWN
//         vtable slot +0x30 with (ptr, &ser). The order is retail's and is
//         transcribed rather than sorted.
//   8  kScalarInt on this+0xa8
//
// ENDGAME B.3.5 block G — THE FOUR SUB-OBJECTS ARE NOW NAMED, and that is the
// cross-check that fixed the 0xb0 layout: +0x40, +0x20, +0x80, +0x60 are exactly
// `_indexBuffer`, `_vertexBuffer`, `_skinIndexBuffer`, `_skinVertexBuffer` plus
// the 8 that `Video::{Vertex,Index}Buffer::PlatformConstruct` writes its
// `_deviceData` qword at (`Mesh.h`'s `VideoBufferHeader` banner pins the same
// idiom on EdgeMesh's eight buffers). So retail's "not ascending" order is INDEX
// BEFORE VERTEX in each pair, the same order `EdgeMesh::Serialize` uses for its
// eight, and it is transcribed rather than sorted.
void TriangleListPass(GraphicsMesh::DetailLevel* level, Serializer* ser, ReaderWriter* rw) {
    int32_t n = level->_triangleLists._size;                   // +0xa0
    EmitInt(rw, &n);
    if (ser->stream->direction == rt::io::kDirectionReading)
        rtcoll::RtColl_ArraySetSize_1800136f0(&level->_triangleLists, n);    // +0x98
    RtGraphics_EdgeMesh_SerializeTriangleListArray(ser, n, level->_triangleLists._data);   // 0x180014850

    void* const kSubObjects[4] = {
        level->_indexBuffer._deviceData,        // +0x38 + 8 = +0x40
        level->_vertexBuffer._deviceData,       // +0x18 + 8 = +0x20
        level->_skinIndexBuffer._deviceData,    // +0x78 + 8 = +0x80
        level->_skinVertexBuffer._deviceData,   // +0x58 + 8 = +0x60
    };
    for (void* const sub : kSubObjects) {
        void** const vt = *reinterpret_cast<void***>(sub);      // faithful: unguarded
        reinterpret_cast<void (*)(void*, Serializer*)>(vt[0x30 / 8])(sub, ser);
    }

    EmitInt(rw, &level->_maxInstances);                         // +0xa8
}

}  // namespace

extern "C" void RtGraphics_GraphicsMeshTriangleList_Serialize(void* selfv, Serializer* outer) {
    GraphicsMesh::DetailLevel* const self =
        reinterpret_cast<GraphicsMesh::DetailLevel*>(selfv);     // B.3.5 bridge
    Stream* const stream = outer->stream;
    Encoding* const enc = stream->encoding;

    using EncQueryFn = bool (*)(Encoding*);
    if (!reinterpret_cast<EncQueryFn>(enc->vtable[0])(enc)) return;   // 0x1800a46a0
    ++*SerializeCounter();                                            // 0x1800a46aa
    if (!reinterpret_cast<EncQueryFn>(enc->vtable[0])(enc)) return;   // 0x1800a46ba

    // ---- pass 1: device mode 0 --------------------------------------------
    {
        Serializer ser;
        ReaderWriter* rw = MakeCodec(stream, &ser, 0);
        if (*reinterpret_cast<const int32_t*>(VideoDevice() + 0xf8) == 0)
            TriangleListPass(self, &ser, rw);
        else
            CodecSkip(rw);                                   // CALL [RAX+0x10]
        DestroyCodec(rw);
    }

    // ---- pass 2: device mode 1 or 2 ---------------------------------------
    {
        Serializer ser;
        ReaderWriter* rw = MakeCodec(stream, &ser, 0);
        const int32_t mode = *reinterpret_cast<const int32_t*>(VideoDevice() + 0xf8);
        if (mode == 1 || mode == 2)
            TriangleListPass(self, &ser, rw);
        else
            CodecSkip(rw);
        DestroyCodec(rw);
    }
}

// ============================================================================
// slot 5 — ?Serialize@GraphicsMesh@Graphics@@UEAA_NAEAVStream@IO@@@Z
//                                                    retail 0x1800ff3b0
// ============================================================================
// The on-disk layout of every mesh in the game, so the ORDER and the TYPE of
// each emission is the whole port. Transcribed call by call from 0x1800ff3d5 ..
// 0x1800ff647:
//
//   codec = factory(stream, &ser, 1)     ; VERSION 1 — see MakeCodec's note
//   *0x1801fc69c = 0                     ; the TriangleList counter
//   *0x1801fc6a0 = this                  ; the "current mesh" static
//   String::Serialize(this+0x20, &ser)
//   kArrayFloat(3, this+0x28)            ; bounds min
//   kArrayFloat(3, this+0x34)            ; bounds max
//   kScalarInt  (this+0x40)
//   kScalarFloat(this+0x44)
//   kScalarFloat(this+0x48)
//   kScalarInt  (&n)  where n = this->_detailLevels._size
//     if reading: Array<DetailLevel>::SetSize(this+0x50, n)
//     for each of the n elements (stride 0xb0):
//         kScalarFloat(e+0x10)                   ; DetailLevel::_distance
//         String::Serialize(e+0x00, &ser)        ; DetailLevel::_name
//         0x18005d0f0(e+0x08, &ser)              ; DetailLevel::_image, an
//                                                  ExternalPtr<Video::ImageBuffer>
//         *** if (codec->GetVersion() == 0) the inline-geometry pass(e, &ser) ***
//   kScalarInt  (&n)  where n = this->_skinMaterials._size
//     if reading: Array<Array<ExternalPtr<MaterialInstance>>>::SetSize(this+0x70, n)
//     for each of the n elements (stride 0x10):
//         kScalarInt(&m) where m = e->_size
//         if reading: 0x18005cc60(e, m)
//         0x18005d4a0(&ser, m, e->_data)
//   kScalarInt  (&n)  where n = this->_nodes._size
//     if reading: Array<Node>::SetSize(this+0x60, n)
//     0x18005c120(&ser, n, this->_nodes._data)
//   kArrayFloat(3, this+0x80)            ; box min
//   kArrayFloat(3, this+0x8c)            ; box max
//   *** if (codec->GetVersion() >= 1) PlatformSerialize(this, &ser) ***
//   destroy the codec; return true
//
// THE TWO VERSION BRANCHES ARE THE FORMAT'S HINGE and they are complementary:
// version 0 carries the geometry INLINE (TriangleList::Serialize walks the
// index array and the four buffers), version >= 1 carries it through
// `PlatformSerialize` instead. A build that took both, or neither, would
// produce a file no other build can read. `CMP EAX,1 / JL` at 0x1800ff622 is a
// SIGNED >= 1, not a != 0.
//
// The three counts are read into a STACK temporary before being emitted
// (`MOV EAX,[R14+0x58]; MOV [RBP+0x20],EAX`), never emitted in place, because
// on the reading path the value that comes back is used to SIZE the array and
// must not have already clobbered the live `_size`. Transcribed that way.
//
// `MOV AL,1` at 0x1800ff647: always true, no failure path.
// ============================================================================
namespace Graphics {

bool GraphicsMesh::Serialize(void* streamv) {
    Stream* const stream = static_cast<Stream*>(streamv);

    Serializer ser;
    ReaderWriter* const rw = MakeCodec(stream, &ser, 1);
    const bool reading = (stream->direction == rt::io::kDirectionReading);

    *SerializeCounter()     = 0;
    *SerializeCurrentMesh() = this;

    StringSerialize(&_name, &ser);
    EmitFloatArray(rw, 3, &_vector28.x);
    EmitFloatArray(rw, 3, &_vector34.x);
    EmitInt  (rw, &_field40);
    EmitFloat(rw, &_scale);
    EmitFloat(rw, &_maxDepthDistance);

    // ---- the detail levels ---------------------------------------------------
    {
        int32_t n = _detailLevels._size;
        EmitInt(rw, &n);
        if (reading) rtcoll::Rt_SetSize_18005d840(&_detailLevels, n);

        DetailLevel* e = reinterpret_cast<DetailLevel*>(_detailLevels._data);
        for (int32_t i = 0; i < n; ++i, ++e) {     // ++e strides retail's 0xb0
            EmitFloat(rw, &e->_distance);
            StringSerialize(&e->_name, &ser);
            RtObject_ExternalPtrSerialize_ImageBuffer(                    // 0x18005d0f0
                reinterpret_cast<RtObject::External**>(&e->_image), &ser);
            if (CodecVersion(rw) == 0)
                RtGraphics_GraphicsMeshTriangleList_Serialize(e, &ser);
        }
    }

    // ---- the per-skin material lists ----------------------------------------
    {
        int32_t n = _skinMaterials._size;
        EmitInt(rw, &n);
        if (reading) rtcoll::Rt_SetSize_18005cda0(&_skinMaterials, n);

        ArrayHdrView* e = reinterpret_cast<ArrayHdrView*>(_skinMaterials._data);
        for (int64_t i = 0; i < static_cast<int64_t>(n); ++i, ++e) {
            int32_t m = e->size;
            EmitInt(rw, &m);
            if (reading) rtcoll::RtColl_ArraySetSize_18005cc60(e, m);
            RtObject_ExternalPtrArraySerialize_MaterialInstance(          // 0x18005d4a0
                &ser, m, reinterpret_cast<RtObject::External**>(e->data));
        }
    }

    // ---- the nodes ----------------------------------------------------------
    {
        int32_t n = _nodes._size;
        EmitInt(rw, &n);
        if (reading) rtcoll::Rt_SetSize_18005aa30(&_nodes, n);
        RtGraphics_SerializeMeshNodes(&ser, n, _nodes._data);            // 0x18005c120
    }

    EmitFloatArray(rw, 3, &_aabb.min.x);
    EmitFloatArray(rw, 3, &_aabb.max.x);

    if (CodecVersion(rw) >= 1) PlatformSerializeDoor(this, &ser);

    DestroyCodec(rw);
    return true;                                              // MOV AL,1
}

}  // namespace Graphics

// (ENDGAME B.3.9 block GFX: the slot-5 door is gone with the minted table.)

// ============================================================================
// Registration — name AND address for every body that has a name, address only
// for the one that does not (TriangleList::Serialize is unexported in both
// shipped builds; checked against the export directory, not assumed).
// ============================================================================
int RtGraphicsMeshVtableBatch6Register() {
    int n = 0;

    // The factory pair (B39_RULES D2), the class's own statics, by name AND
    // address. The ctor / dtor / slot doors are retired (above).

    return n;
}
