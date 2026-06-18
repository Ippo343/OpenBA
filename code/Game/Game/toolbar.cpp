// code/Game/Game/toolbar.cpp
// Clean reimplementation of Toolbar game logic.
//
// Toolbar (0x68 bytes) -- toolbar UI container.
// Extends Object::External (0x20 base).
// Vtable at 0x140160120.
//
// Contains toolbar items, layout configuration, selection state,
// and references to building descriptions.
//
// Decompiled from Application-x64.exe.

#include "Game/toolbar_funcs.h"
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>

// ============================================================
// Typed ExternalPtr serialize for Toolbar (exe function at 0x140024c70)
// Uses TypeInfo_Toolbar for type checking during load.
// Declared locally -- should be moved to engine_funcs.h later.
// ============================================================
typedef void (*fn_ExtPtr_Toolbar_Serialize_t)(void* field, void* serializer);
static fn_ExtPtr_Toolbar_Serialize_t s_fn_ExtPtr_Toolbar_Serialize = nullptr;

static void EnsureResolved() {
    if (s_fn_ExtPtr_Toolbar_Serialize) return;
    s_fn_ExtPtr_Toolbar_Serialize = reinterpret_cast<fn_ExtPtr_Toolbar_Serialize_t>(
        ResolveAddr(Module::Exe, 0x140024c70));
}

// ============================================================
// Toolbar::Dtor (vtable +0x00) -- Scalar deleting destructor
// ============================================================
// Original: 0x140024c10
//
// Destroys two String fields (+0x38, +0x30) in reverse order,
// calls External base dtor, conditionally frees memory.
void* Toolbar_Dtor(Toolbar* self, uint32_t flags) {
    // Destroy String at +0x38 then +0x30
    if (fn_String_Dtor) {
        fn_String_Dtor(&self->_field38);
        fn_String_Dtor(&self->_field30);
    }

    // Call External base destructor
    if (fn_External_Dtor) {
        fn_External_Dtor(self);
    }

    // Conditional free
    if (flags & 1) {
        fn_MemFreeStatic(self);
    }
    return self;
}

// ============================================================
// Toolbar::Serialize (vtable +0x28, Slot5) -- Serialize fields
// ============================================================
// Original: 0x1400d2010
//
// Serializes all Toolbar fields:
//   ExtPtr2 +0x20, ExtPtr +0x28, String +0x30, String +0x38,
//   encoding vcall(0x98) +0x48, ExtPtrToolbar +0x60,
//   ExtPtrBuild +0x58, encoding vcall(0x98) +0x40,
//   bool +0x45, bool +0x44, int +0x4c, int +0x50
void Toolbar_Serialize(Toolbar* self, void* stream) {
    EnsureResolved();

    // Create serializer on stack (24 bytes), version 0
    uint8_t serializer[24];
    if (g_fn_Serializer_Ctor) {
        g_fn_Serializer_Ctor(serializer, stream, 0);
    }

    // ExternalPtr fields
    if (fn_ExternalPtr_Serialize2) {
        fn_ExternalPtr_Serialize2(&self->_field20, serializer);
    }
    if (fn_ExternalPtr_Serialize) {
        fn_ExternalPtr_Serialize(&self->_field28, serializer);
    }

    // String fields
    if (g_fn_String_Serialize) {
        g_fn_String_Serialize(&self->_field30, serializer);
        g_fn_String_Serialize(&self->_field38, serializer);
    }

    // Encoding vcall 0x98 for int at +0x48
    SerializerEncodingVCall(serializer, 0x98, (void*)((uintptr_t)self + 0x48));

    // Typed ExternalPtr serialize for Toolbar at +0x60
    if (s_fn_ExtPtr_Toolbar_Serialize) {
        s_fn_ExtPtr_Toolbar_Serialize(&self->_field60, serializer);
    }

    // ExternalPtr<BuildingDescription> at +0x58
    if (fn_ExternalPtr_BuildSerialize) {
        fn_ExternalPtr_BuildSerialize(&self->_field58, serializer);
    }

    // Encoding vcall 0x98 for int at +0x40
    SerializerEncodingVCall(serializer, 0x98, (void*)((uintptr_t)self + 0x40));

    // Bool fields
    if (g_fn_Serialize_Bool) {
        g_fn_Serialize_Bool(serializer, &self->_field45, nullptr);
        g_fn_Serialize_Bool(serializer, &self->_field44, nullptr);
    }

    // Int fields
    if (g_fn_Serialize_Int) {
        g_fn_Serialize_Int(serializer, &self->selectedIndex, nullptr);
        g_fn_Serialize_Int(serializer, &self->_field50, nullptr);
    }

    // Destroy serializer
    if (g_fn_Serializer_Dtor) {
        g_fn_Serializer_Dtor(serializer);
    }
}

