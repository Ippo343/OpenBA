// code/Game/Component/cropfieldcomponent.cpp
// Clean reimplementation of CropFieldComponent lifecycle methods from Application-x64.exe.
//
// CropFieldComponent (0x68 bytes) extends ControllerComponent with crop flags
// and entity reference arrays for field tiles.
//
// Vtable at 0x140162ec0. One non-trivial hookable override:
//   Shutdown (0x14011ae70) — Component_Serialize + empty serializer + versioned block
//                            with entity ref validation, crop flags, IBP array, entity list
//
// Decompiled from Application-x64.exe using Ghidra.

#include "Component/cropfieldcomponent_funcs.h"
#include "engine_funcs.h"
#include "Game/GameUpdateLoop.h"   // B3R STORAGE: GameUpdateLoop::_singleton
#include "resolve.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch A
#include <new>
#include "Entity/ComponentEntity.h"                 // ENDGAME B.3.9 C1: 0x1401c5d70 cell class
#include "Component/NaturalResourceComponent.h"     // ENDGAME B.3.9 C1: 0x1401c7b30 cell class
#include "Component/CropFieldDescription.h"         // ENDGAME B.3.9 C1: 0x1401c7548 cell class
#include "Component/exe_bodies.h"
#include "system/MemoryAllocator.h"
#include "Game/exe_bodies.h"
#include "Work/exe_bodies.h"
#include "Action/exe_bodies.h"
#include "Player/exe_bodies.h"
#include "Game/exe_constants.h"

// ============================================================
// Helpers, called directly (B.3R)
// ============================================================
//   FUN_14011aa20 CropFieldComponent_ValidateEntityRefs — validate entity references
//   FUN_140002340 Reimpl_ExternalPtr_Serialize_StringTable — serialize the IBP at +0x50
//   0x1400335b0   CropFieldComponent_ResizeEntityList (CropFieldComponent_helper_35b0)
//   0x1400336c0   CropFieldComponent_SerializeEntityList (CropFieldComponent_helper_36c0)

// ============================================================
// Reimplemented exe helpers (callee bodies for the guarded call sites)
// ============================================================

// Extra sub-helpers needed by the reimpl bodies below.

// FUN_1400052c0 — FindComponent (BuildStateComponent variant) on a ComponentEntity:
//   WorkBase_FindComponent_52c0, called directly.
// Object::Base::IsA — the import slot 0x140151410 — is the member (Object::Base::IsA).

// Retail CropFieldComponent_helper_36c0 (0x1400336c0) disassembles to the
// dispatch slots ac8, ac8, aa0, a98, aa0, a98, a58 — i.e. two Serialize(int&),
// two InternalBasePtr::Serialize with an IsDerivedFrom check after each, and one
// Serialize(bool&). All of those are real C++ members now.
// ENDGAME B.3.9 C1 (B39_RULES.md D5): the three cells below are registered
// type-id cells (INVENTORY_C Table 1), read BY CELL. 0x1401c7b30 is
// NaturalResourceComponent (not "BuildStateComp" as the old local name said).


