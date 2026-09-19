// code/Game/Component/citizenai_handlers.cpp
//
// Faithful C++ ports of the CitizenAIComponent state HANDLERS (Brick 3b) — the
// functions ValidateAndEnterState dispatches per state-id. Each handler that
// PUSHES an Action/Work object onto the citizen's action stack
// (CharacterComponent+0x48) makes its state ENTERABLE: once an action is pushed,
// CharacterComponent+0x60 (stack top index) becomes >= 0, TransitionState records
// the state id, and CitizenAIComponent::Update returns early at its tail (the
// stack-non-empty gate) so no re-entry/unbounded growth occurs while the action
// executor (move-tick, Brick 4b) is still inert.
//
// Functions ported in this file:
//   0x1400c26c0  PlayAnimation_Action_Ctor5  (the 3-action builder: pushes a
//                SendComponentMessage[+0x58] + optional PlayAnimation + MoveTo)
//   0x14010a2b0  CitizenAI_State_Wander
//
// All exe sub-callees are already registered (resolved via ResolveAddr):
//   0x14009ccf0  WorkGrid_FindCell        0x14009f3e0  MapGetTerrainPos
//   0x1400c9170  MoveTo_Action_Ctor       0x1400c54e0  ActionStack push
//   0x1400c5400  ActionStack cleanup      0x1400cb1d0  SendComponentMessage CopyCtor
//   0x1400012d0  Entity_FindComponent_Family
// Action vtables (exe .rdata): vtable_Action 0x14015faa0, vtable_PlayAnimation
//   0x140158950.  Float consts: 0x140172114 (0.5), 0x140172180 (3.0),
//   0x1401721a0 (7.0), 0x140172280 (-1.0, MoveTo randomized-speed sentinel).
//


#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B
#include "system/Value.h"   // ENDGAME B.3.4b System caller batch

namespace {

// --- registered exe sub-callees (called via ResolveAddr) ---
typedef void  (*fn_taskCleanup_t)(int64_t* stackAt48);                       // 0x1400c5400
typedef void  (*fn_actPush_t)(int64_t* stackAt48, void* srcIBP);            // 0x1400c54e0
typedef void* (*fn_sendMsgCopyCtor_t)(void* mem, uint64_t* entityIBP,
                                      uint32_t* msgId, uint32_t a4,
                                      uint64_t* destIBP, uint32_t a6);       // 0x1400cb1d0
typedef void* (*fn_findFamily_t)(uintptr_t entity, void* retIBP);           // 0x1400012d0
typedef void* (*fn_moveToCtor_t)(void* self, void* familyIBP, uint32_t pathType,
                                 int a4, float* dest, int a6, int a7, float speed); // 0x1400c9170
typedef int   (*fn_findCell_t)(int* grid, int from, int to, uint32_t mask); // 0x14009ccf0
typedef float* (*fn_terrainPos_t)(void* game, float* out3, float* inXY2,
                                  char flag, void* outTri);                 // 0x14009f3e0

static fn_taskCleanup_t     s_fn_taskCleanup    = nullptr;
static fn_actPush_t         s_fn_actPush        = nullptr;
static fn_sendMsgCopyCtor_t s_fn_sendMsgCopy    = nullptr;
static fn_findFamily_t      s_fn_findFamily     = nullptr;
static fn_moveToCtor_t      s_fn_moveToCtor     = nullptr;
static fn_findCell_t        s_fn_findCell       = nullptr;
static fn_terrainPos_t      s_fn_terrainPos     = nullptr;

// --- action vtable addresses (exe .rdata) ---
static uintptr_t s_vt_Action        = 0;   // 0x14015faa0
static uintptr_t s_vt_PlayAnimation = 0;   // 0x140158950

// --- float / dword constants (exe .rdata) ---
static const uint32_t* s_pMsgId_c70e8 = nullptr;  // DWORD_1401c70e8 (SendComponentMessage id)
static const float*    s_pf_172114    = nullptr;   // 0.5  (cell-centre bias)
static const float*    s_pf_172180    = nullptr;   // 3.0  (anim-time base)
static const float*    s_pf_1721a0    = nullptr;   // 7.0  (anim-time scale)
static const float*    s_pf_172280    = nullptr;   // -1.0 (MoveTo speed sentinel)

static bool s_init = false;
static void EnsureInit() {
    if (s_init) return;
    s_fn_taskCleanup = reinterpret_cast<fn_taskCleanup_t>(ResolveAddr(Module::Exe, 0x1400c5400));
    s_fn_actPush     = reinterpret_cast<fn_actPush_t>(ResolveAddr(Module::Exe, 0x1400c54e0));
    s_fn_sendMsgCopy = reinterpret_cast<fn_sendMsgCopyCtor_t>(ResolveAddr(Module::Exe, 0x1400cb1d0));
    s_fn_findFamily  = reinterpret_cast<fn_findFamily_t>(ResolveAddr(Module::Exe, 0x1400012d0));
    s_fn_moveToCtor  = reinterpret_cast<fn_moveToCtor_t>(ResolveAddr(Module::Exe, 0x1400c9170));
    s_fn_findCell    = reinterpret_cast<fn_findCell_t>(ResolveAddr(Module::Exe, 0x14009ccf0));
    s_fn_terrainPos  = reinterpret_cast<fn_terrainPos_t>(ResolveAddr(Module::Exe, 0x14009f3e0));
    s_vt_Action        = ResolveAddr(Module::Exe, 0x14015faa0);
    s_vt_PlayAnimation = ResolveAddr(Module::Exe, 0x140158950);
    s_pMsgId_c70e8 = reinterpret_cast<const uint32_t*>(ResolveAddr(Module::Exe, 0x1401c70e8));
    s_pf_172114 = reinterpret_cast<const float*>(ResolveAddr(Module::Exe, 0x140172114));
    s_pf_172180 = reinterpret_cast<const float*>(ResolveAddr(Module::Exe, 0x140172180));
    s_pf_1721a0 = reinterpret_cast<const float*>(ResolveAddr(Module::Exe, 0x1401721a0));
    s_pf_172280 = reinterpret_cast<const float*>(ResolveAddr(Module::Exe, 0x140172280));
    s_init = true;
}

} // namespace

