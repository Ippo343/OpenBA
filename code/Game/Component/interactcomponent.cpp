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
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>

// ============================================================
// Resolved helpers
// ============================================================

// Component_Serialize (base class) -- at 0x1401189b0
typedef void (*fn_Component_Serialize_t)(void* self, void* stream);
static fn_Component_Serialize_t s_fn_Component_Serialize = nullptr;

// IO::Serializer::Serializer(stream, version)
typedef void (*fn_Serializer_Ctor_t)(void* self, void* stream, int version);
static fn_Serializer_Ctor_t s_fn_Serializer_Ctor = nullptr;

// IO::Serializer::~Serializer()
typedef void (*fn_Serializer_Dtor_t)(void* self);
static fn_Serializer_Dtor_t s_fn_Serializer_Dtor = nullptr;

// IO::Serializer::Serialize(int*)
typedef void (*fn_Serialize_Int_t)(void* serializer, int32_t* field, const char* name);
static fn_Serialize_Int_t s_fn_Serialize_Int = nullptr;

// Object::InternalBasePtr::Serialize(this, Serializer*)
typedef void (*fn_IBP_Serialize_t)(void* self, void* serializer);
static fn_IBP_Serialize_t s_fn_IBP_Serialize = nullptr;

// IsDerivedFrom -- dispatch slot
typedef bool (*fn_IsDerivedFrom_dispatch_t)(void* obj, uint32_t typeId);
static fn_IsDerivedFrom_dispatch_t s_fn_IsDerivedFrom_dispatch = nullptr;

// CheckPtr
typedef void (*fn_CheckPtr_t)(void* ptrField);
static fn_CheckPtr_t s_fn_CheckPtr = nullptr;

// InternalBasePtr::Ctor(retbuf, rawPtr)
typedef void (*fn_IBP_Ctor_t)(void* retbuf, void* rawPtr);
static fn_IBP_Ctor_t s_fn_IBP_Ctor = nullptr;

// FUN_1400032e0 -- dynamic array resize for IBP arrays
typedef void (*fn_IBPArray_Resize_t)(void* arrayPtr, int newCount);
static fn_IBPArray_Resize_t s_fn_IBPArray_Resize = nullptr;

// FUN_140004740 -- FindComponent (MultiObject)
typedef void (*fn_FindMultiObjComp_t)(uintptr_t entity, void* retBuf);
static fn_FindMultiObjComp_t s_fn_FindMultiObjComp = nullptr;

// Type ID: DAT_1401c7900 -- InteractComponent type for IsDerivedFrom
static uint32_t* s_TypeId_Interact = nullptr;

static bool s_helpersResolved = false;

static void EnsureHelpers() {
    if (s_helpersResolved) return;

    uintptr_t slot;

    s_fn_Component_Serialize = reinterpret_cast<fn_Component_Serialize_t>(
        ResolveAddr(Module::Exe, 0x1401189b0));

    slot = ResolveAddr(Module::Exe, 0x140152ad8);
    if (slot) s_fn_Serializer_Ctor = *reinterpret_cast<fn_Serializer_Ctor_t*>(slot);

    slot = ResolveAddr(Module::Exe, 0x140152ad0);
    if (slot) s_fn_Serializer_Dtor = *reinterpret_cast<fn_Serializer_Dtor_t*>(slot);

    slot = ResolveAddr(Module::Exe, 0x140152ac8);
    if (slot) s_fn_Serialize_Int = *reinterpret_cast<fn_Serialize_Int_t*>(slot);

    slot = ResolveAddr(Module::Exe, 0x140152a98);
    if (slot) s_fn_IsDerivedFrom_dispatch = *reinterpret_cast<fn_IsDerivedFrom_dispatch_t*>(slot);

    // Shared mangled-name resolve — 0x1800c0210 (previously used here) is
    // MID-FUNCTION (real InternalBasePtr::Serialize starts at 0x1800c0180);
    // calling it silently mis-read the stream and desynced everything after
    // this component on load (save/load wave 2026-08-28).
    s_fn_IBP_Serialize = reinterpret_cast<fn_IBP_Serialize_t>(
        reinterpret_cast<uintptr_t>(fn_InternalBasePtr_Serialize));

    s_fn_CheckPtr = reinterpret_cast<fn_CheckPtr_t>(
        ResolveAddr(Module::Runtime, 0x1800c0290));

    s_fn_IBP_Ctor = reinterpret_cast<fn_IBP_Ctor_t>(
        reinterpret_cast<uintptr_t>(fn_InternalBasePtr_Ctor));

    s_fn_IBPArray_Resize = reinterpret_cast<fn_IBPArray_Resize_t>(
        ResolveAddr(Module::Exe, 0x1400032e0));

    s_fn_FindMultiObjComp = reinterpret_cast<fn_FindMultiObjComp_t>(
        ResolveAddr(Module::Exe, 0x140004740));

    s_TypeId_Interact = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c7900));

    s_helpersResolved = true;
}

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
    return *reinterpret_cast<int32_t*>(*reinterpret_cast<uintptr_t*>(serializer) + 8) == 0;
}

