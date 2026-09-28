// code/Game/Dialog/edittool_select.cpp
// Faithful clean-room port of Entity_impl_d3b0 (Application-x64.exe 0x1400fd3b0):
// EditTool "select entity -> open its info dialog".
//
// This is the reimplementation body behind fn_EditTool_SelectEntity (engine_funcs).
// standalone/reg_edittool_select.cpp registers 0x1400fd3b0 -> EditTool_SelectEntity_Impl,
// so EditTool::OnKey's click-to-select path (edittool.cpp:517) opens the entity dialog.
//
// Original behavior (verified against the 0x1400fd3b0 DISASSEMBLY, not just the
// decompiler):
//   1. CheckPtr(entity); if self->+0x30 (last-selected) == *entity -> return.
//   2. Entity_impl_cc90(self) — close the previously-open panel.
//   3. ui = *(gameWorld + 0x98).
//   4. Walk the open-dialog list (GetFirstDialog; node[0]=dialog IBP, node+0x10=next).
//      For each dialog: GetController(dialog, &ctrl, EntityUI-typeid); if the controller
//      derives from EntityUI and its entity ref (+0x30) == *entity -> reuse this dialog.
//   5. Else scan the entity's description container (*(entity+0x28); array at +0x28,
//      count at +0x30) for the UIDescription child. Find the entity's UIComponent
//      (Entity_helper_db60 -> ctrlData/ctrlCount), MemAlloc(0xB8), construct the
//      DialogInstance from the UIDescription's external resource (*(obj+0x28)), overwrite
//      the vtable with g_ToolUI_ChildDialog_Vtable, then AddDialog.
//   6. Finalize: self+0x28 = dialog, self+0x30 = *entity, Entity_impl_c9e0(self).
//
// ABI notes (from disassembly):
//   [0x140151ca0] GetFirstDialog: member, RCX=this RDX=retbuf(node*). Its whole body is
//                                 `*retbuf = this->_dialogs._head`, so we now iterate
//                                 UI::UserInterface::_dialogs directly (B.3.2).
//   [0x140151768] GetController : member, RCX=dialog RDX=retbuf R8=typeId (Ghidra drops R8);
//                                 R8 = DWORD_1401c4f00 (EntityUI) -> fn_DialogInstance_GetController
//   [0x140151518] DialogInstance ctor: RCX=self RDX=ui R8=extRes R9=&parentIBP
//                                 [RSP+0x20]=entityPtr [RSP+0x28]=ctrlData [RSP+0x30]=ctrlCount
//                                 -> fn_DialogInstance_Ctor(self,ui,extRes,parent,dataSource,extraPtr,flags)
//   [0x140151730] AddDialog: RCX=ui RDX=&ref -> fn_UI_AddDialog
//   vtable overwrite LEA 0x14015f5a0 -> g_ToolUI_ChildDialog_Vtable
//
// Type ids (ENDGAME B.3.9 CB: the class statics of the cells retail reads):
//   DWORD_1401c4f00 = EntityUI     (writer: RegisterType_EntityUI)
//   DWORD_1401c7f90 = UIDescription
//   DAT_1401c7fc8   = UIComponent  (writer: UIComponent_RegisterType, used by db60)

#include "engine_funcs.h"
#include "Player/exe_bodies.h"   // B3R: HighlightClear_Impl, HighlightSelect_Impl
#include "Component/UIComponent.h"     // ENDGAME B.3.9 CB: 0x1401c7fc8 cell class
#include "Component/UIDescription.h"   // ENDGAME B.3.9 CB: 0x1401c7f90 cell class
#include "Dialog/EntityUI.h"           // ENDGAME B.3.9 CB: 0x1401c4f00 cell class
#include "Dialog/ToolUI.h"             // ENDGAME B.3.9 CB: 0x1401c5788 cell class
#include "Object/factory_registration.h"
#include "ui/UserInterface.h"
#include <cstdint>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B
#include "runtime/types/ui/DialogInstance.h"  // ENDGAME B.3.6 UI callers (K3)
#include "runtime/types/ui/UserInterface.h"  // ENDGAME B.3.6 UI callers (K3)
#include <new>  // ENDGAME B.3.6 UI callers (K3)
#include "Dialog/exe_bodies.h"
#include "system/MemoryAllocator.h"   // B3R: the allocator doors

