// code/Game/Tutorial/tutorial_wave.cpp
//
// The second half of the Tutorial ResolveAddr gap: 12 exe-internal helpers the
// already-ported Tutorial module resolves by address and which are registered
// NOWHERE, so every one of them is a null function pointer.
//
// The eight from notes/wave_remaining/lane2_tutorial.txt:
//   0x1400262B0  ExternalPtr<Tutorial>::Serialize        (fn_ExternalPtr_SerializeTutorial)
//   0x140028650  ExternalPtr<UI::Dialog>::Serialize      (fn_SerializeExternalPtr)
//   0x1400E1DB0  FinishTutorialInstance step driver      (fn_TutorialStepExecute)
//   0x1400E2330  TutorialSetupDialog::SetAllDialogFlags  (fn_TSD_SetDialogFlags)
//   0x1400E24F0  TutorialSetupDialog::FindHighlight      (fn_TSD_FindInstance)
//   0x1400E2700  TutorialSetupDialog::OpenHighlight      (fn_TSD_OpenDialog)
//   0x1400E29F0  TutorialSetupDialog::ShowListed         (fn_TSD_Slot9Helper)
//   0x1401001F0  FinishTutorialGoal tutorial-tree insert (fn_TutorialTreeInsert)
//
// plus four sub-callees of those that are equally unregistered and were not on
// the list (closure verified in Ghidra with get_function_callees on every one):
//   0x1400E2250  recursive element show/hide             (called by 2330)
//   0x14001AF90  ExternalRef copy-construct              (called by 2700)
//   0x14001AFE0  ExternalRef resolve + extract           (called by 2700)
//   0x1400E1260  FinishTutorialInstance step->UI refresh (called by 1DB0)
//
// Every address above was verified as a real function ENTRY with
// get_function_by_address (Entry == address) — the mid-function-resolve trap
// ([[reference-midfunction-raw-address-resolves]]) does not apply to any of them.
//
// STILL DARK, deliberately (see the report):
//   0x1400E0D70  "tutorial finished -> launch the scenario" transition. Its own
//                closure pulls in four more unported exe functions
//                (0x140013A60 / 0x140013AA0 / 0x140098060 GameUpdateLoop ctor /
//                0x140099130). TutWave_StepExecute logs and returns instead of
//                calling a null pointer, and says so.
// (0x1400E1870 FindUIElementByPath is now ported — see
//  code/Game/Tutorial/find_ui_element_by_path.cpp. Both call sites here,
//  ShowListed and OpenHighlight's by-path branch, are live.)
//
// Decompiled from Application-x64.exe; control flow checked against the
// disassembly, not just the Ghidra pseudo-C, for 0x1400E01D0 / 0x1400E1260 /
// 0x1400E2700 where the decompiler mis-renders the stack arguments.

#include "engine_funcs.h"
#include "system/String.h"
#include "resolve.h"
#include "Object/factory_registration.h"
#include "Game/serialize_externals.h"
#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <new>

extern "C" void* GetTutorialHighlightDialogVtable();
// Port of exe 0x1400E0D70 -- boots the next scenario when a tutorial finishes.
// Defined in tutorial_next_scenario.cpp.
extern "C" void TutWave_LoadNextScenario(void* self);
extern "C" void* TutWave_HighlightDialogCtor(void* self, void* ui, void* dialogExternal,
                                             void* parentIBP, void* ibpE0,
                                             int32_t valD8, void* ibpE8);

namespace {


// One-shot "this leg of the tutorial is not ported" notice. Distinct from a
// null-check that papers over a faithful crash: these are functions we KNOW we
// have not written, and the alternative is EXEC-of-0.
void DarkOnce(const char* what) {
    static const char* s_seen[8] = {};
    static int n = 0;
    for (int i = 0; i < n; ++i) if (s_seen[i] == what) return;
    if (n < 8) s_seen[n++] = what;
    fprintf(stderr, "[Tutorial] NOT PORTED: %s — this leg of the tutorial stops here\n", what);
    fflush(stderr);
}

inline uintptr_t GameWorld() {
    return g_pGameWorldSingleton ? reinterpret_cast<uintptr_t>(*g_pGameWorldSingleton) : 0;
}
inline void* UserInterface() {
    uintptr_t gw = GameWorld();
    return gw ? *reinterpret_cast<void**>(gw + 0x98) : nullptr;
}

// Runtime exports the Tutorial helpers need that engine_funcs does not carry.
// Mangled names read straight out of Runtime-x64.dll's export table.
typedef int  (*fn_GetAddDialogCount_t)(const void*);
typedef bool (*fn_UIStringIsValid_t)(const void*);
fn_GetAddDialogCount_t s_GetAddDialogCount = nullptr;
fn_UIStringIsValid_t   s_UIStringIsValid   = nullptr;

void EnsureRuntimeFuncs() {
    static bool done = false;
    if (done) return;
    done = true;
    s_GetAddDialogCount = reinterpret_cast<fn_GetAddDialogCount_t>(
        ResolveExport("?GetAddDialogCount@UserInterface@UI@@QEBAHXZ"));
    s_UIStringIsValid = reinterpret_cast<fn_UIStringIsValid_t>(
        ResolveExport("?IsValid@String@UI@@QEBA_NXZ"));
}

// TutorialHighlightDialog type id (retail DAT_1401C6550 — written by
// RegisterType_TutorialHighlightDialog, unregistered in standalone).
uint32_t HighlightTypeId() {
    static uint32_t c = 0;
    if (!c) c = ::Object::ResolveTypeIdByNameLazy("TutorialHighlightDialog");
    return c;
}
// TutorialUI type id (retail DAT_1401C67F0).
uint32_t TutorialUITypeId() {
    static uint32_t c = 0;
    // TutorialUI is an EXE-side REGISTER_TYPE (tutorial_dialog_vtables.cpp:534), so the
    // exe registration table is the authority; ResolveTypeIdByNameLazy walks the live
    // Factory tree and is the fallback, not the primary. Order matters: a 0 here makes
    // StepExecute's controller narrow fail and the step panel is never bound.
    if (!c) c = ::Object::GetTypeIdByName("TutorialUI");
    if (!c) c = ::Object::ResolveTypeIdByNameLazy("TutorialUI");
    return c;
}

// A BST node in the (gameWorld+0xF0)+0x160 map, and in the FinishTutorialGoal
// tree at goal+0x50. Same shape both places:
//   +0x00 key (External* / uint64)   +0x08 payload ptr
//   +0x10 count / value              +0x20 left   +0x28 right
struct BstNode {
    uint64_t key;
    void*    payload;
    uint32_t count;
    uint32_t _pad14;
    uint64_t _pad18;
    BstNode* left;
    BstNode* right;
};

BstNode* BstFind(BstNode* node, uint64_t key) {
    while (node) {
        if (key < node->key)      node = node->left;
        else if (node->key < key) node = node->right;
        else                      return node;
    }
    return nullptr;
}

}  // namespace

