// code/Game/Dialog/tutorialsetupdialog.cpp
// Clean reimplementation of TutorialSetupDialog vtable methods.
//
// TutorialSetupDialog (0x78 bytes), vtable at 0x140160d20.
// Extends Object::External (NOT DialogInstance). Base ~0x20 bytes.
//
// Struct layout (from Ctor + Shutdown decompilation):
//   +0x00: vtable ptr
//   +0x08: Object::External base data (0x18 bytes)
//   +0x20: void* — string array ptr (1st vector data)
//   +0x28: uint32_t — count of 1st string vector
//   +0x2C: uint32_t — capacity of 1st string vector (managed by FUN_140028860)
//   +0x30: void* — string array ptr (2nd vector data)
//   +0x38: uint32_t — count of 2nd string vector
//   +0x3C: uint32_t — capacity of 2nd string vector
//   +0x40: System::String (inline, 8 bytes)
//   +0x48: int32_t field
//   +0x4C: padding
//   +0x50: External* — external resource pointer (serialized as reference)
//   +0x58: System::String (inline, 8 bytes) — external path string
//   +0x60: int32_t = -1 sentinel
//   +0x64: padding
//   +0x68: void* pointer field
//   +0x70: uint32_t field
//   +0x74: padding

#include "Dialog/ui_dialogs.h"
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>

// ============================================================
// Locally resolved function pointers
// (not yet in engine_funcs.h — resolved from exe exref table or direct addresses)
// ============================================================

// IO::Serializer::Serializer(this, stream, mode) — via exref 0x140152ad8
typedef void (*fn_SerializerCtor_t)(void* self, void* stream, int mode);
static fn_SerializerCtor_t fn_SerializerCtor_local = nullptr;

// IO::Serializer::~Serializer(this) — via exref 0x140152ad0
typedef void (*fn_SerializerDtor_t)(void* self);
static fn_SerializerDtor_t fn_SerializerDtor_local = nullptr;

// IO::Serializer::Serialize(this, int* field, const char* name) — via exref 0x140152ac8
typedef void (*fn_SerializeInt_t)(void* self, void* field, const char* name);
static fn_SerializeInt_t fn_SerializeInt_local = nullptr;

// System::String::Serialize(this, serializer) — via exref 0x140151238
typedef void (*fn_StringSerialize_t)(void* self, void* serializer);
static fn_StringSerialize_t fn_StringSerialize_local = nullptr;

// System::Filename::Filename(this) — default ctor, via exref 0x140151d40
typedef void (*fn_FilenameCtor_t)(void* self);
static fn_FilenameCtor_t fn_FilenameCtor_local = nullptr;

// System::Filename::~Filename(this) — dtor, via exref 0x1401529e8
typedef void (*fn_FilenameDtor_t)(void* self);
static fn_FilenameDtor_t fn_FilenameDtor_local = nullptr;

// FUN_140028860 — string vector resize helper (exe-local)
// Signature: (vector_ptr, new_count) — manages String[] array with ptr+count+cap layout
typedef void (*fn_StringVectorResize_t)(void* vecPtr, uint32_t newCount);
static fn_StringVectorResize_t fn_StringVectorResize_local = nullptr;

// FUN_140028650 — serialize External pointer as a reference (exe-local)
// Signature: (External** fieldPtr, Serializer*)
typedef void (*fn_SerializeExternalPtr_t)(void* fieldPtr, void* serializer);
static fn_SerializeExternalPtr_t fn_SerializeExternalPtr_local = nullptr;

// FUN_14001b1d0 — ResolveExternalByName(String*) — resolve external resource by name
typedef void (*fn_ResolveExternalByName_t)(void* str);
static fn_ResolveExternalByName_t fn_ResolveExternalByName_local = nullptr;

static bool s_localFuncsResolved = false;

