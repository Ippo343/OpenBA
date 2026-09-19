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

#include "../../funcs/rt_port.h"
#include "../../types/io/Serializer.h"
#include "../../types/graphics/GraphicsMesh.h"   // the real Graphics::GraphicsMesh

using rt::io::Encoding;
using rt::io::ReaderWriter;
using rt::io::Serializer;
using rt::io::Stream;

// See imagebuffer_derived.cpp for why this is re-declared instead of included.
enum class Module : int { Exe = 0, Runtime = 1, VideoDX11 = 2, COUNT = 3 };
uintptr_t ResolveAddr(Module mod, uintptr_t ghidra_addr);

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

// The two constants the ctor writes as raw bit patterns. Spelled as bits and
// then bit-cast, not as decimal floats: 0x461c4000 and 0x4cbebc20 are what the
// listing holds, and a decimal spelling invites a rounding argument.
constexpr uint32_t kBits_1_0     = 0x3f800000u;   //  1.0f
constexpr uint32_t kBits_neg1_0  = 0xbf800000u;   // -1.0f
constexpr uint32_t kBits_10000   = 0x461c4000u;   //  10000.0f
constexpr uint32_t kBits_1e8     = 0x4cbebc20u;   //  1e8f
constexpr uint32_t kBits_neg1e8  = 0xccbebc20u;   // -1e8f

// The four vtables the ctor stamps in sequence, and the two the dtor restores.
// MSVC emits one store per level of the hierarchy even though only the last of
// them survives; all four are transcribed (see imagebuffer_derived.cpp's note
// on the same idiom).
constexpr uintptr_t kVtBase             = 0x180154788ull;  // ??_7Base@Object@@6B@
constexpr uintptr_t kVtExternal         = 0x1801547a0ull;  // ??_7External@Object@@6B@
constexpr uintptr_t kVtCompiledExternal = 0x180154a30ull;  // ??_7CompiledExternal@Object@@6B@
constexpr uintptr_t kVtGraphicsMesh     = 0x180154cc0ull;  // ??_7GraphicsMesh@Graphics@@6B@

void* Vt(uintptr_t va) {
    return reinterpret_cast<void*>(ResolveAddr(Module::Runtime, va));
}

// ---------------------------------------------------------------------------
// `?_typeInfo@GraphicsMesh@Graphics@@0VBaseTypeInfo@Object@@A` — 0x1801fa708,
// MINTED and PROMOTED (standalone/reg_mint_promoted.cpp:936). Read through the
// name for the reason graphics_typeinfo_accessors.cpp gives.
// ---------------------------------------------------------------------------
const uint8_t* TypeInfoGraphicsMesh() {
    return RT_DATA_SLOT(const uint8_t*,
        "?_typeInfo@GraphicsMesh@Graphics@@0VBaseTypeInfo@Object@@A");
}

// ---------------------------------------------------------------------------
// Named dependencies — RT_EXPORT_ONCE, so a ported one is reached as ours.
// ---------------------------------------------------------------------------
using VoidThisFn  = void  (*)(void*);
using PtrThisFn   = void* (*)(void*);
using SerMemberFn = void  (*)(void*, Serializer*);

// `??1CompiledExternal@Object@@UEAA@XZ` — retail 0x1800bc950, the dtor's tail
// jump. PORTED (runtime/src/object/), reached by NAME.
void CompiledExternalDtor(void* p) {
    RT_EXPORT_ONCE(VoidThisFn, "??1CompiledExternal@Object@@UEAA@XZ")(p);
}

// `?Serialize@String@System@@QEAAXAEAVSerializer@IO@@@Z` — retail 0x1800c9930,
// PORTED (system/string.cpp). ASKED FOR BY ADDRESS DELIBERATELY: string.cpp
// registers this symbol with RegisterRuntimeAddr ONLY — there is no
// RegisterRuntimeExport for it anywhere in the tree (grepped, not assumed), so
// a name door would resolve straight past our port into the DLL. The same
// reasoning imagebuffer_vtable.cpp:170 wrote down for the same callee.
void StringSerialize(void* str, Serializer* ser) {
    using Fn = void (*)(void*, Serializer*);
    static Fn f = nullptr;
    if (!f) f = reinterpret_cast<Fn>(ResolveAddr(Module::Runtime, 0x1800c9930));
    f(str, ser);
}