// ============================================================
// 0x1400c26c0 — PlayAnimation_Action_Ctor5
// ------------------------------------------------------------
// Builds the citizen's wander/animation action triple and installs it:
//   (1) FUN_1400c5400 clears the action stack at CharacterComponent+0x48.
//   (2) Allocates a SendComponentMessage action (0x40 bytes), CopyCtor'd from the
//       entity IBP + msg-id DWORD_1401c70e8, and stores its IBP at
//       CharacterComponent+0x58 (DeletePtr the old one first).
//   (3) If animTime > 0: allocates a PlayAnimation action (0x40 bytes), inits its
//       vtable/entity(+0x20)/index(+0x28=0)/Time(+0x30)/+0x38=0, and PUSHES it.
//   (4) Allocates a MoveTo action (0x480 bytes), constructs it via MoveTo_Action_Ctor
//       toward `dest`, and PUSHES it.
//
// param_1 = InternalBasePtr* whose [0] = the CitizenComponent.
// param_2 = float* destination (world XY+; z filled by terrain query upstream).
// param_3 = animation time in seconds.
//
// chain: citizen = *param_1 ; character = *(citizen+0x278) ; stack = character+0x48.
// ============================================================
void CitizenAI_PlayAnimationActionCtor5(void* param_1, float* dest, float animTime) {
    EnsureInit();
    int64_t citizen = *reinterpret_cast<int64_t*>(param_1);
    int64_t character = *reinterpret_cast<int64_t*>(citizen + 0x278);
    int64_t* stack = reinterpret_cast<int64_t*>(character + 0x48);

    if (s_fn_taskCleanup) s_fn_taskCleanup(stack);

    // (2) SendComponentMessage action -> CharacterComponent+0x58
    void* msgAction = nullptr;
    void* mem = (fn_MemAlloc && g_pMemAllocSingleton)
                ? fn_MemAlloc(g_pMemAllocSingleton, 0x40, 8) : nullptr;
    if (mem) {
        int64_t destIBP = 0;
        if (fn_InternalBasePtr_Ctor) new (&destIBP) ::Object::InternalBasePtr(nullptr);   // B.3.3 bridge
        uint32_t msgId[2] = { s_pMsgId_c70e8 ? *s_pMsgId_c70e8 : 0u, 0u };
        uint64_t entityIBP = *reinterpret_cast<uint64_t*>(*reinterpret_cast<int64_t*>(param_1) + 0x10);
        msgAction = s_fn_sendMsgCopy
                    ? s_fn_sendMsgCopy(mem, &entityIBP, msgId, 0, reinterpret_cast<uint64_t*>(&destIBP), 0)
                    : mem;
    }
    int64_t msgIBP = 0;
    if (fn_InternalBasePtr_Ctor) new (&msgIBP) ::Object::InternalBasePtr(
        reinterpret_cast<RtObject::Internal*>(msgAction));   // B.3.3 bridge
    if (*reinterpret_cast<int64_t*>(character + 0x58) != 0) {
        if (fn_InternalBasePtr_DeletePtr) reinterpret_cast< ::Object::InternalBasePtr*>(reinterpret_cast<void*>(character + 0x58))->DeletePtr(
            );   // B.3.3 bridge
    }
    *reinterpret_cast<int64_t*>(character + 0x58) = msgIBP;

    // (3) PlayAnimation action (only when animTime > 0)
    if (0.0f < animTime) {
        void* pmem = (fn_MemAlloc && g_pMemAllocSingleton)
                     ? fn_MemAlloc(g_pMemAllocSingleton, 0x40, 8) : nullptr;
        void* animAction = nullptr;
        if (pmem) {
            int64_t timeStore = 0;
            if (fn_Time_Ctor_Float) fn_Time_Ctor_Float(&timeStore, animTime);
            uint64_t entityIBP = *reinterpret_cast<uint64_t*>(*reinterpret_cast<int64_t*>(param_1) + 0x10);
            if (fn_ObjectInternal_Ctor) fn_ObjectInternal_Ctor(pmem);
            uint8_t* a = reinterpret_cast<uint8_t*>(pmem);
            *reinterpret_cast<uintptr_t*>(a) = s_vt_Action;
            *reinterpret_cast<uint64_t*>(a + 0x10) = 0;
            a[0x18] = 0;
            *reinterpret_cast<uintptr_t*>(a) = s_vt_PlayAnimation;
            *reinterpret_cast<uint64_t*>(a + 0x20) = entityIBP;
            *reinterpret_cast<uint32_t*>(a + 0x28) = 0;
            *reinterpret_cast<int64_t*>(a + 0x30) = timeStore;
            *reinterpret_cast<uint64_t*>(a + 0x38) = 0;
            animAction = pmem;
        }
        int64_t animIBP = 0;
        if (fn_InternalBasePtr_Ctor) new (&animIBP) ::Object::InternalBasePtr(
            reinterpret_cast<RtObject::Internal*>(animAction));   // B.3.3 bridge
        if (s_fn_actPush) s_fn_actPush(stack, &animIBP);
    }

    // (4) MoveTo action (always)
    void* moveAction = nullptr;
    void* mmem = (fn_MemAlloc && g_pMemAllocSingleton)
                 ? fn_MemAlloc(g_pMemAllocSingleton, 0x480, 8) : nullptr;
    if (mmem) {
        int64_t entityVal = *reinterpret_cast<int64_t*>(*reinterpret_cast<int64_t*>(param_1) + 0x10);
        int64_t familyOut = 0;
        if (s_fn_findFamily) s_fn_findFamily(static_cast<uintptr_t>(entityVal), &familyOut);
        if (familyOut == 0) {
            // Standalone: the CharacterComponent type-id global (DAT_1401c7078) read by
            // Entity_FindComponent_Family is unfilled, so the list-scan returns null. The
            // "family" component IS the citizen's own CharacterComponent (citizen+0x278),
            // populated by Brick 2/4a — use it directly (Brick self-contained pattern).
            if (fn_InternalBasePtr_Ctor) new (&familyOut) ::Object::InternalBasePtr(
                reinterpret_cast<RtObject::Internal*>(character));   // B.3.3 bridge
        }
        float speed = s_pf_172280 ? *s_pf_172280 : -1.0f;
        moveAction = s_fn_moveToCtor
                     ? s_fn_moveToCtor(mmem, &familyOut, 1 /*Slow*/, 0, dest, 0xe1f, 0, speed)
                     : mmem;
    }
    int64_t moveIBP = 0;
    if (fn_InternalBasePtr_Ctor) new (&moveIBP) ::Object::InternalBasePtr(
        reinterpret_cast<RtObject::Internal*>(moveAction));   // B.3.3 bridge
    if (s_fn_actPush) s_fn_actPush(stack, &moveIBP);
}

