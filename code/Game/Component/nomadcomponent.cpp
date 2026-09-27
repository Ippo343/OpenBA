// code/Game/Component/nomadcomponent.cpp
// Clean reimplementation of NomadComponent lifecycle methods from Application-x64.exe.
//
// NomadComponent (0x78 bytes) — Component with SHIFTED controller at +0x58.
// Fields: +0x28=IBP, +0x30=Vector3, +0x3C=Vector3, +0x48=Time, +0x50=int
//
// Vtable at 0x14015a020. Two non-trivial overrides:
//   Shutdown (0x14004fc40) — Serialize IBP + 2 Vector3 + int + Time
//   OnCreate (0x14004fd20) — Register with nomad list
//

#include "Component/nomadcomponent_funcs.h"
#include "Dialog/EntityUI.h"
#include "engine_funcs.h"
#include "resolve.h"
#include "math/Math.h"
#include "ui/UserInterface.h"
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch A
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <new>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B
#include "system/Value.h"   // ENDGAME B.3.4b System caller batch
#include "runtime/types/ui/DialogInstance.h"   // ENDGAME B.3.6 UI callers (K4)

// ============================================================
// Resolved helpers
// ============================================================

// IsDerivedFrom
typedef bool (*fn_IsDerivedFrom_dispatch_t)(void* obj, uint32_t typeId);
static fn_IsDerivedFrom_dispatch_t s_fn_IsDerivedFrom_d = nullptr;

// FUN_140103480 — register with nomad list
typedef void (*fn_RegisterNomad_t)(void* nomadList, void* ibp);
static fn_RegisterNomad_t s_fn_RegisterNomad = nullptr;

static void** s_pGameState = nullptr;
static uint32_t* s_pComponentEntityTypeId = nullptr;

static bool s_initialized = false;


// ============================================================
// NomadComponent::OnCreate (0x14004fd20)
// ============================================================
// Registers owner entity with game state nomad list at +0x120.

void NomadComponent_OnCreate(NomadComponent* self) {
    if (!s_pGameState || !*s_pGameState || !s_fn_RegisterNomad) return;

    // Create IBP from owner (copy the owner pointer)
    uintptr_t localIBP[4] = {0};
    localIBP[0] = *(uintptr_t*)((uintptr_t)self + 0x10);

    void* nomadList = *(void**)((uintptr_t)*s_pGameState + 0x120);
    s_fn_RegisterNomad(nomadList, localIBP);
}

