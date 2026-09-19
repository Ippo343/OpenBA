// code/Game/Component/multiobjectcomponent.cpp
// Clean reimplementation of MultiObjectComponent lifecycle methods from Application-x64.exe.
//
// MultiObjectComponent (0x60 bytes) extends ControllerComponent.
// Manages multi-part objects (multi-tile buildings spanning several entities).
//
// Vtable at 0x140163cf0. Two non-trivial overrides:
//   Shutdown  (0x140125880) — Serialize entity ref array + multiCount
//   OnCreate  (0x1401257C0) — Mark child entities for removal
//

#include "Component/multiobjectcomponent_funcs.h"
#include "Component/coll_bridge.h"
#include "engine_funcs.h"
#include "resolve.h"
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch A
#include "io/Stream.h"      // ENDGAME B.3.4 callers batch H: IO::Stream::GetDirection
#include <cstdint>
#include <cstdio>
#include <new>
#include "object/Internal.h"   // ENDGAME B.3.4 caller batch G

// ============================================================
// Resolved helpers
// ============================================================

typedef void (*fn_Component_Serialize_t)(void* self, void* stream);
static fn_Component_Serialize_t s_fn_Component_Serialize = nullptr;

// IsDerivedFrom — dispatched
typedef bool (*fn_IsDerivedFrom_dispatch_t)(void* obj, uint32_t typeId);
static fn_IsDerivedFrom_dispatch_t s_fn_IsDerivedFrom_d = nullptr;

// FUN_1400c2ea0 — mark entity for removal
typedef void (*fn_MarkForRemoval_t)(void* entity);
static fn_MarkForRemoval_t s_fn_MarkForRemoval = nullptr;

static void** s_pGameState = nullptr;
static uint32_t* s_pComponentEntityTypeId = nullptr;  // TypeInfo_ComponentEntity

static bool s_initialized = false;

// ============================================================
// MultiObjectComponent::Shutdown (0x140125880)
// ============================================================
// Component_Serialize + empty serializer block + second serializer:
//   serialize count from +0x50, resize array if loading, serialize each IBP
//   with IsDerivedFrom validation, serialize multiCount at +0x58.
//   After loading, remove null entries from array (compact).

