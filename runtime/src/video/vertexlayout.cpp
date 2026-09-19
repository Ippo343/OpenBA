// ============================================================================
// runtime/src/video/vertexlayout.cpp — Video::VertexLayout, the whole class
// ============================================================================
//
// ??_7VertexLayout@Video@@6B@ (retail 0x180157ba0), nine slots:
//
//   0  deleting dtor        0x18004e000  runtime/src/object/deleting_dtor.cpp row 52
//   1  TypeName             0x18004dcd0  here
//   2  Type                 0x18004dcb0  here
//   3  GetFileVersion       0x1800f5e60  here  (returns 3)
//   4  External::Compile    0x1800066e0  already ported
//   5  Serialize            0x1800f5e70  here
//   6  Reconstruct          0x18004de50  here
//   7  Destruct             0x18004de80  here
//   8  Construct            0x18004de70  here
//
// plus ??0VertexLayout@Video@@QEAA@XZ (0x1800f5d50), ~VertexLayout (0x1800f5df0)
// and the element serializer loop 0x1800563c0. GetElementCount / GetElement /
// GetStreamCount / Add / BeginAdd / EndAdd are non-virtual and not on the boot
// path; they are declared in the header and not ported here.
//
// ============================================================================
// ENDGAME B.3.5 BLOCK B — THE BODIES MOVED INTO THE MEMBERS, AND THIS FILE'S
// LOCAL STRUCTS ARE GONE
// ============================================================================
// `Video::VertexLayout` and its nested `Element` are a real C++ class, authored
// by block A in `runtime/types/video/VertexLayout.h` from retail bytes, deriving
// from the real `Object::CompiledExternal`. This file's anonymous-namespace
// `struct VertexLayout` / `struct VertexElement` — one of the two partial,
// private views block A's header replaced — ARE DELETED. Each body below is a
// member, defined in the `namespace Video` block at the bottom (B34_BRIEFS A.2),
// and each registered DOOR is a one-line QUALIFIED non-virtual delegation through
// a `reinterpret_cast` bridge, qualified because a live VertexLayout wears the
// MINTED `??_7VertexLayout@Video@@6B@` whose slots resolve back to these doors
// (B34_BRIEFS A.1).
//
// *** THE FIELD NAMES CHANGED AND THE WIRE DID NOT. ***
// Block A PROVED from retail that this file's `VertexElement` map was wrong in
// NAMING: `?Add@VertexLayout@Video@@` (0x1800f5a90) stores its five parameters at
// +0x08/+0x0c/+0x10/+0x14/+0x18 in declaration order, so the map is
//     +0x08 stream   +0x0c offset   +0x10 usage   +0x14 usageIndex   +0x18 format
// where this file used to read +0x0c as `usage`, +0x10 as `usageIndex`, +0x14 as
// `format` and +0x18 as `offset`. `?GetStreamCount@` (0x1800f5a30) and VideoDX11's
// `PlatformCreate` (0x180003ff0) agree with block A independently.
//
// The mistake was INERT because retail's element loop (0x1800563c0) writes the
// five fields POSITIONALLY, in address order, through a fixed sequence of
// reader/writer slots — Int, Int, UInt, Int, UInt — and the old permutation
// happened to preserve that sequence. So the rename must NOT reorder the loop:
// the loop below walks the same five offsets in the same order through the same
// five slots, and only the names moved. The pairing is written out field by field
// at the loop so a future edit cannot "tidy" `usage` into the Int slot because its
// C++ type is `int32_t`.
//
// `_platform` is now `_signature`, block A's proved name: VideoDX11 hands those
// bytes to `CreateInputLayout` as the vertex-shader INPUT SIGNATURE and aborts the
// whole of `PlatformCreate` when its `_size` is 0.
//
// Two runtime-internal bindings retire, both inside this block's file set:
//   * `RT_EXPORT_ONCE(ExtDtorFn, "??1External@Object@@UEAA@XZ")(self)` — the base
//     destructor is the destructor CHAIN's now, which is what retail's tail-JMP to
//     0x1800bc950 is (spelling 3);
//   * `RtAddr<ElemSerFn>(0x1800563c0)(...)` — Serialize reached its OWN registered
//     element loop by going out to the registry and back; it calls it directly
//     (spelling 4). The registration at that address is UNCHANGED.
// The ctor door stays a ctor door (B4b-2: it writes the MINTED vptr).
//
// Layout (sizeof 0x48) and every offset's evidence: runtime/types/video/VertexLayout.h.
//
// Serialize (0x1800f5e70): build the {stream, readerWriter} pair;
//   int n = _elements.size; rw->Int(&n); if reading SetSize(_elements, n) (0x180056240)
//   0x1800563c0(&ser, n, _elements.data)                 element loop, below
//   PlatformSerialize inlined: int m = _signature.size; rw->Int(&m);
//     if reading SetSize(_signature, m) (0x180029630); rw->UCharArray(m, _signature.data)
//   if reading: _data->vtable[2](_data)                  PlatformCreate
//   delete rw
// ============================================================================
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "../../funcs/rt_port.h"
#include "../../types/system/String.h"
#include "../../../standalone/resolve.h"
#include "../../types/collection/Collection.h"
#include "../../types/io/Serializer.h"
#include "../../types/object/External.h"
#include "../../types/video/VertexLayout.h"

