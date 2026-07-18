// code/Game/Component/orchardcomponent.cpp
// Clean reimplementation of OrchardComponent lifecycle methods from Application-x64.exe.
//
// OrchardComponent (0x68 bytes) extends ControllerComponent with:
//   +0x48: uint8  orchardFlag, +0x4C: int32 orchardId, +0x50-0x60: arrays
//
// Vtable at 0x14015a180. One hooked override:
//   Shutdown (0x140051de0) — Complex entity ref serialization
//

#include "Component/orchardcomponent_funcs.h"
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cstdio>

// ============================================================
// Resolved helpers
// ============================================================

typedef void (*fn_Component_Serialize_t)(void* self, void* stream);
static fn_Component_Serialize_t s_fn_Component_Serialize = nullptr;

typedef void* (*fn_Serializer_Ctor_t)(void* self, void* stream, int version);
static fn_Serializer_Ctor_t s_fn_Serializer_Ctor = nullptr;

typedef void (*fn_Serializer_Dtor_t)(void* self);
static fn_Serializer_Dtor_t s_fn_Serializer_Dtor = nullptr;

typedef void (*fn_SerializeBool_t)(void* self, bool* value, const char* name);
static fn_SerializeBool_t s_fn_SerializeBool = nullptr;

typedef void (*fn_SerializeInt_t)(void* self, int* value, const char* name);
static fn_SerializeInt_t s_fn_SerializeInt = nullptr;

// FUN_140051830 — validate entity references in orchard
typedef void (*fn_ValidateOrchardRefs_t)(void* self);
static fn_ValidateOrchardRefs_t s_fn_ValidateOrchardRefs = nullptr;

// FUN_140009f80 — resize entity ref array at +0x50
typedef void (*fn_ResizeRefArray_t)(void* arrayPtr, int newCount);
static fn_ResizeRefArray_t s_fn_ResizeRefArray = nullptr;

// FUN_14000a230 — serialize entity ref array entries
typedef void (*fn_SerializeRefEntries_t)(void* serializer, int count, void* arrayBase);
static fn_SerializeRefEntries_t s_fn_SerializeRefEntries = nullptr;

// FUN_140002340 — serialize IBP array (for +0x60)
typedef void (*fn_SerializeIBPArray_t)(void* arrayPtr, void* serializer);
static fn_SerializeIBPArray_t s_fn_SerializeIBPArray = nullptr;

static bool s_initialized = false;

// ============================================================
// OrchardComponent::Shutdown (0x140051de0)
// ============================================================
// Component_Serialize + empty controller serializer + second serializer:
//   validate refs, serialize orchardFlag, orchardId, entity ref array, resource array

void OrchardComponent_Shutdown(OrchardComponent* self, void* stream) {
    uintptr_t selfAddr = (uintptr_t)self;

    if (s_fn_Component_Serialize)
        s_fn_Component_Serialize(self, stream);

    // Empty controller serializer block
    uint8_t serializer1[24];
    if (s_fn_Serializer_Ctor) s_fn_Serializer_Ctor(serializer1, stream, 0);
    if (s_fn_Serializer_Dtor) s_fn_Serializer_Dtor(serializer1);

    // Main data serializer
    uint8_t serializer2[24];
    if (s_fn_Serializer_Ctor) s_fn_Serializer_Ctor(serializer2, stream, 0);

    // Validate entity references
    if (s_fn_ValidateOrchardRefs)
        s_fn_ValidateOrchardRefs((void*)selfAddr);

    // Serialize orchardFlag (bool at +0x48)
    if (s_fn_SerializeBool)
        s_fn_SerializeBool(serializer2, (bool*)(selfAddr + 0x48), nullptr);

    // Serialize orchardId (int at +0x4C)
    if (s_fn_SerializeInt)
        s_fn_SerializeInt(serializer2, (int*)(selfAddr + 0x4C), nullptr);

    // Serialize entity ref count from +0x58, resize +0x50 if loading
    int localCount = *(int*)(selfAddr + 0x58);
    if (s_fn_SerializeInt)
        s_fn_SerializeInt(serializer2, &localCount, nullptr);

    if (*(int*)((uintptr_t)stream + 8) == 0 && s_fn_ResizeRefArray) {
        s_fn_ResizeRefArray((void*)(selfAddr + 0x50), localCount);
    }

    // Serialize entity ref array entries
    if (s_fn_SerializeRefEntries)
        s_fn_SerializeRefEntries(serializer2, localCount, *(void**)(selfAddr + 0x50));

    // Serialize resource IBP array at +0x60
    if (s_fn_SerializeIBPArray)
        s_fn_SerializeIBPArray((void*)(selfAddr + 0x60), serializer2);

    if (s_fn_Serializer_Dtor) s_fn_Serializer_Dtor(serializer2);
}

