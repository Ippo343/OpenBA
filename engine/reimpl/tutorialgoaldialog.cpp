// engine/reimpl/tutorialgoaldialog.cpp
// Reimplementation of TutorialGoalDialog vtable methods from Application-x64.exe.
//
// TutorialGoalDialog (0x40 bytes), vtable at 0x140160870.
// Extends Object::External (NOT DialogInstance).
// Object::External base ~0x20 bytes, subclass fields start at +0x20.
//
// Vtable layout (10 slots — Object::External base):
//   +0x00: 0x140027590 — ScalarDtor (hookable, 14b prologue)
//   +0x08: 0x140027430 — GetTypeInfo (8b, unhookable)
//   +0x10: 0x140027420 — GetTypeId (7b, unhookable)
//   +0x18: 0x1400df080 — Slot3: MOV EAX,1; RET (6b, unhookable)
//   +0x20: 0x14003a912 — Runtime thunk (6b, unhookable)
//   +0x28: 0x1400defd0 — Shutdown: serialize/cleanup (hookable, 14b)
//   +0x30: 0x140027520 — OnEvent: handle goal dialog events (hookable, 14b)
//   +0x38: 0x140001640 — Delegator: MOV RAX,[RCX]; XOR EDX,EDX; JMP [RAX] (8b, unhookable)
//   +0x40: 0x1400deee0 — OnInit: initialize tutorial goal state (hookable, 13b)
//   +0x48: 0x1400274b0 — OnDestroy: cleanup tutorial goal dialog (hookable, 14b)

#include "ui/Dialogs.h"
#include "hooks.h"
#include "resolve.h"
#include "engine_funcs.h"
#include <cstdio>
#include <cstdint>

// ============================================================
// Hook state
// ============================================================
static HookInfo g_hookScalarDtor;
static HookInfo g_hookShutdown;
static HookInfo g_hookOnEvent;
static HookInfo g_hookOnInit;
static HookInfo g_hookOnDestroy;

// ============================================================
// Reimplemented / Passthrough wrappers
// ============================================================

// ============================================================
// Locally resolved function pointers (not in engine_funcs.h yet)
// ============================================================

// Object::External::~External — now in engine_funcs.h as fn_External_Dtor
// Object::External::External — now in engine_funcs.h as fn_External_External

// IO::Serializer::Serializer(this, stream, mode)
typedef void* (*fn_Serializer_Ctor_t)(void* self, void* stream, int mode);
static fn_Serializer_Ctor_t fn_Serializer_Ctor = nullptr;

// IO::Serializer::~Serializer(this)
typedef void (*fn_Serializer_Dtor_t)(void* self);
static fn_Serializer_Dtor_t fn_Serializer_Dtor = nullptr;

// System::String::Serialize(this, serializer)
typedef void (*fn_String_Serialize_t)(void* self, void* serializer);
static fn_String_Serialize_t fn_String_Serialize = nullptr;

// System::Filename::Filename(this) — default ctor
typedef void* (*fn_Filename_Ctor_t)(void* self);
static fn_Filename_Ctor_t fn_Filename_Ctor = nullptr;

// System::Filename::~Filename(this) — dtor
// Note: fn_Filename_Dtor is now the global in engine_funcs.h; use it directly.

// ResolveExternalByName(string*) — 0x14001b1d0 in exe
// Resolves an External resource by looking up the string name in ExternalFactory.
typedef void (*fn_ResolveExternalByName_t)(void* str);
static fn_ResolveExternalByName_t fn_ResolveExternalByName_local = nullptr;

// FindDialogByName(retval, nameStr) — 0x1400e1600 in exe
// Parses dialog name string, looks up first matching dialog by type ID.
// Uses non-trivial return ABI: retval is InternalBasePtr* passed as first arg.
typedef void* (*fn_FindDialogByName_t)(void* retval, void* nameStr);
static fn_FindDialogByName_t fn_FindDialogByName_local = nullptr;

