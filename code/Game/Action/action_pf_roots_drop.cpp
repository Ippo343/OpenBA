// code/Game/Action/action_pf_roots_drop.cpp
// Wave 3 (Action) — DropOff / RemoveAnimal-ctor ROOT callers.
//
// Faithful 1:1 ports of the 4 root-caller exe helpers in the raw-material drop-off /
// RemoveAnimal-action closure. In standalone these resolve to 0 (silently skipped)
// unless registered; each is transcribed literally from the Ghidra decompile,
// cross-checked against disassembly for the call-site arg ABI. Do NOT "improve" — none
// of this is exercised on the autostart path, so a wrong port is undetectable latent
// corruption.
//
// Registration: RegisterPfRootsDropBatch() in standalone/reg_pf_roots_drop.cpp.
//
// Sub-callees (all registered elsewhere or in earlier batches — called via ResolveAddr,
// bodies NOT duplicated):
//   0x14009bd40 CreateRawMaterial_FindPlacementCell — ported by another agent THIS wave
//   0x1400c3720 ComponentEntity_CtorFromDescription (CopyCtor)
//   0x1400223c0 FindComponent (rawmat batch)
//   0x140129760 PositionEntity / helper_9760 (rawmat batch)
//   0x1400c48a0 EntityList_RegisterEntity
//   0x140006270 Entity_FindComponent_Storage (work_base)
//   0x1400012d0 Entity_FindComponent_Family (action_move)
//   0x14012d150 StorageComponent::SubtractItem (rawmat)
//   0x1401167c0 storage count-delta (action_pf_ctors)
//   0x140116760 storage drop-off (action_pf_ctors)
//   0x14012cd80 StorageComponent::CanAdd (engine_funcs fn_StorageCanAdd)
//   0x14012d000 StorageComponent::AddItem (rawmat)
//   0x14008d590 FindStorageForResource (engine_funcs fn_FindStorageForResource)
//   0x1400c9170 MoveTo_Action_Ctor (action_pf_ctors)
//   0x1400c9720 PerformWork_Action_Ctor7 (action_pf_ctors)
//   0x1400c5da0 CompleteTask_Action_CopyCtor (action_pf_ctors)
//   0x1400c71a0 CreateRawMaterial_Action_CopyCtor (action_pf_ctors)
//   0x1400cb1d0 SendComponentMessage_Action_CopyCtor (action_pf_ctors)
//   0x1400c54e0 action-stack push (action_pf_ctors) — bbea0/c7620 inline this; call helper
//   0x1400c5400 TaskListCleanup (work_base)
//   0x140107260 set-flag/refresh/Update == RemoveAnimal_helper_7260 (action_pf_ctors)
//   0x1400032e0 array-grow (component_people)
// Engine helpers (engine_funcs.h): fn_InternalBasePtr_Ctor, fn_InternalBasePtr_CheckPtr,
//   fn_InternalBasePtr_DeletePtr, fn_MemAlloc/fn_MemFreeStatic, fn_Time_Ctor_Float.
// Runtime exports: System::Random::RandTol, System::MemoryAllocator::MemAlloc.
//
// Vtable global written by bbea0: vtable_RemoveAnimal = 0x14015ffe0 (+ vtable_Action 0x14015faa0).

#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace {

// [diag] BANISHED_DELIVER_TRACE — watch the deposit half of the haul-to-storage leg.
static inline bool DeliverTraceOn() {
    static int s_ts = -1;
    if (s_ts < 0) { const char* e = std::getenv("BANISHED_DELIVER_TRACE"); s_ts = (e && e[0] && e[0] != '0') ? 1 : 0; }
    return s_ts == 1;
}
// --- Float constants (exe .rdata). ---
static const float* s_pDAT_172204 = nullptr;  // c78c0 RandTol(2) scale
static const float* s_pDAT_172280 = nullptr;  // MoveTo speed (c7620, bbea0)
static const float* s_pDAT_172194 = nullptr;  // PerformWork Time seconds (bbea0)
static const float* s_pDAT_1722a8 = nullptr;  // PerformWork animIndex float (bbea0)
static const float* s_pUVScale_17214c = nullptr;  // g_fUVScale — CreateRawMaterial CopyCtor float arg
// --- Type-id global (exe .rdata) for bbea0's SendComponentMessage message id. ---
static const uint32_t* s_pDAT_1c70e8 = nullptr;  // DWORD_1401c70e8
// --- Vtable addresses (exe .rdata). ---
static uintptr_t s_vt_Action = 0;
static uintptr_t s_vt_RemoveAnimal = 0;