void InteractComponent_Shutdown(InteractComponent* self, void* stream) {
    EnsureHelpers();

    // Call base Component_Serialize
    if (s_fn_Component_Serialize) {
        s_fn_Component_Serialize(self, stream);
    }

    // Create local IO::Serializer (24 bytes on stack)
    uint8_t serializer[24];
    if (s_fn_Serializer_Ctor) {
        s_fn_Serializer_Ctor(serializer, stream, 0);
    }

    // Serialize count
    int32_t count = *reinterpret_cast<int32_t*>(
        reinterpret_cast<uint8_t*>(self) + 0x30);
    if (s_fn_Serialize_Int) {
        s_fn_Serialize_Int(serializer, &count, nullptr);
    }

    // "Am I loading?" is `*(int32*)(*(void**)serializer + 8) == 0` -- deref the
    // Serializer to reach its STREAM, then read the stream's +8. This read was
    // missing that dereference and tested the Serializer's own +8, which is
    // m_encoding, not a loading flag (see
    // reference_serializer_plus8_is_encoding_not_loading_flag -- the same trap,
    // found here a second time). With the wrong field the resize below never ran
    // on a real load, so the array at +0x28 stayed NULL while `count` came back
    // non-zero from the stream, and the element loop dereferenced null.
    if (SerializerIsLoading(serializer)) {
        // Resize array for loading
        if (s_fn_IBPArray_Resize) {
            s_fn_IBPArray_Resize(
                reinterpret_cast<uint8_t*>(self) + 0x28, count);
        }
    }

    // Serialize each InternalBasePtr element and validate
    uint8_t* arrayBase = *reinterpret_cast<uint8_t**>(
        reinterpret_cast<uint8_t*>(self) + 0x28);
    for (int i = 0; i < count; i++) {
        uint8_t* element = arrayBase + i * 8;

        // Serialize InternalBasePtr
        if (s_fn_IBP_Serialize) {
            s_fn_IBP_Serialize(element, serializer);
        }

        // Validate with IsDerivedFrom
        void* ptr = *reinterpret_cast<void**>(element);
        if (ptr != nullptr) {
            if (s_fn_IsDerivedFrom_dispatch && s_TypeId_Interact) {
                bool valid = s_fn_IsDerivedFrom_dispatch(ptr, *s_TypeId_Interact);
                if (!valid) {
                    *reinterpret_cast<uint64_t*>(element) = 0;
                }
            }
        }
    }

    // Destroy serializer
    if (s_fn_Serializer_Dtor) {
        s_fn_Serializer_Dtor(serializer);
    }
}

// ============================================================
// Reimpl of FUN_140004740 (find the WorkplaceComponent (type DWORD_1401c7ac0)
//   on an entity and return it wrapped in an InternalBasePtr via retBuf)
// ============================================================
// Shared callee: also used from buildcomponent.cpp (BuildComponent::Draw).
void InteractComponent_FindWorkplaceComponent(uintptr_t entity, void* retBuf) {
    EnsureHelpers();
    static uint32_t* s_pTId_Workplace = nullptr;
    if (!s_pTId_Workplace)
        s_pTId_Workplace = reinterpret_cast<uint32_t*>(ResolveAddr(Module::Exe, 0x1401c7ac0));

    void* found = nullptr;
    if (s_pTId_Workplace) {
        int count = *(int*)(entity + 0x38);
        int64_t arr = *(int64_t*)(entity + 0x30);
        for (int i = 0; i < count; i++) {
            int64_t* slot = (int64_t*)(arr + (int64_t)i * 8);
            if (s_fn_CheckPtr) s_fn_CheckPtr(slot);
            void* comp = (void*)*slot;
            typedef uint32_t (*GetTypeId_t)(void*);
            uint32_t tid = ((GetTypeId_t)(*(uintptr_t*)(*(uintptr_t*)comp + 0x10)))(comp);
            if (tid == *s_pTId_Workplace) {
                if (s_fn_CheckPtr) s_fn_CheckPtr(slot);
                void* c = (void*)*slot;
                if (s_fn_IsDerivedFrom_dispatch && s_fn_IsDerivedFrom_dispatch(c, *s_pTId_Workplace))
                    found = c;
                break;
            }
        }
    }
    if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor(retBuf, found);
}

