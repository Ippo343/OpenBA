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

#include "Tutorial/TutorialSetupRestrictPlayer.h"
#include "object/Internal.h"      // Object::detail::InternalMemAlloc / InternalMemFree
#include "Object/factory_registration.h"
#include "engine_funcs.h"
#include "Game/GameUpdateLoop.h"   // B3R STORAGE: GameUpdateLoop::_singleton
#include "system/Filename.h"
#include "system/String.h"
#include "object/External.h"   // ENDGAME B.3.3 step 6A
#include "object/TypeInfo.h"
#include "Dialog/exe_bodies.h"   // 0x140102b60 Toolbar_ShowGroup_Impl
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <new>
#include <cstdlib>
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch D
#include "Component/ComponentDescription.h"

// ============================================================
// ENDGAME B.3.8: the String::Serialize / Filename ctor+dtor dispatch-slot
// pointers this file filled lazily are gone — the Serialize member calls
// System::String::Serialize and scopes a System::Filename directly (B6-5).
// ============================================================

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

    if (!::Object::ExternalFactory::_singleton) return;
    void* factory = ::Object::ExternalFactory::_singleton;

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
    {
        System::String   normStr(normalized);
        System::Filename normFile(normStr);
        ::Object::ExternalFactory* efactory =
            reinterpret_cast<::Object::ExternalFactory*>(factory);  // B.3.3 bridge
        found = const_cast<void*>(static_cast<const void*>(efactory->FindOrLoad(normFile)));
    }

    void* result = nullptr;
    if (found) {
        if (Object::IsDerivedFromNullSafe(found, ComponentDescription::TypeId())) {
            result = found;
        }
    }
    *(void**)(r + 0x10) = result;

}


// ============================================================
// Construction / destruction (ENDGAME B.3.8 block GOAL)
// ============================================================
// ctor 0x1400e3220 (createInPlace 0x140028b80 null-checks then runs it):
// External(), the TutorialSetup and TutorialSetupRestrictPlayer stamps (the
// compiler's), then the fields.
TutorialSetupRestrictPlayer::TutorialSetupRestrictPlayer()
    : restrictMinX(-1), restrictMinChunkRow(-1),      // one qword store of -1
      restrictMaxX(-1), restrictMaxChunkCol(-1),      // one qword store of -1
      restrictToolFlags(0), _pad34{},
      toolRefString(0), toolRefStamp(-1), _pad44{}, toolRefCached(nullptr),
      cameraRefString(0), cameraRefStamp(-1), _pad5C{}, cameraRefCached(nullptr) {}

// The dtor inside [0] 0x140028bb0: ~String +0x50, ~String +0x38; the compiler
// re-stamps TutorialSetup and runs ~External.
TutorialSetupRestrictPlayer::~TutorialSetupRestrictPlayer() {
    reinterpret_cast<System::String*>(&cameraRefString)->~String();   // B.3.1 bridge
    reinterpret_cast<System::String*>(&toolRefString)->~String();     // B.3.1 bridge
}

// ============================================================
// TutorialSetupRestrictPlayer::Serialize (vtable +0x28, 0x1400e3090)
// ============================================================
// 5 int32 restriction fields and 2 ExternalRef strings; on load (serializer
// mode == 0) each ExternalRef is resolved by name.
bool TutorialSetupRestrictPlayer::Serialize(void* stream) {
    uintptr_t s = reinterpret_cast<uintptr_t>(this);

    // Create serializer on stack
    alignas(8) uint8_t serializer[32];
    ::new (static_cast<void*>(serializer)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge

    // Serialize 5 int32 fields: +0x20, +0x24, +0x28, +0x2C, +0x30
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<int*>(s + 0x20), nullptr);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<int*>(s + 0x24), nullptr);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<int*>(s + 0x28), nullptr);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<int*>(s + 0x2C), nullptr);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<int*>(s + 0x30), nullptr);   // B.3.4 bridge

    // Serialize ExternalRef1 (tool restriction) at +0x38, inside the Filename
    // stack temporary retail's compiler emitted.
    {
        System::Filename tempFilename;
        (void)tempFilename;

        reinterpret_cast<System::String*>(s + 0x38)->Serialize(serializer);   // B.3.1 bridge

        // On load: resolve external by name
        void* serializerBase = *(void**)serializer;
        if (serializerBase && static_cast<int>(reinterpret_cast<IO::Serializer*>(serializer)->GetDirection()) == 0) {   // B.3.4 bridge  (IO::Serializer::GetDirection, retail 0x180006210)
            TutRestrict_ResolveRefOrLoad((void*)(s + 0x38));
        }
    }

    // Serialize ExternalRef2 (camera restriction) at +0x50
    {
        System::Filename tempFilename;
        (void)tempFilename;

        reinterpret_cast<System::String*>(s + 0x50)->Serialize(serializer);   // B.3.1 bridge

        // On load: resolve external by name
        void* serializerBase = *(void**)serializer;
        if (serializerBase && static_cast<int>(reinterpret_cast<IO::Serializer*>(serializer)->GetDirection()) == 0) {   // B.3.4 bridge  (IO::Serializer::GetDirection, retail 0x180006210)
            TutRestrict_ResolveRefOrLoad((void*)(s + 0x50));
        }
    }

    // Destroy serializer
    reinterpret_cast<IO::Serializer*>(serializer)->~Serializer();   // B.3.4 bridge
    return true;
}

