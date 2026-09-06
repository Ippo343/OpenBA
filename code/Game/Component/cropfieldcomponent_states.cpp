// code/Game/Component/cropfieldcomponent_states.cpp
// CropFieldComponent controller state machine.
//
// Structurally the Orchard's twin: a ControllerComponent (0x68) driving a drag-created
// area through plant / tend / harvest, with its own private state table. Where the
// orchard has six states (2/9/10/12/11/20) the crop field has FOUR bodies (2/9/10/11)
// plus two transition-only targets (3 and 21).
//
// ── How this table was recovered ──────────────────────────────────────────────────
// CropFieldDescription_Setup (0x14014edb0) tail-calls the populator. Ghidra decompiles
// that call as `TownHallUI_Ctor()` and names the whole helper family `TownHallUI_*`,
// which is WRONG — the disassembly is unambiguous:
//     14014edb4: CALL 0x14011b150
//     14014edb9: MOV dword ptr [0x1401d7ad8],EAX
// So the real populator is 0x14011b150, the state array is 0x1401d6b50 and the count is
// 0x1401d7ad0. (0x1401d7ad8, the thing Setup writes, is a separate "registered ok" flag
// — do not mistake it for the count.) Every handler address below was read from that
// function's DISASSEMBLY operands, not from Ghidra's symbol names.
//
// ── Register-helper argument order (0x1400330d0), verified from its own disassembly ──
// It stages the new entry in a stack buffer at RSP+0x30 and memcpy-style installs it:
//     [RSP+0x30] = ECX          -> +0x00 id
//     [RSP+0x38] = RDX          -> +0x08 enter
//     [RSP+0x40] = 0            -> +0x10 trans30   (always zero for this controller)
//     [RSP+0x48] = [RSP+0x170]  -> +0x18 trans38   (the 5th/stack argument)
//     [RSP+0x50] = R8           -> +0x20 update
// i.e. the shape is sub_330d0(id, enter, UPDATE, _, TRANS38) — update and trans38 are
// NOT in source order, and reading them the other way round would silently swap the
// per-frame handler with the update handler. Companions: 0x1400331f0 adds a transition,
// 0x1400332a0 adds an event.
//
// ── The recovered table ───────────────────────────────────────────────────────────
//   state 2   enter 0x14011a920  trans38 0x14011a860   -> 3, 9        event 3 -> 0x14011a7b0
//   state 9   enter 0x14011a310  trans38 0x14011a1e0   -> 3, 10
//   state 10  enter 0x14011a100  trans38 0x140119fd0
//                                update  0x140119f70   -> 3, 21, 11   event 4 -> 0x140119f00
//   state 11  enter 0x140119e60  trans38 0x140119d80   -> 3, 21
// States 3 and 21 are transition TARGETS with no registered body — exactly as the
// orchard table leaves its state 3 — so they are deliberately absent here too.
//
// ── STATUS: ALL ELEVEN HANDLER BODIES ARE PORTED ───────────────────────────────
// This block used to read "FIRST PASS: the handler BODIES are not ported yet, so every
// handler is registered as nullptr". That has been FALSE since the crop-field wave
// (81dc93c "reaches state 10", cbe5ab0 "HARVESTS", ce26616 "61 harvested"); the comment
// simply outlived the port that replaced it — the same trap as
// reference_stub_comment_outlived_the_port_that_replaced_it. Corrected 2026-09-06 after
// re-measuring, so the next reader does not re-port work that is already done.
//
// Every address in the table above now has a body in this file, and each body carries
// its retail address in its own banner comment:
//   0x14011a920 State2Enter   0x14011a860 State2PerFrame   0x14011a7b0 Event3Plant
//   0x14011a310 State9Enter   0x14011a1e0 State9PerFrame
//   0x14011a100 State10Enter  0x140119fd0 State10PerFrame  0x140119f70 State10Update
//                                                          0x140119f00 Event4Harvest
//   0x140119e60 State11Enter  0x140119d80 State11PerFrame
// The `nullptr` arguments that remain in the CropFieldCtrl_PopulateTable calls below are
// the trans30 slot, which retail leaves genuinely empty for this controller (the
// register helper at 0x1400330d0 hard-writes 0 into +0x10 for every entry) — they are
// NOT unported handlers. Read TableRegisterState's signature before assuming otherwise:
// it is (id, enter, TRANS30, TRANS38, UPDATE), so the per-frame body sits in the 4th
// argument and the update body in the 5th.

#include "Component/controller_state_table.h"
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>

// Work reimpl resolvers — see the note in CF_MakeAreaWork.
bool PlantArea_ResolveFuncs();
bool TendArea_ResolveFuncs();
bool HarvestArea_ResolveFuncs();

// Watched by the citizen idle claim walk (citizenai_housing_idle.cpp) so we can tell
// "never reached" from "reached and rejected".
extern "C" void* g_pCropPlantWork = nullptr;

namespace { struct CropFieldCtrlTag {}; }
using CropFieldDispatch = ::ControllerLive::Dispatcher<CropFieldCtrlTag>;

extern "C" void** CropFieldCtrl_NodeVtable() { return CropFieldDispatch::Vtable(); }

