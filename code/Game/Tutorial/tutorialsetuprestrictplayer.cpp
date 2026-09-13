// code/Game/Tutorial/tutorialsetuprestrictplayer.cpp
// Clean reimplementation of TutorialSetupRestrictPlayer vtable methods.
//
// TutorialSetupRestrictPlayer (0x68 bytes), vtable at 0x140160e20.
// Extends TutorialSetup (which extends Object::External).
//
// Fields (beyond External base at +0x08..+0x1F):
//   +0x20: int32 restrictMinX        — cell X offset (min bound)
//   +0x24: int32 restrictMinChunkRow — chunk row (min bound)
//   +0x28: int32 restrictMaxX        — cell X offset (max bound)
//   +0x2C: int32 restrictMaxChunkCol — chunk col (max bound)
//   +0x30: int32 restrictToolFlags   — tool restriction bitmask
//   +0x38: ExternalRef (tool restriction) — {String(8), stamp(4), pad(4), cachedPtr(8)}
//   +0x50: ExternalRef (camera restriction) — {String(8), stamp(4), pad(4), cachedPtr(8)}
//
// Slot8 (Apply) computes absolute camera positions from chunk*cellSize+offset,
// resolves ExternalRef cached pointers, writes restriction state to PlayerController.
// Slot10 (Undo) clears the restriction state back to defaults.
//
// Decompiled from Application-x64.exe using Ghidra.

#include "Tutorial/tutorialsetuprestrictplayer_funcs.h"
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cstdlib>

// ============================================================
// Local function pointers resolved once
// ============================================================

// IO::Serializer::Serialize(this, int*, name) — dispatch slot 0x140152ac8
typedef void (*fn_Serializer_SerializeInt_t)(void* serializer, void* intPtr, const char* name);
static fn_Serializer_SerializeInt_t s_fn_SerializeInt = nullptr;

// System::String::Serialize(this, serializer) — dispatch slot 0x140151238
typedef void (*fn_StringSerialize_t)(void* self, void* serializer);
static fn_StringSerialize_t s_fn_StringSerialize = nullptr;

// System::Filename::Filename(this) — dispatch slot 0x140151d40
typedef void* (*fn_FilenameCtor_t)(void* self);
static fn_FilenameCtor_t s_fn_FilenameCtor = nullptr;

// System::Filename::~Filename(this) — dispatch slot 0x1401529e8
typedef void (*fn_FilenameDtor_t)(void* self);
static fn_FilenameDtor_t s_fn_FilenameDtor = nullptr;

static bool s_funcsResolved = false;