// ===========================================================================
// 0x1400262B0 — Object::ExternalPtr<Tutorial>::Serialize
// 0x140028650 — Object::ExternalPtr<UI::Dialog>::Serialize
//
// Both are the stock ExternalPtr<T>::Serialize template body — byte-identical
// to FUN_14001B8E0 / FUN_140002340 / FUN_140027AD0 — differing only in the type
// used for the post-load IsDerivedFrom. 0x140028650's operand is the exe slot
// at 0x140151728, which standalone/address_registry.cpp already documents as
// UI::Dialog's type id.
//
// These two are why a Tutorial.rsc load could not work: every FinishTutorialGoal
// serialize skipped its Tutorial ExternalPtr and every TutorialSetupDialog
// serialize skipped its Dialog ExternalPtr, which does not merely lose the
// field — it mis-advances the stream cursor for everything that follows
// ([[reference-unresolved-rdata-const-deletes-guarded-test]] family).
// ===========================================================================
extern "C" void TutWave_SerializeTutorialRef(void* field, void* serializer) {
    ExternalPtrSerializeGeneric(reinterpret_cast<void**>(field), serializer,
                                nullptr, "Tutorial");
}

extern "C" void TutWave_SerializeDialogRef(void* field, void* serializer) {
    ExternalPtrSerializeGeneric(reinterpret_cast<void**>(field), serializer,
                                nullptr, "UI::Dialog");
}

// ===========================================================================
// 0x1401001F0 — FinishTutorialGoal tutorial-tree insert.
//
// Upsert into the BST at goal+0x50 keyed by the tutorial's External id.
// Existing node: payload byte at +0x10 = 1, dword +0x08 = value, dword +0x0C =
// 0x20. Absent: build that same 0x18-byte record on the stack and hand it to
// the shared BST inserter at 0x140014890 (already registered by
// standalone/reg_toolbar_utility.cpp as fn_BST_Insert_0x14).
// ===========================================================================
extern "C" void TutWave_TutorialTreeInsert(void* treeOwner, uint64_t tutorialId,
                                           void* /*param3*/, uint32_t value) {
    if (!treeOwner) return;
    uintptr_t owner = reinterpret_cast<uintptr_t>(treeOwner);
    int64_t* root = reinterpret_cast<int64_t*>(owner + 0x50);

    // Walk. NB the retail node here is the 0x14-payload flavour, so the byte /
    // dword offsets below are relative to the node base, not to BstNode.
    uint64_t* node = *reinterpret_cast<uint64_t**>(root);
    while (node) {
        if (tutorialId < node[0]) { node = reinterpret_cast<uint64_t*>(node[4]); continue; }
        if (node[0] < tutorialId) { node = reinterpret_cast<uint64_t*>(node[5]); continue; }
        uint8_t* n = reinterpret_cast<uint8_t*>(node);
        *reinterpret_cast<uint8_t*>(n + 0x10)  = 1;
        *reinterpret_cast<uint32_t*>(n + 0x08) = value;
        *reinterpret_cast<uint32_t*>(n + 0x0C) = 0x20;
        return;
    }

    if (!fn_BST_Insert_0x14) { DarkOnce("BST insert 0x140014890 unresolved"); return; }

    struct { uint64_t key; uint32_t value; uint32_t flags; uint8_t on; uint8_t _pad[7]; } rec;
    rec.key   = tutorialId;
    rec.value = value;
    rec.flags = 0x20;
    rec.on    = 1;
    for (int i = 0; i < 7; ++i) rec._pad[i] = 0;

    void* out[4] = {};
    fn_BST_Insert_0x14(root, out, reinterpret_cast<uint64_t*>(&rec));
}

