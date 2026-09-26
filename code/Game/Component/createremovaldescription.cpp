// code/Game/Component/createremovaldescription.cpp
// Clean reimplementation of CreateRemovalDescription (entity description block).
//
// CreateRemovalDescription (0x48 bytes) — removal/demolish placement tool.
// Extends Object::External (0x20 base). Vtable at 0x140162d80. Parent "Description".
//   +0x20: bool
//   +0x28: void* heap ExternalPtr array base (8-byte elements)
//   +0x30: int   count
//   +0x38: ExternalPtr (single)
//   +0x40..+0x43: 4 bools
//
// Factory  : MemAlloc 0x48 + External ctor + vtable + field defaults (zero).
// Serialize (vtable +0x28, 0x140119ac0 CreateRemovalDescription_Slot5), stream order:
//   bool  @ +0x20
//   int count (local copy of +0x30)
//   [if loading: resize array @+0x28 to count via FUN_140001bb0 = Array<ptr>::SetSize]
//   ExternalRefArray(count) @ *(+0x28)  (FUN_1400049c0 / fn_ExternalRefArray_Serialize)
//   ExternalPtr @ +0x38  (FUN_140002340 / fn_ExternalPtr_BuildSerialize)
//   bool  @ +0x40   bool @ +0x41   bool @ +0x42   bool @ +0x43
// GetVersion (vtable +0x18, 0x140119bc0): mov eax,1; ret -> version 1.
// Dtor (vtable +0x00, 0x140032ca0): MemFree(array @+0x28) + External::~External + free.
//
// NOTE: count is serialized into a local; the on-disk count then drives array resize +
// the per-element load. The resize is `Collection::Array<Ptr8>::SetSize` (B.3.2).
// fn_ExternalRefArray_Serialize -> exe FUN_1400049c0, null in standalone until 0x1400049c0
// is registered (REPORTED). param4 is a dead init -> nullptr safe.
//
// Field knowledge harvested from engine/reimpl/createremovaldescription.cpp.
// Decompiled from Application-x64.exe (READ-ONLY recon).
//
// ENDGAME B.3.8 K1: a REAL class (CreateRemovalDescription.h) with a compiler-generated vtable;
// the hand-built 11-slot table and its RegisterExeAddr(0x140162d80) row are gone
// (nothing in the tree read that address back — B38_RULES D5).

#include "Component/CreateRemovalDescription.h"
#include "object/Internal.h"      // Object::detail::InternalMemAlloc / InternalMemFree
#include "io/Serializer.h"
#include "Object/factory_registration.h"
#include "engine_funcs.h"
#include "Component/coll_bridge.h"
#include <cstdint>
#include <new>

// ctor (inlined into create 0x140032b70 / createInPlace 0x140032be0):
// `??0External@Object@@` (Description()), the stamps (the compiler's), then
// the defaults.
CreateRemovalDescription::CreateRemovalDescription()
    : Description(),
      _b20(false),
      _refs(nullptr),
      _refCount(0),
      _refCap(0),
      _ref38(nullptr),
      _b40(false),
      _b41(false),
      _b42(false),
      _b43(false) {}

// dtor (slot 0 0x140032ca0 minus the stamps): frees the ExternalPtr array.
// The compiler then chains ~Description -> ~External.
CreateRemovalDescription::~CreateRemovalDescription() {
    if (fn_MemFreeStatic && _refs) fn_MemFreeStatic(_refs);   // heap ExternalPtr array
    _refs = nullptr;
    _refCount = 0;
    _refCap = 0;
}

// [0] 0x140032ca0: ~CreateRemovalDescription -> ~Description -> ~External, MemFree on &1.
Object::Base* CreateRemovalDescription::DeletingDtor(unsigned flags) {
    this->~CreateRemovalDescription();
    if (flags & 1) Object::detail::InternalMemFree(this);
    return this;
}

// [5] 0x140119ac0: stream order as retail.
bool CreateRemovalDescription::Serialize(void* stream) {
    if (!fn_ExternalRefArray_Serialize || !fn_ExternalPtr_BuildSerialize) return true;   // standalone: unwired doors
    IO::Serializer ser(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.8 bridge (External::Serialize takes void*)
    ser.Serialize(_b20, nullptr);
    int count = _refCount;
    ser.Serialize(count, nullptr);   // a local copy, as retail
    // On load (stream mode == 0): resize the heap ExternalPtr array at +0x28.
    if (*reinterpret_cast<void**>(&ser) && static_cast<int>(ser.GetDirection()) == 0) {
        // B.3.2: Collection::Array<T>::SetSize — retail 0x140001bb0, stride 8 align 8, new tail zeroed.
        // Element is the 8-byte ExternalPtr slot the ExternalRefArray serializer walks below.
        GameColl::AsArray<GameColl::Ptr8>(reinterpret_cast<void*>(&_refs)).SetSize(count);
    }
    fn_ExternalRefArray_Serialize(&ser, count, _refs, nullptr);   // ExtPtr[count] (0x1400049c0)
    fn_ExternalPtr_BuildSerialize(&_ref38, &ser);
    ser.Serialize(_b40, nullptr);
    ser.Serialize(_b41, nullptr);
    ser.Serialize(_b42, nullptr);
    ser.Serialize(_b43, nullptr);
    return true;
}

// [6] 0x140032c40 — PORT GAP (retail: DeletingDtor(0), then the ctor again on
// `this`, B38_RULES §2.2). The hand-built table had a `return 0` stub; kept so
// this commit changes no behaviour. D9 #10 transcribes it for every description.
Object::External* CreateRemovalDescription::Reconstruct() { return nullptr; }

// [7] 0x140001640 — PORT GAP (retail: the shared `DeletingDtor(0)` body). The
// hand-built table had a `return 0` stub; kept until D9 #10's commit.
void CreateRemovalDescription::Destruct() {}

// [8] 0x140004950 (shared by 19 tables): `MOV dword [RDX],0; MOV RAX,RDX; RET` —
// a data-only description builds no component.
Object::Type* CreateRemovalDescription::GetComponentType(Object::Type* ret) const {
    ret->_type = 0;
    return ret;
}

// [9] 0x140032be0: placement-new on `this` (folded with createInPlace). The old
// table held the same construction in this slot.
Object::External* CreateRemovalDescription::Construct() {
    return new (static_cast<void*>(this)) CreateRemovalDescription();
}

// create 0x140032b70: MemAlloc(0x48, 8), then the (inlined) ctor.
void* CreateRemovalDescription::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(CreateRemovalDescription), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) CreateRemovalDescription());
}
// createInPlace 0x140032be0.
void* CreateRemovalDescription::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) CreateRemovalDescription());
}

DEFINE_EXE_VTABLE(CreateRemovalDescription, "_ZTV24CreateRemovalDescription")
DEFINE_EXE_TYPE(CreateRemovalDescription, "Description")
