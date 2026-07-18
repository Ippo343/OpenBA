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
//   [0x140151ca0] GetFirstDialog: member, RCX=this RDX=retbuf(node*) -> fn_UI_GetFirstDialog
//   [0x140151768] GetController : member, RCX=dialog RDX=retbuf R8=typeId (Ghidra drops R8);
//                                 R8 = DWORD_1401c4f00 (EntityUI) -> fn_DialogInstance_GetController
//   [0x140151518] DialogInstance ctor: RCX=self RDX=ui R8=extRes R9=&parentIBP
//                                 [RSP+0x20]=entityPtr [RSP+0x28]=ctrlData [RSP+0x30]=ctrlCount
//                                 -> fn_DialogInstance_Ctor(self,ui,extRes,parent,dataSource,extraPtr,flags)
//   [0x140151730] AddDialog: RCX=ui RDX=&ref -> fn_UI_AddDialog
//   vtable overwrite LEA 0x14015f5a0 -> g_ToolUI_ChildDialog_Vtable
//
// Type-id .rdata reads (NULL in standalone -> filled lazily via Object::GetTypeIdByName,
// the proven toolbardialog.cpp/Toolbar_Utility_Impl pattern):
//   DWORD_1401c4f00 = EntityUI     (writer: RegisterType_EntityUI)
//   DWORD_1401c7f90 = UIDescription
//   DAT_1401c7fc8   = UIComponent  (writer: UIComponent_RegisterType, used by db60)

#include "engine_funcs.h"
#include "resolve.h"
#include "Object/factory_registration.h"
#include <cstdint>

namespace {

// ---- helper exe function pointers -------------------------------------------
// Entity_impl_cc90 (0x1400fcc90) — deselect: clear the previous entity's outline
//   (Entity_impl_cb40) + close the previously-open selection panel.
// Entity_impl_c9e0 (0x1400fc9e0) — finalize: mark the entity's UIComponents dirty
//   (+0x11c) and repaint the selection overlay via Entity_impl_c4a0.
// Both are ported in code/Game/Player/tool_base_render.cpp and registered by
// RegisterPlayerRenderBatch — but ONLY when BANISHED_SELECTION_OUTLINE is set
// (their closure transits the EdgeMesh render-mesh family, which mutates the live
// Graphics::Scene). Resolved here as direct exe pointers: our ports in
// standalone-with-outline, NULL in standalone-default -> null-guarded skip.
// Effect when skipped (outline off, the shipping default): the info dialog still
// OPENS; only the previous-panel teardown + selection-highlight repaint are skipped.
typedef void (*fn_void_self_t)(void* self);
fn_void_self_t s_fn_cb40 = nullptr;   // 0x1400fcb40 (clear highlight on self+0x30)
fn_void_self_t s_fn_c9e0 = nullptr;   // 0x1400fc9e0
bool s_resolved = false;

uint32_t s_tid_EntityUI = 0;       // DWORD_1401c4f00
uint32_t s_tid_UIDescription = 0;  // DWORD_1401c7f90
uint32_t s_tid_UIComponent = 0;    // DAT_1401c7fc8
uint32_t s_tid_ToolUI = 0;         // DWORD_1401c5788 (cc90 close-prev-dialog gate)

void EnsureFuncs() {
    if (s_resolved) return;
    s_fn_cb40 = reinterpret_cast<fn_void_self_t>(ResolveAddr(Module::Exe, 0x1400fcb40));
    s_fn_c9e0 = reinterpret_cast<fn_void_self_t>(ResolveAddr(Module::Exe, 0x1400fc9e0));
    s_resolved = true;
}

// Resolve a class type-id: prefer the raw .rdata DWORD; in the standalone exe that
// pointer is NULL, so fall back to the live factory tree.
uint32_t ResolveTypeId(uintptr_t rdataAddr, const char* name, uint32_t* cache) {
    if (*cache) return *cache;
    uint32_t* p = reinterpret_cast<uint32_t*>(ResolveAddr(Module::Exe, rdataAddr));
    if (p && *p) { *cache = *p; return *cache; }
    *cache = ::Object::GetTypeIdByName(name);
    return *cache;
}

typedef uint32_t (*GetTypeId_t)(void* obj);

// ---- Entity_helper_db60 (0x14002db60), ported inline -------------------------
// Find the entity's UIComponent (type-id DAT_1401c7fc8) in its component list
// (entity+0x30 = array of component IBPs, entity+0x38 = count). Writes the found
// component (or null) into *outIBP as an InternalBasePtr. Faithful 1:1 (offsets
// verified: FUN_140004740 uses the same entity+0x30/+0x38 component-list layout).
void EntityFindUIComponent(uint64_t entity, uint64_t* outIBP) {
    const uint32_t tidUIComp = ResolveTypeId(0x1401c7fc8, "UIComponent", &s_tid_UIComponent);
    void* found = nullptr;
    int count = *reinterpret_cast<int32_t*>(entity + 0x38);
    for (int i = 0; i < count; i++) {
        uint64_t* slot = reinterpret_cast<uint64_t*>(
            *reinterpret_cast<uintptr_t*>(entity + 0x30) + static_cast<int64_t>(i) * 8);
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(slot);
        void* comp = *reinterpret_cast<void**>(slot);
        uint32_t tid = reinterpret_cast<GetTypeId_t>(
            *reinterpret_cast<uintptr_t*>(*reinterpret_cast<uintptr_t*>(comp) + 0x10))(comp);
        if (tid == tidUIComp) {
            uint64_t* slot2 = reinterpret_cast<uint64_t*>(
                *reinterpret_cast<uintptr_t*>(entity + 0x30) + static_cast<int64_t>(i) * 8);
            if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(slot2);
            void* comp2 = *reinterpret_cast<void**>(slot2);
            if (fn_IsDerivedFrom && fn_IsDerivedFrom(comp2, tidUIComp)) found = comp2;
            break;
        }
    }
    if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor(outIBP, found);
}

}  // namespace