static bool s_localFuncsResolved = false;
static void ResolveTutorialGoalDialogFuncs() {
    if (s_localFuncsResolved) return;

    // External lifecycle — fn_External_Dtor and fn_External_External are in engine_funcs
    // Resolve locally only if global init hasn't run yet
    if (!fn_External_Dtor)
        fn_External_Dtor = (fn_External_Dtor_t)ResolveAddr(Module::Runtime, 0x1800bc950);
    if (!fn_External_External)
        fn_External_External = (fn_External_External_t)ResolveAddr(Module::Runtime, 0x1800bc9c0);

    // IO::Serializer (Runtime DLL)
    fn_Serializer_Ctor = (fn_Serializer_Ctor_t)ResolveAddr(Module::Runtime, 0x1800b9960);
    fn_Serializer_Dtor = (fn_Serializer_Dtor_t)ResolveAddr(Module::Runtime, 0x1800b99b0);

    // Exref table entries in exe .rdata — these hold resolved function pointers
    // String::Serialize at exref 0x140151238
    uintptr_t exref_StringSerialize = ResolveAddr(Module::Exe, 0x140151238);
    if (exref_StringSerialize) fn_String_Serialize = *(fn_String_Serialize_t*)exref_StringSerialize;

    // Filename::Filename at exref 0x140151d40
    uintptr_t exref_FilenameCtor = ResolveAddr(Module::Exe, 0x140151d40);
    if (exref_FilenameCtor) fn_Filename_Ctor = *(fn_Filename_Ctor_t*)exref_FilenameCtor;

    // Filename::~Filename — resolved globally via engine_funcs; also set local fallback
    uintptr_t exref_FilenameDtor = ResolveAddr(Module::Exe, 0x1401529e8);
    if (!fn_Filename_Dtor && exref_FilenameDtor)
        fn_Filename_Dtor = *(fn_Filename_Dtor_t*)exref_FilenameDtor;

    // ResolveExternalByName in exe at 0x14001b1d0
    fn_ResolveExternalByName_local = (fn_ResolveExternalByName_t)ResolveAddr(Module::Exe, 0x14001b1d0);

    // FindDialogByName in exe at 0x1400e1600
    fn_FindDialogByName_local = (fn_FindDialogByName_t)ResolveAddr(Module::Exe, 0x1400e1600);

    s_localFuncsResolved = true;
#ifdef _DEBUG
    printf("[TutorialGoalDialog] Local funcs resolved: ExtDtor=%p ExtCtor=%p SerCtor=%p SerDtor=%p StrSer=%p FnCtor=%p FnDtor=%p Resolve=%p\n",
           (void*)fn_External_Dtor, (void*)fn_External_External,
           (void*)fn_Serializer_Ctor, (void*)fn_Serializer_Dtor,
           (void*)fn_String_Serialize, (void*)fn_Filename_Ctor, (void*)fn_Filename_Dtor,
           (void*)fn_ResolveExternalByName_local);
#endif
}

// Slot 0: ScalarDtor(this, flags) — REIMPLEMENTED
// Sets TutorialGoalDialog vtable, destroys strings at +0x28 and +0x20,
// sets External base vtable, calls External::~External, conditionally frees memory.
// Original: 0x140027590.
typedef void* (*fn_ScalarDtor_t)(void* self, uint32_t flags);
static void* Reimpl_ScalarDtor(void* self, uint32_t flags) {
    g_lastHook = "TutorialGoalDialog::ScalarDtor";
    uintptr_t s = (uintptr_t)self;

    // Set vtable to TutorialGoalDialog vtable
    static void* s_vtable = nullptr;
    if (!s_vtable) s_vtable = (void*)ResolveAddr(Module::Exe, 0x140160870);
    *(void**)self = s_vtable;

    // Destroy string at +0x28 (target entity name)
    fn_String_Dtor((void*)(s + 0x28));

    // Destroy string at +0x20 (dialog name)
    fn_String_Dtor((void*)(s + 0x20));

    // Set vtable to External base vtable
    static void* s_baseVtable = nullptr;
    if (!s_baseVtable) s_baseVtable = (void*)ResolveAddr(Module::Exe, 0x1401607a0);
    *(void**)self = s_baseVtable;

    // Call Object::External::~External
    fn_External_Dtor(self);

    // Conditional free if flags & 1
    if (flags & 1) {
        fn_MemFreeStatic(self);
    }

    return self;
}

