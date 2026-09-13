// code/Game/Component/happinesscomponent.cpp
// Clean reimplementation of HappinessComponent lifecycle methods from Application-x64.exe.
//
// HappinessComponent (0x50 bytes) -- ControllerComponent subclass with shifted
// controller sub-object at +0x30 (own data byte at +0x28).
// Tracks citizen happiness state.
//
// Vtable at 0x1401595d0:
//   [3] Shutdown  = 0x14004aa80 -- serialize base + happinessFlag
//   [7] OnRemove  = 0x14004aa20 -- ERASE from the happiness list (Ghidra names this
//                                  slot OnCreate on several components; it is the remove side)
//
// Decompiled from Application-x64.exe using Ghidra.

#include "Component/happinesscomponent_funcs.h"
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

// ============================================================
// Resolved helpers
// ============================================================

// Component_Serialize (base class) -- at 0x1401189b0
typedef void (*fn_Component_Serialize_t)(void* self, void* stream);
static fn_Component_Serialize_t s_fn_Component_Serialize = nullptr;

// IO::Serializer::Serializer(stream, version) -- dispatch slot at 0x140152ad8
typedef void* (*fn_Serializer_Ctor_t)(void* self, void* stream, int version);
static fn_Serializer_Ctor_t s_fn_Serializer_Ctor = nullptr;

// IO::Serializer::~Serializer() -- dispatch slot at 0x140152ad0
typedef void (*fn_Serializer_Dtor_t)(void* self);
static fn_Serializer_Dtor_t s_fn_Serializer_Dtor = nullptr;

// IO::Serializer::Serialize(bool*) -- dispatch slot at 0x140152a58
typedef void (*fn_Serializer_SerializeBool_t)(void* self, bool* value, const char* name);
static fn_Serializer_SerializeBool_t s_fn_Serializer_SerializeBool = nullptr;

// 0x140002cc0 DynArray8_EraseByValue -- REMOVES every element equal to *value, shifting
// the tail down and decrementing the count. It is NOT a push, despite the name this
// typedef carried until 2026-08-25; the append counterpart is DynArray8_ResizeFill
// (0x140003110), which is what Register uses. Ghidra's own plate comment on the callee
// records that leaving it unnamed already caused two component OnCreate/OnRemove vtable
// entries to be mislabelled as the create side. Renamed here so nobody "fixes"
// Unregister into duplicating the source it is supposed to remove.
typedef void (*fn_ArrayEraseByValue_t)(void* arrayBase, void* internalBasePtr);
static fn_ArrayEraseByValue_t s_fn_ArrayEraseByValue = nullptr;

// FishingComponent_impl_a300 -- sets index on entity to given value
// Complex helper that looks up building type, finds linked component, sets map index.
typedef void (*fn_SetEntityIndex_t)(void* self, int16_t index);
static fn_SetEntityIndex_t s_fn_SetEntityIndex = nullptr;

// FUN_140003110 -- ArrayPush(base, newCount, elemPtr): grow to newCount and store.
// (0x140002cc0 above, despite its legacy name, is the ERASE -- see the 1:1 port in
// cemeterycomponent.cpp. The two are the register/unregister pair for townMgr+0x50.)
typedef void (*fn_ArrayPush_t)(void* arrayBase, int newCount, void* elementPtr);
static fn_ArrayPush_t s_fn_ArrayPush = nullptr;

// 0x140002e10 AttendanceComponent_GetJobHostComponent(entity, outIBP) -- the owner's
// workplace, used by the "needs staff" arm of IsActive.
typedef void (*fn_GetJobHost_t)(uintptr_t entity, void* outIBP);
static fn_GetJobHost_t s_fn_GetJobHost = nullptr;

// 0x140006270 Entity_FindComponent_Storage(entity, outIBP) -- the "needs goods" arm.
typedef void (*fn_FindStorage_t)(uintptr_t entity, void* outIBP);
static fn_FindStorage_t s_fn_FindStorage = nullptr;

