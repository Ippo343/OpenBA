// code/Game/Tutorial/tutorialimagehotkey.cpp
// Clean reimplementation of TutorialImageHotKey vtable methods.
//
// TutorialImageHotKey (0x48 bytes) — displays hot key image in tutorial steps.
// Vtable: 0x140160ab0
// Extends TutorialImage -> Object::External
//
// Fields (from Ghidra decompilation of Ctor at 0x1400e0680):
//   +0x00: vtable pointer
//   +0x08: External base (0x18 bytes)
//   +0x20: void* imageRef — External pointer to image resource (TutorialImage base)
//   +0x28: int32_t field28 — base image param 1 (serialized)
//   +0x2c: int32_t field2c — base image param 2 (serialized)
//   +0x30: System::String hotkeyName — hotkey identifier string
//   +0x38: int32_t field38 — initialized to -1
//   +0x3c: padding
//   +0x40: void* field40 — zeroed (pointer, used in Slot9 for KeyMap lookup)
//
// Decompiled from Application-x64.exe using Ghidra.
// This file must NOT depend on hooks.h — portable for standalone exe.

#include "Tutorial/tutorialimagehotkey_funcs.h"
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>

// ============================================================
// Local function pointers resolved on first use
// ============================================================

// TutorialImage External ref serialize helper (0x140027ad0)
// Serializes/deserializes an External pointer with factory lookup
typedef void (*fn_ExternalRef_Serialize_t)(void* externalPtr, void* serializer);
static fn_ExternalRef_Serialize_t s_fn_ExternalRef_Serialize = nullptr;

// FUN_140027de0 — resolves hotkey string to External ref via ExternalFactory
typedef void (*fn_ResolveHotkeyRef_t)(void* stringField);
static fn_ResolveHotkeyRef_t s_fn_ResolveHotkeyRef = nullptr;

static bool s_funcsResolved = false;

static void EnsureFuncsResolved() {
    if (s_funcsResolved) return;
    s_funcsResolved = true;

    s_fn_ExternalRef_Serialize = (fn_ExternalRef_Serialize_t)
        ResolveAddr(Module::Exe, 0x140027ad0);
    s_fn_ResolveHotkeyRef = (fn_ResolveHotkeyRef_t)
        ResolveAddr(Module::Exe, 0x140027de0);
}

