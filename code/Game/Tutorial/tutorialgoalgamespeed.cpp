// code/Game/Goal/tutorialgoalgamespeed.cpp
// Clean reimplementation of TutorialGoalGameSpeed vtable methods.
//
// TutorialGoalGameSpeed (0x28 bytes) — tutorial goal waiting for game speed change.
// Vtable: 0x1401608e0
// Extends TutorialGoal -> Object::External
//
// Fields:
//   +0x20: int32_t targetSpeed — target game speed (default 1)
//
// The Check function is trivial: compare game's current speed against targetSpeed.
// Reads from: game(singleton) -> +0x120 (time/speed controller) -> +0x34 (current speed)
//
// Decompiled from Application-x64.exe using Ghidra.

#include "Tutorial/tutorialgoalgamespeed_funcs.h"
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch D
#include <new>

// ============================================================
// TutorialGoalGameSpeed::Check (vtable +0x40, 0x1400df150)
// ============================================================
// Compares game's current speed against targetSpeed.
// Original: MOV RAX,[g_pGameWorldSingleton]; MOV ECX,[RCX+0x20];
//           MOV RDX,[RAX+0x120]; CMP [RDX+0x34],ECX; SETZ AL; RET
int TutorialGoalGameSpeed_Check(TutorialGoalGameSpeed* self) {
    if (!g_pGameWorldSingleton || !*g_pGameWorldSingleton) return 0;
    uintptr_t game = (uintptr_t)*g_pGameWorldSingleton;

    // game+0x120 = time/speed controller
    uintptr_t timeCtrl = *(uintptr_t*)(game + 0x120);
    if (timeCtrl == 0) return 0;

    // time controller +0x34 = current game speed
    int32_t currentSpeed = *(int32_t*)(timeCtrl + 0x34);

    return (currentSpeed == self->targetSpeed) ? 1 : 0;
}

// ============================================================
// TutorialGoalGameSpeed::Dtor (vtable +0x00, 0x140027730)
// ============================================================
// 1. Set vtable to TutorialGoalGameSpeed (no-op destruction step)
// 2. Set vtable to TutorialGoal base
// 3. Call External::~External
// 4. Optionally free memory
void* TutorialGoalGameSpeed_Dtor(void* self, uint32_t flags) {
    uintptr_t s = (uintptr_t)self;

    // Set vtable to TutorialGoalGameSpeed (for correct dtor dispatch if nested)
    uintptr_t myVtable = ResolveAddr(Module::Exe, TutorialGoalGameSpeedAddr::Vtable);
    *(uintptr_t*)s = myVtable;

    // Set vtable to TutorialGoal base
    uintptr_t baseVtable = ResolveAddr(Module::Exe, TutorialGoalAddr::Vtable);
    *(uintptr_t*)s = baseVtable;

    // Call External::~External
    if (fn_External_Dtor) {
        fn_External_Dtor(self);
    }

    // Conditional free
    if ((flags & 1) != 0) {
        if (fn_MemFreeStatic) {
            fn_MemFreeStatic(self);
        }
    }

    return self;
}

