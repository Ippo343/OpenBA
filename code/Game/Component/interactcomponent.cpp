// code/Game/Component/interactcomponent.cpp
// Clean reimplementation of InteractComponent lifecycle methods.
//
// InteractComponent (0x38 bytes) -- direct Component subclass.
// Manages interactive object selection (clickable entities).
// Holds a dynamic array of entity references at +0x28 (ptr) / +0x30 (count).
//
// Vtable at 0x1401638b0:
//   [3] Shutdown = 0x140123340 -- serialization with entity array + IsDerivedFrom
//   [6] Draw     = 0x140123110 -- 140-instruction rebuild of interact list from scene
//
// Decompiled from Application-x64.exe using Ghidra.

#include "Component/interactcomponent_funcs.h"
#include "Component/MultiObjectComponent.h"
#include "Component/coll_bridge.h"
#include "object/TypeInfo.h"
#include "system/MemoryAllocator.h"   // System::MemoryAllocator::MemAlloc / MemFree
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch A
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <new>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B
#include "Component/exe_bodies.h"
#include "Entity/ComponentEntity.h"   // ComponentEntity::FindComponent<T>

// Helpers, called by name (B3R): Object::Base::IsDerivedFrom (import slot 0x140152a98),
// InteractComponent_FindWorkplaceComponent (0x140004740, below). 0x1400032e0 was
// Collection::Array<void*>::SetSize(n) monomorphised into the exe (ENDGAME B.3.2 step 2)
// — converted at its call site below.

// ============================================================
// InteractComponent::Shutdown (vtable slot [3], 0x140123340)
// ============================================================
// Serializes Component base + dynamic array of InternalBasePtr elements.
//
// Original pseudocode:
//   Component_Serialize(self, stream)
//   Serializer local(stream, 0)
//   int count = self->field30
//   Serialize(&count, nullptr)
//   if (serializer loading): IBPArray_Resize(&self->field28, count)
//   for each element in array:
//     IBP::Serialize(element, serializer)
//     if element != null && !IsDerivedFrom(element, InteractTypeId):
//       element = 0
//   ~Serializer()

// Engine-wide "am I loading?" test: 0 = loading, non-zero = saving. Reached by
// dereferencing the Serializer to its stream first. Same idiom as
// marketwork_bst_b290.cpp's SerIsLoading and pasturecomponent.cpp et al.
static inline bool SerializerIsLoading(void* serializer) {
    return static_cast<int>(reinterpret_cast<IO::Serializer*>(serializer)->GetDirection()) == 0;   // B.3.4 bridge  (IO::Serializer::GetDirection, retail 0x180006210)
}

// ============================================================
// Reimpl of FUN_140004740 (find the WorkplaceComponent (type DWORD_1401c7ac0)
//   on an entity and return it wrapped in an InternalBasePtr via retBuf)
// ============================================================
// Shared callee: also used from buildcomponent.cpp (BuildComponent::Draw).
void InteractComponent_FindWorkplaceComponent(uintptr_t entity, void* retBuf) {
    new (retBuf) ::Object::InternalPtr<MultiObjectComponent>(
        reinterpret_cast<ComponentEntity*>(entity)->FindComponent<MultiObjectComponent>());   // B4 FORWARD: receiver typed by the callers' blocks
}

// ============================================================
// InteractComponent::Draw (vtable slot [6], 0x140123110)
// ============================================================
// Reimpl of FUN_140123110: rebuild the interact-use-location list from the
// owner's MultiObject children. For each child's MapComponent (DAT_1401c7900),
// scan its job hosts for an InteractComponent and append it to the +0x28 list.
// Faithful port of the Ghidra decompilation.

// ============================================================================
// ENDGAME B.3.8 VIS — InteractComponent as a REAL class (InteractComponent.h).
//
// Before: a 48-slot InteractComponent_InertVtable filled per create,
// Inert_ScalarDtor in slot 0 (only ~Internal).
// After: the compiler's 16-slot vtable and a real ctor/dtor chain
// (B38_RULES D1, D2); slot bodies are the ones the old table installed.
// ============================================================================
#include "Object/factory_registration.h"
#include "Component/InteractComponent.h"

// Slot 6 is the ON-ADDED broadcast: ComponentEntity_OnAddedPass (FUN_1400c2f60, the
// tail of EntityList::RegisterEntity 0x1400c48a0) calls it on every component whose
// capability flags (+0x20) have bit 0 — InteractComponent's 0x81 qualifies. The body
// rebuilds the owner's interact use-location list (+0x28 array / +0x30 count) from the
// owner's MultiObjectComponent children; FUN_140122b00 (CalcChildTargetPos) recurses
// over that list to find a tile a worker can stand on (bridge bank end-pieces, fenced
// pasture masters).

// 0x140123480: Component() is the CALL 0x1401188b0; the 0x1401638b0 stamp is
// the compiler's; +0x28 = +0x30 = 0; flags |= 1 (0x81).
InteractComponent::InteractComponent() : field28(nullptr), field30(nullptr) {
    OrFlagsInCtor(0x1);
}