namespace {

using rt::io::Serializer;
using rt::io::Stream;
using rt::io::ReaderWriter;
using Video::VertexLayout;

void* BaseVTable()             { return RT_DATA_SLOT(void*, "??_7Base@Object@@6B@"); }
void* ExternalVTable()         { return RT_DATA_SLOT(void*, "??_7External@Object@@6B@"); }
void* CompiledExternalVTable() { return RT_DATA_SLOT(void*, "??_7CompiledExternal@Object@@6B@"); }
void* VertexLayoutVTable()     { return RT_DATA_SLOT(void*, "??_7VertexLayout@Video@@6B@"); }
const uint8_t* VertexLayoutTypeInfo() {
    return RT_DATA_SLOT(const uint8_t*, "?_typeInfo@VertexLayout@Video@@0VBaseTypeInfo@Object@@A");
}
void* DeviceData() {
    void** slot = RT_DATA_SLOT(void**, "?_singleton@Device@Video@@0PEAV12@EA");
    void* device = slot ? *slot : nullptr;
    if (!device) return nullptr;
    return *reinterpret_cast<void**>(static_cast<uint8_t*>(device) + 0x100);
}
constexpr size_t kDeviceDataCreateVertexLayoutData = 0x70;   // DeviceData vtable slot 14

using Slot0Fn = void* (*)(void* self, int flag);
void CallSlot0(void* self, int flag) {
    void** vt = *reinterpret_cast<void***>(self);
    reinterpret_cast<Slot0Fn>(vt[0])(self, flag);
}
template <typename Fn>
Fn RtAddr(uintptr_t ghidra) { return reinterpret_cast<Fn>(ResolveAddr(Module::Runtime, ghidra)); }

// ??0VertexLayout@Video@@QEAA@XZ (0x1800f5d50) — the ctor DOOR's body, and the
// body `Construct` (0x18004de70) repeats behind a null test. NOT a constructor:
// its first act is writing the MINTED vptr, which no C++ constructor can do
// (B4b-2 / B35_RULES 11).
void ConstructBody(VertexLayout* self) {
    void** vp = reinterpret_cast<void**>(self);
    *vp = BaseVTable();
    *vp = ExternalVTable();
    self->_path.data      = nullptr;   // +0x08
    self->_refs._data     = nullptr;   // +0x10
    self->_refs._size     = 0;         // +0x18
    self->_refs._capacity = 0;
    *vp = CompiledExternalVTable();
    *vp = VertexLayoutVTable();
    self->_elements._data     = nullptr;    // +0x20
    self->_elements._size     = 0;          // +0x28
    self->_elements._capacity = 0;
    self->_signature._data     = nullptr;   // +0x38
    self->_signature._size     = 0;         // +0x40
    self->_signature._capacity = 0;
    self->Video::VertexLayout::PlatformConstruct();   // +0x30
}

using ScalarFn = void (*)(ReaderWriter*, void*, const char*);
using ArrayFn  = void (*)(ReaderWriter*, int, void*, const char*);

}  // namespace

