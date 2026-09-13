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

using rt::io::Encoding;
using rt::io::ReaderWriter;
using rt::io::Serializer;
using rt::io::Stream;

// See imagebuffer_derived.cpp for why this is re-declared instead of included.
enum class Module : int { Exe = 0, Runtime = 1, VideoDX11 = 2, COUNT = 3 };
uintptr_t ResolveAddr(Module mod, uintptr_t ghidra_addr);

namespace {

// ---------------------------------------------------------------------------
// GRAPHICSMESH'S LAYOUT, 0x98 bytes, read from the constructor's stores
// (0x1800ff181 .. 0x1800ff256) and cross-checked against the destructor and
// Serialize. Spelled as offsets rather than as a struct because three of the
// members are Collection::Array headers whose element types live in
// array_graphics_mesh.cpp, and a struct here would duplicate that file's
// authority over them.
//
//   +0x00  vtable                (Base -> External -> CompiledExternal -> this)
//   +0x08  |
//   +0x10  |  Object::External's three zeroed qwords
//   +0x18  |
//   +0x20  System::String        the mesh name (refcounted; released by ~this)
//   +0x28  float x3 = 1,1,1      \  two Vector3s; the ctor's six immediates
//   +0x34  float x3 = -1,-1,-1   /
//   +0x40  int32  = 1
//   +0x44  float  = 1.0f
//   +0x48  float  = 10000.0f     (0x461c4000)
//   +0x50  Array<TriangleList>   stride 0xb0   (dtor: 0x180013fb0)
//   +0x60  Array<Node>           stride 0x48   (dtor: 0x180014090)
//   +0x70  Array<Array<T>>       stride 0x10   (dtor: 0x180014160)
//   +0x80  float x3 =  1e8f      \  the bounding box, 0x4cbebc20 / 0xccbebc20
//   +0x8c  float x3 = -1e8f      /
// ---------------------------------------------------------------------------
constexpr uint32_t kName          = 0x20;
constexpr uint32_t kBoundsMin     = 0x28;
constexpr uint32_t kBoundsMax     = 0x34;
constexpr uint32_t kInt40         = 0x40;
constexpr uint32_t kFloat44       = 0x44;
constexpr uint32_t kFloat48       = 0x48;
constexpr uint32_t kTriangleLists = 0x50;
constexpr uint32_t kNodes         = 0x60;
constexpr uint32_t kInnerArrays   = 0x70;
constexpr uint32_t kBoxMin        = 0x80;
constexpr uint32_t kBoxMax        = 0x8c;
constexpr uint32_t kGraphicsMeshSize = 0x98;

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
// retail 0x1800a48e0. NOT PORTED, and reached BY NAME because it IS exported
// (`QEAA`, public). Exactly the arrangement imagebuffer_vtable.cpp uses for
// `ImageBuffer::PlatformSerialize`: it drives the GPU buffer upload and is a
// Video-side body, so it stays on the DLL until that side is ported. This is
// the one dependency in this file that the cutover still needs.
void PlatformSerialize(void* self, Serializer* ser) {
    RT_EXPORT_ONCE(SerMemberFn,
        "?PlatformSerialize@GraphicsMesh@Graphics@@QEAAXAEAVSerializer@IO@@@Z")(self, ser);
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

// An Array<T> header, as the three members at +0x50/+0x60/+0x70 are shaped:
// data, size, capacity. Only `data` and `size` are read here.
struct ArrayHdrView { uint8_t* data; int32_t size; int32_t capacity; };
static_assert(sizeof(ArrayHdrView) == 0x10, "Array<T> header is 0x10 bytes");

ArrayHdrView* ArrayAt(void* self, uint32_t off) {
    return reinterpret_cast<ArrayHdrView*>(static_cast<uint8_t*>(self) + off);
}

}  // namespace

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
// `MOV RAX,RCX` at the end: it returns `this`.
// ============================================================================
extern "C" void* RtGraphics_GraphicsMesh_Ctor(void* self) {
    uint8_t* const d = static_cast<uint8_t*>(self);

    *reinterpret_cast<void**>(d) = Vt(kVtBase);               // 0x1800ff181
    *reinterpret_cast<void**>(d) = Vt(kVtExternal);           // 0x1800ff18b
    *reinterpret_cast<uint64_t*>(d + 0x08) = 0;               // LEA RAX,[RCX+8]; [RAX]=0
    *reinterpret_cast<uint64_t*>(d + 0x10) = 0;
    *reinterpret_cast<uint64_t*>(d + 0x18) = 0;
    *reinterpret_cast<void**>(d) = Vt(kVtCompiledExternal);   // 0x1800ff1ab
    *reinterpret_cast<void**>(d) = Vt(kVtGraphicsMesh);       // 0x1800ff1b5

    *reinterpret_cast<uint64_t*>(d + kName) = 0;              // the String, null

    uint32_t* const w = reinterpret_cast<uint32_t*>(d);
    w[(kBoundsMin + 0) / 4] = kBits_1_0;
    w[(kBoundsMin + 4) / 4] = kBits_1_0;
    w[(kBoundsMin + 8) / 4] = kBits_1_0;
    w[(kBoundsMax + 0) / 4] = kBits_neg1_0;
    w[(kBoundsMax + 4) / 4] = kBits_neg1_0;
    w[(kBoundsMax + 8) / 4] = kBits_neg1_0;
    w[kInt40 / 4]   = 1;
    w[kFloat44 / 4] = kBits_1_0;
    w[kFloat48 / 4] = kBits_10000;

    *reinterpret_cast<uint64_t*>(d + kTriangleLists + 0) = 0; // three Array headers,
    *reinterpret_cast<uint64_t*>(d + kTriangleLists + 8) = 0; // six qword zero stores
    *reinterpret_cast<uint64_t*>(d + kNodes + 0)         = 0;
    *reinterpret_cast<uint64_t*>(d + kNodes + 8)         = 0;
    *reinterpret_cast<uint64_t*>(d + kInnerArrays + 0)   = 0;
    *reinterpret_cast<uint64_t*>(d + kInnerArrays + 8)   = 0;

    w[(kBoxMin + 0) / 4] = kBits_1e8;
    w[(kBoxMin + 4) / 4] = kBits_1e8;
    w[(kBoxMin + 8) / 4] = kBits_1e8;
    w[(kBoxMax + 0) / 4] = kBits_neg1e8;
    w[(kBoxMax + 4) / 4] = kBits_neg1e8;
    w[(kBoxMax + 8) / 4] = kBits_neg1e8;

    return self;                                              // MOV RAX,RCX
}

// ============================================================================
// ??1GraphicsMesh@Graphics@@UEAA@XZ                  retail 0x1800ff270
// ============================================================================
// Restamp to GraphicsMesh's own table, destroy the three arrays IN RETAIL'S
// ORDER (+0x70, then +0x60, then +0x50 — the REVERSE of declaration order, as a
// destructor should be, and the order is transcribed rather than re-derived),
// release the name String, restamp to CompiledExternal and tail-jump its dtor.
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
extern "C" void RtGraphics_GraphicsMesh_Dtor(void* self) {
    uint8_t* const d = static_cast<uint8_t*>(self);

    *reinterpret_cast<void**>(d) = Vt(kVtGraphicsMesh);        // 0x1800ff286

    ByAddr1(0x180014160ull, d + kInnerArrays);                 // ~Array<Array<T>>
    ByAddr1(0x180014090ull, d + kNodes);                       // ~Array<Node>
    ByAddr1(0x180013fb0ull, d + kTriangleLists);               // ~Array<TriangleList>

    uint8_t** const nameSlot = reinterpret_cast<uint8_t**>(d + kName);
    uint8_t* const name = *nameSlot;
    if (name != nullptr) {
        int32_t* const refs = reinterpret_cast<int32_t*>(name) - 1;
        --*refs;
        if (*refs == 0) {
            *nameSlot = name - 8;                              // ADD qword [RBX+0x20],-8
            MemFree(*nameSlot);
        } else {
            *nameSlot = nullptr;                               // MOV qword [RBX+0x20],0
        }
    }

    *reinterpret_cast<void**>(d) = Vt(kVtCompiledExternal);    // 0x1800ff2d8
    CompiledExternalDtor(self);                                // JMP 0x1800bc950
}

// slot 2 — `MOV EAX,dword ptr [0x1801fa708]`
extern "C" uint32_t RtGraphics_GraphicsMesh_Type(const void* self) {
    (void)self;
    uint32_t v = 0; std::memcpy(&v, TypeInfoGraphicsMesh(), sizeof v); return v;
}

// slot 1 — `MOV RAX,qword ptr [0x1801fa710]` (= ?_typeInfo@… + 8)
extern "C" const char* RtGraphics_GraphicsMesh_TypeName(const void* self) {
    (void)self;
    const char* v = nullptr;
    std::memcpy(&v, TypeInfoGraphicsMesh() + 8, sizeof v);
    return v;
}

// slot 3 — `MOV EAX,0x3415 ; RET`. 13333 decimal; a LITERAL, and nothing in
// this tree may derive it, for the reason graphics_typeinfo_accessors.cpp gives
// about MaterialInstance's 4: a derived spelling would silently track OUR
// format if this class's Serialize is ever revised, and retail's does not.
extern "C" int32_t RtGraphics_GraphicsMesh_GetFileVersion(const void* self) {
    (void)self;
    return 0x3415;
}

// slot 7 — retail 0x180012540. `MOV RAX,[RCX]; XOR EDX,EDX; JMP [RAX]`.
extern "C" void RtGraphics_GraphicsMesh_Destruct(void* self) {
    void** vt = *reinterpret_cast<void***>(self);
    reinterpret_cast<void* (*)(void*, unsigned)>(vt[0])(self, 0u);   // flags 0
}

// slot 8 — retail 0x180012530. `TEST RCX,RCX; JNZ 0x1800ff170; XOR EAX,EAX`.
extern "C" void* RtGraphics_GraphicsMesh_Construct(void* self) {
    if (self == nullptr) return nullptr;
    return RtGraphics_GraphicsMesh_Ctor(self);
}

// slot 6 — retail 0x180012510. Slot 0 with flags 0, then the ctor, in place.
extern "C" void* RtGraphics_GraphicsMesh_Reconstruct(void* self) {
    void** vt = *reinterpret_cast<void***>(self);
    reinterpret_cast<void* (*)(void*, unsigned)>(vt[0])(self, 0u);
    return RtGraphics_GraphicsMesh_Ctor(self);
}

// ============================================================================
// §T  GraphicsMesh::TriangleList::Serialize        retail 0x1800a4670
// ============================================================================
// UNEXPORTED, and a NON-SLOT helper: `GraphicsMesh::Serialize` below calls it
// per triangle list, so the class cannot round-trip without it.
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
void TriangleListPass(void* self, Serializer* ser, ReaderWriter* rw) {
    uint8_t* const t = static_cast<uint8_t*>(self);

    int32_t n = *reinterpret_cast<const int32_t*>(t + 0xa0);
    EmitInt(rw, &n);
    if (ser->stream->direction == rt::io::kDirectionReading)
        ByAddr2(0x1800136f0ull, t + 0x98, n);
    ByAddrSerN(0x180014850ull, ser, n, *reinterpret_cast<void**>(t + 0x98));

    static const uint32_t kSubObjects[4] = { 0x40, 0x20, 0x80, 0x60 };
    for (uint32_t off : kSubObjects) {
        void* const sub = *reinterpret_cast<void**>(t + off);   // faithful: unguarded
        void** const vt = *reinterpret_cast<void***>(sub);
        reinterpret_cast<void (*)(void*, Serializer*)>(vt[0x30 / 8])(sub, ser);
    }

    EmitInt(rw, reinterpret_cast<int32_t*>(t + 0xa8));
}

}  // namespace