// [3] 0x140123340 — the use-location IBP array (+0x28/+0x30) round-trips here.
void InteractComponent::Persist(void* stream) {
    // Call base Component_Serialize
    Component::Persist(stream);   // retail's direct CALL 0x1401189b0

    // Create local IO::Serializer (24 bytes on stack)
    alignas(8) uint8_t serializer[24];
    ::new (static_cast<void*>(serializer)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge

    // Serialize count
    int32_t count = *reinterpret_cast<int32_t*>(
        reinterpret_cast<uint8_t*>(this) + 0x30);
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(count, nullptr);   // B.3.4 bridge

    // "Am I loading?" is `*(int32*)(*(void**)serializer + 8) == 0` -- deref the
    // Serializer to reach its STREAM, then read the stream's +8. This read was
    // missing that dereference and tested the Serializer's own +8, which is
    // m_encoding, not a loading flag (see
    // reference_serializer_plus8_is_encoding_not_loading_flag -- the same trap,
    // found here a second time). With the wrong field the resize below never ran
    // on a real load, so the array at +0x28 stayed NULL while `count` came back
    // non-zero from the stream, and the element loop dereferenced null.
    if (SerializerIsLoading(serializer)) {
        // B.3.2 bridge: exe 0x1400032e0 IS Collection::Array<void*>::SetSize(n).
        GameColl::AsArray<GameColl::Ptr8>(
            reinterpret_cast<uint8_t*>(this) + 0x28).SetSize(count);
    }

    // Serialize each InternalBasePtr element and validate
    uint8_t* arrayBase = *reinterpret_cast<uint8_t**>(
        reinterpret_cast<uint8_t*>(this) + 0x28);
    for (int i = 0; i < count; i++) {
        uint8_t* element = arrayBase + i * 8;

        // Serialize InternalBasePtr
        reinterpret_cast<Object::InternalBasePtr*>(element)->Serialize(*reinterpret_cast<IO::Serializer*>(serializer));   // B.3.4 bridge

        // Validate with IsDerivedFrom
        void* ptr = *reinterpret_cast<void**>(element);
        if (ptr != nullptr) {
            bool valid = reinterpret_cast<const Object::Base*>(ptr)->IsDerivedFrom(
                InteractComponent::TypeId());   // B.3.x bridge (slot 0x140152a98)
            if (!valid) {
                *reinterpret_cast<uint64_t*>(element) = 0;
            }
        }
    }

    // Destroy serializer
    reinterpret_cast<IO::Serializer*>(serializer)->~Serializer();   // B.3.4 bridge
}
// [6] 0x140123110
void InteractComponent::OnAdded() {
    uintptr_t selfA = (uintptr_t)this;

    int64_t multiBuf = 0;
    InteractComponent_FindWorkplaceComponent(*(uintptr_t*)(selfA + 0x10), &multiBuf);   // 0x140004740
    if (multiBuf == 0) return;

    // Clear the existing use-location array
    System::MemoryAllocator::MemFree(*(void**)(selfA + 0x28));
    *(int64_t*)(selfA + 0x28) = 0;
    *(int64_t*)(selfA + 0x30) = 0;

    int childCount = *(int*)(multiBuf + 0x50);
    for (int ci = 0; ci < childCount; ci++) {
        int64_t host = *(int64_t*)((int64_t)ci * 8 + *(int64_t*)(multiBuf + 0x48));

        // Find the InteractComponent (DAT_1401c7900) on this host
        void* interact = nullptr;
        int hc = *(int*)(host + 0x38);
        int64_t harr = *(int64_t*)(host + 0x30);
        for (int k = 0; k < hc; k++) {
            int64_t* slot = (int64_t*)(harr + (int64_t)k * 8);
            void* comp = (void*)*slot;
            uint32_t tid = reinterpret_cast<Object::Base*>(comp)->Type();   // B.3.8 bridge (slot 2)
            if (tid == InteractComponent::TypeId()) {
                void* c = (void*)*slot;
                if (reinterpret_cast<const Object::Base*>(c)->IsDerivedFrom(
                        InteractComponent::TypeId()))   // B.3.x bridge (slot 0x140152a98)
                    interact = c;
                break;
            }
        }

        int64_t interactIbp = 0;
        new (&interactIbp) ::Object::InternalBasePtr(
            reinterpret_cast<RtObject::Internal*>(interact));   // B.3.3 bridge
        if (interactIbp != 0) {
            // Append interactIbp to the use-location array at +0x28 (count +0x30, cap +0x34)
            uint32_t oldCount = *(uint32_t*)(selfA + 0x30);
            uint32_t newCount = oldCount + 1;
            if (newCount != oldCount) {
                void* oldBuf = *(void**)(selfA + 0x28);
                *(uint32_t*)(selfA + 0x30) = newCount;
                if (*(int*)(selfA + 0x34) < (int)newCount) {
                    *(uint32_t*)(selfA + 0x34) = newCount;
                    if (oldCount != 0) *(uint32_t*)(selfA + 0x34) = newCount * 3;
                    void* nb = System::MemoryAllocator::MemAlloc((uint64_t)((int64_t)*(int*)(selfA + 0x34) << 3), 8);
                    *(void**)(selfA + 0x28) = nb;
                }
                int start = (oldBuf == *(void**)(selfA + 0x28)) ? (int)oldCount : 0;
                int64_t* nbArr = *(int64_t**)(selfA + 0x28);
                int64_t* oldArr = (int64_t*)oldBuf;
                for (int i = start; i < (int)newCount; i++) {
                    int64_t* dst = nbArr + i;
                    if (dst) {
                        *dst = 0;
                        *dst = (i < (int)oldCount) ? oldArr[i] : interactIbp;
                    }
                }
                if (oldBuf != *(void**)(selfA + 0x28)) System::MemoryAllocator::MemFree(oldBuf);
            }
        }
    }
}

// create 0x1400351a0: MemAlloc(0x38, 8), then the ctor.
void* InteractComponent::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(InteractComponent), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) InteractComponent());
}
// Retail's createInPlace is `Internal::CreateInPlaceObject` (returns null); the
// tree's in-place entry built the object, and that is kept (B38 B8-13).
void* InteractComponent::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) InteractComponent());
}

DEFINE_EXE_TYPE(InteractComponent, "Component")
