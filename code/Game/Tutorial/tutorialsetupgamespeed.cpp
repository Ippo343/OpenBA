// code/Game/Tutorial/tutorialsetupgamespeed.cpp
// Clean reimplementation of TutorialSetupGameSpeed vtable methods.
//
// TutorialSetupGameSpeed (0x28 bytes) -- set game speed for tutorial steps.
// Vtable: 0x140160da0
// Extends TutorialSetup -> Object::External
//
// Fields:
//   +0x20: int32_t speed      -- game speed to set (default 1)
//   +0x24: uint8_t _field24   -- if nonzero, also pause (set controller +0x30 to 0)
//
// The Apply function (Slot8) writes speed to the game's time/speed controller,
// and optionally pauses the game.
//
// Decompiled from Application-x64.exe using Ghidra.

#include "Tutorial/tutorialsetupgamespeed_funcs.h"
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch D
#include <new>

// ============================================================
// TutorialSetupGameSpeed::Slot8/Apply (vtable +0x40, 0x1400e2d60)
// ============================================================
// Sets game speed from self->speed into the game's time controller.
// If _field24 is set, also pauses the game (controller +0x30 = 0).
//
// Original disassembly:
//   MOV R8,[g_pGameWorldSingleton]   ; R8 = game singleton
//   MOV EAX,[RCX+0x20]              ; EAX = self->speed
//   MOV RDX,[R8+0x120]              ; RDX = time controller
//   MOV [RDX+0x34],EAX              ; controller->speed = self->speed
//   CMP byte [RCX+0x24],0           ; if (_field24)
//   JZ skip
//   MOV RAX,[R8+0x120]              ;   re-read controller
//   MOV byte [RAX+0x30],0           ;   controller->paused = 0
//   skip: RET
void TutorialSetupGameSpeed_Slot8(void* self) {
    uintptr_t s = (uintptr_t)self;

    if (!g_pGameWorldSingleton || !*g_pGameWorldSingleton) return;
    uintptr_t game = (uintptr_t)*g_pGameWorldSingleton;

    // game+0x120 = time/speed controller pointer
    uintptr_t timeCtrl = *(uintptr_t*)(game + 0x120);
    if (timeCtrl == 0) return;

    // Write speed to controller +0x34
    int32_t speed = *(int32_t*)(s + 0x20);
    *(int32_t*)(timeCtrl + 0x34) = speed;

    // If _field24 is set, also set controller +0x30 to 0 (pause)
    uint8_t field24 = *(uint8_t*)(s + 0x24);
    if (field24 != 0) {
        // Re-read controller (matching original code)
        timeCtrl = *(uintptr_t*)(game + 0x120);
        *(uint8_t*)(timeCtrl + 0x30) = 0;
    }
}