// InternalBasePtr::CheckPtr (slot 0x140152ab0) / IsDerivedFrom (slot 0x140152a98).
typedef void (*fn_CheckPtr_t)(void* ibp);
static fn_CheckPtr_t s_fn_CheckPtr = nullptr;
typedef bool (*fn_IsDerivedFrom_t)(void* obj, uint32_t typeId);
static fn_IsDerivedFrom_t s_fn_IsDerivedFrom = nullptr;

// Type-id .rdata globals: HappinessDescription (0x1401c42f8) and the
// RawMaterialDescription (0x1401c7cb8) whose +0x5c flags the goods test masks.
static const uint32_t* s_pTid_HappinessDesc = nullptr;
static const uint32_t* s_pTid_RawMatDesc    = nullptr;

// Global: DAT_1401c3dd8 -- pointer to game world/population manager
static void** s_pGameWorld = nullptr;

static bool s_initialized = false;

static void EnsureHelpers() {
    if (s_initialized) return;

    uintptr_t slot;

    s_fn_Component_Serialize = reinterpret_cast<fn_Component_Serialize_t>(
        ResolveAddr(Module::Exe, 0x1401189b0));

    s_fn_ArrayEraseByValue = reinterpret_cast<fn_ArrayEraseByValue_t>(
        ResolveAddr(Module::Exe, 0x140002cc0));
    s_fn_SetEntityIndex = reinterpret_cast<fn_SetEntityIndex_t>(
        ResolveAddr(Module::Exe, 0x14004a300));

    s_pGameWorld = reinterpret_cast<void**>(
        ResolveAddr(Module::Exe, 0x1401c3dd8));

    slot = ResolveAddr(Module::Exe, 0x140152ad8);
    if (slot) s_fn_Serializer_Ctor = reinterpret_cast<fn_Serializer_Ctor_t>(
        *reinterpret_cast<uintptr_t*>(slot));

    slot = ResolveAddr(Module::Exe, 0x140152ad0);
    if (slot) s_fn_Serializer_Dtor = reinterpret_cast<fn_Serializer_Dtor_t>(
        *reinterpret_cast<uintptr_t*>(slot));

    slot = ResolveAddr(Module::Exe, 0x140152a58);
    if (slot) s_fn_Serializer_SerializeBool = reinterpret_cast<fn_Serializer_SerializeBool_t>(
        *reinterpret_cast<uintptr_t*>(slot));

    s_fn_ArrayPush = reinterpret_cast<fn_ArrayPush_t>(
        ResolveAddr(Module::Exe, 0x140003110));
    s_fn_GetJobHost = reinterpret_cast<fn_GetJobHost_t>(
        ResolveAddr(Module::Exe, 0x140002e10));
    s_fn_FindStorage = reinterpret_cast<fn_FindStorage_t>(
        ResolveAddr(Module::Exe, 0x140006270));
    s_pTid_HappinessDesc = reinterpret_cast<const uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c42f8));
    s_pTid_RawMatDesc = reinterpret_cast<const uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c7cb8));

    slot = ResolveAddr(Module::Exe, 0x140152ab0);
    if (slot) s_fn_CheckPtr = *reinterpret_cast<fn_CheckPtr_t*>(slot);
    slot = ResolveAddr(Module::Exe, 0x140152a98);
    if (slot) s_fn_IsDerivedFrom = *reinterpret_cast<fn_IsDerivedFrom_t*>(slot);

    s_initialized = true;
}

// ============================================================
// HappinessComponent::Shutdown (vtable slot [3], 0x14004aa80)
// ============================================================
// Ghidra decompilation:
//   Component_Serialize(param_1, param_2)
//   Serializer(local_20, param_2, 0); ~Serializer(local_20)  // empty controller block
//   Serializer(local_30, param_2, 0)
//   Serialize(local_30, param_1+0x28, nullptr)  // happinessFlag as bool
//   ~Serializer(local_30)