// Reimpl of FUN_14011aa20 (validate field-tile entity refs: drop stale/dead refs)
// Iterates the +0x58 array (stride 0x20, count at +0x60). For each "assigned"
// entry (state==1 at +0x4) it looks up the world entity by id; if it is gone, not
// a ComponentEntity, or its build-state component link no longer matches, the
// entry is reset to state 2 with null IBPs.
void CropFieldComponent_ValidateEntityRefs(uintptr_t self) {
    if (GameUpdateLoop::_singleton == nullptr) return;
    uintptr_t world = (uintptr_t)GameUpdateLoop::_singleton;
    uintptr_t map = *(uintptr_t*)(world + 0xf8);

    int count = *(int*)(self + 0x60);
    uintptr_t arr = *(uintptr_t*)(self + 0x58);
    for (int i = 0; i < count; i++) {
        uint8_t* entry = (uint8_t*)(arr + (int64_t)i * 0x20);
        if (*(int*)(entry + 0x4) != 1) continue;

        int id = *(int*)entry;
        void* obj = *(void**)(*(uintptr_t*)(map + 0x80) + 8 + (int64_t)id * 0x30);
        bool keep = false;
        if (obj != nullptr) {
            if (reinterpret_cast<const Object::Base*>(obj)->IsA(ComponentEntity::TypeId())) {   // B.3.x bridge
                uint8_t outIBP[8] = {0};
                WorkBase_FindComponent_52c0(reinterpret_cast<uintptr_t>(obj), outIBP);
                if (*(void**)outIBP != nullptr) {
                    if (obj == *(void**)(entry + 0x8)) keep = true;
                }
            }
        }
        if (keep) continue;

        // Stale: reset entry to state 2 and null the two IBP slots.
        *(int*)(entry + 0x4) = 2;
        uint64_t nullIBP = 0;
        new (&nullIBP) ::Object::InternalBasePtr(nullptr);   // B.3.3 bridge
        *(uint64_t*)(entry + 0x8) = nullIBP;
        nullIBP = 0;
        new (&nullIBP) ::Object::InternalBasePtr(nullptr);   // B.3.3 bridge
        *(uint64_t*)(entry + 0x10) = nullIBP;
    }
}

// Reimpl of CropFieldComponent_helper_35b0 @0x1400335b0
// (grow/shrink the +0x58 entity-ref array; element stride 0x20)
// Matches the standard grow-and-copy: count at [+8], capacity at [+0xc]. On grow,
// allocates cap*0x20, copies old [int,int,ptr,ptr,bool] entries, zero-inits new
// ones, frees old. On shrink, just updates count (no realloc).
void CropFieldComponent_ResizeEntityList(void* arrayPtr, int newSize) {
    int64_t* base = reinterpret_cast<int64_t*>(arrayPtr);
    int oldCount = (int)base[1];
    if (newSize == oldCount) return;

    void* oldData = (void*)base[0];
    *(int*)(base + 1) = newSize;
    int* pCap = (int*)((uintptr_t)base + 0xc);
    if (*pCap < newSize) {
        *pCap = newSize;
        if (newSize == oldCount + 1 && oldCount != 0) {
            *pCap = newSize * 3;
        }
        void* newData = ::System::MemoryAllocator::MemAlloc((uint64_t)(*pCap) * 0x20, 8);
        base[0] = (int64_t)newData;
    }

    int startIdx = (oldData == (void*)base[0]) ? oldCount : 0;
    for (int64_t idx = startIdx; idx < newSize; idx++) {
        uint8_t* dst = (uint8_t*)(base[0] + idx * 0x20);
        if (idx < oldCount) {
            uint8_t* src = (uint8_t*)((uintptr_t)oldData + idx * 0x20);
            *(uint32_t*)(dst + 0) = *(uint32_t*)(src + 0);
            *(uint32_t*)(dst + 4) = *(uint32_t*)(src + 4);
            *(uint64_t*)(dst + 8) = 0;
            *(uint64_t*)(dst + 8) = *(uint64_t*)(src + 8);
            *(uint64_t*)(dst + 16) = 0;
            *(uint64_t*)(dst + 16) = *(uint64_t*)(src + 16);
            *(uint8_t*)(dst + 24) = *(uint8_t*)(src + 24);
        } else {
            *(uint64_t*)(dst + 8) = 0;
            *(uint64_t*)(dst + 16) = 0;
        }
    }

    if (oldData != (void*)base[0]) {
        ::System::MemoryAllocator::MemFree(oldData);
    }
}