namespace {

// ---- helper exe function pointers -------------------------------------------
// Entity_impl_cc90 (0x1400fcc90) — deselect: clear the previous entity's outline
//   (Entity_impl_cb40) + close the previously-open selection panel.
// Entity_impl_c9e0 (0x1400fc9e0) — finalize: mark the entity's UIComponents dirty
//   (+0x11c) and repaint the selection overlay via Entity_impl_c4a0.
// Both are ported in code/Game/Player/tool_base_render.cpp (0x1400fcb40
// HighlightClear_Impl clears the highlight on self+0x30, 0x1400fc9e0
// HighlightSelect_Impl) and are called by name (B3R). RegisterPlayerRenderBatch
// registers them unconditionally (the BANISHED_SELECTION_OUTLINE gate is gone), so
// the old null-guarded skip never ran.

typedef uint32_t (*GetTypeId_t)(void* obj);

// ---- Entity_helper_db60 (0x14002db60), ported inline -------------------------
// Find the entity's UIComponent (type-id DAT_1401c7fc8) in its component list
// (entity+0x30 = array of component IBPs, entity+0x38 = count). Writes the found
// component (or null) into *outIBP as an InternalBasePtr. Faithful 1:1 (offsets
// verified: FUN_140004740 uses the same entity+0x30/+0x38 component-list layout).
void EntityFindUIComponent(uint64_t entity, uint64_t* outIBP) {
    const uint32_t tidUIComp = UIComponent::TypeId();   // cell 0x1401c7fc8
    void* found = nullptr;
    int count = *reinterpret_cast<int32_t*>(entity + 0x38);
    for (int i = 0; i < count; i++) {
        uint64_t* slot = reinterpret_cast<uint64_t*>(
            *reinterpret_cast<uintptr_t*>(entity + 0x30) + static_cast<int64_t>(i) * 8);
        void* comp = *reinterpret_cast<void**>(slot);
        uint32_t tid = reinterpret_cast<Object::Base*>(comp)->Type();  // B.3.8 bridge
        if (tid == tidUIComp) {
            uint64_t* slot2 = reinterpret_cast<uint64_t*>(
                *reinterpret_cast<uintptr_t*>(entity + 0x30) + static_cast<int64_t>(i) * 8);
            void* comp2 = *reinterpret_cast<void**>(slot2);
            if (::Object::IsDerivedFromNullSafe(comp2, tidUIComp)) found = comp2;
            break;
        }
    }
    new (outIBP) ::Object::InternalBasePtr(
        reinterpret_cast<RtObject::Internal*>(found));   // B.3.3 bridge
}

}  // namespace

// ============================================================================
// Entity_impl_d3b0 (0x1400fd3b0) — EditTool::SelectEntity
//   self       = EditTool* (RCX)
//   entityIBPv = InternalBasePtr* to the selected entity (RDX); *entityIBPv = entity ptr
// ============================================================================
extern "C" void* g_LastSelectStore[4] = {};
extern "C" int   g_LastSelectStoreIdx = 0;