// ── Resolved helpers ──────────────────────────────────────────────────────────────
// 0x140002e10 FindComponent_WorkPlace(entity, outIBP) -> WorkPlaceComponent.
// 0x14012fc00 WorkPlace SetAccepting(wpc, accepting)  -> drives WPC+0x50.
namespace {

typedef void (*fn_FindComp_t)(uintptr_t entity, uintptr_t* out);
typedef void (*fn_SetAccepting_t)(uintptr_t wpc, char accepting);

fn_FindComp_t     s_findWpc      = nullptr;   // 0x140002e10
fn_SetAccepting_t s_setAccepting = nullptr;   // 0x14012fc00
const float*      s_fTempMin     = nullptr;   // 0x1401721d4 = 33.0f
const float*      s_fYearSeconds = nullptr;   // 0x14017225c = 3600.0f
const float*      s_fYearMonths  = nullptr;   // 0x1401721b8 = 12.0f
bool              s_resolved     = false;

void CF_Resolve() {
    if (s_resolved) return;
    s_findWpc      = reinterpret_cast<fn_FindComp_t>(ResolveAddr(Module::Exe, 0x140002e10));
    s_setAccepting = reinterpret_cast<fn_SetAccepting_t>(ResolveAddr(Module::Exe, 0x14012fc00));
    s_fTempMin     = reinterpret_cast<const float*>(ResolveAddr(Module::Exe, 0x1401721d4));
    s_fYearSeconds = reinterpret_cast<const float*>(ResolveAddr(Module::Exe, 0x14017225c));
    s_fYearMonths  = reinterpret_cast<const float*>(ResolveAddr(Module::Exe, 0x1401721b8));
    s_resolved = true;
}

// Push the component's +0x48 master switch into the workplace's accepting flag.
// Shared by state-2 enter and the event-3 plant command, both of which do exactly
// this before doing anything else.
void CF_PushAccepting(uintptr_t comp) {
    CF_Resolve();
    if (!s_findWpc || !s_setAccepting) return;
    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(reinterpret_cast<void*>(comp + 0x10));
    uintptr_t wpcIbp = 0;
    s_findWpc(*reinterpret_cast<uintptr_t*>(comp + 0x10), &wpcIbp);
    if (!wpcIbp) return;
    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&wpcIbp);
    // wpcIbp already IS the component — the finder returns its out-param's address, so
    // there is no second dereference here (the trap that crashed the RP family).
    s_setAccepting(wpcIbp, *reinterpret_cast<char*>(comp + 0x48));
}

}  // namespace