// `?MemFree@MemoryAllocator@System@@SAXPEAX@Z` — retail 0x1800c7ad0. The
// destructor's String release calls it directly (0x1800ff2c9). PORTED.
void MemFree(void* p) {
    using Fn = void (*)(void*);
    static Fn f = nullptr;
    if (!f) f = reinterpret_cast<Fn>(ResolveAddr(Module::Runtime, 0x1800c7ad0));
    f(p);
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
// The unexported Collection helpers this class's Serialize drives. All six are
// template instantiations with no export name in either shipped build, so all
// six are ADDRESS doors; `ResolveAddr` is registry-first, so each lands on our
// port where one exists.
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
using SetSizeFn   = void (*)(void*, int);
using SerNCountFn = void (*)(Serializer*, int, void*);

void ByAddr2(uintptr_t va, void* a, int n) {
    using Fn = void (*)(void*, int);
    reinterpret_cast<Fn>(ResolveAddr(Module::Runtime, va))(a, n);
}
// The one-argument form. Spelled separately rather than reusing ByAddr2 with a
// dummy: `~Array` reads RCX only (checked at all three addresses), and a
// two-argument typedef over a one-argument body is the shape lane Q's SetSize
// correction was about — harmless on this ABI today, wrong as documentation.
void ByAddr1(uintptr_t va, void* a) {
    using Fn = void* (*)(void*);
    reinterpret_cast<Fn>(ResolveAddr(Module::Runtime, va))(a);
}
void ByAddrSerN(uintptr_t va, Serializer* ser, int n, void* data) {
    reinterpret_cast<SerNCountFn>(ResolveAddr(Module::Runtime, va))(ser, n, data);
}

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
// Both are UNEXPORTED DATA and are therefore reached BY ADDRESS. That keeps
// one copy shared with `PlatformVersion`, which is still the DLL's — declaring
// our own statics would split the pair while the DLL half is live, which is the
// split-brain shape. AT CUTOVER these two words need a minted home; there is
// none today, and this comment is the record of that, not a claim otherwise.
// ---------------------------------------------------------------------------
int32_t* SerializeCounter() {
    static int32_t* p = nullptr;
    if (!p) p = reinterpret_cast<int32_t*>(ResolveAddr(Module::Runtime, 0x1801fc69c));
    return p;
}
void** SerializeCurrentMesh() {
    static void** p = nullptr;
    if (!p) p = reinterpret_cast<void**>(ResolveAddr(Module::Runtime, 0x1801fc6a0));
    return p;
}

// `?_singleton@Device@Video@@0PEAV12@EA` — 0x1801fe7a8, a POINTER to the
// Device, so reading +0xf8 is a double indirection.
const uint8_t* VideoDevice() {
    return *RT_DATA_SLOT(const uint8_t* const*, "?_singleton@Device@Video@@0PEAV12@EA");
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

void DestroyCodec(ReaderWriter* rw) {
    if (!rw) return;
    using DtorFn = void (*)(ReaderWriter*, int);
    reinterpret_cast<DtorFn>(rw->vtable[rt::io::rw_vtbl::kDtor / 8])(rw, 1);
}

void EmitInt(ReaderWriter* rw, int32_t* p) {
    using Fn = void (*)(ReaderWriter*, int32_t*, const char*);
    reinterpret_cast<Fn>(rw->vtable[rt::io::rw_vtbl::kScalarInt / 8])(rw, p, nullptr);
}
void EmitFloat(ReaderWriter* rw, float* p) {
    using Fn = void (*)(ReaderWriter*, float*, const char*);
    reinterpret_cast<Fn>(rw->vtable[rt::io::rw_vtbl::kScalarFloat / 8])(rw, p, nullptr);
}
void EmitFloatArray(ReaderWriter* rw, int n, float* p) {
    using Fn = void (*)(ReaderWriter*, int, float*, const char*);
    reinterpret_cast<Fn>(rw->vtable[rt::io::rw_vtbl::kArrayFloat / 8])(rw, n, p, nullptr);
}
int32_t CodecVersion(ReaderWriter* rw) {
    using Fn = int32_t (*)(ReaderWriter*);
    return reinterpret_cast<Fn>(rw->vtable[rt::io::rw_vtbl::kGetVersion / 8])(rw);
}
void CodecSkip(ReaderWriter* rw) {
    using Fn = void (*)(ReaderWriter*);
    reinterpret_cast<Fn>(rw->vtable[rt::io::rw_vtbl::kSkip / 8])(rw);
}

// An Array<T> header, as the elements of the +0x70 array-of-arrays are shaped:
// data, size, capacity. Only `data` and `size` are read here.
struct ArrayHdrView { uint8_t* data; int32_t size; int32_t capacity; };
static_assert(sizeof(ArrayHdrView) == 0x10, "Array<T> header is 0x10 bytes");

// The ctor's immediates are spelled as BIT PATTERNS (see kBits_* above) and
// bit-cast, never as decimal floats, so no rounding argument can arise.
float FromBits(uint32_t bits) {
    float f; std::memcpy(&f, &bits, sizeof f); return f;
}
::Vector3 Bits3(uint32_t bits) {
    const float f = FromBits(bits);
    ::Vector3 v; v.x = f; v.y = f; v.z = f; return v;
}

// Slot 0 of Object::Base is the deleting destructor: flag 0 destroys IN PLACE,
// flag 1 destroys and frees. Dispatched virtually on purpose.
void CallSlot0(void* self, unsigned flags) {
    void** const vt = *reinterpret_cast<void***>(self);
    reinterpret_cast<void* (*)(void*, unsigned)>(vt[0])(self, flags);
}

// ============================================================================
// ??0GraphicsMesh@Graphics@@QEAA@XZ                  retail 0x1800ff170
// ============================================================================
// Four vptr stamps, three zeroed External qwords, and twenty-odd immediates.
// The FIRST THREE STAMPS ARE DEAD — each is overwritten by the next with no
// virtual dispatch in between — and all four are transcribed for the reason
// wave23_lane_gfxbody.cpp gives: they are what the bytes do.
//
// *** THE ctor DOES NOT CALL A BASE CONSTRUCTOR. *** It open-codes the whole
// chain (`Object::Base`, `Object::External`, `Object::CompiledExternal`) as the
// four stamps plus three zero stores. A transcription that "helpfully" called
// `??0CompiledExternal@Object@@` instead would run that body's OWN stores,
// which are not these.
//
// AND THAT IS WHY THIS IS NOT A MEMBER (B4b-2, ENDGAME section B.1 decision 3):
// the four stamps name the MINTED tables, which is right for every object that
// reaches this body, and a real constructor would stamp GCC's instead.
// `Graphics::GraphicsMesh::Construct()` and `Reconstruct()` call it.
//
// `MOV RAX,RCX` at the end: it returns `this`.
// ============================================================================
void ConstructBody(GraphicsMesh* self) {
    void** const vp = reinterpret_cast<void**>(self);

    *vp = Vt(kVtBase);                                        // 0x1800ff181
    *vp = Vt(kVtExternal);                                    // 0x1800ff18b
    self->_path.data      = nullptr;                          // LEA RAX,[RCX+8]; [RAX]=0
    self->_refs._data     = nullptr;                          // +0x10
    self->_refs._size     = 0;                                // +0x18 (one qword with cap)
    self->_refs._capacity = 0;
    *vp = Vt(kVtCompiledExternal);                            // 0x1800ff1ab
    *vp = Vt(kVtGraphicsMesh);                                // 0x1800ff1b5

    self->_name.data = nullptr;                               // +0x20, the String

    self->_vector28 = Bits3(kBits_1_0);                       // +0x28  ( 1, 1, 1)
    self->_vector34 = Bits3(kBits_neg1_0);                    // +0x34  (-1,-1,-1)
    self->_field40         = 1;                               // +0x40
    self->_scale           = FromBits(kBits_1_0);             // +0x44  1.0f
    self->_maxDepthDistance = FromBits(kBits_10000);          // +0x48  10000.0f

    self->_detailLevels._data     = nullptr;                  // three Array headers,
    self->_detailLevels._size     = 0;                        // six qword zero stores
    self->_detailLevels._capacity = 0;
    self->_nodes._data     = nullptr;
    self->_nodes._size     = 0;
    self->_nodes._capacity = 0;
    self->_skinMaterials._data     = nullptr;
    self->_skinMaterials._size     = 0;
    self->_skinMaterials._capacity = 0;

    self->_aabb.min = Bits3(kBits_1e8);                       // +0x80   1e8f x3
    self->_aabb.max = Bits3(kBits_neg1e8);                    // +0x8c  -1e8f x3
}

}  // namespace

// ============================================================================
// ??1GraphicsMesh@Graphics@@UEAA@XZ                  retail 0x1800ff270
// ============================================================================
// Restamp to GraphicsMesh's own table, destroy the three arrays IN RETAIL'S
// ORDER (+0x70, then +0x60, then +0x50 — the REVERSE of declaration order, as a
// destructor should be, and the order is transcribed rather than re-derived),
// release the name String, restamp to CompiledExternal and tail-jump its dtor.
//
// THIS STAYS A HAND-WRITTEN DOOR AND `~GraphicsMesh()` IS EMPTY — the
// `Object::External` / `AnimationGroup` precedent. The two restamps name the
// MINTED tables, which is right for the minted objects that are the only ones
// alive today, and the three explicit `~Array` calls plus the `str-8` release
// are retail's own decomposition: a compiler-emitted destructor chain would
// stamp GCC's tables and run each `~Array` a SECOND time.
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
extern "C" void RtGraphics_GraphicsMesh_Dtor(void* selfv) {
    GraphicsMesh* const self = reinterpret_cast<GraphicsMesh*>(selfv);  // B.3.5 bridge

    *reinterpret_cast<void**>(self) = Vt(kVtGraphicsMesh);     // 0x1800ff286

    ByAddr1(0x180014160ull, &self->_skinMaterials);            // ~Array<Array<T>>
    ByAddr1(0x180014090ull, &self->_nodes);                    // ~Array<Node>
    ByAddr1(0x180013fb0ull, &self->_detailLevels);             // ~Array<DetailLevel>

    char* const name = self->_name.data;
    if (name != nullptr) {
        int32_t* const refs = reinterpret_cast<int32_t*>(name) - 1;
        --*refs;
        if (*refs == 0) {
            self->_name.data = name - 8;                       // ADD qword [RBX+0x20],-8
            MemFree(self->_name.data);
        } else {
            self->_name.data = nullptr;                        // MOV qword [RBX+0x20],0
        }
    }

    *reinterpret_cast<void**>(self) = Vt(kVtCompiledExternal); // 0x1800ff2d8
    CompiledExternalDtor(self);                                // JMP 0x1800bc950
}

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
uint32_t GraphicsMesh::Type() const {
    uint32_t v = 0; std::memcpy(&v, TypeInfoGraphicsMesh(), sizeof v); return v;
}

// [1] ?TypeName@GraphicsMesh@Graphics@@UEBAPEBDXZ  0x180012390
//     `MOV RAX,qword ptr [0x1801fa710]` (= ?_typeInfo@… + 8).
const char* GraphicsMesh::TypeName() const {
    const char* v = nullptr;
    std::memcpy(&v, TypeInfoGraphicsMesh() + 8, sizeof v);
    return v;
}

// [3] ?GetFileVersion@GraphicsMesh@Graphics@@UEBAHXZ  0x1800ff3a0
//     `MOV EAX,0x3415 ; RET`. 13333 decimal; a LITERAL, and nothing in this tree
//     may derive it, for the reason graphics_typeinfo_accessors.cpp gives about
//     MaterialInstance's 4: a derived spelling would silently track OUR format if
//     this class's Serialize is ever revised, and retail's does not.
int32_t GraphicsMesh::GetFileVersion() const { return 0x3415; }

// [7] ?Destruct@GraphicsMesh@Graphics@@UEAAXXZ  0x180012540
//     `MOV RAX,[RCX]; XOR EDX,EDX; JMP [RAX]` — slot 0 with flags 0, dispatched
//     VIRTUALLY on purpose: a subclass's deleting destructor is what has to run.
void GraphicsMesh::Destruct() { CallSlot0(this, 0u); }

// [8] ?Construct@GraphicsMesh@Graphics@@UEAAPEAVExternal@Object@@XZ 0x180012530
//     `TEST RCX,RCX; JNZ 0x1800ff170; XOR EAX,EAX`. The null test lives in the
//     DOOR, which is where retail put it.
Object::External* GraphicsMesh::Construct() {
    ConstructBody(this);
    return this;
}

// [6] ?Reconstruct@GraphicsMesh@Graphics@@UEAAPEAVExternal@Object@@XZ 0x180012510
//     Slot 0 with flags 0 (destroy IN PLACE, not free), then the ctor on the
//     same storage. NO null test, unlike Construct one slot over; that asymmetry
//     is retail's and is kept.
Object::External* GraphicsMesh::Reconstruct() {
    CallSlot0(this, 0u);
    ConstructBody(this);
    return this;
}

// ??1GraphicsMesh@Graphics@@UEAA@XZ  0x1800ff270 — EMPTY; see the door above.
GraphicsMesh::~GraphicsMesh() {}

}  // namespace Graphics