// Reimpl of CropFieldComponent_helper_36c0 @0x1400336c0
// (serialize the entity-ref array entries; element stride 0x20)
// Per entry: int id, int state, IBP(ComponentEntity), IBP(BuildStateComponent),
// bool. After each IBP is read it is type-validated and nulled if the type is wrong.
void CropFieldComponent_SerializeEntityList(void* serializer, int count, uintptr_t arrayPtr) {
    if (count <= 0) return;
    uintptr_t entry = arrayPtr;
    for (int i = 0; i < count; i++) {
        reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*(int*)(entry + 0), nullptr);   // B.3.4 bridge
        reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*(int*)(entry + 4), nullptr);   // B.3.4 bridge
        // IBP #1 -> ComponentEntity
        reinterpret_cast<Object::InternalBasePtr*>((void*)(entry + 8))->Serialize(*reinterpret_cast<IO::Serializer*>(serializer));   // B.3.4 bridge
        if (*(void**)(entry + 8) != nullptr &&
            !::Object::IsDerivedFromNullSafe(*(void**)(entry + 8), ComponentEntity::TypeId())) {
            *(uint64_t*)(entry + 8) = 0;
        }
        // IBP #2 -> NaturalResourceComponent (0x1401c7b30; the old local name here
        // said "BuildStateComp", but INVENTORY_C Table 1 says NaturalResourceComponent)
        reinterpret_cast<Object::InternalBasePtr*>((void*)(entry + 0x10))->Serialize(*reinterpret_cast<IO::Serializer*>(serializer));   // B.3.4 bridge
        if (*(void**)(entry + 0x10) != nullptr &&
            !::Object::IsDerivedFromNullSafe(*(void**)(entry + 0x10), NaturalResourceComponent::TypeId())) {
            *(uint64_t*)(entry + 0x10) = 0;
        }
        reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*(bool*)(entry + 0x18), nullptr);   // B.3.4 bridge
        entry += 0x20;
    }
}

// ============================================================
// CropFieldComponent::Shutdown (vtable slot [3], 0x14011ae70)
// ============================================================
// Complex entity reference serialization:
//   1. Component_Serialize (base)
//   2. Empty serializer (v0, immediately destroyed)
//   3. Main serializer (v0):
//      a. FUN_14011aa20 — validate entity refs
//      b. Serialize cropFlags as 2 bools (+0x48, +0x49)
//      c. FUN_140002340 — serialize IBP array at +0x50
//      d. Serialize entity count from +0x60
//      e. On load: resize entity list at +0x58
//      f. CropFieldComponent_helper_36c0 — serialize entity list entries

// ===== ENDGAME B.3.8 MI-B — CropFieldComponent's own slot bodies =====
// The class (CropFieldComponent.h) reaches these three through its members
// (OnMessage / CommandMask / Command, below the bodies).

// ============================================================================
// ENDGAME B.3.8 MI-B — CropFieldComponent as a REAL class (CropFieldComponent.h).
//
// Before: CONTROLLER_LIVE_FACTORY_STATES (48-slot table filled per create, slots
// 3/4/13/14/15/18-21 written from InstallLive, Inert_ScalarDtor in slot 0,
// Dispatcher<CropFieldCtrlTag> stamped into raw bytes at +0x28). Slots 18-21 held
// ControllerLive_ScalarDeletingDtor, the class's type info / id and `return 3`: that
// is CropFieldDescription's table 0x140162f50 read past this 18-slot table's end
// (B38 D3.5) — gone; nothing in the tree dispatches a component past slot 17.
// After: the compiler's 18-slot vtable, a real ctor/dtor chain, the controller a
// member object (B38_RULES D1, D2, D6).
// ============================================================================
#include "Object/factory_registration.h"

// 0x14011afd0. Component() is the CALL 0x1401188b0; the stamps are the compiler's;
// `_controller(this)` is the member's back-pointer; +0x48 (word) = 0x100; +0x50 =
// +0x58 = +0x60 = 0; `_flags |= 0x80` (already set by Component()).
// `MOV word [RBX+0x48],0x100`: +0x48 (work enabled) = 0, +0x49 ("a crop type is
// set") = 1; the tree's factory zero-filled both.
CropFieldComponent::CropFieldComponent()
    : _controller(this), cropFlags(0x100), _pad4A{}, field50(nullptr), field58(nullptr),
      field60(nullptr) {
    OrFlagsInCtor(0x80);
}