static void ResolveTutorialSetupDialogFuncs() {
    if (s_localFuncsResolved) return;

    // Resolve via exref table entries in exe .rdata
    // Each exref is a void* slot filled at startup with the resolved runtime function ptr.
    auto loadExref = [](uintptr_t addr) -> void* {
        void** slot = reinterpret_cast<void**>(ResolveAddr(Module::Exe, addr));
        return (slot && *slot) ? *slot : nullptr;
    };

    fn_SerializerCtor_local  = (fn_SerializerCtor_t) loadExref(0x140152ad8);
    fn_SerializerDtor_local  = (fn_SerializerDtor_t) loadExref(0x140152ad0);
    fn_SerializeInt_local    = (fn_SerializeInt_t)   loadExref(0x140152ac8);
    fn_StringSerialize_local = (fn_StringSerialize_t)loadExref(0x140151238);
    fn_FilenameCtor_local    = (fn_FilenameCtor_t)   loadExref(0x140151d40);
    fn_FilenameDtor_local    = (fn_FilenameDtor_t)   loadExref(0x1401529e8);

    // Direct exe addresses for inline helpers
    fn_StringVectorResize_local   = (fn_StringVectorResize_t)  ResolveAddr(Module::Exe, 0x140028860);
    fn_SerializeExternalPtr_local = (fn_SerializeExternalPtr_t)ResolveAddr(Module::Exe, 0x140028650);
    fn_ResolveExternalByName_local= (fn_ResolveExternalByName_t)ResolveAddr(Module::Exe, 0x14001b1d0);

    s_localFuncsResolved = true;
}

// ============================================================
// TutorialSetupDialog::ScalarDtor (vtable slot 0, exe 0x140028620)
// ============================================================
//
// Sets TutorialSetupDialog vtable, destroys inline strings and string vector arrays,
// calls Object::External::~External, then conditionally frees the object memory.
//
// The "body" sub-function (FUN_1400e2bf0) is inlined here since it is only
// called from this destructor:
//   1. Restore TutorialSetupDialog vtable
//   2. Destroy inline String at +0x58
//   3. Destroy inline String at +0x40
//   4. Destroy each String in the 2nd vector (ptrs at *(+0x30), count at +0x38)
//   5. Free the 2nd vector buffer; zero +0x30 and +0x38
//   6. Destroy each String in the 1st vector (ptrs at *(+0x20), count at +0x28)
//   7. Free the 1st vector buffer; zero +0x20 and +0x28
//   8. Restore TutorialSetup base vtable (0x140160c30)
//   9. Call Object::External::~External
//  10. Conditionally call MemFreeStatic if flags & 1
void* TutorialSetupDialog_ScalarDtor(void* self, uint32_t flags) {
    uintptr_t s = (uintptr_t)self;

    // Step 1: restore TutorialSetupDialog vtable
    static void* s_vtable = nullptr;
    if (!s_vtable) s_vtable = (void*)ResolveAddr(Module::Exe, TutorialSetupDialogAddr::Vtable);
    *(void**)self = s_vtable;

    // Step 2: destroy inline String at +0x58
    fn_String_Dtor((void*)(s + 0x58));

    // Step 3: destroy inline String at +0x40
    fn_String_Dtor((void*)(s + 0x40));

    // Step 4-5: destroy 2nd string vector (ptr at +0x30, count at +0x38)
    {
        uint32_t count = *(uint32_t*)(s + 0x38);
        void* buf      = *(void**)(s + 0x30);
        if (buf && count > 0) {
            uint8_t* p = (uint8_t*)buf;
            for (uint32_t i = 0; i < count; ++i, p += 8) {
                fn_String_Dtor(p);
            }
        }
        fn_MemFreeStatic(buf);
        *(uint64_t*)(s + 0x30) = 0;
        *(uint64_t*)(s + 0x38) = 0;
    }

    // Step 6-7: destroy 1st string vector (ptr at +0x20, count at +0x28)
    {
        uint32_t count = *(uint32_t*)(s + 0x28);
        void* buf      = *(void**)(s + 0x20);
        if (buf && count > 0) {
            uint8_t* p = (uint8_t*)buf;
            for (uint32_t i = 0; i < count; ++i, p += 8) {
                fn_String_Dtor(p);
            }
        }
        fn_MemFreeStatic(buf);
        *(uint64_t*)(s + 0x20) = 0;
        *(uint64_t*)(s + 0x28) = 0;
    }

    // Step 8: restore TutorialSetup base vtable
    static void* s_baseVtable = nullptr;
    if (!s_baseVtable) s_baseVtable = (void*)ResolveAddr(Module::Exe, TutorialSetupDialogAddr::BaseVtable);
    *(void**)self = s_baseVtable;

    // Step 9: call Object::External::~External
    fn_External_Dtor(self);

    // Step 10: conditional memory free
    if (flags & 1) {
        fn_MemFreeStatic(self);
    }

    return self;
}

