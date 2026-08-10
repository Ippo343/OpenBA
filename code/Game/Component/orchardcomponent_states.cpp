// code/Game/Component/orchardcomponent_states.cpp
// OrchardComponent controller state table — the standalone replica of retail's
// OrchardDescription_helper_20f0 (0x1400520f0), which is what fills the orchard's
// state-type table at DAT_1401cfe50 (16 entries x 0xf8, count DAT_1401d0dd0).
//
// WHY THIS FILE EXISTS: OrchardComponent was a COMPONENT_INERT_FACTORY stub, so its
// embedded state node at comp+0x28 was never installed and never linked into the
// entity's StateMachineComponent chain. Nothing ticked, so nothing ever built the
// per-tree slot array at +0x50/+0x58, nothing posted PlantOrchard work, no trees grew
// and no HarvestOrchard work was ever scheduled — the "orchard produces no yield"
// symptom. Measured baseline before this file existed (BANISHED_ORCHARD_DIAG=120):
//   node(+0x28).vtable=0  backptr(+0x40)=0  linked=NO  count(+0x58)=0
//   PlantOrchard constructed=0 executed=0 | HarvestOrchard constructed=0 executed=0
//
// ── The retail registration API (0x140009aa0 / 9bc0 / 9c70) ────────────────────────
//   AddState(int id, void* enter /*RDX*/, void* update /*R8*/, <R9 unused>,
//            void* trans38 /*[RSP+0x20]*/)
//   AddTransition(int targetId)              -> appends {targetId, 0} to the last
//                                               state's transition list (+0x28)
//   AddEvent(int eventId, void* handler)     -> appends to the last state's EVENT
//                                               list (+0xb0) — see the gap note below
//
// ⚠ TRAP: AddState's argument order is NOT its field order. The `update` argument
// lands at entry+0x20 and `trans38` at entry+0x18, and trans30 (+0x10) is never set
// by any orchard state. Wiring these positionally would put the per-frame handler in
// a transition slot. Verified by reading 0x140009aa0's body, not by argument names.
//
// ⚠ TRAP: AddState's 4th register argument (R9) is genuinely uninitialised at every
// call site — it shows up as `in_R9` in the decompile. It is not a field; do not
// invent one for it.
//
// ── Node vtable slot mapping (READ FROM VTABLE BYTES, not Ghidra labels) ───────────
// The node vtable is 0x14015a130 (10 slots), sitting immediately before the primary
// component vtable at 0x14015a180. Ghidra names those 10 bodies with generic
// component-vtable names (ScalarDtor/Slot1/Slot2/Shutdown/Serialize/Slot5/Draw/
// OnCreate/OnRemove) but they implement the STATE-NODE interface instead. Proof:
// 0x140009df0, which Ghidra calls "Controller_Shutdown", is the Enter dispatcher —
//     MOVSXD RAX,R8D / LEA R8,[0x1401cfe50] / IMUL RAX,RAX,0xf8 / ADD RAX,R8
//     MOV R8,[RAX+0x8] / TEST R8,R8 / JZ ret0 / MOV RCX,[RCX+0x18] / JMP R8
// — 0xf8 stride, enterHandler at +0x08, and the documented `MOV RCX,[node+0x18]`
// backptr contract. So: [0]=Step [3]=Enter [4]=Update [5]=OnState [6]=Trans30
// [7]=Trans38(per-frame) [9]=NULL. Trusting the Ghidra names would have wired
// Serialize into the per-tick slot: a node that links and ticks and does nothing.
//
// ── GAP (deliberate, increment 1) ─────────────────────────────────────────────────
// Every handler below is registered as nullptr. ControllerLive::Dispatcher treats a
// null handler as a graceful no-op, so the node still runs the REAL table-driven
// structure — Step returns real indices and transitions are validated against the
// real graph — it just performs no per-state work yet. That makes this increment
// verifiable on its own (linked=NO -> linked=YES) without any behaviour risk, and
// the per-state bodies drop in one at a time afterwards.
//
// The 18 handler addresses are recorded per state below so that filling them in is a
// lookup, not another decomp pass. All 18 are UNPORTED as of this increment.
//
// ── SECOND GAP: the event list ────────────────────────────────────────────────────
// controller_state_table.h's StateEntry models id/enter/trans30/trans38/update and
// the transition list, but NOT the event list retail keeps at entry+0xb0 (filled by
// AddEvent 0x140009c70). The orchard needs it: the panel's Command (idx14) posts node
// events 3/4/5, and event 3 on state 2 is the "start planting" kick. Until StateEntry
// carries events, the command path cannot drive the machine. Noted here rather than
// silently omitted — it is required work, not an optional extra.