// ---------------------------------------------------------------------------
// The registered doors. Each is one line over the member of the same name.
// ---------------------------------------------------------------------------
extern "C" uint32_t RtVideo_VertexLayout_Type(const void* selfv) {
    return reinterpret_cast<const VertexLayout*>(selfv)
               ->Video::VertexLayout::Type();                      // B.3.5 bridge
}
extern "C" const char* RtVideo_VertexLayout_TypeName(const void* selfv) {
    return reinterpret_cast<const VertexLayout*>(selfv)
               ->Video::VertexLayout::TypeName();                  // B.3.5 bridge
}
extern "C" int32_t RtVideo_VertexLayout_GetFileVersion(const void* selfv) {
    return reinterpret_cast<const VertexLayout*>(selfv)
               ->Video::VertexLayout::GetFileVersion();            // B.3.5 bridge
}

extern "C" void* RtVideo_VertexLayout_Ctor(void* selfv) {
    ConstructBody(reinterpret_cast<VertexLayout*>(selfv));         // B.3.5 bridge
    return selfv;
}
extern "C" void* RtVideo_VertexLayout_Construct(void* selfv) {
    // RETAIL'S NULL TEST STAYS IN THE DOOR (BLOCK_BRIEF "the shape"): `this` is
    // `nonnull` to the compiler, so the same test inside the member is both a
    // -Wnonnull-compare warning and an optimisation GCC may delete.
    if (selfv == nullptr) return nullptr;
    return reinterpret_cast<VertexLayout*>(selfv)
               ->Video::VertexLayout::Construct();                 // B.3.5 bridge
}
extern "C" void* RtVideo_VertexLayout_Reconstruct(void* selfv) {
    return reinterpret_cast<VertexLayout*>(selfv)
               ->Video::VertexLayout::Reconstruct();               // B.3.5 bridge
}
extern "C" void RtVideo_VertexLayout_Destruct(void* selfv) {
    reinterpret_cast<VertexLayout*>(selfv)
        ->Video::VertexLayout::Destruct();                         // B.3.5 bridge
}

// ??1VertexLayout@Video@@UEAA@XZ (0x1800f5df0)
extern "C" void RtVideo_VertexLayout_Dtor(void* selfv) {
    reinterpret_cast<VertexLayout*>(selfv)
        ->Video::VertexLayout::~VertexLayout();                    // B.3.5 bridge
}

extern "C" bool RtVideo_VertexLayout_Serialize(void* selfv, void* streamv) {
    return reinterpret_cast<VertexLayout*>(selfv)
               ->Video::VertexLayout::Serialize(streamv);          // B.3.5 bridge
}

