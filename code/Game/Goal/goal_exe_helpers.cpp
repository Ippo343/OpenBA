// code/Game/Goal/goal_exe_helpers.cpp
// Clean reimplementation of the StatGoal lazy ExternalPtr resolver
// (FUN_1400265b0), ported to close the standalone Goal code-gap.
//
// Depends only on engine_funcs.h + resolve.h.
// Decompiled from Application-x64.exe via Ghidra.

#include "goal_exe_helpers.h"
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>

// ============================================================
// StatGoal ExternalPtr resolver (FUN_1400265b0)
// ============================================================
// Identical in shape to ResolveExternalByName_Impl (engine_funcs.h): a lazy
// ExternalPtr<T>::Resolve. The input points at an ExternalPtr struct:
//   +0x00: String (8 bytes) — resource name
//   +0x08: int32  — cached generation counter (vs ExternalFactory+0x08)
//   +0x10: void*  — cached External* result
// If the cached generation matches the factory's, the cached pointer is valid.
// Otherwise it converts the name String to a Filename, calls
// ExternalFactory::Find, validates the result against the ExternalPtr's
// expected type (TypeInfo at *0x140152870), caches it (or null), and updates
// the generation.
//
// In standalone the type-id storage slot (0x140152870) is unregistered → null,
// so the IsDerivedFrom check is skipped and the Find result is trusted — same
// strategy as ResolveExternalByName_Impl's g_TypeId_ComponentDescription_FC guard.
void StatGoal_ResolveExternalPtr(void* externalRef) {
    uintptr_t s = reinterpret_cast<uintptr_t>(externalRef);
    if (!g_pExternalFactorySingleton || !*g_pExternalFactorySingleton) return;
    void* factory = *g_pExternalFactorySingleton;

    int32_t refGen     = *reinterpret_cast<int32_t*>(s + 0x08);
    int32_t factoryGen = *reinterpret_cast<int32_t*>(reinterpret_cast<uintptr_t>(factory) + 0x08);
    if (refGen == factoryGen) return;

    uint8_t filename[8] = {};
    if (fn_Filename_CtorString) fn_Filename_CtorString(filename, reinterpret_cast<const void*>(s));
    void* found = fn_ExternalFactory_Find ? fn_ExternalFactory_Find(factory, filename) : nullptr;
    if (fn_Filename_Dtor) fn_Filename_Dtor(filename);

    void* result = nullptr;
    if (found) {
        // TypeInfo lives at *(*(0x140152870)); type id is its first dword.
        const uint32_t* pTypeId = nullptr;
        uintptr_t slot = ResolveAddr(Module::Exe, 0x140152870);
        if (slot) {
            void* tinfo = *reinterpret_cast<void**>(slot);
            if (tinfo) pTypeId = reinterpret_cast<const uint32_t*>(tinfo);
        }
        if (!pTypeId || (fn_IsDerivedFrom && fn_IsDerivedFrom(found, *pTypeId))) {
            result = found;
        }
    }

    *reinterpret_cast<void**>(s + 0x10) = result;
    *reinterpret_cast<int32_t*>(s + 0x08) = factoryGen;
}