// ============================================================
// TutRestrict_ResolveRefOrLoad — ExternalRef resolve with a load-on-miss tail
// ============================================================
// STANDALONE DEVIATION, deliberate. Retail resolves both of this class's
// ExternalRefs with ResolveExternalByName_Impl (0x14001b1d0), which is
// ExternalFactory::Find alone — find-only, no load. Retail can afford that
// because by the time a tutorial step applies, UpdateLoop::LoadResources has
// already preloaded the Game/GameResources.rsc dependency tree (which pulls in
// Template\*.rsc) INTO the factory. Our preload leaves those entries in the
// loop's m_resourceList instead, so Find() misses — see the same, already
// documented gap in TutBase_ResolveToolbarRef (tutorial_base.cpp) and in
// StartTown_ResolveTemplate (starttown.cpp).
//
// A miss here is not cosmetic: the resolved pointer is written to
// PlayerController+0x88, and Toolbar_ShowGroup_Impl / SetEditMode (0x140102b60)
// does
//     if (pc[0x80] && groupId) groupId = pc[0x88];
// so with a null there EVERY toolbar selection during a restricted tutorial step
// resolves to "no tool" — the build tool silently does nothing. The same null
// also skips the +0xa4/+0xa8/+0xac block below, and PlacementTool::Draw
// (0x1400fdcc0) section 2 — the yellow "place it here" footprint the tutorial
// text refers to — only draws while a placement tool is live. One null, both
// symptoms.
//
// Retail would fault on neither (it never has the null), so this is not a guard
// over a faithful crash: it is the standalone resource-environment gap. The real
// fix is to make the preload populate the ExternalFactory the way retail's does.
static void TutRestrict_ResolveRefOrLoad(void* ref) {
    if (!ref) return;
    ResolveExternalByName_Impl(ref);            // faithful find-only path first
    uintptr_t r = (uintptr_t)ref;
    if (*(void**)(r + 0x10)) return;            // hit — nothing to do

    if (!g_pExternalFactorySingleton || !*g_pExternalFactorySingleton) return;
    if (!fn_ExternalFactory_FindOrLoad || !fn_Filename_CtorString || !fn_Filename_Dtor) return;
    void* factory = *g_pExternalFactorySingleton;

    const char* name = *(const char* const*)r;
    if (!name || !name[0]) return;

    // Only the LOADER rejects backslashes; the factory's key comparison
    // normalizes separators (measured — see tutorial_base.cpp).
    char normalized[260];
    size_t k = 0;
    for (; name[k] && k < sizeof(normalized) - 1; ++k)
        normalized[k] = (name[k] == '\\') ? '/' : name[k];
    normalized[k] = 0;

    void* found = nullptr;
    if (fn_String_Ctor_CStr && fn_String_Dtor) {
        uint8_t normStr[8] = {};
        fn_String_Ctor_CStr(normStr, normalized);
        uint8_t normFile[8] = {};
        fn_Filename_CtorString(normFile, normStr);
        found = fn_ExternalFactory_FindOrLoad(factory, normFile);
        fn_Filename_Dtor(normFile);
        fn_String_Dtor(normStr);
    }

    void* result = nullptr;
    if (found) {
        if (!g_TypeId_ComponentDescription_FC ||
            !fn_IsDerivedFrom ||
            fn_IsDerivedFrom(found, *g_TypeId_ComponentDescription_FC)) {
            result = found;
        }
    }
    *(void**)(r + 0x10) = result;

}


static void EnsureFuncsResolved() {
    if (s_funcsResolved) return;
    s_funcsResolved = true;

    auto loadExref = [](uintptr_t addr) -> void* {
        void** slot = reinterpret_cast<void**>(ResolveAddr(Module::Exe, addr));
        return (slot && *slot) ? *slot : nullptr;
    };

    s_fn_SerializeInt    = (fn_Serializer_SerializeInt_t) loadExref(0x140152ac8);
    s_fn_StringSerialize = (fn_StringSerialize_t)         loadExref(0x140151238);
    s_fn_FilenameCtor    = (fn_FilenameCtor_t)            loadExref(0x140151d40);
    s_fn_FilenameDtor    = (fn_FilenameDtor_t)            loadExref(0x1401529e8);

#ifdef _DEBUG
    printf("[TutorialSetupRestrictPlayer] SerializeInt=%p StringSerialize=%p\n",
           (void*)s_fn_SerializeInt, (void*)s_fn_StringSerialize);
#endif
}