// ============================================================================
// 0x1800563c0 — the per-element loop: (Serializer*, int count, Element* data)
//
// UNEXPORTED in retail and registered BY ADDRESS only, so it is not a member of
// anything: it stays a file-scope function, now typed with the real
// `Video::VertexLayout::Element` instead of this file's deleted local struct.
// `Serialize` below calls it DIRECTLY rather than through `ResolveAddr`.
//
// THE FIVE FIELDS AND THEIR READER/WRITER SLOTS, PAIRED EXPLICITLY. Retail walks
// the element in ADDRESS order through a fixed slot sequence, and the slot is NOT
// derivable from the field's C++ type — `usage` and `format` are `int32_t` and go
// through the UNSIGNED slot, `usageIndex` is `int32_t` and goes through the signed
// one. Getting any pair wrong re-encodes every `MeshLayout\*.rsc` and every save.
//
//     +0x00  name        String::Serialize
//     +0x08  stream      kScalarInt
//     +0x0c  offset      kScalarInt     (this file used to call this one `usage`)
//     +0x10  usage       kScalarUInt    (…and this one `usageIndex`)
//     +0x14  usageIndex  kScalarInt     (…and this one `format`)
//     +0x18  format      kScalarUInt    (…and this one `offset`)
// ============================================================================
extern "C" void RtVideo_VertexLayout_SerializeElements(Serializer* ser, int count, void* data) {
    ReaderWriter* rw = ser->readerWriter;
    VertexLayout::Element* e = static_cast<VertexLayout::Element*>(data);
    for (int i = 0; i < count; ++i, ++e) {
        // B.3.1 bridge: `Element::name` is `void*` because a real `System::String`
        // member would give Element a non-trivial destructor that runs IN ADDITION
        // to the by-hand release the ported `Array<Element>::~Array` already
        // performs (0x18004fa30); the eight bytes are a System::String.
        reinterpret_cast<System::String&>(e->name).Serialize(ser);
        reinterpret_cast<ScalarFn>(rw->vtable[rt::io::rw_vtbl::kScalarInt  / 8])(rw, &e->stream,     nullptr);
        reinterpret_cast<ScalarFn>(rw->vtable[rt::io::rw_vtbl::kScalarInt  / 8])(rw, &e->offset,     nullptr);
        reinterpret_cast<ScalarFn>(rw->vtable[rt::io::rw_vtbl::kScalarUInt / 8])(rw, &e->usage,      nullptr);
        reinterpret_cast<ScalarFn>(rw->vtable[rt::io::rw_vtbl::kScalarInt  / 8])(rw, &e->usageIndex, nullptr);
        reinterpret_cast<ScalarFn>(rw->vtable[rt::io::rw_vtbl::kScalarUInt / 8])(rw, &e->format,     nullptr);
    }
}

