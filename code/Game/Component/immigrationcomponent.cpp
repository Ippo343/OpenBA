// code/Game/Component/immigrationcomponent.cpp
// Clean reimplementation of ImmigrationComponent::Shutdown from Application-x64.exe.
//
// ImmigrationComponent (0x68 bytes) -- ControllerComponent subclass.
// Manages immigration/nomad arrival events.
//
// Vtable at 0x140159b30:
//   [3] Shutdown = 0x14004e4b0 -- complex serialization with entity array loop
//
// Ghidra decompilation:
//   Component_Serialize(param_1, param_2)
//   Serializer(local_20, param_2, 0); ~Serializer(local_20)  // empty controller block
//   Serializer(local_30, param_2, 0)
//   Serialize(local_30, param_1+0x50, nullptr)  // field50 (int)
//   local_res8 = *(param_1+0x60)                // array count
//   Serialize(local_30, local_res8, nullptr)
//   if loading: DynArray_Resize(param_1+0x58, count)
//   for each element in array:
//     InternalBasePtr::Serialize(element, serializer)
//     if ptr != null && !IsDerivedFrom(ptr, ComponentEntity): clear ptr
//   Time::Serialize(param_1+0x48, serializer)
//   ~Serializer(local_30)
//
// Decompiled from Application-x64.exe using Ghidra.

#include "Component/immigrationcomponent_funcs.h"
#include "Component/coll_bridge.h"
#include "engine_funcs.h"
#include "resolve.h"
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch A
#include <cstdint>
#include <cstdio>
#include <new>
#include "object/Internal.h"   // ENDGAME B.3.4 caller batch G
#include "system/Value.h"   // ENDGAME B.3.4b System caller batch

// ============================================================
// Resolved helpers
// ============================================================

// Component_Serialize (base class) -- at 0x1401189b0
typedef void (*fn_Component_Serialize_t)(void* self, void* stream);
static fn_Component_Serialize_t s_fn_Component_Serialize = nullptr;

// Object::InternalBasePtr::Serialize is a direct member call since ENDGAME B.3.4 (batch G);
// the engine_funcs global and its typedef were deleted in closing 2d.

// IsDerivedFrom -- dispatch slot at 0x140152a98
typedef bool (*fn_IsDerivedFrom_dispatch_t)(void* self, uint32_t typeId);
static fn_IsDerivedFrom_dispatch_t s_fn_IsDerivedFrom_d = nullptr;

// FUN_1400032e0 -- dynamic array resize; this file DEFINES its body as
// ImmigrationComp_DynArrayResize below (kept, still registered for other
// callers) — this file's own call site goes straight through the bridge.

// Type ID for ComponentEntity (DAT_1401c5d70)
static uint32_t* s_pTypeId_ComponentEntity = nullptr;

static bool s_initialized = false;

static void EnsureHelpers() {
    if (s_initialized) return;

    uintptr_t slot;

    s_fn_Component_Serialize = reinterpret_cast<fn_Component_Serialize_t>(
        ResolveAddr(Module::Exe, 0x1401189b0));
    s_pTypeId_ComponentEntity = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c5d70));

    slot = ResolveAddr(Module::Exe, 0x140152a98);
    if (slot) s_fn_IsDerivedFrom_d = reinterpret_cast<fn_IsDerivedFrom_dispatch_t>(
        *reinterpret_cast<uintptr_t*>(slot));

    s_initialized = true;
}

// ============================================================
// ImmigrationComponent::Shutdown (vtable slot [3], 0x14004e4b0)
// ============================================================
// Complex serialization: base + controller block + field50 + entity array + Time.