// ============================================================
// TutorialSetupRestrictPlayer::Dtor (vtable +0x00, 0x140028bb0)
// ============================================================
// Scalar deleting destructor.
// 1. Set vtable to TutorialSetupRestrictPlayer
// 2. Destroy inline String at +0x50 (cameraRef name)
// 3. Destroy inline String at +0x38 (toolRef name)
// 4. Set vtable to TutorialSetup (base)
// 5. Call External::~External(self)
// 6. If (flags & 1), free memory
void* TutorialSetupRestrictPlayer_Dtor(void* self, uint32_t flags) {
    uintptr_t s = (uintptr_t)self;

    // Set vtable to TutorialSetupRestrictPlayer
    uintptr_t restrictVtable = ResolveAddr(Module::Exe, TutorialSetupRestrictPlayerAddr::Vtable);
    *(uintptr_t*)s = restrictVtable;

    // Destroy inline String at +0x50 (camera ref name)
    if (fn_String_Dtor) fn_String_Dtor((void*)(s + 0x50));

    // Destroy inline String at +0x38 (tool ref name)
    if (fn_String_Dtor) fn_String_Dtor((void*)(s + 0x38));

    // Set vtable to TutorialSetup base
    uintptr_t setupVtable = ResolveAddr(Module::Exe, TutorialSetupAddr::Vtable);
    *(uintptr_t*)s = setupVtable;

    // Call External::~External
    if (fn_External_Dtor) fn_External_Dtor(self);

    // Conditional free
    if ((flags & 1) != 0) {
        if (fn_MemFreeStatic) fn_MemFreeStatic(self);
    }

    return self;
}

// ============================================================
// TutorialSetupRestrictPlayer::Serialize (vtable +0x28, 0x1400e3090)
// ============================================================
// Serializes 5 int32 restriction fields and 2 ExternalRef strings.
// On load (serializer mode == 0), resolves each ExternalRef by name.
void TutorialSetupRestrictPlayer_Serialize(void* self, void* stream) {
    EnsureFuncsResolved();
    uintptr_t s = (uintptr_t)self;

    // Create serializer on stack
    uint8_t serializer[32];
    if (g_fn_Serializer_Ctor)
        g_fn_Serializer_Ctor(serializer, stream, 0);

    // Serialize 5 int32 fields: +0x20, +0x24, +0x28, +0x2C, +0x30
    if (s_fn_SerializeInt) {
        s_fn_SerializeInt(serializer, (void*)(s + 0x20), nullptr);
        s_fn_SerializeInt(serializer, (void*)(s + 0x24), nullptr);
        s_fn_SerializeInt(serializer, (void*)(s + 0x28), nullptr);
        s_fn_SerializeInt(serializer, (void*)(s + 0x2C), nullptr);
        s_fn_SerializeInt(serializer, (void*)(s + 0x30), nullptr);
    }

    // Serialize ExternalRef1 (tool restriction) at +0x38
    {
        uint8_t tempFilename[8];
        if (s_fn_FilenameCtor) s_fn_FilenameCtor(tempFilename);

        if (s_fn_StringSerialize) s_fn_StringSerialize((void*)(s + 0x38), serializer);

        // On load: resolve external by name
        void* serializerBase = *(void**)serializer;
        if (serializerBase && *(int32_t*)((uintptr_t)serializerBase + 0x8) == 0) {
            TutRestrict_ResolveRefOrLoad((void*)(s + 0x38));
        }

        if (s_fn_FilenameDtor) s_fn_FilenameDtor(tempFilename);
    }

    // Serialize ExternalRef2 (camera restriction) at +0x50
    {
        uint8_t tempFilename[8];
        if (s_fn_FilenameCtor) s_fn_FilenameCtor(tempFilename);

        if (s_fn_StringSerialize) s_fn_StringSerialize((void*)(s + 0x50), serializer);

        // On load: resolve external by name
        void* serializerBase = *(void**)serializer;
        if (serializerBase && *(int32_t*)((uintptr_t)serializerBase + 0x8) == 0) {
            TutRestrict_ResolveRefOrLoad((void*)(s + 0x50));
        }

        if (s_fn_FilenameDtor) s_fn_FilenameDtor(tempFilename);
    }

    // Destroy serializer
    if (g_fn_Serializer_Dtor)
        g_fn_Serializer_Dtor(serializer);
}