// ============================================================
// TutorialSetupGameSpeed::Dtor (vtable +0x00, 0x140028ac0)
// ============================================================
// 1. Set vtable to TutorialSetupGameSpeed
// 2. Set vtable to TutorialSetup base
// 3. Call External::~External
// 4. Optionally free memory
void* TutorialSetupGameSpeed_Dtor(void* self, uint32_t flags) {
    uintptr_t s = (uintptr_t)self;

    // Set vtable to TutorialSetupGameSpeed (standard MSVC dtor pattern)
    uintptr_t myVtable = ResolveAddr(Module::Exe, TutorialSetupGameSpeedAddr::Vtable);
    *(uintptr_t*)s = myVtable;

    // Set vtable to TutorialSetup base
    uintptr_t baseVtable = ResolveAddr(Module::Exe, TutorialSetupAddr::Vtable);
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
// TutorialSetupGameSpeed::Serialize (vtable +0x28, 0x1400e2d90)
// ============================================================
// Creates serializer, serializes speed (int32) and _field24 (bool).
void TutorialSetupGameSpeed_Serialize(void* self, void* stream) {
    uintptr_t s = (uintptr_t)self;

    alignas(8) uint8_t ser[32];
    if (g_fn_Serializer_Ctor) {
        ::new (static_cast<void*>(ser)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge
    }

    // Serialize speed (+0x20) -- int
    if (g_fn_Serialize_Int) {
        reinterpret_cast<IO::Serializer*>(ser)->Serialize(*reinterpret_cast<int*>(s + 0x20), nullptr);   // B.3.4 bridge
    }

    // Serialize _field24 (+0x24) -- bool
    if (g_fn_Serialize_Bool) {
        reinterpret_cast<IO::Serializer*>(ser)->Serialize(*reinterpret_cast<bool*>(s + 0x24), nullptr);   // B.3.4 bridge
    }

    if (g_fn_Serializer_Dtor) {
        reinterpret_cast<IO::Serializer*>(ser)->~Serializer();   // B.3.4 bridge
    }
}

// ============================================================
// TutorialSetupGameSpeed::Cleanup (vtable +0x30, 0x140028a70)
// ============================================================
// Calls ScalarDtor(self, 0) via vtable, re-initializes, sets defaults.
void TutorialSetupGameSpeed_Cleanup(void* self) {
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

    // Set vtable chain: TutorialSetup -> TutorialSetupGameSpeed
    uintptr_t baseVtable = ResolveAddr(Module::Exe, TutorialSetupAddr::Vtable);
    *(uintptr_t*)s = baseVtable;
    uintptr_t myVtable = ResolveAddr(Module::Exe, TutorialSetupGameSpeedAddr::Vtable);
    *(uintptr_t*)s = myVtable;

    // Reset fields to defaults
    *(int32_t*)(s + 0x20) = 1;
    *(uint8_t*)(s + 0x24) = 0;
}

// ============================================================
// TutorialSetupGameSpeed::Ctor (vtable +0x58, 0x140028a20)
// ============================================================
// Constructor/init called by factory after allocation.
void* TutorialSetupGameSpeed_Ctor(void* self) {
    if (self == nullptr) {
        return nullptr;
    }

    uintptr_t s = (uintptr_t)self;

    // Call External::External base init
    if (fn_External_External) {
        fn_External_External(self);
    }

    // Set vtable chain: TutorialSetup -> TutorialSetupGameSpeed
    uintptr_t baseVtable = ResolveAddr(Module::Exe, TutorialSetupAddr::Vtable);
    *(uintptr_t*)s = baseVtable;
    uintptr_t myVtable = ResolveAddr(Module::Exe, TutorialSetupGameSpeedAddr::Vtable);
    *(uintptr_t*)s = myVtable;

    // Set default field values
    *(int32_t*)(s + 0x20) = 1;
    *(uint8_t*)(s + 0x24) = 0;

    return self;
}

// ============================================================
// Standalone factory + vtable — External-resource family (factory cascade).
// Vtable @ 0x140160da0 (9 slots, Object::External base), size 0x28, parent "TutorialSetup".
// GetFileVersion (+0x18) = 1. MusicList pattern (musiclist.cpp).
// ============================================================
#include "Object/factory_registration.h"
#include "address_registry.h"

static uint32_t TutorialSetupGameSpeed_GetTypeId(void*) {
    static uint32_t c = 0;
    if (!c) c = ::Object::GetTypeIdByName("TutorialSetupGameSpeed");
    return c;
}
static int TutorialSetupGameSpeed_GetFileVersion(void*) { return 1; }

// Retail slots 9/10 on the TutorialSetup family are empty `ret` bodies.
static void TutorialSetup_SlotEmpty(void*) {}

static void* g_TutorialSetupGameSpeed_Vtable[12] = {
    (void*)&TutorialSetupGameSpeed_Dtor,            // +0x00 Dtor
    nullptr,                      // +0x08 (base thunk)
    (void*)&TutorialSetupGameSpeed_GetTypeId,       // +0x10 GetTypeId
    (void*)&TutorialSetupGameSpeed_GetFileVersion,  // +0x18 GetFileVersion
    nullptr,                      // +0x20 (shared base thunk 0x14003a912)
    (void*)&TutorialSetupGameSpeed_Serialize,       // +0x28 Serialize
    (void*)&TutorialSetupGameSpeed_Cleanup,         // +0x30 Cleanup
    nullptr,                      // +0x38 (base 0x140001640)
    (void*)&TutorialSetupGameSpeed_Slot8,   // +0x40 Apply (0x1400e2d60)
    (void*)&TutorialSetup_SlotEmpty,   // +0x48 Update (0x1400e2d50 - empty in retail)
    (void*)&TutorialSetup_SlotEmpty,   // +0x50 Undo (0x1400e2d40 - empty in retail)
    (void*)&TutorialSetupGameSpeed_Ctor,   // +0x58 Ctor/CreateInPlace (0x140028a20)
};

extern "C" void RegisterTutorialSetupGameSpeedVtable() {
    RegisterExeAddr(0x140160da0, (uintptr_t)g_TutorialSetupGameSpeed_Vtable);
}
extern "C" void* TutorialSetupGameSpeed_CreateInPlace(void* mem) {
    return TutorialSetupGameSpeed_Ctor(mem);
}
extern "C" void* TutorialSetupGameSpeed_Factory(void) {
    if (!fn_MemAlloc || !g_pMemAllocSingleton) return nullptr;
    void* p = fn_MemAlloc(g_pMemAllocSingleton, 0x28, 8);
    return p ? TutorialSetupGameSpeed_Ctor(p) : nullptr;
}
REGISTER_TYPE(TutorialSetupGameSpeed, "TutorialSetup", &TutorialSetupGameSpeed_Factory, &TutorialSetupGameSpeed_CreateInPlace);