// ===========================================================================
// 0x1400E2250 — recursive element show/hide.
//
// SetFlagsOnOff(elem, show ? 0 : 0x1000000, show ? 0x1000000 : 0), then recurse
// into every child that resolves non-null. 0x1000000 is the hidden bit.
// The double Child() call is retail's, not a transcription slip: the first
// fetch is only a null test, the second is the one that is recursed into, and
// ChildCount is re-read every iteration because a child may add siblings.
// ===========================================================================
extern "C" void TutWave_SetElementFlagsRecursive(void* self, void* elemIBP, char show) {
    if (!elemIBP) return;
    if (!fn_Element_SetFlagsOnOff || !fn_Element_ChildCount || !fn_Element_Child) return;

    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(elemIBP);
    void* elem = *reinterpret_cast<void**>(elemIBP);
    if (!elem) return;

    uint32_t hideFlags = show ? 0u : 0x1000000u;
    uint32_t showFlags = show ? 0x1000000u : 0u;
    fn_Element_SetFlagsOnOff(elem, hideFlags, showFlags);

    int i = 0;
    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(elemIBP);
    int n = fn_Element_ChildCount(*reinterpret_cast<void**>(elemIBP));
    while (i < n) {
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(elemIBP);
        uint64_t probe = 0;
        fn_Element_Child(*reinterpret_cast<void**>(elemIBP), &probe, i);
        if (probe != 0) {
            if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(elemIBP);
            uint64_t child = 0;
            fn_Element_Child(*reinterpret_cast<void**>(elemIBP), &child, i);
            TutWave_SetElementFlagsRecursive(self, &child, show);
        }
        ++i;
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(elemIBP);
        n = fn_Element_ChildCount(*reinterpret_cast<void**>(elemIBP));
    }
}

// ===========================================================================
// 0x1400E2330 — show/hide EVERY open dialog's element tree.
//
// This is the "restrict the player to what the tutorial step allows" mechanic:
// with show=0 it hides the entire UI, and ShowListed (0x1400E29F0) then unhides
// only the paths named in the step's first string vector.
// ===========================================================================
extern "C" void TutWave_SetAllDialogFlags(void* self, char show) {
    void* ui = UserInterface();
    if (!ui || !fn_UI_GetFirstDialog || !fn_DialogInstance_GetRoot) return;
    if (!fn_Element_SetFlagsOnOff || !fn_Element_ChildCount || !fn_Element_Child) return;

    uint64_t iter = 0;
    fn_UI_GetFirstDialog(ui, &iter);
    uint64_t* node = reinterpret_cast<uint64_t*>(iter);
    int dialogs = 0;
    while (node) {
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(node);
        uint64_t rootIBP = 0;
        fn_DialogInstance_GetRoot(reinterpret_cast<void*>(node[0]), &rootIBP);
        if (rootIBP) {
            // Retail applies the flags to the root itself and then walks the
            // children — the root pass is inlined here, the children go through
            // the shared recursive helper (same code, same order).
            if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&rootIBP);
            uint32_t hideFlags = show ? 0u : 0x1000000u;
            uint32_t showFlags = show ? 0x1000000u : 0u;
            fn_Element_SetFlagsOnOff(reinterpret_cast<void*>(rootIBP), hideFlags, showFlags);

            int i = 0;
            int n = fn_Element_ChildCount(reinterpret_cast<void*>(rootIBP));
            while (i < n) {
                uint64_t probe = 0;
                fn_Element_Child(reinterpret_cast<void*>(rootIBP), &probe, i);
                if (probe != 0) {
                    uint64_t child = 0;
                    fn_Element_Child(reinterpret_cast<void*>(rootIBP), &child, i);
                    TutWave_SetElementFlagsRecursive(self, &child, show);
                }
                ++i;
                n = fn_Element_ChildCount(reinterpret_cast<void*>(rootIBP));
            }
        }
        ++dialogs;
        node = reinterpret_cast<uint64_t*>(node[2]);   // iterator +0x10 = next
    }
}

// ===========================================================================
// 0x1400E24F0 — find the currently open TutorialHighlightDialog.
//
// Early-out: if the setup has neither a highlight target path (+0x40, an empty
// System::String reads its length at ptr-8) nor a resolved Dialog External
// (+0x50), it resolves the cached External at +0x58 and gives up when that is
// null too. Otherwise it scans the open-dialog list for one whose IsA matches
// the TutorialHighlightDialog type id.
// ===========================================================================
extern "C" void* TutWave_FindHighlightDialog(void* self, void* outIBP) {
    if (!outIBP) return outIBP;
    *reinterpret_cast<uint64_t*>(outIBP) = 0;
    if (!self) return outIBP;

    uintptr_t s = reinterpret_cast<uintptr_t>(self);
    uintptr_t pathStr = *reinterpret_cast<uintptr_t*>(s + 0x40);
    bool havePath = (pathStr != 0) && (*reinterpret_cast<int*>(pathStr - 8) != 0);
    bool haveDialog = (*reinterpret_cast<uint64_t*>(s + 0x50) != 0);
    if (!havePath && !haveDialog) {
        // FUN_14001B1D0 — reimplemented inline in engine_funcs.h; the exe address
        // itself is on Lane 1's list, but the body is already ours.
        ResolveExternalByName_Impl(reinterpret_cast<void*>(s + 0x58));
        if (*reinterpret_cast<uint64_t*>(s + 0x68) == 0) return outIBP;
    }

    void* ui = UserInterface();
    if (!ui || !fn_UI_GetFirstDialog || !fn_IsA) return outIBP;
    uint32_t tid = HighlightTypeId();
    if (!tid) return outIBP;   // type not registered yet — a safe no-match

    uint64_t iter = 0;
    fn_UI_GetFirstDialog(ui, &iter);
    uint64_t* node = reinterpret_cast<uint64_t*>(iter);
    while (node) {
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(node);
        if (fn_IsA(reinterpret_cast<void*>(node[0]), tid)) {
            *reinterpret_cast<uint64_t*>(outIBP) = node[0];
            return outIBP;
        }
        node = reinterpret_cast<uint64_t*>(node[2]);
    }
    return outIBP;
}