// ── Slot array layout (self+0x58, stride 0x20, count self+0x60) ───────────────────
// Recovered from the state-9 enter fill loop and every consumer below:
//   +0x00 int   cellIndex      map cell this slot covers
//   +0x04 int   state          0 = empty, 1 = growing, 2 = done/harvested
//   +0x08 IBP                  (zeroed at plant)
//   +0x10 IBP                  the planted NaturalResource; its +0x28 is growth 0..1
//   +0x18 byte  plantHere      whether this cell gets a crop (spacing + shuffle)
// self+0x48 = master switch, self+0x49 = "a crop type is set", self+0x50 = the
// description list the crop type is read from.
namespace {

constexpr int kSlotStride = 0x20;

typedef void (*fn_ArrayResize_t)(void* vec, int count);        // 0x1400335b0 slots
typedef void (*fn_ByteArrayResize_t)(void* vec, int count);    // 0x14000a590
typedef void (*fn_SlotRefresh_t)(uintptr_t comp);              // 0x14011aa20
typedef void (*fn_FindComp_t2)(uintptr_t entity, uintptr_t* out);
typedef void (*fn_PostWork_t)(uintptr_t workComp, uint32_t code, char flag, void* workIBP);
typedef bool (*fn_SlotOccupied_t)(uintptr_t comp);
typedef void (*fn_SlotRelease_t)(uintptr_t comp);
typedef void (*fn_MapInit_t)(uintptr_t mapComp);               // 0x140124750
typedef void* (*fn_WorkCtor25_t)(void* self, void* pEntityIBP);
typedef uint32_t (*fn_Rand_t)(void* rng, uint32_t bound);

fn_ArrayResize_t     s_slotResize   = nullptr;
fn_ByteArrayResize_t s_byteResize   = nullptr;
fn_SlotRefresh_t     s_slotRefresh  = nullptr;
fn_FindComp_t2       s_findMap      = nullptr;   // 0x140004680
fn_FindComp_t2       s_findWork     = nullptr;   // 0x140003050
fn_FindComp_t2       s_findCropComp = nullptr;   // 0x140005380
fn_PostWork_t        s_postWork     = nullptr;   // 0x14012ec60
fn_SlotOccupied_t    s_slotOccupied = nullptr;   // 0x14012ead0
fn_SlotRelease_t     s_slotRelease  = nullptr;   // 0x14012eb50
fn_MapInit_t         s_mapInit      = nullptr;   // 0x140124750
fn_WorkCtor25_t      s_workCtor25   = nullptr;   // 0x1400f72e0
const float*         s_fRipe        = nullptr;   // 0x140172144 = 0.99f
uint32_t*            s_tidCropDesc  = nullptr;   // 0x1401c7b68
uint32_t*            s_tidRawMatDesc= nullptr;   // 0x1401c7cb8
bool                 s_res2         = false;

void CF_Resolve2() {
    if (s_res2) return;
    s_slotResize   = reinterpret_cast<fn_ArrayResize_t>(ResolveAddr(Module::Exe, 0x1400335b0));
    s_byteResize   = reinterpret_cast<fn_ByteArrayResize_t>(ResolveAddr(Module::Exe, 0x14000a590));
    s_slotRefresh  = reinterpret_cast<fn_SlotRefresh_t>(ResolveAddr(Module::Exe, 0x14011aa20));
    s_findMap      = reinterpret_cast<fn_FindComp_t2>(ResolveAddr(Module::Exe, 0x140004680));
    s_findWork     = reinterpret_cast<fn_FindComp_t2>(ResolveAddr(Module::Exe, 0x140003050));
    s_findCropComp = reinterpret_cast<fn_FindComp_t2>(ResolveAddr(Module::Exe, 0x140005380));
    s_postWork     = reinterpret_cast<fn_PostWork_t>(ResolveAddr(Module::Exe, 0x14012ec60));
    s_slotOccupied = reinterpret_cast<fn_SlotOccupied_t>(ResolveAddr(Module::Exe, 0x14012ead0));
    s_slotRelease  = reinterpret_cast<fn_SlotRelease_t>(ResolveAddr(Module::Exe, 0x14012eb50));
    s_mapInit      = reinterpret_cast<fn_MapInit_t>(ResolveAddr(Module::Exe, 0x140124750));
    s_workCtor25   = reinterpret_cast<fn_WorkCtor25_t>(ResolveAddr(Module::Exe, 0x1400f72e0));
    s_fRipe        = reinterpret_cast<const float*>(ResolveAddr(Module::Exe, 0x140172144));
    s_tidCropDesc  = reinterpret_cast<uint32_t*>(ResolveAddr(Module::Exe, 0x1401c7b68));
    s_tidRawMatDesc= reinterpret_cast<uint32_t*>(ResolveAddr(Module::Exe, 0x1401c7cb8));
    s_res2 = true;
}

uint8_t* SlotAt(uintptr_t comp, int i) {
    uintptr_t base = *reinterpret_cast<uintptr_t*>(comp + 0x58);
    return base ? reinterpret_cast<uint8_t*>(base + (uintptr_t)i * kSlotStride) : nullptr;
}
int SlotCount(uintptr_t comp) { return *reinterpret_cast<int32_t*>(comp + 0x60); }

// Scan a description list object (items +0x28 / count +0x30) for a type.
void* FindInDescList(uintptr_t list, uint32_t tid) {
    if (!list || !tid) return nullptr;
    const int n = *reinterpret_cast<int32_t*>(list + 0x30);
    void** items = *reinterpret_cast<void***>(list + 0x28);
    for (int i = 0; i < n && items; ++i)
        if (items[i] && fn_IsDerivedFrom && fn_IsDerivedFrom(items[i], tid)) return items[i];
    return nullptr;
}

// The shared tail of all three area-work constructors (PlantArea 0x1400f46a0,
// HarvestArea 0x1400ed4c0, and the state-10 TendArea build): Work_Ctor25, install the
// leaf vtable, zero {+0x48 q, +0x50 d, +0x58 q, +0x60 q}, then — for Plant/Harvest —
// size a per-slot byte array at +0x58 to the crop field's slot count and fill it with 1s.
void* CF_MakeAreaWork(uintptr_t comp, int size, uintptr_t vtableAddr,
                      bool perSlotArray, uint64_t descListField, bool hasDescField) {
    CF_Resolve2();
    // Each Work reimpl resolves its engine-function pointers in a *_ResolveFuncs() that
    // ONLY the factory path calls. These works are constructed directly, exactly as
    // retail does, so the resolver has to be called here or the work ships with null
    // helper pointers and dies the first time the scheduler asks it anything — measured:
    // PlantArea_GetTarget_Impl called a null pointer from inside the scheduler insert,
    // three frames away from the construction that caused it. Same trap the
    // ResourceProduce CreateWork family hit; cheap and idempotent, so call all three.
    PlantArea_ResolveFuncs();
    TendArea_ResolveFuncs();
    HarvestArea_ResolveFuncs();
    if (!fn_MemAlloc || !g_pMemAllocSingleton || !s_workCtor25) return nullptr;
    void* mem = fn_MemAlloc(g_pMemAllocSingleton, size, 8);
    if (!mem) return nullptr;

    uintptr_t entity = *reinterpret_cast<uintptr_t*>(comp + 0x10);
    s_workCtor25(mem, &entity);
    if (uintptr_t vt = ResolveAddr(Module::Exe, vtableAddr))
        *reinterpret_cast<uintptr_t*>(mem) = vt;

    uintptr_t w = reinterpret_cast<uintptr_t>(mem);
    *reinterpret_cast<uint64_t*>(w + 0x48) = 0;
    *reinterpret_cast<uint32_t*>(w + 0x50) = 0;
    // The three retail ctors differ in their tails and the object SIZES differ with them:
    //   PlantArea   0x1400f46a0 (0x70): +0x58 q = 0, +0x60 q = 0, +0x68 q = descList
    //   HarvestArea 0x1400ed4c0 (0x68): +0x58 q = 0, +0x60 q = 0
    //   TendArea    0x14011a100 (0x60): +0x58 DWORD = 0   -- nothing at +0x60
    // The shared tail used to zero +0x60 as a qword for all three: for TendArea that is
    // 8 bytes PAST the 0x60-byte allocation, i.e. the boundary tag of the next heap block.
    // Every crop field entering state 10 zeroed a neighbour's tag; the engine allocator's
    // coalescing then produced the "free-list link with wrong high bits" faults in
    // EntityList_Shutdown at the end of every long 800x10 stress run (2026-09-04, caught
    // in the act by the REDZONE watchdog: preceding block = g_TendArea_Vtable object,
    // frame 3146, right after "[CropXition] STATE 10 ENTER - posting TendArea").
    if (size >= 0x68) {
        *reinterpret_cast<uint64_t*>(w + 0x58) = 0;
        *reinterpret_cast<uint64_t*>(w + 0x60) = 0;
    } else {
        *reinterpret_cast<uint32_t*>(w + 0x58) = 0;
    }
    if (hasDescField) *reinterpret_cast<uint64_t*>(w + 0x68) = descListField;

    if (perSlotArray && s_findCropComp && s_byteResize) {
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(reinterpret_cast<void*>(w + 0x10));
        uintptr_t cropIbp = 0;
        s_findCropComp(*reinterpret_cast<uintptr_t*>(w + 0x10), &cropIbp);
        if (cropIbp) {
            if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&cropIbp);
            s_byteResize(reinterpret_cast<void*>(w + 0x58),
                         *reinterpret_cast<int32_t*>(cropIbp + 0x60));
            const int n = *reinterpret_cast<int32_t*>(w + 0x60);
            uint8_t* arr = *reinterpret_cast<uint8_t**>(w + 0x58);
            if (arr) for (int i = 0; i < n; ++i) arr[i] = 1;
        }
    }
    return mem;
}

