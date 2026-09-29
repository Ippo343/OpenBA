// code/Game/Component/ambientenvironmentdescription.cpp
// Clean reimplementation of AmbientEnvironmentDescription (entity description block).
//
// AmbientEnvironmentDescription (0x38 bytes) — ambient-zone environment override.
// Extends Object::External (0x20 base). Vtable at 0x140158bf0. Parent "Description".
//   +0x20: void* dynamic entry-array base (0x28-byte / 5-qword elements)
//   +0x28: int   count   (+0x2c cap)
//   +0x30: float (default 0x42000000 = 32.0f)
//   +0x34: float (default 0x42340000 = 45.0f)
//   Each array element (0x28 bytes):
//     +0x00 enum   +0x08 ExternalPtr  +0x10 ExternalPtr  +0x18 ExternalPtr  +0x20 int
//
// Factory  : MemAlloc 0x38 + External ctor + vtable + defaults
//            (+0x20=0, +0x28=0, +0x30=32.0f, +0x34=45.0f).
// Serialize (vtable +0x28, 0x140043950), stream order:
//   int count @ +0x28
//   [if loading: FUN_140002540(+0x20, count)  resize 0x28-byte-element array]
//   for each element (stride 0x28):
//     enum        @ elem+0x00  (encoding vtable +0x98)
//     ExternalPtr @ elem+0x08  (FUN_140002340 / fn_ExternalPtr_BuildSerialize)
//     ExternalPtr @ elem+0x10  (FUN_1400019a0)
//     ExternalPtr @ elem+0x18  (FUN_1400019a0)
//     int         @ elem+0x20
//   float @ +0x30
//   float @ +0x34
// GetVersion (vtable +0x18, 0x140043a70): mov eax,2; ret -> version 2.
// Dtor (vtable +0x00, 0x140002100): External::~External + free.
//
// The exe helpers FUN_1400019a0 (single ambient ExternalPtr serialize) and
// FUN_140002540 (0x28-stride array resize) are called directly (B.3R).
//
// Decompiled from Application-x64.exe (READ-ONLY recon).
//
// ENDGAME B.3.8 K1: a REAL class (AmbientEnvironmentDescription.h) with a compiler-generated vtable;
// the hand-built 11-slot table and its RegisterExeAddr(0x140158bf0) row are gone
// (nothing in the tree read that address back — B38_RULES D5).

#include "Component/AmbientEnvironmentDescription.h"
#include "Component/AmbientEnvironmentComponent.h"   // B.3.8 CB1a (TypeId)
#include "object/Internal.h"      // Object::detail::InternalMemAlloc / InternalMemFree
#include "io/Serializer.h"
#include "Object/factory_registration.h"
#include "object/External.h"
#include "object/TypeInfo.h"
#include <cstdint>
#include <new>
#include "Component/exe_bodies.h"
#include "Game/exe_bodies.h"
#include "system/MemoryAllocator.h"

// ============================================================
// Reimpl of FUN_140002540 (grow/shrink the 0x28-byte-element entry array)
// ============================================================
// Resizes the array {ptr@arr+0, count@arr+8, cap@arr+0xc} (stride 0x28). On grow
// past capacity: cap = newCount (or newCount*3 when growing by exactly one from a
// non-empty array), MemAlloc(cap*0x28, 8), copy existing elements, zero-init the new
// tail with elem+0x20 = 1 default, free the old block. 1:1 port of the exe helper.
// Registered at 0x140002540 (reg_component_visuals.cpp) and called by
// ambient_externalptr_serialize.cpp: a free function, not a member.
extern "C" void AmbientArrayResize_Reimpl(void* arrayBase, int newCount) {
    uint8_t* arr = reinterpret_cast<uint8_t*>(arrayBase);
    int oldCount = *(int*)(arr + 8);
    if (newCount == oldCount) return;

    void* oldPtr = *(void**)(arr + 0);
    *(int*)(arr + 8) = newCount;
    if (*(int*)(arr + 0xc) < newCount) {
        *(int*)(arr + 0xc) = newCount;
        if (newCount == oldCount + 1 && oldCount != 0) {
            *(int*)(arr + 0xc) = newCount * 3;
        }
        if (::System::MemoryAllocator::_singleton) {
            void* newPtr = ::System::MemoryAllocator::MemAlloc((uint64_t)(*(int*)(arr + 0xc)) * 0x28, 8);
            *(void**)(arr + 0) = newPtr;
        }
    }

    int start = 0;
    if (oldPtr == *(void**)(arr + 0)) start = oldCount;  // grown in place: keep existing

    uint8_t* base = *(uint8_t**)(arr + 0);
    for (int i = start; i < newCount; ++i) {
        uint8_t* dst = base + (intptr_t)i * 0x28;
        if (!dst) continue;
        if (i < oldCount) {
            // copy old element (5 qwords = 0x28 bytes)
            const uint8_t* src = reinterpret_cast<const uint8_t*>(oldPtr) + (intptr_t)i * 0x28;
            for (int q = 0; q < 5; ++q)
                *(uint64_t*)(dst + q * 8) = *(const uint64_t*)(src + q * 8);
        } else {
            // zero-init new element, +0x20 = 1 default
            *(uint32_t*)(dst + 0x00) = 0;
            *(uint64_t*)(dst + 0x08) = 0;
            *(uint64_t*)(dst + 0x10) = 0;
            *(uint64_t*)(dst + 0x18) = 0;
            *(uint32_t*)(dst + 0x20) = 1;
        }
    }

    if (oldPtr != *(void**)(arr + 0) && oldPtr) {
        ::System::MemoryAllocator::MemFree(oldPtr);
    }
}