// --- Registered exe sub-callees, resolved lazily via ResolveAddr. ---
typedef int  (*fn_bd40_t)(int* mapGrid, int cellIdx, char flag, void* outIBP, int z);  // 0x14009bd40
typedef void* (*fn_entityCopyCtor_t)(void* dst, int64_t src);          // 0x1400c3720
typedef void (*fn_223c0_t)(int64_t entity, void* outIBP);             // 0x1400223c0
typedef void (*fn_pos9760_t)(int64_t entity, int cell, float yaw);    // 0x140129760
typedef void (*fn_regEntity_t)(void* entityList, void* entityIBP, int category); // 0x1400c48a0
typedef void* (*fn_findStorage_t)(int64_t entity, void* outIBP);      // 0x140006270
typedef void* (*fn_findFamily_t)(int64_t entity, void* outIBP);       // 0x1400012d0
typedef void (*fn_storageSub_t)(int64_t storage, int64_t resType, int count); // 0x14012d150
typedef int  (*fn_countDelta_t)(int64_t self, int64_t resType);       // 0x1401167c0
typedef void (*fn_dropOff_t)(int64_t self, int64_t resType, int count); // 0x140116760
typedef uint32_t (*fn_canAdd_t)(void* storageComp, void* entityPtr, int qty); // 0x14012cd80
typedef void (*fn_addItem_t)(int64_t storageComp, int64_t entityPtr, int qty); // 0x14012d000
typedef void (*fn_findStorageForRes_t)(void** citizenList, void* outEntity, int cell,
                                       int64_t resType, int p5, char p6, float* vel, void* outIBP); // 0x14008d590
typedef void* (*fn_moveToCtor_t)(void* self, void* entity, uint32_t pathType, int p4,
                                 float* dest, int p6, int p7, float speed); // 0x1400c9170
typedef void* (*fn_perfWkCtor7_t)(void* self, void* entity, float animIdx, uint32_t actType,
                                  int64_t time, uint8_t p6, int p7);  // 0x1400c9720
typedef void* (*fn_completeCtor_t)(void* self, uint32_t p2, uint64_t* p3, uint64_t* p4, uint32_t p5); // 0x1400c5da0
typedef void* (*fn_createRawCtor_t)(void* self, uint64_t p2, float p3, uint64_t* p4, uint64_t* p5,
                                    uint64_t p6, uint32_t p7, uint32_t p8, uint8_t p9, uint64_t* p10, uint32_t p11); // 0x1400c71a0 (p3 float/XMM2)
typedef void* (*fn_sendMsgCtor_t)(void* self, uint64_t* p2, uint32_t* p3, uint32_t p4, uint64_t* p5, uint32_t p6); // 0x1400cb1d0
typedef void (*fn_push54e0_t)(int64_t* stack, void* srcEntry);        // 0x1400c54e0
typedef void (*fn_taskCleanup_t)(void* listAt48);                     // 0x1400c5400
typedef void (*fn_remHelper7260_t)(int64_t* self);                    // 0x140107260
typedef void (*fn_grow032e0_t)(void* self, int newCap);              // 0x1400032e0

static fn_bd40_t            s_fn_bd40 = nullptr;
static fn_entityCopyCtor_t  s_fn_entityCopyCtor = nullptr;
static fn_223c0_t           s_fn_223c0 = nullptr;
static fn_pos9760_t         s_fn_pos9760 = nullptr;
static fn_regEntity_t       s_fn_regEntity = nullptr;
static fn_findStorage_t     s_fn_findStorage = nullptr;
static fn_findFamily_t      s_fn_findFamily = nullptr;
static fn_storageSub_t      s_fn_storageSub = nullptr;
static fn_countDelta_t      s_fn_countDelta = nullptr;
static fn_dropOff_t         s_fn_dropOff = nullptr;
static fn_canAdd_t          s_fn_canAdd = nullptr;
static fn_addItem_t         s_fn_addItem = nullptr;
static fn_findStorageForRes_t s_fn_findStorageForRes = nullptr;
static fn_moveToCtor_t      s_fn_moveToCtor = nullptr;
static fn_perfWkCtor7_t     s_fn_perfWkCtor7 = nullptr;
static fn_completeCtor_t    s_fn_completeCtor = nullptr;
static fn_createRawCtor_t   s_fn_createRawCtor = nullptr;
static fn_sendMsgCtor_t     s_fn_sendMsgCtor = nullptr;
static fn_push54e0_t        s_fn_push = nullptr;
static fn_taskCleanup_t     s_fn_taskCleanup = nullptr;
static fn_remHelper7260_t   s_fn_remHelper7260 = nullptr;
static fn_grow032e0_t       s_fn_grow = nullptr;

