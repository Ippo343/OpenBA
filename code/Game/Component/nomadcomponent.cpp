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
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cstdio>

// ============================================================
// Resolved helpers
// ============================================================

typedef void (*fn_Component_Serialize_t)(void* self, void* stream);
static fn_Component_Serialize_t s_fn_Component_Serialize = nullptr;

typedef void* (*fn_Serializer_Ctor_t)(void* self, void* stream, int version);
static fn_Serializer_Ctor_t s_fn_Serializer_Ctor = nullptr;

typedef void (*fn_Serializer_Dtor_t)(void* self);
static fn_Serializer_Dtor_t s_fn_Serializer_Dtor = nullptr;

typedef void (*fn_Serializer_SerializeInt_t)(void* self, int* value, const char* name);
static fn_Serializer_SerializeInt_t s_fn_Serializer_SerializeInt = nullptr;

// Object::InternalBasePtr::Serialize
typedef void (*fn_IBP_Serialize_t)(void* ibp, void* serializer);
static fn_IBP_Serialize_t s_fn_IBP_Serialize = nullptr;

// Math::Vector3::Serialize
typedef void (*fn_Vector3_Serialize_t)(void* vec3, void* serializer);
static fn_Vector3_Serialize_t s_fn_Vector3_Serialize = nullptr;

// System::Time::Serialize
typedef void (*fn_Time_Serialize_t)(void* time, void* serializer);
static fn_Time_Serialize_t s_fn_Time_Serialize = nullptr;

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
// NomadComponent::Shutdown (0x14004fc40)
// ============================================================
// Component_Serialize + empty controller serializer + second serializer:
//   IBP at +0x28, Vector3 at +0x30, Vector3 at +0x3C, int at +0x50, Time at +0x48