// ============================================================================
// Entity_impl_d3b0 (0x1400fd3b0) — EditTool::SelectEntity
//   self       = EditTool* (RCX)
//   entityIBPv = InternalBasePtr* to the selected entity (RDX); *entityIBPv = entity ptr
// ============================================================================
extern "C" void EditTool_SelectEntity_Impl(void* selfV, void* entityIBPv) {
    EnsureFuncs();
    uintptr_t self = reinterpret_cast<uintptr_t>(selfV);
    uint64_t* entityIBP = reinterpret_cast<uint64_t*>(entityIBPv);

    // 1. CheckPtr(entity); early-out if this entity is already the selected one.
    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(entityIBP);
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
    void* ui = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(gameWorld) + 0x98);

    // Result-dialog IBP holder (local_res18), initialized null.
    uint64_t resultDialog = 0;
    if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor(&resultDialog, nullptr);

    // 4. Walk the open-dialog list for an existing EntityUI dialog bound to this entity.
    const uint32_t tidEntityUI = ResolveTypeId(0x1401c4f00, "EntityUI", &s_tid_EntityUI);
    uint64_t iterNode = 0;
    if (fn_UI_GetFirstDialog) fn_UI_GetFirstDialog(ui, &iterNode);
    while (iterNode != 0) {
        uint64_t* node = reinterpret_cast<uint64_t*>(iterNode);
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(node);
        void* dialog = *reinterpret_cast<void**>(node);   // node[0]

        // GetController(dialog, &ctrlBuf, EntityUI); filter by IsDerivedFrom.
        uint64_t ctrlBuf = 0;
        void* matched = nullptr;
        if (dialog && fn_DialogInstance_GetController)
            fn_DialogInstance_GetController(dialog, &ctrlBuf, tidEntityUI);
        if (ctrlBuf != 0) {
            if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&ctrlBuf);
            void* c = reinterpret_cast<void*>(ctrlBuf);
            if (fn_IsDerivedFrom && fn_IsDerivedFrom(c, tidEntityUI)) matched = c;
        }

        // Wrap the (possibly null) controller in an IBP, resolve, compare +0x30 to *entity.
        uint64_t ctrlRef = 0;
        if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor(&ctrlRef, matched);
        if (ctrlRef != 0) {
            if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&ctrlRef);
            uint64_t ctrlEntity = *reinterpret_cast<uint64_t*>(ctrlRef + 0x30);
            if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(entityIBP);
            if (ctrlEntity == *entityIBP) {
                resultDialog = *reinterpret_cast<uint64_t*>(node);  // *iter = dialog IBP value
                break;
            }
        }
        iterNode = *reinterpret_cast<uint64_t*>(iterNode + 0x10);   // next (node+0x10)
    }

    if (resultDialog == 0) {
        // 5. No existing dialog — scan the entity's description container for the
        //    UIDescription child, then build a fresh DialogInstance.
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(entityIBP);
        uintptr_t descList = *reinterpret_cast<uintptr_t*>(*entityIBP + 0x28);  // *(entity+0x28)
        int count = *reinterpret_cast<int32_t*>(descList + 0x30);
        const uint32_t tidUIDesc = ResolveTypeId(0x1401c7f90, "UIDescription", &s_tid_UIDescription);

        void* obj = nullptr;
        for (int i = 0; i < count; i++) {
            void* item = *reinterpret_cast<void**>(
                *reinterpret_cast<uintptr_t*>(descList + 0x28) + static_cast<int64_t>(i) * 8);
            uint32_t tid = reinterpret_cast<GetTypeId_t>(
                *reinterpret_cast<uintptr_t*>(*reinterpret_cast<uintptr_t*>(item) + 0x10))(item);
            if (tid != tidUIDesc) continue;
            // Matched: re-fetch, require derived + non-null, else abort the whole select.
            obj = *reinterpret_cast<void**>(
                *reinterpret_cast<uintptr_t*>(descList + 0x28) + static_cast<int64_t>(i) * 8);
            if (!(fn_IsDerivedFrom && fn_IsDerivedFrom(obj, tidUIDesc))) return;
            if (obj == nullptr) return;
            break;
        }
        if (obj == nullptr) return;  // no UIDescription in the list -> nothing to open

        // Find the entity's UIComponent (supplies the controller data array).
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(entityIBP);
        uint64_t uiCompIBP = 0;
        EntityFindUIComponent(*entityIBP, &uiCompIBP);

        void* dlg = fn_MemAlloc(g_pMemAllocSingleton, 0xB8, 8);
        void* newDialog = nullptr;
        if (dlg != nullptr) {
            void* ctrlData = nullptr;
            uint32_t ctrlCount = 0;
            if (uiCompIBP != 0) {
                if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&uiCompIBP);
                ctrlCount = *reinterpret_cast<uint32_t*>(uiCompIBP + 0x30);
                if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&uiCompIBP);
                ctrlData = *reinterpret_cast<void**>(uiCompIBP + 0x28);
            }
            if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(entityIBP);
            void* entityPtr = *reinterpret_cast<void**>(entityIBP);  // dataSource (ctor arg5)
            void* extRes = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(obj) + 0x28);
            uint64_t nullParent = 0;
            if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor(&nullParent, nullptr);
            fn_DialogInstance_Ctor(dlg, ui, extRes, &nullParent, entityPtr, ctrlData,
                                   static_cast<int>(ctrlCount));
            *reinterpret_cast<uintptr_t*>(dlg) = g_ToolUI_ChildDialog_Vtable;
            newDialog = dlg;
        }

        // Wrap in IBP; if null (alloc failed) -> abort before finalize.
        uint64_t dialogRef = 0;
        if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor(&dialogRef, newDialog);
        resultDialog = dialogRef;
        if (dialogRef == 0) return;

        uint64_t addRef = dialogRef;
        if (fn_UI_AddDialog) fn_UI_AddDialog(ui, &addRef);
    }

    // 6. Finalize. We're now committed to selecting `entity` (a dialog is opening),
    //    so THIS is the point to clear the previous entity's outline — cb40 clears
    //    the highlight on self+0x30, which is still the OLD entity until the store
    //    below. (Moved off the per-frame step-2 cc90 path so hover-picks over
    //    no-dialog entities can't wipe the current selection; see step 2.)
    if (s_fn_cb40) s_fn_cb40(selfV);

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
    if (oldDialog && oldDialog != reinterpret_cast<void*>(resultDialog) && fn_UI_CloseDialog) {
        const uint32_t tidToolUI = ResolveTypeId(0x1401c5788, "ToolUI", &s_tid_ToolUI);
        uint64_t ctrlBuf = 0;
        if (fn_DialogInstance_GetController)
            fn_DialogInstance_GetController(oldDialog, &ctrlBuf, tidToolUI);
        void* ctrl = nullptr;
        if (ctrlBuf != 0) {
            if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&ctrlBuf);
            void* c = reinterpret_cast<void*>(ctrlBuf);
            if (fn_IsDerivedFrom && fn_IsDerivedFrom(c, tidToolUI)) ctrl = c;
        }
        uint64_t ctrlRef = 0;
        if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor(&ctrlRef, ctrl);
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&ctrlRef);
        // Not pinned (ctrl+0x40 == 0) -> close it. (A null controller -> ctrlRef 0,
        // reading +0x40 off 0 would fault, so require a resolved controller first.)
        if (ctrlRef != 0 && *reinterpret_cast<char*>(ctrlRef + 0x40) == 0) {
            uint64_t closeRef = reinterpret_cast<uint64_t>(oldDialog);
            fn_UI_CloseDialog(ui, &closeRef);
        }
    }

    *reinterpret_cast<uint64_t*>(self + 0x28) = resultDialog;
    *reinterpret_cast<uint64_t*>(self + 0x30) = *entityIBP;
    if (s_fn_c9e0) s_fn_c9e0(selfV);
}
