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
#include "Component/ClearAreaDescription.h"
#include "Component/OrchardDescription.h"
#include "Component/OrchardComponent.h"
#include "engine_funcs.h"
#include "resolve.h"
#include "Object/factory_registration.h"   // Object::GetTypeIdByName
#include <cstdint>
#include "Component/ControllerComponent.h"
#include <cstdio>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B
#include "system/Value.h"   // ENDGAME B.3.4b System caller batch

// OrchardCtrlTag lives in OrchardComponent.h (the member type needs it).
using OrchardDispatch = ::ControllerLive::Dispatcher<OrchardCtrlTag>;


// OrchardComponent_ValidateRefs — retail 0x140051830, already ported.
extern void OrchardComponent_ValidateRefs(void* selfV);
// OrchardComponent_ResizeRefArray — retail 0x140009f80, already ported.
extern void OrchardComponent_ResizeRefArray(void* arrayPtr, int newCount);
// PlantOrchard "from entity" ctor — retail 0x1400f5140 (code/Game/Work/plantorchard_ctor.cpp)
extern "C" void* PlantOrchard_CtorFromEntity(void* mem, void** entityIBP, void* treeType);
// TendArea "from entity" ctor — retail 0x1400f5a80 (code/Game/Work/tendarea_ctor.cpp)
extern "C" void* TendArea_CtorFromEntity(void* mem, void** entityIBP, int kind);
// HarvestOrchard "from entity" ctor — retail 0x1400ee060 (code/Game/Work/harvestorchard_ctor.cpp)
extern "C" void* HarvestOrchard_CtorFromEntity(void* mem, void** entityIBP);
// Map-grid footprint cell stamp — retail 0x14009f770, ported in pasturecomponent_states.cpp.
extern "C" uint64_t MapGrid_StampCellRefs_9f770(void* grid, int x, int y, int w, int h,
                                                void* ref);
// OrchardComponent_GetGrowth — retail 0x140051640, already ported (orchardcomponent.cpp).
extern float OrchardComponent_GetGrowth(void* selfV);

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
uint32_t OrchardDescTypeId() { return OrchardDescription::TypeId(); }   // ti cell 0x1401c46e8
fn_SetWorkplaceActive_t  s_SetWorkplaceActive = nullptr;   // 0x14012fc00
fn_DeactivateWorkplace_t s_DeactivateWorkplace= nullptr;   // 0x14012eb50
fn_SMTick_t              s_SMTick             = nullptr;   // 0x1400a9660
// FUN_14012ead0 — "does the work component currently hold a work slot?"; the companion
// of 0x14012eb50 (release). State 10 gates both its update and its seasonal re-post on it.
typedef char (*fn_HasWorkSlot_t)(uintptr_t wc);
fn_HasWorkSlot_t         s_HasWorkSlot        = nullptr;   // 0x14012ead0
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
    s_HasWorkSlot        = reinterpret_cast<fn_HasWorkSlot_t>(ResolveAddr(Module::Exe, 0x14012ead0));
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
    // chainHead = the first controller member the SM linked (D6); slot 1 FindTransition.
    int idx = reinterpret_cast<ControllerComponent::Controller*>(chainHead)->FindTransition(   // B.3.8 bridge
                  static_cast<int>(static_cast<int8_t>(smb[9])), &want);
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
    uintptr_t wpIBP = 0;
    s_FindWorkPlace_2e10(*reinterpret_cast<uintptr_t*>(s + 0x10), &wpIBP);
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
    uintptr_t wpIBP = 0;
    if (s_FindWorkPlace_2e10)
        s_FindWorkPlace_2e10(*reinterpret_cast<uintptr_t*>(s + 0x10), &wpIBP);
    if (s_SetWorkplaceActive)
        s_SetWorkplaceActive(wpIBP, *reinterpret_cast<char*>(s + 0x48));

    // 2. footprint rect
    O9_STEP(2, "find position component (footprint rect)");
    uintptr_t posIBP = 0;
    if (s_FindPosition_4680)
        s_FindPosition_4680(*reinterpret_cast<uintptr_t*>(s + 0x10), &posIBP);
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
                    new (reinterpret_cast<void*>(slot + 0x10)) ::Object::InternalBasePtr(nullptr);   // B.3.3 bridge
                    new (reinterpret_cast<void*>(slot + 0x18)) ::Object::InternalBasePtr(nullptr);   // B.3.3 bridge
                    new (reinterpret_cast<void*>(slot + 0x08)) ::Object::InternalBasePtr(nullptr);   // B.3.3 bridge
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
    if (fn_InternalBasePtr_Ctor) new (&workIBP) ::Object::InternalBasePtr(
        reinterpret_cast<RtObject::Internal*>(work));   // B.3.3 bridge
    else workIBP = reinterpret_cast<uintptr_t>(work);

    // 8. post it. Retail hand-rolls the component scan for type id DWORD_1401c8070 here
    // instead of calling the shared finder; 0x140003050 performs the identical scan
    // (same type id, same IsDerivedFrom confirmation), so we call it.
    uintptr_t targetIBP = 0;
    if (s_Find_3050) s_Find_3050(*reinterpret_cast<uintptr_t*>(s + 0x10), &targetIBP);
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

    uintptr_t wpIBP = 0;
    if (s_Find_3050) s_Find_3050(*reinterpret_cast<uintptr_t*>(s + 0x10), &wpIBP);
    // Retail recycles one stack slot for the finder out-param and then for `want`.
    // We use separate locals: identical behaviour, and it avoids the recycled-IBP trap
    // where a finder's return value is held across other code.
    if (s_DeactivateWorkplace) s_DeactivateWorkplace(wpIBP);
    OrchardRequestState(sm, 10);
    return 1;
}