// ===========================================================================
// 0x14001AF90 — copy-construct an ExternalRef {String, generation, cached*}.
// 0x14001AFE0 — resolve one and hand back the cached External*, destroying the
//               temporary String the copy owns.
// Used as a pair by OpenHighlightDialog to look the toolbar/dialog External up
// without disturbing the setup's own cached reference.
// ===========================================================================
extern "C" void* TutWave_ExternalRefCopy(void* dst, void* src) {
    if (!dst || !src) return dst;
    // B.3.1 bridge: the ExternalRef triple is still untyped bytes.
    new (dst) System::String(*reinterpret_cast<const System::String*>(src));
    uintptr_t d = reinterpret_cast<uintptr_t>(dst);
    uintptr_t s = reinterpret_cast<uintptr_t>(src);
    *reinterpret_cast<uint32_t*>(d + 0x08) = *reinterpret_cast<uint32_t*>(s + 0x08);
    *reinterpret_cast<uint64_t*>(d + 0x10) = *reinterpret_cast<uint64_t*>(s + 0x10);
    return dst;
}

extern "C" void* TutWave_ExternalRefResolve(void* out, void* ref) {
    if (!out) return out;
    *reinterpret_cast<uint64_t*>(out) = 0;
    if (!ref) return out;
    ResolveExternalByName_Impl(ref);
    *reinterpret_cast<uint64_t*>(out) = *reinterpret_cast<uint64_t*>(
        reinterpret_cast<uintptr_t>(ref) + 0x10);
    reinterpret_cast<System::String*>(ref)->~String();
    return out;
}