void HappinessComponent_Shutdown(HappinessComponent* self, void* stream) {
    EnsureHelpers();

    // Call base Component_Serialize
    if (s_fn_Component_Serialize) {
        s_fn_Component_Serialize(self, stream);
    }

    // Empty serializer block (controller compat)
    uint8_t serializer1[24];
    if (s_fn_Serializer_Ctor && s_fn_Serializer_Dtor) {
        s_fn_Serializer_Ctor(serializer1, stream, 0);
        s_fn_Serializer_Dtor(serializer1);
    }

    // Serialize happinessFlag (bool at +0x28)
    uint8_t serializer2[24];
    if (s_fn_Serializer_Ctor) {
        s_fn_Serializer_Ctor(serializer2, stream, 0);
    }
    if (s_fn_Serializer_SerializeBool) {
        s_fn_Serializer_SerializeBool(serializer2, (bool*)&self->happinessFlag, nullptr);
    }
    if (s_fn_Serializer_Dtor) {
        s_fn_Serializer_Dtor(serializer2);
    }
}

// ============================================================
// HappinessComponent::OnRemove (vtable slot [7], 0x14004aa20)
// ============================================================
// Ghidra decompilation:
//   if (param_1[0x28] != 0) {
//     InternalBasePtr::Ctor(local, param_1)
//     FUN_140002cc0( *(gameWorld + 0xF0) + 0x50, local )
//     FishingComponent_impl_a300(param_1, -1)
//     param_1[0x28] = 0
//   }

void HappinessComponent_OnRemove(HappinessComponent* self) {
    EnsureHelpers();

    if (self->happinessFlag != 0) {
        // Create InternalBasePtr from self (the component, passed as entity ref)
        uint8_t basePtr[24];
        if (fn_InternalBasePtr_Ctor) {
            fn_InternalBasePtr_Ctor(basePtr, self);
        }

        // ERASE from the happiness list at (gameWorld->+0xF0)->+0x50.
        //
        // This said "Push" until 2026-08-25 and the function was called OnCreate.
        // Both were wrong and they were the SAME wrong: 0x140002cc0 is
        // DynArray8_EraseByValue, so a slot that erases, sets the entity index to
        // -1 and clears the registered flag is the REMOVE side. Ghidra mislabels
        // this pair on several components (its own plate comment on the callee
        // records two more), and the mislabel is load-bearing here because the
        // file already refers to 0x14004aa20 as "the slot-7 removal hook"
        // elsewhere -- the name and the behaviour disagreed in one file.
        if (s_fn_ArrayEraseByValue && s_pGameWorld && *s_pGameWorld) {
            void* manager = *s_pGameWorld;
            void* happinessArray = reinterpret_cast<void*>(
                *reinterpret_cast<uintptr_t*>(
                    reinterpret_cast<uintptr_t>(manager) + 0xF0) + 0x50);
            s_fn_ArrayEraseByValue(happinessArray, basePtr);
        }

        // Set entity index to -1
        if (s_fn_SetEntityIndex) {
            s_fn_SetEntityIndex(self, -1);
        }

        // Clear the flag
        self->happinessFlag = 0;
    }
}

// ============================================================
// Reimpl of FUN_14004a300 (FishingComponent_impl_a300: notify the population
//   manager of a fishing tile, mapping the description's mode to a state id)
// ============================================================
// Despite the local typedef name (SetEntityIndex), this reads the FishingDescription
// off owner+0x18 (type 0x1401c42f8), locates the fishing-target component (type
// 0x1401c4870) on the host, the MapComponent (FUN_140004680) for tile coords, maps
// desc+0x20 (0..4) to a state id, and calls FishingComponent_helper_f990 (0x14009f990).
typedef void* (*fn_FC_FindMap_t)(uintptr_t entity, void* retbuf);   // 0x140004680
typedef void (*fn_FC_Helper_f990_t)(int* mgr, int state, int x, int y,
                                    int resourceId, short param6);    // 0x14009f990
static fn_FC_FindMap_t      s_fc_FindMap = nullptr;
static fn_FC_Helper_f990_t  s_fc_Helper = nullptr;
static uint32_t*            s_fc_pTId_FishingDesc = nullptr;   // 0x1401c42f8
static uint32_t*            s_fc_pTId_Target = nullptr;        // 0x1401c4870
static bool s_fcResolved = false;
static void EnsureFishingHelpers() {
    if (s_fcResolved) return;
    s_fc_FindMap = reinterpret_cast<fn_FC_FindMap_t>(ResolveAddr(Module::Exe, 0x140004680));
    s_fc_Helper  = reinterpret_cast<fn_FC_Helper_f990_t>(ResolveAddr(Module::Exe, 0x14009f990));
    s_fc_pTId_FishingDesc = reinterpret_cast<uint32_t*>(ResolveAddr(Module::Exe, 0x1401c42f8));
    s_fc_pTId_Target      = reinterpret_cast<uint32_t*>(ResolveAddr(Module::Exe, 0x1401c4870));
    s_fcResolved = true;
}