// ============================================================
// State 10's shared leaves
// ============================================================
// The calendar/weather constants are the exe's own .rdata floats, read as BYTES
// (read_memory, 2026-08-10) rather than taken from a Ghidra label:
//   DAT_14017225c = 0x45610000 = 3600.0f   seconds per game year
//   DAT_1401721b8 = 0x41400000 =   12.0f   months per year
//   DAT_1401721d4 = 0x42040000 =   33.0f   weather threshold (gw+0x130, +0x18)
//   DAT_140172144 = 0x3f7d70a4 =    0.99f  ripeness threshold on GetGrowth
static constexpr float kSecondsPerYear   = 3600.0f;
static constexpr float kMonthsPerYear    = 12.0f;
static constexpr float kWeatherThreshold = 33.0f;
static constexpr float kRipeGrowth       = 0.99f;

namespace {

// The month index retail derives in three separate places:
//   (int)(Time::AsSeconds(*(gw+0x128) + 8) / 3600.0f * 12.0f)
// Note the Time lives at calendar+8 and the YEAR is the int at calendar+0 (see the
// event-4 handler, which stamps it into the component's +0x4c).
int OrchardCurrentMonth() {
    if (!g_pGameWorldSingleton || !*g_pGameWorldSingleton) return 0;
    uintptr_t cal = *reinterpret_cast<uintptr_t*>(
        reinterpret_cast<uintptr_t>(*g_pGameWorldSingleton) + 0x128);
    if (!cal) return 0;
    const float secs = reinterpret_cast<const System::Time*>(reinterpret_cast<void*>(cal + 8))->AsSeconds();   // B.3.4b bridge
    return static_cast<int>(secs / kSecondsPerYear * kMonthsPerYear);
}

int OrchardCurrentYear() {
    if (!g_pGameWorldSingleton || !*g_pGameWorldSingleton) return 0;
    uintptr_t cal = *reinterpret_cast<uintptr_t*>(
        reinterpret_cast<uintptr_t>(*g_pGameWorldSingleton) + 0x128);
    return cal ? *reinterpret_cast<int32_t*>(cal) : 0;
}

// Slots whose +0x04 is 2 are HOLES — a tile the planter could not use (msg 0 failed to
// bind, or the tree was later removed). Retail writes the scan as
// `if ((v != 0) && (v != 1) && (v == 2))`, a switch artefact that means simply `v == 2`.
int OrchardHoleCount(uintptr_t s) {
    const int count = *reinterpret_cast<int32_t*>(s + 0x58);
    uintptr_t slots = *reinterpret_cast<uintptr_t*>(s + 0x50);
    if (!slots) return 0;
    int holes = 0;
    for (int i = 0; i < count; ++i)
        if (*reinterpret_cast<int32_t*>(slots + static_cast<uintptr_t>(i) * 0x28 + 4) == 2)
            ++holes;
    return holes;
}

// The TendArea post shared by state 10's enter and its per-frame seasonal branch.
void OrchardPostTendArea(uintptr_t s) {
    // Retail runs a finder into a throwaway out-param here before allocating and never
    // reads the result — kept for shape, since it also runs the CheckPtr side effects.
    uintptr_t unusedIBP = 0;
    if (s_Find_3050) s_Find_3050(*reinterpret_cast<uintptr_t*>(s + 0x10), &unusedIBP);

    void* work = nullptr;
    if (fn_MemAlloc) {
        void* mem = fn_MemAlloc(g_pMemAllocSingleton, 0x60, 8);
        if (mem) {
            void* entityLocal = *reinterpret_cast<void**>(s + 0x10);
            work = TendArea_CtorFromEntity(mem, &entityLocal, 1);
        }
    }
    uintptr_t workIBP = 0;
    if (fn_InternalBasePtr_Ctor) new (&workIBP) ::Object::InternalBasePtr(
        reinterpret_cast<RtObject::Internal*>(work));   // B.3.3 bridge
    else workIBP = reinterpret_cast<uintptr_t>(work);

    uintptr_t targetIBP = 0;
    if (s_Find_3050) s_Find_3050(*reinterpret_cast<uintptr_t*>(s + 0x10), &targetIBP);
    if (s_PostWork) s_PostWork(targetIBP, 1, 0, &workIBP);
}

// Is the work component currently holding a work slot for us? (0x14012ead0)
bool OrchardHasWork(uintptr_t s) {
    if (!s_HasWorkSlot) return false;
    uintptr_t wcIBP = 0;
    if (s_Find_3050) s_Find_3050(*reinterpret_cast<uintptr_t*>(s + 0x10), &wcIBP);
    return s_HasWorkSlot(wcIBP) != 0;
}

}  // namespace