// ============================================================
// 0x14010a2b0 — CitizenAI_State_Wander
// ------------------------------------------------------------
// Picks a nearby walkable cell and queues a MoveTo (+ animation) toward it.
//   self        = param_1 + 0x38                  (sibling CitizenComponent IBP)
//   target      = *(citizen + 0x258)              (wander anchor; bail if 0)
//   if (*(target + 0x250) != 0 && Rand(1)==0) target = *(target + 0x250)  (reservation)
//   grid        = *(gameWorld + 0xf8)
//   mapSize     = *grid
//   For both `target` and `self` read their ModelComponent world pos
//   (component+0x278 -> +0x68 model, +0x158 x / +0x15c y), add (mapSize>>1), clamp to
//   [0, mapSize-1], form a cell index mapSize*yCell + xCell.
//   cell = WorkGrid_FindCell(grid, targetCell, selfCell, 0xe1f); bail if < 0.
//   Build a world Vector3 from the found cell { (cell%mapSize)-K, (cell/mapSize)-K, 0 }
//   where K = (float)(mapSize>>1) - 0.5, terrain-resolve it, then queue the action
//   triple via PlayAnimation_Action_Ctor5 with animTime = RandFloat*7 + 3.
// Returns true iff a cell was found (and the action queued).
//
// ABI note: WorkGrid_FindCell (0x14009ccf0) is integer-only and does NOT touch XMM0,
// so the original's `extraout_XMM0_Da` is the stale (float)(mapSize>>1) left in XMM0
// by the position math — reproduced here as `halfMapF`.
// ============================================================
bool CitizenAI_State_Wander(int64_t param_1) {
    EnsureInit();
    int64_t* self = reinterpret_cast<int64_t*>(param_1 + 0x38);

    int64_t target = *reinterpret_cast<int64_t*>(*self + 0x258);
    if (target == 0) return false;

    void* rng = (g_pGameWorldSingleton && *g_pGameWorldSingleton)
                ? *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(*g_pGameWorldSingleton) + 0xd8)
                : nullptr;

    if (*reinterpret_cast<int64_t*>(target + 0x250) != 0 &&
        fn_Random_Rand && rng && reinterpret_cast<System::Random*>(rng)->Rand(1u) == 0) {   // B.3.4b bridge
        target = *reinterpret_cast<int64_t*>(target + 0x250);
    }

    int* grid = (g_pGameWorldSingleton && *g_pGameWorldSingleton)
                ? *reinterpret_cast<int**>(reinterpret_cast<uintptr_t>(*g_pGameWorldSingleton) + 0xf8)
                : nullptr;
    if (!grid) return false;

    // --- cell coords from `target` model position ---
    int64_t tChar = target;
    tChar = *reinterpret_cast<int64_t*>(tChar + 0x278);
    int64_t tModel = *reinterpret_cast<int64_t*>(tChar + 0x68);
    int mapSize = *grid;
    int half = mapSize >> 1;
    int tx = static_cast<int>(*reinterpret_cast<float*>(tModel + 0x158) + static_cast<float>(half));
    int ty = static_cast<int>(*reinterpret_cast<float*>(tModel + 0x15c) + static_cast<float>(half));
    if (tx < 0) tx = 0;
    if (mapSize <= tx) tx = mapSize - 1;
    if (ty < 0) ty = 0;
    if (mapSize <= ty) ty = mapSize - 1;

    // --- cell coords from `self` (the citizen) model position ---
    int64_t sChar = *self;
    sChar = *reinterpret_cast<int64_t*>(sChar + 0x278);
    int64_t sModel = *reinterpret_cast<int64_t*>(sChar + 0x68);
    int sx = static_cast<int>(*reinterpret_cast<float*>(sModel + 0x158) + static_cast<float>(half));
    int sy = static_cast<int>(*reinterpret_cast<float*>(sModel + 0x15c) + static_cast<float>(half));
    if (sx < 0) sx = 0;
    if (mapSize <= sx) sx = mapSize - 1;
    if (sy < 0) sy = 0;
    if (mapSize <= sy) sy = mapSize - 1;

    int cell = s_fn_findCell
               ? s_fn_findCell(grid, mapSize * ty + tx, mapSize * sy + sx, 0xe1f)
               : -1;
    if (cell < 0) return false;

    float kBias = s_pf_172114 ? *s_pf_172114 : 0.5f;
    float K = static_cast<float>(half) - kBias;
    float inVec[3] = {
        static_cast<float>(cell % mapSize) - K,
        static_cast<float>(cell / mapSize) - K,
        0.0f,
    };
    float worldPos[3] = { 0.0f, 0.0f, 0.0f };
    if (s_fn_terrainPos) s_fn_terrainPos(grid, worldPos, inVec, '\0', nullptr);

    int64_t citizenIBP = *self;   // raw CitizenComponent ptr in a stack IBP holder
    float r = (fn_Random_RandFloat && rng) ? reinterpret_cast<System::Random*>(rng)->RandFloat() : 0.0f;   // B.3.4b bridge
    float animTime = r * (s_pf_1721a0 ? *s_pf_1721a0 : 7.0f) + (s_pf_172180 ? *s_pf_172180 : 3.0f);
    CitizenAI_PlayAnimationActionCtor5(&citizenIBP, worldPos, animTime);

    return true;
}