// ============================================================
// InteractComponent::Draw (vtable slot [6], 0x140123110)
// ============================================================
// Reimpl of FUN_140123110: rebuild the interact-use-location list from the
// owner's MultiObject children. For each child's MapComponent (DAT_1401c7900),
// scan its job hosts for an InteractComponent and append it to the +0x28 list.
// Faithful port of the Ghidra decompilation.
void InteractComponent_Draw(InteractComponent* self) {
    EnsureHelpers();
    uintptr_t selfA = (uintptr_t)self;

    if (s_fn_CheckPtr) s_fn_CheckPtr((void*)(selfA + 0x10));
    int64_t multiBuf = 0;
    if (s_fn_FindMultiObjComp) s_fn_FindMultiObjComp(*(uintptr_t*)(selfA + 0x10), &multiBuf);
    if (multiBuf == 0) return;

    // Clear the existing use-location array
    fn_MemFreeStatic(*(void**)(selfA + 0x28));
    *(int64_t*)(selfA + 0x28) = 0;
    *(int64_t*)(selfA + 0x30) = 0;

    if (s_fn_CheckPtr) s_fn_CheckPtr(&multiBuf);
    int childCount = *(int*)(multiBuf + 0x50);
    for (int ci = 0; ci < childCount; ci++) {
        if (s_fn_CheckPtr) s_fn_CheckPtr(&multiBuf);
        int64_t host = *(int64_t*)((int64_t)ci * 8 + *(int64_t*)(multiBuf + 0x48));
        if (s_fn_CheckPtr) s_fn_CheckPtr(&host);

        // Find the InteractComponent (DAT_1401c7900) on this host
        void* interact = nullptr;
        int hc = *(int*)(host + 0x38);
        int64_t harr = *(int64_t*)(host + 0x30);
        for (int k = 0; k < hc; k++) {
            int64_t* slot = (int64_t*)(harr + (int64_t)k * 8);
            if (s_fn_CheckPtr) s_fn_CheckPtr(slot);
            void* comp = (void*)*slot;
            typedef uint32_t (*GetTypeId_t)(void*);
            uint32_t tid = ((GetTypeId_t)(*(uintptr_t*)(*(uintptr_t*)comp + 0x10)))(comp);
            if (tid == (s_TypeId_Interact ? *s_TypeId_Interact : 0xffffffff)) {
                if (s_fn_CheckPtr) s_fn_CheckPtr(slot);
                void* c = (void*)*slot;
                if (s_fn_IsDerivedFrom_dispatch && s_TypeId_Interact &&
                    s_fn_IsDerivedFrom_dispatch(c, *s_TypeId_Interact))
                    interact = c;
                break;
            }
        }

        int64_t interactIbp = 0;
        if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor(&interactIbp, interact);
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
                    void* nb = fn_MemAlloc(g_pMemAllocSingleton,
                                           (uint64_t)((int64_t)*(int*)(selfA + 0x34) << 3), 8);
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
                if (oldBuf != *(void**)(selfA + 0x28)) fn_MemFreeStatic(oldBuf);
            }
        }
        if (s_fn_CheckPtr) s_fn_CheckPtr(&multiBuf);
    }
}

// ============================================================
// Init: resolve function pointers
// ============================================================
void InteractComponent_InitFuncs() {
    EnsureHelpers();

#ifdef _DEBUG
    printf("[InteractComponent] Funcs resolved: Serialize=%p IBP_Serialize=%p "
           "FindWorkplace=%p TypeId=%p\n",
           (void*)s_fn_Component_Serialize, (void*)s_fn_IBP_Serialize,
           (void*)s_fn_FindMultiObjComp,
           (void*)s_TypeId_Interact);
#endif
}