// ============================================================
// event-4 handler — retail 0x140050900 (wired on states 10 and 12)
// Request state 11 (harvest), clear every slot's "tended" byte at +0x20, and stamp the
// current year into +0x4c so TryHarvest cannot fire twice in one year.
//
// Exposed (not static) because TryHarvest calls it directly, which is what makes the
// harvest reachable at all while StateEntry still has no event list — see the GAP note
// at the top of this file.
// ============================================================
extern "C" char Orchard_Event4Harvest(void* selfV, void* sm) {
    Resolve();
    if (!selfV) return 1;
    uintptr_t s = reinterpret_cast<uintptr_t>(selfV);

    OrchardRequestState(sm, 11);

    const int count = *reinterpret_cast<int32_t*>(s + 0x58);
    uintptr_t slots = *reinterpret_cast<uintptr_t*>(s + 0x50);
    for (int i = 0; slots && i < count; ++i)
        *reinterpret_cast<uint8_t*>(slots + static_cast<uintptr_t>(i) * 0x28 + 0x20) = 0;

    *reinterpret_cast<int32_t*>(s + 0x4c) = OrchardCurrentYear();
    return 1;
}

// ============================================================
// event-3 handler — retail 0x140051450 (wired on state 2)
// Unconditionally request state 9. This is the panel's "start planting" kick: the
// Command slot (idx14) maps button id 0 to node event 3, and this bypasses the +0x48
// master-switch gate that state 2's per-frame tests.
// ============================================================
extern "C" char Orchard_Event3Plant(void* selfV, void* sm) {
    Resolve();
    if (!selfV) return 1;
    OrchardRequestState(sm, 9);
    return 1;
}

// ============================================================
// event-5 handler — retail 0x1400513e0 (wired on states 9, 10, 11 and 12)
// Unconditionally request state 20 (0x14) — the clear/replace path, button id 3.
// ============================================================
extern "C" char Orchard_Event5Clear(void* selfV, void* sm) {
    Resolve();
    if (!selfV) return 1;
    {   // The panel proved it POSTS event 5 (the [OrchardCmd] Command(3) line), but nothing
        // showed whether the event reached a handler — and with state 20's handlers null,
        // "the event never arrived" and "it arrived and did nothing" looked identical.
        // sm+0x0a is the requested state id after the request, sm+0x09 the current one.
        // ⚠ sm+0x09 is the table INDEX, not a state id (registration order is
        // [0]=2 [1]=9 [2]=10 [3]=12 [4]=11 [5]=20), so it is labelled as such. sm+0x0a is
        // the requested STATE id — the two must not be printed under the same word.
        static int s_n = 0;
        if (s_n++ < 8) {
            uint8_t* smb = reinterpret_cast<uint8_t*>(sm);
            const int beforeIdx = sm ? static_cast<int>(static_cast<int8_t>(smb[9])) : -99;
            OrchardRequestState(sm, 20);
            std::fprintf(stderr, "[Orchard20] event 5 (cut): from table idx %d -> requested "
                         "state %d (accepted=%d)\n", beforeIdx,
                         sm ? static_cast<int>(smb[0x0a]) : -99,
                         sm ? static_cast<int>(smb[0x0a] == 20) : 0);
            std::fflush(stderr);
            return 1;
        }
    }
    OrchardRequestState(sm, 20);
    return 1;
}

// ============================================================
// TryHarvest — retail 0x140051580 (called by the state 10 and 12 per-frame bodies)
// The growth -> harvest trigger, and the only reader of GetGrowth outside the UI.
//
//   if (!self->+0x48)                  return 0;   // master switch off
//   if (year == self->+0x4c)           return 0;   // already harvested this year
//   if (weather >= 33.0f && month in [7..11] && GetGrowth > 0.0f)  -> harvest
//   if (GetGrowth > 0.99f)                                         -> harvest
//
// The first arm is the "frost is coming, take what is there" rule; the second is the
// ordinary ripeness one. Returning 1 makes the per-frame body return immediately.
// ============================================================
static char Orchard_TryHarvest(void* selfV, void* sm) {
    uintptr_t s = reinterpret_cast<uintptr_t>(selfV);
    if (*reinterpret_cast<char*>(s + 0x48) == 0) return 0;
    if (OrchardCurrentYear() == *reinterpret_cast<int32_t*>(s + 0x4c)) return 0;

    uintptr_t gw = g_pGameWorldSingleton ? reinterpret_cast<uintptr_t>(*g_pGameWorldSingleton) : 0;
    uintptr_t weather = gw ? *reinterpret_cast<uintptr_t*>(gw + 0x130) : 0;
    if (weather && *reinterpret_cast<float*>(weather + 0x18) >= kWeatherThreshold) {
        const unsigned m = static_cast<unsigned>(OrchardCurrentMonth() - 7);
        if (m < 5 && OrchardComponent_GetGrowth(selfV) > 0.0f)
            return Orchard_Event4Harvest(selfV, sm);
    }
    if (OrchardComponent_GetGrowth(selfV) > kRipeGrowth)
        return Orchard_Event4Harvest(selfV, sm);
    return 0;
}