// ============================================================================
// THE DOORS. Each keeps its registered name and address and delegates with a
// QUALIFIED non-virtual call. THE QUALIFICATION IS LOAD-BEARING (B34_BRIEFS
// A.1): a live `GraphicsMesh` wears the MINTED `??_7GraphicsMesh@Graphics@@6B@`,
// whose slots resolve back to these very doors, so an unqualified virtual call
// inside a door would dispatch into itself instead of delegating.
// ============================================================================
extern "C" void* RtGraphics_GraphicsMesh_Ctor(void* selfv) {
    ConstructBody(reinterpret_cast<GraphicsMesh*>(selfv));     // B.3.5 bridge
    return selfv;                                              // MOV RAX,RCX
}

extern "C" uint32_t RtGraphics_GraphicsMesh_Type(const void* selfv) {
    return reinterpret_cast<const GraphicsMesh*>(selfv)         // B.3.5 bridge
               ->Graphics::GraphicsMesh::Type();
}

extern "C" const char* RtGraphics_GraphicsMesh_TypeName(const void* selfv) {
    return reinterpret_cast<const GraphicsMesh*>(selfv)         // B.3.5 bridge
               ->Graphics::GraphicsMesh::TypeName();
}

extern "C" int32_t RtGraphics_GraphicsMesh_GetFileVersion(const void* selfv) {
    return reinterpret_cast<const GraphicsMesh*>(selfv)         // B.3.5 bridge
               ->Graphics::GraphicsMesh::GetFileVersion();
}