#include "Component/controller_state_table.h"
#include "engine_funcs.h"
#include "resolve.h"
#include "Object/factory_registration.h"   // Object::GetTypeIdByName
#include <cstdint>

namespace { struct OrchardCtrlTag {}; }
using OrchardDispatch = ::ControllerLive::Dispatcher<OrchardCtrlTag>;

extern "C" void** OrchardCtrl_NodeVtable() { return OrchardDispatch::Vtable(); }

// OrchardComponent_ValidateRefs — retail 0x140051830, already ported.
extern void OrchardComponent_ValidateRefs(void* selfV);
// OrchardComponent_ResizeRefArray — retail 0x140009f80, already ported.
extern void OrchardComponent_ResizeRefArray(void* arrayPtr, int newCount);
// PlantOrchard "from entity" ctor — retail 0x1400f5140 (code/Game/Work/plantorchard_ctor.cpp)
extern "C" void* PlantOrchard_CtorFromEntity(void* mem, void** entityIBP, void* treeType);

// ============================================================
// Resolved leaves. Every one is bound by ADDRESS rather than through a named
// engine_funcs wrapper, because two of the finders retail uses here carry
// misleading names in our tree (0x140002e10 was documented as "BuildDesc" but its
// type id DWORD_1401c80e0 is written by WorkPlaceComponent_RegisterType; 0x140003050
// is documented as "MatDesc"/DWORD_1401c8070). Binding by address keeps these ports
// byte-faithful and immune to the naming question.
// ============================================================
namespace {

typedef void*    (*fn_Find_t)(uintptr_t entity, void* outIBP);
typedef uint64_t (*fn_SetWorkplaceActive_t)(uintptr_t wp, char active);
typedef void     (*fn_DeactivateWorkplace_t)(uintptr_t wp);
typedef void     (*fn_SMTick_t)(void* sm, void* time, char a, char b);
typedef void     (*fn_TimeCtor_t)(void* outTime, float seconds);

typedef void  (*fn_CreateNatRes_t)(uintptr_t posComp);
typedef uint64_t (*fn_PostWork_t)(uintptr_t target, int a, char b, void* workIBP);

fn_Find_t                s_FindWorkPlace_2e10 = nullptr;   // 0x140002e10
fn_Find_t                s_Find_3050          = nullptr;   // 0x140003050
fn_Find_t                s_FindPosition_4680  = nullptr;   // 0x140004680
fn_CreateNatRes_t        s_CreateNaturalResource = nullptr;// 0x140124750
fn_PostWork_t            s_PostWork           = nullptr;   // 0x14012ec60

// DAT_1401c46e8 = BaseTypeInfo("OrchardDescription"). That .rdata slot is registered
// NOWHERE in our tree, so ResolveAddr returns 0 and the global would be null forever —
// the same shape as the CitizenComponent type-id gap. Resolve by name into private
// storage instead, latching only a non-zero CRC (GetTypeIdByName only returns a real id
// after Object::Factory::Startup). harvestorchard.cpp:202 has the same unfilled slot and
// will need the same treatment.
uint32_t OrchardDescTypeId() {
    static uint32_t cached = 0;
    if (cached) return cached;
    if (uintptr_t slot = ResolveAddr(Module::Exe, 0x1401c46e8)) {
        cached = *reinterpret_cast<uint32_t*>(slot);
        if (cached) return cached;
    }
    cached = ::Object::GetTypeIdByName("OrchardDescription");
    return cached;
}
fn_SetWorkplaceActive_t  s_SetWorkplaceActive = nullptr;   // 0x14012fc00
fn_DeactivateWorkplace_t s_DeactivateWorkplace= nullptr;   // 0x14012eb50
fn_SMTick_t              s_SMTick             = nullptr;   // 0x1400a9660
fn_TimeCtor_t            s_TimeCtor           = nullptr;   // slot [0x140152b00]
bool                     s_resolved           = false;

void Resolve() {
    if (s_resolved) return;
    s_FindWorkPlace_2e10 = reinterpret_cast<fn_Find_t>(ResolveAddr(Module::Exe, 0x140002e10));
    s_Find_3050          = reinterpret_cast<fn_Find_t>(ResolveAddr(Module::Exe, 0x140003050));
    s_SetWorkplaceActive = reinterpret_cast<fn_SetWorkplaceActive_t>(
                               ResolveAddr(Module::Exe, 0x14012fc00));
    s_DeactivateWorkplace= reinterpret_cast<fn_DeactivateWorkplace_t>(
                               ResolveAddr(Module::Exe, 0x14012eb50));
    s_SMTick             = reinterpret_cast<fn_SMTick_t>(ResolveAddr(Module::Exe, 0x1400a9660));
    s_FindPosition_4680  = reinterpret_cast<fn_Find_t>(ResolveAddr(Module::Exe, 0x140004680));
    s_CreateNaturalResource = reinterpret_cast<fn_CreateNatRes_t>(
                               ResolveAddr(Module::Exe, 0x140124750));
    s_PostWork           = reinterpret_cast<fn_PostWork_t>(ResolveAddr(Module::Exe, 0x14012ec60));
    if (uintptr_t slot = ResolveAddr(Module::Exe, 0x140152b00))
        s_TimeCtor = *reinterpret_cast<fn_TimeCtor_t*>(slot);
    s_resolved = s_FindWorkPlace_2e10 && s_Find_3050 && s_SetWorkplaceActive &&
                 s_DeactivateWorkplace && s_SMTick && s_TimeCtor;
    if (!s_resolved) {
        static bool reported = false;
        if (!reported) {
            reported = true;
            std::fprintf(stderr, "[OrchardCtrl] UNRESOLVED:%s%s%s%s%s%s\n",
                s_FindWorkPlace_2e10 ? "" : " Find2e10",
                s_Find_3050          ? "" : " Find3050",
                s_SetWorkplaceActive ? "" : " SetWorkplaceActive",
                s_DeactivateWorkplace? "" : " DeactivateWorkplace",
                s_SMTick             ? "" : " StateMachine_Tick",
                s_TimeCtor           ? "" : " Time::Time");
            std::fflush(stderr);
        }
    }
}

// The "request a state" idiom, shared by ten of the eighteen handlers.
// Retail inlines this everywhere:
//     want = <id>;
//     if ((int8_t)sm[9] < 0) { Time t(0.0f); StateMachine_Tick(sm, t, 0, 0); }
//     idx = (*(void**)sm)->vtable[+0x08]((void*)*sm, (int)(int8_t)sm[9], &want);
//     if (idx >= 0) { sm[0xb] = idx; sm[0xa] = want; }
// `want` is an in/out — node vtable idx1 writes -1 into it on a miss, so it must be a
// real local, never a constant. sm+0x09 is the CURRENT state id (signed; < 0 means the
// machine has not started, hence the priming Tick).
void OrchardRequestState(void* sm, int wantId) {
    if (!sm || !s_SMTick || !s_TimeCtor) return;
    uint8_t* smb = reinterpret_cast<uint8_t*>(sm);
    int want = wantId;
    if (static_cast<int8_t>(smb[9]) < 0) {
        uint8_t timeBuf[16] = {};          // Time is 8 bytes; 16 is deliberate headroom
        s_TimeCtor(timeBuf, 0.0f);         // (see the Time-ctor 8-byte-clobber lesson)
        s_SMTick(sm, timeBuf, 0, 0);
    }
    void*  chainHead = *reinterpret_cast<void**>(sm);
    if (!chainHead) return;
    void** vt = *reinterpret_cast<void***>(chainHead);
    if (!vt || !vt[1]) return;
    typedef int (*fn_RequestTransition_t)(void* node, int curIdx, int* pWant);
    int idx = reinterpret_cast<fn_RequestTransition_t>(vt[1])(
                  chainHead, static_cast<int>(static_cast<int8_t>(smb[9])), &want);
    if (idx >= 0) {
        smb[0x0b] = static_cast<uint8_t>(idx);
        smb[0x0a] = static_cast<uint8_t>(want);
    }
}

}  // namespace