// ============================================================
// TutorialSetupRestrictPlayer::Reconstruct [6] 0x140028b90 / Destruct [7]
// ============================================================
Object::External* TutorialSetupRestrictPlayer::Reconstruct() {
    this->~TutorialSetupRestrictPlayer();
    return ::new (static_cast<void*>(this)) TutorialSetupRestrictPlayer();
}
void TutorialSetupRestrictPlayer::Destruct() { this->~TutorialSetupRestrictPlayer(); }

// ============================================================
// TutorialSetupRestrictPlayer::Begin (vtable +0x40, 0x1400e2f20)
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
void TutorialSetupRestrictPlayer::Begin() {
    uintptr_t s = reinterpret_cast<uintptr_t>(this);

    if (GameUpdateLoop::_singleton == nullptr) return;
    uintptr_t gameWorld = (uintptr_t)GameUpdateLoop::_singleton;

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
    new (localRef2) System::String(*reinterpret_cast<const System::String*>(s + 0x50));
    *(int32_t*)(localRef2 + 8) = *(int32_t*)(s + 0x58);
    *(uint64_t*)(localRef2 + 16) = *(uint64_t*)(s + 0x60);
    TutRestrict_ResolveRefOrLoad(localRef2);
    uint64_t cameraRefPtr = *(uint64_t*)(localRef2 + 16);
    reinterpret_cast<System::String*>(localRef2)->~String();

    // Copy ExternalRef1 (+0x38) into local, resolve, get cached ptr
    uint8_t localRef1[24];
    memset(localRef1, 0, sizeof(localRef1));
    new (localRef1) System::String(*reinterpret_cast<const System::String*>(s + 0x38));
    *(int32_t*)(localRef1 + 8) = *(int32_t*)(s + 0x40);
    *(uint64_t*)(localRef1 + 16) = *(uint64_t*)(s + 0x48);
    TutRestrict_ResolveRefOrLoad(localRef1);
    uint64_t toolRefPtr = *(uint64_t*)(localRef1 + 16);
    reinterpret_cast<System::String*>(localRef1)->~String();

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
// TutorialSetupRestrictPlayer::End (vtable +0x50, 0x1400e2e90)
// ============================================================
// Clears all player restrictions from the PlayerController at GameWorld+0x120.
// If toolRef has a cached External, calls SetEditMode(playerController, 0).
void TutorialSetupRestrictPlayer::End() {
    uintptr_t s = reinterpret_cast<uintptr_t>(this);

    if (GameUpdateLoop::_singleton == nullptr) return;
    uintptr_t gameWorld = (uintptr_t)GameUpdateLoop::_singleton;

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
        uintptr_t playerCtrl2 = *(uintptr_t*)(gameWorld + 0x120);
        Toolbar_ShowGroup_Impl((void*)playerCtrl2, 0);   // 0x140102b60 (the door was named SetEditMode)
    }
}

// ============================================================
// TutorialSetupRestrictPlayer::Construct (vtable +0x58, 0x140028b80)
// ============================================================
Object::External* TutorialSetupRestrictPlayer::Construct() {
    return ::new (static_cast<void*>(this)) TutorialSetupRestrictPlayer();
}

// create 0x140028b40: MemAlloc(0x68, 8), then the ctor.
void* TutorialSetupRestrictPlayer::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(TutorialSetupRestrictPlayer), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(::new (p) TutorialSetupRestrictPlayer());
}
// createInPlace 0x140028b80.
void* TutorialSetupRestrictPlayer::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(::new (storage) TutorialSetupRestrictPlayer());
}

DEFINE_EXE_TYPE(TutorialSetupRestrictPlayer, "TutorialSetup")