// ============================================================
// TutorialImageHotKey::Dtor (vtable +0x00, 0x140027d70)
// ============================================================
// Scalar deleting destructor.
// 1. Set vtable to TutorialImageHotKey
// 2. Destroy String at +0x30
// 3. Set vtable to TutorialImage (base)
// 4. Call External::~External
// 5. If (flags & 1), free memory
void* TutorialImageHotKey_Dtor(void* self, uint32_t flags) {
    uintptr_t s = (uintptr_t)self;

    // Set vtable to TutorialImageHotKey
    uintptr_t vtable = ResolveAddr(Module::Exe, TutorialImageHotKeyAddr::Vtable);
    *(uintptr_t*)s = vtable;

    // Destroy String at +0x30
    if (fn_String_Dtor) {
        fn_String_Dtor((void*)(s + 0x30));
    }

    // Set vtable to TutorialImage (base)
    uintptr_t baseVtable = ResolveAddr(Module::Exe, TutorialImageAddr::Vtable);
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
// TutorialImageHotKey::Serialize (vtable +0x28, 0x1400e0540)
// ============================================================
// Two-phase serialization:
// Phase 1: Serialize base TutorialImage fields (int +0x28, int +0x2c, external ref +0x20)
// Phase 2: Serialize HotKey string +0x30, resolve external ref on load
void TutorialImageHotKey_Serialize(void* self, void* stream) {
    uintptr_t s = (uintptr_t)self;
    EnsureFuncsResolved();

    // Phase 1: Base TutorialImage fields
    uint8_t ser1[32];
    if (g_fn_Serializer_Ctor) {
        g_fn_Serializer_Ctor(ser1, stream, 0);
    }

    // Serialize int at +0x28
    if (g_fn_Serialize_Int) {
        g_fn_Serialize_Int(ser1, (void*)(s + 0x28), nullptr);
    }

    // Serialize int at +0x2c
    if (g_fn_Serialize_Int) {
        g_fn_Serialize_Int(ser1, (void*)(s + 0x2c), nullptr);
    }

    // Serialize external ref at +0x20
    if (s_fn_ExternalRef_Serialize) {
        s_fn_ExternalRef_Serialize((void*)(s + 0x20), ser1);
    }

    if (g_fn_Serializer_Dtor) {
        g_fn_Serializer_Dtor(ser1);
    }

    // Phase 2: HotKey-specific fields
    uint8_t ser2[32];
    if (g_fn_Serializer_Ctor) {
        g_fn_Serializer_Ctor(ser2, stream, 0);
    }

    // Serialize String at +0x30
    if (g_fn_String_Serialize) {
        g_fn_String_Serialize((void*)(s + 0x30), ser2);
    }

    // If loading (version == 0), resolve the hotkey string to an External ref
    if (g_fn_Serializer_GetVersion) {
        int version = g_fn_Serializer_GetVersion(ser2);
        if (version == 0) {
            if (s_fn_ResolveHotkeyRef) {
                s_fn_ResolveHotkeyRef((void*)(s + 0x30));
            }
        }
    }

    if (g_fn_Serializer_Dtor) {
        g_fn_Serializer_Dtor(ser2);
    }
}

// ============================================================
// TutorialImageHotKey::Cleanup (vtable +0x30, 0x140027d50)
// ============================================================
// Calls dtor(self, 0) via vtable, then re-initializes via Ctor.
void TutorialImageHotKey_Cleanup(void* self) {
    uintptr_t s = (uintptr_t)self;

    // Call ScalarDtor(self, 0) via vtable — destroys fields
    uintptr_t* vtable = *(uintptr_t**)s;
    typedef void* (*DtorFn)(void*, uint32_t);
    DtorFn dtor = (DtorFn)vtable[0];
    dtor(self, 0);

    // Re-initialize
    TutorialImageHotKey_Ctor(self);
}

// ============================================================
// TutorialImageHotKey::Ctor (real at 0x1400e0680)
// ============================================================
// Called by factory after allocation and by Cleanup for re-init.
// 1. Call External::External base init
// 2. Set vtable to TutorialImage, zero base fields
// 3. Set vtable to TutorialImageHotKey, zero derived fields
void* TutorialImageHotKey_Ctor(void* self) {
    if (self == nullptr) {
        return nullptr;
    }

    uintptr_t s = (uintptr_t)self;

    // Call External::External base init
    if (fn_External_External) {
        fn_External_External(self);
    }

    // Set vtable to TutorialImage (base), zero base fields
    uintptr_t baseVtable = ResolveAddr(Module::Exe, TutorialImageAddr::Vtable);
    *(uintptr_t*)s = baseVtable;
    *(uint64_t*)(s + 0x20) = 0;  // imageRef = null
    *(uint64_t*)(s + 0x28) = 0;  // field28 + field2c = 0

    // Set vtable to TutorialImageHotKey, init derived fields
    uintptr_t myVtable = ResolveAddr(Module::Exe, TutorialImageHotKeyAddr::Vtable);
    *(uintptr_t*)s = myVtable;
    *(uint64_t*)(s + 0x30) = 0;       // hotkeyName = empty string
    *(int32_t*)(s + 0x38) = -1;       // field38 = -1 sentinel
    *(uint64_t*)(s + 0x40) = 0;       // field40 = null

    return self;
}

// Slot9 (vtable +0x48, 0x1400e0420) — kept as trampoline in reimpl layer.
// Requires many unresolved UI/KeyMap engine functions for clean reimpl.