// ============================================================
// state 2 ENTER — retail 0x140051530
// Deactivate the workplace. `sm` is unused by retail. Returns AL = 0: an enter
// handler that returns non-zero starves every sibling node behind this one.
// ============================================================
extern "C" char Orchard_State2Enter(void* selfV, void* /*sm*/) {
    Resolve();
    if (!selfV || !s_FindWorkPlace_2e10 || !s_SetWorkplaceActive) return 0;
    uintptr_t s = reinterpret_cast<uintptr_t>(selfV);
    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr((void*)(s + 0x10));
    uintptr_t wpIBP = 0;
    s_FindWorkPlace_2e10(*reinterpret_cast<uintptr_t*>(s + 0x10), &wpIBP);
    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&wpIBP);
    s_SetWorkplaceActive(wpIBP, 0);
    return 0;
}

// ============================================================
// state 2 TRANS38 (per-frame) — retail 0x1400514c0
// THE TRIGGER FOR THE WHOLE FEATURE: an orchard whose +0x48 master switch is 0 never
// leaves state 2. The ctor sets +0x48 = 0, and the only two ways to set it are the
// panel's Command(2) toggle and Command(0)'s node event 3 — both ANDed with
// hasType (*(self+0x60) != 0) by CanCommand. So an untyped orchard is correctly and
// permanently idle, which is exactly what the runtime diagnostic shows.
// ============================================================
extern "C" char Orchard_State2PerFrame(void* selfV, void* sm, void* /*time*/) {
    Resolve();
    if (!selfV) return 1;
    uintptr_t s = reinterpret_cast<uintptr_t>(selfV);
    if (*reinterpret_cast<char*>(s + 0x48) != 0)
        OrchardRequestState(sm, 9);
    return 1;
}