extern "C" void RtGraphics_GraphicsMesh_Destruct(void* selfv) {
    reinterpret_cast<GraphicsMesh*>(selfv)                      // B.3.5 bridge
        ->Graphics::GraphicsMesh::Destruct();
}

extern "C" void* RtGraphics_GraphicsMesh_Construct(void* selfv) {
    if (selfv == nullptr) return nullptr;                       // retail's null test
    return reinterpret_cast<GraphicsMesh*>(selfv)               // B.3.5 bridge
               ->Graphics::GraphicsMesh::Construct();
}

extern "C" void* RtGraphics_GraphicsMesh_Reconstruct(void* selfv) {
    return reinterpret_cast<GraphicsMesh*>(selfv)               // B.3.5 bridge
               ->Graphics::GraphicsMesh::Reconstruct();
}

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
        ByAddr2(0x1800136f0ull, &level->_triangleLists, n);    // +0x98
    ByAddrSerN(0x180014850ull, ser, n, level->_triangleLists._data);

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
        if (reading) ByAddr2(0x18005d840ull, &_detailLevels, n);

        DetailLevel* e = reinterpret_cast<DetailLevel*>(_detailLevels._data);
        for (int32_t i = 0; i < n; ++i, ++e) {     // ++e strides retail's 0xb0
            EmitFloat(rw, &e->_distance);
            StringSerialize(&e->_name, &ser);
            reinterpret_cast<SerMemberFn>(
                ResolveAddr(Module::Runtime, 0x18005d0f0ull))(&e->_image, &ser);
            if (CodecVersion(rw) == 0)
                RtGraphics_GraphicsMeshTriangleList_Serialize(e, &ser);
        }
    }

    // ---- the per-skin material lists ----------------------------------------
    {
        int32_t n = _skinMaterials._size;
        EmitInt(rw, &n);
        if (reading) ByAddr2(0x18005cda0ull, &_skinMaterials, n);

        ArrayHdrView* e = reinterpret_cast<ArrayHdrView*>(_skinMaterials._data);
        for (int64_t i = 0; i < static_cast<int64_t>(n); ++i, ++e) {
            int32_t m = e->size;
            EmitInt(rw, &m);
            if (reading) ByAddr2(0x18005cc60ull, e, m);
            ByAddrSerN(0x18005d4a0ull, &ser, m, e->data);
        }
    }

    // ---- the nodes ----------------------------------------------------------
    {
        int32_t n = _nodes._size;
        EmitInt(rw, &n);
        if (reading) ByAddr2(0x18005aa30ull, &_nodes, n);
        ByAddrSerN(0x18005c120ull, &ser, n, _nodes._data);
    }

    EmitFloatArray(rw, 3, &_aabb.min.x);
    EmitFloatArray(rw, 3, &_aabb.max.x);

    if (CodecVersion(rw) >= 1) PlatformSerializeDoor(this, &ser);

    DestroyCodec(rw);
    return true;                                              // MOV AL,1
}

}  // namespace Graphics

