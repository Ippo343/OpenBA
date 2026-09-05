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
#include <cstdio>
#include <cstdlib>

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

    // STANDALONE DEVIATION, deliberate — identical to the one documented at
    // length in TutRestrict_ResolveRefOrLoad (tutorialsetuprestrictplayer.cpp)
    // and TutBase_ResolveToolbarRef (tutorial_base.cpp).
    //
    // Retail's Find() alone is enough because UpdateLoop::LoadResources has
    // already preloaded the Game/GameResources.rsc dependency tree INTO the
    // ExternalFactory; ours leaves those entries in the loop's m_resourceList,
    // so Find() misses for anything nothing else happened to load.
    //
    // This resolver feeds TutorialGoalCount::Check, whose "Zone" mode counts
    // entities in the EntityList's per-description BST keyed by the resolved
    // description POINTER. A miss writes null, the BST is then searched for key
    // 0, the count comes back 0, and the goal can never be met — the tutorial
    // stalls forever with no error anywhere. Measured on stepFirstClear2, whose
    // goal counts "Template\Clear.rsc": the WoodHouse goal one step earlier only
    // worked because the step's own restriction had FindOrLoad'ed that exact
    // name into the factory first. Nothing ever loads Clear.rsc by name.
    //
    // Retail would fault on neither leg (it never has the null), so this is not
    // a guard over a faithful crash — it is the standalone resource-environment
    // gap. The real fix remains making the preload populate the factory.
    bool loaded = false;
    if (!found && fn_ExternalFactory_FindOrLoad && fn_String_Ctor_CStr && fn_String_Dtor &&
        fn_Filename_CtorString && fn_Filename_Dtor) {
        const char* name = *reinterpret_cast<const char* const*>(s);
        if (name && name[0]) {
            // Only the LOADER rejects backslashes; the factory's key comparison
            // normalizes separators (measured — see tutorial_base.cpp).
            char normalized[260];
            size_t k = 0;
            for (; name[k] && k < sizeof(normalized) - 1; ++k)
                normalized[k] = (name[k] == '\\') ? '/' : name[k];
            normalized[k] = 0;

            uint8_t normStr[8] = {};
            fn_String_Ctor_CStr(normStr, normalized);
            uint8_t normFile[8] = {};
            fn_Filename_CtorString(normFile, normStr);
            found = fn_ExternalFactory_FindOrLoad(factory, normFile);
            fn_Filename_Dtor(normFile);
            fn_String_Dtor(normStr);
            loaded = true;
        }
    }

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
    // Re-read the generation: a FindOrLoad that actually loaded bumps it, and
    // stamping the pre-load value would make the next call redo the whole
    // lookup for nothing.
    *reinterpret_cast<int32_t*>(s + 0x08) =
        *reinterpret_cast<int32_t*>(reinterpret_cast<uintptr_t>(factory) + 0x08);

    if (const char* e = std::getenv("BANISHED_TUTORIAL_TRACE")) {
        if (e[0] && e[0] != '0') {
            const char* name = *reinterpret_cast<const char* const*>(s);
            fprintf(stderr, "[GoalRef] \"%s\" %s -> %p\n",
                    name ? name : "(null)",
                    loaded ? "Find=miss FindOrLoad" : "Find=hit", result);
            fflush(stderr);
        }
    }
}

