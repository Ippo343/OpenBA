// code/Game/Dialog/toolbar_utility_helpers.cpp
// GUI_PLAN Keystone F, Phase 4 — Utility dialogs (toolbar case 2).
//
// Two exe helpers that Toolbar_Utility_Impl (toolbardialog.cpp, FUN_140101a80)
// depends on, ported so ResolveAddr(Module::Exe, addr) resolves them in the
// standalone exe. Both are unregistered otherwise → the engine_funcs pointers
// fn_FindDialogByControllerType / fn_BST_Insert_0x14 stay NULL → the very first
// calls in Toolbar_Utility_Impl are null-calls (EXEC of 0).
//
//   FUN_140100270  FindDialogByControllerType — walk the live UI dialog list and
//     return the open dialog whose ToolUI controller's +0x30 (its Description
//     pointer) == param. Used to detect an already-open utility dialog (toggle).
//   FUN_140014890  BST_Insert_0x14 — AVL find-or-insert of a 0x38-byte
//     utility-dialog position node keyed by the Description ptr; rebalances via
//     the already-registered retrace 0x140014b40.
//
// Faithful 1:1 against the Ghidra disassembly (offsets verified). Registered via
// standalone/reg_toolbar_utility.cpp.

#include "resolve.h"
#include "engine_funcs.h"
#include "Object/factory_registration.h"
#include "ui/UserInterface.h"
#include <cstdint>

namespace {

// Retrace/rebalance (0x140014b40, TradeWork_impl_4b40) — shared AVL retrace,
// registered by reg_keystone_a_restree.cpp. Resolve it as a plain exe function
// pointer (direct, not a dispatch slot) so this works in the standalone exe.
typedef void (*BstRebalance_t)(int64_t* rootHolder, int64_t* insertedNodeSlot);
BstRebalance_t s_fn_bstRebalance = nullptr;

void EnsureFuncs() {
    if (!s_fn_bstRebalance) {
        s_fn_bstRebalance =
            reinterpret_cast<BstRebalance_t>(ResolveAddr(Module::Exe, 0x140014b40));
    }
}

// sub_4a90 — allocate + init a new 0x38-byte AVL node (copies the 3 data qwords,
// sets parent, zeroes children + balance).
uint64_t* BstNodeAlloc(uint64_t* insertData, uint64_t parent) {
    uint64_t* n = reinterpret_cast<uint64_t*>(fn_MemAlloc(g_pMemAllocSingleton, 0x38, 8));
    if (!n) return nullptr;
    n[0] = insertData[0];                 // key (Description ptr)
    n[1] = insertData[1];                 // packed (x,y)
    n[2] = insertData[2];                 // flags
    n[3] = parent;                        // +0x18 parent
    n[4] = 0;                             // +0x20 left
    n[5] = 0;                             // +0x28 right
    *reinterpret_cast<uint32_t*>(n + 6) = 0;  // +0x30 balance
    return n;
}

}  // namespace

extern "C" {

// ---- FUN_140100270: FindDialogByControllerType ------------------------------
// (RCX=toolController[unused], RDX=retBuf, R8=param = Description ptr)
// Writes *retBuf = matching DialogInstance* (or null). Returns void (callers read
// retBuf; the exe returns retBuf in RAX, harmlessly ignored here).
void ToolbarUtility_FindDialogByControllerType_0270(void* /*toolController*/,
                                                    void* retBuf, uint64_t param) {
    // Init retBuf InternalBasePtr to null (matches the exe's leading IBP_Ctor).
    if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor(retBuf, nullptr);

    void* gameWorld = g_pGameWorldSingleton ? *g_pGameWorldSingleton : nullptr;
    if (!gameWorld) return;
    UI::UserInterface* ui = *reinterpret_cast<UI::UserInterface**>(
        reinterpret_cast<uintptr_t>(gameWorld) + 0x98);
    if (!ui) return;

    // ToolUI type-id (DWORD_1401c5788, RegisterType_ToolUI writer). Raw .rdata →
    // NULL in standalone; fill from the live factory tree. GetTypeIdByName is a
    // no-op cost after the first hit (cached in the class's own path); cache here.
    static uint32_t s_toolUITid = 0;
    if (s_toolUITid == 0) s_toolUITid = ::Object::GetTypeIdByName("ToolUI");
    const uint32_t toolUITid = s_toolUITid;

    // Walk the dialog linked list. `_dialogs.GetFirst()` IS retail's
    // GetFirstDialog (0x180031150 = `*retval = this->_dialogs._head`), and the
    // iterator is the bare node pointer, so `it.node` is the address retail
    // hands to CheckPtr and `it.node->element` is the DialogInstance IBP.
    for (auto it = ui->_dialogs.GetFirst(); it; ++it) {
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(it.node);
        void* dialog = it.node->element;

        // GetController(dialog, &ctrlBuf, ToolUI tid) — R8=type-id filters internally.
        void* controller = nullptr;
        uintptr_t ctrlBuf[2] = {};
        if (dialog && fn_DialogInstance_GetController)
            fn_DialogInstance_GetController(dialog, ctrlBuf, toolUITid);
        if (ctrlBuf[0] != 0) {
            if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(ctrlBuf);
            void* c = reinterpret_cast<void*>(ctrlBuf[0]);
            if (fn_IsDerivedFrom && fn_IsDerivedFrom(c, toolUITid)) controller = c;
        }

        // Wrap the (possibly null) controller in an IBP, resolve, compare +0x30.
        uintptr_t ctrlRef = 0;
        if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor(&ctrlRef, controller);
        if (ctrlRef != 0) {
            if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&ctrlRef);
            if (*reinterpret_cast<uint64_t*>(ctrlRef + 0x30) == param) {
                *reinterpret_cast<void**>(retBuf) = it.node->element;
                return;
            }
        }
    }
}