// ============================================================
// TutorialSetupDialog::Shutdown (vtable slot 5, exe 0x1400e2a60)
// ============================================================
//
// Serialization method: saves/loads tutorial setup state via IO::Serializer.
// Handles two dynamic String vectors, two inline Strings, an External pointer,
// two int fields, and resolves external reference by name on load.
//
// Returns 1 (true) always.
uint64_t TutorialSetupDialog_Shutdown(void* self, void* stream) {
    ResolveTutorialSetupDialogFuncs();
    uintptr_t s = (uintptr_t)self;

    // Create the serializer on the stack (24 bytes for Serializer object)
    uint8_t serializer[24];
    fn_SerializerCtor_local(serializer, stream, 0);

    // Serialize 1st string vector count at +0x28
    uint32_t count1 = *(uint32_t*)(s + 0x28);
    fn_SerializeInt_local(serializer, &count1, nullptr);

    // On load (serializer mode == 0): resize 1st vector to the loaded count
    void* serializerBase = *(void**)serializer;
    if (*(int*)((uintptr_t)serializerBase + 0x8) == 0) {
        fn_StringVectorResize_local((void*)(s + 0x20), count1);
    }

    // Serialize each string in 1st vector
    if ((int32_t)count1 > 0) {
        uint8_t* p = *(uint8_t**)(s + 0x20);
        for (uint32_t i = 0; i < count1; ++i, p += 8) {
            fn_StringSerialize_local(p, serializer);
        }
    }

    // Serialize 2nd string vector count at +0x38
    uint32_t count2 = *(uint32_t*)(s + 0x38);
    fn_SerializeInt_local(serializer, &count2, nullptr);

    // On load: resize 2nd vector
    serializerBase = *(void**)serializer;
    if (*(int*)((uintptr_t)serializerBase + 0x8) == 0) {
        fn_StringVectorResize_local((void*)(s + 0x30), count2);
    }

    // Serialize each string in 2nd vector
    if ((int32_t)count2 > 0) {
        uint8_t* p = *(uint8_t**)(s + 0x30);
        for (uint32_t i = 0; i < count2; ++i, p += 8) {
            fn_StringSerialize_local(p, serializer);
        }
    }

    // Serialize inline String at +0x40
    fn_StringSerialize_local((void*)(s + 0x40), serializer);

    // Serialize External* pointer at +0x50 (as external reference)
    fn_SerializeExternalPtr_local((void*)(s + 0x50), serializer);

    // Serialize int32 at +0x48
    fn_SerializeInt_local(serializer, (void*)(s + 0x48), nullptr);

    // Construct a temporary Filename on the stack (needed as scope guard by the original)
    uint8_t tempFilename[8];
    fn_FilenameCtor_local(tempFilename);

    // Serialize inline String at +0x58 (external path)
    fn_StringSerialize_local((void*)(s + 0x58), serializer);

    // On load: resolve the external resource by name
    serializerBase = *(void**)serializer;
    if (*(int*)((uintptr_t)serializerBase + 0x8) == 0) {
        fn_ResolveExternalByName_local((void*)(s + 0x58));
    }

    // Destroy temporary Filename
    fn_FilenameDtor_local(tempFilename);

    // Serialize int32 at +0x70
    fn_SerializeInt_local(serializer, (void*)(s + 0x70), nullptr);

    // Destroy serializer
    fn_SerializerDtor_local(serializer);

    return 1;
}

// ============================================================
// TutorialSetupDialog::OnEvent (vtable slot 6, exe 0x140028600)
// ============================================================
//
// Resets the TutorialSetupDialog to a clean state:
//   1. Calls vtable[0](self, 0) — virtual destructor with no-free flag,
//      which destroys all sub-objects (strings, vectors, External dtor)
//   2. Re-constructs the object by calling TutorialSetupDialog_Ctor,
//      which sets vtables and zeros all fields.
//
// This is a "reset" operation used when the tutorial setup needs to be reused.
void TutorialSetupDialog_OnEvent(void* self) {
    // Step 1: call virtual destructor (slot 0) with flags=0 (destruct without freeing)
    typedef void* (*fn_VDtor_t)(void* self, uint32_t flags);
    void** vtbl = *(void***)self;
    fn_VDtor_t vdtor = (fn_VDtor_t)vtbl[0];
    vdtor(self, 0);

    // Step 2: re-construct via TutorialSetupDialog_Ctor
    // (sets External base, both vtables, zeros all fields)
    typedef void* (*fn_TSDCtor_t)(void* self);
    static fn_TSDCtor_t fn_Ctor = nullptr;
    if (!fn_Ctor) fn_Ctor = (fn_TSDCtor_t)ResolveAddr(Module::Exe, TutorialSetupDialogAddr::Ctor);
    fn_Ctor(self);
}
