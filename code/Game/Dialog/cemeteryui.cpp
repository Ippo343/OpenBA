// engine/src/ui/cemeteryui.cpp
// Clean reimplementation of CemeteryUI game logic.
//
// CemeteryUI (0x38 bytes) — cemetery progress bar panel.
// Extends DialogController. Fields:
//   +0x30 progressElement (InternalBasePtr)
//
// Decompiled from Application-x64.exe.

#include "Dialog/ui_panels.h"
#include "ui/UserInterface.h"
#include "object/TypeInfo.h"
#include "Entity/exe_bodies.h"
#include "Dialog/exe_bodies.h"
#include "Game/GameUpdateLoop.h"   // B3R STORAGE: GameUpdateLoop::_singleton
#include "Object/factory_registration.h"
#include "system/String.h"
#include <cstdint>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B
#include "runtime/types/ui/Element.h"  // ENDGAME B.3.6 UI callers (K3)
#include <new>  // ENDGAME B.3.6 UI callers (K3)
#include "runtime/types/ui/ProgressElement.h"  // ENDGAME B.3.6 UI callers round 2 (K3)
#include "Component/CemeteryComponent.h"   // X::TypeId() (B.3.8 callers)
#include "Component/TombstoneComponent.h"   // X::TypeId() (B.3.8 callers)
#include "Entity/ComponentEntity.h"   // X::TypeId() (ENDGAME B.3.9)

// ============================================================
// File-local helper: CemeteryGetBurialCount (FUN_140046dd0)
// ============================================================
// Counts the number of graves in a CemeteryComponent that have a
// TombstoneComponent present (meaning they are occupied by a buried citizen).
//
// CemeteryComponent (param_1) fields:
//   +0x48: void* graveIndexArray   (int32[] of entity-slot indices)
//   +0x50: int32 graveCount
//
// Entity storage (accessed via gameWorld+0xf8):
//   entityStore = *(gameWorld + 0xf8)
//   entitySlot  = *(entityStore + 0x80) (the flat entity slot array)
//   entity      = *(entitySlot + 8 + graveIndex*0x30)  -- InternalPtr value
//
// Entity component search: looks through entity's component list (+0x30, count +0x38)
//   for a component with TypeId == TombstoneComponent (DAT_1401c4a30).
//
// Original: FUN_140046dd0.
//
static int CemeteryGetBurialCount_Impl(void* cemeteryComp) {
    uintptr_t cem = (uintptr_t)cemeteryComp;
    int count = *(int*)(cem + 0x50);
    if (count <= 0) return 0;

    uintptr_t entityStore = *(uintptr_t*)((uintptr_t)GameUpdateLoop::_singleton + 0xF8);
    uintptr_t* graveIdxArray = *(uintptr_t**)(cem + 0x48);
    int burials = 0;

    for (int i = 0; i < count; i++) {
        int graveIdx = ((int*)graveIdxArray)[i];

        // Get entity InternalPtr from entity slot array
        uintptr_t entitySlot = *(uintptr_t*)(entityStore + 0x80);
        uintptr_t entityIPtr = *(uintptr_t*)(entitySlot + 8 + (uintptr_t)graveIdx * 0x30);
        if (entityIPtr == 0) continue;

        uintptr_t entity = entityIPtr;
        if (!entity) continue;

        // Search entity's component list for TombstoneComponent
        int compCount = *(int*)(entity + 0x38);
        uintptr_t* compArray = *(uintptr_t**)(entity + 0x30);
        uint32_t tombTypeId = TombstoneComponent::TypeId();

        bool found = false;
        for (int c = 0; c < compCount; c++) {
            uintptr_t compIPtr = compArray[c];
            void* comp = (void*)compIPtr;
            if (!comp) continue;

            // Check TypeId via vtable[2]
            int typeId = reinterpret_cast<Object::Base*>(comp)->Type();  // B.3.8 bridge
            if (typeId == (int)tombTypeId) {
                bool isTomb = ::Object::IsDerivedFromNullSafe(comp, tombTypeId);
                if (isTomb) { found = true; break; }
            }
        }
        if (found) burials++;
    }
    return burials;
}