// ============================================================
// Toolbar::Cleanup (vtable +0x30, Slot6) -- Reset to defaults
// ============================================================
// Original: 0x140024bf0
//
// Destroys via Dtor (flags=0), then re-constructs in place.
void Toolbar_Cleanup(Toolbar* self) {
    // Call Dtor with flags=0 (don't free memory)
    typedef void* (*fn_VtableDtor_t)(void* self, uint32_t flags);
    fn_VtableDtor_t dtor = *(fn_VtableDtor_t*)(*(uintptr_t*)self);
    dtor(self, 0);

    // Re-construct in place
    Toolbar_Init(self);
}

// ============================================================
// Toolbar::Init -- Initialize all fields to defaults
// ============================================================
// Mirrors Toolbar_Ctor at 0x1400d2130.
// Calls Object::External::External for base init, then sets
// Toolbar-specific fields to their default values.
void Toolbar_Init(Toolbar* self) {
    // Initialize External base class
    if (fn_External_External) {
        fn_External_External(self);
    }

    // Set Toolbar vtable
    static void* s_vtable = nullptr;
    if (!s_vtable) s_vtable = (void*)ResolveAddr(Module::Exe, ToolbarAddr::Vtable);
    self->vtable = s_vtable;

    // Initialize Toolbar-specific fields to defaults
    self->_field20      = nullptr;  // +0x20: ExternalPtr
    self->_field28      = nullptr;  // +0x28: ExternalPtr
    self->_field30      = nullptr;  // +0x30: String (zeroed = empty)
    self->_field38      = nullptr;  // +0x38: String (zeroed = empty)
    self->_field40      = 8;        // +0x40: default 8
    self->_field44      = true;     // +0x44: default true (0x101 as uint16 = both bytes set)
    self->_field45      = true;     // +0x45: default true
    self->_field48      = 0;        // +0x48: zeroed
    self->selectedIndex = -1;       // +0x4C: -1 sentinel (no selection)
    self->_field58      = nullptr;  // +0x58: ExternalPtr
    self->_field60      = nullptr;  // +0x60: ExternalPtr
}

// ============================================================
// Toolbar::Slot8 (vtable +0x40) -- Construct from raw memory
// ============================================================
// Original: 0x140024be0
//
// NOT HOOKED (RIP-relative JNZ in prologue, unsafe for trampoline).
// Included for standalone exe completeness.
//
// Null-safe init: if self is null, returns null. Otherwise calls
// External base ctor and initializes fields. Unlike Toolbar_Init,
// this directly initializes fields without going through the full
// ctor path (writes via pointer offsets matching the original).
void* Toolbar_Slot8(Toolbar* self) {
    if (!self) return nullptr;

    // Initialize External base class
    if (fn_External_External) {
        fn_External_External(self);
    }

    // Set Toolbar vtable
    static void* s_vtable = nullptr;
    if (!s_vtable) s_vtable = (void*)ResolveAddr(Module::Exe, ToolbarAddr::Vtable);
    self->vtable = s_vtable;

    // Initialize fields (same defaults as Init/Ctor)
    self->_field20      = nullptr;
    self->_field28      = nullptr;
    self->_field30      = nullptr;
    self->_field38      = nullptr;
    self->_field40      = 8;
    self->_field44      = true;
    self->_field45      = true;
    self->_field48      = 0;
    self->selectedIndex = -1;
    self->_field58      = nullptr;
    self->_field60      = nullptr;

    return self;
}