// ============================================================
// Reimplemented exe helpers (callee bodies for the guarded call sites above)
// ============================================================

// IO::Serializer::Serialize(int*) dispatch (reuse s_fn_SerializeInt) and InternalBasePtr::Serialize.
// Object::Base::IsA(self, typeId) -> bool.
typedef bool (*fn_BaseIsA_t)(void* self, uint32_t typeId);
static fn_BaseIsA_t s_fn_BaseIsA = nullptr;
// FUN_1400052c0 -- IBP component accessor used by ref validation.
typedef void* (*fn_IBPAccessor_t)(int64_t entity, void* outIBP);
static fn_IBPAccessor_t s_fn_IBPAccessor52c0 = nullptr;
// Type-id globals referenced by the serializer (entity-ref kind filters).
static uint32_t* s_pTypeComponentEntity = nullptr;  // TypeInfo_ComponentEntity
static uint32_t* s_pType7b30 = nullptr;             // DAT_1401c7b30
static uint32_t* s_pType4250 = nullptr;             // DAT_1401c4250
static void**    s_pWorldSingleton2 = nullptr;      // g_pGameWorldSingleton (0x1401c3dd8)

static bool s_subResolved = false;
static void EnsureSubHelpers() {
    if (s_subResolved) return;
    s_fn_BaseIsA          = reinterpret_cast<fn_BaseIsA_t>(ResolveAddr(Module::Exe, 0x140002d40));
    s_fn_IBPAccessor52c0  = reinterpret_cast<fn_IBPAccessor_t>(ResolveAddr(Module::Exe, 0x1400052c0));
    s_pTypeComponentEntity= reinterpret_cast<uint32_t*>(ResolveAddr(Module::Exe, 0x1401c79e0));
    s_pType7b30           = reinterpret_cast<uint32_t*>(ResolveAddr(Module::Exe, 0x1401c7b30));
    s_pType4250           = reinterpret_cast<uint32_t*>(ResolveAddr(Module::Exe, 0x1401c4250));
    s_pWorldSingleton2    = reinterpret_cast<void**>(ResolveAddr(Module::Exe, 0x1401c3dd8));
    s_subResolved = true;
}

// Reimpl of FUN_140009f80 (grow stride-0x28 entity-ref array at +0x50; copy old, zero new, free old)
void OrchardComponent_ResizeRefArray(void* arrayPtr, int newCount) {
    int64_t* a = reinterpret_cast<int64_t*>(arrayPtr);
    int oldCount = (int)a[1];
    if (newCount == oldCount) return;
    void* oldBuf = reinterpret_cast<void*>(a[0]);
    *reinterpret_cast<int*>(&a[1]) = newCount;
    int* capPtr = reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(arrayPtr) + 0xc);
    if (*capPtr < newCount) {
        *capPtr = newCount;
        if (newCount == oldCount + 1 && oldCount != 0) *capPtr = newCount * 3;
        void* newBuf = (fn_MemAlloc && g_pMemAllocSingleton)
                           ? fn_MemAlloc(g_pMemAllocSingleton, (uint64_t)(*capPtr) * 0x28, 8) : nullptr;
        a[0] = reinterpret_cast<int64_t>(newBuf);
    }
    int keep = (oldBuf == reinterpret_cast<void*>(a[0])) ? oldCount : 0;
    uintptr_t newBase = (uintptr_t)a[0];
    uintptr_t oldBase = (uintptr_t)oldBuf;
    for (int idx = keep; idx < newCount; ++idx) {
        uint8_t* dst = reinterpret_cast<uint8_t*>(newBase + (uintptr_t)idx * 0x28);
        if (idx < oldCount) {
            uint8_t* src = reinterpret_cast<uint8_t*>(oldBase + (uintptr_t)idx * 0x28);
            *reinterpret_cast<uint32_t*>(dst + 0)  = *reinterpret_cast<uint32_t*>(src + 0);
            *reinterpret_cast<uint32_t*>(dst + 4)  = *reinterpret_cast<uint32_t*>(src + 4);
            *reinterpret_cast<uint64_t*>(dst + 8)  = *reinterpret_cast<uint64_t*>(src + 8);
            *reinterpret_cast<uint64_t*>(dst + 16) = *reinterpret_cast<uint64_t*>(src + 16);
            *reinterpret_cast<uint64_t*>(dst + 24) = *reinterpret_cast<uint64_t*>(src + 24);
            *reinterpret_cast<uint8_t*>(dst + 32)  = *reinterpret_cast<uint8_t*>(src + 32);
        } else {
            *reinterpret_cast<uint64_t*>(dst + 8)  = 0;
            *reinterpret_cast<uint64_t*>(dst + 16) = 0;
            *reinterpret_cast<uint64_t*>(dst + 24) = 0;
        }
    }
    if (oldBuf != reinterpret_cast<void*>(a[0]) && fn_MemFreeStatic) fn_MemFreeStatic(oldBuf);
}