// ============================================================
// TutorialGoalGameSpeed::Serialize (vtable +0x28, 0x1400df170)
// ============================================================
// 1. Call base TutorialGoal::Serialize (empty serializer create/destroy)
// 2. Create new serializer, serialize int at +0x20 (targetSpeed)
void TutorialGoalGameSpeed_Serialize(void* self, void* stream) {
    uintptr_t s = (uintptr_t)self;

    // Base class serialize
    alignas(8) uint8_t ser0[32];
    ::new (static_cast<void*>(ser0)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(ser0)->~Serializer();   // B.3.4 bridge

    // Serialize our fields
    alignas(8) uint8_t ser[32];
    ::new (static_cast<void*>(ser)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge

    // Serialize targetSpeed (+0x20) — int
    reinterpret_cast<IO::Serializer*>(ser)->Serialize(*reinterpret_cast<int*>(s + 0x20), nullptr);   // B.3.4 bridge

    reinterpret_cast<IO::Serializer*>(ser)->~Serializer();   // B.3.4 bridge
}

// ============================================================
// TutorialGoalGameSpeed::Cleanup (vtable +0x30, 0x1400276e0)
// ============================================================
// Calls ScalarDtor(self, 0) via vtable, then re-initializes.
void TutorialGoalGameSpeed_Cleanup(void* self) {
    uintptr_t s = (uintptr_t)self;

    // Call ScalarDtor(self, 0) via vtable
    uintptr_t* vtable = *(uintptr_t**)s;
    typedef void* (*DtorFn)(void*, uint32_t);
    DtorFn dtor = (DtorFn)vtable[0];
    dtor(self, 0);

    // Re-initialize via External::External
    if (fn_External_External) {
        fn_External_External(self);
    }

    // Set vtable chain
    uintptr_t baseVtable = ResolveAddr(Module::Exe, TutorialGoalAddr::Vtable);
    *(uintptr_t*)s = baseVtable;
    uintptr_t myVtable = ResolveAddr(Module::Exe, TutorialGoalGameSpeedAddr::Vtable);
    *(uintptr_t*)s = myVtable;

    // Reset targetSpeed to default
    *(int32_t*)(s + 0x20) = 1;
}

// ============================================================
// TutorialGoalGameSpeed::Init (vtable +0x48, 0x140027690)
// ============================================================
// Constructor/init called by factory after allocation.
void* TutorialGoalGameSpeed_Init(void* self) {
    if (self == nullptr) {
        return nullptr;
    }

    uintptr_t s = (uintptr_t)self;

    // Call External::External base init
    if (fn_External_External) {
        fn_External_External(self);
    }

    // Set vtable chain: TutorialGoal -> TutorialGoalGameSpeed
    uintptr_t baseVtable = ResolveAddr(Module::Exe, TutorialGoalAddr::Vtable);
    *(uintptr_t*)s = baseVtable;
    uintptr_t myVtable = ResolveAddr(Module::Exe, TutorialGoalGameSpeedAddr::Vtable);
    *(uintptr_t*)s = myVtable;

    // Set default target speed
    *(int32_t*)(s + 0x20) = 1;

    return self;
}

// ============================================================
// Standalone factory + vtable — External-resource family (factory cascade).
// Vtable @ 0x1401608e0 (9 slots, Object::External base), size 0x28, parent "TutorialGoal".
// GetFileVersion (+0x18) = 1. MusicList pattern (musiclist.cpp).
// ============================================================
#include "Object/factory_registration.h"
#include "address_registry.h"

static uint32_t TutorialGoalGameSpeed_GetTypeId(void*) {
    static uint32_t c = 0;
    if (!c) c = ::Object::GetTypeIdByName("TutorialGoalGameSpeed");
    return c;
}
static int TutorialGoalGameSpeed_GetFileVersion(void*) { return 1; }

static void* g_TutorialGoalGameSpeed_Vtable[10] = {
    (void*)&TutorialGoalGameSpeed_Dtor,            // +0x00 Dtor
    nullptr,                      // +0x08 (base thunk)
    (void*)&TutorialGoalGameSpeed_GetTypeId,       // +0x10 GetTypeId
    (void*)&TutorialGoalGameSpeed_GetFileVersion,  // +0x18 GetFileVersion
    nullptr,                      // +0x20 (shared base thunk 0x14003a912)
    (void*)&TutorialGoalGameSpeed_Serialize,       // +0x28 Serialize
    (void*)&TutorialGoalGameSpeed_Cleanup,         // +0x30 Cleanup
    nullptr,                      // +0x38 (base 0x140001640)
    (void*)&TutorialGoalGameSpeed_Check,   // +0x40 Check (0x1400df150)
    (void*)&TutorialGoalGameSpeed_Init,   // +0x48 Ctor/CreateInPlace (0x140027690)
};

extern "C" void RegisterTutorialGoalGameSpeedVtable() {
    RegisterExeAddr(0x1401608e0, (uintptr_t)g_TutorialGoalGameSpeed_Vtable);
}
extern "C" void* TutorialGoalGameSpeed_CreateInPlace(void* mem) {
    return TutorialGoalGameSpeed_Init(mem);
}
extern "C" void* TutorialGoalGameSpeed_Factory(void) {
    if (!fn_MemAlloc || !g_pMemAllocSingleton) return nullptr;
    void* p = fn_MemAlloc(g_pMemAllocSingleton, 0x28, 8);
    return p ? TutorialGoalGameSpeed_Init(p) : nullptr;
}
REGISTER_TYPE(TutorialGoalGameSpeed, "TutorialGoal", &TutorialGoalGameSpeed_Factory, &TutorialGoalGameSpeed_CreateInPlace);