// Post a freshly built work to the reservation ledger, exactly as every state enter does.
void CF_PostWork(uintptr_t comp, void* work) {
    CF_Resolve2();
    // Report the OUTCOME, not the attempt. Every early return here looks identical from
    // outside — the state enters, no work runs — and the crop field spent a whole round
    // stuck at exactly that ambiguity. Gated on the crop trace so it costs nothing off.
    static int s_t = -1;
    if (s_t < 0) {
        const char* e = std::getenv("BANISHED_CROPFIELD_TRACE");
        s_t = (e && e[0] && e[0] != '0') ? 1 : 0;
    }
    if (!work || !s_findWork || !s_postWork) {
        if (s_t) std::fprintf(stderr, "[CropWork] POST SKIPPED work=%p findWork=%p post=%p\n",
                              work, (void*)s_findWork, (void*)s_postWork);
        return;
    }
    uintptr_t workIbp = 0;
    if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor(&workIbp, work);
    else workIbp = reinterpret_cast<uintptr_t>(work);
    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(reinterpret_cast<void*>(comp + 0x10));
    uintptr_t workCompIbp = 0;
    s_findWork(*reinterpret_cast<uintptr_t*>(comp + 0x10), &workCompIbp);
    if (!workCompIbp) {
        // The crop field entity carries no WorkComponent, so there is no ledger to post
        // to and no citizen can ever claim the work. Distinct from "posted and ignored".
        if (s_t) std::fprintf(stderr, "[CropWork] POST FAILED: the field entity has NO "
                              "WorkComponent (finder 0x140003050 returned null) — nothing "
                              "can claim this work\n");
        return;
    }
    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&workCompIbp);
    s_postWork(workCompIbp, 1, 0, &workIbp);
    g_pCropPlantWork = work;   // watched by the citizen claim-walk probe
    if (s_t)
        std::fprintf(stderr, "[CropWork] POSTED work=%p to workComp=%p (assignedId=%d)\n",
                     work, (void*)workCompIbp,
                     *reinterpret_cast<int32_t*>(workCompIbp + 0x28));
}

// Release the held work slot if one is occupied (the 3050 -> ead0 -> eb50 idiom every
// crop-field per-frame handler opens with).
void CF_ReleaseWorkSlot(uintptr_t comp) {
    CF_Resolve2();
    if (!s_findWork || !s_slotOccupied || !s_slotRelease) return;
    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(reinterpret_cast<void*>(comp + 0x10));
    uintptr_t workCompIbp = 0;
    s_findWork(*reinterpret_cast<uintptr_t*>(comp + 0x10), &workCompIbp);
    if (!workCompIbp) return;
    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&workCompIbp);
    if (s_slotOccupied(workCompIbp)) s_slotRelease(workCompIbp);
}

// Count slots in a given state.
int CountSlots(uintptr_t comp, int state) {
    int n = 0;
    const int total = SlotCount(comp);
    for (int i = 0; i < total; ++i) {
        uint8_t* s = SlotAt(comp, i);
        if (s && *reinterpret_cast<int32_t*>(s + 0x04) == state) ++n;
    }
    return n;
}

// retail 0x14011b410 — mean crop maturity: average of min(growth, 1.0) over slots in
// state 1, divided by the TOTAL slot count (not by the number of growing slots, so a
// half-planted field can never read ripe).
float CF_Maturity(uintptr_t comp) {
    CF_Resolve2();
    const int total = SlotCount(comp);
    if (total == 0) return 0.0f;
    if (s_slotRefresh) s_slotRefresh(comp);
    const float cap = g_fUVScale ? *g_fUVScale : 1.0f;
    float sum = 0.0f;
    for (int i = 0; i < total; ++i) {
        uint8_t* s = SlotAt(comp, i);
        if (!s || *reinterpret_cast<int32_t*>(s + 0x04) != 1) continue;
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(s + 0x10);
        uintptr_t nr = *reinterpret_cast<uintptr_t*>(s + 0x10);
        if (!nr) continue;
        float g = *reinterpret_cast<float*>(nr + 0x28);
        if (g > cap) g = cap;
        sum += g;
    }
    return sum / (float)total;
}

}  // namespace

// Retail 0x14011b410 — exposed so CropUI's per-frame slot can resolve it the way
// retail does (CropUI_Update calls FUN_14011b410 to drive the yield bar). CF_Maturity
// itself is in the anonymous namespace above; this is the registered entry point.
extern "C" float CropFieldComponent_Maturity(void* comp) {
    return comp ? CF_Maturity(reinterpret_cast<uintptr_t>(comp)) : 0.0f;
}

// ── state 2 enter — 0x14011a920 ───────────────────────────────────────────────────
// Nothing but "publish my master switch to the workplace".
static char CropField_State2Enter(void* self, void* /*sm*/) {
    CF_PushAccepting(reinterpret_cast<uintptr_t>(self));
    return 0;
}

// ── event 3 on state 2 — 0x14011a7b0 — the panel's PLANT command ──────────────────
// Publish accepting, then request state 9.
static char CropField_Event3Plant(void* self, void* sm) {
    CF_PushAccepting(reinterpret_cast<uintptr_t>(self));
    ::ControllerLive::RequestStateTransition(sm, 9);
    return 1;
}