// ============================================================
// Reimpl of FishingComponent_helper_f990 (0x14009f990) — Component reg wave B.
// Stamps a circular brush of radius `r` (param5) onto a population grid: for every
// cell within the disk it adds `delta` (param6) to a per-cell byte, then bumps a
// per-state accumulator short. mgr (param1): +0 = grid width (int), +0x80 = grid
// base ptr; cell stride 0x30, the touched byte sits at cell+0x1c. The state-totals
// short array starts at mgr+0x90, indexed by `state` (param2). Pure leaf (no callees).
// Faithful 1:1 port of the Ghidra decompile.
// ============================================================
void FishingComponent_BrushTile(int* mgr, int state, int cx, int cy, int r, short delta) {
    uint8_t* base = reinterpret_cast<uint8_t*>(mgr);
    int yy = cy - r;
    if (yy <= cy + r) {
        int dy = cy - yy;
        do {
            int xx = cx - r;
            if (xx <= r + cx) {
                int dx = cx - xx;
                int ix = xx;
                do {
                    int width = *mgr;
                    if (ix >= 0 && yy >= 0 && ix < width && yy < width &&
                        dy * dy + dx * dx <= r * r) {
                        int64_t gridBase = *reinterpret_cast<int64_t*>(base + 0x80);
                        char* cell = reinterpret_cast<char*>(
                            (int64_t)state + 0x1c + (int64_t)(width * yy + ix) * 0x30 + gridBase);
                        *cell = (char)(*cell + (char)delta);
                    }
                    ix++;
                    dx--;
                } while (ix <= r + cx);
            }
            yy++;
            dy--;
        } while (yy <= cy + r);
    }
    short* acc = reinterpret_cast<short*>(base + (int64_t)state * 2 + 0x90);
    *acc = (short)(*acc + delta);
}

void FishingComponent_NotifyTile(void* selfP, int16_t param2) {
    EnsureHelpers();
    EnsureFishingHelpers();
    uintptr_t self = (uintptr_t)selfP;
    if (!s_pGameWorld || !*s_pGameWorld) return;

    void* descObj = *(void**)(self + 0x18);
    void* desc = (fn_IsDerivedFrom && s_fc_pTId_FishingDesc &&
                  fn_IsDerivedFrom(descObj, *s_fc_pTId_FishingDesc)) ? descObj : nullptr;

    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr((void*)(self + 0x10));
    int64_t host = *(int64_t*)(*(int64_t*)(self + 0x10) + 0x28);

    // Find the fishing-target component (type 0x1401c4870) on the host
    void* targetComp = nullptr;
    if (s_fc_pTId_Target) {
        int count = *(int*)(host + 0x30);
        int64_t arr = *(int64_t*)(host + 0x28);
        for (int i = 0; i < count; i++) {
            void* comp = *(void**)(arr + (int64_t)i * 8);
            typedef uint32_t (*GetTypeId_t)(void*);
            uint32_t tid = ((GetTypeId_t)(*(uintptr_t*)(*(uintptr_t*)comp + 0x10)))(comp);
            if (tid == *s_fc_pTId_Target) {
                if (fn_IsDerivedFrom && fn_IsDerivedFrom(comp, *s_fc_pTId_Target))
                    targetComp = comp;
                break;
            }
        }
    }

    // Find the MapComponent for tile coords
    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr((void*)(self + 0x10));
    uint8_t mapBuf[8] = {0};
    if (s_fc_FindMap) s_fc_FindMap(*(uintptr_t*)(self + 0x10), mapBuf);
    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(mapBuf);
    int64_t mapComp = *(int64_t*)mapBuf;

    // Map description mode (desc+0x20) to a state id (default 5)
    int state = 5;
    switch (desc ? *(int*)((uintptr_t)desc + 0x20) : -1) {
        case 0: state = 0; break;
        case 1: state = 1; break;
        case 2: state = 2; break;
        case 3: state = 3; break;
        case 4: state = 4; break;
        default: break;
    }

    int x = ((int)*(int16_t*)(mapComp + 0x2c) >> 1) + (int)*(int16_t*)(mapComp + 0x28);
    int y = ((int)*(int16_t*)(mapComp + 0x2e) >> 1) + (int)*(int16_t*)(mapComp + 0x2a);
    int resourceId = targetComp ? *(int*)((uintptr_t)targetComp + 0x28) : 0;

    if (s_fc_Helper)
        s_fc_Helper(*(int**)((uintptr_t)*s_pGameWorld + 0xf8), state, x, y, resourceId, param2);
}