// ============================================================
// Reimpl of FUN_140102b60 (Component reg wave B) — controller rebuild.
// NOT nomad-specific (Ghidra mislabels it): this rebuilds the controller object at
// self+0x28 from a setting-keyed std::map of controller descriptors at self+0x10.
// Called by NomadComp_FUN_140103480 (and other controller-attach paths) whenever the
// active setting key (self+0x20, taken from self+0x88 when the self+0x80 flag is set)
// changes. Steps (1:1 with the decompile / disassembly):
//   1. Resolve the effective key (param2, or self+0x88 if flagged); if unchanged and a
//      controller already exists, do nothing.
//   2. Tear down the old controller (vtable+0x28 cleanup, then DeletePtr) at self+0x28.
//   3. In-order walk the map (root self+0x10) matching descriptor keys against the
//      component list at self+0x20 (vtable+0x10 GetType), recording the last match key.
//   4. Lower-bound the map for that key -> node.
//   5. Factory::CreateObject(node->key2) validated IsDerivedFrom(DWORD_1401c6eb8) ->
//      IBP -> self+0x28.
//   6. If node has no cached setting (node+0x10==0), pull it from the controller's
//      vtable+0x58 accessor, CreateObject(setting type) validated against
//      TypeInfo_ToolSetting (DAT_1401c6ef0) -> IBP -> node+0x10.
//   7. Wire ctrl->owner(+0x10)=self, ctrl->key(+0x20)=self+0x20, ctrl->setting(+0x18)=
//      node+0x10, then vtable+0x20 Startup.
// Closure-safe: all callees are engine externals (CheckPtr/CreateObject/IsDerivedFrom/
// IBP ctor+dtor) + virtual slots. Map-node layout (uint* index -> byte off):
//   *node=+0 key, node[2]=+8 key2, node[4]=+0x10 setting-IBP, node[6]=+0x18 parent,
//   node[8]=+0x20 right, node[10]=+0x28 left.  INTRICATE — lead must Ghidra-verify.
// ============================================================
typedef void* (*fn_b60_CreateObject_t)(void* factory, uint32_t typeId);
typedef bool  (*fn_b60_IsDerived_t)(void* obj, uint32_t typeId);
typedef void  (*fn_b60_IBPCtor_t)(void* dst, void* src);
typedef void  (*fn_b60_DeletePtr_t)(void* ibp);
static void**            s_b60_pFactoryHolder = nullptr; // [0x140152760] -> &factory
static fn_b60_CreateObject_t s_b60_CreateObject = nullptr; // slot 0x140152778
static fn_b60_IsDerived_t    s_b60_IsDerived = nullptr;    // slot 0x140152a98
static fn_b60_IBPCtor_t      s_b60_IBPCtor = nullptr;      // slot 0x140152a90
static fn_b60_DeletePtr_t    s_b60_DeletePtr = nullptr;    // slot 0x140151ab8
static uint32_t*         s_b60_TypeId_Controller = nullptr; // DWORD_1401c6eb8
static uint32_t*         s_b60_TypeId_ToolSetting = nullptr; // DAT_1401c6ef0
static bool s_b60Resolved = false;
static void EnsureB60() {
    if (s_b60Resolved) return;
    uintptr_t slot;
    slot = ResolveAddr(Module::Exe, 0x140152760);
    if (slot) s_b60_pFactoryHolder = *reinterpret_cast<void***>(slot);
    slot = ResolveAddr(Module::Exe, 0x140152778);
    if (slot) s_b60_CreateObject = *reinterpret_cast<fn_b60_CreateObject_t*>(slot);
    slot = ResolveAddr(Module::Exe, 0x140152a98);
    if (slot) s_b60_IsDerived = *reinterpret_cast<fn_b60_IsDerived_t*>(slot);
    slot = ResolveAddr(Module::Exe, 0x140152a90);
    if (slot) s_b60_IBPCtor = *reinterpret_cast<fn_b60_IBPCtor_t*>(slot);
    slot = ResolveAddr(Module::Exe, 0x140151ab8);
    if (slot) s_b60_DeletePtr = *reinterpret_cast<fn_b60_DeletePtr_t*>(slot);
    s_b60_TypeId_Controller  = reinterpret_cast<uint32_t*>(ResolveAddr(Module::Exe, 0x1401c6eb8));
    s_b60_TypeId_ToolSetting = reinterpret_cast<uint32_t*>(ResolveAddr(Module::Exe, 0x1401c6ef0));
    s_b60Resolved = true;
}