void MultiObjectComponent_Shutdown(MultiObjectComponent* self, void* stream) {
    MultiObjectComponent_InitFuncs();
    if (s_fn_Component_Serialize)
        s_fn_Component_Serialize(self, stream);

    // First empty serializer block (controller section)
    alignas(8) uint8_t serializer1[24];
    ::new (static_cast<void*>(serializer1)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(serializer1)->~Serializer();   // B.3.4 bridge

    // Second serializer block with entity array + multiCount
    alignas(8) uint8_t serializer2[24];
    ::new (static_cast<void*>(serializer2)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge

    int localCount = *(int*)((uintptr_t)self + 0x50);
    reinterpret_cast<IO::Serializer*>(serializer2)->Serialize(localCount, nullptr);   // B.3.4 bridge

    // If loading (stream+8 == 0), resize array
    // B.3.2 bridge: FUN_1400032e0 is Collection::Array<void*>::SetSize(n)
    if (static_cast<int>(reinterpret_cast<IO::Stream*>(stream)->GetDirection()) == 0) {   // B.3.4 bridge  (IO::Stream::GetDirection, retail 0x180006200)
        GameColl::AsArray<GameColl::Ptr8>((void*)((uintptr_t)self + 0x48))
            .SetSize(localCount);
    }

    // Serialize each InternalBasePtr in the array
    void* arrayBase = *(void**)((uintptr_t)self + 0x48);
    if (localCount > 0) {
        for (int i = 0; i < localCount; i++) {
            void* ibp = (void*)((uintptr_t)arrayBase + (uintptr_t)i * 8);
            reinterpret_cast<Object::InternalBasePtr*>(ibp)->Serialize(*reinterpret_cast<IO::Serializer*>(serializer2));   // B.3.4 bridge

            // Validate: if not ComponentEntity, null it out
            void* ptr = *(void**)ibp;
            if (ptr != nullptr && s_fn_IsDerivedFrom_d && s_pComponentEntityTypeId) {
                if (!s_fn_IsDerivedFrom_d(ptr, *s_pComponentEntityTypeId)) {
                    *(uintptr_t*)ibp = 0;
                }
            }
        }
    }

    // Serialize multiCount at +0x58
    reinterpret_cast<IO::Serializer*>(serializer2)->Serialize(*reinterpret_cast<int*>((uintptr_t)self + 0x58), nullptr);   // B.3.4 bridge

    // On load: compact array — remove null entries
    if (static_cast<int>(reinterpret_cast<IO::Stream*>(stream)->GetDirection()) == 0) {   // B.3.4 bridge  (IO::Stream::GetDirection, retail 0x180006200)
        int count = *(int*)((uintptr_t)self + 0x50);
        for (int i = 0; i < count; /* no increment */) {
            void* arr = *(void**)((uintptr_t)self + 0x48);
            uintptr_t entryAddr = (uintptr_t)arr + (uintptr_t)i * 8;
            if (*(uintptr_t*)entryAddr == 0) {
                // Shift remaining entries down
                for (int j = i; j < count - 1; j++) {
                    uintptr_t curAddr = (uintptr_t)arr + (uintptr_t)j * 8;
                    uintptr_t nextAddr = curAddr + 8;
                    // Clear then copy
                    *(uintptr_t*)curAddr = 0;
                    *(uintptr_t*)curAddr = *(uintptr_t*)nextAddr;
                }
                count--;
                *(int*)((uintptr_t)self + 0x50) = count;
                // Don't increment i, re-check this slot
            } else {
                i++;
            }
        }
    }

    reinterpret_cast<IO::Serializer*>(serializer2)->~Serializer();   // B.3.4 bridge
}

// ============================================================
// MultiObjectComponent::OnCreate (0x1401257C0)
// ============================================================
// Iterates child entity array. For each valid child, checks bit 10 of
// entity flags. If not already marked, calls MarkForRemoval and adds
// to removal queue.

void MultiObjectComponent_OnCreate(MultiObjectComponent* self) {
    int count = *(int*)((uintptr_t)self + 0x50);
    void** array = *(void***)((uintptr_t)self + 0x48);

    if (!s_pGameState || !*s_pGameState) return;

    for (int i = 0; i < count; i++) {
        void* entityPtr = array[i];
        void* gameWorld = *(void**)((uintptr_t)*s_pGameState + 0xE8);

        if (entityPtr != nullptr) {

            uint32_t entityFlags = *(uint32_t*)((uintptr_t)entityPtr + 0x40);
            if (((entityFlags >> 10) & 1) == 0) {
                if (s_fn_MarkForRemoval) s_fn_MarkForRemoval(entityPtr);

                if (gameWorld) {
                    int removalCount = *(int*)((uintptr_t)gameWorld + 0xD8);
                    // B.3.2 bridge: FUN_140003110 is Collection::Array<void*>::SetSize(n, fill)
                    GameColl::AsArray<GameColl::Ptr8>((void*)((uintptr_t)gameWorld + 0xD0))
                        .SetSizeFill(removalCount + 1, &array[i]);
                }
            }
        }
    }
}

// ============================================================
// Init: resolve function pointers
// ============================================================
void MultiObjectComponent_InitFuncs() {
    if (s_initialized) return;

    s_fn_Component_Serialize = reinterpret_cast<fn_Component_Serialize_t>(
        ResolveAddr(Module::Exe, 0x1401189b0));
    s_fn_MarkForRemoval = reinterpret_cast<fn_MarkForRemoval_t>(
        ResolveAddr(Module::Exe, 0x1400c2ea0));
    s_pGameState = reinterpret_cast<void**>(
        ResolveAddr(Module::Exe, 0x1401c3dd8));

    uintptr_t slot;
    slot = ResolveAddr(Module::Exe, 0x140152a98);
    if (slot) s_fn_IsDerivedFrom_d = *reinterpret_cast<fn_IsDerivedFrom_dispatch_t*>(slot);

    // TypeInfo_ComponentEntity
    s_pComponentEntityTypeId = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c72e8));

    s_initialized = true;
}

// ===== Standalone factory (Foundation D — component instantiation) =====
// size 0x60 (MultiObjectComponent_Factory MemAlloc), flags 0x82 (Ctor
// 0x140125a70: Component base sets +0x20=0x80, leaf OR 0x2 => 0x82).
#include "Object/factory_registration.h"
#include "Component/component_factory_base.h"