// ============================================================
// Init: resolve function pointers
// ============================================================
void HappinessComponent_InitFuncs() {
    EnsureHelpers();
#ifdef _DEBUG
    printf("[HappinessComponent] Funcs resolved: Serialize=%p ArrayPush=%p SetIdx=%p\n",
           (void*)s_fn_Component_Serialize, (void*)s_fn_ArrayEraseByValue,
           (void*)s_fn_SetEntityIndex);
#endif
}

// ============================================================
// HappinessComponent::IsActive -- 0x14004a550
// ============================================================
// Decides whether this happiness source is currently DOING anything. Read off the
// disassembly at 0x14004a550; the three arms, in retail's order:
//
//   1. desc[+0x20] == 5  -> unconditionally active. This is the plain radius source
//      (the chapel / well / town square kind): being built is enough.
//   2. desc[+0x24] != 0  -> needs staff. Find the owner's workplace via
//      AttendanceComponent_GetJobHostComponent (0x140002e10); with no workplace the
//      staffing test is skipped entirely, otherwise a workplace with no worker
//      (+0x60 == 0), or one that is closed (+0x50 == 0), makes it inactive.
//   3. desc[+0x25] != 0  -> needs goods. Walk the owner's StorageComponent slots
//      (0x140006270; slot records are 0x10 bytes, first qword = the stored good's
//      entity) and AND that good's RawMaterialDescription flags (+0x5c) against the
//      happiness description's accepted-goods mask (+0x28). Active if any slot matches.
//      This is the tavern: it cheers nobody up until there is ale in it.
//
// NOTE the inner RawMaterialDescription lookup is deliberately left unguarded. Retail
// falls out of that loop with RCX == 0 and reads [RCX+0x5c] regardless -- it relies on
// every stored good carrying a RawMaterialDescription. Guarding it here would silently
// skip the mask test rather than restore the behaviour, so the deref stays faithful; if
// it ever faults, the good in storage is what is wrong.
extern "C" bool HappinessComponent_IsActive(void* selfV) {
    HappinessComponent* self = reinterpret_cast<HappinessComponent*>(selfV);
    EnsureHelpers();

    const uintptr_t su = reinterpret_cast<uintptr_t>(self);
    void* descRaw = *reinterpret_cast<void**>(su + 0x18);
    const uintptr_t desc =
        (descRaw && s_fn_IsDerivedFrom && s_pTid_HappinessDesc &&
         s_fn_IsDerivedFrom(descRaw, *s_pTid_HappinessDesc))
            ? reinterpret_cast<uintptr_t>(descRaw) : 0;

    if (*reinterpret_cast<int*>(desc + 0x20) == 5) return true;

    if (*reinterpret_cast<uint8_t*>(desc + 0x24) != 0) {
        void* ownerSlot = *reinterpret_cast<void**>(su + 0x10);
        if (s_fn_CheckPtr) s_fn_CheckPtr(reinterpret_cast<void*>(su + 0x10));
        ownerSlot = *reinterpret_cast<void**>(su + 0x10);

        void* wpIbp = nullptr;
        if (s_fn_GetJobHost) s_fn_GetJobHost(reinterpret_cast<uintptr_t>(ownerSlot), &wpIbp);
        if (wpIbp) {
            if (s_fn_CheckPtr) s_fn_CheckPtr(&wpIbp);
            const uintptr_t wp = reinterpret_cast<uintptr_t>(wpIbp);
            if (*reinterpret_cast<int*>(wp + 0x60) == 0) return false;
            if (s_fn_CheckPtr) s_fn_CheckPtr(&wpIbp);
            if (*reinterpret_cast<uint8_t*>(reinterpret_cast<uintptr_t>(wpIbp) + 0x50) == 0)
                return false;
        }
    }

    if (*reinterpret_cast<uint8_t*>(desc + 0x25) == 0) return true;

    if (s_fn_CheckPtr) s_fn_CheckPtr(reinterpret_cast<void*>(su + 0x10));
    void* owner = *reinterpret_cast<void**>(su + 0x10);

    void* storeIbp = nullptr;
    if (s_fn_FindStorage) s_fn_FindStorage(reinterpret_cast<uintptr_t>(owner), &storeIbp);

    const uint32_t wantMask = *reinterpret_cast<uint32_t*>(desc + 0x28);
    bool matched = false;

    if (s_fn_CheckPtr) s_fn_CheckPtr(&storeIbp);
    uintptr_t store = reinterpret_cast<uintptr_t>(storeIbp);
    if (!store) return false;

    for (int i = 0; i < *reinterpret_cast<int*>(store + 0x30); ++i) {
        if (s_fn_CheckPtr) s_fn_CheckPtr(&storeIbp);
        store = reinterpret_cast<uintptr_t>(storeIbp);
        const uintptr_t slots = *reinterpret_cast<uintptr_t*>(store + 0x28);
        const uintptr_t good  = *reinterpret_cast<uintptr_t*>(slots + static_cast<uintptr_t>(i) * 0x10);

        uintptr_t rawMatDesc = 0;
        if (good && s_pTid_RawMatDesc) {
            const int       n   = *reinterpret_cast<int*>(good + 0x30);
            const uintptr_t arr = *reinterpret_cast<uintptr_t*>(good + 0x28);
            for (int j = 0; j < n; ++j) {
                void* c = *reinterpret_cast<void**>(arr + static_cast<uintptr_t>(j) * 8);
                if (!c) continue;
                typedef uint32_t (*GetTypeId_t)(void*);
                const uint32_t tid =
                    (reinterpret_cast<GetTypeId_t>(
                        *reinterpret_cast<uintptr_t*>(
                            *reinterpret_cast<uintptr_t*>(c) + 0x10)))(c);
                if (tid == *s_pTid_RawMatDesc) {
                    if (s_fn_IsDerivedFrom && s_fn_IsDerivedFrom(c, *s_pTid_RawMatDesc))
                        rawMatDesc = reinterpret_cast<uintptr_t>(c);
                    break;
                }
            }
        }
        // Faithful unguarded read -- see the note above.
        const uint32_t flags = *reinterpret_cast<uint32_t*>(rawMatDesc + 0x5c);
        matched |= (wantMask & flags) != 0;
    }
    return matched;
}