void NomadComp_RebuildController_b60(int64_t param_1, int64_t param_2) {
    EnsureB60();
    uint8_t* self = reinterpret_cast<uint8_t*>(param_1);

    int64_t key = param_2;
    if (*reinterpret_cast<char*>(self + 0x80) != '\0' && param_2 != 0) {
        key = *reinterpret_cast<int64_t*>(self + 0x88);
    }

    if (*reinterpret_cast<int64_t*>(self + 0x20) == key &&
        *reinterpret_cast<int64_t*>(self + 0x28) != 0 && key != 0) {
        return;  // unchanged and controller present
    }

    *reinterpret_cast<int64_t*>(self + 0x20) = key;

    // Tear down existing controller IBP
    if (*reinterpret_cast<int64_t*>(self + 0x28) != 0) {
        int64_t ctrl = *reinterpret_cast<int64_t*>(self + 0x28);
        int64_t vt = *reinterpret_cast<int64_t*>(ctrl);
        (*reinterpret_cast<void(**)(void*)>(vt + 0x28))(reinterpret_cast<void*>(ctrl));
        if (s_b60_DeletePtr) s_b60_DeletePtr(self + 0x28);
    }

    // --- In-order walk the map, matching descriptor keys against the component list ---
    uint32_t matchKey = 0;
    uint32_t* node = *reinterpret_cast<uint32_t**>(self + 0x10);  // root
    if (node != nullptr) {
        // descend to leftmost
        uint32_t* cur;
        do { cur = node; node = *reinterpret_cast<uint32_t**>(cur + 8); } while (node != nullptr);
        node = cur;
        while (node != nullptr) {
            int64_t list = *reinterpret_cast<int64_t*>(self + 0x20);
            if (list != 0) {
                uint32_t nodeKey = *node;
                int cnt = *reinterpret_cast<int*>(list + 0x30);
                int64_t arr = *reinterpret_cast<int64_t*>(list + 0x28);
                for (int i = 0; i < cnt; i++) {
                    void* comp = *reinterpret_cast<void**>(arr + (int64_t)i * 8);
                    uint32_t tid = reinterpret_cast<Object::Base*>(comp)->Type();   // B.3.8 bridge (slot 2)
                    if (tid == nodeKey) {
                        if (*reinterpret_cast<int64_t*>(arr + (int64_t)i * 8) != 0)
                            matchKey = *node;
                        break;
                    }
                }
            }
            if (node == nullptr) break;
            // in-order successor
            uint32_t* right = *reinterpret_cast<uint32_t**>(node + 10);
            if (right == nullptr) {
                uint32_t* parent = *reinterpret_cast<uint32_t**>(node + 6);
                while (parent != nullptr &&
                       *reinterpret_cast<uint32_t**>(parent + 10) == node) {
                    node = parent;
                    parent = *reinterpret_cast<uint32_t**>(node + 6);
                }
                node = parent;
            } else {
                uint32_t* d = right;
                while (d != nullptr) { right = d; d = *reinterpret_cast<uint32_t**>(d + 8); }
                node = right;
            }
        }
    }

    // --- Lower-bound the map for matchKey ---
    uint32_t* found = nullptr;
    {
        uint32_t* n = *reinterpret_cast<uint32_t**>(self + 0x10);
        while (n != nullptr) {
            if (matchKey < *n) {
                n = *reinterpret_cast<uint32_t**>(n + 8);   // go left
            } else {
                found = n;
                if (matchKey <= *n) break;
                n = *reinterpret_cast<uint32_t**>(n + 10);  // go right
            }
        }
    }
    if (found == nullptr) return;  // defensive (orig assumes a node)

    void* factory = s_b60_pFactoryHolder ? *s_b60_pFactoryHolder : nullptr;

    // --- CreateObject(node->key2) validated against controller type ---
    void* obj = (factory && s_b60_CreateObject) ? s_b60_CreateObject(factory, found[2]) : nullptr;
    void* ctrlObj = nullptr;
    if (obj && s_b60_IsDerived && s_b60_TypeId_Controller &&
        s_b60_IsDerived(obj, *s_b60_TypeId_Controller)) {
        ctrlObj = obj;
    }
    int64_t ibpA = 0;
    if (s_b60_IBPCtor) s_b60_IBPCtor(&ibpA, ctrlObj);
    *reinterpret_cast<int64_t*>(self + 0x28) = ibpA;

    // --- Build the cached setting (node+0x10) if absent ---
    if (*reinterpret_cast<int64_t*>(found + 4) == 0) {
        int64_t ctrl = *reinterpret_cast<int64_t*>(self + 0x28);
        int* probe = reinterpret_cast<int*>(
            (*reinterpret_cast<void*(**)(void*, void*)>(*reinterpret_cast<int64_t*>(ctrl) + 0x58))(
                reinterpret_cast<void*>(ctrl), &ibpA));
        if (probe && *probe != 0) {
            ctrl = *reinterpret_cast<int64_t*>(self + 0x28);
            uint32_t* setRef = reinterpret_cast<uint32_t*>(
                (*reinterpret_cast<void*(**)(void*, void*)>(*reinterpret_cast<int64_t*>(ctrl) + 0x58))(
                    reinterpret_cast<void*>(ctrl), &ibpA));
            void* setObj = (factory && s_b60_CreateObject)
                               ? s_b60_CreateObject(factory, *setRef) : nullptr;
            void* setValidated = nullptr;
            if (setObj && s_b60_IsDerived && s_b60_TypeId_ToolSetting &&
                s_b60_IsDerived(setObj, *s_b60_TypeId_ToolSetting)) {
                setValidated = setObj;
            }
            int64_t ibpB = 0;
            if (s_b60_IBPCtor) s_b60_IBPCtor(&ibpB, setValidated);
            *reinterpret_cast<int64_t*>(found + 4) = ibpB;
        }
    }

    // --- Wire the controller and start it ---
    int64_t setting = *reinterpret_cast<int64_t*>(found + 4);
    int64_t ctrl = *reinterpret_cast<int64_t*>(self + 0x28);
    int64_t keyVal = *reinterpret_cast<int64_t*>(self + 0x20);
    *reinterpret_cast<int64_t*>(ctrl + 0x10) = param_1;
    *reinterpret_cast<int64_t*>(ctrl + 0x20) = keyVal;
    *reinterpret_cast<int64_t*>(ctrl + 0x18) = setting;
    ctrl = *reinterpret_cast<int64_t*>(self + 0x28);
    (*reinterpret_cast<void(**)(void*)>(*reinterpret_cast<int64_t*>(ctrl) + 0x20))(
        reinterpret_cast<void*>(ctrl));
}