// ── state 2 per-frame — 0x14011a860 — the SEASONAL PLANTING TRIGGER ───────────────
// Faithful shape:
//   if (self+0x48 == 0) return 1;                       // field switched off
//   if (weather(gw+0x130)+0x18 < 33.0f) return 1;       // too cold
//   v = (int)((Time::AsSeconds(gw+0x128 + 8) / 3600.0f) * 12.0f);
//   if (v < 0) return 1;
//   if ((unsigned)(v - 3) > 8) Event3Plant(self, sm);   // -> state 9
//
// ⚠ That last test is an UNSIGNED compare on `v - 3`, so it is true both when v > 11
// AND when v < 3 (the subtraction underflows to a huge unsigned). It is retail's
// idiomatic "outside the closed window [3, 11]" check — i.e. plant in the cold months
// at either end of the year, not mid-season. Writing it as a signed `v - 3 > 8` would
// silently drop the entire v < 3 arm and the field would only ever plant late in the
// year. Kept as an explicit unsigned cast so the behaviour cannot drift.
static char CropField_State2PerFrame(void* self, void* sm, void* /*time*/) {
    CF_Resolve();
    const uintptr_t comp = reinterpret_cast<uintptr_t>(self);
    if (*reinterpret_cast<char*>(comp + 0x48) == 0) return 1;
    if (!g_pGameWorldSingleton || !*g_pGameWorldSingleton) return 1;
    if (!fn_Time_AsSeconds || !s_fTempMin || !s_fYearSeconds || !s_fYearMonths) return 1;

    const uintptr_t gw = reinterpret_cast<uintptr_t>(*g_pGameWorldSingleton);
    const uintptr_t seasonMgr = *reinterpret_cast<uintptr_t*>(gw + 0x128);
    const uintptr_t weather   = *reinterpret_cast<uintptr_t*>(gw + 0x130);
    if (!seasonMgr || !weather) return 1;

    if (*reinterpret_cast<float*>(weather + 0x18) < *s_fTempMin) return 1;

    const float secs = fn_Time_AsSeconds(reinterpret_cast<void*>(seasonMgr + 8));
    const int v = static_cast<int>((secs / *s_fYearSeconds) * *s_fYearMonths);
    if (v < 0) return 1;
    if (static_cast<uint32_t>(v - 3) > 8u)
        CropField_Event3Plant(self, sm);
    return 1;
}

// ── state 9 enter — 0x14011a310 — BUILD THE GRID AND POST A PlantArea ─────────────
// 1. Read the footprint from the MapComponent: origin (+0x28,+0x2a), extent (+0x2c,+0x2e).
// 2. Resize the slot array to w*h, filling each slot with {cellIndex, state 0, null, null}.
// 3. Decide which cells actually get a crop: if the crop type (desc list +0x20 off the
//    CropFieldDescription) is unset, +0x49 = 0 and nothing is planted; otherwise +0x49 = 1
//    and every spacing-th cell is marked, spacing = RawMaterialDescription+0x4c.
// 4. Shuffle those marks (Fisher-Yates over the game RNG) so rows are not identical.
// 5. Re-init the map component, then post a PlantArea work carrying the desc list.
static char CropField_State9Enter(void* self, void* /*sm*/) {
    CF_Resolve2();
    const uintptr_t comp = reinterpret_cast<uintptr_t>(self);
    if (!s_findMap || !s_slotResize) return 1;

    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(reinterpret_cast<void*>(comp + 0x10));
    uintptr_t mapIbp = 0;
    s_findMap(*reinterpret_cast<uintptr_t*>(comp + 0x10), &mapIbp);
    if (!mapIbp) return 1;
    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&mapIbp);
    const int ox = *reinterpret_cast<int16_t*>(mapIbp + 0x28);
    const int oy = *reinterpret_cast<int16_t*>(mapIbp + 0x2a);
    const int w  = *reinterpret_cast<int16_t*>(mapIbp + 0x2c);
    const int h  = *reinterpret_cast<int16_t*>(mapIbp + 0x2e);

    s_slotResize(reinterpret_cast<void*>(comp + 0x58), w * h);

    if (!g_pGameWorldSingleton || !*g_pGameWorldSingleton) return 1;
    const uintptr_t gw = reinterpret_cast<uintptr_t>(*g_pGameWorldSingleton);
    int* mapGrid = *reinterpret_cast<int**>(gw + 0xf8);
    if (!mapGrid) return 1;
    const int mapW = *mapGrid;

    for (int row = 0; row < h; ++row) {
        for (int col = 0; col < w; ++col) {
            uint8_t* s = SlotAt(comp, row * w + col);
            if (!s) continue;
            *reinterpret_cast<int32_t*>(s + 0x00) = (row + oy) * mapW + col + ox;
            uintptr_t nullIbp = 0;
            if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor(&nullIbp, nullptr);
            *reinterpret_cast<uintptr_t*>(s + 0x10) = nullIbp;
            nullIbp = 0;
            if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor(&nullIbp, nullptr);
            *reinterpret_cast<uintptr_t*>(s + 0x08) = nullIbp;
            *reinterpret_cast<int32_t*>(s + 0x04) = 0;
        }
    }

    const int count = SlotCount(comp);
    if (count <= 0) return 1;
    uint8_t* marks = reinterpret_cast<uint8_t*>(
        (fn_MemAlloc && g_pMemAllocSingleton)
            ? fn_MemAlloc(g_pMemAllocSingleton, count, 1) : nullptr);
    if (!marks) return 1;

    // Crop type: CropFieldDescription (0x1401c7b68) in the desc list at self+0x50, then
    // its +0x20. Null means "no crop chosen" — the field stays bare and +0x49 clears.
    void* cropDesc = FindInDescList(*reinterpret_cast<uintptr_t*>(comp + 0x50),
                                    s_tidCropDesc ? *s_tidCropDesc : 0);
    uintptr_t cropList = cropDesc
        ? *reinterpret_cast<uintptr_t*>(reinterpret_cast<uintptr_t>(cropDesc) + 0x20) : 0;

    if (cropList == 0) {
        *reinterpret_cast<uint8_t*>(comp + 0x49) = 0;
        for (int i = 0; i < count; ++i) marks[i] = 0;
    } else {
        *reinterpret_cast<uint8_t*>(comp + 0x49) = 1;
        void* rawMat = FindInDescList(cropList, s_tidRawMatDesc ? *s_tidRawMatDesc : 0);
        const int spacing = rawMat
            ? *reinterpret_cast<int32_t*>(reinterpret_cast<uintptr_t>(rawMat) + 0x4c) : 1;
        for (int i = 0; i < count; ++i)
            marks[i] = (spacing != 0 && (i % spacing) == 0) ? 1 : 0;
    }

    void* rng = *reinterpret_cast<void**>(gw + 0xd8);
    if (rng && fn_Random_Rand) {
        for (int i = 0; i < count; ++i) {
            const uint32_t j = fn_Random_Rand(rng, static_cast<uint32_t>(count));
            if (static_cast<uint32_t>(i) != j) {
                uint8_t t = marks[i]; marks[i] = marks[j]; marks[j] = t;
            }
        }
    }
    for (int i = 0; i < count; ++i) {
        uint8_t* s = SlotAt(comp, i);
        if (s) s[0x18] = marks[i];
    }

    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&mapIbp);
    if (s_mapInit) s_mapInit(mapIbp);

    // PlantArea is 0x70 and carries the desc list at +0x68 (vtable 0x140161890).
    void* work = CF_MakeAreaWork(comp, 0x70, 0x140161890, /*perSlotArray=*/true,
                                 *reinterpret_cast<uint64_t*>(comp + 0x50),
                                 /*hasDescField=*/true);
    CF_PostWork(comp, work);

    if (fn_MemFree && g_pMemAllocSingleton) fn_MemFree(g_pMemAllocSingleton, marks);
    return 1;
}