// ===========================================================================
// 0x1400E2700 — open the tutorial highlight dialog for the current setup.
//
// Two branches, both ending in MemAlloc(0xF0) + TutorialHighlightDialogCtor +
// UserInterface::AddDialog:
//
//  A. TOOLBAR-CELL branch, taken when the setup's cached External at +0x68 is
//     non-null AND the toolbar's element count exceeds the setup's index
//     (+0x70). The element comes out of the BST at (gameWorld+0xF0)+0x160,
//     keyed by that External: node+0x08 is the element array, node+0x10 the
//     count. Ctor gets parent=0, +0xE0=0, +0xD8=setup+0x48, +0xE8=element.
//  B. ELEMENT-PATH branch otherwise: FindUIElementByPath(&elem, setup+0x40,
//     &dialog); bail if the element is not found. Ctor gets parent=dialog,
//     +0xE0=element, +0xD8=setup+0x48, +0xE8=0.
//
// Guard rails that are RETAIL, not ours: the same +0x40/+0x50/+0x58/+0x68
// early-out as FindHighlightDialog, and a hard skip while
// UserInterface::GetAddDialogCount() > 0 (never stack a highlight on a frame
// that is already adding dialogs). `checkExisting` non-zero additionally skips
// when a highlight dialog is already open.
// ===========================================================================
extern "C" void TutWave_OpenHighlightDialog(void* self, char checkExisting) {
    if (!self) return;
    EnsureRuntimeFuncs();
    uintptr_t s = reinterpret_cast<uintptr_t>(self);

    uintptr_t pathStr = *reinterpret_cast<uintptr_t*>(s + 0x40);
    bool havePath = (pathStr != 0) && (*reinterpret_cast<int*>(pathStr - 8) != 0);
    bool haveDialog = (*reinterpret_cast<uint64_t*>(s + 0x50) != 0);
    if (!havePath && !haveDialog) {
        ResolveExternalByName_Impl(reinterpret_cast<void*>(s + 0x58));
        if (*reinterpret_cast<uint64_t*>(s + 0x68) == 0) return;
    }

    void* ui = UserInterface();
    if (!ui) return;
    if (s_GetAddDialogCount && s_GetAddDialogCount(ui) > 0) return;

    uint64_t existing = 0;
    TutWave_FindHighlightDialog(self, &existing);
    if (checkExisting != 0 && existing != 0) return;

    ResolveExternalByName_Impl(reinterpret_cast<void*>(s + 0x58));

    if (!fn_MemAlloc || !g_pMemAllocSingleton || !fn_UI_AddDialog) return;

    uint64_t targetElem = 0;     // -> ctor arg that lands at +0xE8 (branch A)
    uint64_t parentIBP  = 0;     // -> ctor R9 parent
    uint64_t elemIBP    = 0;     // -> ctor arg that lands at +0xE0 (branch B)
    bool     branchA    = false;

    if (*reinterpret_cast<uint64_t*>(s + 0x68) != 0) {
        uintptr_t gw = GameWorld();
        uintptr_t mgr = gw ? *reinterpret_cast<uintptr_t*>(gw + 0xF0) : 0;
        BstNode* root = mgr ? *reinterpret_cast<BstNode**>(mgr + 0x160) : nullptr;

        uint8_t refCopy[0x18] = {};
        TutWave_ExternalRefCopy(refCopy, reinterpret_cast<void*>(s + 0x58));
        uint64_t key = 0;
        TutWave_ExternalRefResolve(&key, refCopy);

        BstNode* found = BstFind(root, key);
        int count = found ? static_cast<int>(found->count) : 0;
        int index = *reinterpret_cast<int32_t*>(s + 0x70);

        if (count > index) {
            uint8_t refCopy2[0x18] = {};
            TutWave_ExternalRefCopy(refCopy2, reinterpret_cast<void*>(s + 0x58));
            uint64_t key2 = 0;
            TutWave_ExternalRefResolve(&key2, refCopy2);
            BstNode* found2 = BstFind(root, key2);
            if (found2 && found2->payload) {
                targetElem = reinterpret_cast<uint64_t*>(found2->payload)[index];
            }
            branchA = true;
        }
    }

    if (!branchA) {
        // Branch B needs FindUIElementByPath (exe 0x1400E1870) — LANE 1's.
        if (!fn_FindUIElementByPath) { DarkOnce("FindUIElementByPath 0x1400E1870 (lane 1)"); return; }
        fn_FindUIElementByPath(&elemIBP, reinterpret_cast<void*>(s + 0x40), &parentIBP);
        if (elemIBP == 0) return;
    }

    void* mem = fn_MemAlloc(g_pMemAllocSingleton, 0xF0, 8);
    void* dlg = nullptr;
    if (mem) {
        int32_t valD8 = *reinterpret_cast<int32_t*>(s + 0x48);
        void* dialogExternal = *reinterpret_cast<void**>(s + 0x50);
        if (branchA) {
            uint64_t zeroParent = 0, zeroE0 = 0;
            dlg = TutWave_HighlightDialogCtor(mem, ui, dialogExternal,
                                              &zeroParent, &zeroE0, valD8, &targetElem);
        } else {
            uint64_t zeroE8 = 0;
            dlg = TutWave_HighlightDialogCtor(mem, ui, dialogExternal,
                                              &parentIBP, &elemIBP, valD8, &zeroE8);
        }
    }

    uint64_t dlgIBP = reinterpret_cast<uint64_t>(dlg);
    fn_UI_AddDialog(ui, &dlgIBP);
}

// ===========================================================================
// 0x1400E29F0 — hide everything, then unhide the paths this setup names.
//
// The first string vector (+0x20 data / +0x28 count) holds dotted element paths
// ("DialogType.ElementName"); each resolves through FindUIElementByPath and is
// re-shown recursively. This is the whole "the tutorial greys out the UI except
// the button it wants you to press" effect.
// ===========================================================================
extern "C" void TutWave_ShowListedElements(void* self) {
    if (!self) return;
    uintptr_t s = reinterpret_cast<uintptr_t>(self);

    TutWave_SetAllDialogFlags(self, '\0');

    int count = *reinterpret_cast<int32_t*>(s + 0x28);
    if (count <= 0) return;
    if (!fn_FindUIElementByPath) { DarkOnce("FindUIElementByPath 0x1400E1870 (lane 1)"); return; }

    uintptr_t arr = *reinterpret_cast<uintptr_t*>(s + 0x20);
    if (!arr) return;
    for (int i = 0; i < count; ++i) {
        uint64_t elem = 0;
        fn_FindUIElementByPath(&elem, reinterpret_cast<void*>(arr + static_cast<uintptr_t>(i) * 8), nullptr);
        if (elem != 0) {
            uint64_t copy = elem;
            TutWave_SetElementFlagsRecursive(self, &copy, '\x01');
        }
    }
}