// ============================================================
// Reimpl of FUN_140103480 (Component reg wave "people")
// Ghidra mislabels this "TradeWork_impl_3480"; its real callers are entity-removal
// paths (CitizenComponent_OnRemove, DestroyDescription helpers, NomadComponent_OnCreate):
// it tears down any controller/trade reference for the entity. Steps:
//   1. If the controller at +0x28 reports active (vtable+0x50), call FUN_140102b60(self,0).
//   2. TradeWork_impl_c690 detaches the entity from the work list (+0x60).
//   3. Walk the UI's open dialogs; for any whose controller derives from
//      g_TypeId_PageToggleUI (DAT_1401c4f00) and whose bound entity (+0x30) == param_2,
//      close that dialog and return.
// Sub-deps + the member-return UI calls resolved via .rdata dispatch slots.
// Non-static for registration. Faithful structural port of 0x140103480.
// ============================================================
// vtable+0x50 takes TWO arguments — (self, entityRef) — NOT one.
//
// Retail's slot is EditTool_Slot10 (0x1400fd290), reached here because the object at
// self+0x28 is the EditTool holding the open info dialog:
//     bool EditTool_Slot10(longlong self, longlong *entityRef) {
//         CheckPtr(self + 0x30);
//         return *entityRef == *(longlong *)(self + 0x30);   // <-- DEREFS ARG 2
//     }
// i.e. "is the entity I have selected the one being removed?". TradeWork_impl_3480 sets
// `local_res8 = *(longlong *)param_2` immediately BEFORE the call and passes &local_res8;
// Ghidra renders the call as `(...)()` because it cannot recover args through the
// unrecovered function-pointer type, which is what made this look like a one-arg getter.
//
// Declaring it one-arg left RDX holding whatever the previous call put there, and the
// callee dereferenced it. On the FIRST invocation that is 0x140152a90 — the last argument
// to ResolveAddr inside Ensure3480() — so the crash presented as "a raw exe dispatch-slot
// VA dereferenced as a pointer" and sent the investigation after an unregistered slot.
// The fault address was stale-register garbage, not a resolve failure. (Same trap family
// as reference_byvalue_return_hidden_rdx.)
typedef char  (*fn_ctrl_active_t)(void* ctrl, int64_t* entityRef);   // vtable+0x50
typedef void  (*fn_FUN_140102b60_t)(int64_t self, int v);  // 0x140102b60
typedef void  (*fn_TradeWork_c690_t)(int64_t* listBase, void* ibp); // 0x14002c690
// slot 0x140151768 -- member, non-trivial return AND a THIRD argument Ghidra hides.
// Retail 0x140103480 at 0x140103517: `MOV R8D,[0x1401c4f00]` (EntityUI) immediately
// before `CALL [0x140151768]`. The mangled name confirms it: ...@Object@@I@Z -- the
// trailing `I` is that uint32 type-id FILTER. Declared with two parameters, the call
// passes whatever the previous call left in R8 as the filter, and GetController
// returns nothing for a garbage id -- silently, with no crash and no log.
// See reference_getcontroller_third_arg_is_a_type_filter.
typedef void  (*fn_IBP_CtorFrom_t)(void* dst, void* src);  // slot 0x140152a90