// ============================================================
// HappinessComponent::Register -- 0x14004a4f0
// ============================================================
// Stamp the happiness radius onto the map (a300 with state 1), append the component's
// IBP to the TownManager happiness list at *(GameWorld+0xF0)+0x50 (count at +0x58),
// and latch self[+0x28] so it is not registered twice.
extern "C" void HappinessComponent_Register(void* selfV) {
    HappinessComponent* self = reinterpret_cast<HappinessComponent*>(selfV);
    EnsureHelpers();

    if (s_fn_SetEntityIndex) s_fn_SetEntityIndex(self, 1);

    uint8_t basePtr[24] = {};
    if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor(basePtr, self);

    if (s_fn_ArrayPush && s_pGameWorld && *s_pGameWorld) {
        const uintptr_t townMgr = *reinterpret_cast<uintptr_t*>(
            reinterpret_cast<uintptr_t>(*s_pGameWorld) + 0xF0);
        if (townMgr) {
            const int count = *reinterpret_cast<int*>(townMgr + 0x58);
            s_fn_ArrayPush(reinterpret_cast<void*>(townMgr + 0x50), count + 1, basePtr);

            static int n = 0;
            if (n < 8) { ++n;
                fprintf(stderr, "[Happiness] source REGISTERED: comp=%p townMgr=%p "
                                "happinessCount=%d\n", selfV,
                        reinterpret_cast<void*>(townMgr),
                        *reinterpret_cast<int*>(townMgr + 0x58));
                fflush(stderr);
            }
        }
    }
    self->happinessFlag = 1;
}

