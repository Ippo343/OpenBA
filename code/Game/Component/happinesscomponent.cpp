// code/Game/Component/happinesscomponent.cpp
// Clean reimplementation of HappinessComponent lifecycle methods from Application-x64.exe.
//
// HappinessComponent (0x50 bytes) -- ControllerComponent subclass with shifted
// controller sub-object at +0x30 (own data byte at +0x28).
// Tracks citizen happiness state.
//
// Vtable at 0x1401595d0:
//   [3] Shutdown  = 0x14004aa80 -- serialize base + happinessFlag
//   [7] OnRemove  = 0x14004aa20 -- ERASE from the happiness list (Ghidra names this
//                                  slot OnCreate on several components; it is the remove side)
//
// Decompiled from Application-x64.exe using Ghidra.

#include "Component/happinesscomponent_funcs.h"
#include "Component/coll_bridge.h"
#include "engine_funcs.h"
#include "Work/exe_bodies.h"   // WorkBase_FindComponent_6270
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch A
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B
#include "Component/HappinessDescription.h"     // ENDGAME B.3.9 C1: 0x1401c42f8 cell class
#include "Component/RadiusDescription.h"        // ENDGAME B.3.9 C1: 0x1401c4870 cell class
#include "Component/RawMaterialDescription.h"   // ENDGAME B.3.9 C1: 0x1401c7cb8 cell class
#include "Component/exe_bodies.h"

// ============================================================
// Helpers, called by name (B3R)
// ============================================================
// 0x140002cc0 CemeteryListErase_Reimpl = DynArray8_EraseByValue -- REMOVES every element
// equal to *value, shifting the tail down and decrementing the count. It is NOT a push; the
// append counterpart is DynArray8_ResizeFill (0x140003110), which is what Register uses.
// Ghidra's own plate comment on the callee records that leaving it unnamed already caused
// two component OnCreate/OnRemove vtable entries to be mislabelled as the create side.
// 0x14004a300 FishingComponent_NotifyTile (below) -- sets index on entity to given value.
// FUN_140003110 was Collection::Array<void*>::SetSize(n, fill) monomorphised into the exe
// (ENDGAME B.3.2 step 2) — converted at its call site below.
// 0x140002e10 AttendanceComponent_GetJobHostComponent(entity, outIBP) -- the owner's
// workplace, used by the "needs staff" arm of IsActive.
// 0x140006270 WorkBase_FindComponent_6270(entity, outIBP) -- the storage, "needs goods" arm.
// IsDerivedFrom (slot 0x140152a98) is Object::Base::IsDerivedFrom.
// DAT_1401c3dd8 (the game world) is read through g_pGameWorldSingleton, the engine_funcs
// global bound to the same cell.
//
// ENDGAME B.3.9 C1 (D5): 0x1401c42f8 (HappinessDescription) and 0x1401c7cb8
// (RawMaterialDescription, whose +0x5c flags the goods test masks) are read
// BY CELL below, as the class's static TypeId().

// ============================================================
// HappinessComponent::Shutdown (vtable slot [3], 0x14004aa80) — the Persist member below
// ============================================================
// Ghidra decompilation:
//   Component_Serialize(param_1, param_2)
//   Serializer(local_20, param_2, 0); ~Serializer(local_20)  // empty controller block
//   Serializer(local_30, param_2, 0)
//   Serialize(local_30, param_1+0x28, nullptr)  // happinessFlag as bool
//   ~Serializer(local_30)

// ============================================================
// HappinessComponent::OnRemove (vtable slot [7], 0x14004aa20) — the member below
// ============================================================
// Ghidra decompilation:
//   if (param_1[0x28] != 0) {
//     InternalBasePtr::Ctor(local, param_1)
//     FUN_140002cc0( *(gameWorld + 0xF0) + 0x50, local )
//     FishingComponent_impl_a300(param_1, -1)
//     param_1[0x28] = 0
//   }