extern "C" bool RtGraphics_GraphicsMesh_Serialize(void* selfv, Stream* stream) {
    return reinterpret_cast<GraphicsMesh*>(selfv)              // B.3.5 bridge
               ->Graphics::GraphicsMesh::Serialize(stream);
}

// ============================================================================
// Registration — name AND address for every body that has a name, address only
// for the one that does not (TriangleList::Serialize is unexported in both
// shipped builds; checked against the export directory, not assumed).
// ============================================================================
int RtGraphicsMeshVtableBatch6Register() {
    int n = 0;

    RegisterRuntimeExport("??0GraphicsMesh@Graphics@@QEAA@XZ",
                          (void*)&RtGraphics_GraphicsMesh_Ctor); ++n;
    RegisterRuntimeAddr(0x1800ff170ull, (void*)&RtGraphics_GraphicsMesh_Ctor);

    RegisterRuntimeExport("??1GraphicsMesh@Graphics@@UEAA@XZ",
                          (void*)&RtGraphics_GraphicsMesh_Dtor); ++n;
    RegisterRuntimeAddr(0x1800ff270ull, (void*)&RtGraphics_GraphicsMesh_Dtor);

    RegisterRuntimeExport("?Type@GraphicsMesh@Graphics@@UEBAIXZ",
                          (void*)&RtGraphics_GraphicsMesh_Type); ++n;
    RegisterRuntimeAddr(0x180012370ull, (void*)&RtGraphics_GraphicsMesh_Type);

    RegisterRuntimeExport("?TypeName@GraphicsMesh@Graphics@@UEBAPEBDXZ",
                          (void*)&RtGraphics_GraphicsMesh_TypeName); ++n;
    RegisterRuntimeAddr(0x180012390ull, (void*)&RtGraphics_GraphicsMesh_TypeName);

    RegisterRuntimeExport("?GetFileVersion@GraphicsMesh@Graphics@@UEBAHXZ",
                          (void*)&RtGraphics_GraphicsMesh_GetFileVersion); ++n;
    RegisterRuntimeAddr(0x1800ff3a0ull, (void*)&RtGraphics_GraphicsMesh_GetFileVersion);

    RegisterRuntimeExport("?Serialize@GraphicsMesh@Graphics@@UEAA_NAEAVStream@IO@@@Z",
                          (void*)&RtGraphics_GraphicsMesh_Serialize); ++n;
    RegisterRuntimeAddr(0x1800ff3b0ull, (void*)&RtGraphics_GraphicsMesh_Serialize);

    RegisterRuntimeExport("?Reconstruct@GraphicsMesh@Graphics@@UEAAPEAVExternal@Object@@XZ",
                          (void*)&RtGraphics_GraphicsMesh_Reconstruct); ++n;
    RegisterRuntimeAddr(0x180012510ull, (void*)&RtGraphics_GraphicsMesh_Reconstruct);

    RegisterRuntimeExport("?Destruct@GraphicsMesh@Graphics@@UEAAXXZ",
                          (void*)&RtGraphics_GraphicsMesh_Destruct); ++n;
    RegisterRuntimeAddr(0x180012540ull, (void*)&RtGraphics_GraphicsMesh_Destruct);

    RegisterRuntimeExport("?Construct@GraphicsMesh@Graphics@@UEAAPEAVExternal@Object@@XZ",
                          (void*)&RtGraphics_GraphicsMesh_Construct); ++n;
    RegisterRuntimeAddr(0x180012530ull, (void*)&RtGraphics_GraphicsMesh_Construct);

    RegisterRuntimeAddr(0x1800a4670ull,
                        (void*)&RtGraphics_GraphicsMeshTriangleList_Serialize); ++n;

    return n;
}