// ── state 9 per-frame — 0x14011a1e0 ───────────────────────────────────────────────
// Refresh the slots, then: all done -> 21; all either growing or done -> 10 (tend).
static char CropField_State9PerFrame(void* self, void* sm, void* /*time*/) {
    CF_Resolve2();
    const uintptr_t comp = reinterpret_cast<uintptr_t>(self);
    if (s_slotRefresh) s_slotRefresh(comp);
    const int total   = SlotCount(comp);
    const int growing = CountSlots(comp, 1);
    const int done    = CountSlots(comp, 2);

    // Slot-state histogram. Planting is measured to run to completion (64 cells, 64
    // binds), so every slot should read 1 "growing" and the transition below should
    // fire. It does not, and the two candidate causes need different fixes:
    // ValidateEntityRefs (s_slotRefresh, 0x14011aa20) demoting slots whose entity fails
    // validation, versus the transition request being dropped. The histogram separates
    // them — it prints what the states ACTUALLY are, not what they should be.
    {
        static int s_h = -1;
        if (s_h < 0) {
            const char* he = std::getenv("BANISHED_CROPFIELD_TRACE");
            s_h = (he && he[0] && he[0] != '0') ? 1 : 0;
        }
        static int s_n = 0;
        if (s_h && (s_n % 600) == 0) {
            int hist[8] = {0};
            int other = 0;
            for (int i = 0; i < total; ++i) {
                uint8_t* sl = SlotAt(comp, i);
                if (!sl) continue;
                const int st = *reinterpret_cast<int32_t*>(sl + 0x04);
                if (st >= 0 && st < 8) ++hist[st]; else ++other;
            }
            std::fprintf(stderr, "[CropSlots] total=%d growing=%d done=%d | state histogram "
                         "0:%d 1:%d 2:%d 3:%d other:%d\n",
                         total, growing, done, hist[0], hist[1], hist[2], hist[3], other);
            // Which slots are still at 0? Contiguous tail indices mean a pairing/edge
            // boundary; scattered indices mean individual plant actions failed.
            {
                char buf[512]; int n = 0;
                for (int i = 0; i < total && n < (int)sizeof(buf) - 24; ++i) {
                    uint8_t* sl = SlotAt(comp, i);
                    if (!sl || *reinterpret_cast<int32_t*>(sl + 0x04) != 0) continue;
                    n += std::snprintf(buf + n, sizeof(buf) - n, " %d(cell=%d,mark=%u)",
                                       i, *reinterpret_cast<int32_t*>(sl + 0x00),
                                       (unsigned)sl[0x18]);
                }
                std::fprintf(stderr, "[CropSlots] state0 slots:%s\n", n ? buf : " none");
            // How many of this field's cells currently hold an entity on the MAP TILE?
            // CreateNaturalResource verifies tile+8 is SET at plant time and HarvestArea
            // finds it NULL later, at the SAME cell indices, with dc20 recording zero
            // wipes. So the crops are being destroyed between the two. Sampling the live
            // count each time this prints shows whether it starts full and decays, and
            // roughly when.
            {
                const uintptr_t gwx = reinterpret_cast<uintptr_t>(*g_pGameWorldSingleton);
                const uintptr_t mp = gwx ? *reinterpret_cast<uintptr_t*>(gwx + 0xf8) : 0;
                const uintptr_t gd = mp ? *reinterpret_cast<uintptr_t*>(mp + 0x80) : 0;
                int live = 0;
                if (gd) for (int i = 0; i < total; ++i) {
                    uint8_t* sl = SlotAt(comp, i);
                    if (!sl) continue;
                    const int c = *reinterpret_cast<int32_t*>(sl + 0x00);
                    if (*reinterpret_cast<uintptr_t*>(gd + 8 + (uintptr_t)c * 0x30)) ++live;
                }
                std::fprintf(stderr, "[CropTiles] %d/%d field cells hold an entity on the map tile\n",
                             live, total);
                std::fflush(stderr);
            }
            }
            std::fflush(stderr);
        }
        ++s_n;
    }
    if (done == total) {
        CF_ReleaseWorkSlot(comp);
        std::fprintf(stderr, "[CropXition] state 9 -> 21 (all %d done)\n", total);
        std::fflush(stderr);
        ::ControllerLive::RequestStateTransition(sm, 21);
    } else if (growing + done == total) {
        CF_ReleaseWorkSlot(comp);
        std::fprintf(stderr, "[CropXition] state 9 -> 10 TEND (growing=%d done=%d total=%d)\n",
                     growing, done, total);
        std::fflush(stderr);
        ::ControllerLive::RequestStateTransition(sm, 10);
    }
    return 1;
}