static fn_FUN_140102b60_t   s_fn_FUN_140102b60 = nullptr;
static fn_TradeWork_c690_t  s_fn_TradeWork_c690 = nullptr;
static fn_IBP_CtorFrom_t     s_fn_IBP_CtorFrom = nullptr;
static bool s_fn3480Resolved = false;

static void Ensure3480() {
    if (s_fn3480Resolved) return;
    s_fn_FUN_140102b60 = reinterpret_cast<fn_FUN_140102b60_t>(ResolveAddr(Module::Exe, 0x140102b60));
    s_fn_TradeWork_c690 = reinterpret_cast<fn_TradeWork_c690_t>(ResolveAddr(Module::Exe, 0x14002c690));
    // 0x1401c4f00 is the **EntityUI** type-id slot (registrar RegisterType_EntityUI), not
    // PageToggleUI. It is not RegisterExeAddr'd, so this ResolveAddr always returned 0 and
    // every `if (s_TypeId_PageToggleUI && ...)` guard below was permanently false — this
    // whole "close the removed entity's open info dialog" path was silently dead code.
    // Retail FUN_140103480 gates on that slot and then reads ctrl+0x30 (EntityUI's bound
    // entity), which confirms the type. Resolve by value with a by-name fallback.
    uintptr_t slot;
    slot = ResolveAddr(Module::Exe, 0x140152a90);
    if (slot) s_fn_IBP_CtorFrom = *reinterpret_cast<fn_IBP_CtorFrom_t*>(slot);

    // By-name fallbacks. This function used to read ONLY the raw exe dispatch slots
    // with no fallback of any kind, which is why it behaved differently from every
    // other consumer of the same four functions: edittool.cpp:265 and
    // placementtool.cpp:192 both fall back to the engine_funcs pointers and have
    // therefore always worked. 0x140151ca0 (GetFirstDialog) is unregistered, so
    // `if (!s_fn_GetFirstDialog) return;` below killed the whole walk -- meaning a
    // prior wave's registration of 0x140151768 for exactly this path (see
    // standalone/reg_unreg_lane2.cpp) never actually revived it; it stayed gated
    // behind the slot in front of it. That slot, its typedef and its fallback are
    // gone as of B.3.2: the walk is now a plain iteration of
    // UI::UserInterface::_dialogs, which IS what 0x140151ca0 read (its whole body
    // is `*retbuf = this->_dialogs._head`), so nothing can gate it shut again.
    //
    // Every one of these resolves to the same Runtime export the slot names, so a
    // fallback cannot change WHICH function runs -- only whether one runs at all.
    if (!s_fn_IBP_CtorFrom && fn_InternalBasePtr_Ctor)
        s_fn_IBP_CtorFrom = reinterpret_cast<fn_IBP_CtorFrom_t>(fn_InternalBasePtr_Ctor);
    s_fn3480Resolved = true;
}

