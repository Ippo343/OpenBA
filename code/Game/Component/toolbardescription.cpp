// code/Game/Component/toolbardescription.cpp
// Clean reimplementation of ToolbarDescription (entity sub-description).
//
// ToolbarDescription (0x68 bytes) — extends Object::External (0x20 base).
// Parent type name: "Description". Vtable @ 0x140164980.  GetVersion = 4.
//
// NOTE: this is an exe-defined type parented to "Description", NOT the Runtime
// UI::ElementDescription. UIDescription/ToolbarDescription register their own
// factory in the exe; they are not Runtime-self-registered.
//
// Field layout (from Ctor 0x14012dc30 + Serialize 0x14012da30 + dtor 0x14012db70):
//   +0x20: void*   _descRef       ExternalPtr (Description)         (default 0)
//   +0x28: String  _name                                           (default empty)
//   +0x30: void*   _iconRef       ExternalPtr (StringTable variant) (default 0)
//   +0x38: String  _str1                                           (default empty)
//   +0x40: String  _str2                                           (default empty)
//   +0x48: String  _str3                                           (default empty)
//   +0x50: String* _entries[]     String array buffer              (default 0)
//   +0x58: int32   _entryCount                                     (default 0)
//   +0x5c: int32   _entryCap                                       (default 0)
//   +0x60: int32   _val0          (default -1)
//   +0x64: int32   _val1          (default 0)
//
// Serialize (0x14012da30) stream order:
//   1. ExternalPtr (+0x20)               FUN_14000bd30
//   2. String (+0x28)
//   3. ExternalPtr (+0x30)               FUN_14000bf40 (StringTable variant)
//   4. String (+0x38)
//   5. String (+0x40)
//   6. String (+0x48)
//   7. int count (+0x58)
//   8. (load) resize String array (+0x50) to count   FUN_140028860
//   9. loop count: String::Serialize(entries[i])
//   10. int (+0x64)
//   11. int (+0x60)
//
// Decompiled from Application-x64.exe (READ-ONLY Ghidra).

#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>

namespace {
constexpr uintptr_t kVtable  = 0x140164980;
constexpr int       kVersion = 4;
}

static void* ToolbarDescription_Ctor_Impl(void* mem) {
    if (fn_External_External) fn_External_External(mem);
    uintptr_t s = (uintptr_t)mem;
    *(void**)mem = (void*)ResolveAddr(Module::Exe, kVtable);
    *(uint64_t*)(s + 0x20) = 0;   // _descRef
    *(uint64_t*)(s + 0x28) = 0;   // _name (empty String)
    *(uint64_t*)(s + 0x30) = 0;   // _iconRef
    *(uint64_t*)(s + 0x38) = 0;   // _str1
    *(uint64_t*)(s + 0x40) = 0;   // _str2
    *(uint64_t*)(s + 0x48) = 0;   // _str3
    *(uint64_t*)(s + 0x50) = 0;   // _entries ptr
    *(uint64_t*)(s + 0x58) = 0;   // count + cap
    *(uint32_t*)(s + 0x60) = 0xffffffff;  // _val0 = -1
    *(uint32_t*)(s + 0x64) = 0;   // _val1
    return mem;
}

// Serialize (vtable +0x28, 0x14012da30)
void ToolbarDescription_Serialize(void* self, void* stream) {
    uintptr_t s = (uintptr_t)self;
    uint8_t serializer[24];
    if (!g_fn_Serializer_Ctor || !g_fn_Serialize_Int || !g_fn_String_Serialize ||
        !fn_ExternalPtr_Serialize2) {
        return;
    }
    g_fn_Serializer_Ctor(serializer, stream, 0);

    // +0x20 in the exe uses FUN_14000bd30; +0x30 uses FUN_14000bf40. Both have
    // a byte-identical stream layout (only the validated typeId differs, trusted
    // in standalone). Use the registered Serialize2 for both.
    fn_ExternalPtr_Serialize2((void*)(s + 0x20), serializer);  // 1. _descRef
    g_fn_String_Serialize((void*)(s + 0x28), serializer);      // 2. _name
    fn_ExternalPtr_Serialize2((void*)(s + 0x30), serializer);  // 3. _iconRef (StringTable)
    g_fn_String_Serialize((void*)(s + 0x38), serializer);      // 4. _str1
    g_fn_String_Serialize((void*)(s + 0x40), serializer);      // 5. _str2
    g_fn_String_Serialize((void*)(s + 0x48), serializer);      // 6. _str3

    // 7. count (+0x58)
    g_fn_Serialize_Int(serializer, (void*)(s + 0x58), nullptr);
    int count = *(int32_t*)(s + 0x58);

    // 8. on load, resize the String array (+0x50) to count
    void* serObj = *(void**)(serializer + 8);
    bool loading = serObj && (*(int*)((uintptr_t)serObj + 8) == 0);
    if (loading && fn_StringArray_Resize) {
        fn_StringArray_Resize((void*)(s + 0x50), count);
    }

    // 9. per-element String::Serialize (stride 8)
    uint8_t* buf = *(uint8_t**)(s + 0x50);
    if (buf) {
        for (int i = 0; i < count; ++i) {
            g_fn_String_Serialize(buf + (size_t)i * 8, serializer);
        }
    }

    // 10/11. two ints (note: +0x64 BEFORE +0x60 in the original stream)
    g_fn_Serialize_Int(serializer, (void*)(s + 0x64), nullptr);
    g_fn_Serialize_Int(serializer, (void*)(s + 0x60), nullptr);

    if (g_fn_Serializer_Dtor) g_fn_Serializer_Dtor(serializer);
}