// ============================================================
// HappinessComponent::Unregister -- 0x14004a4a0
// ============================================================
// The mirror of Register, and byte-identical to the body of slot 7 / the state-3 enter
// minus their `if (self[+0x28])` guard.
extern "C" void HappinessComponent_Unregister(void* selfV) {
    HappinessComponent* self = reinterpret_cast<HappinessComponent*>(selfV);
    EnsureHelpers();

    uint8_t basePtr[24] = {};
    if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor(basePtr, self);

    if (s_fn_ArrayEraseByValue && s_pGameWorld && *s_pGameWorld) {
        const uintptr_t townMgr = *reinterpret_cast<uintptr_t*>(
            reinterpret_cast<uintptr_t>(*s_pGameWorld) + 0xF0);
        if (townMgr)
            s_fn_ArrayEraseByValue(reinterpret_cast<void*>(townMgr + 0x50), basePtr);
    }
    if (s_fn_SetEntityIndex) s_fn_SetEntityIndex(self, -1);
    self->happinessFlag = 0;
}

// ============================================================
// HappinessComponent state 2 / 13 "built" enter -- 0x14004a9f0
// ============================================================
// Register on entry, but only if not already registered and currently active.
extern "C" char HappinessComponent_EnterBuilt(void* comp, void* /*sm*/) {
    HappinessComponent* self = reinterpret_cast<HappinessComponent*>(comp);
    const bool active = HappinessComponent_IsActive(comp);
    if (self->happinessFlag == 0 && active)
        HappinessComponent_Register(comp);
    return 0;
}

// ============================================================
// HappinessComponent state 2 / 13 trans38 tick -- 0x14004a9a0
// ============================================================
// The per-frame column (StateMachine::Tick's +0x38 handler). It re-evaluates IsActive
// every tick and moves the source in and out of the town list as the conditions change
// -- a tavern that runs out of ale drops out here and re-registers when it is restocked.
// This is the only state-table column HappinessComponent populates, which is why its
// RegisterState instantiation (0x140005e30) is the 3-argument one: the extra stack
// argument lands in the trans38 slot at table entry +0x18.
extern "C" char HappinessComponent_Tick38(void* comp, void* /*sm*/, void* /*time*/,
                                          int /*idx*/) {
    HappinessComponent* self = reinterpret_cast<HappinessComponent*>(comp);
    const bool active     = HappinessComponent_IsActive(comp);
    const bool registered = (self->happinessFlag != 0);

    if (!registered && active) {
        HappinessComponent_Register(comp);
    } else if (registered && !active) {
        HappinessComponent_Unregister(comp);
    }
    return 0;
}

// HappinessComponent state-3 "demolish" enter -- 0x14004a940. Byte-identical to the
// slot-7 removal hook (0x14004aa20), guard included.
extern "C" char HappinessComponent_EnterDemolish(void* comp, void* /*sm*/) {
    HappinessComponent_OnRemove(reinterpret_cast<HappinessComponent*>(comp));
    return 0;
}

// ===== Standalone factory =====
// The WellComponent pattern, with two differences read off vtable 0x1401595d0:
// slot 17 (CreateState) is 0x140005be0 -> self+0x30, NOT self+0x28 -- HappinessComponent
// carries its own byte at +0x28 and the controller sub-object is shifted -- and the
// state table has a third state (13) plus the trans38 column.
#include "Object/factory_registration.h"
#include "Component/component_factory_base.h"
#include "Component/controller_state_table.h"

namespace { struct HappinessCtrlTag {}; }
using HappinessCtrlDispatch = ::ControllerLive::Dispatcher<HappinessCtrlTag>;