void NomadComp_FUN_140103480(int64_t param_1, void* param_2) {
    Ensure3480();
    uint8_t* self = reinterpret_cast<uint8_t*>(param_1);

    if (*reinterpret_cast<int64_t*>(self + 0x28) != 0) {
        // Retail's local_res8: seeded from *param_2 BEFORE the call, and passed by
        // address as the slot's second argument (see the typedef note above).
        int64_t entityRef = *reinterpret_cast<int64_t*>(param_2);
        int64_t ctrl = *reinterpret_cast<int64_t*>(self + 0x28);
        int64_t vt = *reinterpret_cast<int64_t*>(ctrl);
        char active = (*reinterpret_cast<fn_ctrl_active_t*>(vt + 0x50))(
            reinterpret_cast<void*>(ctrl), &entityRef);
        // 0x140102b60 is EntityUI::SetEditMode / Toolbar::ShowGroup — resetting the
        // active tool to mode 0 when the entity it was bound to goes away. This call
        // was DEAD for the whole life of the project (0x140102b60 was ported but never
        // registered); registering it for GameUpdateLoop::Shutdown woke it up. Waking
        // it coincided with the workplace/residence panels going inert, so it is gated
        // while that is confirmed: SetEditMode(pc, 0) tears down the live tool
        // controller at +0x28, which is what those panels' spinners hang off.
        if (active != '\0' && s_fn_FUN_140102b60) {
            static int s_hits = 0;
            if (++s_hits <= 16) {
                fprintf(stderr, "[NomadB60] entity-removed -> SetEditMode(pc,0) hit#%d self=%p\n",
                        s_hits, (void*)param_1);
                fflush(stderr);
            }
            s_fn_FUN_140102b60(param_1, 0);
        }
    }

    int64_t entity = *reinterpret_cast<int64_t*>(param_2);
    if (s_fn_TradeWork_c690)
        s_fn_TradeWork_c690(reinterpret_cast<int64_t*>(self + 0x60), &entity);

    UI::UserInterface* ui = *reinterpret_cast<UI::UserInterface**>(
        reinterpret_cast<uint8_t*>(*g_pGameWorldSingleton) + 0x98);
    // HISTORY, kept because it cost two bugs. Retail reaches this list through
    // GetFirstDialog, which returns a struct BY VALUE, so the ABI passes a hidden
    // return-buffer pointer in RDX and the callee WRITES through it. Runtime-x64.dll
    // 0x180031150:
    //   ?GetFirstDialog@UserInterface@UI@@QEAA?AUIterator@?$LinkedList@...@@XZ
    //   void *__thiscall GetFirstDialog(UserInterface *this, void *retval)
    //   { *(undefined8 *)retval = *(undefined8 *)this; return retval; }
    // An early one-arg typedef here left RDX stale and that store wrote 8 bytes through
    // whatever the previous call had left there — a memory-corrupting WRITE, worse than
    // the READ fault fixed above. It also took the RAX return (which is `retval`, i.e.
    // the buffer's ADDRESS) as the iterator, one dereference too few.
    // LIVE as of 2026-09-04 via a by-name fallback to fn_UI_GetFirstDialog; since
    // B.3.2 the walk is the member iteration below instead, which is the same read
    // (`_dialogs._head`) with no slot, no typedef and no hidden return buffer to get
    // wrong. The GetController call below was fixed to pass its type-id filter in the
    // 2026-09-04 change — that had to land FIRST, because reviving the walk with a
    // two-argument typedef would have called GetController with a stale R8 as the
    // filter.
    // Bounded proof-of-life: this walk was dead for the whole life of the project, so
    // "it runs at all" is the thing to confirm first. Reports the dialog count it sees
    // and whether it closed one.
    // Verified live 2026-09-04 on a Remove of a starting building:
    //   [NomadDlgWalk] #1 visited=3 CLOSED the removed entity's panel
    // with 0x140151ca0 and 0x140151550 both still logging NOT IMPLEMENTED, i.e. the
    // by-name fallbacks are what carry it. Kept bounded so a future regression is one
    // grep away, but it is not free: leave the cap small.
    static int s_walks = 0;
    const bool traceWalk = (++s_walks <= 8);
    int visited = 0, closed = 0;
    for (auto it = ui->_dialogs.GetFirst(); it; ++it) {
        ++visited;
        if (it.node->element != nullptr) {
            void* ctrlBuf[2] = {nullptr, nullptr};
            // R8 = EntityUI (retail `MOV R8D,[0x1401c4f00]` at 0x140103517). The
            // IsDerivedFrom(EntityUI) narrow just below is retail's own second check,
            // NOT a substitute for this filter -- without it the call returns nothing
            // and the narrow never gets anything to reject.
            ::new (ctrlBuf) Object::InternalPtr<UI::DialogController>(
                reinterpret_cast<UI::DialogInstance*>(it.node->element)->GetController(  // B.3.6 bridge
                    EntityUI::TypeId()));   // DAT_1401c4f00
            void* found = nullptr;
            if (ctrlBuf[0] != nullptr) {
                if (fn_IsDerivedFrom(ctrlBuf[0], EntityUI::TypeId()))
                    found = ctrlBuf[0];
            }
            int64_t ibp = 0;
            new (&ibp) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>(found));   // B.3.3 bridge
            if (ibp != 0) {
                int64_t bound = *reinterpret_cast<int64_t*>(ibp + 0x30);
                if (bound == *reinterpret_cast<int64_t*>(param_2)) {
                    void* dlgPtr = it.node->element;
                    ui->CloseDialog(reinterpret_cast<Object::InternalPtr<UI::DialogInstance>*>(&dlgPtr));  // B.3.6 bridge
                    closed = 1;
                    if (traceWalk) {
                        fprintf(stderr, "[NomadDlgWalk] #%d visited=%d CLOSED the removed "
                                        "entity's panel (dlg=%p)\n", s_walks, visited, dlgPtr);
                        fflush(stderr);
                    }
                    return;
                }
            }
        }
    }
    if (traceWalk) {
        fprintf(stderr, "[NomadDlgWalk] #%d visited=%d closed=%d (no open panel bound to "
                        "the removed entity)\n", s_walks, visited, closed);
        fflush(stderr);
    }
}