// ============================================================================
// THE MEMBER BODIES — `Video::VertexLayout::*`. At `namespace Video` scope and
// AFTER the anonymous namespace (B34_BRIEFS A.2).
// ============================================================================
namespace Video {

// slot 1 — the `_typeInfo` record's +0x08 (`BaseTypeInfo::name`, CONTENT-only).
const char* VertexLayout::TypeName() const {
    const char* v = nullptr; std::memcpy(&v, VertexLayoutTypeInfo() + 8, sizeof v); return v;
}
// slot 2 — the record's +0x00, 0xFFFFFFFF until Factory::Startup.
uint32_t VertexLayout::Type() const {
    uint32_t v = 0; std::memcpy(&v, VertexLayoutTypeInfo(), sizeof v); return v;
}

// slot 3 — 0x1800f5e60 is `MOV EAX,3 ; RET`. This port said 1 until the
// 2026-09-12 cutover walk: DLL-present the DLL's own vtable answered, so nothing
// noticed; DLL-absent every MeshLayout\*.rsc load failed External::Load's version
// check (badVer=65), every VideoProgram lost its VertexLayout, and
// Renderer::AddDrawItem silently dropped every draw item -- the black window.
int32_t VertexLayout::GetFileVersion() const { return 3; }

// slot 8 — ?Construct@VertexLayout@Video@@UEAAPEAVExternal@Object@@XZ 0x18004de70.
// `virtual`, not `override`: this branch introduces the slot (RULE B5-16). Retail's
// leading `TEST RCX,RCX / JZ` lives in the DOOR.
Object::External* VertexLayout::Construct() {
    ConstructBody(this);
    return this;
}

// slot 6 — 0x18004de50: destroy in place through slot 0 with flags=0 (no free),
// then rebuild.
//
// THE SLOT-0 CALL IS DELIBERATELY NOT `DeletingDtor(0)`. Our slot 0 is declared
// PURE (see the header), so the C++ spelling would be a virtual call on an
// abstract class that GCC is entitled to fold to `__cxa_pure_virtual`. `CallSlot0`
// reads the object's ACTUAL table — the minted one — which is where the real
// table-driven body is.
Object::External* VertexLayout::Reconstruct() {
    CallSlot0(this, 0);
    ConstructBody(this);
    return this;
}

// slot 7 — ?Destruct@VertexLayout@Video@@UEAAXXZ 0x18004de80.
void VertexLayout::Destruct() { CallSlot0(this, 0); }

// ??1VertexLayout@Video@@UEAA@XZ (0x1800f5df0)
//
// Retail's order: stamp ??_7VertexLayout, PlatformDestruct, ~Array<uint8> over
// `_signature`, ~Array<Element> over `_elements` (which releases the names), stamp
// ??_7CompiledExternal, tail-JMP ~External. The first stamp is written out by
// hand, naming the MINTED address, because GCC's own vptr store at the top of a
// polymorphic destructor names GCC's table while the mint rows and the rest of the
// tree expect the minted one. The last two steps are the destructor CHAIN's —
// `~CompiledExternal()` and `~External()` are real members
// (runtime/src/object/external_class.cpp), which is what retail's tail-JMP is.
//
// The two array teardowns call the REGISTERED retail bodies, which end in
// `ArrayDtorImpl` and zero `_data`/`_size`/`_capacity`
// (runtime/src/collection/array.cpp:225), so the compiler-emitted `~Array<T>` that
// runs after this body sees an empty header and frees nothing. Each half runs
// exactly once — the same decomposition `Object::External::~External` uses for
// `_refs`.
VertexLayout::~VertexLayout() {
    *reinterpret_cast<void**>(this) = VertexLayoutVTable();
    this->Video::VertexLayout::PlatformDestruct();
    using ArrayDtorFn = void* (*)(void*);
    RtAddr<ArrayDtorFn>(0x1800112a0ull)(&_signature);   // Array<uint8>::~Array
    RtAddr<ArrayDtorFn>(0x18004fa30ull)(&_elements);    // ~Array<Element> (releases names)
}

// ?PlatformConstruct@VertexLayout@Video@@AEAAXXZ (0x1800f5f90) — DeviceData slot
// 14 (+0x70), CreateVertexLayoutData.
void VertexLayout::PlatformConstruct() {
    void* dd = DeviceData();
    if (dd == nullptr) { _data = nullptr; return; }
    using CreateFn = void* (*)(void*, void*);
    void** vt = *reinterpret_cast<void***>(dd);
    _data = reinterpret_cast<CreateFn>(vt[kDeviceDataCreateVertexLayoutData / 8])(dd, this);
}
// ?PlatformCreate@VertexLayout@Video@@AEAAXXZ (0x1800f6060): _data->vtable[2](_data).
// VOID, unlike VertexProgram's and PixelProgram's bool — VideoDX11's 0x180003ff0
// falls off its end with no return value set.
void VertexLayout::PlatformCreate() {
    using Fn = void (*)(void*);
    void** vt = *reinterpret_cast<void***>(_data);
    reinterpret_cast<Fn>(vt[2])(_data);
}
// ?PlatformDestruct@VertexLayout@Video@@AEAAXXZ (0x1800f5fd0)
void VertexLayout::PlatformDestruct() {
    if (_data == nullptr) return;
    using Fn = void (*)(void*, int);
    void** vt = *reinterpret_cast<void***>(_data);
    reinterpret_cast<Fn>(vt[0])(_data, 1);
}

// slot 5 — ?Serialize@VertexLayout@Video@@UEAA_NAEAVStream@IO@@@Z (0x1800f5e70)
bool VertexLayout::Serialize(void* streamv) {
    Stream* stream = static_cast<Stream*>(streamv);

    Serializer ser = { stream, nullptr };
    void** encVt = *reinterpret_cast<void***>(stream->encoding);
    const int slot = (stream->direction == 0) ? rt::io::encoding_vtbl::kCreateReader
                                              : rt::io::encoding_vtbl::kCreateWriter;
    using MakeRwFn = ReaderWriter* (*)(void*, Serializer*, int);
    ReaderWriter* rw = reinterpret_cast<MakeRwFn>(encVt[slot / 8])(stream->encoding, &ser, 0);
    ser.readerWriter = rw;

    using SetSizeFn = void (*)(void*, int);
    int32_t n = _elements._size;
    reinterpret_cast<ScalarFn>(rw->vtable[rt::io::rw_vtbl::kScalarInt / 8])(rw, &n, nullptr);
    if (ser.stream->direction == 0) RtAddr<SetSizeFn>(0x180056240ull)(&_elements, n);
    // Retail CALLs 0x1800563c0; this IS that function, in this file.
    RtVideo_VertexLayout_SerializeElements(&ser, n, _elements._data);

    // PlatformSerialize (0x1800f5ff0), inlined by retail and inlined here.
    int32_t m = _signature._size;
    reinterpret_cast<ScalarFn>(rw->vtable[rt::io::rw_vtbl::kScalarInt / 8])(rw, &m, nullptr);
    if (ser.stream->direction == 0) RtAddr<SetSizeFn>(0x180029630ull)(&_signature, m);
    reinterpret_cast<ArrayFn>(rw->vtable[rt::io::rw_vtbl::kArrayUChar / 8])(
        rw, m, _signature._data, nullptr);

    if (stream->direction == 0)
        this->Video::VertexLayout::PlatformCreate();   // retail derefs _data unguarded
    if (rw != nullptr) {
        using DeleteFn = void (*)(ReaderWriter*, int);
        reinterpret_cast<DeleteFn>(rw->vtable[0])(rw, 1);
    }
    return true;
}

}  // namespace Video