// ============================================================
// state 9 ENTER — retail 0x140051070 (Ghidra: PlantOrchard_impl_1070)
// THIS IS WHAT BUILDS THE SLOT ARRAY. Until this ran, OrchardComponent+0x58 stayed 0,
// which is exactly what the runtime diagnostic reported.
//
// Steps, in retail's order:
//   1. push the master switch to the workplace: SetWorkplaceActive(wp, *(self+0x48))
//      — note it forwards +0x48, NOT a literal 0 as state 2's enter does
//   2. footprint rect from the position component: x,y,w,h as int16 at +0x28/+0x2a/+0x2c/+0x2e
//   3. spacing from the OrchardDescription at self+0x18: +0x24 spacingX, +0x28 spacingY
//   4. cols = (w-1+spacingX)/spacingX, rows = (h-1+spacingY)/spacingY
//   5. resize the slot array to rows*cols and fill each entry:
//      +0x00 tileIdx = curY*mapStride + colOffset + x, three null IBPs, +0x04 = 0
//   6. CreateNaturalResource on the position component
//   7. allocate 0x70 and build a PlantOrchard carrying *(self+0x60) as the tree type
//   8. post it to the entity's work component
// ============================================================
extern "C" char Orchard_State9Enter(void* selfV, void* /*sm*/) {
    Resolve();
    if (!selfV) return 1;
    uintptr_t s = reinterpret_cast<uintptr_t>(selfV);

    // One-shot step trace. State 9 enter runs once per orchard, and it is the first
    // thing in this wave to touch the natural-resource and work-posting layers, so when
    // it faults the crash log alone cannot say WHICH step did it. Prints the step it is
    // ABOUT to attempt, so the last line printed names the faulting call.
    #define O9_STEP(n, what) do { \
        static bool seen##n = false; \
        if (!seen##n) { seen##n = true; \
            std::fprintf(stderr, "[Orchard9] step %d: %s\n", (n), (what)); \
            std::fflush(stderr); } \
    } while (0)
    O9_STEP(0, "entered");

    // 1. workplace ← master switch
    O9_STEP(1, "find workplace + SetWorkplaceActive");
    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr((void*)(s + 0x10));
    uintptr_t wpIBP = 0;
    if (s_FindWorkPlace_2e10)
        s_FindWorkPlace_2e10(*reinterpret_cast<uintptr_t*>(s + 0x10), &wpIBP);
    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&wpIBP);
    if (s_SetWorkplaceActive)
        s_SetWorkplaceActive(wpIBP, *reinterpret_cast<char*>(s + 0x48));

    // 2. footprint rect
    O9_STEP(2, "find position component (footprint rect)");
    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr((void*)(s + 0x10));
    uintptr_t posIBP = 0;
    if (s_FindPosition_4680)
        s_FindPosition_4680(*reinterpret_cast<uintptr_t*>(s + 0x10), &posIBP);
    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&posIBP);
    if (!posIBP) return 1;            // SA: retail derefs unguarded; without a position
                                      // component there is no footprint to lay out at all.
    const int rectX = *reinterpret_cast<int16_t*>(posIBP + 0x28);
    const int rectY = *reinterpret_cast<int16_t*>(posIBP + 0x2a);
    const int rectW = *reinterpret_cast<int16_t*>(posIBP + 0x2c);
    const int rectH = *reinterpret_cast<int16_t*>(posIBP + 0x2e);

    // 3. spacing from the OrchardDescription. Retail nulls the pointer when the
    // IsDerivedFrom check fails and then reads +0x24 off it anyway; we bail instead,
    // because our type id comes from a by-name resolve that can legitimately be absent
    // (the shared .rdata slot 0x1401c46e8 is registered nowhere in our tree).
    void* desc = *reinterpret_cast<void**>(s + 0x18);
    const uint32_t tidOrchardDesc = OrchardDescTypeId();
    if (!desc || !tidOrchardDesc) return 1;
    if (fn_IsDerivedFrom && !fn_IsDerivedFrom(desc, tidOrchardDesc)) return 1;
    const int spacingX = *reinterpret_cast<int32_t*>(reinterpret_cast<uintptr_t>(desc) + 0x24);
    const int spacingY = *reinterpret_cast<int32_t*>(reinterpret_cast<uintptr_t>(desc) + 0x28);
    if (spacingX <= 0 || spacingY <= 0) return 1;   // retail divides by these unguarded

    // 4/5. lay out the grid
    O9_STEP(3, "resize + fill slot array");
    const int cols = (rectW - 1 + spacingX) / spacingX;
    const int rows = (rectH - 1 + spacingY) / spacingY;
    OrchardComponent_ResizeRefArray(reinterpret_cast<void*>(s + 0x50), rows * cols);

    int mapStride = 0;
    if (g_pGameWorldSingleton && *g_pGameWorldSingleton) {
        int* stridePtr = *reinterpret_cast<int**>(
            reinterpret_cast<uintptr_t>(*g_pGameWorldSingleton) + 0xf8);
        if (stridePtr) mapStride = *stridePtr;
    }

    uintptr_t slots = *reinterpret_cast<uintptr_t*>(s + 0x50);
    if (slots) {
        int curY = rectY;
        for (int r = 0; r < rows; ++r) {
            int colOff = 0;
            for (int c = 0; c < cols; ++c) {
                uintptr_t slot = slots + (static_cast<uintptr_t>(r) * cols + c) * 0x28;
                *reinterpret_cast<int32_t*>(slot + 0x00) = curY * mapStride + colOff + rectX;
                // Retail writes the three IBPs in the order +0x10, +0x18, +0x08.
                if (fn_InternalBasePtr_Ctor) {
                    fn_InternalBasePtr_Ctor(reinterpret_cast<void*>(slot + 0x10), nullptr);
                    fn_InternalBasePtr_Ctor(reinterpret_cast<void*>(slot + 0x18), nullptr);
                    fn_InternalBasePtr_Ctor(reinterpret_cast<void*>(slot + 0x08), nullptr);
                } else {
                    *reinterpret_cast<uint64_t*>(slot + 0x10) = 0;
                    *reinterpret_cast<uint64_t*>(slot + 0x18) = 0;
                    *reinterpret_cast<uint64_t*>(slot + 0x08) = 0;
                }
                *reinterpret_cast<int32_t*>(slot + 0x04) = 0;
                colOff += spacingX;
            }
            curY += spacingY;
        }
    }

    // 6. seed the natural-resource layer over the footprint
    O9_STEP(4, "CreateNaturalResource");
    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&posIBP);
    if (s_CreateNaturalResource) s_CreateNaturalResource(posIBP);

    // 7. build the PlantOrchard work item carrying the chosen tree type
    O9_STEP(5, "alloc + PlantOrchard_CtorFromEntity");
    void* work = nullptr;
    if (fn_MemAlloc) {
        void* mem = fn_MemAlloc(g_pMemAllocSingleton, 0x70, 8);
        if (mem) {
            void* entityLocal = *reinterpret_cast<void**>(s + 0x10);
            work = PlantOrchard_CtorFromEntity(mem, &entityLocal,
                                               *reinterpret_cast<void**>(s + 0x60));
        }
    }
    uintptr_t workIBP = 0;
    if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor(&workIBP, work);
    else workIBP = reinterpret_cast<uintptr_t>(work);

    // 8. post it. Retail hand-rolls the component scan for type id DWORD_1401c8070 here
    // instead of calling the shared finder; 0x140003050 performs the identical scan
    // (same type id, same IsDerivedFrom confirmation), so we call it.
    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr((void*)(s + 0x10));
    uintptr_t targetIBP = 0;
    if (s_Find_3050) s_Find_3050(*reinterpret_cast<uintptr_t*>(s + 0x10), &targetIBP);
    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&targetIBP);
    O9_STEP(6, "post work to the work component");
    {
        static bool once = false;
        if (!once) {
            once = true;
            std::fprintf(stderr,
                "[Orchard9] step6 values: entity=%p targetIBP=%p work=%p workIBP=%p "
                "PostWork=%p Find3050=%p\n",
                (void*)*reinterpret_cast<uintptr_t*>(s + 0x10), (void*)targetIBP,
                work, (void*)workIBP, (void*)s_PostWork, (void*)s_Find_3050);
            std::fflush(stderr);
        }
    }
    if (s_PostWork) s_PostWork(targetIBP, 1, 0, &workIBP);
    O9_STEP(7, "DONE - state 9 enter completed");
    return 1;
}