// ============================================================
// state 10 ENTER — retail 0x140050e30
// ValidateRefs; if ANY slot is a hole, go replant (state 12) and post nothing.
// Otherwise post one TendArea work item. Returns AL = 1 on both paths.
// ============================================================
extern "C" char Orchard_State10Enter(void* selfV, void* sm) {
    Resolve();
    if (!selfV) return 1;
    uintptr_t s = reinterpret_cast<uintptr_t>(selfV);

    OrchardComponent_ValidateRefs(selfV);
    const int holes = OrchardHoleCount(s);
    {   // both arms are silent otherwise, and "posted a TendArea" vs "bounced to replant"
        // is the whole meaning of this handler.
        static int s_n = 0;
        if (s_n++ < 4) {
            std::fprintf(stderr, "[Orchard10] enter: holes=%d -> %s\n", holes,
                         holes > 0 ? "request state 12 (replant)" : "post TendArea");
            std::fflush(stderr);
        }
    }
    if (holes > 0) {
        OrchardRequestState(sm, 12);
        return 1;
    }
    OrchardPostTendArea(s);
    return 1;
}

// ============================================================
// state 10 UPDATE — retail 0x140050ba0
// Find the work component; if it is holding a work slot, release it. `sm` unused.
// ============================================================
extern "C" char Orchard_State10Update(void* selfV, void* /*sm*/, void* /*time*/) {
    Resolve();
    if (!selfV) return 1;
    uintptr_t s = reinterpret_cast<uintptr_t>(selfV);
    if (OrchardHasWork(s)) {
        uintptr_t wcIBP = 0;
        if (s_Find_3050) s_Find_3050(*reinterpret_cast<uintptr_t*>(s + 0x10), &wcIBP);
        if (s_DeactivateWorkplace) s_DeactivateWorkplace(wcIBP);
    }
    return 1;
}

// ============================================================
// state 10 TRANS38 (per-frame) — retail 0x140050c00
// ValidateRefs; TryHarvest (returns straight out if it fired); holes -> request 12;
// then the seasonal tending gate.
//
// ⚠ FAITHFUL: the state-12 request is NOT followed by a return — the seasonal block
// still runs in the same frame. Do not "tidy" it into an early return.
//
// inSeason = month in [1..8]: retail spells it `(unsigned)(m-1) < 11 && !((unsigned)(m-9) < 3)`,
// i.e. [1..11] minus [9..11]. Out of season the tending work is dropped rather than
// left queued; in season a fresh TendArea is posted only when none is outstanding.
// ============================================================
extern "C" char Orchard_State10PerFrame(void* selfV, void* sm, void* /*time*/) {
    Resolve();
    if (!selfV) return 1;
    uintptr_t s = reinterpret_cast<uintptr_t>(selfV);

    OrchardComponent_ValidateRefs(selfV);
    if (Orchard_TryHarvest(selfV, sm) != 0) return 1;

    if (OrchardHoleCount(s) > 0) OrchardRequestState(sm, 12);   // no return — faithful

    const int m = OrchardCurrentMonth();
    const bool inSeason = (static_cast<unsigned>(m - 1) < 11u) &&
                          !(static_cast<unsigned>(m - 9) < 3u);
    const bool hasWork = OrchardHasWork(s);

    {   // This handler runs every frame and its four branch combinations look identical
        // from outside. Report the deciding inputs a few times, then periodically.
        static int s_n = 0;
        if (++s_n <= 4 || (s_n % 600) == 0) {
            std::fprintf(stderr, "[Orchard10] perFrame #%d: month=%d inSeason=%d hasWork=%d "
                         "holes=%d growth=%.6f year=%d lastHarvestYear(+0x4c)=%d -> %s\n",
                         s_n, m, (int)inSeason, (int)hasWork, OrchardHoleCount(s),
                         OrchardComponent_GetGrowth(selfV), OrchardCurrentYear(),
                         *reinterpret_cast<int32_t*>(s + 0x4c),
                         (hasWork && !inSeason) ? "drop the tending work"
                             : (!hasWork && inSeason) ? "post TendArea" : "nothing");
            std::fflush(stderr);
        }
    }
    if (hasWork && !inSeason) {
        uintptr_t wcIBP = 0;
        if (s_Find_3050) s_Find_3050(*reinterpret_cast<uintptr_t*>(s + 0x10), &wcIBP);
        if (s_DeactivateWorkplace) s_DeactivateWorkplace(wcIBP);
    } else if (!hasWork && inSeason) {
        OrchardPostTendArea(s);
    }
    return 1;
}