// ============================================================
// Reimpl of FUN_14004a300 (FishingComponent_impl_a300: notify the population
//   manager of a fishing tile, mapping the description's mode to a state id)
// ============================================================
// Despite the local typedef/function names (borrowed from the fishing lane's
// near-identical shape), this reads a HappinessDescription off owner+0x18 (type
// 0x1401c42f8 -- INVENTORY_C Table 1 says HappinessDescription, not
// "FishingDescription" as the old comment guessed), locates the RadiusDescription
// (type 0x1401c4870 -- Table 1, not a generic "fishing-target component") on the
// host, the MapComponent (FUN_140004680) for tile coords, maps desc+0x20 (0..4)
// to a state id, and calls the helper below (0x14009f990).
// 0x140004680 is FloorComponent_FindResData, 0x14009f990 FishingComponent_BrushTile (below).

// ============================================================
// Reimpl of FishingComponent_helper_f990 (0x14009f990) — Component reg wave B.
// Stamps a circular brush of radius `r` (param5) onto a population grid: for every
// cell within the disk it adds `delta` (param6) to a per-cell byte, then bumps a
// per-state accumulator short. mgr (param1): +0 = grid width (int), +0x80 = grid
// base ptr; cell stride 0x30, the touched byte sits at cell+0x1c. The state-totals
// short array starts at mgr+0x90, indexed by `state` (param2). Pure leaf (no callees).
// Faithful 1:1 port of the Ghidra decompile.
// ============================================================
void FishingComponent_BrushTile(int* mgr, int state, int cx, int cy, int r, short delta) {
    uint8_t* base = reinterpret_cast<uint8_t*>(mgr);
    int yy = cy - r;
    if (yy <= cy + r) {
        int dy = cy - yy;
        do {
            int xx = cx - r;
            if (xx <= r + cx) {
                int dx = cx - xx;
                int ix = xx;
                do {
                    int width = *mgr;
                    if (ix >= 0 && yy >= 0 && ix < width && yy < width &&
                        dy * dy + dx * dx <= r * r) {
                        int64_t gridBase = *reinterpret_cast<int64_t*>(base + 0x80);
                        char* cell = reinterpret_cast<char*>(
                            (int64_t)state + 0x1c + (int64_t)(width * yy + ix) * 0x30 + gridBase);
                        *cell = (char)(*cell + (char)delta);
                    }
                    ix++;
                    dx--;
                } while (ix <= r + cx);
            }
            yy++;
            dy--;
        } while (yy <= cy + r);
    }
    short* acc = reinterpret_cast<short*>(base + (int64_t)state * 2 + 0x90);
    *acc = (short)(*acc + delta);
}

void FishingComponent_NotifyTile(void* selfP, int16_t param2) {
    uintptr_t self = (uintptr_t)selfP;
    if (!g_pGameWorldSingleton || !*g_pGameWorldSingleton) return;

    void* descObj = *(void**)(self + 0x18);
    void* desc = ::Object::IsDerivedFromNullSafe(descObj, HappinessDescription::TypeId())
                     ? descObj : nullptr;

    int64_t host = *(int64_t*)(*(int64_t*)(self + 0x10) + 0x28);

    // Find the RadiusDescription (0x1401c4870) on the host
    void* targetComp = nullptr;
    {
        const uint32_t tidTarget = RadiusDescription::TypeId();
        int count = *(int*)(host + 0x30);
        int64_t arr = *(int64_t*)(host + 0x28);
        for (int i = 0; i < count; i++) {
            void* comp = *(void**)(arr + (int64_t)i * 8);
            uint32_t tid = reinterpret_cast<Object::Base*>(comp)->Type();   // B.3.8 bridge (slot 2)
            if (tid == tidTarget) {
                if (::Object::IsDerivedFromNullSafe(comp, tidTarget))
                    targetComp = comp;
                break;
            }
        }
    }

    // Find the MapComponent for tile coords
    uint8_t mapBuf[8] = {0};
    FloorComponent_FindResData(*(void**)(self + 0x10), mapBuf);   // 0x140004680
    int64_t mapComp = *(int64_t*)mapBuf;

    // Map description mode (desc+0x20) to a state id (default 5)
    int state = 5;
    switch (desc ? *(int*)((uintptr_t)desc + 0x20) : -1) {
        case 0: state = 0; break;
        case 1: state = 1; break;
        case 2: state = 2; break;
        case 3: state = 3; break;
        case 4: state = 4; break;
        default: break;
    }

    int x = ((int)*(int16_t*)(mapComp + 0x2c) >> 1) + (int)*(int16_t*)(mapComp + 0x28);
    int y = ((int)*(int16_t*)(mapComp + 0x2e) >> 1) + (int)*(int16_t*)(mapComp + 0x2a);
    int resourceId = targetComp ? *(int*)((uintptr_t)targetComp + 0x28) : 0;

    FishingComponent_BrushTile(*(int**)((uintptr_t)*g_pGameWorldSingleton + 0xf8), state, x, y,
                               resourceId, param2);   // 0x14009f990
}

