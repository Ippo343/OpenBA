// code/Game/Component/uidescription.cpp
// Clean reimplementation of UIDescription (entity sub-description).
//
// UIDescription (0x48 bytes) — extends Object::External (0x20 base).
// Parent type name: "Description". Vtable @ 0x140164a70.  GetVersion = 4.
//
// NOTE: exe-defined type parented to "Description" (NOT Runtime UI::*). It
// registers its own factory (UIDescription_Register @ 0x14014fd10).
//
// Field layout (from Factory inline ctor + Serialize FUN_14012de80 + dtor
// FUN_140039700 / element-array cleanup FUN_1400397d0):
//   +0x20: int32  _valTail        (serialized LAST)                 (default 0)
//   +0x28: void*  _descRef        ExternalPtr (Description)         (default 0)
//   +0x30: void*  _entries[]      element-array buffer (stride 0x28)(default 0)
//   +0x38: int32  _entryCount                                       (default 0)
//   +0x3c: int32  _entryCap                                         (default 0)
//   +0x40: int32  _valHead        (serialized 2nd)                  (default 0)
//
// Per-element layout (stride 0x28):
//   +0x00: Object::Type  (4B, serialized via Object::Type::Serialize IAT 0x1401516b0)
//   +0x08: ExternalPtr   (FUN_140027ad0 — identical stream)
//   +0x10: ExternalPtr   (FUN_140039840 — identical stream)
//   +0x18: ExternalPtr   (FUN_140027ad0 — identical stream)
//   +0x20: String
//
// Serialize (FUN_14012de80) stream order:
//   1. ExternalPtr (+0x28)         FUN_140028650
//   2. int (+0x40)
//   3. int count (+0x38)
//   4. (load) resize element array (+0x30) to count   FUN_140039a50
//   5. loop count: Type::Serialize, ExternalPtr x3, String::Serialize
//   6. int (+0x20)
//
// Decompiled from Application-x64.exe (READ-ONLY Ghidra).

#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>

namespace {
constexpr uintptr_t kVtable  = 0x140164a70;
constexpr int       kVersion = 4;

typedef void (*fn_Type_Serialize_t)(void* type, void* serializer);
fn_Type_Serialize_t s_fn_Type_Serialize = nullptr;
bool s_resolved = false;

void EnsureResolved() {
    if (s_resolved) return;
    // Object::Type::Serialize via IAT dispatch slot 0x1401516b0.
    uintptr_t slot = ResolveAddr(Module::Exe, 0x1401516b0);
    if (slot) s_fn_Type_Serialize = *reinterpret_cast<fn_Type_Serialize_t*>(slot);
    s_resolved = true;
}
}

static void* UIDescription_Ctor_Impl(void* mem) {
    if (fn_External_External) fn_External_External(mem);
    uintptr_t s = (uintptr_t)mem;
    *(void**)mem = (void*)ResolveAddr(Module::Exe, kVtable);
    *(uint32_t*)(s + 0x20) = 0;   // _valTail
    *(uint64_t*)(s + 0x28) = 0;   // _descRef
    *(uint64_t*)(s + 0x30) = 0;   // _entries ptr
    *(uint64_t*)(s + 0x38) = 0;   // count + cap
    *(uint32_t*)(s + 0x40) = 0;   // _valHead
    return mem;
}

// Serialize (vtable +0x28, FUN_14012de80)
void UIDescription_Serialize(void* self, void* stream) {
    EnsureResolved();
    uintptr_t s = (uintptr_t)self;
    uint8_t serializer[24];
    if (!g_fn_Serializer_Ctor || !g_fn_Serialize_Int || !g_fn_String_Serialize ||
        !fn_ExternalPtr_Serialize2) {
        return;
    }
    g_fn_Serializer_Ctor(serializer, stream, 0);

    // 1. _descRef ExternalPtr (+0x28). FUN_140028650 — identical stream layout
    //    to the registered Serialize2 (typeId trusted in standalone).
    fn_ExternalPtr_Serialize2((void*)(s + 0x28), serializer);
    // 2. int (+0x40)
    g_fn_Serialize_Int(serializer, (void*)(s + 0x40), nullptr);
    // 3. count (+0x38)
    g_fn_Serialize_Int(serializer, (void*)(s + 0x38), nullptr);
    int count = *(int32_t*)(s + 0x38);

    // 4. on load, allocate/zero the element array (+0x30), stride 0x28.
    //    Fresh-load: count/cap are 0 from the ctor, so the exe resize helper
    //    (FUN_140039a50) collapses to "alloc count*0x28 + zero each element".
    void* serObj = *(void**)(serializer + 8);
    bool loading = serObj && (*(int*)((uintptr_t)serObj + 8) == 0);
    if (loading) {
        if (fn_MemAlloc && g_pMemAllocSingleton && count > 0) {
            void* buf = fn_MemAlloc(g_pMemAllocSingleton, (uint64_t)count * 0x28, 8);
            *(void**)(s + 0x30) = buf;
            if (buf) {
                uint8_t* e = reinterpret_cast<uint8_t*>(buf);
                for (int i = 0; i < count; ++i, e += 0x28) {
                    *(uint64_t*)(e + 0x00) = 0;
                    *(uint64_t*)(e + 0x08) = 0;
                    *(uint64_t*)(e + 0x10) = 0;
                    *(uint64_t*)(e + 0x18) = 0;
                    *(uint64_t*)(e + 0x20) = 0;  // String (empty)
                }
            }
        } else {
            *(void**)(s + 0x30) = nullptr;
        }
        *(int32_t*)(s + 0x38) = count;
        *(int32_t*)(s + 0x3c) = count;
    }

    // 5. per-element serialize
    uint8_t* buf = *(uint8_t**)(s + 0x30);
    if (buf) {
        uint8_t* e = buf;
        for (int i = 0; i < count; ++i, e += 0x28) {
            if (s_fn_Type_Serialize) s_fn_Type_Serialize(e + 0x00, serializer);
            fn_ExternalPtr_Serialize2(e + 0x08, serializer);
            fn_ExternalPtr_Serialize2(e + 0x10, serializer);
            fn_ExternalPtr_Serialize2(e + 0x18, serializer);
            g_fn_String_Serialize(e + 0x20, serializer);
        }
    }

    // 6. int (+0x20)
    g_fn_Serialize_Int(serializer, (void*)(s + 0x20), nullptr);

    if (g_fn_Serializer_Dtor) g_fn_Serializer_Dtor(serializer);
}