// ── state 10 enter — 0x14011a100 — post a TendArea ────────────────────────────────
// TendArea is 0x60 with no per-slot array and no desc field (vtable 0x140161990).
static char CropField_State10Enter(void* self, void* /*sm*/) {
    const uintptr_t comp = reinterpret_cast<uintptr_t>(self);
    std::fprintf(stderr, "[CropXition] STATE 10 ENTER - posting TendArea\n");
    std::fflush(stderr);
    void* work = CF_MakeAreaWork(comp, 0x60, 0x140161990, false, 0, false);
    CF_PostWork(comp, work);
    return 1;
}

// ── state 10 update — 0x140119f70 — release the held work slot ────────────────────
static char CropField_State10Update(void* self, void* /*sm*/) {
    CF_ReleaseWorkSlot(reinterpret_cast<uintptr_t>(self));
    return 1;
}

// ── event 4 on state 10 — 0x140119f00 — the panel's HARVEST command ───────────────
static char CropField_Event4Harvest(void* self, void* sm) {
    (void)self;
    std::fprintf(stderr, "[CropXition] event 4 -> STATE 11 HARVEST\n");
    std::fflush(stderr);
    ::ControllerLive::RequestStateTransition(sm, 11);
    return 1;
}

// ── state 10 per-frame — 0x140119fd0 — the HARVEST TRIGGER ────────────────────────
// All slots done -> 21. Then, if the field is on AND has a crop type:
//   warm (temp >= 33) AND the year position inside [6,11]  -> harvest NOW
//   otherwise                                              -> harvest only if ripe
//                                                             (mean maturity > 0.99)
// The (unsigned)(month - 6) > 5 test is again retail's "outside [6,11]" idiom — true
// for month < 6 via underflow as well as for month > 11.
static char CropField_State10PerFrame(void* self, void* sm, void* /*time*/) {
    CF_Resolve2();
    const uintptr_t comp = reinterpret_cast<uintptr_t>(self);
    if (s_slotRefresh) s_slotRefresh(comp);

    const int total = SlotCount(comp);
    if (CountSlots(comp, 2) == total)
        ::ControllerLive::RequestStateTransition(sm, 21);

    // Report WHICH guard stops the trigger. These five early returns all look identical
    // from outside -- a silent state 10 says only "no harvest", not why -- and the
    // [CropHarvest] census below can never print if one of them fires first.
    {
        static int s_gt = -1;
        if (s_gt < 0) { const char* ge = std::getenv("BANISHED_CROPFIELD_TRACE");
                        s_gt = (ge && ge[0] && ge[0] != '0') ? 1 : 0; }
        static int s_g = 0;
        if (s_gt && (s_g++ % 600) == 0) {
            const uintptr_t gwd = (g_pGameWorldSingleton && *g_pGameWorldSingleton)
                ? reinterpret_cast<uintptr_t>(*g_pGameWorldSingleton) : 0;
            std::fprintf(stderr, "[CropTrigger] switch(+0x48)=%d cropType(+0x49)=%d gw=%d "
                         "consts=%d weather=%d season=%d\n",
                         (int)*reinterpret_cast<char*>(comp + 0x48),
                         (int)*reinterpret_cast<char*>(comp + 0x49), gwd != 0,
                         (s_fTempMin?1:0)|(s_fYearSeconds?2:0)|(s_fYearMonths?4:0)
                          |(s_fRipe?8:0)|(fn_Time_AsSeconds?16:0),
                         gwd ? (*reinterpret_cast<uintptr_t*>(gwd + 0x130) != 0) : 0,
                         gwd ? (*reinterpret_cast<uintptr_t*>(gwd + 0x128) != 0) : 0);
            std::fflush(stderr);
        }
    }

    if (*reinterpret_cast<char*>(comp + 0x48) == 0) return 1;
    if (*reinterpret_cast<char*>(comp + 0x49) == 0) return 1;
    if (!g_pGameWorldSingleton || !*g_pGameWorldSingleton) return 1;
    if (!s_fTempMin || !s_fYearSeconds || !s_fYearMonths || !s_fRipe || !fn_Time_AsSeconds)
        return 1;

    const uintptr_t gw = reinterpret_cast<uintptr_t>(*g_pGameWorldSingleton);
    const uintptr_t weather   = *reinterpret_cast<uintptr_t*>(gw + 0x130);
    const uintptr_t seasonMgr = *reinterpret_cast<uintptr_t*>(gw + 0x128);
    if (!weather || !seasonMgr) return 1;

    bool harvestNow = false;
    bool seasonArm = false;
    int  monthDbg = -1;
    const float temp = *reinterpret_cast<float*>(weather + 0x18);
    if (temp >= *s_fTempMin) {
        const float secs = fn_Time_AsSeconds(reinterpret_cast<void*>(seasonMgr + 8));
        monthDbg = static_cast<int>((secs / *s_fYearSeconds) * *s_fYearMonths);
        if (static_cast<uint32_t>(monthDbg - 6) <= 5u) { harvestNow = true; seasonArm = true; }
    }
    const float maturity = CF_Maturity(comp);
    if (!harvestNow && maturity > *s_fRipe) harvestNow = true;

    // State 10 is the ONLY path to state 11, and it has two independent arms. If the
    // field never harvests, the arms fail for different reasons and need different
    // fixes: a maturity that never climbs means the growth tick (slot 15 msg 1) is not
    // advancing the crops, whereas a maturity that climbs while the season arm stays
    // shut means the year window is the blocker. Print both.
    {
        static int s_ht = -1;
        if (s_ht < 0) {
            const char* he = std::getenv("BANISHED_CROPFIELD_TRACE");
            s_ht = (he && he[0] && he[0] != '0') ? 1 : 0;
        }
        static int s_n2 = 0;
        if (s_ht && (s_n2++ % 600) == 0) {
            // Carry the live tile count into state 10 as well. It reaches ~56-64 by the
            // end of state 9 and is 0 by the time HarvestArea runs, so the crops are
            // lost DURING TEND. Printing it alongside maturity says whether they vanish
            // gradually (something consuming them) or all at once (a sweep).
            const uintptr_t gwx2 = reinterpret_cast<uintptr_t>(*g_pGameWorldSingleton);
            const uintptr_t mp2 = gwx2 ? *reinterpret_cast<uintptr_t*>(gwx2 + 0xf8) : 0;
            const uintptr_t gd2 = mp2 ? *reinterpret_cast<uintptr_t*>(mp2 + 0x80) : 0;
            const int tot2 = SlotCount(comp);
            int live2 = 0;
            if (gd2) for (int i = 0; i < tot2; ++i) {
                uint8_t* sl = SlotAt(comp, i);
                if (!sl) continue;
                const int c = *reinterpret_cast<int32_t*>(sl + 0x00);
                if (*reinterpret_cast<uintptr_t*>(gd2 + 8 + (uintptr_t)c * 0x30)) ++live2;
            }
            std::fprintf(stderr, "[CropHarvest] comp=%p entity=%p temp=%.1f (min %.1f) month=%d seasonArm=%d "
                         "maturity=%.4f (ripe>%.4f) -> harvest=%d | tiles=%d/%d\n",
                         (void*)comp, (void*)*reinterpret_cast<uintptr_t*>(comp + 0x10),
                         temp, *s_fTempMin, monthDbg, (int)seasonArm,
                         maturity, *s_fRipe, (int)harvestNow, live2, tot2);
            std::fflush(stderr);
        }
    }

    if (harvestNow) CropField_Event4Harvest(self, sm);
    return 1;
}