// ============================================================
// state 12 ENTER — retail 0x140050ae0 (Ghidra: PlantOrchard_impl_0ae0)
// The replant-the-gaps state. Post a fresh PlantOrchard; unlike state 9 it does NOT
// touch the slot array — PlantOrchard_CtorFromEntity re-sizes its own byte array from
// the orchard's existing slot count, so the holes it finds are whatever ValidateRefs
// last marked as state 2.
//
// Retail runs a finder into a throwaway out-param before allocating and never reads the
// result; kept for shape (it also runs the CheckPtr side effects).
// ============================================================
extern "C" char Orchard_State12Enter(void* selfV, void* /*sm*/) {
    Resolve();
    if (!selfV) return 1;
    uintptr_t s = reinterpret_cast<uintptr_t>(selfV);

    uintptr_t unusedIBP = 0;
    if (s_Find_3050) s_Find_3050(*reinterpret_cast<uintptr_t*>(s + 0x10), &unusedIBP);

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
    if (fn_InternalBasePtr_Ctor) new (&workIBP) ::Object::InternalBasePtr(
        reinterpret_cast<RtObject::Internal*>(work));   // B.3.3 bridge
    else workIBP = reinterpret_cast<uintptr_t>(work);

    uintptr_t targetIBP = 0;
    if (s_Find_3050) s_Find_3050(*reinterpret_cast<uintptr_t*>(s + 0x10), &targetIBP);
    {   // state 12 is only reachable when ValidateRefs found a hole, so "we got here at
        // all" is the interesting fact; the hole count says how much replanting is due.
        static int s_n = 0;
        if (s_n++ < 4) {
            std::fprintf(stderr, "[Orchard12] enter: holes=%d work=%p -> post PlantOrchard\n",
                         OrchardHoleCount(s), work);
            std::fflush(stderr);
        }
    }
    if (s_PostWork) s_PostWork(targetIBP, 1, 0, &workIBP);
    return 1;
}

// ============================================================
// state 12 UPDATE — retail 0x1400509b0
// Byte-for-byte the same body as state 10's update (separate retail function, identical
// logic): if the work component holds a work slot, release it.
// ============================================================
extern "C" char Orchard_State12Update(void* selfV, void* sm, void* time) {
    return Orchard_State10Update(selfV, sm, time);
}

// ============================================================
// state 12 TRANS38 (per-frame) — retail 0x140050a10
// ValidateRefs; TryHarvest (short-circuits the frame if it fired); if any hole remains
// stay in 12, otherwise go back to tending.
//
// ⚠ FAITHFUL zero-degenerate, third instance: when +0x58 is 0 retail skips the scan loop
// entirely and falls straight through to requesting state 10 — a slot-less orchard leaves
// replant on its first frame. Do not "fix" it.
// ============================================================
extern "C" char Orchard_State12PerFrame(void* selfV, void* sm, void* /*time*/) {
    Resolve();
    if (!selfV) return 1;
    uintptr_t s = reinterpret_cast<uintptr_t>(selfV);

    OrchardComponent_ValidateRefs(selfV);
    if (Orchard_TryHarvest(selfV, sm) != 0) return 1;

    if (*reinterpret_cast<int32_t*>(s + 0x58) > 0 && OrchardHoleCount(s) != 0)
        return 1;                                   // holes remain — stay in 12

    OrchardRequestState(sm, 10);
    return 1;
}

// ============================================================
// state 11 ENTER — retail 0x140050800 (Ghidra: HarvestOrchard_impl_0800)
// Post one HarvestOrchard work item, then stamp the footprint cells with the orchard's
// tree type. Note the ctor takes NO tree-type argument, unlike PlantOrchard's — the work
// item derives its worklist from the component's slot array instead.
// ============================================================
extern "C" char Orchard_State11Enter(void* selfV, void* /*sm*/) {
    Resolve();
    if (!selfV) return 1;
    uintptr_t s = reinterpret_cast<uintptr_t>(selfV);

    void* work = nullptr;
    if (fn_MemAlloc) {
        void* mem = fn_MemAlloc(g_pMemAllocSingleton, 0x68, 8);
        if (mem) {
            void* entityLocal = *reinterpret_cast<void**>(s + 0x10);
            work = HarvestOrchard_CtorFromEntity(mem, &entityLocal);
        }
    }
    uintptr_t workIBP = 0;
    if (fn_InternalBasePtr_Ctor) new (&workIBP) ::Object::InternalBasePtr(
        reinterpret_cast<RtObject::Internal*>(work));   // B.3.3 bridge
    else workIBP = reinterpret_cast<uintptr_t>(work);

    uintptr_t targetIBP = 0;
    if (s_Find_3050) s_Find_3050(*reinterpret_cast<uintptr_t*>(s + 0x10), &targetIBP);
    if (s_PostWork) s_PostWork(targetIBP, 1, 0, &workIBP);

    // Footprint stamp. Retail passes *(gw+0xf8) — the map grid — and the rect as four
    // int16s off the position component.
    uintptr_t posIBP = 0;
    if (s_FindPosition_4680) s_FindPosition_4680(*reinterpret_cast<uintptr_t*>(s + 0x10), &posIBP);
    void* grid = nullptr;
    if (g_pGameWorldSingleton && *g_pGameWorldSingleton)
        grid = *reinterpret_cast<void**>(
            reinterpret_cast<uintptr_t>(*g_pGameWorldSingleton) + 0xf8);
    {   // The single line that says "the harvest actually started".
        static int s_n = 0;
        if (s_n++ < 4) {
            std::fprintf(stderr, "[Orchard11] enter: work=%p slots=%d grid=%p treeType=%p\n",
                         work, *reinterpret_cast<int32_t*>(s + 0x58), grid,
                         *reinterpret_cast<void**>(s + 0x60));
            std::fflush(stderr);
        }
    }
    if (posIBP)     // SA: retail derefs the finder's out-param unguarded; see state 9 enter.
        MapGrid_StampCellRefs_9f770(grid,
            *reinterpret_cast<int16_t*>(posIBP + 0x28), *reinterpret_cast<int16_t*>(posIBP + 0x2a),
            *reinterpret_cast<int16_t*>(posIBP + 0x2c), *reinterpret_cast<int16_t*>(posIBP + 0x2e),
            *reinterpret_cast<void**>(s + 0x60));
    return 1;
}

