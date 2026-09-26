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

// ===== ENDGAME B.3.8 MI-B — MultiObjectComponent's own slot bodies =====
// The class (MultiObjectComponent.h) reaches the slot-12 body below through its
// OnEvent member, at the bottom of this file.
#include "Object/factory_registration.h"
#include "Component/controller_state_table.h"

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

// ============================================================================
// ENDGAME B.3.8 MI-B — MultiObjectComponent as a REAL class with its retail
// controller member (MultiObjectComponent.h; B38_RULES D1, D2, D6, D9 #8).
//
// Before: a hand-built 48-slot table (slots 3/12 patched per create,
// Inert_ScalarDtor in slot 0) and NO controller: slots 16/17 were inert stubs, so
// the entity's StateMachineComponent never linked this node. After: the compiler's
// 18-slot vtable, a real ctor/dtor chain, and retail's member at +0x28 over retail's
// state table.
// ============================================================================
extern void MapComponent_sub_41b0(int64_t mapComponent);   // keystone_a_map.cpp, 0x1401241b0
extern void FloorComponent_SubF390(void* floorComponent);  // floorcomponent.cpp, 0x14011f390

using MultiObjectCtrlDispatch = ::ControllerLive::Dispatcher<MultiObjectCtrlTag>;

// The first component of `entity` whose slot-2 type id is `tid`, kept only if it
// IsDerivedFrom `tid` — retail's inlined FindComponent<T> shape (the scan breaks at
// the FIRST id match, then re-validates). Component array at entity+0x30, count +0x38.
static void* MultiObject_FindComponent(uintptr_t entity, uint32_t tid) {
    const int n = *reinterpret_cast<int*>(entity + 0x38);
    const uintptr_t comps = *reinterpret_cast<uintptr_t*>(entity + 0x30);
    for (int i = 0; i < n; ++i) {
        void* c = *reinterpret_cast<void**>(comps + static_cast<uintptr_t>(i) * 8);
        typedef uint32_t (*GetTypeId_t)(void*);
        const uint32_t id = reinterpret_cast<GetTypeId_t>(
            *reinterpret_cast<uintptr_t*>(*reinterpret_cast<uintptr_t*>(c) + 0x10))(c);
        if (id == tid)
            return (fn_IsDerivedFrom && fn_IsDerivedFrom(c, tid)) ? c : nullptr;
    }
    return nullptr;
}

// state-2 ENTER — retail 0x140125560. For every child entity in the list at +0x48
// (count +0x50, re-read each pass):
//   map   = FindComponent<MapComponent>(child)      (type 0x1401c79e0)
//   MapComponent_sub_41b0(map)                      (0x1401241b0 — stamp the child's
//                                                    footprint; guarded by its own
//                                                    +0x20 bit 20, so idempotent)
//   floor = FindComponent<FloorComponent>(child)    (type 0x1401c76d0)
//   if (floor) FloorComponent_sub_f390(floor)       (0x14011f390)
// then `XOR AL,AL`. Retail derefs the MapComponent unguarded: every child of a
// multi-object structure carries one (a faithful crash if not, not a guard here).
static char MultiObject_State2Enter(void* data, void* /*sm*/) {
    const uintptr_t self = reinterpret_cast<uintptr_t>(data);
    const uint32_t* pTidMap   = reinterpret_cast<const uint32_t*>(ResolveAddr(Module::Exe, 0x1401c79e0));
    const uint32_t* pTidFloor = reinterpret_cast<const uint32_t*>(ResolveAddr(Module::Exe, 0x1401c76d0));
    if (!pTidMap || !pTidFloor) return 0;   // the registry is not up (never after boot)
    for (int i = 0; i < *reinterpret_cast<int*>(self + 0x50); ++i) {
        const uintptr_t child = *reinterpret_cast<uintptr_t*>(
            *reinterpret_cast<uintptr_t*>(self + 0x48) + static_cast<uintptr_t>(i) * 8);
        void* map = MultiObject_FindComponent(child, *pTidMap);
        MapComponent_sub_41b0(reinterpret_cast<int64_t>(map));
        void* floor = MultiObject_FindComponent(child, *pTidFloor);
        if (floor) FloorComponent_SubF390(floor);
    }
    return 0;
}

// Retail's table, transcribed from MultiObjectDescription_Setup (0x14014f530): its
// RegisterState (0x1400360f0) registers state 2 with enter 0x140125560 and every other
// column zeroed; its AddTransition (0x140036210) appends 3 with a null handler. State 3
// is a transition target only. Built ONCE at static-init time (B38 D6/D7).
static void MultiObjectComponent_PopulateStateTable() {
    using namespace ::ControllerLive;
    StateTable& t = MultiObjectCtrlDispatch::table;
    if (t.count != 0) return;                      // idempotent
    TableRegisterState(t, 2, reinterpret_cast<void*>(&MultiObject_State2Enter),
                       nullptr, nullptr, nullptr);
    TableAddTransition(t, 3, nullptr);
}
static const bool s_multiObjectStateTableBuilt = (MultiObjectComponent_PopulateStateTable(), true);

// 0x140125a70. Component() is the CALL 0x1401188b0; the stamps are the compiler's;
// `_controller(this)` is the member's back-pointer (+0x40); +0x48 = +0x50 = 0;
// +0x58 = 1; then `_flags |= 2` (0x82).
// (+0x58 is the cell count the bridge/fence tools stamp; the tree's factory zeroed it.)
MultiObjectComponent::MultiObjectComponent()
    : _controller(this), field48(nullptr), field50(nullptr), multiCount(1), _pad5C{} {
    OrFlagsInCtor(0x2);
}

// [0] 0x1400360c0: ~MultiObjectComponent (0x140125a00), MemFree on &1.
Object::Base* MultiObjectComponent::DeletingDtor(unsigned flags) {
    this->~MultiObjectComponent();
    if (flags & 1) Object::detail::InternalMemFree(this);
    return this;
}
// 0x140125a00: MemFree(+0x48), +0x48 = +0x50 = 0, then the restamps and ~Internal
// (the compiler chain).
// PORT GAP (retail 0x140125a24 MemFree of the child list) — B38 D9: the tree's slot 0
// (Inert_ScalarDtor) never freed it; kept that way here.
MultiObjectComponent::~MultiObjectComponent() {}

// create 0x140035ef0: MemAlloc(0x60, 8), then the ctor.
void* MultiObjectComponent::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(MultiObjectComponent), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) MultiObjectComponent());
}
// Retail's createInPlace is `Internal::CreateInPlaceObject` (returns null); the
// tree's in-place entry built the object, and that is kept (B38 B8-13).
void* MultiObjectComponent::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) MultiObjectComponent());
}

// [3] 0x140125880 — the child-entity list (+0x48/+0x50/+0x58); resolves at its top.
void MultiObjectComponent::Persist(void* stream) { MultiObjectComponent_Shutdown(this, stream); }
// [12] 0x140125490 — relay the event to every child's components (the body above,
// also registered by address from standalone/reg_terraform_live.cpp).
void MultiObjectComponent::OnEvent(int eventId) { MultiObjectComponent_Slot12(this, eventId); }

DEFINE_EXE_VTABLE(MultiObjectComponent, "_ZTV20MultiObjectComponent")
DEFINE_EXE_TYPE(MultiObjectComponent, "ControllerComponent")
