// ============================================================================
// runtime/src/collision2d/surface.cpp — Collision2D::Surface, the whole class
// ============================================================================
//
// Vtable batch 8. ??_7Surface@Collision2D@@6B@ (retail), nine slots:
//
//   0  deleting dtor        0x180064ea0  Surface::DeletingDtor (Surface.h)
//   1  TypeName             0x180064a70  here
//   2  Type                 0x180064a50  here
//   3  GetFileVersion       0x18010ca30  here  (returns 1)
//   4  Compile              -- NOT a Surface slot: retail's table holds
//                              External::Compile (0x1800066e0) there
//   5  Serialize            0x18010ca40  here
//   6  Reconstruct          0x180064c70  here
//   7  Destruct             0x180064d40  here
//   8  Construct            0x180064ce0  here
//
// plus ~Surface (0x18010ca00) here.
//
// ENDGAME B.3.9 ENG (B39_RULES D1/D2): the file-local POD became the real class
// `Collision2D::Surface` (runtime/types/collision2d/Surface.h). Its constructor
// holds the factory rows' stores, its DeletingDtor the retired RT_DELDTOR row,
// and the slot bodies are its members: every live Surface wears the COMPILER's
// table. The eight slot doors, the ??1 door, `ConstructBody` and `CallSlot0`
// went with the minted `??_7Surface@Collision2D@@6B@` row and the FACTREST
// pair (rt_factories_collision2d.{cpp,inc}).
// ============================================================================
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>

#include "../../funcs/rt_port.h"
#include "../../types/io/Serializer.h"
#include "../../types/collision2d/Surface.h"

namespace {

using rt::io::ReaderWriter;
using rt::io::Serializer;
using rt::io::Stream;

}  // namespace

namespace Collision2D {

// TypeId 0x180064a60 / [2] 0x180064a50 / [1] 0x180064a70: DECLARE_TYPE's
// loads over the class's own record (collision2d_type_records.cpp; ENDGAME
// B.3.9 TI-REST).
// [3] 0x18010ca30: `MOV EAX,1 ; RET`.
int32_t Surface::GetFileVersion() const { return 1; }

// [8] 0x180064ce0 -- the inlined constructor, i.e. placement construction.
// (Retail's null test sat in the retired door; a virtual call always has a
// `this`.) `::new`: the class declares its own placement new.
Object::External* Surface::Construct() {
    return ::new (this) Surface();
}
// [6] 0x180064c70: slot0(this, 0) dispatched VIRTUALLY, then the same stores.
Object::External* Surface::Reconstruct() {
    this->~Surface();
    return ::new (this) Surface();
}
// [7] 0x180064d40: `MOV RAX,[RCX] ; XOR EDX,EDX ; JMP [RAX]`.
void Surface::Destruct() { this->~Surface(); }

// ?CreateNewObject@Surface@Collision2D@@SAPEAVBase@Object@@XZ  0x180064b60
// The FACTREST row this replaces: MemAlloc of 0x30 bytes, align 8 (the locked
// `?MemAlloc@MemoryAllocator@System@@` Object::detail::InternalMemAlloc
// reaches); retail's null branch; then the inlined constructor.
Object::Base* Surface::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(Surface), 8);
    if (p == nullptr) return nullptr;
    return ::new (p) Surface();
}
// ?CreateInPlaceObject@Surface@Collision2D@@SAPEAVBase@Object@@PEAX@Z  0x180064c10
Object::Base* Surface::CreateInPlaceObject(void* place) {
    if (place == nullptr) return nullptr;
    return ::new (place) Surface();
}

// ??1Surface@Collision2D@@UEAA@XZ (0x18010ca00): restamp own vtable, chain to
// ~External. No owned resource of its own -- three plain uint32 fields. The
// restamp and the chain are the compiler's.
Surface::~Surface() {}

// ?Serialize@Surface@Collision2D@@UEAA_NAEAVStream@IO@@@Z (0x18010ca40)
bool Surface::Serialize(void* streamv) {
    Stream* stream = static_cast<Stream*>(streamv);

    Serializer ser = { stream, nullptr };
    void** encVt = *reinterpret_cast<void***>(stream->encoding);
    const int slot = (stream->direction == 0) ? rt::io::encoding_vtbl::kCreateReader
                                              : rt::io::encoding_vtbl::kCreateWriter;
    using MakeRwFn = ReaderWriter* (*)(void*, Serializer*, int);
    ReaderWriter* rw = reinterpret_cast<MakeRwFn>(encVt[slot / 8])(stream->encoding, &ser, 0);
    ser.readerWriter = rw;

    // The `rt::io::ReaderWriter` POD and `IO::ReaderWriter` are asserted
    // interchangeable (Serializer.h); SerializeUInt is +0x98.
    IO::ReaderWriter* rwc = reinterpret_cast<IO::ReaderWriter*>(rw);   // B.3.x bridge
    rwc->SerializeUInt(_f20, nullptr);
    rwc->SerializeUInt(_f28, nullptr);
    rwc->SerializeUInt(_f24, nullptr);

    if (rw != nullptr) {
        delete rwc;
    }
    return true;
}

}  // namespace Collision2D

// B3R (registry teardown, W1 RT-Core): every row this function registered had
// no reader by name or by address, so they went. It registers nothing and
// stays defined only because standalone/resolve_standalone.cpp's
// RuntimePortInit() still calls it (the coordinator removes the call).
int RtCollision2DSurfaceRegister() {
    return 0;
}