void ImmigrationComponent_Shutdown(ImmigrationComponent* self, void* stream) {
    EnsureHelpers();

    // Call base Component_Serialize
    if (s_fn_Component_Serialize) {
        s_fn_Component_Serialize(self, stream);
    }

    // Empty serializer block (controller compat)
    alignas(8) uint8_t serializer1[24];
    ::new (static_cast<void*>(serializer1)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(serializer1)->~Serializer();                                       // B.3.4 bridge

    // Main serializer block
    alignas(8) uint8_t serializer2[24];
    ::new (static_cast<void*>(serializer2)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge

    // Serialize field50 (int)
    reinterpret_cast<IO::Serializer*>(serializer2)->Serialize(*reinterpret_cast<int*>(&self->field50), nullptr);   // B.3.4 bridge

    // Serialize array count from field60
    int arrayCount = *(int*)&self->field60;
    reinterpret_cast<IO::Serializer*>(serializer2)->Serialize(arrayCount, nullptr);   // B.3.4 bridge

    // "Am I loading?" is `*(int32*)(*(void**)serializer + 8) == 0` -- deref the
    // Serializer to reach its STREAM first, then read the stream's +8. This
    // tested the Serializer's OWN +8 (m_encoding), so on a real load the resize
    // below never ran and the element loop walked a NULL array with a non-zero
    // count straight off the stream. Same trap as interactcomponent.cpp and
    // reference_serializer_plus8_is_encoding_not_loading_flag.
    if (static_cast<int>(reinterpret_cast<IO::Serializer*>(serializer2)->GetDirection()) == 0) {   // B.3.4 bridge  (IO::Serializer::GetDirection, retail 0x180006210)
        // B.3.2 bridge: exe 0x1400032e0 IS Collection::Array<void*>::SetSize(n).
        GameColl::AsArray<GameColl::Ptr8>((void*)&self->field58).SetSize(arrayCount);
    }

    // Serialize each InternalBasePtr element in the array
    // Array base at +0x58, count = arrayCount
    void* arrayBase = self->field58;
    int64_t remaining = (int64_t)arrayCount;
    if (remaining > 0) {
        uint8_t* elementPtr = (uint8_t*)arrayBase;
        do {
            // InternalBasePtr::Serialize(element, serializer)
            reinterpret_cast<Object::InternalBasePtr*>(elementPtr)->Serialize(*reinterpret_cast<IO::Serializer*>(serializer2));   // B.3.4 bridge

            // Validate: if ptr is non-null but not a ComponentEntity, clear it
            void* objPtr = *(void**)elementPtr;
            if (objPtr != nullptr) {
                if (s_fn_IsDerivedFrom_d && s_pTypeId_ComponentEntity) {
                    bool isEntity = s_fn_IsDerivedFrom_d(objPtr, *s_pTypeId_ComponentEntity);
                    if (!isEntity) {
                        *(uint64_t*)elementPtr = 0;
                    }
                }
            }

            elementPtr += 8;  // InternalBasePtr is 8 bytes
            remaining--;
        } while (remaining != 0);
    }

    // Serialize Time at +0x48
    reinterpret_cast<System::Time*>((void*)&self->field48)->Serialize(*reinterpret_cast<IO::Serializer*>(serializer2));   // B.3.4b bridge

    // Destroy serializer
    reinterpret_cast<IO::Serializer*>(serializer2)->~Serializer();   // B.3.4 bridge
}

// ============================================================
// Reimpl of FUN_1400032e0 (Component reg wave "people") — shared DynArray<ptr> resize.
// ENDGAME B.3.2 step 2: this IS Collection::Array<void*>::SetSize(n) — the old
// hand-written body's copy path did `*slot = 0;` immediately before assigning
// the copied 8-byte value into the same slot; that default-construct-then-
// assign is exactly what SetSizeCore's byte-copy `assign` does for a
// zero-default element and is unobservable (the write is overwritten before
// anything reads it), so the bridge call below is byte-for-byte identical.
// Non-static: still registered by reg_component_people.cpp for other callers,
// so this function is KEPT (not deleted/unregistered) — only its body changed.
// ============================================================
void ImmigrationComp_DynArrayResize(int64_t* param_1, int newCount) {
    GameColl::AsArray<GameColl::Ptr8>(param_1).SetSize(newCount);
}

// ============================================================
// Init: resolve function pointers
// ============================================================
void ImmigrationComponent_InitFuncs() {
    EnsureHelpers();
#ifdef _DEBUG
    printf("[ImmigrationComponent] Funcs resolved: Serialize=%p\n",
           (void*)s_fn_Component_Serialize);
#endif
}

// ============================================================================
// ENDGAME B.3.8 MI-A — ImmigrationComponent as a REAL class (ImmigrationComponent.h).
//
// Before: CONTROLLER_LIVE_FACTORY_STATES + CONTROLLER_INSTALL_SERIALIZE (a 48-slot
// table re-filled at every create), `Inert_ScalarDtor` in slot 0 (only `~Internal`), the
// ControllerLive::Dispatcher<ImmigrationCtrlTag> table stamped into raw bytes at +0x28.
// After: the compiler's 18-slot vtable, a real constructor and destructor
// chain, the controller a member object (B38_RULES D1, D2, D6).
// ============================================================================
#include "Object/factory_registration.h"
#include "Component/ImmigrationComponent.h"

// The nomad-arrival state machine (states 2/0xf/0x10/0x11,
// immigrationcomponent_states.cpp). Node at +0x28 (slot 17 0x140004080), default
// state 2 (slot 16 0x1400026c0). The state table is built ONCE at static-init time,
// as retail's static initializer does (B38 D6/D7).
extern "C" void ImmigrationCtrl_PopulateTable();
static const bool s_immigrationStateTableBuilt = (ImmigrationCtrl_PopulateTable(), true);

// 0x14004e640. Component() is the CALL 0x1401188b0; the stamps are the compiler's;
// `_controller(this)` is the member's back-pointer; +0x48..+0x60 = 0. No flag OR.
ImmigrationComponent::ImmigrationComponent()
    : _controller(this), field48(nullptr), field50(0), _pad54{}, field58(nullptr), field60(nullptr) {}

// [0] 0x140007b70: ~ImmigrationComponent (0x14004e5d0), MemFree on &1.
Object::Base* ImmigrationComponent::DeletingDtor(unsigned flags) {
    this->~ImmigrationComponent();
    if (flags & 1) Object::detail::InternalMemFree(this);
    return this;
}
// 0x14004e5d0: the group array (+0x58 data, +0x60 count/capacity) is freed and
// zeroed; the member's and the bases' dtors (restamps only) and ~Internal are the
// compiler's chain.
ImmigrationComponent::~ImmigrationComponent() {
    Object::detail::InternalMemFree(field58);
    field58 = nullptr;
    field60 = nullptr;
}

// create 0x140007a20: MemAlloc(0x68, 8), then the ctor.
void* ImmigrationComponent::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(ImmigrationComponent), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) ImmigrationComponent());
}
// Retail's createInPlace is `Internal::CreateInPlaceObject` (returns null); the
// tree's in-place entry built the object, and that is kept (B38 B8-13).
void* ImmigrationComponent::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) ImmigrationComponent());
}

// [3] 0x14004e4b0.
void ImmigrationComponent::Persist(void* stream) { ImmigrationComponent_Shutdown(this, stream); }

DEFINE_EXE_VTABLE(ImmigrationComponent, "_ZTV20ImmigrationComponent")
DEFINE_EXE_TYPE(ImmigrationComponent, "ControllerComponent")