// ---- FUN_140014890: BST_Insert_0x14 ----------------------------------------
// (RCX=rootHolder, RDX=retNode, R8=insertData)
//   rootHolder: [0]=root node ptr, [+8] int32 count
//   retNode:    *retNode = the found-or-inserted node
//   insertData: [0]=key(Description ptr), [1]=packed(x,y), [2]=flags
// Faithful AVL find-or-insert; rebalances from the insertion point via 0x140014b40.
void ToolbarUtility_BstInsert_4890(int64_t* rootHolder, void** retNode, uint64_t* insertData) {
    EnsureFuncs();

    if (*rootHolder == 0) {
        // Empty tree — the new node becomes the root.
        uint64_t* n = reinterpret_cast<uint64_t*>(fn_MemAlloc(g_pMemAllocSingleton, 0x38, 8));
        if (!n) {
            *rootHolder = 0;
            *retNode = nullptr;
        } else {
            n[0] = insertData[0];
            n[1] = insertData[1];
            n[2] = insertData[2];
            n[3] = 0; n[4] = 0; n[5] = 0;
            *reinterpret_cast<uint32_t*>(n + 6) = 0;
            *retNode = n;
            *rootHolder = reinterpret_cast<int64_t>(n);
        }
    } else {
        const uint64_t key = insertData[0];
        uint64_t* next = reinterpret_cast<uint64_t*>(*rootHolder);
        uint64_t* node = nullptr;      // current node examined (== insertion parent)
        uint64_t* newNode = nullptr;
        bool inserted = false;

        while (!inserted) {
            node = next;
            // Walk right while node->key < key.
            while (*node <= key) {
                if (key <= *node) {          // equal → existing node found (no count++)
                    *retNode = node;
                    return;
                }
                if (node[5] == 0) {          // right child null → insert right
                    newNode = BstNodeAlloc(insertData, reinterpret_cast<uint64_t>(node));
                    node[5] = reinterpret_cast<uint64_t>(newNode);
                    inserted = true;
                    break;
                }
                next = reinterpret_cast<uint64_t*>(node[5]);
                node = next;
            }
            if (inserted) break;
            // node->key > key → go left.
            if (node[4] == 0) {              // left child null → insert left
                newNode = BstNodeAlloc(insertData, reinterpret_cast<uint64_t>(node));
                node[4] = reinterpret_cast<uint64_t>(newNode);
                inserted = true;
                break;
            }
            next = reinterpret_cast<uint64_t*>(node[4]);
        }

        if (s_fn_bstRebalance) s_fn_bstRebalance(rootHolder, reinterpret_cast<int64_t*>(&node));
        *retNode = newNode;
    }

    // count (rootHolder[+8], int32) — incremented on every non-early-return path,
    // matching the exe (even the alloc-failure sub-case).
    *reinterpret_cast<int32_t*>(reinterpret_cast<uint8_t*>(rootHolder) + 8) += 1;
}

}  // extern "C"