// ============================================================
// HappinessComponent::IsActive -- 0x14004a550
// ============================================================
// Decides whether this happiness source is currently DOING anything. Read off the
// disassembly at 0x14004a550; the three arms, in retail's order:
//
//   1. desc[+0x20] == 5  -> unconditionally active. This is the plain radius source
//      (the chapel / well / town square kind): being built is enough.
//   2. desc[+0x24] != 0  -> needs staff. Find the owner's workplace via
//      AttendanceComponent_GetJobHostComponent (0x140002e10); with no workplace the
//      staffing test is skipped entirely, otherwise a workplace with no worker
//      (+0x60 == 0), or one that is closed (+0x50 == 0), makes it inactive.
//   3. desc[+0x25] != 0  -> needs goods. Walk the owner's StorageComponent slots
//      (0x140006270; slot records are 0x10 bytes, first qword = the stored good's
//      entity) and AND that good's RawMaterialDescription flags (+0x5c) against the
//      happiness description's accepted-goods mask (+0x28). Active if any slot matches.
//      This is the tavern: it cheers nobody up until there is ale in it.
//
// NOTE the inner RawMaterialDescription lookup is deliberately left unguarded. Retail
// falls out of that loop with RCX == 0 and reads [RCX+0x5c] regardless -- it relies on
// every stored good carrying a RawMaterialDescription. Guarding it here would silently
// skip the mask test rather than restore the behaviour, so the deref stays faithful; if
// it ever faults, the good in storage is what is wrong.
extern "C" bool HappinessComponent_IsActive(void* selfV) {
    HappinessComponent* self = reinterpret_cast<HappinessComponent*>(selfV);

    const uintptr_t su = reinterpret_cast<uintptr_t>(self);
    void* descRaw = *reinterpret_cast<void**>(su + 0x18);
    const uintptr_t desc =
        (descRaw && reinterpret_cast<const Object::Base*>(descRaw)->IsDerivedFrom(
                        HappinessDescription::TypeId()))   // B.3.x bridge (slot 0x140152a98)
            ? reinterpret_cast<uintptr_t>(descRaw) : 0;

    if (*reinterpret_cast<int*>(desc + 0x20) == 5) return true;

    if (*reinterpret_cast<uint8_t*>(desc + 0x24) != 0) {
        void* ownerSlot = *reinterpret_cast<void**>(su + 0x10);
        ownerSlot = *reinterpret_cast<void**>(su + 0x10);

        void* wpIbp = nullptr;
        AttendanceComponent_GetJobHostComponent(ownerSlot, &wpIbp);   // 0x140002e10
        if (wpIbp) {
            const uintptr_t wp = reinterpret_cast<uintptr_t>(wpIbp);
            if (*reinterpret_cast<int*>(wp + 0x60) == 0) return false;
            if (*reinterpret_cast<uint8_t*>(reinterpret_cast<uintptr_t>(wpIbp) + 0x50) == 0)
                return false;
        }
    }

    if (*reinterpret_cast<uint8_t*>(desc + 0x25) == 0) return true;

    void* owner = *reinterpret_cast<void**>(su + 0x10);

    void* storeIbp = nullptr;
    WorkBase_FindComponent_6270(reinterpret_cast<uintptr_t>(owner), &storeIbp);   // 0x140006270

    const uint32_t wantMask = *reinterpret_cast<uint32_t*>(desc + 0x28);
    bool matched = false;

    uintptr_t store = reinterpret_cast<uintptr_t>(storeIbp);
    if (!store) return false;

    for (int i = 0; i < *reinterpret_cast<int*>(store + 0x30); ++i) {
        store = reinterpret_cast<uintptr_t>(storeIbp);
        const uintptr_t slots = *reinterpret_cast<uintptr_t*>(store + 0x28);
        const uintptr_t good  = *reinterpret_cast<uintptr_t*>(slots + static_cast<uintptr_t>(i) * 0x10);

        uintptr_t rawMatDesc = 0;
        if (good) {
            const uint32_t  tidRawMat = RawMaterialDescription::TypeId();
            const int       n   = *reinterpret_cast<int*>(good + 0x30);
            const uintptr_t arr = *reinterpret_cast<uintptr_t*>(good + 0x28);
            for (int j = 0; j < n; ++j) {
                void* c = *reinterpret_cast<void**>(arr + static_cast<uintptr_t>(j) * 8);
                if (!c) continue;
                const uint32_t tid = reinterpret_cast<Object::Base*>(c)->Type();   // B.3.8 bridge (slot 2)
                if (tid == tidRawMat) {
                    if (reinterpret_cast<const Object::Base*>(c)->IsDerivedFrom(tidRawMat))   // B.3.x bridge
                        rawMatDesc = reinterpret_cast<uintptr_t>(c);
                    break;
                }
            }
        }
        // Faithful unguarded read -- see the note above.
        const uint32_t flags = *reinterpret_cast<uint32_t*>(rawMatDesc + 0x5c);
        matched |= (wantMask & flags) != 0;
    }
    return matched;
}