// ============================================================
// Init: resolve function pointers
// ============================================================
void NomadComponent_InitFuncs() {
    if (s_initialized) return;

    s_fn_RegisterNomad = reinterpret_cast<fn_RegisterNomad_t>(
        ResolveAddr(Module::Exe, 0x140103480));
    s_pGameState = reinterpret_cast<void**>(
        ResolveAddr(Module::Exe, 0x1401c3dd8));
    s_pComponentEntityTypeId = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c72e8));

    uintptr_t slot;
    slot = ResolveAddr(Module::Exe, 0x140152a98);
    if (slot) s_fn_IsDerivedFrom_d = *reinterpret_cast<fn_IsDerivedFrom_dispatch_t*>(slot);

    s_initialized = true;
}

// ============================================================================
// ENDGAME B.3.8 MI-A — NomadComponent as a REAL class (NomadComponent.h).
//
// Before: CONTROLLER_LIVE_FACTORY_STATES + CONTROLLER_INSTALL_SERIALIZE (a 48-slot
// table re-filled at every create), `Inert_ScalarDtor` in slot 0 (only `~Internal`), the
// ControllerLive::Dispatcher<NomadCtrlTag> table stamped into raw bytes at +0x58.
// After: the compiler's 18-slot vtable, a real constructor and destructor
// chain, the controller a member object (B38_RULES D1, D2, D6).
// ============================================================================
#include "Object/factory_registration.h"
#include "Component/NomadComponent.h"

// The nomad walk-in/loiter/walk-out state machine (nomadcomponent_states.cpp).
// Node at +0x58 (slot 17 0x140009060), default state 0 (slot 16 0x140003730). The
// state table is built ONCE at static-init time, as retail's static initializer
// does (B38 D6/D7).
extern "C" void NomadCtrl_PopulateTable();
static const bool s_nomadStateTableBuilt = (NomadCtrl_PopulateTable(), true);

// 0x14004fdb0. Component() is the CALL 0x1401188b0; the stamps are the compiler's;
// +0x28..+0x50 = 0; `_controller(this)` is the member's back-pointer at +0x70;
// then `_flags |= 2` (0x82).
NomadComponent::NomadComponent()
    : field28(nullptr), field30(nullptr), field38(nullptr), field40(nullptr), field48(nullptr),
      field50(0), _pad54{}, _controller(this) {
    OrFlagsInCtor(0x2);
}

// [0] 0x140009230: ~NomadComponent (0x14004fd50), MemFree on &1.
Object::Base* NomadComponent::DeletingDtor(unsigned flags) {
    this->~NomadComponent();
    if (flags & 1) Object::detail::InternalMemFree(this);
    return this;
}
// 0x14004fd50: restamp + ~Internal, no own work.
NomadComponent::~NomadComponent() = default;

// create 0x140009020: MemAlloc(0x78, 8), then the ctor.
void* NomadComponent::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(NomadComponent), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) NomadComponent());
}
// Retail's createInPlace is `Internal::CreateInPlaceObject` (returns null); the
// tree's in-place entry built the object, and that is kept (B38 B8-13).
void* NomadComponent::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) NomadComponent());
}

