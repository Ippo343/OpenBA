// code/Game/Tutorial/tutorialgoaldialog.cpp
// Clean reimplementation of TutorialGoalDialog vtable methods.
//
// TutorialGoalDialog (0x40 bytes), vtable at 0x140160870.
// Extends Object::External (NOT DialogInstance).
//
// Decompiled from Application-x64.exe using Ghidra.

#include "Tutorial/tutorialgoaldialog_funcs.h"
#include "engine_funcs.h"
#include "resolve.h"
#include "system/String.h"
#include "system/Filename.h"
#include <cstdint>
#include <cstdio>
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch D
#include <new>

// ============================================================
// Local function pointers (resolved once)
// ============================================================

// IO::Serializer::Serializer(this, stream, mode)
typedef void* (*fn_Serializer_Ctor_t)(void* self, void* stream, int mode);
static fn_Serializer_Ctor_t s_fn_Serializer_Ctor = nullptr;

// IO::Serializer::~Serializer(this)
typedef void (*fn_Serializer_Dtor_t)(void* self);
static fn_Serializer_Dtor_t s_fn_Serializer_Dtor = nullptr;

static bool s_resolved = false;

static void EnsureResolved() {
    if (s_resolved) return;
    s_resolved = true;

    // External lifecycle — fn_External_Dtor and fn_External_External are in engine_funcs
    if (!fn_External_Dtor)
        fn_External_Dtor = (fn_External_Dtor_t)ResolveAddr(Module::Runtime, 0x1800bc950);
    if (!fn_External_External)
        fn_External_External = (fn_External_External_t)ResolveAddr(Module::Runtime, 0x1800bc9c0);

    // IO::Serializer — BY NAME (retail 0x1800b9960 / 0x1800b99b0).
    // Both are ported (runtime/src/io/serializer.cpp:636-637). ResolveAddr
    // consults the ADDRESS registry only, so the by-address form these two lines
    // used until 2026-09-07 walked past the port into the shipped DLL, and was
    // counted as ported by the census. Only the name reaches the port, and only
    // the name survives cutover.
    s_fn_Serializer_Ctor = (fn_Serializer_Ctor_t)ResolveExport(
        "??0Serializer@IO@@QEAA@AEAVStream@1@H@Z");
    s_fn_Serializer_Dtor = (fn_Serializer_Dtor_t)ResolveExport(
        "??1Serializer@IO@@QEAA@XZ");
}

// ============================================================
// Vtable addresses (resolved once)
// ============================================================
static void* s_vtable = nullptr;
static void* s_baseVtable = nullptr;

static void EnsureVtables() {
    if (!s_vtable) {
        s_vtable = (void*)ResolveAddr(Module::Exe, 0x140160870);
        s_baseVtable = (void*)ResolveAddr(Module::Exe, 0x1401607a0);
    }
}

// ============================================================
// Slot0: ScalarDtor (0x140027590)
// ============================================================
void* TutorialGoalDialog_ScalarDtor(void* self, uint32_t flags) {
    EnsureResolved();
    EnsureVtables();
    uintptr_t s = (uintptr_t)self;

    // Set vtable to TutorialGoalDialog
    *(void**)self = s_vtable;

    // Destroy string at +0x28 (target entity name)
    reinterpret_cast<System::String*>(s + 0x28)->~String();

    // Destroy string at +0x20 (dialog name)
    reinterpret_cast<System::String*>(s + 0x20)->~String();

    // Set vtable to External base
    *(void**)self = s_baseVtable;

    // Call Object::External::~External
    fn_External_Dtor(self);

    // Conditional free if flags & 1
    if (flags & 1) {
        fn_MemFreeStatic(self);
    }

    return self;
}