// ===========================================================================
// 0x1400E1260 — push the current TutorialStep into the TutorialUI panel.
//
// Ported from the disassembly (0x1400E1260-0x1400E1513); the Ghidra decompile
// renders the two UI::String constructions with different arity, which is wrong
// — both are the (retbuf, ExternalPtr<StringTable>, String const&) overload
// through the same dispatch slot 0x140151300.
//
//   step = tutorial->steps[self->stepIndex]
//   1. every setup in step (+0x50 data / +0x58 count): vtable slot 8
//   2. preText  = UI::String(step+0x28 stringTable, step+0x30) -> ui+0x38
//      postText = UI::String(step+0x28 stringTable, step+0x38) -> ui+0x40
//   3. each text element is shown iff its own resolved UI::String
//      (element+0xD8/+0xE0) IsValid
//   4. the image table at ui+0x48 is emptied and re-grown to its previous child
//      count with null children
//   5. every image in step (+0x40 data / +0x48 count): vtable slot 9 builds an
//      element against the dialog at ui+0x18, placed with
//      TableElement::SetChild(elem, image+0x2C, image+0x28)
// ===========================================================================
extern "C" void TutWave_RefreshStepUI(void* self) {
    if (!self) return;
    EnsureRuntimeFuncs();
    uintptr_t s = reinterpret_cast<uintptr_t>(self);

    uintptr_t tutorial = *reinterpret_cast<uintptr_t*>(s + 0x30);
    if (!tutorial) return;
    uintptr_t steps = *reinterpret_cast<uintptr_t*>(tutorial + 0x28);
    if (!steps) return;
    int idx = *reinterpret_cast<int32_t*>(s + 0x10);
    uintptr_t step = *reinterpret_cast<uintptr_t*>(steps + static_cast<uintptr_t>(idx) * 8);
    if (!step) return;

    // 1. setup objects — vtable slot 8 (+0x40)
    {
        int n = *reinterpret_cast<int32_t*>(step + 0x58);
        uintptr_t data = *reinterpret_cast<uintptr_t*>(step + 0x50);
        for (int i = 0; i < n && data; ++i) {
            void* obj = *reinterpret_cast<void**>(data + static_cast<uintptr_t>(i) * 8);
            if (!obj) continue;
            void** vt = *reinterpret_cast<void***>(obj);
            if (vt && vt[8]) reinterpret_cast<void (*)(void*)>(vt[8])(obj);
        }
    }

    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(reinterpret_cast<void*>(s + 0x20));
    uintptr_t ui = *reinterpret_cast<uintptr_t*>(s + 0x20);
    if (!ui) return;   // no TutorialUI controller attached yet

    // 2/3. the two text elements
    if (fn_UIString_Ctor && fn_TextElement_SetText && s_UIStringIsValid && fn_Element_SetFlagsOnOff) {
        uint64_t stringTable = *reinterpret_cast<uint64_t*>(step + 0x28);
        struct UIStr { uint64_t a, b; };

        const struct { uintptr_t srcString; uintptr_t uiField; } texts[2] = {
            { step + 0x30, ui + 0x38 },
            { step + 0x38, ui + 0x40 },
        };
        for (const auto& t : texts) {
            uint8_t retbuf[32] = {};
            void* r = fn_UIString_Ctor(retbuf, stringTable, reinterpret_cast<const void*>(t.srcString));
            UIStr tmp = *reinterpret_cast<UIStr*>(r ? r : retbuf);
            if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(reinterpret_cast<void*>(t.uiField));
            void* elem = *reinterpret_cast<void**>(t.uiField);
            if (!elem) continue;
            fn_TextElement_SetText(elem, &tmp);
        }
        for (const auto& t : texts) {
            if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(reinterpret_cast<void*>(t.uiField));
            void* elem = *reinterpret_cast<void**>(t.uiField);
            if (!elem) continue;
            UIStr cur;
            cur.a = *reinterpret_cast<uint64_t*>(reinterpret_cast<uintptr_t>(elem) + 0xD8);
            cur.b = *reinterpret_cast<uint64_t*>(reinterpret_cast<uintptr_t>(elem) + 0xE0);
            bool valid = s_UIStringIsValid(&cur);
            fn_Element_SetFlagsOnOff(elem, valid ? 0u : 1u, valid ? 1u : 0u);
        }
    }

    // 4. clear + re-grow the image table
    void* table = nullptr;
    {
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(reinterpret_cast<void*>(ui + 0x48));
        table = *reinterpret_cast<void**>(ui + 0x48);
        if (table && fn_Element_ChildCount && fn_Element_RemoveChildren && fn_Element_AddChild &&
            fn_InternalBasePtr_Ctor) {
            int n = fn_Element_ChildCount(table);
            fn_Element_RemoveChildren(table);
            for (int i = 0; i < n; ++i) {
                uint64_t nullChild = 0;
                fn_InternalBasePtr_Ctor(&nullChild, nullptr);
                fn_Element_AddChild(table, &nullChild);
            }
        }
    }

    // 5. images — vtable slot 9 (+0x48) builds each element
    {
        int n = *reinterpret_cast<int32_t*>(step + 0x48);
        uintptr_t data = *reinterpret_cast<uintptr_t*>(step + 0x40);
        if (n > 0 && data && table && fn_TableElement_SetChild3) {
            if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(reinterpret_cast<void*>(ui + 0x18));
            uintptr_t dlgIBP = *reinterpret_cast<uintptr_t*>(ui + 0x18);
            void* dialog = dlgIBP ? *reinterpret_cast<void**>(dlgIBP + 0x18) : nullptr;
            for (int i = 0; i < n; ++i) {
                void* img = *reinterpret_cast<void**>(data + static_cast<uintptr_t>(i) * 8);
                if (!img) continue;
                int32_t col = *reinterpret_cast<int32_t*>(reinterpret_cast<uintptr_t>(img) + 0x28);
                int32_t row = *reinterpret_cast<int32_t*>(reinterpret_cast<uintptr_t>(img) + 0x2C);
                void** vt = *reinterpret_cast<void***>(img);
                if (!vt || !vt[9]) continue;
                uint8_t retbuf[16] = {};
                void* elemPtr = reinterpret_cast<void* (*)(void*, void*, void*)>(vt[9])(img, retbuf, dialog);
                fn_TableElement_SetChild3(table, elemPtr ? elemPtr : retbuf, row, col);
            }
        }
    }
}

