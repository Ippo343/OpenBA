// engine/src/ui/cropui.cpp
// Clean reimplementation of CropUI game logic.
//
// CropUI (0x60 bytes) — crop selection panel.
// Extends SelectResourceUI (abstract) > DialogController.
// data30 (internalBasePtr) = selectResource combo element.
//
// Decompiled from Application-x64.exe.

#include "Dialog/ui_panels.h"
#include "Dialog/CropUI.h"
#include "ui/UserInterface.h"
#include "object/TypeInfo.h"
#include "Dialog/exe_bodies.h"
#include "Game/GameUpdateLoop.h"   // B3R STORAGE: GameUpdateLoop::_singleton
#include "system/String.h"
#include <cstdint>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B
#include "runtime/types/ui/Element.h"  // ENDGAME B.3.6 UI callers (K3)
#include "runtime/types/ui/DialogInstance.h"  // ENDGAME B.3.6 UI callers (K3)
#include "runtime/types/ui/String.h"  // ENDGAME B.3.6 UI callers (K3)
#include "runtime/types/ui/Sprite.h"  // ENDGAME B.3.6 UI callers (K3)
#include "runtime/types/ui/SpriteSheet.h"  // ENDGAME B.3.6 UI callers (K3)
#include "runtime/types/ui/StringTable.h"  // ENDGAME B.3.6 UI callers (K3)
#include "object/External.h"  // ENDGAME B.3.6 UI callers (K3)
#include <new>  // ENDGAME B.3.6 UI callers (K3)
#include "runtime/types/ui/ComboElement.h"  // ENDGAME B.3.6 UI callers round 2 (K3)
#include "Entity/ComponentEntity.h"   // X::TypeId() (ENDGAME B.3.9)
#include "Component/SelectDescription.h"   // X::TypeId() (ENDGAME B.3.9)

// ============================================================
// SelectDescription layout (from Ghidra analysis):
//   +0x20: SpriteSheet (ExternalPtr, 8 bytes)
//   +0x28: SpriteName (System::String, 8 bytes)
//   +0x30: StringTable (ExternalPtr, 8 bytes)
//   +0x38: StringName (System::String, 8 bytes)
//   +0x40: categoryId (int)
// ============================================================

// ============================================================
// Resource type list in game world (from DAT_1401c3dd8 + 0xF0):
//   +0x120: resourceTypes array pointer (array of pointers)
//   +0x128: resourceTypes count (int)
//
// Each resource type entry:
//   +0x28: sub-components array pointer (array of pointers)
//   +0x30: sub-components count (int)
// ============================================================

// CropUI_IterateResources / CropUI_PopulateCombo / CropUI_Slot4 were CropUI's
// copies of SelectResourceUI's 0x140078a40 / 0x140078830 / 0x140078be0 (defined in
// selectresourceui.cpp). CropUI::OnCreated (0x1400b2b50) calls the base's
// 0x140078be0, as OrchardUI and PastureUI do (B4 DLG-a, behaviour commit).

// ============================================================
// CropUI::Slot7 (vtable +0x38) — Update handler
// ============================================================
// Enables or disables the selectResource combo based on whether
// it has any items. Uses virtual call at vtable+0x40 (SetEnabled).
//
// Original: 0x140078CB0.

// [7] 0x140078cb0 — SelectResourceUI's own slot 7 body (the tree ported it under
// CropUI's name; it reads only the base's +0x28/+0x30).
void SelectResourceUI::OnUpdateLayout() {
    // Per-frame (+0x38): bail on a dead / marked-for-removal datasource before the
    // IsDerivedFrom probe (reliable via the one-frame removal deferral, see
    // entitylist.cpp Step 0).
    if (!SA_DialogEntityAlive(this)) return;
    // Wrap entity in InternalBasePtr (base class requirement, not used directly)
    void* entity = *(void**)((uintptr_t)this + 0x28);
    bool isEntity = ::Object::IsDerivedFromNullSafe(entity, ComponentEntity::TypeId());
    void* entityPtr = isEntity ? entity : nullptr;
    uint8_t entityBuf[8];
    new (entityBuf) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>(entityPtr));   // B.3.3 bridge

    // Get selectResource combo element
    void* combo = (void*)this->internalBasePtr;

    // Enable combo if it has items, disable if empty
    int itemCount = reinterpret_cast<const UI::ComboElement*>(combo)->GetItemCount();  // B.3.6 bridge

    // Virtual call: vtable[8] = SetEnabled(hasItems)
    reinterpret_cast<UI::Element*>(combo)->Enable(itemCount > 0);  // B.3.6 bridge: Element slot 8 Enable
}