// ============================================================
// TutorialSetupRestrictPlayer::Cleanup (vtable +0x30, 0x140028b90)
// ============================================================
// Destroys and reinitializes: calls Dtor(flags=0) via vtable, then Ctor.
void TutorialSetupRestrictPlayer_Cleanup(void* self) {
    // Call Dtor via vtable with flags=0 (no free)
    uintptr_t vtable = *(uintptr_t*)self;
    typedef void* (*fn_Dtor_t)(void* self, uint32_t flags);
    fn_Dtor_t dtorFn = *reinterpret_cast<fn_Dtor_t*>(vtable);
    if (dtorFn) dtorFn(self, 0);

    // Re-construct
    TutorialSetupRestrictPlayer_Ctor(self);
}

// ============================================================
// TutorialSetupRestrictPlayer::Slot8 / Apply (vtable +0x40, 0x1400e2f20)
// ============================================================
// Applies player restrictions to the PlayerController at GameWorld+0x120.
//
// Flow:
// 1. Compute absolute min/max positions: chunkRow * cellSize + cellX
// 2. Copy ExternalRef2 (+0x50) into a local, resolve it, get cached ptr
// 3. Copy ExternalRef1 (+0x38) into a local, resolve it, get cached ptr
// 4. Write restriction state to PlayerController (+0x80..+0xA0)
// 5. Resolve original toolRef (+0x38) and if it has a cached ptr,
//    also set camera restriction bounds (+0xA4..+0xAC)
void TutorialSetupRestrictPlayer_Slot8(void* self) {
    uintptr_t s = (uintptr_t)self;

    if (!g_pGameWorldSingleton || !*g_pGameWorldSingleton) return;
    uintptr_t gameWorld = (uintptr_t)*g_pGameWorldSingleton;

    // Get map cell size from WorldGrid at +0xF8
    int32_t** gridPtr = reinterpret_cast<int32_t**>(gameWorld + 0xF8);
    if (!gridPtr || !*gridPtr) return;
    int32_t cellSize = **gridPtr;

    // Compute absolute positions
    int32_t absMin = *(int32_t*)(s + 0x24) * cellSize + *(int32_t*)(s + 0x20);
    int32_t absMax = *(int32_t*)(s + 0x2C) * cellSize + *(int32_t*)(s + 0x28);

    // Copy ExternalRef2 (+0x50) into local, resolve, get cached ptr
    // Local layout: String(8) + stamp(4) + pad(4) + ptr(8) = 24 bytes
    uint8_t localRef2[24];
    memset(localRef2, 0, sizeof(localRef2));
    if (fn_String_Ctor_Copy) fn_String_Ctor_Copy(localRef2, (void*)(s + 0x50));
    *(int32_t*)(localRef2 + 8) = *(int32_t*)(s + 0x58);
    *(uint64_t*)(localRef2 + 16) = *(uint64_t*)(s + 0x60);
    TutRestrict_ResolveRefOrLoad(localRef2);
    uint64_t cameraRefPtr = *(uint64_t*)(localRef2 + 16);
    if (fn_String_Dtor) fn_String_Dtor(localRef2);

    // Copy ExternalRef1 (+0x38) into local, resolve, get cached ptr
    uint8_t localRef1[24];
    memset(localRef1, 0, sizeof(localRef1));
    if (fn_String_Ctor_Copy) fn_String_Ctor_Copy(localRef1, (void*)(s + 0x38));
    *(int32_t*)(localRef1 + 8) = *(int32_t*)(s + 0x40);
    *(uint64_t*)(localRef1 + 16) = *(uint64_t*)(s + 0x48);
    TutRestrict_ResolveRefOrLoad(localRef1);
    uint64_t toolRefPtr = *(uint64_t*)(localRef1 + 16);
    if (fn_String_Dtor) fn_String_Dtor(localRef1);

    // Write restriction state to PlayerController at GameWorld+0x120
    uintptr_t playerCtrl = *(uintptr_t*)(gameWorld + 0x120);
    if (!playerCtrl) return;

    int32_t toolFlags = *(int32_t*)(s + 0x30);

    *(uint8_t*)(playerCtrl + 0x80) = 1;            // restrictions active
    *(uint64_t*)(playerCtrl + 0x88) = toolRefPtr;   // resolved tool description
    *(uint64_t*)(playerCtrl + 0x90) = cameraRefPtr; // resolved camera description
    *(int32_t*)(playerCtrl + 0x98) = absMin;        // min bound
    *(int32_t*)(playerCtrl + 0x9C) = absMax;        // max bound
    *(int32_t*)(playerCtrl + 0xA0) = toolFlags;     // tool restriction flags

    // Resolve toolRef on original (+0x38) and if it has a cached External ptr,
    // also set camera restriction bounds
    TutRestrict_ResolveRefOrLoad((void*)(s + 0x38));
    if (*(uint64_t*)(s + 0x48) != 0) {
        uintptr_t playerCtrl2 = *(uintptr_t*)(gameWorld + 0x120);
        *(int32_t*)(playerCtrl2 + 0xA8) = absMin;
        *(int32_t*)(playerCtrl2 + 0xAC) = absMax;
        *(uint8_t*)(playerCtrl2 + 0xA4) = 1;
    }

}