// ============================================================
// HappinessComponent::Register -- 0x14004a4f0
// ============================================================
// Stamp the happiness radius onto the map (a300 with state 1), append the component's
// IBP to the TownManager happiness list at *(GameWorld+0xF0)+0x50 (count at +0x58),
// and latch self[+0x28] so it is not registered twice.
extern "C" void HappinessComponent_Register(void* selfV) {
    HappinessComponent* self = reinterpret_cast<HappinessComponent*>(selfV);

    FishingComponent_NotifyTile(self, 1);   // 0x14004a300

    uint8_t basePtr[24] = {};
    new (basePtr) ::Object::InternalBasePtr(
        reinterpret_cast<RtObject::Internal*>(self));   // B.3.3 bridge

    if (g_pGameWorldSingleton && *g_pGameWorldSingleton) {
        const uintptr_t townMgr = *reinterpret_cast<uintptr_t*>(
            reinterpret_cast<uintptr_t>(*g_pGameWorldSingleton) + 0xF0);
        if (townMgr) {
            const int count = *reinterpret_cast<int*>(townMgr + 0x58);
            // B.3.2 bridge: exe 0x140003110 IS Collection::Array<void*>::SetSize(n, fill).
            GameColl::AsArray<GameColl::Ptr8>(reinterpret_cast<void*>(townMgr + 0x50))
                .SetSizeFill(count + 1, basePtr);

            static int n = 0;
            if (n < 8) { ++n;
                fprintf(stderr, "[Happiness] source REGISTERED: comp=%p townMgr=%p "
                                "happinessCount=%d\n", selfV,
                        reinterpret_cast<void*>(townMgr),
                        *reinterpret_cast<int*>(townMgr + 0x58));
                fflush(stderr);
            }
        }
    }
    self->happinessFlag = 1;
}