// ============================================================
// Reference-triple array resize (FUN_14001b340)
// ============================================================
// Callers: BuildingGoal::Serialize (0x1400d76a0) and MapGenerator::Serialize
// (0x1400a2980), both on the LOAD leg only (serializer m_encoding == 0).
// `arr` is the familiar {data* @+0x00, count @+0x08, capacity @+0x0c} header.
//
// Same primitive as EventArray_Grow in code/Game/Game/eventlog.cpp — including
// the identical triple-capacity heuristic on a single-element grow — but with a
// 0x18 stride, over the ExternalPtr triple that StatGoal_ResolveExternalPtr and
// ResolveExternalByName_Impl operate on:
//   +0x00 System::String (resource name)   +0x08 int32 cached generation
//   +0x10 void* cached External*
//
// ⚠ The tail SEED VALUES are load-bearing: a newly added entry gets generation
// **-1**, not 0. Zero is a legitimate ExternalFactory generation, so a 0 seed
// would make the lazy resolver take its "cache still valid" early-out while the
// pointer is still null — the entry would silently never resolve. Faithful to
// retail, which stores 0xffffffff explicitly.
void Goal_RefArray_Resize(void* arr, uint32_t newCount) {
    if (!arr) return;
    uintptr_t* base = reinterpret_cast<uintptr_t*>(arr);
    uint8_t*   hdr  = reinterpret_cast<uint8_t*>(arr);
    uint32_t oldCount = *reinterpret_cast<uint32_t*>(hdr + 8);
    if (newCount == oldCount) return;

    uintptr_t oldData = *base;
    *reinterpret_cast<uint32_t*>(hdr + 8) = newCount;
    int32_t* capPtr = reinterpret_cast<int32_t*>(hdr + 0xC);
    if (*capPtr < static_cast<int32_t>(newCount)) {
        *capPtr = static_cast<int32_t>(newCount);
        if (newCount == oldCount + 1 && oldCount != 0)
            *capPtr = static_cast<int32_t>(newCount) * 3;
        *base = reinterpret_cast<uintptr_t>(
            fn_MemAlloc(g_pMemAllocSingleton, static_cast<uint64_t>(*capPtr) * 0x18, 8));
    }
    uintptr_t newData = *base;
    if (!newData) return;

    // On an in-place grow retail skips the preserved entries entirely and only
    // initialises the tail; when the block moved it copy-constructs all of them.
    uint32_t startIdx = (oldData == newData) ? oldCount : 0;
    for (uint32_t i = startIdx; i < newCount; i++) {
        uint8_t* dst = reinterpret_cast<uint8_t*>(newData) + static_cast<uintptr_t>(i) * 0x18;
        if (i < oldCount) {
            uint8_t* src = reinterpret_cast<uint8_t*>(oldData) + static_cast<uintptr_t>(i) * 0x18;
            fn_String_Ctor_Copy(dst, src);                                       // +0x00
            *reinterpret_cast<uint32_t*>(dst + 0x08) = *reinterpret_cast<uint32_t*>(src + 0x08);
            *reinterpret_cast<uint64_t*>(dst + 0x10) = *reinterpret_cast<uint64_t*>(src + 0x10);
        } else {
            *reinterpret_cast<uint64_t*>(dst + 0x00) = 0;
            *reinterpret_cast<uint32_t*>(dst + 0x08) = 0xFFFFFFFFu;  // generation -1
            *reinterpret_cast<uint64_t*>(dst + 0x10) = 0;
        }
    }

    if (oldData == newData) {
        for (uint32_t i = newCount; i < oldCount; i++)   // in-place shrink
            fn_String_Dtor(reinterpret_cast<uint8_t*>(oldData) + static_cast<uintptr_t>(i) * 0x18);
    } else if (oldData) {
        for (uint32_t i = 0; i < oldCount; i++)
            fn_String_Dtor(reinterpret_cast<uint8_t*>(oldData) + static_cast<uintptr_t>(i) * 0x18);
        fn_MemFreeStatic(reinterpret_cast<void*>(oldData));
    }
}

// ============================================================
// Goal_FillInheritedVtableSlots — see goal_exe_helpers.h
// ============================================================
// Slot 7 (exe 0x140001640): `mov rax,[rcx]; xor edx,edx; jmp [rax]` — tail-call the
// object's OWN slot-0 destructor with flags = 0 (destroy, do not free).
static void* Goal_Slot7_CallOwnDtor(void* self) {
    if (!self) return nullptr;
    void** vt = *reinterpret_cast<void***>(self);
    if (!vt || !vt[0]) return nullptr;
    return reinterpret_cast<void* (*)(void*, uint32_t)>(vt[0])(self, 0);
}

void Goal_FillInheritedVtableSlots(void** vtable, const char* who) {
    if (!vtable) return;

    // Both are ordinary virtuals with the standard MS x64 ABI, so the resolved export
    // goes straight into the slot — exactly what retail's JMP thunk amounts to.
    static void* s_compile = nullptr;
    static void* s_getInstanceType = nullptr;
    static bool  s_resolved = false;
    if (!s_resolved) {
        s_resolved = true;
        s_compile = reinterpret_cast<void*>(ResolveExport(
            "?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z"));
        s_getInstanceType = reinterpret_cast<void*>(ResolveExport(
            "?GetInstanceType@Goal@Achievement@@UEBA?AVType@Object@@XZ"));
    }

    if (!vtable[4]) vtable[4] = s_compile;
    if (!vtable[7]) vtable[7] = reinterpret_cast<void*>(&Goal_Slot7_CallOwnDtor);
    if (!vtable[9]) vtable[9] = s_getInstanceType;

    if (!s_compile || !s_getInstanceType) {
        fprintf(stderr, "[Goal] %s inherited slots UNRESOLVED (Compile=%p GetInstanceType=%p)"
                        " — GoalList::Initialize will EXEC 0""\n",
                who, s_compile, s_getInstanceType);
        fflush(stderr);
    }
}