// ===========================================================================
// 0x1400E1DB0 — the FinishTutorialInstance per-tick driver. This is the heart
// of the tutorial: it binds the TutorialUI controller on the first tick, ticks
// the current step's setups, polls its goals, and advances when all goals pass.
//
//   if self+0x18 (dialog IBP) is empty:
//       find the dialog hosting the tutorial's Dialog External
//         (FindDialogByControllerType, exe 0x140100270 — already registered)
//       self+0x20 = that dialog's controller, if it IsDerivedFrom TutorialUI
//       RefreshStepUI(self)
//   step = tutorial->steps[self->stepIndex]
//   for each setup:  vtable slot 9 (+0x48)(setup, param2)
//   for each goal:   passed += vtable slot 8 (+0x40)(goal, param2, &selfIBP)
//   if passed == goalCount:
//       for each setup: vtable slot 10 (+0x50)(setup)
//       ++stepIndex
//       if more steps: RefreshStepUI(self)
//       else:          <tutorial complete>  (NOT PORTED — see DarkOnce below)
// ===========================================================================
extern "C" { int g_TutUndoDepth = 0; int g_TutUndoIndex = -1; }

extern "C" void TutWave_StepExecute(void* self, void* param2, void* param3, uint64_t param4) {
    if (!self) return;
    uintptr_t s = reinterpret_cast<uintptr_t>(self);

    uintptr_t tutorial = *reinterpret_cast<uintptr_t*>(s + 0x30);
    if (!tutorial) {
        // Both of retail's early returns are silent, which is fine in retail and
        // useless here: "the tutorial never appears" looks identical whether the
        // Tutorial definition never reached +0x30 or the dialog lookup missed.
        // One line each, once.
        static bool once = false;
        if (!once) { once = true;
            fprintf(stderr, "[Tutorial] StepExecute BAILED: tutInst+0x30 (Tutorial def) is NULL "
                            "- FinishTutorialInstance::Slot4 never copied it from the goal\n");
            fflush(stderr); }
        return;
    }

    if (*reinterpret_cast<uint64_t*>(s + 0x18) == 0) {
        if (!fn_FindDialogByControllerType) { DarkOnce("FindDialogByControllerType 0x140100270"); return; }
        uint64_t dlg = 0;
        fn_FindDialogByControllerType(self, &dlg, *reinterpret_cast<uint64_t*>(tutorial + 0x20));
        *reinterpret_cast<uint64_t*>(s + 0x18) = dlg;
        if (dlg == 0) {
            static bool once = false;
            if (!once) { once = true;
                fprintf(stderr, "[Tutorial] StepExecute BAILED: FindDialogByControllerType found no "
                                "dialog for tutorial=%p dialogExt=%p\n",
                        reinterpret_cast<void*>(tutorial),
                        *reinterpret_cast<void**>(tutorial + 0x20));
                fflush(stderr); }
            return;
        }

        uint32_t uiTid = TutorialUITypeId();
        void* controller = nullptr;
        void* ctrlRaw = nullptr;   // whatever GetController handed back, before narrowing
        if (fn_DialogInstance_GetController) {
            if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(reinterpret_cast<void*>(s + 0x18));
            uint64_t ctrl = 0;
            // Third arg is a TYPE FILTER, not padding: retail is
            //   MOV R8D,[0x1401c67f0]   ; TutorialUI type id
            //   CALL [0x140151768]      ; DialogInstance::GetController
            // (0x1400e1e08). Ghidra renders the call as two arguments and hides it.
            // Passing 0 made GetController return NOTHING, so the controller was null,
            // the TutorialUI narrow below could not run, and RefreshStepUI had no panel
            // to write into — the tutorial panel rendered as a bare title and two "Ok"
            // rows. Measured as `ctrl=0 tid=0 (?) wantTutorialUI=857087488`.
            fn_DialogInstance_GetController(reinterpret_cast<void*>(dlg), &ctrl, uiTid);
            ctrlRaw = reinterpret_cast<void*>(ctrl);
            if (ctrl && fn_IsDerivedFrom && uiTid && fn_IsDerivedFrom(reinterpret_cast<void*>(ctrl), uiTid)) {
                controller = reinterpret_cast<void*>(ctrl);
            }
        }
        *reinterpret_cast<uint64_t*>(s + 0x20) = reinterpret_cast<uint64_t>(controller);
        TutWave_RefreshStepUI(self);
    }

    int stepIndex = *reinterpret_cast<int32_t*>(s + 0x10);
    int stepCount = *reinterpret_cast<int32_t*>(tutorial + 0x30);

    if (stepIndex >= stepCount) return;

    uintptr_t steps = *reinterpret_cast<uintptr_t*>(tutorial + 0x28);
    if (!steps) return;
    uintptr_t step = *reinterpret_cast<uintptr_t*>(steps + static_cast<uintptr_t>(stepIndex) * 8);
    if (!step) return;

    // setups: slot 9
    {
        int n = *reinterpret_cast<int32_t*>(step + 0x58);
        uintptr_t data = *reinterpret_cast<uintptr_t*>(step + 0x50);
        for (int i = 0; i < n && data; ++i) {
            void* obj = *reinterpret_cast<void**>(data + static_cast<uintptr_t>(i) * 8);
            if (!obj) continue;
            void** vt = *reinterpret_cast<void***>(obj);
            if (vt && vt[9]) reinterpret_cast<void (*)(void*, void*)>(vt[9])(obj, param2);
        }
    }

    // goals: slot 8, counts the ones that report complete
    int passed = 0;
    int goalCount = *reinterpret_cast<int32_t*>(step + 0x68);
    {
        uintptr_t data = *reinterpret_cast<uintptr_t*>(step + 0x60);
        for (int i = 0; i < goalCount && data; ++i) {
            void* obj = *reinterpret_cast<void**>(data + static_cast<uintptr_t>(i) * 8);
            if (!obj) continue;
            void** vt = *reinterpret_cast<void***>(obj);
            if (!vt || !vt[8]) continue;
            uint64_t selfIBP = 0;
            if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor(&selfIBP, self);
            else selfIBP = reinterpret_cast<uint64_t>(self);
            char ok = reinterpret_cast<char (*)(void*, void*, void*)>(vt[8])(obj, param2, &selfIBP);
            if (ok) ++passed;
        }
    }

    if (goalCount == 0 || passed != goalCount) return;

    // step complete: slot 10 on every setup, then advance.
    //
    // [STANDALONE DEVIATION — ordering] Retail walks this array strictly forward
    // (0x1400E1DB0: `pvVar6 + 8` per iteration). We undo every
    // TutorialSetupRestrictPlayer FIRST, then the rest in the original forward order.
    // Measured reason:
    //   TutorialSetupDialog::Slot10 (0x1400E2460) ends in UI::UserInterface::CloseDialog,
    //   and CloseDialog calls UI::UserInterface::UpdateEvents (Runtime 0x18011FC60),
    //   which DRAINS the queued toolbar command re-entrantly, in the middle of this
    //   loop. Captured on stepFirstHouse2 -> stepFirstHouse3:
    //     GoalButton bit22=1                     goal met (button pressed)
    //     [ToolTrace] OnEvent CLICK tutUndoDepth=1 tutUndoIdx=0 ret=<UpdateEvents>
    //     [ToolTrace] RESTRICT override groupId=<WoodHouse> -> NULL
    //     [ToolTrace] ShowGroup CREATE toolTypeId=...(EditTool)
    //   Setup[0] is the dialog setup; setup[1] is `disableSelect`, an all-default
    //   TutorialSetupRestrictPlayer that holds PlayerController+0x80=1 with +0x88=NULL.
    //   So the Wooden House command was dispatched while the OUTGOING step's "no tool"
    //   restriction was still armed: SetEditMode (0x140102B60) forced the group to NULL
    //   and installed an EditTool, and the placement tool — and with it the yellow target
    //   footprint, which PlacementTool::Draw (0x1400FDCC0) section 2 only renders while a
    //   placement tool is live — never came up.
    //   Undoing the restriction first means that drain sees no restriction at all, which
    //   is the same state retail's drain sees.
    {
        int n = *reinterpret_cast<int32_t*>(step + 0x58);
        uintptr_t data = *reinterpret_cast<uintptr_t*>(step + 0x50);
        static uint32_t s_restrictTid = 0;
        if (!s_restrictTid)
            s_restrictTid = ::Object::GetTypeIdByName("TutorialSetupRestrictPlayer");

        for (int pass = 0; pass < 2; ++pass) {
            for (int i = 0; i < n && data; ++i) {
                void* obj = *reinterpret_cast<void**>(data + static_cast<uintptr_t>(i) * 8);
                if (!obj) continue;
                void** vt = *reinterpret_cast<void***>(obj);
                if (!vt) continue;
                uint32_t tid = vt[2] ? reinterpret_cast<uint32_t (*)(void*)>(vt[2])(obj) : 0;
                const bool isRestrict = (s_restrictTid && tid == s_restrictTid);
                if ((pass == 0) != isRestrict) continue;
                // g_TutUndoDepth lets the toolbar trace say out loud when a UI command
                // is dispatched from inside this loop (see above).
                ++g_TutUndoDepth;
                g_TutUndoIndex = i;
                if (vt[10]) reinterpret_cast<void (*)(void*)>(vt[10])(obj);
                --g_TutUndoDepth;
            }
        }
    }

    *reinterpret_cast<int32_t*>(s + 0x10) = stepIndex + 1;

    if (stepIndex + 1 < stepCount) {
        TutWave_RefreshStepUI(self);
        return;
    }

    // Tutorial finished: boot the next scenario through a fresh GameUpdateLoop.
    //
    // This used to be a DarkOnce stub whose comment named four unported exe
    // functions as blockers. That comment was stale -- 0x140013A60 and
    // 0x140013AA0 are one-line String accessors, 0x140099130 was already inline
    // in engine_funcs.h, and 0x140098060 is a flat field-init. The port lives in
    // tutorial_next_scenario.cpp; see its header for the full retail body.
    //
    // Retail also calls FUN_140101A80 first (lane 1's tutorial child-dialog
    // opener). That one is still unported and is NOT on this path's critical
    // line -- 0D70 replaces the whole loop, dialogs included -- so it stays out.
    (void)param3; (void)param4;
    TutWave_LoadNextScenario(self);
}