// Dtor (vtable +0x00, FUN_140039700 / element cleanup FUN_1400397d0).
// Destroys each element's String (+0x20), frees the element-array buffer, then
// the base External.
void* UIDescription_Dtor(void* self, uint32_t flags) {
    uintptr_t s = (uintptr_t)self;
    *(void**)self = (void*)ResolveAddr(Module::Exe, kVtable);

    uint8_t* buf = *(uint8_t**)(s + 0x30);
    int n = *(int32_t*)(s + 0x38);
    if (buf && fn_String_Dtor) {
        uint8_t* e = buf;
        for (int i = 0; i < n; ++i, e += 0x28) fn_String_Dtor(e + 0x20);
    }
    if (fn_MemFreeStatic) fn_MemFreeStatic(buf);
    *(uint64_t*)(s + 0x30) = 0;
    *(uint64_t*)(s + 0x38) = 0;

    if (fn_External_Dtor) fn_External_Dtor(self);
    if ((flags & 1) && fn_MemFreeStatic) fn_MemFreeStatic(self);
    return self;
}

#ifdef STANDALONE_EXE
#include "Object/factory_registration.h"
#include "address_registry.h"

static uint32_t UIDescription_GetTypeId(void*) {
    static uint32_t cached = 0;
    if (cached == 0) cached = ::Object::GetTypeIdByName("UIDescription");
    return cached;
}
static int   Vtbl_GetVersion(void*) { return kVersion; }
static int   Vtbl_Return0(void*) { return 0; }
static void* Vtbl_ReturnNull(void*, ...) { return nullptr; }

static void* g_Vtable[11] = {
    reinterpret_cast<void*>(&UIDescription_Dtor),      // +0x00
    reinterpret_cast<void*>(&Vtbl_ReturnNull),         // +0x08 GetTypeInfo
    reinterpret_cast<void*>(&UIDescription_GetTypeId), // +0x10
    reinterpret_cast<void*>(&Vtbl_GetVersion),         // +0x18 (=4)
    reinterpret_cast<void*>(&Vtbl_Return0),            // +0x20 Compile
    reinterpret_cast<void*>(&UIDescription_Serialize), // +0x28
    reinterpret_cast<void*>(&Vtbl_Return0),            // +0x30 Cleanup
    reinterpret_cast<void*>(&Vtbl_Return0),            // +0x38 GetDesc
    reinterpret_cast<void*>(&Vtbl_ReturnNull),         // +0x40 Slot8
    reinterpret_cast<void*>(&Vtbl_ReturnNull),         // +0x48 guard
    reinterpret_cast<void*>(&Vtbl_ReturnNull),         // +0x50 guard
};

extern "C" void RegisterUIDescriptionVtable() {
    RegisterExeAddr(kVtable, reinterpret_cast<uintptr_t>(g_Vtable));
}
extern "C" void* UIDescription_Factory(void) {
    if (!fn_MemAlloc || !g_pMemAllocSingleton) return nullptr;
    void* p = fn_MemAlloc(g_pMemAllocSingleton, 0x48, 8);
    if (!p) return nullptr;
    return UIDescription_Ctor_Impl(p);
}
extern "C" void* UIDescription_CreateInPlace(void* mem) {
    if (!mem) return nullptr;
    return UIDescription_Ctor_Impl(mem);
}

REGISTER_TYPE(UIDescription, "Description",
              &UIDescription_Factory, &UIDescription_CreateInPlace);
#endif