// Reimpl of FUN_14000a230 (serialize stride-0x28 entity-ref array entries: 2 ints, 3 IBPs, 1 bool)
void OrchardComponent_SerializeRefEntries(void* serializer, int count, void* arrayBase) {
    EnsureSubHelpers();
    if (count <= 0) return;
    uint8_t* e = reinterpret_cast<uint8_t*>(arrayBase);
    for (int i = 0; i < count; ++i) {
        if (s_fn_SerializeInt) s_fn_SerializeInt(serializer, reinterpret_cast<int*>(e + 0), nullptr);
        if (s_fn_SerializeInt) s_fn_SerializeInt(serializer, reinterpret_cast<int*>(e + 4), nullptr);

        if (fn_InternalBasePtr_Serialize) fn_InternalBasePtr_Serialize(e + 8, serializer);
        if (*reinterpret_cast<void**>(e + 8) != nullptr && s_pTypeComponentEntity &&
            !(fn_IsDerivedFrom && fn_IsDerivedFrom(*reinterpret_cast<void**>(e + 8), *s_pTypeComponentEntity)))
            *reinterpret_cast<void**>(e + 8) = nullptr;

        if (fn_InternalBasePtr_Serialize) fn_InternalBasePtr_Serialize(e + 0x10, serializer);
        if (*reinterpret_cast<void**>(e + 0x10) != nullptr && s_pType7b30 &&
            !(fn_IsDerivedFrom && fn_IsDerivedFrom(*reinterpret_cast<void**>(e + 0x10), *s_pType7b30)))
            *reinterpret_cast<void**>(e + 0x10) = nullptr;

        if (fn_InternalBasePtr_Serialize) fn_InternalBasePtr_Serialize(e + 0x18, serializer);
        if (*reinterpret_cast<void**>(e + 0x18) != nullptr && s_pType4250 &&
            !(fn_IsDerivedFrom && fn_IsDerivedFrom(*reinterpret_cast<void**>(e + 0x18), *s_pType4250)))
            *reinterpret_cast<void**>(e + 0x18) = nullptr;

        if (s_fn_SerializeBool) s_fn_SerializeBool(serializer, reinterpret_cast<bool*>(e + 0x20), nullptr);
        e += 0x28;
    }
}