// ── state 11 enter — 0x140119e60 — post a HarvestArea ─────────────────────────────
// HarvestArea is 0x68 with a per-slot byte array and no desc field (vtable 0x1401614a0).
static char CropField_State11Enter(void* self, void* /*sm*/) {
    const uintptr_t comp = reinterpret_cast<uintptr_t>(self);
    std::fprintf(stderr, "[CropXition] STATE 11 ENTER - posting HarvestArea\n");
    std::fflush(stderr);
    void* work = CF_MakeAreaWork(comp, 0x68, 0x1401614a0, /*perSlotArray=*/true, 0, false);
    CF_PostWork(comp, work);
    return 1;
}

// ── state 11 per-frame — 0x140119d80 ──────────────────────────────────────────────
// When every slot reads done, release the work slot and go to 21.
static char CropField_State11PerFrame(void* self, void* sm, void* /*time*/) {
    CF_Resolve2();
    const uintptr_t comp = reinterpret_cast<uintptr_t>(self);
    if (s_slotRefresh) s_slotRefresh(comp);
    if (CountSlots(comp, 2) == SlotCount(comp)) {
        CF_ReleaseWorkSlot(comp);
        ::ControllerLive::RequestStateTransition(sm, 21);
    }
    return 1;
}

// Replica of the populator at 0x14011b150, in registration order. Order matters: it
// fixes the state INDEX that StateMachine::Tick carries in its +0x09/+0x0a/+0x0b bytes.
extern "C" void CropFieldCtrl_PopulateTable() {
    using namespace ::ControllerLive;
    StateTable& t = CropFieldDispatch::table;
    if (t.count != 0) return;                       // idempotent — runs per instance

    // ── state 2 — DEFAULT / idle ──────────────────────────────────────────────────
    // Slot 16 (GetDefaultState) is the shared 0x1400026c0 = `MOV EAX,2; RET`, so this
    // is where a new field starts. enter 0x14011a920 / trans38 0x14011a860; the event-3
    // body 0x14011a7b0 is the panel's plant command, mirroring the orchard's event 3.
    TableRegisterState(t, 2, reinterpret_cast<void*>(&CropField_State2Enter), nullptr,
                       reinterpret_cast<void*>(&CropField_State2PerFrame), nullptr);
    TableAddTransition(t, 3,  nullptr);
    TableAddTransition(t, 9,  nullptr);
    TableAddEvent(t, 3, reinterpret_cast<void*>(&CropField_Event3Plant));

    // ── state 9 — PLANT ───────────────────────────────────────────────────────────
    // enter 0x14011a310 / trans38 0x14011a1e0.
    TableRegisterState(t, 9, reinterpret_cast<void*>(&CropField_State9Enter), nullptr,
                       reinterpret_cast<void*>(&CropField_State9PerFrame), nullptr);
    TableAddTransition(t, 3,  nullptr);
    TableAddTransition(t, 10, nullptr);

    // ── state 10 — TEND / GROW ────────────────────────────────────────────────────
    // The only state with an UPDATE handler (0x140119f70) as well as a per-frame
    // trans38 (0x140119fd0); enter 0x14011a100. Event 4 (0x140119f00) is the harvest
    // command, and 21 is the clear/cut target.
    TableRegisterState(t, 10, reinterpret_cast<void*>(&CropField_State10Enter), nullptr,
                       reinterpret_cast<void*>(&CropField_State10PerFrame),
                       reinterpret_cast<void*>(&CropField_State10Update));
    TableAddTransition(t, 3,  nullptr);
    TableAddTransition(t, 21, nullptr);
    TableAddTransition(t, 11, nullptr);
    TableAddEvent(t, 4, reinterpret_cast<void*>(&CropField_Event4Harvest));

    // ── state 11 — HARVEST ────────────────────────────────────────────────────────
    // enter 0x140119e60 / trans38 0x140119d80.
    TableRegisterState(t, 11, reinterpret_cast<void*>(&CropField_State11Enter), nullptr,
                       reinterpret_cast<void*>(&CropField_State11PerFrame), nullptr);
    TableAddTransition(t, 3,  nullptr);
    TableAddTransition(t, 21, nullptr);
}