// ctor (inlined into create 0x140001f40 / createInPlace 0x140001fb0):
// `??0External@Object@@` (Description()), the stamps (the compiler's), then
// the defaults.
AmbientEnvironmentDescription::AmbientEnvironmentDescription()
    : Description(),
      _entries(nullptr),
      _entryCount(0),
      _entryCap(0),
      _f30(32.0f),
      _f34(45.0f) {}

// dtor (slot 0 0x140002100 minus the stamps): frees the entry array.
// The compiler then chains ~Description -> ~External.
AmbientEnvironmentDescription::~AmbientEnvironmentDescription() {
    if (_entries) ::System::MemoryAllocator::MemFree(_entries);   // dynamic entry array
    _entries = nullptr;
    _entryCount = 0;
    _entryCap = 0;
}

// [5] 0x140043950: stream order as retail.
bool AmbientEnvironmentDescription::Serialize(void* stream) {
    IO::Serializer ser(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.8 bridge (External::Serialize takes void*)

    int count = _entryCount;
    ser.Serialize(count, nullptr);   // int count +0x28

    // On load: resize the 0x28-byte-element array at +0x20 (0x140002540).
    if (static_cast<int>(ser.GetDirection()) == 0) {
        AmbientArrayResize_Reimpl(&_entries, count);
    }

    if (count > 0) {
        AmbientEnvironmentBiome* elem = _entries;
        for (int i = 0; i < count; ++i, ++elem) {
            ser.Serialize(elem->terrainMask, nullptr);                          // enum
            Reimpl_ExternalPtr_Serialize_StringTable(&elem->entityFilter, &ser);  // ExternalPtr, 0x140002340
            AmbientExternalPtrSerialize_Reimpl(&elem->daySound, &ser);           // ExternalPtr, 0x1400019a0
            AmbientExternalPtrSerialize_Reimpl(&elem->nightSound, &ser);         // ExternalPtr, 0x1400019a0
            ser.Serialize(elem->weight, nullptr);                               // int
        }
    }

    // +0x30 / +0x34: floats written through the 4-byte INT door (the tree's
    // choice; same four bytes either way). LEFT: which overload retail calls is
    // not read here (an H8 question).
    ser.Serialize(*reinterpret_cast<int*>(&_f30), nullptr);
    ser.Serialize(*reinterpret_cast<int*>(&_f34), nullptr);
    return true;
}

// [6] 0x140002010: retail destroys and rebuilds in place: a virtual `this->~T()` (retail: slot 0, flags 0),
// then the ctor again on `this` (D9 #10).
Object::External* AmbientEnvironmentDescription::Reconstruct() {
    this->~AmbientEnvironmentDescription();
    return new (static_cast<void*>(this)) AmbientEnvironmentDescription();
}

// [7] 0x140001640: retail's shared body, a virtual `this->~T()` (retail: slot 0, flags 0) (D9 #10).
void AmbientEnvironmentDescription::Destruct() { this->~AmbientEnvironmentDescription(); }

// [8] 0x140002070: AmbientEnvironmentComponent's type id through the hidden return slot.
Object::Type* AmbientEnvironmentDescription::GetComponentType(Object::Type* ret) const {
    ret->_type = AmbientEnvironmentComponent::TypeId();
    return ret;
}

// [9] 0x140001fb0: placement-new on `this` (folded with createInPlace). The old
// table held the same construction in this slot.
Object::External* AmbientEnvironmentDescription::Construct() {
    return new (static_cast<void*>(this)) AmbientEnvironmentDescription();
}

// create 0x140001f40: MemAlloc(0x38, 8), then the (inlined) ctor.
void* AmbientEnvironmentDescription::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(AmbientEnvironmentDescription), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) AmbientEnvironmentDescription());
}
// createInPlace 0x140001fb0.
void* AmbientEnvironmentDescription::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) AmbientEnvironmentDescription());
}

DEFINE_EXE_TYPE(AmbientEnvironmentDescription, "Description")
