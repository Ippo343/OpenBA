// standalone/factory_enum_stub.cpp
//
// Accelerator A (notes/PRELOAD_CASCADES_PLAN.md, Phase 0): convert the fatal
// "factory returns NULL -> Base::IsDerivedFrom(NULL) crash @Runtime+0xC039A"
// into a NON-FATAL, enumerable diagnostic so a single in-game preload run lists
// the ENTIRE remaining factory cascade instead of dying on the first type.
//
// Mechanism (from the External::Load / IsDerivedFrom decompiles):
//   Object::External::Load (Runtime 0x1800bc620):
//     param_2 = factoryFn();                       // our factory result
//     IsDerivedFrom(param_2, __typeInfo);          // derefs param_2->vtable[0x10]
//                                                  //   -> NULL deref crash if NULL
//     if (versionFromStream == param_2->vtable[0x18]())   // GetFileVersion gate
//         <deserialize body>                       // String::Serialize etc.
//     // else: body SKIPPED entirely (no field reads, no crash)
//   Object::Base::IsDerivedFrom (Runtime 0x1800c0390):
//     uVar2 = this->vtable[0x10]();                // GetTypeId — the +0xC039A deref
//
// So instead of returning NULL, return a tiny stub object with a real vtable
// whose:
//   slot +0x10 (GetTypeId)      returns GetTypeIdByName(name)  (IsDerivedFrom safe)
//   slot +0x18 (GetFileVersion) returns 0x7FFFFFFF             (NEVER matches data)
//   slot +0x00 (Dtor)           frees self via fn_MemFreeStatic when flags&1
//   all other slots             benign no-op
// The sentinel version forces External::Load to skip the deserialize body every
// time -> no crash -> the loader limps to the next type, which logs its own
// "[NullFactory-ENUM] need factory: X" line. One run enumerates the whole tree.
//
// Gated by env BANISHED_FACTORY_ENUM (checked in the generated LogAndReturnNull,
// code/Game/Object/factory_registrations_bulk.cpp). Default OFF -> the original
// crash-on-first behavior is preserved exactly.

#ifdef STANDALONE_EXE

#include "resolve.h"
#include "engine_funcs.h"
#include "Object/factory_registration.h"

#include <cstdint>
#include <cstdio>
#include <cstring>

namespace {

// Generic no-op for every slot we don't explicitly model. Returns null; never
// called on the skip path, but present so a stray virtual dispatch is benign.
void* EnumStub_Noop(void* /*self*/) { return nullptr; }

// slot +0x00 Dtor (scalar-deleting). flags&1 => free the block.
void* EnumStub_Dtor(void* self, uint32_t flags) {
    if ((flags & 1) && self && fn_MemFreeStatic) fn_MemFreeStatic(self);
    return self;
}

// slot +0x10 GetTypeId — the value IsDerivedFrom reads. We stash the resolved
// typeId at self+0x8 at allocation time so a single shared vtable serves every
// enumerated type.
uint32_t EnumStub_GetTypeId(void* self) {
    return self ? *reinterpret_cast<uint32_t*>(reinterpret_cast<uint8_t*>(self) + 8) : 0;
}

// slot +0x18 GetFileVersion — sentinel that can never equal a real .rsc version
// word, forcing External::Load to skip the deserialize body.
uint32_t EnumStub_GetFileVersion(void* /*self*/) { return 0x7FFFFFFF; }

// 24-slot shared vtable. Filled once (the few modelled slots in the literal,
// the rest defaulted to EnumStub_Noop by EnsureVtable()).
void* g_EnumStub_Vtable[24] = {
    reinterpret_cast<void*>(&EnumStub_Dtor),            // +0x00 Dtor
    nullptr,                                            // +0x08
    reinterpret_cast<void*>(&EnumStub_GetTypeId),       // +0x10 GetTypeId
    reinterpret_cast<void*>(&EnumStub_GetFileVersion),  // +0x18 GetFileVersion
    // +0x20.. filled with EnumStub_Noop by EnsureVtable()
};

void EnsureVtable() {
    static bool s_done = false;
    if (s_done) return;
    for (int i = 4; i < 24; ++i) {
        if (!g_EnumStub_Vtable[i])
            g_EnumStub_Vtable[i] = reinterpret_cast<void*>(&EnumStub_Noop);
    }
    s_done = true;
}

// One-line-per-distinct-type dedup so the enumeration log stays readable.
const char* g_seen[512];
int         g_seenCount = 0;

bool MarkSeen(const char* name) {
    for (int i = 0; i < g_seenCount; ++i)
        if (std::strcmp(g_seen[i], name) == 0) return false;  // already logged
    if (g_seenCount < 512) g_seen[g_seenCount++] = name;      // names are static literals
    return true;
}

} // namespace

// Returns a non-null skip-deserialize stub object (see file header). Called from
// the generated LogAndReturnNull only when BANISHED_FACTORY_ENUM is set.
extern "C" void* FactoryEnumStub(const char* typeName) {
    if (MarkSeen(typeName)) {
        std::fprintf(stderr, "[NullFactory-ENUM] need factory: %s\n", typeName);
        std::fflush(stderr);
    }

    if (!fn_MemAlloc || !g_pMemAllocSingleton) {
        std::fprintf(stderr, "[NullFactory-ENUM] allocator not ready for %s — returning null\n",
                     typeName);
        std::fflush(stderr);
        return nullptr;
    }

    EnsureVtable();

    void* p = fn_MemAlloc(g_pMemAllocSingleton, 0x40, 8);
    if (!p) return nullptr;
    std::memset(p, 0, 0x40);
    *reinterpret_cast<void**>(p) = g_EnumStub_Vtable;                       // [0]   vtable
    *reinterpret_cast<uint32_t*>(reinterpret_cast<uint8_t*>(p) + 8) =       // [0x8] typeId
        ::Object::GetTypeIdByName(typeName);
    return p;
}

#endif // STANDALONE_EXE