// ============================================================
// HappinessComponent::Unregister -- 0x14004a4a0
// ============================================================
// The mirror of Register, and byte-identical to the body of slot 7 / the state-3 enter
// minus their `if (self[+0x28])` guard.
extern "C" void HappinessComponent_Unregister(void* selfV) {
    HappinessComponent* self = reinterpret_cast<HappinessComponent*>(selfV);

    uint8_t basePtr[24] = {};
    new (basePtr) ::Object::InternalBasePtr(
        reinterpret_cast<RtObject::Internal*>(self));   // B.3.3 bridge

    if (g_pGameWorldSingleton && *g_pGameWorldSingleton) {
        const uintptr_t townMgr = *reinterpret_cast<uintptr_t*>(
            reinterpret_cast<uintptr_t>(*g_pGameWorldSingleton) + 0xF0);
        if (townMgr)
            CemeteryListErase_Reimpl(reinterpret_cast<void*>(townMgr + 0x50), basePtr);
    }
    FishingComponent_NotifyTile(self, -1);   // 0x14004a300
    self->happinessFlag = 0;
}

// ============================================================
// HappinessComponent state 2 / 13 "built" enter -- 0x14004a9f0
// ============================================================
// Register on entry, but only if not already registered and currently active.
extern "C" char HappinessComponent_EnterBuilt(void* comp, void* /*sm*/) {
    HappinessComponent* self = reinterpret_cast<HappinessComponent*>(comp);
    const bool active = HappinessComponent_IsActive(comp);
    if (self->happinessFlag == 0 && active)
        HappinessComponent_Register(comp);
    return 0;
}

// ============================================================
// HappinessComponent state 2 / 13 trans38 tick -- 0x14004a9a0
// ============================================================
// The per-frame column (StateMachine::Tick's +0x38 handler). It re-evaluates IsActive
// every tick and moves the source in and out of the town list as the conditions change
// -- a tavern that runs out of ale drops out here and re-registers when it is restocked.
// This is the only state-table column HappinessComponent populates, which is why its
// RegisterState instantiation (0x140005e30) is the 3-argument one: the extra stack
// argument lands in the trans38 slot at table entry +0x18.
extern "C" char HappinessComponent_Tick38(void* comp, void* /*sm*/, void* /*time*/,
                                          int /*idx*/) {
    HappinessComponent* self = reinterpret_cast<HappinessComponent*>(comp);
    const bool active     = HappinessComponent_IsActive(comp);
    const bool registered = (self->happinessFlag != 0);

    if (!registered && active) {
        HappinessComponent_Register(comp);
    } else if (registered && !active) {
        HappinessComponent_Unregister(comp);
    }
    return 0;
}

// HappinessComponent state-3 "demolish" enter -- 0x14004a940. Byte-identical to the
// slot-7 removal hook (0x14004aa20), guard included.
extern "C" char HappinessComponent_EnterDemolish(void* comp, void* /*sm*/) {
    reinterpret_cast<HappinessComponent*>(comp)->HappinessComponent::OnRemove();   // B.3.8 bridge (state enters take void*)
    return 0;
}

// ============================================================================
// ENDGAME B.3.8 MI-A — HappinessComponent as a REAL class (HappinessComponent.h).
//
// Before: a 48-slot hand-built `HappinessComponent_LiveVtable` filled at every
// create (FillTypedVtable + FinishLiveVtable), `Inert_ScalarDtor` in slot 0 (only `~Internal`), the
// ControllerLive::Dispatcher<HappinessCtrlTag> table stamped into raw bytes at +0x30.
// After: the compiler's 18-slot vtable, a real constructor and destructor
// chain, the controller a member object (B38_RULES D1, D2, D6).
// ============================================================================
#include "Object/factory_registration.h"
#include "Component/HappinessComponent.h"
#include "Component/controller_state_table.h"

using HappinessCtrlDispatch = ::ControllerLive::Dispatcher<HappinessCtrlTag>;