static bool s_init = false;
static void EnsureInit() {
    if (s_init) return;
    s_pDAT_172204 = reinterpret_cast<const float*>(ResolveAddr(Module::Exe, 0x140172204));
    s_pDAT_172280 = reinterpret_cast<const float*>(ResolveAddr(Module::Exe, 0x140172280));
    s_pDAT_172194 = reinterpret_cast<const float*>(ResolveAddr(Module::Exe, 0x140172194));
    s_pDAT_1722a8 = reinterpret_cast<const float*>(ResolveAddr(Module::Exe, 0x1401722a8));
    s_pUVScale_17214c = reinterpret_cast<const float*>(ResolveAddr(Module::Exe, 0x14017214c));
    s_pDAT_1c70e8 = reinterpret_cast<const uint32_t*>(ResolveAddr(Module::Exe, 0x1401c70e8));
    s_vt_Action = ResolveAddr(Module::Exe, 0x14015faa0);
    s_vt_RemoveAnimal = ResolveAddr(Module::Exe, 0x14015ffe0);

    s_fn_bd40           = reinterpret_cast<fn_bd40_t>(ResolveAddr(Module::Exe, 0x14009bd40));
    s_fn_entityCopyCtor = reinterpret_cast<fn_entityCopyCtor_t>(ResolveAddr(Module::Exe, 0x1400c3720));
    s_fn_223c0          = reinterpret_cast<fn_223c0_t>(ResolveAddr(Module::Exe, 0x1400223c0));
    s_fn_pos9760        = reinterpret_cast<fn_pos9760_t>(ResolveAddr(Module::Exe, 0x140129760));
    s_fn_regEntity      = reinterpret_cast<fn_regEntity_t>(ResolveAddr(Module::Exe, 0x1400c48a0));
    s_fn_findStorage    = reinterpret_cast<fn_findStorage_t>(ResolveAddr(Module::Exe, 0x140006270));
    s_fn_findFamily     = reinterpret_cast<fn_findFamily_t>(ResolveAddr(Module::Exe, 0x1400012d0));
    s_fn_storageSub     = reinterpret_cast<fn_storageSub_t>(ResolveAddr(Module::Exe, 0x14012d150));
    s_fn_countDelta     = reinterpret_cast<fn_countDelta_t>(ResolveAddr(Module::Exe, 0x1401167c0));
    s_fn_dropOff        = reinterpret_cast<fn_dropOff_t>(ResolveAddr(Module::Exe, 0x140116760));
    s_fn_canAdd         = reinterpret_cast<fn_canAdd_t>(ResolveAddr(Module::Exe, 0x14012cd80));
    s_fn_addItem        = reinterpret_cast<fn_addItem_t>(ResolveAddr(Module::Exe, 0x14012d000));
    s_fn_findStorageForRes = reinterpret_cast<fn_findStorageForRes_t>(ResolveAddr(Module::Exe, 0x14008d590));
    s_fn_moveToCtor     = reinterpret_cast<fn_moveToCtor_t>(ResolveAddr(Module::Exe, 0x1400c9170));
    s_fn_perfWkCtor7    = reinterpret_cast<fn_perfWkCtor7_t>(ResolveAddr(Module::Exe, 0x1400c9720));
    s_fn_completeCtor   = reinterpret_cast<fn_completeCtor_t>(ResolveAddr(Module::Exe, 0x1400c5da0));
    s_fn_createRawCtor  = reinterpret_cast<fn_createRawCtor_t>(ResolveAddr(Module::Exe, 0x1400c71a0));
    s_fn_sendMsgCtor    = reinterpret_cast<fn_sendMsgCtor_t>(ResolveAddr(Module::Exe, 0x1400cb1d0));
    s_fn_push           = reinterpret_cast<fn_push54e0_t>(ResolveAddr(Module::Exe, 0x1400c54e0));
    s_fn_taskCleanup    = reinterpret_cast<fn_taskCleanup_t>(ResolveAddr(Module::Exe, 0x1400c5400));
    s_fn_remHelper7260  = reinterpret_cast<fn_remHelper7260_t>(ResolveAddr(Module::Exe, 0x140107260));
    s_fn_grow           = reinterpret_cast<fn_grow032e0_t>(ResolveAddr(Module::Exe, 0x1400032e0));
    s_init = true;
}

// MemAlloc shim matching the original `System::MemoryAllocator::MemAlloc(size, align)`
// (the engine_funcs wrapper takes the singleton as its first argument).
static void* MemAlloc(int64_t size, int64_t align) {
    return fn_MemAlloc ? fn_MemAlloc(g_pMemAllocSingleton, static_cast<uint64_t>(size),
                                     static_cast<uint64_t>(align))
                       : nullptr;
}
} // namespace