// 0x14011af60: MemFree(+0x58), +0x58 = +0x60 = 0, then the restamps and ~Internal
// (the compiler chain).
CropFieldComponent::~CropFieldComponent() {
    Object::detail::InternalMemFree(field58);
    field58 = nullptr;
    field60 = nullptr;
}

// create 0x140032ed0: MemAlloc(0x68, 8), then the ctor.
void* CropFieldComponent::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(CropFieldComponent), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) CropFieldComponent());
}
// Retail's createInPlace is `Internal::CreateInPlaceObject` (returns null); the
// tree's in-place entry built the object, and that is kept (B38 B8-13).
void* CropFieldComponent::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) CropFieldComponent());
}

// [3] 0x14011ae70 (slot 3 is the first call into this class on a load-from-save).
void CropFieldComponent::Persist(void* stream) {
    // Base Component serialization
    Component::Persist(stream);   // retail's direct CALL 0x1401189b0

    // Empty serializer block (version 0, immediately destroyed)
    alignas(8) uint8_t emptySerializer[24];
    ::new (static_cast<void*>(emptySerializer)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(emptySerializer)->~Serializer();   // B.3.4 bridge

    // Main serializer block (version 0)
    alignas(8) uint8_t serializer[16];
    ::new (static_cast<void*>(serializer)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge

    // Validate entity references
    CropFieldComponent_ValidateEntityRefs((uintptr_t)this);   // 0x14011aa20

    // Serialize cropFlags as two bools (+0x48 and +0x49)
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*(bool*)((uintptr_t)this + 0x48), nullptr);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*(bool*)((uintptr_t)this + 0x49), nullptr);   // B.3.4 bridge

    // Serialize InternalBasePtr array at +0x50
    Reimpl_ExternalPtr_Serialize_StringTable((void*)((uintptr_t)this + 0x50), serializer);   // 0x140002340

    // Serialize entity count from +0x60
    int entityCount = *(int*)((uintptr_t)this + 0x60);
    int countBuf = entityCount;
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(countBuf, nullptr);   // B.3.4 bridge

    // On load, resize the entity list to the count just read.
    // ⚠ IO::Serializer +0x08 is m_encoding (a POINTER, never 0) — testing it as
    // the loading flag leaves this load-only branch dead on BOTH legs, so the
    // array is never grown and the loop below silently skips reading the IBPs
    // the save wrote -> stream desync. The flag is on the STREAM (+0x08).
    // Same defect as WorkPlaceComponent (21b3b33); see
    // reference_serializer_plus8_is_encoding_not_loading_flag.
    const uintptr_t cfStream = *(uintptr_t*)serializer;
    if (cfStream && static_cast<int>(reinterpret_cast<IO::Serializer*>(serializer)->GetDirection()) == 0) {   // B.3.4 bridge  (IO::Serializer::GetDirection, retail 0x180006210)
        CropFieldComponent_ResizeEntityList((void*)((uintptr_t)this + 0x58), countBuf);   // 0x1400335b0
    }

    // Serialize entity list entries
    {
        uintptr_t entityArray = *(uintptr_t*)((uintptr_t)this + 0x58);
        CropFieldComponent_SerializeEntityList(serializer, countBuf, entityArray);   // 0x1400336c0
    }

    reinterpret_cast<IO::Serializer*>(serializer)->~Serializer();   // B.3.4 bridge
}
// [4] 0x14011ae60: `MOV qword [RCX+0x50],0` — one store, nothing else.
void CropFieldComponent::Startup() { field50 = nullptr; }
// [13] 0x14011ada0 / [14] 0x14011acc0 / [15] 0x14011ab50 — the bodies above. The
// OnMessage body takes the slot index from the base's third register (R9 = arg0).
// ── slot 13 — CanCommand — retail 0x14011ada0 ─────────────────────────────────────
// Decides which of the crop-field panel's three buttons are enabled. Unlike the
// ResourceProduce family's slot 13 (which ignores its argument and returns a mask),
// this one IS a per-command query:
//   cmd 0 (plant)   enabled when the SM sits in state 2  AND the field has slots
//   cmd 1 (harvest) enabled when the SM sits in state 10 AND the field has slots
//   cmd 2 (toggle)  returns a two-bit value: bit 1 = currently on (+0x48), bit 0 =
//                   selectable at all (has slots). Retail computes it as
//                   `-(+0x48 != 0) & 2 | hasSlots`, i.e. a MASK, not a boolean — the
//                   panel reads bit 1 to draw the toggle's state and bit 0 to enable
//                   it. Returning a plain bool here would collapse "on" and
//                   "available" into one and mis-draw the button.
// "Has slots" is self+0x50 != 0 — the slot array state 9 builds. An unplanted field
// therefore greys everything, which is retail behaviour, not a defect.
uint32_t CropFieldComponent::CommandMask(int command) {
    const uintptr_t self = reinterpret_cast<uintptr_t>(this);
    uintptr_t smc = 0;
    PlayerPath_FindComponent_3ea0(*reinterpret_cast<uintptr_t*>(self + 0x10), &smc);   // 0x140003ea0
    if (!smc) return 0;

    int8_t cur = *reinterpret_cast<int8_t*>(smc + 0x52);
    if (cur < 0) cur = *reinterpret_cast<int8_t*>(smc + 0x50);
    const bool hasSlots = (*reinterpret_cast<uintptr_t*>(self + 0x50) != 0);

    switch (command) {
        case 0:  return (cur == 2  && hasSlots) ? 1 : 0;
        case 1:  return (cur == 10 && hasSlots) ? 1 : 0;
        case 2:  return static_cast<uint8_t>(
                     ((*reinterpret_cast<char*>(self + 0x48) != 0) ? 2 : 0) |
                     (hasSlots ? 1 : 0));
        default: return 0;
    }
}
// ── slot 14 — Command — retail 0x14011acc0 ────────────────────────────────────────
//   cmd 0 -> post node event 3 (PLANT)    — the event state 2 registers
//   cmd 1 -> post node event 4 (HARVEST)  — the event state 10 registers
//   cmd 2 -> flip +0x48 and push it straight into the workplace accepting flag
// The two event ids line up exactly with the state table in
// cropfieldcomponent_states.cpp, so the panel drives the same paths the seasonal
// triggers do.
void CropFieldComponent::Command(int command) {
    const uintptr_t self = reinterpret_cast<uintptr_t>(this);
    // 0x140003ea0 PlayerPath_FindComponent_3ea0 (SMC), 0x140002e10
    // AttendanceComponent_GetJobHostComponent (WPC), 0x14012fc00 WorkPlaceComponent_SetAccepting.
    if (command != 0 && command != 1 && command != 2) return;

    if (command == 2) {
        char* master = reinterpret_cast<char*>(self + 0x48);
        *master = (*master == 0) ? 1 : 0;
        uintptr_t wpc = 0;
        AttendanceComponent_GetJobHostComponent(*reinterpret_cast<void**>(self + 0x10), &wpc);
        if (!wpc) return;
        WorkPlaceComponent_SetAccepting(static_cast<int64_t>(wpc), *master != 0);
        return;
    }

    uintptr_t smc = 0;
    PlayerPath_FindComponent_3ea0(*reinterpret_cast<uintptr_t*>(self + 0x10), &smc);
    if (!smc) return;

    const uintptr_t sm = smc + 0x48;
    void* node = *reinterpret_cast<void**>(sm);
    if (!node) return;
    void** nvt = *reinterpret_cast<void***>(node);
    if (!nvt || !nvt[2]) return;
    const int curIdx  = *reinterpret_cast<int8_t*>(smc + 0x51);
    const int eventId = (command == 0) ? 3 : 4;
    reinterpret_cast<char (*)(void*, void*, int, int)>(nvt[2])(
        node, reinterpret_cast<void*>(sm), curIdx, eventId);
}
// ── slot 15 — OnMessage — retail 0x14011ab50 ──────────────────────────────────────
// OnMessage(self, msgId, sender, slotIndex). Two messages, both addressed at ONE slot
// of the field's grid (self+0x58, stride 0x20, indexed by the 4th argument):
//
//   msg 0 — PLANT BOUND. The PlantArea work has just created the crop entity on this
//           slot's cell, so bind it: state (+0x04) = 1 "growing", stash the entity at
//           +0x08 and its NaturalResource component at +0x10, then copy the slot's
//           plant-mark byte (+0x18) into that component's +0x2d. This is what moves a
//           slot from "empty" to "growing", which is in turn what the state-9
//           per-frame handler counts to decide the field is planted and advance to
//           state 10. Without it a field plants and never progresses.
//
//   msg 1 — GROW TICK. Find the slot entity's growth component, then advance it by
//           Time(growthComp+0x28 * 300.0f * CropFieldDescription+0x24) through that
//           component's own vtable slot 9 (+0x48). 300.0f is 0x140172234.
//
// Retail's profile build carries two assertion strings for this function — "message
// send to planter harvester is wrong" and "map componment isn't initalized correctly!"
// [sic] — i.e. it treats a bad slot index or an uninitialised map as programmer error
// and derefs regardless. Kept unguarded to match, except where our standalone can
// legitimately have no entity on the cell (msg 1 already tests that in retail too).
void CropFieldComponent::OnMessage(int message, void* /*data*/, uint32_t arg0, uint32_t /*arg1*/) {
    const uintptr_t self = reinterpret_cast<uintptr_t>(this);
    if (GameUpdateLoop::_singleton == nullptr) return;

    const uintptr_t map = *reinterpret_cast<uintptr_t*>(
        reinterpret_cast<uintptr_t>(GameUpdateLoop::_singleton) + 0xf8);
    if (!map) return;
    const uintptr_t grid = *reinterpret_cast<uintptr_t*>(map + 0x80);
    const uintptr_t slots = *reinterpret_cast<uintptr_t*>(self + 0x58);
    if (!grid || !slots) return;

    uint8_t* slot = reinterpret_cast<uint8_t*>(slots + (uintptr_t)arg0 * 0x20);
    const int cell = *reinterpret_cast<int32_t*>(slot + 0x00);
    const uintptr_t entity = *reinterpret_cast<uintptr_t*>(grid + 8 + (uintptr_t)cell * 0x30);

    if (message == 0) {
        uintptr_t nrIbp = 0;
        WorkBase_FindComponent_52c0(entity, &nrIbp);   // 0x1400052c0
        *reinterpret_cast<int32_t*>(slot + 0x04)   = 1;          // growing
        *reinterpret_cast<uintptr_t*>(slot + 0x08) = entity;
        *reinterpret_cast<uintptr_t*>(slot + 0x10) = nrIbp;
        if (nrIbp)
            *reinterpret_cast<char*>(nrIbp + 0x2d) = static_cast<char>(slot[0x18]);
        return;
    }

    if (message == 1) {
        if (!entity) return;
        uintptr_t growth = 0;
        ActCNR_FindComponent_c460(entity, &growth);   // 0x14000c460
        if (!growth) return;

        void* desc = *reinterpret_cast<void**>(self + 0x18);
        const uint32_t tidCropDesc = CropFieldDescription::TypeId();
        if (!(desc && ::Object::IsDerivedFromNullSafe(desc, tidCropDesc)))
            return;

        const float rate = *reinterpret_cast<float*>(
            *reinterpret_cast<uintptr_t*>(growth + 0x38) + 0x28);
        const float mult = *reinterpret_cast<float*>(
            reinterpret_cast<uintptr_t>(desc) + 0x24);
        int64_t t = 0;
        RtSystem_Time_Ctor(reinterpret_cast<::System::Time*>(&t), rate * kRdata_140172234 * mult);   // 300.0f

        reinterpret_cast<::Component*>(growth)->Update(&t);   // B.3.8 bridge (the GrowthComponent find 0x14000c460; slot 9)
    }
}

DEFINE_EXE_TYPE(CropFieldComponent, "ControllerComponent")