// ============================================================
// state 9 TRANS38 (per-frame) — retail 0x140050f90
// When every slot has reached state 1 (planted), deactivate the workplace and move to
// state 10.
//
// ⚠ FAITHFUL LANDMINE, deliberately preserved: the test is `planted == slotCount`,
// evaluated before any planting has happened. A footprint yielding cols*rows == 0 gives
// 0 == 0, so a zero-area orchard falls straight through to state 10 on its first frame
// having planted nothing, and looks healthy. Retail does this; do not "fix" it here.
// ============================================================
extern "C" char Orchard_State9PerFrame(void* selfV, void* sm, void* /*time*/) {
    Resolve();
    if (!selfV) return 1;
    uintptr_t s = reinterpret_cast<uintptr_t>(selfV);

    OrchardComponent_ValidateRefs(selfV);

    const int  count = *reinterpret_cast<int32_t*>(s + 0x58);
    uintptr_t  slots = *reinterpret_cast<uintptr_t*>(s + 0x50);
    int planted = 0;
    for (int i = 0; i < count; ++i) {
        const int st = *reinterpret_cast<int32_t*>(slots + static_cast<uintptr_t>(i) * 0x28 + 4);
        if (st != 0 && st == 1) ++planted;   // retail's redundant pair, kept verbatim
    }
    if (planted != count) return 1;

    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr((void*)(s + 0x10));
    uintptr_t wpIBP = 0;
    if (s_Find_3050) s_Find_3050(*reinterpret_cast<uintptr_t*>(s + 0x10), &wpIBP);
    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&wpIBP);
    // Retail recycles one stack slot for the finder out-param and then for `want`.
    // We use separate locals: identical behaviour, and it avoids the recycled-IBP trap
    // where a finder's return value is held across other code.
    if (s_DeactivateWorkplace) s_DeactivateWorkplace(wpIBP);
    OrchardRequestState(sm, 10);
    return 1;
}