// The retail table, transcribed from 0x14004ac10 call for call. Its RegisterState
// instantiation (0x140005e30) copies the caller's 5th stack argument into the entry's
// +0x18 field -- verified in the callee (it reads [rsp+0x170] into [rsp+0x48]), which is
// the trans38 column. States 2 and 13 pass 0x14004a9a0 there; state 3 passes null.
static void HappinessComponent_PopulateStateTable() {
    using namespace ::ControllerLive;
    StateTable& t = HappinessCtrlDispatch::table;
    if (t.count != 0) return;                      // idempotent
    TableRegisterState(t, 2, reinterpret_cast<void*>(&HappinessComponent_EnterBuilt),
                       nullptr, reinterpret_cast<void*>(&HappinessComponent_Tick38), nullptr);
    TableAddTransition(t, 3, nullptr);
    TableRegisterState(t, 13, reinterpret_cast<void*>(&HappinessComponent_EnterBuilt),
                       nullptr, reinterpret_cast<void*>(&HappinessComponent_Tick38), nullptr);
    TableAddTransition(t, 3, nullptr);
    TableRegisterState(t, 3, reinterpret_cast<void*>(&HappinessComponent_EnterDemolish),
                       nullptr, nullptr, nullptr);
    TableAddTransition(t, 6, nullptr);
    TableAddTransition(t, 4, nullptr);
    TableAddTransition(t, 5, nullptr);
}

static uint32_t HappinessComponent_LiveGetTypeId(void*) {
    static uint32_t cached = 0;
    if (cached == 0) cached = ::Object::GetTypeIdByName("HappinessComponent");
    return cached;
}
static void* HappinessComponent_LiveVtable[::ComponentFactory::kInertVtableSlots];

static void HappinessComponent_ShutdownVSlot(void* s, void* stream) {   // vtable index 3
    HappinessComponent_Shutdown(reinterpret_cast<HappinessComponent*>(s), stream);
}
static void HappinessComponent_OnRemoveVSlot(void* s) {                 // vtable index 7
    HappinessComponent_OnRemove(reinterpret_cast<HappinessComponent*>(s));
}
static int   HappinessComponent_GetDefaultState(void*) { return 2; }    // 0x1400026c0
static void* HappinessComponent_CreateState(void* self) {               // 0x140005be0
    return reinterpret_cast<uint8_t*>(self) + 0x30;
}

static void HappinessComponent_FinishLiveVtable(void* mem) {
    // Slot 3 (Shutdown/Serialize, 0x14004aa80) is wired UNCONDITIONALLY (save/load
    // wave 2026-08-28), same rationale as GrowthComponent/ResidenceComponent/
    // StorageComponent: without this, a loaded save's HappinessComponent kept the
    // base-Component no-op serialize and happinessFlag came back zeroed.
    HappinessComponent_LiveVtable[3] = reinterpret_cast<void*>(&HappinessComponent_ShutdownVSlot);
    HappinessComponent_LiveVtable[7]  = reinterpret_cast<void*>(&HappinessComponent_OnRemoveVSlot);
    HappinessComponent_LiveVtable[16] = reinterpret_cast<void*>(&HappinessComponent_GetDefaultState);
    HappinessComponent_LiveVtable[17] = reinterpret_cast<void*>(&HappinessComponent_CreateState);
    HappinessComponent_PopulateStateTable();
    if (mem) {
        // Embedded state node at +0x30 (slot 17), not +0x28.
        *reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(mem) + 0x30) =
            reinterpret_cast<void*>(HappinessCtrlDispatch::Vtable());
        *reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(mem) + 0x30 + 0x18) = mem;
    }
}

extern "C" void* HappinessComponent_Factory(void) {
    void* p = ::ComponentFactory::MakeInertTyped(0x50, 0x82,
        HappinessComponent_LiveVtable,
        reinterpret_cast<void*>(&HappinessComponent_LiveGetTypeId));
    HappinessComponent_FinishLiveVtable(p);
    return p;
}
static void* HappinessComponent_CreateInPlace(void* mem) {
    if (mem) ::ComponentFactory::InitInertTyped(mem, 0x50, 0x82,
        HappinessComponent_LiveVtable,
        reinterpret_cast<void*>(&HappinessComponent_LiveGetTypeId));
    HappinessComponent_FinishLiveVtable(mem);
    return mem;
}

REGISTER_TYPE(HappinessComponent, "ControllerComponent",
              &HappinessComponent_Factory, &HappinessComponent_CreateInPlace);