// ============================================================
// Standalone factory + type registration
// ============================================================
// Required so the in-game resource preload (UpdateLoop::LoadResources, the
// "Game/Game" group enabled by BANISHED_GAME_PRELOAD) can instantiate a Toolbar
// when External::Load reads Game\Toolbar.rsc out of the resource tree. Without
// it the loader hits `[NullFactory] Hit for type: Toolbar` -> IsDerivedFrom(NULL)
// -> crash. Mirrors the Scenario/NameDatabase factory-cascade pattern.
//
// Original registration (Toolbar_RegisterType @0x14014d410):
//   BaseTypeInfo(&TypeInfo_Toolbar, "Toolbar", "Object::External",
//                null, Toolbar_Factory(0x140024ba0), Toolbar_Cleanup).
// Vtable 0x140160120 is a 9-slot Object::External layout (byte-verified):
//   +0x00 Dtor(0x140024c10) +0x10 GetTypeId(0x140024b80, =TypeInfo_Toolbar)
//   +0x18 GetFileVersion(0x1400d2120, =4) +0x28 Serialize(0x1400d2010)
//   +0x30 Cleanup(0x140024bf0) +0x40 Init(0x140024be0).
#ifdef STANDALONE_EXE

#include "Object/factory_registration.h"
#include "address_registry.h"

// Slot2 (vtable +0x10): GetTypeId — original returns TypeInfo_Toolbar; standalone
// reads it from REGISTER_TYPE storage.
static uint32_t Toolbar_GetTypeId(void* /*self*/) {
    static uint32_t cached = 0;
    if (cached == 0) cached = ::Object::GetTypeIdByName("Toolbar");
    return cached;
}

// Slot3 (vtable +0x18): GetFileVersion — original (0x1400d2120) returns 4.
// External::Load compares this against the version word in the resource data.
static uint32_t Toolbar_GetFileVersion(void* /*self*/) {
    return 4;
}

// Slot8 (vtable +0x40): in-place Init wrapper with the original's null guard,
// returning self (raw vtable slot signature).
static void* Toolbar_Slot8_Init(void* self) {
    if (self) Toolbar_Slot8(reinterpret_cast<Toolbar*>(self));
    return self;
}

// 9-slot vtable matching exe 0x140160120 (Object::External base).
static void* g_Toolbar_Vtable[9] = {
    reinterpret_cast<void*>(&Toolbar_Dtor),           // +0x00 Dtor
    nullptr,                                           // +0x08 Slot1
    reinterpret_cast<void*>(&Toolbar_GetTypeId),       // +0x10 Slot2 GetTypeId
    reinterpret_cast<void*>(&Toolbar_GetFileVersion),  // +0x18 Slot3 GetFileVersion
    nullptr,                                           // +0x20 Slot4 (base thunk)
    reinterpret_cast<void*>(&Toolbar_Serialize),       // +0x28 Slot5 Serialize
    reinterpret_cast<void*>(&Toolbar_Cleanup),         // +0x30 Slot6 Cleanup
    nullptr,                                           // +0x38 Slot7
    reinterpret_cast<void*>(&Toolbar_Slot8_Init),      // +0x40 Slot8 Init
};

extern "C" void RegisterToolbarVtable() {
    RegisterExeAddr(ToolbarAddr::Vtable, reinterpret_cast<uintptr_t>(g_Toolbar_Vtable));
}

extern "C" void* Toolbar_Factory(void) {
    if (!fn_MemAlloc || !g_pMemAllocSingleton) return nullptr;
    void* p = fn_MemAlloc(g_pMemAllocSingleton, 0x68, 8);  // Toolbar size = 0x68
    if (!p) return nullptr;
    Toolbar_Init(reinterpret_cast<Toolbar*>(p));
    return p;
}

extern "C" void* Toolbar_CreateInPlace(void* memory) {
    if (!memory) return nullptr;
    Toolbar_Init(reinterpret_cast<Toolbar*>(memory));
    return memory;
}

// Parent "Object::External" matches the original Toolbar_RegisterType.
REGISTER_TYPE(Toolbar, "Object::External",
              &Toolbar_Factory, &Toolbar_CreateInPlace);

#endif // STANDALONE_EXE