// ===== Standalone factory (Foundation D — component instantiation) =====
// In standalone the original exe is not mapped, so Object::Factory::CreateObject
// must resolve to a C++ factory here. Produces an inert-but-valid InteractComponent
// (size 0x38, +0x20 capability flags 0x81, Object::Internal base + shared stub
// vtable, zeroed body). Size from InteractComponent_Factory MemAlloc; flags from the
// Component_Ctor (+0x20=0x80) chain OR'd with the leaf ctor bits.
#include "Object/factory_registration.h"
#include "Component/component_factory_base.h"

// Hand-expanded from COMPONENT_INERT_FACTORY(InteractComponent, 0x38, 0x81) so vtable
// slot 6 (+0x30) can carry the real InteractComponent_Draw.
//
// Slot 6 is the ON-ADDED broadcast: ComponentEntity_OnAddedPass (FUN_1400c2f60, the tail
// of EntityList::RegisterEntity 0x1400c48a0) calls it on every component whose capability
// flags (+0x20) have bit 0 — InteractComponent's 0x81 qualifies. 0x140123110 has always
// been ported AND registered by address, and was reached by nothing, because the inert
// vtable's slot 6 was a stub. Exactly the CitizenComponent::OnAdd gap again.
//
// What it costs to leave it unwired: the function rebuilds the owner's interact
// use-location list (+0x28 array / +0x30 count) from the owner's MultiObjectComponent
// children. FUN_140122b00 (CalcChildTargetPos) recurses over precisely that list to find
// a tile a worker can stand on. A bridge master's own footprint is all water, so its only
// reachable access points are the two bank end-pieces — which live in that child list.
// With the list empty, GatherRawMaterials bailed "no target pos (found==0)" on every tick
// and no material was ever hauled to a bridge.
//
// Graduated default-on 2026-07-30 (kDefaultOnFlags; BANISHED_LIVE_INTERACT_ONADD=0 is the
// kill-switch). The same gap blocks a fenced PASTURE master, whose description carries zero
// access points of its own (measured accessPts=0), so every haul bailed "no target pos" and
// the site never took a single log. With the slot live the master rebuilds children=1 from
// its 29 MultiObject entries, delivers 56/56, and builds to completion.
static uint32_t InteractComponent_InertGetTypeId(void*) {
    static uint32_t cached = 0;
    if (cached == 0) cached = ::Object::GetTypeIdByName("InteractComponent");
    return cached;
}
static void* InteractComponent_InertVtable[::ComponentFactory::kInertVtableSlots];
static void InteractComponent_OnAddedVSlot(void* self) {
    InteractComponent_Draw(reinterpret_cast<InteractComponent*>(self));
}
// Slot 3 = Shutdown/Serialize (0x140123340). The body has been ported in this file
// since the original wave (InteractComponent_Shutdown, above) but nothing ever wrote
// it into the live vtable, so a loaded save's InteractComponent kept the
// FillTypedVtable base-Component serialize only — the use-location IBP array (+0x28/
// +0x30) never round-tripped. Wired UNCONDITIONALLY (save/load wave 2026-08-27),
// unlike the OnAdded slot below which stays on its own kill-switch.
static void InteractComponent_ShutdownVSlot(void* self, void* stream) {
    InteractComponent_Shutdown(reinterpret_cast<InteractComponent*>(self), stream);
}
static void InteractComponent_FinishVtable() {
    InteractComponent_InertVtable[3] =
        reinterpret_cast<void*>(&InteractComponent_ShutdownVSlot);
    InteractComponent_InertVtable[6] =
        reinterpret_cast<void*>(&InteractComponent_OnAddedVSlot);
}
extern "C" void* InteractComponent_Factory(void) {
    void* p = ::ComponentFactory::MakeInertTyped(0x38, 0x81,
        InteractComponent_InertVtable,
        reinterpret_cast<void*>(&InteractComponent_InertGetTypeId));
    InteractComponent_FinishVtable();
    return p;
}
static void* InteractComponent_CreateInPlace(void* mem) {
    if (mem) ::ComponentFactory::InitInertTyped(mem, 0x38, 0x81,
        InteractComponent_InertVtable,
        reinterpret_cast<void*>(&InteractComponent_InertGetTypeId));
    InteractComponent_FinishVtable();
    return mem;
}
REGISTER_TYPE(InteractComponent, "Component",
              &InteractComponent_Factory, &InteractComponent_CreateInPlace);