// ============================================================
// state 11 TRANS38 (per-frame) — retail 0x140050650
// The harvest-complete latch: count slots that still hold a tree (+0x04 == 1) against
// slots already picked (+0x20 != 0). Once picked >= live, release the work slot and go
// back to tending.
//
// The event-4 handler clears every slot's +0x20 on the way IN to state 11, which is what
// makes this latch meaningful — HarvestOrchard sets the byte per slot as citizens pick.
//
// ⚠ FAITHFUL zero-degenerate: with no slots 0 <= 0 holds, so a slot-less orchard exits to
// state 10 on its first frame.
// ============================================================
extern "C" char Orchard_State11PerFrame(void* selfV, void* sm, void* /*time*/) {
    Resolve();
    if (!selfV) return 1;
    uintptr_t s = reinterpret_cast<uintptr_t>(selfV);

    const int count = *reinterpret_cast<int32_t*>(s + 0x58);
    uintptr_t slots = *reinterpret_cast<uintptr_t*>(s + 0x50);
    int liveCount = 0, doneCount = 0;
    for (int i = 0; slots && i < count; ++i) {
        const int st = *reinterpret_cast<int32_t*>(slots + static_cast<uintptr_t>(i) * 0x28 + 4);
        if (st != 0 && st == 1) ++liveCount;        // retail's redundant pair, kept verbatim
    }
    for (int i = 0; slots && i < count; ++i)
        if (*reinterpret_cast<uint8_t*>(slots + static_cast<uintptr_t>(i) * 0x28 + 0x20) != 0)
            ++doneCount;
    {
        static int s_n = 0;
        if (++s_n <= 4 || (s_n % 600) == 0) {
            std::fprintf(stderr, "[Orchard11] perFrame #%d: live=%d picked=%d -> %s\n",
                         s_n, liveCount, doneCount,
                         liveCount <= doneCount ? "harvest done, request state 10" : "keep picking");
            std::fflush(stderr);
        }
    }
    if (liveCount > doneCount) return 1;

    // Retail inlines the WorkComponent scan here rather than calling 0x140003050; the scan
    // is identical (same type id DWORD_1401c8070, same IsDerivedFrom confirmation), so we
    // call the shared finder. Note the release is unconditional — no HasWorkSlot test.
    uintptr_t wcIBP = 0;
    if (s_Find_3050) s_Find_3050(*reinterpret_cast<uintptr_t*>(s + 0x10), &wcIBP);
    if (s_DeactivateWorkplace) s_DeactivateWorkplace(wcIBP);
    OrchardRequestState(sm, 10);
    return 1;
}

// ============================================================
// state 20 (0x14) — CLEAR / "cut the orchard down"
//
// This is the state the panel's CLEAR button (cmd id 3 -> node event 5) drives, and until
// now it was registered with three nullptr handlers. ControllerLive::Dispatcher treats a
// null handler as a graceful no-op, so the machine DID transition into state 20 and then
// sat there doing nothing — which from the panel is indistinguishable from a dead button.
// That is the user-reported "cut is clickable but does nothing".
//
// The two real leaves are already ported in clearareacomponent_states.cpp (retail's state
// 20 calls the same two functions the ClearAreaComponent controller does), so they are
// used through the exports rather than forked.
// ============================================================
extern "C" bool  ClearAreaCtrl_IsDone(void* comp);
extern "C" void* ClearAreaCtrl_BuildWork(void* work, void* ownerEnt, uint64_t descRef,
                                         uint8_t clearTrees, uint8_t clearGround,
                                         uint8_t clearBuildings);

namespace {

// The entity's description carries a list of sub-descriptions at desc+0x28 (count +0x30).
// State 20's enter scans it for the ClearAreaDescription and passes its +0x20 (the
// resource filter) into the work item. Type id read via the .rdata slot, which
// reg_cleararea_typeids.cpp now backs; GetTypeIdByName is the fallback for the same
// reason OrchardDescTypeId has one.
uint32_t ClearAreaDescTypeId() { return ClearAreaDescription::TypeId(); }   // ti cell 0x1401c7200

void* FindClearAreaDescription(uintptr_t entity) {
    const uint32_t want = ClearAreaDescTypeId();
    uintptr_t descList = entity ? *reinterpret_cast<uintptr_t*>(entity + 0x28) : 0;
    if (!descList || !want) return nullptr;
    const int count = *reinterpret_cast<int32_t*>(descList + 0x30);
    uintptr_t arr = *reinterpret_cast<uintptr_t*>(descList + 0x28);
    for (int i = 0; arr && i < count; ++i) {
        void* elem = *reinterpret_cast<void**>(arr + static_cast<uintptr_t>(i) * 8);
        if (!elem) continue;
        if (reinterpret_cast<Object::Base*>(elem)->Type() != want) continue;   // B.3.8 bridge (slot 2)
        // Retail confirms the hit with IsDerivedFrom before accepting it, and BREAKS out of
        // the loop either way — a type-id match that fails the derived test yields null.
        if (fn_IsDerivedFrom && !fn_IsDerivedFrom(elem, want)) return nullptr;
        return elem;
    }
    return nullptr;
}

}  // namespace