// -- slot 12 (+0x60) -- retail 0x140125490 -- the child-entity MESSAGE FORWARDER -----
// Pure relay, no message filter: for every child entity in the array at +0x48 (count at
// +0x50), dispatch slot 12 on EVERY component that child carries, with the same msgId.
//
// This is how a multi-part structure's parts learn that the structure finished. It was
// inert, and that is why a completed Tunnel had no terrain deformation: Tunnel.rsc's own
// `TerraformDescription terraform {}` block is EMPTY -- the floor/ceiling picking meshes
// live on Template/TunnelStart.rsc, i.e. on the two MOUTH SEGMENTS, not on the master.
// BuildComponent state 8 broadcasts msg 4 to the master only (0x1400c2b70 walks ONE
// entity), so without this forwarder the segments that actually own the meshes were never
// told, and their TerraformComponent slot 12 never ran.
//
// Measured 2026-08-22: with terraform live but this still inert, the trace showed exactly
// one "msg 4 -> deform" (the master), both mesh passes correctly skipped on a null mesh,
// and FootprintPass converting 18 cells -- master-level work only, no carve.
extern "C" void MultiObjectComponent_Slot12(void* selfV, int msgId) {
    MultiObjectComponent_InitFuncs();
    const uintptr_t self = reinterpret_cast<uintptr_t>(selfV);

    const int childCount = *reinterpret_cast<int*>(self + 0x50);
    const uintptr_t children = *reinterpret_cast<uintptr_t*>(self + 0x48);
    if (childCount <= 0 || !children) return;

    for (int i = 0; i < childCount; ++i) {
        void** childSlot = reinterpret_cast<void**>(children + static_cast<uintptr_t>(i) * 8);
        uintptr_t child = reinterpret_cast<uintptr_t>(*childSlot);
        if (!child) continue;

        const int compCount = *reinterpret_cast<int*>(child + 0x38);
        const uintptr_t comps = *reinterpret_cast<uintptr_t*>(child + 0x30);
        for (int k = 0; k < compCount && comps; ++k) {
            void* comp = *reinterpret_cast<void**>(comps + static_cast<uintptr_t>(k) * 8);
            if (!comp) continue;
            typedef void (*MsgSlot_t)(void*, int);
            MsgSlot_t fn = *reinterpret_cast<MsgSlot_t*>(
                *reinterpret_cast<uintptr_t*>(comp) + 0x60);
            if (fn) fn(comp, msgId);
        }
    }
}

static uint32_t MultiObjectComponent_LiveGetTypeId(void*) {
    static uint32_t cached = 0;
    if (cached == 0) cached = ::Object::GetTypeIdByName("MultiObjectComponent");
    return cached;
}
static void* MultiObjectComponent_LiveVtable[::ComponentFactory::kInertVtableSlots];

static void MultiObjectComponent_ShutdownVSlot(void* self, void* stream) {
    MultiObjectComponent_Shutdown(reinterpret_cast<MultiObjectComponent*>(self), stream);
}

// Re-run per instance: InitInertTyped re-stubs the shared vtable on every spawn.
static void MultiObjectComponent_InstallLive() {
    // Slot 3 (stream Shutdown/Serialize, retail 0x140125880) wired UNCONDITIONALLY,
    // save/load wave 2026-08-28 — previously the class fell through to base-only
    // Component_SerializeReimpl and the child-entity array (+0x48/+0x50/+0x58)
    // silently reset on load.
    MultiObjectComponent_LiveVtable[3] =
        reinterpret_cast<void*>(&MultiObjectComponent_ShutdownVSlot);
    MultiObjectComponent_LiveVtable[12] = reinterpret_cast<void*>(&MultiObjectComponent_Slot12);
}

extern "C" void* MultiObjectComponent_Factory(void) {
    void* p = ::ComponentFactory::MakeInertTyped(0x60, 0x82,
        MultiObjectComponent_LiveVtable,
        reinterpret_cast<void*>(&MultiObjectComponent_LiveGetTypeId));
    MultiObjectComponent_InstallLive();
    return p;
}
static void* MultiObjectComponent_CreateInPlace(void* mem) {
    if (mem) ::ComponentFactory::InitInertTyped(mem, 0x60, 0x82,
        MultiObjectComponent_LiveVtable,
        reinterpret_cast<void*>(&MultiObjectComponent_LiveGetTypeId));
    MultiObjectComponent_InstallLive();
    return mem;
}
REGISTER_TYPE(MultiObjectComponent, "ControllerComponent",
              &MultiObjectComponent_Factory, &MultiObjectComponent_CreateInPlace);