// ============================================================
// NomadComponent::Persist (vtable slot [3], 0x14004fc40)
// ============================================================
// Component_Serialize + empty controller serializer + second serializer:
//   IBP at +0x28, Vector3 at +0x30, Vector3 at +0x3C, int at +0x50, Time at +0x48

void NomadComponent::Persist(void* stream) {
    // resolver call added at top, save/load wave 2026-08-28: InitFuncs was declared
    // but never called anywhere, so every s_fn_* below stayed null and load silently
    // skipped this component's fields.
    NomadComponent_InitFuncs();
    uintptr_t selfAddr = reinterpret_cast<uintptr_t>(this);

    Component::Persist(stream);   // retail: CALL 0x1401189b0 (qualified)

    // Empty controller serializer block
    alignas(8) uint8_t serializer1[24];
    ::new (static_cast<void*>(serializer1)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(serializer1)->~Serializer();                                       // B.3.4 bridge

    // Main data serializer
    alignas(8) uint8_t serializer2[24];
    ::new (static_cast<void*>(serializer2)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge

    // Serialize IBP at +0x28
    reinterpret_cast<Object::InternalBasePtr*>((void*)(selfAddr + 0x28))->Serialize(*reinterpret_cast<IO::Serializer*>(serializer2));   // B.3.4 bridge

    // Validate IBP after deserialize
    void* ibpVal = *(void**)(selfAddr + 0x28);
    if (ibpVal != nullptr && s_fn_IsDerivedFrom_d && s_pComponentEntityTypeId) {
        if (!s_fn_IsDerivedFrom_d(ibpVal, *s_pComponentEntityTypeId)) {
            *(uintptr_t*)(selfAddr + 0x28) = 0;
        }
    }

    // Serialize Vector3 at +0x30 (12 bytes)
    reinterpret_cast<Math::Vector3*>(selfAddr + 0x30)->Serialize(serializer2);   // B.3.1 bridge

    // Serialize Vector3 at +0x3C (12 bytes)
    reinterpret_cast<Math::Vector3*>(selfAddr + 0x3C)->Serialize(serializer2);   // B.3.1 bridge

    // Serialize int at +0x50
    reinterpret_cast<IO::Serializer*>(serializer2)->Serialize(*reinterpret_cast<int*>(selfAddr + 0x50), nullptr);   // B.3.4 bridge

    // Serialize Time at +0x48
    reinterpret_cast<System::Time*>((void*)(selfAddr + 0x48))->Serialize(*reinterpret_cast<IO::Serializer*>(serializer2));   // B.3.4b bridge

    reinterpret_cast<IO::Serializer*>(serializer2)->~Serializer();                                       // B.3.4 bridge
}

// [7] 0x14004fd20 — the removed-from-world broadcast: hand the owner entity to the
// player/selection manager's removal notify (0x140103480, "TradeWork_impl_3480" in
// Ghidra: drop the selection if it is this entity, close its EntityUI dialogs).
// Retail, whole body:
//     MOV RAX,[RCX+0x10]; MOV RCX,[0x1401c3dd8]; LEA RDX,[RSP+0x30]
//     MOV RCX,[RCX+0x120]; MOV [RSP+0x30],RAX; CALL 0x140103480
// Unguarded, as retail. The notify is resolved per call (no first-use cache:
// B38 D7). NomadComponent_OnCreate above is the same body under the old slot name,
// never installed; it stays for its registration's sake.
typedef void (*fn_NomadRemovalNotify_t)(int64_t manager, void* entityIBP);   // 0x140103480
void NomadComponent::OnRemove() {
    const auto notify = reinterpret_cast<fn_NomadRemovalNotify_t>(ResolveAddr(Module::Exe, 0x140103480));
    int64_t entityIBP = reinterpret_cast<int64_t>(_entity);
    notify(*reinterpret_cast<int64_t*>(reinterpret_cast<uintptr_t>(*g_pGameWorldSingleton) + 0x120),
           &entityIBP);
}

DEFINE_EXE_VTABLE(NomadComponent, "_ZTV14NomadComponent")
DEFINE_EXE_TYPE(NomadComponent, "ControllerComponent")