// ============================================================
// state 20 ENTER — retail 0x1400504e0 (Ghidra: ClearArea_impl_04e0)
// Release any work slot the orchard is holding, then post ONE ClearArea work item at
// priority 0x401 carrying the ClearAreaDescription's filter. The three literal flags
// (1, 0, 1) are retail's — clear trees, leave the ground, clear buildings — and are NOT
// read off the description the way ClearAreaComponent's own state 0 reads them.
// ============================================================
extern "C" char Orchard_State20Enter(void* selfV, void* /*sm*/) {
    Resolve();
    if (!selfV) return 1;
    uintptr_t s = reinterpret_cast<uintptr_t>(selfV);

    uintptr_t wcIBP = 0;
    if (s_Find_3050) s_Find_3050(*reinterpret_cast<uintptr_t*>(s + 0x10), &wcIBP);
    if (s_HasWorkSlot && s_HasWorkSlot(wcIBP)) {
        if (s_DeactivateWorkplace) s_DeactivateWorkplace(wcIBP);
    }

    uintptr_t entity = *reinterpret_cast<uintptr_t*>(s + 0x10);
    void* caDesc = FindClearAreaDescription(entity);

    void* work = nullptr;
    if (caDesc && fn_MemAlloc) {
        void* mem = fn_MemAlloc(g_pMemAllocSingleton, 0x78, 8);
        if (mem) {
            void* entityLocal = reinterpret_cast<void*>(entity);
            work = ClearAreaCtrl_BuildWork(
                mem, entityLocal,
                *reinterpret_cast<uint64_t*>(reinterpret_cast<uintptr_t>(caDesc) + 0x20),
                1, 0, 1);
        }
    }
    {   // The whole point of porting this state is to make "cut" observable. Retail derefs
        // caDesc+0x20 unguarded, so a miss here would be a FAULT in retail — meaning the
        // orchard template is expected to carry a ClearAreaDescription. Report it loudly
        // instead of faulting: if this line ever prints, the description list is the bug,
        // not this handler.
        static int s_n = 0;
        if (s_n++ < 4) {
            std::fprintf(stderr, "[Orchard20] enter: caDesc=%p work=%p slots=%d -> %s\n",
                         caDesc, work, *reinterpret_cast<int32_t*>(s + 0x58),
                         caDesc ? "post ClearArea (prio 0x401)"
                                : "NO ClearAreaDescription on this entity — retail would "
                                  "fault here; nothing posted, the cut will not progress");
            std::fflush(stderr);
        }
    }
    if (!work) return 1;

    uintptr_t workIBP = 0;
    if (fn_InternalBasePtr_Ctor) new (&workIBP) ::Object::InternalBasePtr(
        reinterpret_cast<RtObject::Internal*>(work));   // B.3.3 bridge
    else workIBP = reinterpret_cast<uintptr_t>(work);
    if (s_PostWork) s_PostWork(wcIBP, 0x401, 0, &workIBP);
    return 1;
}

// ============================================================
// state 20 UPDATE — retail 0x1400503e0
// Unconditional release of the work slot (no HasWorkSlot test, unlike state 10's update).
// ============================================================
extern "C" char Orchard_State20Update(void* selfV, void* /*sm*/, void* /*time*/) {
    Resolve();
    if (!selfV) return 1;
    uintptr_t s = reinterpret_cast<uintptr_t>(selfV);
    uintptr_t wcIBP = 0;
    if (s_Find_3050) s_Find_3050(*reinterpret_cast<uintptr_t*>(s + 0x10), &wcIBP);
    if (s_DeactivateWorkplace) s_DeactivateWorkplace(wcIBP);
    return 1;
}

// ============================================================
// state 20 TRANS38 (per-frame) — retail 0x140050430
// Find the entity's ClearAreaComponent (FUN_14000a170 = fn_FindComponent_SM) and ask
// FUN_140118440 whether the footprint is clear yet. When it is, request state 9 — i.e.
// a cut orchard re-plants itself with whatever tree type is now selected. That round trip
// through state 9 is what makes "cut" the REPLACE command in the panel.
// ============================================================
extern "C" char Orchard_State20PerFrame(void* selfV, void* sm, void* /*time*/) {
    Resolve();
    if (!selfV) return 1;
    uintptr_t s = reinterpret_cast<uintptr_t>(selfV);

    uintptr_t caIBP = 0;
    fn_FindComponent_SM(*reinterpret_cast<void**>(s + 0x10), &caIBP);

    // SA: retail derefs the finder result unguarded (caComp+0x18 is the description). A
    // null here is NOT benign — ClearAreaCtrl_IsDone answers `true` for a null component,
    // which would bounce the orchard straight back to state 9 and read exactly like the
    // "cut does nothing" symptom it is meant to fix. Report and stay put instead.
    if (!caIBP) {
        static int s_n = 0;
        if (s_n++ < 4) {
            std::fprintf(stderr, "[Orchard20] perFrame: entity has NO ClearAreaComponent — "
                         "the cut cannot complete (retail would fault here)\n");
            std::fflush(stderr);
        }
        return 1;
    }

    const bool done = ClearAreaCtrl_IsDone(reinterpret_cast<void*>(caIBP));
    {   // "waiting on the woodcutters" and "finished" are the only two outcomes, and they
        // look identical from outside; print the first few and then every 600 frames.
        static int s_n = 0;
        if (++s_n <= 4 || (s_n % 600) == 0) {
            std::fprintf(stderr, "[Orchard20] perFrame #%d: cleared=%d -> %s\n",
                         s_n, (int)done, done ? "request state 9 (replant)" : "still clearing");
            std::fflush(stderr);
        }
    }
    if (done) OrchardRequestState(sm, 9);
    return 1;
}