// Slot 5: Shutdown(this, stream) — REIMPLEMENTED
// Serialization method for save/load. Creates a Serializer from the stream,
// serializes the dialog name string (+0x20) and target entity string (+0x28).
// When loading (serializer mode == 0), resolves the external reference by name.
// Returns 1 (true) always.
// Original: 0x1400defd0.
typedef uint64_t (*fn_Shutdown_t)(void* self, void* stream);
static uint64_t Reimpl_Shutdown(void* self, void* stream) {
    g_lastHook = "TutorialGoalDialog::Shutdown";
    uintptr_t s = (uintptr_t)self;

    // The original creates a Serializer, immediately destroys it (version check?),
    // then creates another one for the actual serialization.
    uint8_t serializer1[24];
    fn_Serializer_Ctor(serializer1, stream, 0);
    fn_Serializer_Dtor(serializer1);

    // Create the real serializer
    uint8_t serializer2[16];
    fn_Serializer_Ctor(serializer2, stream, 0);

    // Serialize dialog name string at +0x20
    fn_String_Serialize((void*)(s + 0x20), serializer2);

    // Create a temporary Filename (stack local, used for scope lifetime)
    uint8_t tempFilename[8];
    fn_Filename_Ctor(tempFilename);

    // Serialize target entity string at +0x28
    fn_String_Serialize((void*)(s + 0x28), serializer2);

    // If loading (serializer mode at +0x08 == 0), resolve external by name
    int serializerMode = *(int*)(*(uintptr_t*)serializer2 + 0x8);
    if (serializerMode == 0) {
        fn_ResolveExternalByName_local((void*)(s + 0x28));
    }

    // Cleanup
    fn_Filename_Dtor(tempFilename);
    fn_Serializer_Dtor(serializer2);

    return 1;
}

// Slot 6: OnEvent(this, p2, p3, p4) — REIMPLEMENTED
// Destructs and reconstructs the TutorialGoalDialog in place.
// First calls the virtual destructor (ScalarDtor via vtable slot 0) with flags=0
// (no memory free), then re-constructs by calling External::External base ctor,
// setting vtables, and zeroing all subclass fields.
// Original: 0x140027520.
typedef void* (*fn_OnEvent_t)(void* self, uint64_t p2, uint64_t p3, uint64_t p4);
static void* Reimpl_OnEvent(void* self, uint64_t p2, uint64_t p3, uint64_t p4) {
    g_lastHook = "TutorialGoalDialog::OnEvent";
    uintptr_t s = (uintptr_t)self;

    // Call virtual destructor (vtable slot 0) with flags=0 (destruct without free).
    // This handles string cleanup and base destructor.
    typedef void* (*fn_VDtor_t)(void* self, uint32_t flags);
    void** vtbl = *(void***)self;
    fn_VDtor_t vdtor = (fn_VDtor_t)vtbl[0];
    vdtor(self, 0);

    // Re-construct: call External base constructor
    fn_External_External(self);

    // Set intermediate External base vtable (immediately overwritten)
    static void* s_baseVtable = nullptr;
    if (!s_baseVtable) s_baseVtable = (void*)ResolveAddr(Module::Exe, 0x1401607a0);
    *(void**)self = s_baseVtable;

    // Set final TutorialGoalDialog vtable
    static void* s_vtable = nullptr;
    if (!s_vtable) s_vtable = (void*)ResolveAddr(Module::Exe, 0x140160870);
    *(void**)self = s_vtable;

    // Zero all subclass fields
    *(uint64_t*)(s + 0x20) = 0;   // dialog name string
    *(uint64_t*)(s + 0x28) = 0;   // target entity string
    *(uint32_t*)(s + 0x30) = 0xFFFFFFFF;  // target index = -1
    *(uint64_t*)(s + 0x38) = 0;   // target pointer

    return self;
}

// Slot 8: OnInit(this) — REIMPLEMENTED
// Finds a dialog by string name from +0x20, checks its user data against
// the target description. Returns true(1) if the dialog's description
// matches self->data38, or if data38 is null.
//
// Original: 0x1400DEEE0.
static uint64_t Reimpl_OnInit(void* self) {
    g_lastHook = "TutorialGoalDialog::OnInit";
    uintptr_t s = (uintptr_t)self;

    // Find dialog by string name at +0x20
    uint64_t dialogPtr = 0;
    fn_FindDialogByName_local(&dialogPtr, (void*)(s + 0x20));

    if (dialogPtr == 0) {
        return 0;  // dialog not found — return false
    }

    // Get dialog's user data at dialogInstance+0x30
    void* userData = *(void**)(dialogPtr + 0x30);
    void* description = nullptr;

    if (userData != nullptr) {
        // Check if it's a Description type (DAT_1401c7318 = g_TypeId_BuildComponent)
        if (fn_IsDerivedFrom(userData, *g_TypeId_BuildComponent)) {
            // Direct Description — use as-is
            if (fn_IsDerivedFrom(userData, *g_TypeId_BuildComponent)) {
                description = userData;
            }
        } else {
            // Check if it's a ComponentEntity
            if (fn_IsDerivedFrom(userData, *g_TypeId_ComponentEntity)) {
                void* entity = nullptr;
                if (fn_IsDerivedFrom(userData, *g_TypeId_ComponentEntity)) {
                    entity = userData;
                }
                // Get Description from ComponentEntity at +0x28
                description = *(void**)((uintptr_t)entity + 0x28);
            }
        }
    }

    // Resolve the target External name at +0x28
    fn_ResolveExternalByName_local((void*)(s + 0x28));

    // If self->data38 is set, compare with found description
    void* target = *(void**)(s + 0x38);
    if (target != nullptr) {
        fn_ResolveExternalByName_local((void*)(s + 0x28));
        if (target != description) {
            return 0;  // mismatch — return false
        }
    }

    return 1;  // match — return true
}

