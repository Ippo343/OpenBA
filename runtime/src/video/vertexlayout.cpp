// ============================================================================
// runtime/src/video/vertexlayout.cpp — Video::VertexLayout, the whole class
// ============================================================================
//
// ??_7VertexLayout@Video@@6B@ (retail 0x180157ba0), nine slots:
//
//   0  deleting dtor        0x18004e000  VertexLayout.h (inline, B.3.9)
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
// ENDGAME B.3.9 block VID: the class is CONCRETE and wears the compiler's table.
// The ctor door, the dtor door and the seven slot doors are GONE (their callers
// were the minted table, the FACTREST interpreter and the RT_DELDTOR row, all
// retired with ??_7VertexLayout@Video@@6B@); the factory pair is the class's own.
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
#include <new>

#include "../../types/video/Device.h"   // ENDGAME B.3.9 VID: Video::Device::_singleton
#include "../../types/system/String.h"
#include "../../types/collection/Collection.h"
#include "../../types/io/Modification.h"   // RtIO::RtColl_ArrayU8Dtor_1800112a0
#include "../../types/io/Serializer.h"
#include "../../types/object/External.h"
#include "../../types/video/VertexLayout.h"

namespace {

using rt::io::Serializer;
using rt::io::Stream;
using rt::io::ReaderWriter;
using Video::VertexLayout;

void* ActiveDeviceDataPtr() {
    void** slot = reinterpret_cast<void**>(&::Video::Device::_singleton);
    void* device = slot ? *slot : nullptr;
    if (!device) return nullptr;
    return *reinterpret_cast<void**>(static_cast<uint8_t*>(device) + 0x100);
}
constexpr size_t kDeviceDataCreateVertexLayoutData = 0x70;   // DeviceData vtable slot 14

// B3X W0: the ReaderWriter members, by name on the real class (the
// `rt::io::ReaderWriter` POD and `IO::ReaderWriter` are asserted
// interchangeable in Serializer.h). `name` is nullptr, as retail passes.
inline IO::ReaderWriter* AsRw(ReaderWriter* rw) {
    return reinterpret_cast<IO::ReaderWriter*>(rw);   // B.3.x bridge
}

}  // namespace

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
        AsRw(rw)->SerializeInt(e->stream, nullptr);
        AsRw(rw)->SerializeInt(e->offset, nullptr);
        AsRw(rw)->SerializeUInt(*reinterpret_cast<uint32_t*>(&e->usage), nullptr);
        AsRw(rw)->SerializeInt(e->usageIndex, nullptr);
        AsRw(rw)->SerializeUInt(*reinterpret_cast<uint32_t*>(&e->format), nullptr);
    }
}

// ============================================================================
// THE MEMBER BODIES — `Video::VertexLayout::*`. At `namespace Video` scope and
// AFTER the anonymous namespace (B34_BRIEFS A.2).
// ============================================================================
namespace Video {

// slot 1, TypeId 0x18004dcc0, slot 2 0x18004dcb0: DECLARE_TYPE's over the
// class's own record (video_type_records.cpp; ENDGAME B.3.9 TI-REST).

// slot 3 — 0x1800f5e60 is `MOV EAX,3 ; RET`. This port said 1 until the
// 2026-09-12 cutover walk: DLL-present the DLL's own vtable answered, so nothing
// noticed; DLL-absent every MeshLayout\*.rsc load failed External::Load's version
// check (badVer=65), every VideoProgram lost its VertexLayout, and
// Renderer::AddDrawItem silently dropped every draw item -- the black window.
int32_t VertexLayout::GetFileVersion() const { return 3; }

// slot 8 — ?Construct@VertexLayout@Video@@UEAAPEAVExternal@Object@@XZ 0x18004de70:
// `TEST RCX,RCX / JZ` then the ctor. `virtual`, not `override`: this branch
// introduces the slot (RULE B5-16). `::new` because the class declares its own
// (unported) placement operator new.
Object::External* VertexLayout::Construct() {
    return ::new (this) VertexLayout();
}

// slot 6 — 0x18004de50: slot 0 with flags 0 (destroy in place), then the ctor.
Object::External* VertexLayout::Reconstruct() {
    this->~VertexLayout();
    return ::new (this) VertexLayout();
}

// slot 7 — ?Destruct@VertexLayout@Video@@UEAAXXZ 0x18004de80: slot 0, flags 0.
void VertexLayout::Destruct() { this->~VertexLayout(); }

// ??1VertexLayout@Video@@UEAA@XZ (0x1800f5df0)
//
// Retail's order: stamp ??_7VertexLayout, PlatformDestruct, ~Array<uint8> over
// `_signature`, ~Array<Element> over `_elements` (which releases the names), stamp
// ??_7CompiledExternal, tail-JMP ~External. The stamps are the compiler's (entry
// to each destructor of the chain) and the tail-JMP is the chain itself.
//
// The two array teardowns call the REGISTERED retail bodies, which end in
// `ArrayDtorImpl` and zero `_data`/`_size`/`_capacity`
// (runtime/src/collection/array.cpp:225), so the compiler-emitted `~Array<T>` that
// runs after this body sees an empty header and frees nothing. Each half runs
// exactly once — the same decomposition `Object::External::~External` uses for
// `_refs`.
VertexLayout::~VertexLayout() {
    this->Video::VertexLayout::PlatformDestruct();
    RtIO::RtColl_ArrayU8Dtor_1800112a0(&_signature);     // 0x1800112a0 Array<uint8>::~Array
    rtcoll::RtColl_ArrayDtorElem_18004fa30(&_elements);  // 0x18004fa30 ~Array<Element> (releases names)
}

// ?CreateNewObject@VertexLayout@Video@@SAPEAVBase@Object@@XZ  0x18004ddc0:
// MemAlloc(0x48, 8), null branch, the ctor (B39_RULES D2).
Object::Base* VertexLayout::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(VertexLayout), 8);
    if (p == nullptr) return nullptr;
    return ::new (p) VertexLayout();
}
// ?CreateInPlaceObject@VertexLayout@Video@@SAPEAVBase@Object@@PEAX@Z  0x18004de40
Object::Base* VertexLayout::CreateInPlaceObject(void* storage) {
    return storage ? ::new (storage) VertexLayout() : nullptr;
}

// ?PlatformConstruct@VertexLayout@Video@@AEAAXXZ (0x1800f5f90) — DeviceData slot
// 14 (+0x70), CreateVertexLayoutData.
void VertexLayout::PlatformConstruct() {
    void* dd = ActiveDeviceDataPtr();
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

    int32_t n = _elements._size;
    AsRw(rw)->SerializeInt(n, nullptr);
    if (ser.stream->direction == 0) rtcoll::RtColl_ArraySetSize_180056240(&_elements, n);
    // Retail CALLs 0x1800563c0; this IS that function, in this file.
    RtVideo_VertexLayout_SerializeElements(&ser, n, _elements._data);

    // PlatformSerialize (0x1800f5ff0), inlined by retail and inlined here.
    int32_t m = _signature._size;
    AsRw(rw)->SerializeInt(m, nullptr);
    if (ser.stream->direction == 0) rtcoll::RtColl_ArraySetSize_180029630(&_signature, m);
    AsRw(rw)->SerializeUCharArray(
        m, reinterpret_cast<unsigned char*>(_signature._data), nullptr);

    if (stream->direction == 0)
        this->Video::VertexLayout::PlatformCreate();   // retail derefs _data unguarded
    if (rw != nullptr) {
        delete AsRw(rw);
    }
    return true;
}

}  // namespace Video