// Replica of OrchardDescription_helper_20f0 (0x1400520f0), in registration order.
// State ids are literal; the names in comments are inferred from the bodies.
static void OrchardCtrl_PopulateTable() {
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
    TableAddEvent(t, 3, reinterpret_cast<void*>(&Orchard_Event3Plant));

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
    TableAddEvent(t, 5, reinterpret_cast<void*>(&Orchard_Event5Clear));

    // ── state 10 — TEND / IDLE-GROWN ──────────────────────────────────────────────
    // enter    0x140050e30  hole -> state 12, else post a TendArea work
    // update   0x140050ba0  deactivate the workplace if active
    // trans38  0x140050c00  TryHarvest; hole -> 12; seasonal TendArea re-post
    // event 5  0x1400513e0 -> state 20 ; event 4  0x140050900 -> state 11
    // Event 4 has no wiring point yet (StateEntry carries no event list), but the harvest
    // is still reachable: TryHarvest calls the event-4 body directly, which is what retail's
    // per-frame does too.
    TableRegisterState(t, 10, reinterpret_cast<void*>(&Orchard_State10Enter), nullptr,
                       reinterpret_cast<void*>(&Orchard_State10PerFrame),
                       reinterpret_cast<void*>(&Orchard_State10Update));
    TableAddTransition(t, 3, nullptr);
    TableAddTransition(t, 12, nullptr);
    TableAddTransition(t, 11, nullptr);
    TableAddTransition(t, 20, nullptr);
    TableAddEvent(t, 5, reinterpret_cast<void*>(&Orchard_Event5Clear));
    TableAddEvent(t, 4, reinterpret_cast<void*>(&Orchard_Event4Harvest));

    // ── state 12 — REPLANT GAPS ───────────────────────────────────────────────────
    // enter    0x140050ae0  PlantOrchard_impl_0ae0: post a PlantOrchard for the holes
    // update   0x1400509b0
    // trans38  0x140050a10  TryHarvest; no holes left -> request state 10
    // event 5  0x1400513e0 -> state 20 ; event 4  0x140050900 -> state 11
    TableRegisterState(t, 12, reinterpret_cast<void*>(&Orchard_State12Enter), nullptr,
                       reinterpret_cast<void*>(&Orchard_State12PerFrame),
                       reinterpret_cast<void*>(&Orchard_State12Update));
    TableAddTransition(t, 3, nullptr);
    TableAddTransition(t, 10, nullptr);
    TableAddTransition(t, 11, nullptr);
    TableAddTransition(t, 20, nullptr);
    TableAddEvent(t, 5, reinterpret_cast<void*>(&Orchard_Event5Clear));
    TableAddEvent(t, 4, reinterpret_cast<void*>(&Orchard_Event4Harvest));

    // ── state 11 — HARVEST ────────────────────────────────────────────────────────
    // enter    0x140050800  HarvestOrchard_impl_0800: post a HarvestOrchard work, then
    //                       stamp the footprint cells with *(self+0x60)
    // trans38  0x140050650  harvest-complete latch -> request state 10
    // event 5  0x1400513e0 -> state 20
    TableRegisterState(t, 11, reinterpret_cast<void*>(&Orchard_State11Enter), nullptr,
                       reinterpret_cast<void*>(&Orchard_State11PerFrame), nullptr);
    TableAddTransition(t, 3, nullptr);
    TableAddTransition(t, 10, nullptr);
    TableAddTransition(t, 20, nullptr);
    TableAddEvent(t, 5, reinterpret_cast<void*>(&Orchard_Event5Clear));

    // ── state 20 (0x14) — CLEAR ───────────────────────────────────────────────────
    // enter    0x1400504e0  ClearArea_impl_04e0: post a ClearArea work at prio 0x401
    // update   0x1400503e0  deactivate the workplace
    // trans38  0x140050430  when the footprint tests clear -> request state 9
    TableRegisterState(t, 20, reinterpret_cast<void*>(&Orchard_State20Enter), nullptr,
                       reinterpret_cast<void*>(&Orchard_State20PerFrame),
                       reinterpret_cast<void*>(&Orchard_State20Update));
    TableAddTransition(t, 3, nullptr);
    TableAddTransition(t, 9, nullptr);

    // State 3 is the shared teardown/"stop" id every state can transition to. Retail
    // registers NO body for it in this table — it is a transition target only, so it
    // is deliberately absent here too rather than added as an empty state.
}

// Built ONCE at static-init time, as retail's OrchardDescription_helper_20f0 does
// (B38 D6/D7). Before, every OrchardComponent factory call re-ran it from both threads.
static const bool s_orchardStateTableBuilt = (OrchardCtrl_PopulateTable(), true);