// ============================================================
// TutorialSetupRestrictPlayer::Slot10 / Undo (vtable +0x50, 0x1400e2e90)
// ============================================================
// Clears all player restrictions from the PlayerController at GameWorld+0x120.
// Resets restriction fields to defaults (0/-1).
// If toolRef has a cached External, calls SetEditMode(playerController, 0).
void TutorialSetupRestrictPlayer_Slot10(void* self) {
    uintptr_t s = (uintptr_t)self;

    if (!g_pGameWorldSingleton || !*g_pGameWorldSingleton) return;
    uintptr_t gameWorld = (uintptr_t)*g_pGameWorldSingleton;

    uintptr_t playerCtrl = *(uintptr_t*)(gameWorld + 0x120);
    if (!playerCtrl) return;

    // Clear restriction state
    *(uint8_t*)(playerCtrl + 0x80) = 0;
    *(uint64_t*)(playerCtrl + 0x88) = 0;
    *(uint64_t*)(playerCtrl + 0x90) = 0;
    *(uint64_t*)(playerCtrl + 0x98) = 0xFFFFFFFFFFFFFFFFULL;  // -1 (both min/max int32s)
    *(int32_t*)(playerCtrl + 0xA0) = -1;                      // tool flags cleared

    // Resolve toolRef on original (+0x38) and if it has a cached External,
    // call SetEditMode(playerController, 0) to clear tool selection
    TutRestrict_ResolveRefOrLoad((void*)(s + 0x38));
    if (*(uint64_t*)(s + 0x48) != 0) {
        if (fn_SetEditMode) {
            uintptr_t playerCtrl2 = *(uintptr_t*)(gameWorld + 0x120);
            fn_SetEditMode((void*)playerCtrl2, 0);
        }
    }
}

// ============================================================
// TutorialSetupRestrictPlayer::Ctor (vtable +0x58, 0x140028b80 -> 0x1400e3220)
// ============================================================
// Construct from raw memory. Null-check, call External::External,
// set vtable chain, init all fields to defaults.
void* TutorialSetupRestrictPlayer_Ctor(void* self) {
    if (!self) return nullptr;

    uintptr_t s = (uintptr_t)self;

    // Init External base
    if (fn_External_External) fn_External_External(self);

    // Set vtable chain: TutorialSetup then TutorialSetupRestrictPlayer
    uintptr_t setupVtable = ResolveAddr(Module::Exe, TutorialSetupAddr::Vtable);
    *(uintptr_t*)s = setupVtable;

    uintptr_t restrictVtable = ResolveAddr(Module::Exe, TutorialSetupRestrictPlayerAddr::Vtable);
    *(uintptr_t*)s = restrictVtable;

    // Init restriction fields
    *(int64_t*)(s + 0x20) = -1LL;     // restrictMinX + restrictMinChunkRow = -1, -1
    *(int64_t*)(s + 0x28) = -1LL;     // restrictMaxX + restrictMaxChunkCol = -1, -1
    *(int32_t*)(s + 0x30) = 0;        // restrictToolFlags

    // Init ExternalRef1 (tool restriction) at +0x38
    *(uint64_t*)(s + 0x38) = 0;       // String data ptr = null
    *(int32_t*)(s + 0x40) = -1;       // generation stamp = -1 (force re-resolve)
    *(uint64_t*)(s + 0x48) = 0;       // cached External ptr = null

    // Init ExternalRef2 (camera restriction) at +0x50
    *(uint64_t*)(s + 0x50) = 0;       // String data ptr = null
    *(int32_t*)(s + 0x58) = -1;       // generation stamp = -1 (force re-resolve)
    *(uint64_t*)(s + 0x60) = 0;       // cached External ptr = null

    return self;
}