// Replica of OrchardDescription_helper_20f0 (0x1400520f0), in registration order.
// State ids are literal; the names in comments are inferred from the bodies.
extern "C" void OrchardCtrl_PopulateTable() {
    using namespace ::ControllerLive;
    StateTable& t = OrchardDispatch::table;
    if (t.count != 0) return;                       // idempotent, as pasture's is

    // ── state 2 — DEFAULT (idle; GetDefaultState 0x1400026c0 is `MOV EAX,2; RET`) ──
    // enter    0x140051530  deactivate the workplace
    // trans38  0x1400514c0  if +0x48 set -> request state 9   (the idle->plant trigger)
    // event 3  0x140051450  unconditional request of state 9  (the panel's plant cmd)
    TableRegisterState(t, 2, reinterpret_cast<void*>(&Orchard_State2Enter), nullptr,
                       reinterpret_cast<void*>(&Orchard_State2PerFrame), nullptr);
    TableAddTransition(t, 3, nullptr);
    TableAddTransition(t, 9, nullptr);

    // ── state 9 — PLANT GRID ──────────────────────────────────────────────────────
    // enter    0x140051070  PlantOrchard_impl_1070: activates the workplace, reads the
    //                       footprint rect + desc spacing, RESIZES THE SLOT ARRAY to
    //                       cols*rows (this is what makes +0x58 non-zero), then posts
    //                       a PlantOrchard work carrying *(self+0x60) as the tree type
    // trans38  0x140050f90  when every slot reaches state 1 -> request state 10
    // event 5  0x1400513e0  -> request state 20
    TableRegisterState(t, 9, reinterpret_cast<void*>(&Orchard_State9Enter), nullptr,
                       reinterpret_cast<void*>(&Orchard_State9PerFrame), nullptr);
    TableAddTransition(t, 3, nullptr);
    TableAddTransition(t, 10, nullptr);
    TableAddTransition(t, 20, nullptr);

    // ── state 10 — TEND / IDLE-GROWN ──────────────────────────────────────────────
    // enter    0x140050e30  hole -> state 12, else post a TendArea work
    // update   0x140050ba0  deactivate the workplace if active
    // trans38  0x140050c00  TryHarvest; hole -> 12; seasonal TendArea re-post
    // event 5  0x1400513e0 -> state 20 ; event 4  0x140050900 -> state 11
    TableRegisterState(t, 10, nullptr, nullptr, nullptr, nullptr);
    TableAddTransition(t, 3, nullptr);
    TableAddTransition(t, 12, nullptr);
    TableAddTransition(t, 11, nullptr);
    TableAddTransition(t, 20, nullptr);

    // ── state 12 — REPLANT GAPS ───────────────────────────────────────────────────
    // enter    0x140050ae0  PlantOrchard_impl_0ae0: post a PlantOrchard for the holes
    // update   0x1400509b0
    // trans38  0x140050a10  TryHarvest; no holes left -> request state 10
    // event 5  0x1400513e0 -> state 20 ; event 4  0x140050900 -> state 11
    TableRegisterState(t, 12, nullptr, nullptr, nullptr, nullptr);
    TableAddTransition(t, 3, nullptr);
    TableAddTransition(t, 10, nullptr);
    TableAddTransition(t, 11, nullptr);
    TableAddTransition(t, 20, nullptr);

    // ── state 11 — HARVEST ────────────────────────────────────────────────────────
    // enter    0x140050800  HarvestOrchard_impl_0800: post a HarvestOrchard work, then
    //                       stamp the footprint cells with *(self+0x60)
    // trans38  0x140050650  harvest-complete latch -> request state 10
    // event 5  0x1400513e0 -> state 20
    TableRegisterState(t, 11, nullptr, nullptr, nullptr, nullptr);
    TableAddTransition(t, 3, nullptr);
    TableAddTransition(t, 10, nullptr);
    TableAddTransition(t, 20, nullptr);

    // ── state 20 (0x14) — CLEAR ───────────────────────────────────────────────────
    // enter    0x1400504e0  ClearArea_impl_04e0: post a ClearArea work at prio 0x401
    // update   0x1400503e0  deactivate the workplace
    // trans38  0x140050430  when the footprint tests clear -> request state 9
    TableRegisterState(t, 20, nullptr, nullptr, nullptr, nullptr);
    TableAddTransition(t, 3, nullptr);
    TableAddTransition(t, 9, nullptr);

    // State 3 is the shared teardown/"stop" id every state can transition to. Retail
    // registers NO body for it in this table — it is a transition target only, so it
    // is deliberately absent here too rather than added as an empty state.
}