// The retail table, transcribed from 0x14004ac10 call for call. Its RegisterState
// instantiation (0x140005e30) copies the caller's 5th stack argument into the entry's
// +0x18 field -- verified in the callee (it reads [rsp+0x170] into [rsp+0x48]), which is
// the trans38 column. States 2 and 13 pass 0x14004a9a0 there; state 3 passes null.
// Built ONCE at static-init time, as retail's static initializer does (B38 D6/D7).
static void HappinessComponent_PopulateStateTable() {
    using namespace ::ControllerLive;
    StateTable& t = HappinessCtrlDispatch::table;
    if (t.count != 0) return;                      // idempotent
    TableRegisterState(t, 2, reinterpret_cast<void*>(&HappinessComponent_EnterBuilt),
                       nullptr, reinterpret_cast<void*>(&HappinessComponent_Tick38), nullptr);
    TableAddTransition(t, 3, nullptr);
    TableRegisterState(t, 13, reinterpret_cast<void*>(&HappinessComponent_EnterBuilt),
                       nullptr, reinterpret_cast<void*>(&HappinessComponent_Tick38), nullptr);
    TableAddTransition(t, 3, nullptr);
    TableRegisterState(t, 3, reinterpret_cast<void*>(&HappinessComponent_EnterDemolish),
                       nullptr, nullptr, nullptr);
    TableAddTransition(t, 6, nullptr);
    TableAddTransition(t, 4, nullptr);
    TableAddTransition(t, 5, nullptr);
}
static const bool s_happinessStateTableBuilt = (HappinessComponent_PopulateStateTable(), true);

// 0x14004ab60. Component() is the CALL 0x1401188b0; the stamps are the compiler's;
// +0x28 = 0 (byte); `_controller(this)` is the member's back-pointer at +0x48; then
// `_flags |= 2` (0x82).
HappinessComponent::HappinessComponent() : happinessFlag(0), _pad29{}, _controller(this) {
    OrFlagsInCtor(0x2);
}

// 0x14004ab00: restamp + ~Internal, no own work.
HappinessComponent::~HappinessComponent() = default;

// create 0x140005ba0: MemAlloc(0x50, 8), then the ctor.
void* HappinessComponent::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(HappinessComponent), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) HappinessComponent());
}
// Retail's createInPlace is `Internal::CreateInPlaceObject` (returns null); the
// tree's in-place entry built the object, and that is kept (B38 B8-13).
void* HappinessComponent::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) HappinessComponent());
}

// [3] 0x14004aa80 — happinessFlag round-trips (save/load wave 2026-08-28):
// Component::Persist (a direct CALL 0x1401189b0), an empty controller scope,
// then a scope carrying happinessFlag (+0x28) as a bool.
void HappinessComponent::Persist(void* stream) {
    Component::Persist(stream);
    { IO::Serializer scope(*reinterpret_cast<IO::Stream*>(stream), 0); }   // B.3.8 bridge (stream is void* in Internal::Persist)
    IO::Serializer ser(*reinterpret_cast<IO::Stream*>(stream), 0);         // B.3.8 bridge
    ser.Serialize(*reinterpret_cast<bool*>(&happinessFlag), nullptr);
}

// [7] 0x14004aa20 — if registered: ERASE this component from the happiness list
// at (gameWorld+0xF0)+0x50, set the entity index to -1, clear the flag.
//
// This said "Push" until 2026-08-25 and the function was called OnCreate. Both
// were wrong and they were the SAME wrong: 0x140002cc0 is DynArray8_EraseByValue,
// so a slot that erases, sets the entity index to -1 and clears the registered
// flag is the REMOVE side.
void HappinessComponent::OnRemove() {
    if (happinessFlag == 0) return;
    uint8_t basePtr[24];
    new (basePtr) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>(this));   // B.3.3 bridge
    if (g_pGameWorldSingleton && *g_pGameWorldSingleton) {
        void* happinessArray = reinterpret_cast<void*>(
            *reinterpret_cast<uintptr_t*>(
                reinterpret_cast<uintptr_t>(*g_pGameWorldSingleton) + 0xF0) + 0x50);
        CemeteryListErase_Reimpl(happinessArray, basePtr);   // 0x140002cc0
    }
    FishingComponent_NotifyTile(this, -1);                    // 0x14004a300 (entity index)
    happinessFlag = 0;
}


// [15] 0x14004a780 — PORT GAP (HappinessComponent.h): the tree's slot 15 was the
// inert stub, kept; porting retail's body is its own titled commit.
void HappinessComponent::OnMessage(int /*message*/, void* /*data*/, uint32_t /*arg0*/, uint32_t /*arg1*/) {}

DEFINE_EXE_TYPE(HappinessComponent, "ControllerComponent")