extern "C" void EditTool_SelectEntity_Impl(void* selfV, void* entityIBPv) {
    uintptr_t self = reinterpret_cast<uintptr_t>(selfV);
    uint64_t* entityIBP = reinterpret_cast<uint64_t*>(entityIBPv);

    // 1. CheckPtr(entity); early-out if this entity is already the selected one.
    if (*reinterpret_cast<uint64_t*>(self + 0x30) == *entityIBP) return;

    // 2. (Retail calls Entity_impl_cc90 here to tear down the previous panel +
    //    clear its outline. We DON'T, because our EditTool::OnKey pick handler
    //    (edittool.cpp:629) re-runs this select PER FRAME while the pick button
    //    reads "pressed", firing for whatever entity is under the cursor. Clearing
    //    the old outline here would then wipe the current selection every frame the
    //    cursor drifts off it — e.g. a walking citizen leaves its outline flickering
    //    off. Instead we clear the previous outline at step 6, which is only reached
    //    when a NEW selection actually completes (a dialog is opening). A hover over
    //    a no-dialog entity returns early below and leaves the current outline intact.)

    // 3. UserInterface = *(gameWorld + 0x98).
    void* gameWorld = *g_pGameWorldSingleton;
    UI::UserInterface* ui = *reinterpret_cast<UI::UserInterface**>(
        reinterpret_cast<uintptr_t>(gameWorld) + 0x98);

    // Result-dialog IBP holder (local_res18), initialized null.
    uint64_t resultDialog = 0;
    new (&resultDialog) ::Object::InternalBasePtr(nullptr);   // B.3.3 bridge

    // 4. Walk the open-dialog list for an existing EntityUI dialog bound to this entity.
    const uint32_t tidEntityUI = EntityUI::TypeId();   // cell 0x1401c4f00
    // `_dialogs.GetFirst()` IS retail's GetFirstDialog (0x180031150 = `*retval =
    // this->_dialogs._head`); the iterator is the bare node pointer, so `it.node`
    // is the address retail passes to CheckPtr and `it.node->element` is node[0].
    for (auto it = ui->_dialogs.GetFirst(); it; ++it) {
        void* dialog = it.node->element;

        // GetController(dialog, &ctrlBuf, EntityUI); filter by IsDerivedFrom.
        uint64_t ctrlBuf = 0;
        void* matched = nullptr;
        if (dialog)
            ::new (&ctrlBuf) Object::InternalPtr<UI::DialogController>(reinterpret_cast<const UI::DialogInstance*>(dialog)->GetController(tidEntityUI));  // B.3.6 bridge
        if (ctrlBuf != 0) {
            void* c = reinterpret_cast<void*>(ctrlBuf);
            if (::Object::IsDerivedFromNullSafe(c, tidEntityUI)) matched = c;
        }

        // Wrap the (possibly null) controller in an IBP, resolve, compare +0x30 to *entity.
        uint64_t ctrlRef = 0;
        new (&ctrlRef) ::Object::InternalBasePtr(
            reinterpret_cast<RtObject::Internal*>(matched));   // B.3.3 bridge
        if (ctrlRef != 0) {
            uint64_t ctrlEntity = *reinterpret_cast<uint64_t*>(ctrlRef + 0x30);
            if (ctrlEntity == *entityIBP) {
                resultDialog = reinterpret_cast<uint64_t>(it.node->element);  // *iter = dialog IBP value
                break;
            }
        }
    }

    if (resultDialog == 0) {
        // 5. No existing dialog — scan the entity's description container for the
        //    UIDescription child, then build a fresh DialogInstance.
        uintptr_t descList = *reinterpret_cast<uintptr_t*>(*entityIBP + 0x28);  // *(entity+0x28)
        int count = *reinterpret_cast<int32_t*>(descList + 0x30);
        const uint32_t tidUIDesc = UIDescription::TypeId();   // cell 0x1401c7f90

        void* obj = nullptr;
        for (int i = 0; i < count; i++) {
            void* item = *reinterpret_cast<void**>(
                *reinterpret_cast<uintptr_t*>(descList + 0x28) + static_cast<int64_t>(i) * 8);
            uint32_t tid = reinterpret_cast<Object::Base*>(item)->Type();  // B.3.8 bridge
            if (tid != tidUIDesc) continue;
            // Matched: re-fetch, require derived + non-null, else abort the whole select.
            obj = *reinterpret_cast<void**>(
                *reinterpret_cast<uintptr_t*>(descList + 0x28) + static_cast<int64_t>(i) * 8);
            if (!(::Object::IsDerivedFromNullSafe(obj, tidUIDesc))) return;
            if (obj == nullptr) return;
            break;
        }
        if (obj == nullptr) return;  // no UIDescription in the list -> nothing to open

        // Find the entity's UIComponent (supplies the controller data array).
        uint64_t uiCompIBP = 0;
        EntityFindUIComponent(*entityIBP, &uiCompIBP);

        void* dlg = System::MemoryAllocator::MemAlloc(0xB8, 8);
        void* newDialog = nullptr;
        if (dlg != nullptr) {
            void* ctrlData = nullptr;
            uint32_t ctrlCount = 0;
            if (uiCompIBP != 0) {
                ctrlCount = *reinterpret_cast<uint32_t*>(uiCompIBP + 0x30);
                ctrlData = *reinterpret_cast<void**>(uiCompIBP + 0x28);
            }
            void* entityPtr = *reinterpret_cast<void**>(entityIBP);  // dataSource (ctor arg5)
            void* extRes = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(obj) + 0x28);
            // ENDGAME B.3.9 UI-D (B9-13 foreign site): the real 6-arg ctor
            // (0x180114870); its compiler table is the one retail's copy at
            // 0x14015f5a0 describes, so the hand stamp is gone.
            newDialog = ::new (dlg) UI::DialogInstance(
                ui,
                reinterpret_cast<const UI::Dialog*>(extRes),             // B.3.8 bridge
                Object::InternalPtr<UI::DialogInstance>(),               // parent: null
                reinterpret_cast<Object::Base*>(entityPtr),              // B.3.8 bridge
                ctrlData, static_cast<int32_t>(ctrlCount));
        }

        // Wrap in IBP; if null (alloc failed) -> abort before finalize.
        uint64_t dialogRef = 0;
        new (&dialogRef) ::Object::InternalBasePtr(
            reinterpret_cast<RtObject::Internal*>(newDialog));   // B.3.3 bridge
        resultDialog = dialogRef;
        if (dialogRef == 0) return;

        uint64_t addRef = dialogRef;
        reinterpret_cast<UI::UserInterface*>(ui)->AddDialog(reinterpret_cast<Object::InternalPtr<UI::DialogInstance>*>(&addRef));  // B.3.6 bridge
    }

    // 6. Finalize. We're now committed to selecting `entity` (a dialog is opening),
    //    so THIS is the point to clear the previous entity's outline — cb40 clears
    //    the highlight on self+0x30, which is still the OLD entity until the store
    //    below. (Moved off the per-frame step-2 cc90 path so hover-picks over
    //    no-dialog entities can't wipe the current selection; see step 2.)
    HighlightClear_Impl(selfV);   // 0x1400fcb40

    // 6a. Close the PREVIOUSLY-open info dialog (self+0x28) before replacing it with
    //     the new one. This is retail cc90's third action (see Entity_impl_cc90 @
    //     0x1400fcc90): GetController(oldDialog, ToolUI); if the controller isn't
    //     "pinned" (ctrl+0x40 == 0) -> UI::CloseDialog. Without this the old panel
    //     lingers and every new building/citizen click stacks another dialog, which
    //     reads exactly like the pin option being stuck on. We do ONLY the close
    //     here (not cc90's ref-nulling): self+0x28/+0x30 are overwritten immediately
    //     below, so nulling them would just defeat the EditTool::OnKey per-frame
    //     `lastEntityRef != foundEntity` guard and re-fire the select every frame
    //     (the documented citizen-dialog flicker). The +0x40 pin flag is honored, so
    //     a user-pinned panel is left open — matching retail.
    void* oldDialog = *reinterpret_cast<void**>(self + 0x28);
    if (oldDialog && oldDialog != reinterpret_cast<void*>(resultDialog)) {
        const uint32_t tidToolUI = ToolUI::TypeId();   // cell 0x1401c5788
        uint64_t ctrlBuf = 0;
        ::new (&ctrlBuf) Object::InternalPtr<UI::DialogController>(reinterpret_cast<const UI::DialogInstance*>(oldDialog)->GetController(tidToolUI));  // B.3.6 bridge
        void* ctrl = nullptr;
        if (ctrlBuf != 0) {
            void* c = reinterpret_cast<void*>(ctrlBuf);
            if (::Object::IsDerivedFromNullSafe(c, tidToolUI)) ctrl = c;
        }
        uint64_t ctrlRef = 0;
        new (&ctrlRef) ::Object::InternalBasePtr(
            reinterpret_cast<RtObject::Internal*>(ctrl));   // B.3.3 bridge
        // Not pinned (ctrl+0x40 == 0) -> close it. (A null controller -> ctrlRef 0,
        // reading +0x40 off 0 would fault, so require a resolved controller first.)
        if (ctrlRef != 0 && *reinterpret_cast<char*>(ctrlRef + 0x40) == 0) {
            uint64_t closeRef = reinterpret_cast<uint64_t>(oldDialog);
            reinterpret_cast<UI::UserInterface*>(ui)->CloseDialog(reinterpret_cast<Object::InternalPtr<UI::DialogInstance>*>(&closeRef));  // B.3.6 bridge
        }
    }

    *reinterpret_cast<uint64_t*>(self + 0x28) = resultDialog;
    // [diag] ring of the last 4 entities this store committed — read back by
    // tool_base_render.cpp's ReportBadSelectionRef when cb40 finds a non-canonical
    // ref, to tell "a bad value was stored here" from "self isn't an EditTool".
    g_LastSelectStore[g_LastSelectStoreIdx & 3] = reinterpret_cast<void*>(*entityIBP);
    g_LastSelectStoreIdx = (g_LastSelectStoreIdx + 1) & 3;
    *reinterpret_cast<uint64_t*>(self + 0x30) = *entityIBP;
    HighlightSelect_Impl(selfV);   // 0x1400fc9e0
}