// ============================================================
// FUN_1400c78c0 — spawn a raw-material entity on a found map cell, then subtract the
// dropped amount from the source storage.
// param_1 = action self (entity IBP at +0x30); param_2 = source ComponentEntity ptr;
// param_3 = amount. If amount != 0: pick a placement cell near the entity's map position
// via 9bd40; on success copy-construct an entity, wire its component (223c0), position it
// (9760) with a small random yaw, set +0x28=amount/+0x30=0, register it in the world
// entity list (c48a0). Finally subtract `amount` from the source storage (12d150).
// ============================================================
void Pf_SpawnRawMaterialOnCell_78c0(int64_t param_1, int64_t param_2, int param_3) {
    EnsureInit();
    void* gworld = *g_pGameWorldSingleton;
    if (param_3 == 0) return;

    int64_t local_res20 = 0;
    int64_t local_48 = 0;
    void* local_40 = nullptr;
    uint32_t local_38 = 0;

    int* piVar2 = *reinterpret_cast<int**>(reinterpret_cast<uintptr_t>(gworld) + 0xf8);
    if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor(&local_res20, nullptr);
    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(reinterpret_cast<void*>(param_1 + 0x30));
    int64_t lVar3 = *reinterpret_cast<int64_t*>(param_1 + 0x30);
    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(reinterpret_cast<void*>(lVar3 + 0x278));
    lVar3 = *reinterpret_cast<int64_t*>(lVar3 + 0x278);
    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(reinterpret_cast<void*>(lVar3 + 0x68));
    lVar3 = *reinterpret_cast<int64_t*>(lVar3 + 0x68);
    local_38 = *reinterpret_cast<uint32_t*>(lVar3 + 0x160);
    int iVar1 = *piVar2;
    int iVar6 = static_cast<int>(*reinterpret_cast<float*>(lVar3 + 0x158) + static_cast<float>(iVar1 >> 1));
    int iVar5 = static_cast<int>(*reinterpret_cast<float*>(lVar3 + 0x15c) + static_cast<float>(iVar1 >> 1));
    void* ptr = nullptr;
    if (iVar6 < 0) iVar6 = 0;
    if (iVar1 <= iVar6) iVar6 = iVar1 - 1;
    if (iVar5 < 0) iVar5 = 0;
    if (iVar1 <= iVar5) iVar5 = iVar1 - 1;
    int cell = s_fn_bd40 ? s_fn_bd40(piVar2, iVar1 * iVar5 + iVar6, '\0', &local_res20, 0) : -1;
    if (-1 < cell) {
        local_40 = MemAlloc(0x48, 8);
        if (local_40 != nullptr) {
            ptr = s_fn_entityCopyCtor ? s_fn_entityCopyCtor(local_40, param_2) : nullptr;
        }
        if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor(&local_48, ptr);
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&local_48);
        if (s_fn_223c0) s_fn_223c0(local_48, &local_res20);
        void* random = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(gworld) + 0xd8);
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&local_res20);
        int64_t entAdj = local_res20;
        int randv = (fn_Random_RandTol && random) ? fn_Random_RandTol(random, 2) : 0;
        if (s_fn_pos9760)
            s_fn_pos9760(entAdj, cell,
                         static_cast<float>(randv) * (s_pDAT_172204 ? *s_pDAT_172204 : 0.0f));
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&local_res20);
        *reinterpret_cast<int*>(local_res20 + 0x28) = param_3;
        *reinterpret_cast<uint64_t*>(local_res20 + 0x30) = 0;
        local_40 = reinterpret_cast<void*>(local_48);
        if (s_fn_regEntity)
            s_fn_regEntity(*reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(gworld) + 0xe8),
                           &local_40, *reinterpret_cast<int*>(param_2 + 0x20));
    }
    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(reinterpret_cast<void*>(param_1 + 0x30));
    lVar3 = *reinterpret_cast<int64_t*>(param_1 + 0x30);
    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(reinterpret_cast<void*>(lVar3 + 0x10));
    if (s_fn_findStorage) s_fn_findStorage(*reinterpret_cast<int64_t*>(lVar3 + 0x10), &local_res20);
    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&local_res20);
    if (s_fn_storageSub) s_fn_storageSub(local_res20, param_2, param_3);
    (void)local_38;
}