int RtVideoVertexLayoutRegister() {
    int n = 0;
    RegisterRuntimeExport("?Type@VertexLayout@Video@@UEBAIXZ",           (void*)&RtVideo_VertexLayout_Type);           ++n;
    RegisterRuntimeAddr  (0x18004dcb0ull,                                (void*)&RtVideo_VertexLayout_Type);
    RegisterRuntimeExport("?TypeName@VertexLayout@Video@@UEBAPEBDXZ",    (void*)&RtVideo_VertexLayout_TypeName);       ++n;
    RegisterRuntimeAddr  (0x18004dcd0ull,                                (void*)&RtVideo_VertexLayout_TypeName);
    RegisterRuntimeExport("?GetFileVersion@VertexLayout@Video@@UEBAHXZ", (void*)&RtVideo_VertexLayout_GetFileVersion); ++n;
    RegisterRuntimeAddr  (0x1800f5e60ull,                                (void*)&RtVideo_VertexLayout_GetFileVersion);
    RegisterRuntimeExport("??0VertexLayout@Video@@QEAA@XZ",              (void*)&RtVideo_VertexLayout_Ctor);           ++n;
    RegisterRuntimeAddr  (0x1800f5d50ull,                                (void*)&RtVideo_VertexLayout_Ctor);
    RegisterRuntimeExport("?Construct@VertexLayout@Video@@UEAAPEAVExternal@Object@@XZ",   (void*)&RtVideo_VertexLayout_Construct);   ++n;
    RegisterRuntimeAddr  (0x18004de70ull,                                                 (void*)&RtVideo_VertexLayout_Construct);
    RegisterRuntimeExport("?Reconstruct@VertexLayout@Video@@UEAAPEAVExternal@Object@@XZ", (void*)&RtVideo_VertexLayout_Reconstruct); ++n;
    RegisterRuntimeAddr  (0x18004de50ull,                                                 (void*)&RtVideo_VertexLayout_Reconstruct);
    RegisterRuntimeExport("?Destruct@VertexLayout@Video@@UEAAXXZ",       (void*)&RtVideo_VertexLayout_Destruct);       ++n;
    RegisterRuntimeAddr  (0x18004de80ull,                                (void*)&RtVideo_VertexLayout_Destruct);
    RegisterRuntimeExport("??1VertexLayout@Video@@UEAA@XZ",              (void*)&RtVideo_VertexLayout_Dtor);           ++n;
    RegisterRuntimeAddr  (0x1800f5df0ull,                                (void*)&RtVideo_VertexLayout_Dtor);
    RegisterRuntimeExport("?Serialize@VertexLayout@Video@@UEAA_NAEAVStream@IO@@@Z", (void*)&RtVideo_VertexLayout_Serialize); ++n;
    RegisterRuntimeAddr  (0x1800f5e70ull,                                           (void*)&RtVideo_VertexLayout_Serialize);
    RegisterRuntimeAddr  (0x1800563c0ull,                                (void*)&RtVideo_VertexLayout_SerializeElements); ++n;
    return n;
}