// Reimpl of FUN_140051830 (validate orchard entity refs against live world entities; clear stale)
void OrchardComponent_ValidateRefs(void* selfV) {
    EnsureSubHelpers();
    uintptr_t self = reinterpret_cast<uintptr_t>(selfV);
    uintptr_t world = s_pWorldSingleton2 ? reinterpret_cast<uintptr_t>(*s_pWorldSingleton2) : 0;
    if (!world) return;
    uintptr_t resMgr = *reinterpret_cast<uintptr_t*>(world + 0xF8);
    int count = *reinterpret_cast<int*>(self + 0x58);
    for (int i = 0; i < count; ++i) {
        uintptr_t arr = *reinterpret_cast<uintptr_t*>(self + 0x50);
        uintptr_t entry = arr + (uintptr_t)i * 0x28;
        if (*reinterpret_cast<int*>(entry + 4) != 1) continue;

        int32_t tile = *reinterpret_cast<int32_t*>(entry + 0);
        void* live = *reinterpret_cast<void**>(
            *reinterpret_cast<uintptr_t*>(resMgr + 0x80) + 8 + (uintptr_t)tile * 0x30);
        bool ok = false;
        if (live) {
            if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&live);
            if (s_fn_BaseIsA && s_pTypeComponentEntity &&
                s_fn_BaseIsA(live, *s_pTypeComponentEntity)) {
                if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&live);
                int64_t accBuf = 0;
                void* acc = s_fn_IBPAccessor52c0 ? s_fn_IBPAccessor52c0((int64_t)live, &accBuf) : nullptr;
                if (acc && *reinterpret_cast<int64_t*>(acc) != 0) {
                    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(reinterpret_cast<void*>(entry + 8));
                    if (live == *reinterpret_cast<void**>(entry + 8)) ok = true;
                }
            }
        }
        if (ok) continue;
        // Stale -> mark removed (kind=2) and null the 3 IBP slots.
        *reinterpret_cast<int32_t*>(entry + 4) = 2;
        int64_t z = 0;
        if (fn_InternalBasePtr_Ctor) { fn_InternalBasePtr_Ctor(&z, nullptr);
            *reinterpret_cast<int64_t*>(entry + 8) = z; }
        z = 0;
        if (fn_InternalBasePtr_Ctor) { fn_InternalBasePtr_Ctor(&z, nullptr);
            *reinterpret_cast<int64_t*>(entry + 0x10) = z; }
        z = 0;
        if (fn_InternalBasePtr_Ctor) { fn_InternalBasePtr_Ctor(&z, nullptr);
            *reinterpret_cast<int64_t*>(entry + 0x18) = z; }
    }
}

// ============================================================
// Init: resolve function pointers
// ============================================================
void OrchardComponent_InitFuncs() {
    if (s_initialized) return;

    s_fn_Component_Serialize = reinterpret_cast<fn_Component_Serialize_t>(
        ResolveAddr(Module::Exe, 0x1401189b0));
    s_fn_ValidateOrchardRefs = reinterpret_cast<fn_ValidateOrchardRefs_t>(
        ResolveAddr(Module::Exe, 0x140051830));
    s_fn_ResizeRefArray = reinterpret_cast<fn_ResizeRefArray_t>(
        ResolveAddr(Module::Exe, 0x140009f80));
    s_fn_SerializeRefEntries = reinterpret_cast<fn_SerializeRefEntries_t>(
        ResolveAddr(Module::Exe, 0x14000a230));
    s_fn_SerializeIBPArray = reinterpret_cast<fn_SerializeIBPArray_t>(
        ResolveAddr(Module::Exe, 0x140002340));

    uintptr_t slot;
    slot = ResolveAddr(Module::Exe, 0x140152ad8);
    if (slot) s_fn_Serializer_Ctor = *reinterpret_cast<fn_Serializer_Ctor_t*>(slot);
    slot = ResolveAddr(Module::Exe, 0x140152ad0);
    if (slot) s_fn_Serializer_Dtor = *reinterpret_cast<fn_Serializer_Dtor_t*>(slot);
    slot = ResolveAddr(Module::Exe, 0x140152a58);
    if (slot) s_fn_SerializeBool = *reinterpret_cast<fn_SerializeBool_t*>(slot);
    slot = ResolveAddr(Module::Exe, 0x140152ac8);
    if (slot) s_fn_SerializeInt = *reinterpret_cast<fn_SerializeInt_t*>(slot);

    s_initialized = true;
}

// ===== Standalone factory (Foundation D — component instantiation) =====
#include "Object/factory_registration.h"
#include "Component/component_factory_base.h"
COMPONENT_INERT_FACTORY(OrchardComponent, 0x68, 0x80)
REGISTER_TYPE(OrchardComponent, "ControllerComponent",
              &OrchardComponent_Factory, &OrchardComponent_CreateInPlace);