// ============================================================
// FUN_1400c7570 — iterate the source storage's resource stacks; for each stack with a
// positive count, spill it onto a placed entity via 78c0. Re-reads the storage count each
// iteration because 78c0 mutates it (12d150 compacts the array). param_1 = action self.
// ============================================================
void Pf_SpillAllStorage_7570(int64_t param_1) {
    EnsureInit();
    int64_t local_res8 = 0;
    int64_t local_res10 = 0;
    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(reinterpret_cast<void*>(param_1 + 0x30));
    local_res10 = *reinterpret_cast<int64_t*>(*reinterpret_cast<int64_t*>(param_1 + 0x30) + 0x10);
    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&local_res10);
    if (s_fn_findStorage) s_fn_findStorage(local_res10, &local_res8);
    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&local_res8);
    int iVar1 = *reinterpret_cast<int*>(local_res8 + 0x30);
    while (0 < iVar1) {
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&local_res8);
        int64_t* arr = *reinterpret_cast<int64_t**>(local_res8 + 0x28);
        iVar1 = static_cast<int>(arr[1]);  // first stack's count
        if (0 < iVar1) {
            Pf_SpawnRawMaterialOnCell_78c0(param_1, *arr, iVar1);  // *arr = first stack's resType
        }
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&local_res8);
        iVar1 = *reinterpret_cast<int*>(local_res8 + 0x30);
    }
}

// ============================================================
// FUN_1400c7620 — DropOffRawMaterial action executor (deposit-or-move branch).
// param_1 = action self; param_2 = action-stack list to push onto; param_3 = dest storage
// IBP; param_4 = resource type. Computes a deposit amount = min(self.+0x24 cap, available
// from 1167c0); if it fits (cd80 CanAdd) deposit it (d000 AddItem + 116760 source subtract)
// and return 1. Otherwise, if self.+0x20 flag set, search for a storage destination cell
// (8d590); if none, spill via 78c0 and return 2; else stash the found cell at self.+0x38,
// build a MoveTo action toward it and push it (c54e0). Returns 1.
// ============================================================
uint64_t Pf_DropOffExec_7620(int64_t param_1, int64_t* param_2, void* param_3, uint64_t param_4) {
    EnsureInit();
    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(reinterpret_cast<void*>(param_1 + 0x30));
    int iVar5 = s_fn_countDelta ? s_fn_countDelta(*reinterpret_cast<int64_t*>(param_1 + 0x30), param_4) : 0;
    int iVar8 = *reinterpret_cast<int*>(param_1 + 0x24);
    int iVar9 = iVar5;
    if ((-1 < iVar8) && (iVar9 = iVar8, iVar5 < iVar8)) {
        iVar9 = iVar5;
    }
    if (0 < iVar9) {
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(param_3);
        uint32_t uVar6 = s_fn_canAdd ? s_fn_canAdd(*reinterpret_cast<void**>(param_3), reinterpret_cast<void*>(param_4), iVar9) : 0;
        // Open C: when a fetch-for-home trip reaches the house and CanAdd says no, the
        // load bounces to the nearest acceptor — which is the barn it came from, so the
        // whole haul becomes a round trip and the house stays cold. Name the reason:
        // an accept-mask miss (+0x3c) and a capacity miss (+0x38 vs QueryCount) look
        // identical from the outside but need opposite fixes.
        if (static_cast<char>(uVar6) == '\0' && DeliverTraceOn()) {
            uint8_t* ds = *reinterpret_cast<uint8_t**>(param_3);
            std::fprintf(stderr, "[Deliver] REFUSED destStorage=%p amount=%d resType=%p "
                                 "cap(+0x38)=%d used(+0x34)=%d acceptMask(+0x3c)=0x%x "
                                 "slots(+0x30)=%d\n",
                         (void*)ds, iVar9, reinterpret_cast<void*>(param_4),
                         ds ? *reinterpret_cast<int32_t*>(ds + 0x38) : 0,
                         ds ? *reinterpret_cast<int32_t*>(ds + 0x34) : 0,
                         ds ? *reinterpret_cast<uint32_t*>(ds + 0x3c) : 0u,
                         ds ? *reinterpret_cast<int32_t*>(ds + 0x30) : 0);
            std::fflush(stderr);
        }
        if (static_cast<char>(uVar6) != '\0') {
            if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(param_3);
            if (s_fn_addItem) s_fn_addItem(*reinterpret_cast<int64_t*>(param_3), param_4, iVar9);
            if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(reinterpret_cast<void*>(param_1 + 0x30));
            if (s_fn_dropOff) s_fn_dropOff(*reinterpret_cast<int64_t*>(param_1 + 0x30), param_4, iVar9);
            if (DeliverTraceOn()) {
                std::fprintf(stderr, "[Deliver] DEPOSIT amount=%d resType=%p -> destStorage=%p "
                                     "(banked; carry subtracted)\n",
                             iVar9, reinterpret_cast<void*>(param_4),
                             *reinterpret_cast<void**>(param_3));
                std::fflush(stderr);
            }
            return 1;
        }
    }
    if (*reinterpret_cast<char*>(param_1 + 0x20) != '\0') {
        void* gworld = *g_pGameWorldSingleton;
        int* piVar2 = *reinterpret_cast<int**>(reinterpret_cast<uintptr_t>(gworld) + 0xf8);
        void** pplVar3 = *reinterpret_cast<void***>(reinterpret_cast<uintptr_t>(gworld) + 0xf0);
        float local_50[6] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
        void* ptr = nullptr;
        int64_t local_res8 = 0;
        int64_t local_70[2] = {0, 0};
        uint8_t local_60[8] = {0,0,0,0,0,0,0,0};  // InternalBasePtr local_60 (Family find out)
        uint32_t local_58 = 0;

        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(reinterpret_cast<void*>(param_1 + 0x30));
        int64_t lVar4 = *reinterpret_cast<int64_t*>(param_1 + 0x30);
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(reinterpret_cast<void*>(lVar4 + 0x278));
        lVar4 = *reinterpret_cast<int64_t*>(lVar4 + 0x278);
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(reinterpret_cast<void*>(lVar4 + 0x68));
        lVar4 = *reinterpret_cast<int64_t*>(lVar4 + 0x68);
        local_58 = *reinterpret_cast<uint32_t*>(lVar4 + 0x160);
        int iVar1 = *piVar2;
        iVar8 = static_cast<int>(*reinterpret_cast<float*>(lVar4 + 0x158) + static_cast<float>(iVar1 >> 1));
        iVar5 = static_cast<int>(*reinterpret_cast<float*>(lVar4 + 0x15c) + static_cast<float>(iVar1 >> 1));
        if (iVar8 < 0) iVar8 = 0;
        if (iVar1 <= iVar8) iVar8 = iVar1 - 1;
        if (iVar5 < 0) iVar5 = 0;
        if (iVar1 <= iVar5) iVar5 = iVar1 - 1;
        if (s_fn_findStorageForRes)
            s_fn_findStorageForRes(pplVar3, local_70, iVar1 * iVar5 + iVar8, param_4, iVar9, '\x01',
                                   local_50, &local_res8);
        if (local_70[0] == 0) {
            if (DeliverTraceOn()) {
                std::fprintf(stderr, "[Deliver] SPILL resType=%p amount=%d (no storage found; "
                                     "dropping a pile on the ground)\n",
                             reinterpret_cast<void*>(param_4), iVar9);
                std::fflush(stderr);
            }
            Pf_SpawnRawMaterialOnCell_78c0(param_1, param_4, iVar9);
            return 2;
        }
        if (DeliverTraceOn()) {
            std::fprintf(stderr, "[Deliver] WALK-TO-STORAGE dest=%p (pushing MoveTo; deposit "
                                 "retries on arrival)\n",
                         reinterpret_cast<void*>(local_70[0]));
            std::fflush(stderr);
        }
        *reinterpret_cast<int64_t*>(param_1 + 0x38) = local_70[0];
        void* self = MemAlloc(0x480, 8);
        if (self != nullptr) {
            if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(reinterpret_cast<void*>(param_1 + 0x30));
            local_res8 = *reinterpret_cast<int64_t*>(*reinterpret_cast<int64_t*>(param_1 + 0x30) + 0x10);
            if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&local_res8);
            void* pIVar7 = s_fn_findFamily ? s_fn_findFamily(local_res8, local_60) : nullptr;
            ptr = s_fn_moveToCtor ? s_fn_moveToCtor(self, pIVar7, /*Slow*/1, 0, local_50, 0xe1f, 0,
                                                    s_pDAT_172280 ? *s_pDAT_172280 : 0.0f) : nullptr;
        }
        if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor(local_70, ptr);
        if (s_fn_push) s_fn_push(param_2, local_70);
        (void)local_58;
    }
    return 1;
}

