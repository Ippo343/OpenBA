// engine/src/ui/radiusui.cpp
// Clean reimplementation of RadiusUI game logic.
//
// RadiusUI (0x38 bytes) — displays building radius overlay.
// Extends DialogController. data30 (int32_t +0x30) = radius overlay index (-1 = none).
//
// Decompiled from Application-x64.exe.

#include "Dialog/ui_panels.h"
#include "engine_funcs.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B
#include "Entity/ComponentEntity.h"   // X::TypeId() (B.3.9 CG2)
#include "Component/RadiusDescription.h"   // X::TypeId() (B.3.9 CG2)

// ============================================================
// RadiusUI::Slot6 (vtable +0x30) — Remove radius overlay
// ============================================================
// When the panel is hidden/closed, removes the active radius overlay
// from the RadiusDisplayManager and resets the index to -1.
//
// Original: 0x140077600, 28 bytes.

// ============================================================
// RadiusUI::Slot5 (vtable +0x28) — Calculate and store radius overlay
// ============================================================
// When the panel is shown, finds the RadiusComponent on the entity's
// class sub-object, gets the BuildComponent, calculates the center
// tile index, and stores the radius overlay display index at +0x30.
//
// Original: 0x140077650.

// [5] 0x140077650
void RadiusUI::OnOpened() {
    if (!SA_DialogEntityAlive(this)) {
        return;
    }
    uintptr_t s = (uintptr_t)this;

    // Get entity at +0x28, check IsA ComponentEntity
    void* entity = (void*)*(uint64_t*)(s + 0x28);
    uint32_t entityTypeId = ComponentEntity::TypeId();
    void* checkedEntity = nullptr;
    if (fn_IsDerivedFrom(entity, entityTypeId)) {
        checkedEntity = entity;
    }

    uint64_t entityBasePtr = 0;
    new (&entityBasePtr) ::Object::InternalBasePtr(
        reinterpret_cast<RtObject::Internal*>(checkedEntity));   // B.3.3 bridge

    // SA-only guard (retail derefs this unguarded): retail can only route a RadiusUI
    // controller to a live ComponentEntity, but our promoted vtable is also dispatched
    // from the harness-driven panel open, where the data source (+0x28) can still be
    // null on the first Slot5 before the framework seats it.
    if (!entityBasePtr) { this->data30 = -1; return; }

    // Get class/template sub-object from entity+0x28
    uint64_t subObj = *(uint64_t*)(entityBasePtr + 0x28);

    // Search sub-object's component array (+0x28 base, +0x30 count) for RadiusComponent
    // ENDGAME B.3.9 CG2: g_TypeId_RadiusComponent binds cell 0x1401c4870, whose retail
    // class is RadiusDescription (Table 1 NAME LIE — the global's name says
    // RadiusComponent).
    uint32_t radiusTypeId = RadiusDescription::TypeId();
    void* radiusComp = nullptr;
    int count = *(int*)(subObj + 0x30);
    uint64_t arrayBase = *(uint64_t*)(subObj + 0x28);
    for (int i = 0; i < count; i++) {
        void* comp = (void*)*(uint64_t*)(arrayBase + (uint64_t)i * 8);
        // vtable[2] (offset 0x10) = GetTypeId. Ghidra prints this as `(...)()` — that
        // means "args unknown", NOT "no args"; the callee still reads `this` from RCX.
        int typeId = reinterpret_cast<Object::Base*>(comp)->Type();  // B.3.8 bridge
        if (typeId == (int)radiusTypeId) {
            if (fn_IsDerivedFrom(comp, radiusTypeId)) {
                radiusComp = comp;
            }
            break;
        }
    }

    // Find the placement component on the entity (0x1401c79e0 — labelled
    // "MapComponent" in reg_economy_typeids; the finder name is a known misnomer).
    uint64_t buildComp = 0;
    fn_FindComponent_Build((void*)entityBasePtr, &buildComp);

    // SA-only guards (retail derefs both unguarded). Retail reaches Slot5 only from a
    // .rsc panel whose entity is a placed building carrying both components; our
    // promoted controller is also mounted by the harness panel-open path, which can
    // select an entity whose description has no RadiusDescription. Without this,
    // 0x140055430 reads radiusComp+0x20 off a null pointer.
    if (!buildComp || !radiusComp) {
        this->data30 = -1;
        return;
    }

    // Calculate center tile index from BuildComponent coordinates:
    // +0x28 = int16 baseX, +0x2a = int16 baseY
    // +0x2c = int16 sizeX, +0x2e = int16 sizeY
    int gridSize = **(int**)((uintptr_t)(*g_pGameWorldSingleton) + 0xf8);
    int centerY = (int)*(int16_t*)(buildComp + 0x2a) + ((int)*(int16_t*)(buildComp + 0x2e) >> 1);
    int centerX = (int)*(int16_t*)(buildComp + 0x28) + ((int)*(int16_t*)(buildComp + 0x2c) >> 1);
    int tileIndex = centerY * gridSize + centerX;

    // Calculate radius and store display index
    this->data30 = fn_RadiusCalc(radiusComp, tileIndex);

}

// ============================================================
// RadiusUI::Slot6 (vtable +0x30) — Remove radius overlay
// ============================================================
// When the panel is hidden/closed, removes the active radius overlay
// from the RadiusDisplayManager and resets the index to -1.
//
// Original: 0x140077600, 28 bytes.

// [6] 0x140077600 — unconditional: it resets +0x30 to -1 in both arms, and the
// manager pointer read is guarded by data30 >= 0 inside the body.
void RadiusUI::OnClosed() {
    if (this->data30 >= 0) {
        // Game/World singleton +0x110 = RadiusDisplayManager
        void* gameWorld = *g_pGameWorldSingleton;
        void* radiusMgr = *(void**)((uintptr_t)gameWorld + 0x110);
        fn_RadiusDisplay_Remove(radiusMgr, this->data30);
    }
    this->data30 = -1;
}