// Dtor (vtable +0x00, 0x140039500 / cleanup 0x14012db70).
// Destroys the String array (+0x50) elements + buffer, then the four String
// fields, then the base External.
void* ToolbarDescription_Dtor(void* self, uint32_t flags) {
    uintptr_t s = (uintptr_t)self;
    *(void**)self = (void*)ResolveAddr(Module::Exe, kVtable);

    if (fn_String_Dtor) {
        uint8_t* buf = *(uint8_t**)(s + 0x50);
        int n = *(int32_t*)(s + 0x58);
        if (buf) {
            for (int i = 0; i < n; ++i) fn_String_Dtor(buf + (size_t)i * 8);
        }
    }
    if (fn_MemFreeStatic) fn_MemFreeStatic(*(void**)(s + 0x50));
    *(uint64_t*)(s + 0x50) = 0;
    *(uint64_t*)(s + 0x58) = 0;
    if (fn_String_Dtor) {
        fn_String_Dtor((void*)(s + 0x48));
        fn_String_Dtor((void*)(s + 0x40));
        fn_String_Dtor((void*)(s + 0x38));
        fn_String_Dtor((void*)(s + 0x28));
    }
    if (fn_External_Dtor) fn_External_Dtor(self);
    if ((flags & 1) && fn_MemFreeStatic) fn_MemFreeStatic(self);
    return self;
}

#ifdef STANDALONE_EXE
#include "Object/factory_registration.h"
#include "address_registry.h"

static uint32_t ToolbarDescription_GetTypeId(void*) {
    static uint32_t cached = 0;
    if (cached == 0) cached = ::Object::GetTypeIdByName("ToolbarDescription");
    return cached;
}
static int   Vtbl_GetVersion(void*) { return kVersion; }
static int   Vtbl_Return0(void*) { return 0; }
static void* Vtbl_ReturnNull(void*, ...) { return nullptr; }

static void* g_Vtable[11] = {
    reinterpret_cast<void*>(&ToolbarDescription_Dtor),      // +0x00
    reinterpret_cast<void*>(&Vtbl_ReturnNull),              // +0x08 GetTypeInfo
    reinterpret_cast<void*>(&ToolbarDescription_GetTypeId), // +0x10
    reinterpret_cast<void*>(&Vtbl_GetVersion),              // +0x18 (=4)
    reinterpret_cast<void*>(&Vtbl_Return0),                 // +0x20 Compile
    reinterpret_cast<void*>(&ToolbarDescription_Serialize), // +0x28
    reinterpret_cast<void*>(&Vtbl_Return0),                 // +0x30 Cleanup
    reinterpret_cast<void*>(&Vtbl_Return0),                 // +0x38 GetDesc
    reinterpret_cast<void*>(&Vtbl_ReturnNull),              // +0x40 Slot8
    reinterpret_cast<void*>(&Vtbl_ReturnNull),              // +0x48 guard
    reinterpret_cast<void*>(&Vtbl_ReturnNull),              // +0x50 guard
};

extern "C" void RegisterToolbarDescriptionVtable() {
    RegisterExeAddr(kVtable, reinterpret_cast<uintptr_t>(g_Vtable));
}
extern "C" void* ToolbarDescription_Factory(void) {
    if (!fn_MemAlloc || !g_pMemAllocSingleton) return nullptr;
    void* p = fn_MemAlloc(g_pMemAllocSingleton, 0x68, 8);
    if (!p) return nullptr;
    return ToolbarDescription_Ctor_Impl(p);
}
extern "C" void* ToolbarDescription_CreateInPlace(void* mem) {
    if (!mem) return nullptr;
    return ToolbarDescription_Ctor_Impl(mem);
}

REGISTER_TYPE(ToolbarDescription, "Description",
              &ToolbarDescription_Factory, &ToolbarDescription_CreateInPlace);
#endif