// ============================================================
// FUN_1400bbea0 — RemoveAnimal_Action Ctor. Builds a child-action sub-tree on the action
// list at (entity.+0x278).+0x48. param_1 = entity IBP; param_2 = a CompleteTask arg;
// param_3 = MoveTo destination vec3; param_4 = CreateRawMaterial flag arg; param_5 = a
// second-entity IBP; param_6 = a description-list (count +0x50, array +0x48); param_7 = a
// gate flag (low byte). If gated, pushes a SendComponentMessage + CompleteTask pair. Then
// for each entry in param_6 pushes a CreateRawMaterial action; appends a RemoveAnimal
// (raw 0x28-byte action), a PerformWork, and a MoveTo; finally calls 107260 on param_5.
// The original inlined the action-stack push (c54e0) at each append site — we call the
// registered push helper (byte-identical behaviour).
// ============================================================
void Pf_RemoveAnimalCtor_bbea0(void* param_1, uint32_t param_2, uint32_t* param_3, uint32_t param_4,
                               void* param_5, int64_t param_6, uint32_t param_7) {
    EnsureInit();

    int64_t local_78 = 0;
    uint64_t local_70 = 0;
    void* local_68 = nullptr;
    int64_t local_res8_v = 0;       // longlong* local_res8 (IBP out / scratch)
    uint8_t local_58[32] = {0};     // Internal* local_58[4] / Time scratch

    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(param_1);
    int64_t lVar3 = *reinterpret_cast<int64_t*>(param_1);
    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(reinterpret_cast<void*>(lVar3 + 0x278));
    lVar3 = *reinterpret_cast<int64_t*>(lVar3 + 0x278);
    int64_t* plVar18 = reinterpret_cast<int64_t*>(lVar3 + 0x48);  // action-stack list

    if (static_cast<char>(param_7) != '\0') {
        if (s_fn_taskCleanup) s_fn_taskCleanup(plVar18);
        // --- SendComponentMessage child action ---
        void* pIVar7 = MemAlloc(0x40, 8);
        void* pIVar8 = nullptr;
        local_68 = pIVar7;
        if (pIVar7 != nullptr) {
            if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor(&local_res8_v, nullptr);
            uint32_t msgId = s_pDAT_1c70e8 ? *s_pDAT_1c70e8 : 0;  // param_7 = DWORD_1401c70e8
            if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(param_1);
            local_78 = *reinterpret_cast<int64_t*>(*reinterpret_cast<int64_t*>(param_1) + 0x10);
            pIVar8 = s_fn_sendMsgCtor ? s_fn_sendMsgCtor(pIVar7, reinterpret_cast<uint64_t*>(&local_78),
                                                         &msgId, 0, reinterpret_cast<uint64_t*>(&local_res8_v), 0)
                                      : nullptr;
        }
        if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor(&local_70, pIVar8);
        if (*reinterpret_cast<int64_t*>(lVar3 + 0x58) != 0) {
            if (fn_InternalBasePtr_DeletePtr) fn_InternalBasePtr_DeletePtr(reinterpret_cast<void*>(lVar3 + 0x58));
        }
        *reinterpret_cast<uint64_t*>(lVar3 + 0x58) = local_70;
        // --- CompleteTask child action ---
        void* pIVar8b = MemAlloc(0x38, 8);
        void* pIVar9 = nullptr;
        local_68 = pIVar8b;
        if (pIVar8b != nullptr) {
            if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(param_5);
            local_res8_v = *reinterpret_cast<int64_t*>(*reinterpret_cast<int64_t*>(param_5) + 0x10);
            local_70 = *reinterpret_cast<uint64_t*>(param_1);
            pIVar9 = s_fn_completeCtor ? s_fn_completeCtor(pIVar8b, param_2,
                                                           &local_70, reinterpret_cast<uint64_t*>(&local_res8_v), 0)
                                       : nullptr;
        }
        if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor(&local_78, pIVar9);
        if (s_fn_push) s_fn_push(plVar18, &local_78);
    }

    // --- find Storage on entity (+0x10) for the CreateRawMaterial loop ---
    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(param_1);
    local_res8_v = *reinterpret_cast<int64_t*>(*reinterpret_cast<int64_t*>(param_1) + 0x10);
    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&local_res8_v);
    void* pStorIBP = s_fn_findStorage ? s_fn_findStorage(local_res8_v, &local_68) : nullptr;
    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(pStorIBP);
    float fVar6 = s_pUVScale_17214c ? *s_pUVScale_17214c : 0.0f;  // g_fUVScale
    uint32_t uVar1 = *reinterpret_cast<uint32_t*>(*reinterpret_cast<int64_t*>(pStorIBP) + 0x38);
    (void)uVar1;  // faithful dead load from the original (decompiler-surfaced; no side effect)

    // --- CreateRawMaterial action per description entry ---
    if (0 < *reinterpret_cast<int*>(param_6 + 0x50)) {
        int iVar20 = 0;
        do {
            void* pIVar8 = MemAlloc(0x58, 8);
            void* pIVar9 = nullptr;
            if (pIVar8 != nullptr) {
                // Disasm-verified arg setup (Ghidra's bbea0 decompile aliased the integer
                // args; the disassembly is ground truth):
                //   arg2 = *(*(param_6+0x48)+iVar20*8)   (description entry)
                //   arg3 = fVar6 (XMM2/g_fUVScale float, stored at ctor +0x4c)
                //   arg4 = &local_78  (local_78 = *param_1)
                //   arg5 = &local_70  (local_70 = (*param_1)+0x248)
                //   arg6 = <unwritten by caller — uninitialized, ctor never reads it>
                //   arg7 = 0          (R12D)
                //   arg8 = param_4    (R13D = bbea0's 4th arg)
                //   arg9 = 1
                //   arg10 = &local_res8_v  (local_res8_v = (*param_1)+0x248, a 2nd copy)
                //   arg11 = 0xc
                if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(param_1);
                local_res8_v = *reinterpret_cast<int64_t*>(*reinterpret_cast<int64_t*>(param_1) + 0x248);
                if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(param_1);
                local_78 = *reinterpret_cast<int64_t*>(param_1);
                local_70 = *reinterpret_cast<uint64_t*>(local_78 + 0x248);
                pIVar9 = s_fn_createRawCtor ? s_fn_createRawCtor(
                    pIVar8,
                    *reinterpret_cast<uint64_t*>(*reinterpret_cast<int64_t*>(param_6 + 0x48) + static_cast<int64_t>(iVar20) * 8),
                    fVar6,   // float arg3 -> XMM2 (ctor MOVSS to +0x4c); pass the value, not its bits
                    reinterpret_cast<uint64_t*>(&local_78),
                    &local_70,
                    0 /*arg6 uninitialized — unused by ctor*/,
                    0 /*arg7 = R12D = 0*/, param_4 /*arg8 = R13D*/, 1, reinterpret_cast<uint64_t*>(&local_res8_v), 0xc) : nullptr;
            }
            if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor(&local_68, pIVar9);
            if (s_fn_push) s_fn_push(plVar18, &local_68);
            iVar20 = iVar20 + 1;
        } while (iVar20 < *reinterpret_cast<int*>(param_6 + 0x50));
    }

    // --- RemoveAnimal action (raw 0x28-byte, no dedicated ctor) ---
    {
        void* pIVar9 = MemAlloc(0x28, 8);
        if (pIVar9 != nullptr) {
            if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(param_5);
            uint64_t uVar4 = *reinterpret_cast<uint64_t*>(*reinterpret_cast<int64_t*>(param_5) + 0x10);
            if (fn_ObjectInternal_Ctor) fn_ObjectInternal_Ctor(pIVar9);
            uint8_t* s = reinterpret_cast<uint8_t*>(pIVar9);
            *reinterpret_cast<uintptr_t*>(s) = s_vt_Action;
            *reinterpret_cast<uint64_t*>(s + 0x10) = 0;
            s[0x18] = 0;
            *reinterpret_cast<uintptr_t*>(s) = s_vt_RemoveAnimal;
            *reinterpret_cast<uint64_t*>(s + 0x20) = uVar4;
        }
        if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor(&local_res8_v, pIVar9);
        if (s_fn_push) s_fn_push(plVar18, &local_res8_v);
    }

    // --- PerformWork action ---
    {
        void* pIVar13 = MemAlloc(0x48, 8);
        void* pvVar11 = nullptr;
        if (pIVar13 != nullptr) {
            local_res8_v = *reinterpret_cast<int64_t*>(param_1);
            uint64_t timeStore = 0;
            if (fn_Time_Ctor_Float) fn_Time_Ctor_Float(local_58, s_pDAT_172194 ? *s_pDAT_172194 : 0.0f);
            timeStore = *reinterpret_cast<uint64_t*>(local_58);
            pvVar11 = s_fn_perfWkCtor7 ? s_fn_perfWkCtor7(pIVar13, &local_res8_v,
                                                          s_pDAT_1722a8 ? *s_pDAT_1722a8 : 0.0f,
                                                          0xb /*Fast|ShowGroup*/, static_cast<int64_t>(timeStore),
                                                          1, 2) : nullptr;
        }
        if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor(&local_68, pvVar11);
        if (s_fn_push) s_fn_push(plVar18, &local_68);
    }

    // --- MoveTo action toward param_3 ---
    {
        void* pIVar13 = MemAlloc(0x480, 8);
        void* pvVar11 = nullptr;
        if (pIVar13 != nullptr) {
            if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(param_1);
            local_res8_v = *reinterpret_cast<int64_t*>(*reinterpret_cast<int64_t*>(param_1) + 0x10);
            if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&local_res8_v);
            void* pIVar15 = s_fn_findFamily ? s_fn_findFamily(local_res8_v, local_58) : nullptr;
            pvVar11 = s_fn_moveToCtor ? s_fn_moveToCtor(pIVar13, pIVar15, /*Slow*/1, 0,
                                                        reinterpret_cast<float*>(param_3), 0xe1f, 0,
                                                        s_pDAT_172280 ? *s_pDAT_172280 : 0.0f) : nullptr;
        }
        if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor(&local_68, pvVar11);
        if (s_fn_push) s_fn_push(plVar18, &local_68);
    }

    // --- finalise on param_5 ---
    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(pStorIBP);
    if (s_fn_remHelper7260) s_fn_remHelper7260(*reinterpret_cast<int64_t**>(pStorIBP));
    (void)plVar18; (void)param_3;
}