extern "C" void RtGraphics_GraphicsMeshTriangleList_Serialize(void* self, Serializer* outer) {
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
//   kScalarInt  (&n)  where n = this->_triangleLists._size
//     if reading: Array<TriangleList>::SetSize(this+0x50, n)
//     for each of the n elements (stride 0xb0):
//         kScalarFloat(e+0x10)
//         String::Serialize(e+0x00, &ser)
//         0x18005d0f0(e+0x08, &ser)
//         *** if (codec->GetVersion() == 0) TriangleList::Serialize(e, &ser) ***
//   kScalarInt  (&n)  where n = this->_innerArrays._size
//     if reading: Array<Array<T>>::SetSize(this+0x70, n)
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
extern "C" bool RtGraphics_GraphicsMesh_Serialize(void* self, Stream* stream) {
    uint8_t* const d = static_cast<uint8_t*>(self);

    Serializer ser;
    ReaderWriter* const rw = MakeCodec(stream, &ser, 1);
    const bool reading = (stream->direction == rt::io::kDirectionReading);

    *SerializeCounter()     = 0;
    *SerializeCurrentMesh() = self;

    StringSerialize(d + kName, &ser);
    EmitFloatArray(rw, 3, reinterpret_cast<float*>(d + kBoundsMin));
    EmitFloatArray(rw, 3, reinterpret_cast<float*>(d + kBoundsMax));
    EmitInt  (rw, reinterpret_cast<int32_t*>(d + kInt40));
    EmitFloat(rw, reinterpret_cast<float*>(d + kFloat44));
    EmitFloat(rw, reinterpret_cast<float*>(d + kFloat48));

    // ---- the triangle lists -------------------------------------------------
    {
        int32_t n = ArrayAt(self, kTriangleLists)->size;
        EmitInt(rw, &n);
        if (reading) ByAddr2(0x18005d840ull, d + kTriangleLists, n);

        uint8_t* e = ArrayAt(self, kTriangleLists)->data;
        for (int32_t i = 0; i < n; ++i, e += 0xb0) {
            EmitFloat(rw, reinterpret_cast<float*>(e + 0x10));
            StringSerialize(e, &ser);
            reinterpret_cast<SerMemberFn>(
                ResolveAddr(Module::Runtime, 0x18005d0f0ull))(e + 8, &ser);
            if (CodecVersion(rw) == 0)
                RtGraphics_GraphicsMeshTriangleList_Serialize(e, &ser);
        }
    }

    // ---- the inner arrays ---------------------------------------------------
    {
        int32_t n = ArrayAt(self, kInnerArrays)->size;
        EmitInt(rw, &n);
        if (reading) ByAddr2(0x18005cda0ull, d + kInnerArrays, n);

        ArrayHdrView* e = reinterpret_cast<ArrayHdrView*>(ArrayAt(self, kInnerArrays)->data);
        for (int64_t i = 0; i < static_cast<int64_t>(n); ++i, ++e) {
            int32_t m = e->size;
            EmitInt(rw, &m);
            if (reading) ByAddr2(0x18005cc60ull, e, m);
            ByAddrSerN(0x18005d4a0ull, &ser, m, e->data);
        }
    }

    // ---- the nodes ----------------------------------------------------------
    {
        int32_t n = ArrayAt(self, kNodes)->size;
        EmitInt(rw, &n);
        if (reading) ByAddr2(0x18005aa30ull, d + kNodes, n);
        ByAddrSerN(0x18005c120ull, &ser, n, ArrayAt(self, kNodes)->data);
    }

    EmitFloatArray(rw, 3, reinterpret_cast<float*>(d + kBoxMin));
    EmitFloatArray(rw, 3, reinterpret_cast<float*>(d + kBoxMax));

    if (CodecVersion(rw) >= 1) PlatformSerialize(self, &ser);

    DestroyCodec(rw);
    return true;                                              // MOV AL,1
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