// Slot 9: OnDestroy(this) — REIMPLEMENTED
// Reinitializes the TutorialGoalDialog to its default state by
// calling the Object::External base constructor, then setting the
// final vtable and zeroing all subclass fields.
//
// Original: 0x1400274B0.
static void* Reimpl_OnDestroy(void* self) {
    g_lastHook = "TutorialGoalDialog::OnDestroy";

    if (self != nullptr) {
        // Call Object::External::External(self) to reinit base
        fn_External_External(self);

        // Set final vtable (PTR_FUN_140160870)
        // The disassembly shows two writes — intermediate then final vtable.
        // We only need the final one since the intermediate is immediately overwritten.
        static void* s_vtable = nullptr;
        if (!s_vtable) s_vtable = (void*)ResolveAddr(Module::Exe, 0x140160870);
        *(void**)self = s_vtable;

        uintptr_t s = (uintptr_t)self;

        // Zero data20 (dialog name string — 8 bytes)
        *(uint64_t*)(s + 0x20) = 0;

        // Zero data28 (target entity string — 8 bytes)
        *(uint64_t*)(s + 0x28) = 0;

        // Set data30 = 0xFFFFFFFF (target index = -1)
        *(uint32_t*)(s + 0x30) = 0xFFFFFFFF;

        // Zero data38 (target pointer)
        *(uint64_t*)(s + 0x38) = 0;
    }

    return self;
}

// ============================================================
// Install / Remove
// ============================================================

void InstallTutorialGoalDialogScalarDtor() {
    ResolveTutorialGoalDialogFuncs();
    uintptr_t target = ResolveAddr(Module::Exe, 0x140027590);
    // Prologue: MOV [rsp+8],rcx(5) + PUSH RDI(1) + SUB RSP,0x30(4) + MOV qword,-2(9) = 19
    InstallHook(target, (void*)Reimpl_ScalarDtor, &g_hookScalarDtor, 19);
#ifdef _DEBUG
    printf("[TutorialGoalDialog] ScalarDtor hooked at %p\n", (void*)target);
#endif
}
void RemoveTutorialGoalDialogScalarDtor() { RemoveHook(&g_hookScalarDtor); }

void InstallTutorialGoalDialogShutdown() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400defd0);
    // Prologue: PUSH RDI(2) + SUB RSP,0x50(4) + MOV qword,-2(9) = 15
    InstallHook(target, (void*)Reimpl_Shutdown, &g_hookShutdown, 15);
#ifdef _DEBUG
    printf("[TutorialGoalDialog] Shutdown hooked at %p\n", (void*)target);
#endif
}
void RemoveTutorialGoalDialogShutdown() { RemoveHook(&g_hookShutdown); }

void InstallTutorialGoalDialogOnEvent() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140027520);
    // Prologue: PUSH RBX(2) + SUB RSP,0x30(4) + MOV qword,-2(9) = 15
    InstallHook(target, (void*)Reimpl_OnEvent, &g_hookOnEvent, 15);
#ifdef _DEBUG
    printf("[TutorialGoalDialog] OnEvent hooked at %p\n", (void*)target);
#endif
}
void RemoveTutorialGoalDialogOnEvent() { RemoveHook(&g_hookOnEvent); }

void InstallTutorialGoalDialogOnInit() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400deee0);
    InstallHook(target, (void*)Reimpl_OnInit, &g_hookOnInit, 13);
#ifdef _DEBUG
    printf("[TutorialGoalDialog] OnInit hooked at %p\n", (void*)target);
#endif
}
void RemoveTutorialGoalDialogOnInit() { RemoveHook(&g_hookOnInit); }

void InstallTutorialGoalDialogOnDestroy() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400274b0);
    // Prologue: PUSH RBX(2) + SUB RSP,0x30(4) + MOV qword,-2(9) = 15
    InstallHook(target, (void*)Reimpl_OnDestroy, &g_hookOnDestroy, 15);
#ifdef _DEBUG
    printf("[TutorialGoalDialog] OnDestroy hooked at %p\n", (void*)target);
#endif
}
void RemoveTutorialGoalDialogOnDestroy() { RemoveHook(&g_hookOnDestroy); }