// ============================================================
// Standalone factory + vtable — External-resource family (factory cascade).
// Vtable @ 0x140160e20 (9 slots, Object::External base), size 0x68, parent "TutorialSetup".
// GetFileVersion (+0x18) = 4. MusicList pattern (musiclist.cpp).
// ============================================================
#include "Object/factory_registration.h"
#include "address_registry.h"

static uint32_t TutorialSetupRestrictPlayer_GetTypeId(void*) {
    static uint32_t c = 0;
    if (!c) c = ::Object::GetTypeIdByName("TutorialSetupRestrictPlayer");
    return c;
}
static int TutorialSetupRestrictPlayer_GetFileVersion(void*) { return 4; }

// Retail slots 9/10 on the TutorialSetup family are empty `ret` bodies.
static void TutorialSetup_SlotEmpty(void*) {}

static void* g_TutorialSetupRestrictPlayer_Vtable[12] = {
    (void*)&TutorialSetupRestrictPlayer_Dtor,            // +0x00 Dtor
    nullptr,                      // +0x08 (base thunk)
    (void*)&TutorialSetupRestrictPlayer_GetTypeId,       // +0x10 GetTypeId
    (void*)&TutorialSetupRestrictPlayer_GetFileVersion,  // +0x18 GetFileVersion
    nullptr,                      // +0x20 (shared base thunk 0x14003a912)
    (void*)&TutorialSetupRestrictPlayer_Serialize,       // +0x28 Serialize
    (void*)&TutorialSetupRestrictPlayer_Cleanup,         // +0x30 Cleanup
    nullptr,                      // +0x38 (base 0x140001640)
    (void*)&TutorialSetupRestrictPlayer_Slot8,   // +0x40 Apply (0x1400e2f20)
    (void*)&TutorialSetup_SlotEmpty,   // +0x48 Update (0x1400e2f10 - empty in retail)
    (void*)&TutorialSetupRestrictPlayer_Slot10,   // +0x50 Undo (0x1400e2e90)
    (void*)&TutorialSetupRestrictPlayer_Ctor,   // +0x58 Ctor/CreateInPlace (0x140028b80)
};

extern "C" void RegisterTutorialSetupRestrictPlayerVtable() {
    RegisterExeAddr(0x140160e20, (uintptr_t)g_TutorialSetupRestrictPlayer_Vtable);
}
extern "C" void* TutorialSetupRestrictPlayer_CreateInPlace(void* mem) {
    return TutorialSetupRestrictPlayer_Ctor(mem);
}
extern "C" void* TutorialSetupRestrictPlayer_Factory(void) {
    if (!fn_MemAlloc || !g_pMemAllocSingleton) return nullptr;
    void* p = fn_MemAlloc(g_pMemAllocSingleton, 0x68, 8);
    return p ? TutorialSetupRestrictPlayer_Ctor(p) : nullptr;
}
REGISTER_TYPE(TutorialSetupRestrictPlayer, "TutorialSetup", &TutorialSetupRestrictPlayer_Factory, &TutorialSetupRestrictPlayer_CreateInPlace);