// CemeteryUI: PROMOTED from a bare UI_DIALOG_STUB 2026-08-23. The panel bodies in
// code/Game/Dialog/cemeteryui.cpp were fully ported but wired into NOTHING -- the stub
// vtable left slots 4 and 7 as no-ops, so neither ProgressElement text parameter was
// ever set and the panel rendered a literal "  of   graves" with both numbers blank.
// Same shape as AttendanceUI directly above.
// ENDGAME B.3.9: ProgressElement/ComponentEntity/CemeteryComponent/TombstoneComponent
// all have a ported TypeId() now, so the lazy-cache over the exe .rdata globals is dead.

// ============================================================
// CemeteryUI::Slot4 (vtable +0x20) — Setup/refresh
// ============================================================
// Finds ProgressElement by type, stores at +0x30.
// Gets CemeteryComponent, reads max slots from +0x50,
// sets as progress maximum.
//
// Original: 0x14005F090.

// [4] 0x14005f090
void CemeteryUI::OnCreated() {
    void* dialogElem = *(void**)((uintptr_t)this + 0x20);

    // Find ProgressElement by type
    uint64_t result = 0;
    ::new (&result) Object::InternalPtr<UI::Element>(reinterpret_cast<UI::Element*>(dialogElem)->FindElementType(UI::ProgressElement::TypeId()));  // B.3.6 bridge
    void* foundElem = (void*)result;

    // Type-check and wrap as InternalBasePtr
    bool isProgress = ::Object::IsDerivedFromNullSafe(foundElem, UI::ProgressElement::TypeId());
    void* progressPtr = isProgress ? foundElem : nullptr;
    uint8_t basePtrBuf[8];
    new (basePtrBuf) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>(progressPtr));   // B.3.3 bridge
    this->data30 = *(uint64_t*)basePtrBuf;

    // Setup: bail on a dead / marked-for-removal datasource (see Slot7).
    if (!SA_DialogEntityAlive(this)) return;
    // Get entity, find CemeteryComponent
    void* entity = *(void**)((uintptr_t)this + 0x28);
    bool isEntity = ::Object::IsDerivedFromNullSafe(entity, ComponentEntity::TypeId());
    void* entityPtr = isEntity ? entity : nullptr;
    uint8_t entityBuf[8];
    new (entityBuf) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>(entityPtr));   // B.3.3 bridge
    void* entityInternal = *(void**)entityBuf;

    void* cemetComp = nullptr;
    fn_FindComponent_Cemetery(entityInternal, &cemetComp);

    // Set progress maximum from component's max slots field (+0x50)
    int maxSlots = *(int*)((uintptr_t)cemetComp + 0x50);
    void* progress = (void*)this->data30;
    *(float*)((uintptr_t)progress + 0x124) = (float)maxSlots;
}

// ============================================================
// CemeteryUI::Slot7 (vtable +0x38) — Update handler
// ============================================================
// Gets burial count and max slots from CemeteryComponent,
// updates progress bar value and two text parameters.
//
// Original: 0x14005EF50.

// [7] 0x14005ef50
void CemeteryUI::OnUpdateLayout() {
    // Per-frame (+0x38): bail on a dead / marked-for-removal datasource before the
    // IsDerivedFrom probe (reliable via the one-frame removal deferral, see
    // entitylist.cpp Step 0).
    if (!SA_DialogEntityAlive(this)) return;
    // Get entity, find CemeteryComponent
    void* entity = *(void**)((uintptr_t)this + 0x28);
    bool isEntity = ::Object::IsDerivedFromNullSafe(entity, ComponentEntity::TypeId());
    void* entityPtr = isEntity ? entity : nullptr;
    uint8_t entityBuf[8];
    new (entityBuf) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>(entityPtr));   // B.3.3 bridge
    void* entityInternal = *(void**)entityBuf;

    void* cemetComp = nullptr;
    fn_FindComponent_Cemetery(entityInternal, &cemetComp);

    void* progress = (void*)this->data30;

    // Get burial count and set as progress current value
    int burialCount = CemeteryGetBurialCount_Impl(cemetComp);
    *(float*)((uintptr_t)progress + 0x128) = (float)burialCount;

    // Set text param 0: burial count
    {
        System::String str = System::String::Format("%i", (unsigned int)burialCount);
        reinterpret_cast<UI::ProgressElement*>(progress)->SetProgressTextParameter(0, str);  // B.3.6 bridge
    }

    // Set text param 1: max slots
    int maxSlots = *(int*)((uintptr_t)cemetComp + 0x50);
    {
        System::String str = System::String::Format("%i", (unsigned int)maxSlots);
        reinterpret_cast<UI::ProgressElement*>(progress)->SetProgressTextParameter(1, str);  // B.3.6 bridge
    }
}