void NomadComponent_Shutdown(NomadComponent* self, void* stream) {
    uintptr_t selfAddr = (uintptr_t)self;

    if (s_fn_Component_Serialize)
        s_fn_Component_Serialize(self, stream);

    // Empty controller serializer block
    uint8_t serializer1[24];
    if (s_fn_Serializer_Ctor) s_fn_Serializer_Ctor(serializer1, stream, 0);
    if (s_fn_Serializer_Dtor) s_fn_Serializer_Dtor(serializer1);

    // Main data serializer
    uint8_t serializer2[24];
    if (s_fn_Serializer_Ctor) s_fn_Serializer_Ctor(serializer2, stream, 0);

    // Serialize IBP at +0x28
    if (s_fn_IBP_Serialize)
        s_fn_IBP_Serialize((void*)(selfAddr + 0x28), serializer2);

    // Validate IBP after deserialize
    void* ibpVal = *(void**)(selfAddr + 0x28);
    if (ibpVal != nullptr && s_fn_IsDerivedFrom_d && s_pComponentEntityTypeId) {
        if (!s_fn_IsDerivedFrom_d(ibpVal, *s_pComponentEntityTypeId)) {
            *(uintptr_t*)(selfAddr + 0x28) = 0;
        }
    }

    // Serialize Vector3 at +0x30 (12 bytes)
    if (s_fn_Vector3_Serialize)
        s_fn_Vector3_Serialize((void*)(selfAddr + 0x30), serializer2);

    // Serialize Vector3 at +0x3C (12 bytes)
    if (s_fn_Vector3_Serialize)
        s_fn_Vector3_Serialize((void*)(selfAddr + 0x3C), serializer2);

    // Serialize int at +0x50
    if (s_fn_Serializer_SerializeInt)
        s_fn_Serializer_SerializeInt(serializer2, (int*)(selfAddr + 0x50), nullptr);

    // Serialize Time at +0x48
    if (s_fn_Time_Serialize)
        s_fn_Time_Serialize((void*)(selfAddr + 0x48), serializer2);

    if (s_fn_Serializer_Dtor) s_fn_Serializer_Dtor(serializer2);
}

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
        fn_InternalBasePtr_CheckPtr(self + 0x28);
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
                    int64_t cvt = *reinterpret_cast<int64_t*>(comp);
                    uint32_t tid = (*reinterpret_cast<uint32_t(**)(void*)>(cvt + 0x10))(comp);
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
        fn_InternalBasePtr_CheckPtr(self + 0x28);
        int64_t ctrl = *reinterpret_cast<int64_t*>(self + 0x28);
        int* probe = reinterpret_cast<int*>(
            (*reinterpret_cast<void*(**)(void*, void*)>(*reinterpret_cast<int64_t*>(ctrl) + 0x58))(
                reinterpret_cast<void*>(ctrl), &ibpA));
        if (probe && *probe != 0) {
            fn_InternalBasePtr_CheckPtr(self + 0x28);
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
    fn_InternalBasePtr_CheckPtr(self + 0x28);
    int64_t ctrl = *reinterpret_cast<int64_t*>(self + 0x28);
    int64_t keyVal = *reinterpret_cast<int64_t*>(self + 0x20);
    *reinterpret_cast<int64_t*>(ctrl + 0x10) = param_1;
    *reinterpret_cast<int64_t*>(ctrl + 0x20) = keyVal;
    *reinterpret_cast<int64_t*>(ctrl + 0x18) = setting;
    fn_InternalBasePtr_CheckPtr(self + 0x28);
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
// slot 0x140151ca0 — by-value struct return: hidden retbuf pointer in RDX (see the call
// site). The second parameter is NOT optional; omitting it corrupts memory.
typedef void* (*fn_GetFirstDialog_t)(void* ui, void* retbuf);
typedef void* (*fn_GetController_t)(void* dialog, void* retbuf); // slot 0x140151768 (member, non-trivial ret)
typedef void  (*fn_CloseDialog_slot_t)(void* ui, void* dlgPtr);// slot 0x140151550
typedef void  (*fn_IBP_CtorFrom_t)(void* dst, void* src);  // slot 0x140152a90

static fn_FUN_140102b60_t   s_fn_FUN_140102b60 = nullptr;
static fn_TradeWork_c690_t  s_fn_TradeWork_c690 = nullptr;
static fn_GetFirstDialog_t  s_fn_GetFirstDialog = nullptr;
static fn_GetController_t    s_fn_GetController = nullptr;
static fn_CloseDialog_slot_t s_fn_CloseDialog_slot = nullptr;
static fn_IBP_CtorFrom_t     s_fn_IBP_CtorFrom = nullptr;
static uint32_t*             s_TypeId_PageToggleUI = nullptr; // DAT_1401c4f00
static bool s_fn3480Resolved = false;

static void Ensure3480() {
    if (s_fn3480Resolved) return;
    s_fn_FUN_140102b60 = reinterpret_cast<fn_FUN_140102b60_t>(ResolveAddr(Module::Exe, 0x140102b60));
    s_fn_TradeWork_c690 = reinterpret_cast<fn_TradeWork_c690_t>(ResolveAddr(Module::Exe, 0x14002c690));
    s_TypeId_PageToggleUI = reinterpret_cast<uint32_t*>(ResolveAddr(Module::Exe, 0x1401c4f00));
    uintptr_t slot;
    slot = ResolveAddr(Module::Exe, 0x140151ca0);
    if (slot) s_fn_GetFirstDialog = *reinterpret_cast<fn_GetFirstDialog_t*>(slot);
    slot = ResolveAddr(Module::Exe, 0x140151768);
    if (slot) s_fn_GetController = *reinterpret_cast<fn_GetController_t*>(slot);
    slot = ResolveAddr(Module::Exe, 0x140151550);
    if (slot) s_fn_CloseDialog_slot = *reinterpret_cast<fn_CloseDialog_slot_t*>(slot);
    slot = ResolveAddr(Module::Exe, 0x140152a90);
    if (slot) s_fn_IBP_CtorFrom = *reinterpret_cast<fn_IBP_CtorFrom_t*>(slot);
    s_fn3480Resolved = true;
}

void NomadComp_FUN_140103480(int64_t param_1, void* param_2) {
    Ensure3480();
    uint8_t* self = reinterpret_cast<uint8_t*>(param_1);

    if (*reinterpret_cast<int64_t*>(self + 0x28) != 0) {
        fn_InternalBasePtr_CheckPtr(self + 0x28);
        // Retail's local_res8: seeded from *param_2 BEFORE the call, and passed by
        // address as the slot's second argument (see the typedef note above).
        int64_t entityRef = *reinterpret_cast<int64_t*>(param_2);
        int64_t ctrl = *reinterpret_cast<int64_t*>(self + 0x28);
        int64_t vt = *reinterpret_cast<int64_t*>(ctrl);
        char active = (*reinterpret_cast<fn_ctrl_active_t*>(vt + 0x50))(
            reinterpret_cast<void*>(ctrl), &entityRef);
        if (active != '\0' && s_fn_FUN_140102b60) s_fn_FUN_140102b60(param_1, 0);
    }

    int64_t entity = *reinterpret_cast<int64_t*>(param_2);
    if (s_fn_TradeWork_c690)
        s_fn_TradeWork_c690(reinterpret_cast<int64_t*>(self + 0x60), &entity);

    void* ui = *reinterpret_cast<void**>(
        reinterpret_cast<uint8_t*>(*g_pGameWorldSingleton) + 0x98);
    if (!s_fn_GetFirstDialog) return;
    // GetFirstDialog returns a struct BY VALUE, so the ABI passes a hidden return-buffer
    // pointer in RDX and the callee WRITES through it. Runtime-x64.dll 0x180031150:
    //   ?GetFirstDialog@UserInterface@UI@@QEAA?AUIterator@?$LinkedList@...@@XZ
    //   void *__thiscall GetFirstDialog(UserInterface *this, void *retval)
    //   { *(undefined8 *)retval = *(undefined8 *)this; return retval; }
    // The old one-arg typedef left RDX stale and that store wrote 8 bytes through whatever
    // the previous call had left there — a memory-corrupting WRITE, worse than the READ
    // fault fixed above. It also took the RAX return (which is `retval`, i.e. the buffer's
    // ADDRESS) as the iterator, one dereference too few. Both are fixed here: pass a real
    // buffer, then read the head-node pointer out of it, matching retail's use of the RDX
    // home slot (`local_res10`) as the iterator it then walks via [2].
    // Currently dormant — slot 0x140151ca0 is unregistered, so the guard above returns
    // first; this fix is what makes registering that slot safe.
    int64_t iter = 0;
    s_fn_GetFirstDialog(ui, &iter);
    int64_t* dlg = reinterpret_cast<int64_t*>(iter);
    while (dlg != nullptr) {
        int64_t* cur = dlg;
        if (*dlg != 0) {
            fn_InternalBasePtr_CheckPtr(dlg);
            void* ctrlBuf[2] = {nullptr, nullptr};
            if (s_fn_GetController) s_fn_GetController(reinterpret_cast<void*>(*cur), ctrlBuf);
            void* found = nullptr;
            if (ctrlBuf[0] != nullptr) {
                fn_InternalBasePtr_CheckPtr(ctrlBuf);
                if (s_TypeId_PageToggleUI && fn_IsDerivedFrom(ctrlBuf[0], *s_TypeId_PageToggleUI))
                    found = ctrlBuf[0];
            }
            int64_t ibp = 0;
            fn_InternalBasePtr_Ctor(&ibp, found);
            if (ibp != 0) {
                fn_InternalBasePtr_CheckPtr(&ibp);
                int64_t bound = *reinterpret_cast<int64_t*>(ibp + 0x30);
                fn_InternalBasePtr_CheckPtr(param_2);
                if (bound == *reinterpret_cast<int64_t*>(param_2)) {
                    void* dlgPtr = reinterpret_cast<void*>(*dlg);
                    if (s_fn_CloseDialog_slot) s_fn_CloseDialog_slot(ui, &dlgPtr);
                    return;
                }
            }
        }
        dlg = reinterpret_cast<int64_t*>(dlg[2]);
    }
}

// ============================================================
// Init: resolve function pointers
// ============================================================
void NomadComponent_InitFuncs() {
    if (s_initialized) return;

    s_fn_Component_Serialize = reinterpret_cast<fn_Component_Serialize_t>(
        ResolveAddr(Module::Exe, 0x1401189b0));
    s_fn_RegisterNomad = reinterpret_cast<fn_RegisterNomad_t>(
        ResolveAddr(Module::Exe, 0x140103480));
    s_pGameState = reinterpret_cast<void**>(
        ResolveAddr(Module::Exe, 0x1401c3dd8));
    s_pComponentEntityTypeId = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c72e8));

    uintptr_t slot;
    slot = ResolveAddr(Module::Exe, 0x140152ad8);
    if (slot) s_fn_Serializer_Ctor = *reinterpret_cast<fn_Serializer_Ctor_t*>(slot);
    slot = ResolveAddr(Module::Exe, 0x140152ad0);
    if (slot) s_fn_Serializer_Dtor = *reinterpret_cast<fn_Serializer_Dtor_t*>(slot);
    slot = ResolveAddr(Module::Exe, 0x140152ac8);
    if (slot) s_fn_Serializer_SerializeInt = *reinterpret_cast<fn_Serializer_SerializeInt_t*>(slot);
    slot = ResolveAddr(Module::Exe, 0x140152a98);
    if (slot) s_fn_IsDerivedFrom_d = *reinterpret_cast<fn_IsDerivedFrom_dispatch_t*>(slot);
    slot = ResolveAddr(Module::Exe, 0x140152aa8);
    if (slot) s_fn_Time_Serialize = *reinterpret_cast<fn_Time_Serialize_t*>(slot);

    s_fn_IBP_Serialize = reinterpret_cast<fn_IBP_Serialize_t>(
        ResolveExport("?Serialize@InternalBasePtr@Object@@QEAAXAEAVSerializer@IO@@@Z"));
    s_fn_Vector3_Serialize = reinterpret_cast<fn_Vector3_Serialize_t>(
        ResolveExport("?Serialize@Vector3@Math@@QEAAXAEAVSerializer@IO@@@Z"));

    s_initialized = true;
}

// ===== Standalone factory (Foundation D — component instantiation) =====
// In standalone the original exe is not mapped, so Object::Factory::CreateObject
// must resolve to a C++ factory here. Produces an inert-but-valid NomadComponent
// (size 0x78, +0x20 capability flags 0x82, Object::Internal base + shared stub
// vtable, zeroed body). Size from NomadComponent_Factory MemAlloc; flags from the
// Component_Ctor (+0x20=0x80) chain OR'd with the leaf ctor bits.
#include "Object/factory_registration.h"
#include "Component/component_factory_base.h"

COMPONENT_INERT_FACTORY(NomadComponent, 0x78, 0x82)
REGISTER_TYPE(NomadComponent, "ControllerComponent",
              &NomadComponent_Factory, &NomadComponent_CreateInPlace);