// ============================================================
// Slot5: Shutdown/Serialize (0x1400defd0)
// ============================================================
uint64_t TutorialGoalDialog_Shutdown(void* self, void* stream) {
    EnsureResolved();
    uintptr_t s = (uintptr_t)self;

    // First serializer block: version check (create + immediately destroy)
    alignas(8) uint8_t serializer1[24];
    ::new (static_cast<void*>(serializer1)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(serializer1)->~Serializer();   // B.3.4 bridge

    // Second serializer block: actual serialization
    alignas(8) uint8_t serializer2[16];
    ::new (static_cast<void*>(serializer2)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge

    // Serialize dialog name string at +0x20
    reinterpret_cast<System::String*>(s + 0x20)->Serialize(serializer2);

    // Filename temp retail's compiler emitted for this scope; only ever
    // constructed and destroyed.
    System::Filename tempFilename;
    (void)tempFilename;

    // Serialize target entity string at +0x28
    reinterpret_cast<System::String*>(s + 0x28)->Serialize(serializer2);

    // If loading (serializer mode at +0x08 == 0), resolve external by name
    int serializerMode = static_cast<int>(reinterpret_cast<IO::Serializer*>(serializer2)->GetDirection());   // B.3.4 bridge  (IO::Serializer::GetDirection, retail 0x180006210)
    if (serializerMode == 0) {
        ResolveExternalByName_Impl((void*)(s + 0x28));
    }

    // Cleanup
    reinterpret_cast<IO::Serializer*>(serializer2)->~Serializer();   // B.3.4 bridge

    return 1;
}

// ============================================================
// Slot6: OnEvent/Cleanup (0x140027520)
// ============================================================
void* TutorialGoalDialog_OnEvent(void* self, uint64_t p2, uint64_t p3, uint64_t p4) {
    EnsureResolved();
    EnsureVtables();
    uintptr_t s = (uintptr_t)self;

    // Call virtual destructor (vtable slot 0) with flags=0 (destruct without free)
    typedef void* (*fn_VDtor_t)(void* self, uint32_t flags);
    void** vtbl = *(void***)self;
    fn_VDtor_t vdtor = (fn_VDtor_t)vtbl[0];
    vdtor(self, 0);

    // Re-construct: call External base constructor
    fn_External_External(self);

    // Set intermediate External base vtable (immediately overwritten)
    *(void**)self = s_baseVtable;

    // Set final TutorialGoalDialog vtable
    *(void**)self = s_vtable;

    // Zero all subclass fields
    *(uint64_t*)(s + 0x20) = 0;   // dialog name string
    *(uint64_t*)(s + 0x28) = 0;   // target entity string
    *(uint32_t*)(s + 0x30) = 0xFFFFFFFF;  // target index = -1
    *(uint64_t*)(s + 0x38) = 0;   // target pointer

    return self;
}

// ============================================================
// Slot7: Delegator (0x140001640)
// ============================================================
void TutorialGoalDialog_Slot7(void* self) {
    typedef void* (*fn_VDtor_t)(void* self, uint32_t flags);
    void** vtbl = *(void***)self;
    fn_VDtor_t vdtor = (fn_VDtor_t)vtbl[0];
    vdtor(self, 0);
}

// ============================================================
// Slot8: OnInit (0x1400deee0)
// ============================================================
uint64_t TutorialGoalDialog_OnInit(void* self) {
    uintptr_t s = (uintptr_t)self;

    // Find dialog by string name at +0x20
    uint64_t dialogPtr = 0;
    FindDialogByName_Impl(&dialogPtr, (void*)(s + 0x20));

    if (dialogPtr == 0) {
        return 0;  // dialog not found
    }

    // Get dialog's user data at dialogInstance+0x30
    void* userData = *(void**)(dialogPtr + 0x30);
    void* description = nullptr;

    if (userData != nullptr) {
        // Check if it's a BuildingDescription type
        if (fn_IsDerivedFrom(userData, *g_TypeId_ComponentDescription_FC)) {
            description = userData;
        } else {
            // Check if it's a ComponentEntity
            if (fn_IsDerivedFrom(userData, *g_TypeId_ComponentEntity)) {
                // Get Description from ComponentEntity at +0x28
                description = *(void**)((uintptr_t)userData + 0x28);
            }
        }
    }

    // Resolve the target External name at +0x28
    ResolveExternalByName_Impl((void*)(s + 0x28));

    // If self->data38 is set, compare with found description
    void* target = *(void**)(s + 0x38);

    if (target != nullptr) {
        if (target != description) {
            return 0;  // mismatch
        }
    }

    return 1;  // match
}

// ============================================================
// Slot9: OnDestroy (0x1400274b0)
// ============================================================
void* TutorialGoalDialog_OnDestroy(void* self) {
    EnsureResolved();
    EnsureVtables();

    if (self != nullptr) {
        // Reinit External base
        fn_External_External(self);

        // Set final vtable
        *(void**)self = s_vtable;

        uintptr_t s = (uintptr_t)self;

        // Zero all subclass fields
        *(uint64_t*)(s + 0x20) = 0;   // dialog name
        *(uint64_t*)(s + 0x28) = 0;   // target entity name
        *(uint32_t*)(s + 0x30) = 0xFFFFFFFF;  // target index = -1
        *(uint64_t*)(s + 0x38) = 0;   // target pointer
    }

    return self;
}
