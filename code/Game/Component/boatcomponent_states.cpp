// code/Game/Component/boatcomponent_states.cpp
// BoatComponent's OWN state machine — layer 4 of the trade wave, and the thing that
// makes the merchant boat actually sail.
//
// ── WHY THIS EXISTS ──────────────────────────────────────────────────────────────
// After layer 2/3 the trading post spawned a merchant, built it a 993-point route and
// parked in state 0x12 forever. The note left behind said "BoatComponent's own Update is
// the base no-op, so some other system drives it". It does not: BoatComponent drives
// ITSELF, through the same embedded-state-node mechanism TradeComponent uses. It was
// invisible because BoatComponent's per-frame handlers all live in the trans30 column,
// and nothing in our build had ever dispatched that column.
//
// The geometry is read off the vtable, not guessed. BoatComponent vtable 0x140158f40:
//   slot 16 = 0x140003730  (`XOR EAX,EAX; RET`)   => default state 0
//   slot 17 = 0x140003720  (`LEA RAX,[RCX+0x78]`) => node at +0x78, back-pointer +0x90
// and the ctor 0x140046800 writes node vtable 0x140158ef0 at +0x78 with +0x90 = this,
// exactly TradeComponent's shape one offset family over.
//
// ── THE TABLE (BoatDescription_Setup -> 0x1400468a0) ─────────────────────────────
// Built with BoatComponent's own three primitives — 0x1400039d0 register-state,
// 0x140003ae0 add-transition, 0x140003b90 add-event — into the table at 0x1401c9150
// (count 0x1401ca0e0). The argument mapping is read off 0x1400039d0's DISASSEMBLY,
// where the temporary StateEntry is assembled at RSP+0x30:
//     [RSP+0x30] = ECX  -> entry+0x00  id
//     [RSP+0x38] = RDX  -> entry+0x08  enter
//     [RSP+0x40] = R9   -> entry+0x10  trans30      <-- NOT R8
//     [RSP+0x48] = 0    -> entry+0x18  trans38      <-- ALWAYS NULL for this table
//     [RSP+0x50] = R8   -> entry+0x20  update
// which differs from TradeComponent's 0x14000dd70 (whose trans38 arrives on the stack).
// Getting this backwards silently swaps the arrival test with the exit predicate.
//
//   state 0  OUTBOUND — follow the route to the dock
//     enter   53f0  latch ModelComponent/CharacterComponent, snap to the nearest route
//                   point, face along it, play animation 1
//     update  5190  retail dead code (see the note on the port) — kept faithful
//     trans30 5250  per-frame: steer (63d0); if the destination died, delete the boat
//                   and post event 0xb to the post; on arrival -> state 1
//     event 1 5670  -> state 4
//   state 1  FINAL APPROACH — drive straight at the dock position
//     enter   5150  play animation 2
//     trans30 4ff0  per-frame: move toward +0x40; within range -> state 2
//     event 1 5670  -> state 4
//   state 2  DOCKED
//     enter   4f70  stop the mover AND ***POST EVENT 9 TO THE TRADING POST*** — this
//                   single call is what unblocks layer 3
//     event 0 4f00  -> state 3
//     event 1 5670  -> state 4
//   state 3  DEPARTING — retrace the route outward
//     enter   4ec0  play animation 3
//     trans30 4d10  per-frame: steer toward +0x50; when clear of the dock -> state 4
//   state 4  GONE
//     enter   4cd0  play animation 1
//     trans30 4c40  per-frame: once 63d0 runs out of route, delete the boat entity
//
// ── WHAT ACTUALLY MOVES THE HULL ─────────────────────────────────────────────────
// Nothing here does. These handlers only write STEERING into the boat's own
// CharacterComponent (+0x38): +0x28 moving flag, +0x2c..+0x34 direction, +0x38 speed,
// +0x3c heading. The CharacterComponent move tick (already live) integrates it. That is
// why the boat looked inert from every angle: the mover was live, the route was built,
// and the one thing missing was the state machine that points the mover at it.
//
// boat+0x30 = ModelComponent   (finder 0x140003f60, type-id global 0x1401c7a50)
// boat+0x38 = CharacterComponent (finder 0x1400012d0, type-id global 0x1401c7078)
// boat+0x40 = the dock position   boat+0x50 = the CURRENT waypoint
// boat+0x60/+0x68/+0x6c = route array/count/capacity   boat+0x70 = waypoint index
//
// GATED: BANISHED_LIVE_BOAT_CTRL, opt-in and NOT in kDefaultOnFlags. Unproven.

#include "Component/controller_state_table.h"
#include "Component/component_factory_base.h"
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace { struct BoatCtrlTag {}; }
using BoatDispatch = ::ControllerLive::Dispatcher<BoatCtrlTag>;

extern "C" void** BoatCtrl_NodeVtable() { return BoatDispatch::Vtable(); }

namespace {

// ── Resolved leaves ──────────────────────────────────────────────────────────────
typedef void*    (*fn_Find_t)(int64_t entity, void* outIBP);
typedef void     (*fn_CheckPtr_t)(void* ibp);
typedef void     (*fn_V3Ctor3f_t)(void* out, float x, float y, float z);
typedef void     (*fn_V3CtorS_t)(void* out, const float* x, const float* y, const float* z);
typedef void     (*fn_V3Length_t)(const void* v, float* out);
typedef void     (*fn_V3Dot_t)(const void* a, float* out, const void* b);
// Value-return member ABI: RCX=this, RDX=hidden 12-byte retbuf, retbuf also in RAX.
// Passing only RCX leaves RDX holding garbage and the callee writes a Vector3 through
// it — see the same note in action_base_move.cpp.
typedef float*   (*fn_V3Normalize_t)(const void* v, void* retbuf);
typedef void     (*fn_V3NormalizeLen_t)(const void* v, void* out, float* outLen);
typedef float    (*fn_V3Heading_t)(const void* v);
typedef void     (*fn_TimeCtor_t)(void* outTime, float seconds);
typedef void     (*fn_SMTick_t)(void* sm, void* time, char useAlt, char run);
typedef void     (*fn_MarkRemoval_t)(int64_t entity);
typedef void*    (*fn_ArrResize_t)(void* arr, int newCount, void* fill);
// Static value-return: RCX=retbuf, RDX=&yaw, R8=&pitch, R9=&offset (0x1400455db).
typedef void*    (*fn_XformFromYPO_t)(void* out, const float* yaw, const float* pitch,
                                      const void* offset);
typedef void*    (*fn_XformAssign_t)(void* dst, const void* src);
typedef void     (*fn_GeomSetXform_t)(void* geom, const void* xform);

fn_Find_t           s_FindModel      = nullptr;   // 0x140003f60
fn_Find_t           s_FindCharacter  = nullptr;   // 0x1400012d0
fn_Find_t           s_FindSMC        = nullptr;   // 0x140003ea0
fn_CheckPtr_t       s_CheckPtr       = nullptr;   // slot 0x140152ab0
fn_V3Ctor3f_t       s_V3Ctor3f       = nullptr;   // slot 0x140152a30
fn_V3CtorS_t        s_V3CtorS        = nullptr;   // slot 0x140151268
fn_V3Length_t       s_V3Length       = nullptr;   // slot 0x140151290
fn_V3Dot_t          s_V3Dot          = nullptr;   // slot 0x140151298
fn_V3Normalize_t    s_V3Normalize    = nullptr;   // slot 0x1401512c0
fn_V3NormalizeLen_t s_V3NormalizeLen = nullptr;   // slot 0x1401512b8
fn_V3Heading_t      s_V3Heading      = nullptr;   // slot 0x1401512d8
fn_TimeCtor_t       s_TimeCtor       = nullptr;   // slot 0x140152b00
fn_SMTick_t         s_SMTick         = nullptr;   // 0x1400a9660
fn_MarkRemoval_t    s_MarkRemoval    = nullptr;   // 0x1400c2ea0
fn_ArrResize_t      s_ArrResize      = nullptr;   // 0x140003110
fn_XformFromYPO_t   s_XformFromYPO   = nullptr;   // slot 0x140151288
fn_XformAssign_t    s_XformAssign    = nullptr;   // slot 0x140151280
fn_GeomSetXform_t   s_GeomSetXform   = nullptr;   // slot 0x1401512e0

// Float constants, by address (retail reads them from .rdata, so do we).
const float* s_fArrive      = nullptr;   // 0x140172180  waypoint arrival radius
const float* s_fPointSpace  = nullptr;   // 0x14017219c  min spacing to the NEXT waypoint
const float* s_fSpeedBase   = nullptr;   // 0x14017214c  ( == g_fUVScale, 1.0f )
const float* s_fSpeedScale  = nullptr;   // 0x1401720ec
const float* s_fSpeedMin    = nullptr;   // 0x14017210c
const float* s_fDockRange   = nullptr;   // 0x1401720fc  state 1 -> 2 range
const float* s_fApproach    = nullptr;   // 0x1401721f4  state 0 -> 1 range (squared)

// The boat's animation ids, as retail passes them to 0x14010a1f0.
// (0x14010a1f0 is already ported as ActMv_SetAnimation_a1f0 in action_base_move.cpp.)
bool s_resolved = false;

inline void* Slot(uintptr_t addr) {
    uintptr_t s = ResolveAddr(Module::Exe, addr);
    return s ? *reinterpret_cast<void**>(s) : nullptr;
}
inline const float* Flt(uintptr_t addr) {
    return reinterpret_cast<const float*>(ResolveAddr(Module::Exe, addr));
}
// Tiny per-slot formatter so one fprintf can print seven "value or UNRESOLVED" fields.
inline const char* FmtF(int slot, float v) {
    static char buf[7][24];
    std::snprintf(buf[slot], sizeof(buf[slot]), "%.3f", v);
    return buf[slot];
}

void Resolve() {
    if (s_resolved) return;
    s_resolved = true;

    s_FindModel      = reinterpret_cast<fn_Find_t>(ResolveAddr(Module::Exe, 0x140003f60));
    s_FindCharacter  = reinterpret_cast<fn_Find_t>(ResolveAddr(Module::Exe, 0x1400012d0));
    s_FindSMC        = reinterpret_cast<fn_Find_t>(ResolveAddr(Module::Exe, 0x140003ea0));
    s_SMTick         = reinterpret_cast<fn_SMTick_t>(ResolveAddr(Module::Exe, 0x1400a9660));
    s_MarkRemoval    = reinterpret_cast<fn_MarkRemoval_t>(ResolveAddr(Module::Exe, 0x1400c2ea0));
    s_ArrResize      = reinterpret_cast<fn_ArrResize_t>(ResolveAddr(Module::Exe, 0x140003110));

    s_CheckPtr       = reinterpret_cast<fn_CheckPtr_t>(Slot(0x140152ab0));
    s_V3Ctor3f       = reinterpret_cast<fn_V3Ctor3f_t>(Slot(0x140152a30));
    s_V3CtorS        = reinterpret_cast<fn_V3CtorS_t>(Slot(0x140151268));
    s_V3Length       = reinterpret_cast<fn_V3Length_t>(Slot(0x140151290));
    s_V3Dot          = reinterpret_cast<fn_V3Dot_t>(Slot(0x140151298));
    s_V3Normalize    = reinterpret_cast<fn_V3Normalize_t>(Slot(0x1401512c0));
    s_V3NormalizeLen = reinterpret_cast<fn_V3NormalizeLen_t>(Slot(0x1401512b8));
    s_V3Heading      = reinterpret_cast<fn_V3Heading_t>(Slot(0x1401512d8));
    s_TimeCtor       = reinterpret_cast<fn_TimeCtor_t>(Slot(0x140152b00));
    s_XformFromYPO   = reinterpret_cast<fn_XformFromYPO_t>(Slot(0x140151288));
    s_XformAssign    = reinterpret_cast<fn_XformAssign_t>(Slot(0x140151280));
    s_GeomSetXform   = reinterpret_cast<fn_GeomSetXform_t>(Slot(0x1401512e0));

    s_fArrive     = Flt(0x140172180);
    s_fPointSpace = Flt(0x14017219c);
    s_fSpeedBase  = Flt(0x14017214c);
    s_fSpeedScale = Flt(0x1401720ec);
    s_fSpeedMin   = Flt(0x14017210c);
    s_fDockRange  = Flt(0x1401720fc);
    s_fApproach   = Flt(0x1401721f4);

    std::fprintf(stderr,
        "[BoatCtrl] leaves: findModel=%p findChar=%p findSMC=%p len=%p normLen=%p "
        "norm=%p heading=%p v3ctor=%p tick=%p mark=%p\n",
        (void*)s_FindModel, (void*)s_FindCharacter, (void*)s_FindSMC,
        (void*)s_V3Length, (void*)s_V3NormalizeLen, (void*)s_V3Normalize,
        (void*)s_V3Heading, (void*)s_V3Ctor3f, (void*)s_SMTick, (void*)s_MarkRemoval);

    // ⚠ REPORT THE OUTCOME, NOT THE INTENT. Every use of these constants is guarded
    // `if (s_fX && ...)`, so an unresolved one does not crash and does not log — it
    // DELETES the test it guards. That is exactly how the first run of this file sailed a
    // boat past its own dock: 0x1401721f4 was never registered, so the arrival test was
    // simply not there, and the boat ran the full 987-point route and scrapped itself.
    {
        const bool ok = s_fArrive && s_fPointSpace && s_fSpeedBase && s_fSpeedScale &&
                        s_fSpeedMin && s_fDockRange && s_fApproach;
        std::fprintf(stderr,
            "[BoatCtrl] consts: arrive(172180)=%s pointSpace(17219c)=%s "
            "speedBase(17214c)=%s speedScale(1720ec)=%s speedMin(17210c)=%s "
            "dockRange(1720fc)=%s approachSq(1721f4)=%s%s\n",
            s_fArrive     ? FmtF(0, *s_fArrive)     : "UNRESOLVED",
            s_fPointSpace ? FmtF(1, *s_fPointSpace) : "UNRESOLVED",
            s_fSpeedBase  ? FmtF(2, *s_fSpeedBase)  : "UNRESOLVED",
            s_fSpeedScale ? FmtF(3, *s_fSpeedScale) : "UNRESOLVED",
            s_fSpeedMin   ? FmtF(4, *s_fSpeedMin)   : "UNRESOLVED",
            s_fDockRange  ? FmtF(5, *s_fDockRange)  : "UNRESOLVED",
            s_fApproach   ? FmtF(6, *s_fApproach)   : "UNRESOLVED",
            ok ? "" : "   <-- an UNRESOLVED constant silently removes the test it guards; "
                      "the boat will sail its whole route and never dock");
    }
    std::fflush(stderr);
}

bool TraceEnabled() {
    static int s = -1;
    if (s < 0) {
        const char* e = std::getenv("BANISHED_BOAT_TRACE");
        s = (e && e[0] && std::strcmp(e, "0") != 0) ? 1 : 0;
    }
    return s == 1;
}

// ── Small shared shapes ──────────────────────────────────────────────────────────

struct Vec3 { float x, y, z; };

// Defined after the 0x1400a70c0 port below; Steer_63d0 needs it first.
int NextRoutePoint(uintptr_t boat, const Vec3* ref);

inline const Vec3* ModelPos(uintptr_t boat) {
    if (s_CheckPtr) s_CheckPtr(reinterpret_cast<void*>(boat + 0x30));
    const uintptr_t model = *reinterpret_cast<uintptr_t*>(boat + 0x30);
    if (!model) return nullptr;                     // see the guard note in Steer()
    return reinterpret_cast<const Vec3*>(model + 0x158);
}

// The request-transition idiom every boat handler ends with, byte-for-byte:
//     if (sm[9] < 0) { Time t(0.0f); StateMachine::Tick(sm, &t, 0, 0); }
//     i = node->slot08(node, (int8)sm[9], &desired);
//     if (i >= 0) { sm[0xb] = i; sm[0xa] = (uint8)desired; }
// ControllerLive::RequestStateTransition already models the second half; the nested
// flush in the first half is retail's way of forcing a valid current index before
// asking, and it matters here because a boat handler can fire on the very tick the
// machine entered its state.
void RequestTransition(void* smV, int desiredStateId) {
    uint8_t* sm = reinterpret_cast<uint8_t*>(smV);
    if (!sm) return;
    if (static_cast<int8_t>(sm[9]) < 0 && s_SMTick && s_TimeCtor) {
        uint8_t zeroTime[8] = {};
        s_TimeCtor(zeroTime, 0.0f);
        s_SMTick(smV, zeroTime, 0, 0);
    }
    ::ControllerLive::RequestStateTransition(smV, desiredStateId);
}

// Post an event to the DESTINATION entity's StateMachineComponent — the boat's only
// channel back to the trading post. Retail (0x140044fda, 0x140045311):
//     smc  = FindComponent<StateMachineComponent>(*(boat+0x28))
//     node = *(smc+0x48)                       ; the SM's current node
//     node->slot2(node, sm = smc+0x48, curIdx = (int8)smc[0x51], eventId)
// smc+0x48 IS the StateMachine struct (its +0x00 is the node), and smc+0x51 is that
// machine's current node-local index — the same +0x09 byte the node ABI uses one field
// earlier in the struct.
void PostEventToDestination(uintptr_t boat, int eventId) {
    if (!s_FindSMC) return;
    if (s_CheckPtr) s_CheckPtr(reinterpret_cast<void*>(boat + 0x28));
    const int64_t destEntity = *reinterpret_cast<int64_t*>(boat + 0x28);
    if (!destEntity) return;

    uint64_t smcIBP[2] = {0, 0};
    void* out = s_FindSMC(destEntity, smcIBP);
    if (s_CheckPtr) s_CheckPtr(out);
    const uintptr_t smc = out ? *reinterpret_cast<uintptr_t*>(out) : 0;
    if (!smc) {
        if (TraceEnabled())
            std::fprintf(stderr, "[BoatCtrl] event %d NOT posted: destination entity %p has "
                                 "no StateMachineComponent\n", eventId, (void*)destEntity);
        return;
    }

    void* sm   = reinterpret_cast<void*>(smc + 0x48);
    void* node = *reinterpret_cast<void**>(smc + 0x48);
    if (!node) return;
    void** vt = *reinterpret_cast<void***>(node);
    if (!vt || !vt[2]) return;
    const int curIdx = *reinterpret_cast<int8_t*>(smc + 0x51);
    typedef char (*PostEvent_t)(void*, void*, int, int);
    const char taken = reinterpret_cast<PostEvent_t>(vt[2])(node, sm, curIdx, eventId);
    if (TraceEnabled())
        std::fprintf(stderr, "[BoatCtrl] posted event %d to destination smc=%p curIdx=%d "
                             "-> handled=%d\n", eventId, (void*)smc, curIdx, (int)taken);
}

// Retail's "the boat is finished — delete it" tail, shared verbatim by 5250 and 4c40
// (0x140045272.. and 0x140044c52..): mark the boat's own entity for removal and push it
// onto the entity list's removal array at gw+0xE8 -> +0xd0/+0xd8.
void DeleteBoatEntity(uintptr_t boat) {
    int64_t entIBP = *reinterpret_cast<int64_t*>(boat + 0x10);
    if (!entIBP) return;
    void* gw = (g_pGameWorldSingleton && *g_pGameWorldSingleton) ? *g_pGameWorldSingleton : nullptr;
    if (!gw) return;
    const uintptr_t entityList = *reinterpret_cast<uintptr_t*>(
        reinterpret_cast<uintptr_t>(gw) + 0xe8);
    if (!entityList) return;

    if (s_CheckPtr) s_CheckPtr(&entIBP);
    if ((*reinterpret_cast<uint32_t*>(entIBP + 0x40) >> 10) & 1) return;   // already going
    if (s_CheckPtr) s_CheckPtr(&entIBP);
    if (s_MarkRemoval) s_MarkRemoval(entIBP);
    if (s_ArrResize) {
        void* arr = reinterpret_cast<void*>(entityList + 0xd0);
        s_ArrResize(arr, *reinterpret_cast<int32_t*>(entityList + 0xd8) + 1, &entIBP);
    }
    if (TraceEnabled()) {
        std::fprintf(stderr, "[BoatCtrl] boat entity marked for removal\n");
        std::fflush(stderr);
    }
}

// ── 0x1400463d0 — the steering core ──────────────────────────────────────────────
// Ported from DISASSEMBLY. Ghidra's decompile of this one loses the XMM plumbing badly
// (it renders the model position as three separate half-live locals and drops the
// hidden out-params of Length/Normalize entirely).
//
//   v = boat.waypoint(+0x50) - model.pos, FLATTENED to z = 0
//   if (|v| < 0x140172180) {                       // reached the current waypoint
//       i = NextRoutePoint(boat+0x50, model.pos, 0x14017219c)
//       if (i < 0) return 0;                        // route exhausted -> caller deletes
//       boat+0x70 = i;  boat+0x50 = route[i] (z = 0)
//   }
//   dir = Normalize(boat.waypoint - model.pos, &len)
//   character+0x28 = 0                              // NOT under mover control
//   character+0x2c..0x34 = Normalize(dir.xy, 0)
//   character+0x3c = Heading(dir)
//   return 1
//
// ⚠ +0x28 = 0 here but 0x14010a0e0 (used by state 1) writes 1. That is not a
// transcription slip: state 0 steers by direction only, state 1 hands the mover an
// explicit target and speed.
char Steer_63d0(uintptr_t boat) {
    const Vec3* mp = ModelPos(boat);
    if (!mp) return 0;
    const Vec3 modelPos = *mp;
    const Vec3* wp = reinterpret_cast<const Vec3*>(boat + 0x50);

    char ok = 1;
    {
        Vec3 v = {0, 0, 0};
        if (s_V3Ctor3f)
            s_V3Ctor3f(&v, wp->x - modelPos.x, wp->y - modelPos.y, wp->z - modelPos.z);
        v.z = 0.0f;                                   // the flatten at 0x140046473
        float len = 0.0f;
        if (s_V3Length) s_V3Length(&v, &len);
        if (s_fArrive && len < *s_fArrive) {
            const int idx = NextRoutePoint(boat, &modelPos);
            if (idx < 0) return 0;
            *reinterpret_cast<int32_t*>(boat + 0x70) = idx;
            const float* pt = *reinterpret_cast<const float**>(boat + 0x60) + (size_t)idx * 3;
            const float px = pt[0], py = pt[1], pz = 0.0f;
            if (s_V3CtorS) s_V3CtorS(reinterpret_cast<void*>(boat + 0x50), &px, &py, &pz);
        }
    }

    // dir = Normalize(waypoint - modelPos), with the length dropped on the floor here
    // (retail passes an out-param it does not read in this function).
    Vec3 toWp = { wp->x - modelPos.x, wp->y - modelPos.y, wp->z - modelPos.z };
    Vec3 dir = {0, 0, 0};
    float dropLen = 0.0f;
    if (s_V3NormalizeLen) s_V3NormalizeLen(&toWp, &dir, &dropLen);

    if (s_CheckPtr) s_CheckPtr(reinterpret_cast<void*>(boat + 0x38));
    const uintptr_t chr = *reinterpret_cast<uintptr_t*>(boat + 0x38);
    if (!chr) return ok;
    *reinterpret_cast<uint8_t*>(chr + 0x28) = 0;

    Vec3 flat = { dir.x, dir.y, 0.0f };
    Vec3 nrm = {0, 0, 0};
    if (s_V3Normalize && s_V3Normalize(&flat, &nrm)) {
        *reinterpret_cast<float*>(chr + 0x2c) = nrm.x;
        *reinterpret_cast<float*>(chr + 0x30) = nrm.y;
        *reinterpret_cast<float*>(chr + 0x34) = nrm.z;
    }
    if (s_V3Heading) *reinterpret_cast<float*>(chr + 0x3c) = s_V3Heading(&dir);
    return ok;
}

}  // namespace

// ── 0x1400a70c0 — the next route point at least `spacing` away ───────────────────
// Walks FORWARD from the current index (boat+0x70) through the route array
// (base +0x60, count +0x68) and returns the first index whose point is at least
// `spacing` from `ref`; -1 once the route runs out. The caller treats -1 as "the boat
// has finished this leg" — in state 4 that is what deletes it.
//
// Note the argument: retail passes boat+0x50, i.e. the routine reads count/base/index
// at +0x18/+0x10/+0x20 RELATIVE TO THE WAYPOINT FIELD, which lands on the boat's
// +0x68/+0x60/+0x70. Same three fields, addressed from a different anchor.
extern "C" int BoatCtrl_NextRoutePoint(void* waypointField, const float* ref, float spacing) {
    const uintptr_t w = reinterpret_cast<uintptr_t>(waypointField);
    const int   count = *reinterpret_cast<const int32_t*>(w + 0x18);
    const float* base = *reinterpret_cast<const float* const*>(w + 0x10);
    int i = *reinterpret_cast<const int32_t*>(w + 0x20);
    if (!base) return -1;
    const float r2 = spacing * spacing;
    for (;;) {
        ++i;
        if (i >= count) return -1;
        const float dx = ref[0] - base[(size_t)i * 3 + 0];
        const float dy = ref[1] - base[(size_t)i * 3 + 1];
        const float dz = ref[2] - 0.0f;
        if (dx * dx + dy * dy + dz * dz >= r2) return i;
    }
}

namespace {
int NextRoutePoint(uintptr_t boat, const Vec3* ref) {
    const float spacing = s_fPointSpace ? *s_fPointSpace : 0.0f;
    return BoatCtrl_NextRoutePoint(reinterpret_cast<void*>(boat + 0x50),
                                   reinterpret_cast<const float*>(ref), spacing);
}
}  // namespace

// ── 0x14010a0e0 — point the mover at a world target ──────────────────────────────
//     character+0x28 = 1
//     dir = Normalize(target - character.model(+0x68).pos)
//     character+0x2c..0x34 = dir
//     character+0x3c = heading      (passed in — retail hands it boat+0x4c)
//     character+0x38 = character.model(+0x13c) * speed
extern "C" void BoatCtrl_DriveToward(void* charComp, const float* target,
                                     float heading, float speed) {
    Resolve();
    const uintptr_t chr = reinterpret_cast<uintptr_t>(charComp);
    if (!chr) return;
    *reinterpret_cast<uint8_t*>(chr + 0x28) = 1;

    if (s_CheckPtr) s_CheckPtr(reinterpret_cast<void*>(chr + 0x68));
    const uintptr_t model = *reinterpret_cast<uintptr_t*>(chr + 0x68);
    if (!model) return;
    const float* mp = reinterpret_cast<const float*>(model + 0x158);

    Vec3 v = { target[0] - mp[0], target[1] - mp[1], target[2] - mp[2] };
    Vec3 nrm = {0, 0, 0};
    if (s_V3Normalize && s_V3Normalize(&v, &nrm)) {
        *reinterpret_cast<float*>(chr + 0x2c) = nrm.x;
        *reinterpret_cast<float*>(chr + 0x30) = nrm.y;
        *reinterpret_cast<float*>(chr + 0x34) = nrm.z;
    }
    if (s_CheckPtr) s_CheckPtr(reinterpret_cast<void*>(chr + 0x68));
    const uintptr_t model2 = *reinterpret_cast<uintptr_t*>(chr + 0x68);
    *reinterpret_cast<float*>(chr + 0x3c) = heading;
    *reinterpret_cast<float*>(chr + 0x38) =
        model2 ? *reinterpret_cast<const float*>(model2 + 0x13c) * speed : 0.0f;
}

// 0x14010a1f0 SetAnimation, already ported for the citizen mover.
extern void ActMv_SetAnimation_a1f0(void* selfp, uint32_t animId, float p3, float p4);

namespace {
void SetAnim(uintptr_t boat, uint32_t animId) {
    if (s_CheckPtr) s_CheckPtr(reinterpret_cast<void*>(boat + 0x38));
    void* chr = *reinterpret_cast<void**>(boat + 0x38);
    if (!chr) return;
    const float f = s_fSpeedBase ? *s_fSpeedBase : 1.0f;
    ActMv_SetAnimation_a1f0(chr, animId, f, f);
}
}  // namespace

// ============================================================
// state 0 ENTER — 0x1400453f0
// ============================================================
// Latches the two sibling components the rest of the machine steers through, snaps the
// current waypoint to the first route point far enough away, faces the hull along the
// route and starts animation 1.
extern "C" char BoatCtrl_State0_Enter(void* selfV) {
    Resolve();
    const uintptr_t boat = reinterpret_cast<uintptr_t>(selfV);

    uint64_t ibp[2] = {0, 0};
    if (s_CheckPtr) s_CheckPtr(reinterpret_cast<void*>(boat + 0x10));
    if (s_FindModel) {
        void* r = s_FindModel(*reinterpret_cast<int64_t*>(boat + 0x10), ibp);
        *reinterpret_cast<uint64_t*>(boat + 0x30) = *reinterpret_cast<uint64_t*>(r);
    }
    if (s_CheckPtr) s_CheckPtr(reinterpret_cast<void*>(boat + 0x10));
    if (s_FindCharacter) {
        void* r = s_FindCharacter(*reinterpret_cast<int64_t*>(boat + 0x10), ibp);
        *reinterpret_cast<uint64_t*>(boat + 0x38) = *reinterpret_cast<uint64_t*>(r);
    }

    const float* route = *reinterpret_cast<const float**>(boat + 0x60);
    if (!route) {
        // Retail derefs this unguarded — it relies on the route builder having run. If we
        // are here with no route the fault is upstream (layer 2), and a silent no-op would
        // hide it, so say so once and bail rather than fault.
        std::fprintf(stderr, "[BoatCtrl] state0 enter with NO ROUTE (boat+0x60 null) — "
                             "the route builder did not run for this merchant\n");
        std::fflush(stderr);
        return 1;
    }

    // Snap to the nearest usable route point when we are already close to route[0].
    {
        const Vec3 p0 = { route[0], route[1], 0.0f };
        Vec3 v = {0, 0, 0};
        const Vec3* wp = reinterpret_cast<const Vec3*>(boat + 0x50);
        if (s_V3Ctor3f) s_V3Ctor3f(&v, wp->x - p0.x, wp->y - p0.y, wp->z - 0.0f);
        float len = 0.0f;
        if (s_V3Length) s_V3Length(&v, &len);
        if (s_fArrive && len < *s_fArrive) {
            const int idx = NextRoutePoint(boat, &p0);
            if (idx >= 0) {
                *reinterpret_cast<int32_t*>(boat + 0x70) = idx;
                const float* pt = route + (size_t)idx * 3;
                const float px = pt[0], py = pt[1], pz = 0.0f;
                if (s_V3CtorS) s_V3CtorS(reinterpret_cast<void*>(boat + 0x50), &px, &py, &pz);
            }
        }
    }

    // Face along (waypoint - route[0]) and push the transform into the model + geometry.
    {
        const Vec3* wp = reinterpret_cast<const Vec3*>(boat + 0x50);
        Vec3 dir = { wp->x - route[0], wp->y - route[1], wp->z - 0.0f };
        if (s_CheckPtr) s_CheckPtr(reinterpret_cast<void*>(boat + 0x30));
        const uintptr_t model = *reinterpret_cast<uintptr_t*>(boat + 0x30);
        if (model && s_V3Heading && s_XformFromYPO && s_XformAssign && s_GeomSetXform) {
            const float yaw   = s_V3Heading(&dir);
            const float pitch = 0.0f;
            // ⚠ The OFFSET is route[0], not the origin (R9 = [RBP-0x21] at 0x1400455db,
            // the Vector3 built from route[0] at 0x140045595). This transform is what
            // puts the newly spawned hull at the start of its route; passing a zero
            // offset would park every merchant at the map origin.
            const Vec3 offset = { route[0], route[1], 0.0f };
            uint8_t xform[0x30] = {};
            s_XformFromYPO(xform, &yaw, &pitch, &offset);
            s_XformAssign(reinterpret_cast<void*>(model + 0x148), xform);
            *reinterpret_cast<float*>(model + 0x168) = yaw;
            s_GeomSetXform(reinterpret_cast<void*>(model + 0x28),
                           reinterpret_cast<const void*>(model + 0x148));
        }
    }

    // ── ROUTE WATER AUDIT (trace-only, once per boat) ────────────────────────────
    // Settles by measurement the question a screenshot cannot: is the route the boat is
    // about to sail actually navigable water end to end, or does the A* leg cut across
    // land between two unconnected channels? Samples the terrain class of every route
    // point's cell and reports non-water counts PER SEGMENT, because the segments have
    // very different expectations — the approach/departure polylines run OFF-MAP (the
    // route builder's own entry/exit points are outside the grid) and are skipped, while
    // both A* legs must be 100% water or the pathfinder and the renderer disagree.
    // Water mask 0x380 = the boat pathfinder's own terrain class set.
    if (TraceEnabled()) {
        void* gwv = (g_pGameWorldSingleton && *g_pGameWorldSingleton) ? *g_pGameWorldSingleton
                                                                      : nullptr;
        uint8_t* mapObj = gwv ? *reinterpret_cast<uint8_t**>(
                                    reinterpret_cast<uint8_t*>(gwv) + 0xf8) : nullptr;
        uint8_t* grid   = mapObj ? *reinterpret_cast<uint8_t**>(mapObj + 0x80) : nullptr;
        const int width = mapObj ? *reinterpret_cast<int32_t*>(mapObj) : 0;
        const int n     = *reinterpret_cast<int32_t*>(boat + 0x68);
        if (grid && width > 0) {
            const int half = width / 2;
            const uint32_t mask = 0x380u;
            int offMap = 0, water = 0, land = 0, firstLand = -1;
            for (int i = 0; i < n; ++i) {
                const float* pt = route + (size_t)i * 3;
                const int cx = (int)(pt[0] + (float)half);
                const int cy = (int)(pt[1] + (float)half);
                if (cx < 0 || cy < 0 || cx >= width || cy >= width) { ++offMap; continue; }
                const uint8_t t = *(grid + (size_t)(cy * width + cx) * 0x30 + 0x29);
                if ((mask >> (t & 0x1f)) & 1) { ++water; }
                else { ++land; if (firstLand < 0) firstLand = i; }
            }
            std::fprintf(stderr,
                "[BoatCtrl] route audit: n=%d offMap=%d water=%d LAND=%d%s\n",
                n, offMap, water, land,
                land ? "   <-- the route crosses non-water cells; the boat will sail "
                       "over ground there" : "");
            if (firstLand >= 0) {
                const float* pt = route + (size_t)firstLand * 3;
                std::fprintf(stderr, "[BoatCtrl]   first land point idx=%d world=(%.2f,%.2f) "
                                     "tile=(%d,%d)\n", firstLand, pt[0], pt[1],
                             (int)(pt[0] + (float)half), (int)(pt[1] + (float)half));
            }
            std::fflush(stderr);
        }
    }

    SetAnim(boat, 1);
    if (TraceEnabled()) {
        std::fprintf(stderr, "[BoatCtrl] state 0 ENTER: model=%p char=%p route=%p n=%d "
                             "idx=%d wp=(%.2f,%.2f)\n",
                     *reinterpret_cast<void**>(boat + 0x30),
                     *reinterpret_cast<void**>(boat + 0x38),
                     (const void*)route, *reinterpret_cast<int32_t*>(boat + 0x68),
                     *reinterpret_cast<int32_t*>(boat + 0x70),
                     *reinterpret_cast<float*>(boat + 0x50),
                     *reinterpret_cast<float*>(boat + 0x54));
        std::fflush(stderr);
    }
    return 1;
}

// ============================================================
// state 0 UPDATE — 0x140045190
// ============================================================
// ⚠ THIS IS DEAD CODE IN RETAIL, AND IT IS PORTED THAT WAY DELIBERATELY.
// The disassembly builds its test vector from the waypoint MINUS ITSELF:
//     MOVSS XMM3,XMM1 / SUBSS XMM3,XMM1     -> x component = 0
//     MOVSS XMM0,XMM2 / SUBSS XMM0,XMM2     -> y component = 0
//     MOVSS [..],XMM6 (XORPS'd)             -> z component = 0
// so it takes Length((0,0,0)) and enters the re-pick branch only when that is < 0.
// It never is. Whatever the source line meant to subtract, the shipped binary does
// nothing here, and the real waypoint advance lives in 0x1400463d0 (Steer_63d0) which
// the per-frame trans30 handler calls. Reproducing the fold rather than "fixing" it
// keeps the port honest — and it matters, because a repaired version would advance the
// waypoint twice per frame.
extern "C" char BoatCtrl_State0_Update(void* selfV) {
    Resolve();
    const uintptr_t boat = reinterpret_cast<uintptr_t>(selfV);
    Vec3 zero = {0.0f, 0.0f, 0.0f};
    float len = 0.0f;
    if (s_V3Length) s_V3Length(&zero, &len);
    if (len < 0.0f) {                     // never taken; kept for shape
        const Vec3* wp = reinterpret_cast<const Vec3*>(boat + 0x50);
        const int idx = NextRoutePoint(boat, wp);
        if (idx >= 0) {
            *reinterpret_cast<int32_t*>(boat + 0x70) = idx;
            const float* pt = *reinterpret_cast<const float**>(boat + 0x60) + (size_t)idx * 3;
            *reinterpret_cast<float*>(boat + 0x50) = pt[0];
            *reinterpret_cast<float*>(boat + 0x54) = pt[1];
            *reinterpret_cast<float*>(boat + 0x58) = 0.0f;
        }
    }
    return 1;
}

// ============================================================
// state 0 TRANS30 (per-frame) — 0x140045250
// ============================================================
// Steer; if the route ran out, scrap the boat and tell the post its merchant is gone
// (event 0xb — the handler layer 1 already ported as TradeCtrl_Event0B_MerchantLost).
// Then, unconditionally, test arrival at the dock and hand over to state 1.
//
// The arrival test is Dot(d, d) — the SQUARED distance — against 0x1401721f4, which is
// therefore a squared radius. Retail calls Vector3::Dot with the same vector for both
// operands (RCX and R8 are the same address at 0x14004532c).
extern "C" char BoatCtrl_State0_Tick(void* selfV, void* smV, void* /*time*/) {
    Resolve();
    const uintptr_t boat = reinterpret_cast<uintptr_t>(selfV);

    if (!Steer_63d0(boat)) {
        DeleteBoatEntity(boat);
        PostEventToDestination(boat, 0xb);
    }

    const Vec3* mp = ModelPos(boat);
    if (!mp) return 1;
    const Vec3* dockPos = reinterpret_cast<const Vec3*>(boat + 0x40);
    Vec3 d = { mp->x - dockPos->x, mp->y - dockPos->y, mp->z - dockPos->z };
    float dist2 = 0.0f;
    if (s_V3Dot) s_V3Dot(&d, &dist2, &d);

    // Bring-up trace: reports OUTCOMES, not intent — the first call proves trans30 is
    // dispatched at all, and the periodic lines show whether the hull is actually moving
    // (model pos changing) as opposed to being steered at a target it never approaches.
    if (TraceEnabled()) {
        static int n = 0;
        if (n < 3 || (n % 240) == 0) {
            const Vec3* wp = reinterpret_cast<const Vec3*>(boat + 0x50);
            const uintptr_t chr = *reinterpret_cast<uintptr_t*>(boat + 0x38);
            std::fprintf(stderr, "[BoatCtrl] tick#%d model=(%.2f,%.2f) wp=(%.2f,%.2f) "
                                 "idx=%d dock=(%.2f,%.2f) dist2=%.1f chrMove=%d "
                                 "chrSpeed=%.3f\n",
                         n, mp->x, mp->y, wp->x, wp->y,
                         *reinterpret_cast<int32_t*>(boat + 0x70),
                         dockPos->x, dockPos->y, dist2,
                         chr ? (int)*reinterpret_cast<uint8_t*>(chr + 0x28) : -1,
                         chr ? *reinterpret_cast<float*>(chr + 0x38) : -1.0f);
            std::fflush(stderr);
        }
        ++n;
    }

    if (s_fApproach && *s_fApproach > dist2) {
        if (TraceEnabled()) {
            std::fprintf(stderr, "[BoatCtrl] state 0 -> 1 (dist^2=%.3f < %.3f)\n",
                         dist2, *s_fApproach);
            std::fflush(stderr);
        }
        RequestTransition(smV, 1);
    }
    return 1;
}

// ============================================================
// state 1 ENTER — 0x140045150
// ============================================================
extern "C" char BoatCtrl_State1_Enter(void* selfV) {
    Resolve();
    SetAnim(reinterpret_cast<uintptr_t>(selfV), 2);
    if (TraceEnabled()) { std::fprintf(stderr, "[BoatCtrl] state 1 ENTER\n"); std::fflush(stderr); }
    return 1;
}

// ============================================================
// state 1 TRANS30 (per-frame) — 0x140044ff0
// ============================================================
// Drive straight at the dock position and, when close enough, dock.
//
//   d      = boat+0x40 - model.pos
//   dir    = Normalize(d, &dist)
//   speed  = clamp(dist * 0x1401720ec, 0x14017210c, 0x14017214c)
//   DriveToward(character, boat+0x40, heading = boat+0x4c, speed)
//   if (0x1401720fc > dist) -> state 2
//
// The clamp is read off the branch pair at 0x14005008d/0x14005009a: `COMISS XMM6,max;
// JA take-max` then `COMISS min,XMM6; JBE keep`, i.e. high clamp to 0x14017214c and low
// clamp to 0x14017210c. Ghidra renders this as nonsense involving `DAT_1401720ec * 0.0`.
extern "C" char BoatCtrl_State1_Tick(void* selfV, void* smV, void* /*time*/) {
    Resolve();
    const uintptr_t boat = reinterpret_cast<uintptr_t>(selfV);

    const Vec3* mp = ModelPos(boat);
    if (!mp) return 1;
    const Vec3* dockPos = reinterpret_cast<const Vec3*>(boat + 0x40);
    Vec3 d = { dockPos->x - mp->x, dockPos->y - mp->y, dockPos->z - mp->z };
    Vec3 dir = {0, 0, 0};
    float dist = 0.0f;
    if (s_V3NormalizeLen) s_V3NormalizeLen(&d, &dir, &dist);

    float speed = dist * (s_fSpeedScale ? *s_fSpeedScale : 0.0f);
    const float hi = s_fSpeedBase ? *s_fSpeedBase : 1.0f;
    const float lo = s_fSpeedMin  ? *s_fSpeedMin  : 0.0f;
    // 0x14005008d: COMISS speed,hi / JA take-XMM0 (XMM0 == hi)
    // 0x14005009a: COMISS lo,speed  / JBE keep      (else XMM0 == lo)
    if (speed > hi) speed = hi;
    else if (lo > speed) speed = lo;

    if (s_CheckPtr) s_CheckPtr(reinterpret_cast<void*>(boat + 0x38));
    void* chr = *reinterpret_cast<void**>(boat + 0x38);
    if (chr)
        BoatCtrl_DriveToward(chr, reinterpret_cast<const float*>(boat + 0x40),
                             *reinterpret_cast<const float*>(boat + 0x4c), speed);

    if (s_fDockRange && *s_fDockRange > dist) {
        if (TraceEnabled()) {
            std::fprintf(stderr, "[BoatCtrl] state 1 -> 2 DOCKED (dist=%.3f < %.3f)\n",
                         dist, *s_fDockRange);
            std::fflush(stderr);
        }
        RequestTransition(smV, 2);
    }
    return 1;
}

// ============================================================
// state 2 ENTER — 0x140044f70   ***THE EVENT-9 CALL SITE***
// ============================================================
// Stop the mover (SetAnimation 0 at speed 0/0) and post event 9 to the trading post.
// This is the single instruction sequence the whole of layer 3 was waiting on: the trade
// machine's state 0x12 has no per-frame handler at all, only events 9 and 0xb, so until
// something raised 9 it could never reach 0x13 and run the manifest / buy-and-pay code.
extern "C" char BoatCtrl_State2_Enter(void* selfV) {
    Resolve();
    const uintptr_t boat = reinterpret_cast<uintptr_t>(selfV);

    // 0x140044f8d/f95: XMM2 = 0x14017214c, XMM3 = 0 (XORPS), EDX = 0.
    if (s_CheckPtr) s_CheckPtr(reinterpret_cast<void*>(boat + 0x38));
    if (void* chr = *reinterpret_cast<void**>(boat + 0x38))
        ActMv_SetAnimation_a1f0(chr, 0, s_fSpeedBase ? *s_fSpeedBase : 1.0f, 0.0f);

    PostEventToDestination(boat, 9);
    return 1;
}

// ============================================================
// state 2 EVENT 0 — 0x140044f00      (the post releases the merchant)
// ============================================================
extern "C" char BoatCtrl_State2_Event0(void* /*selfV*/, void* smV) {
    Resolve();
    RequestTransition(smV, 3);
    if (TraceEnabled()) { std::fprintf(stderr, "[BoatCtrl] event 0 -> state 3\n"); std::fflush(stderr); }
    return 1;
}

// ============================================================
// EVENT 1 on states 0/1/2 — 0x140045670   (abort: go straight to GONE)
// ============================================================
extern "C" char BoatCtrl_Event1_Abort(void* /*selfV*/, void* smV) {
    Resolve();
    RequestTransition(smV, 4);
    if (TraceEnabled()) { std::fprintf(stderr, "[BoatCtrl] event 1 -> state 4\n"); std::fflush(stderr); }
    return 1;
}

// ============================================================
// state 3 ENTER — 0x140044ec0
// ============================================================
extern "C" char BoatCtrl_State3_Enter(void* selfV) {
    Resolve();
    const uintptr_t boat = reinterpret_cast<uintptr_t>(selfV);
    if (s_CheckPtr) s_CheckPtr(reinterpret_cast<void*>(boat + 0x38));
    if (void* chr = *reinterpret_cast<void**>(boat + 0x38)) {
        const float f = s_fSpeedBase ? *s_fSpeedBase : 1.0f;
        ActMv_SetAnimation_a1f0(chr, 3, f, f);
    }
    if (TraceEnabled()) { std::fprintf(stderr, "[BoatCtrl] state 3 ENTER (departing)\n"); std::fflush(stderr); }
    return 1;
}

// ============================================================
// state 3 TRANS30 (per-frame) — 0x140044d10
// ============================================================
//   Length(boat+0x40 - boat+0x50, &distFromDockToWaypoint)   [out at RBP+0x20]
//   dir = Normalize(boat+0x50 - model.pos)
//   character+0x28 = 0
//   character+0x2c..0x34 = Normalize(dir.xy, 0)
//   character+0x3c = Heading(dir)
//   if (0x140172180 > distFromDockToWaypoint) -> state 4
//
// ⚠ The distance that gates the transition is measured between the DOCK POSITION and the
// CURRENT WAYPOINT, not between the boat and anything. That reads oddly until you note
// the departing leg re-walks the same route array: the waypoint marching away from the
// dock is exactly the progress signal.
extern "C" char BoatCtrl_State3_Tick(void* selfV, void* smV, void* /*time*/) {
    Resolve();
    const uintptr_t boat = reinterpret_cast<uintptr_t>(selfV);

    const Vec3* mp = ModelPos(boat);
    if (!mp) return 1;
    const Vec3 modelPos = *mp;
    const Vec3* dockPos = reinterpret_cast<const Vec3*>(boat + 0x40);
    const Vec3* wp      = reinterpret_cast<const Vec3*>(boat + 0x50);

    Vec3 fromDock = { dockPos->x - wp->x, dockPos->y - wp->y, dockPos->z - wp->z };
    float gate = 0.0f;
    if (s_V3Length) s_V3Length(&fromDock, &gate);

    Vec3 toWp = { wp->x - modelPos.x, wp->y - modelPos.y, wp->z - modelPos.z };
    Vec3 dir = {0, 0, 0};
    float dropLen = 0.0f;
    if (s_V3NormalizeLen) s_V3NormalizeLen(&toWp, &dir, &dropLen);

    if (s_CheckPtr) s_CheckPtr(reinterpret_cast<void*>(boat + 0x38));
    if (const uintptr_t chr = *reinterpret_cast<uintptr_t*>(boat + 0x38)) {
        *reinterpret_cast<uint8_t*>(chr + 0x28) = 0;
        Vec3 flat = { dir.x, dir.y, 0.0f };
        Vec3 nrm = {0, 0, 0};
        if (s_V3Normalize && s_V3Normalize(&flat, &nrm)) {
            *reinterpret_cast<float*>(chr + 0x2c) = nrm.x;
            *reinterpret_cast<float*>(chr + 0x30) = nrm.y;
            *reinterpret_cast<float*>(chr + 0x34) = nrm.z;
        }
        if (s_V3Heading) *reinterpret_cast<float*>(chr + 0x3c) = s_V3Heading(&dir);
    }

    if (s_fArrive && *s_fArrive > gate) {
        if (TraceEnabled()) {
            std::fprintf(stderr, "[BoatCtrl] state 3 -> 4 (gate=%.3f < %.3f)\n",
                         gate, *s_fArrive);
            std::fflush(stderr);
        }
        RequestTransition(smV, 4);
    }
    return 1;
}

// ============================================================
// state 4 ENTER — 0x140044cd0
// ============================================================
extern "C" char BoatCtrl_State4_Enter(void* selfV) {
    Resolve();
    SetAnim(reinterpret_cast<uintptr_t>(selfV), 1);
    if (TraceEnabled()) { std::fprintf(stderr, "[BoatCtrl] state 4 ENTER (leaving map)\n"); std::fflush(stderr); }
    return 1;
}

// ============================================================
// state 4 TRANS30 (per-frame) — 0x140044c40
// ============================================================
// Same steer call, same "route exhausted" test — but no event to the post, because by
// state 4 the post has already released this merchant. Just delete the entity.
extern "C" char BoatCtrl_State4_Tick(void* selfV, void* /*smV*/, void* /*time*/) {
    Resolve();
    const uintptr_t boat = reinterpret_cast<uintptr_t>(selfV);
    if (!Steer_63d0(boat)) DeleteBoatEntity(boat);
    return 1;
}

// ============================================================
// Table build — replicates 0x1400468a0
// ============================================================
// Idempotent: the installer runs once per component instance.
extern "C" void BoatCtrl_PopulateTable() {
    ::ControllerLive::StateTable& t = BoatDispatch::table;
    if (t.count != 0) return;
    using namespace ::ControllerLive;

    // TableRegisterState(t, id, enter, trans30, trans38, update)
    // state 0 — outbound along the route
    TableRegisterState(t, 0,
        reinterpret_cast<void*>(&BoatCtrl_State0_Enter),    // 53f0
        reinterpret_cast<void*>(&BoatCtrl_State0_Tick),     // 5250  (R9)
        nullptr,                                            // retail: always 0
        reinterpret_cast<void*>(&BoatCtrl_State0_Update));  // 5190  (R8)
    TableAddTransition(t, 1, nullptr);
    TableAddTransition(t, 4, nullptr);
    TableAddEvent(t, 1, reinterpret_cast<void*>(&BoatCtrl_Event1_Abort));   // 5670

    // state 1 — final approach
    TableRegisterState(t, 1,
        reinterpret_cast<void*>(&BoatCtrl_State1_Enter),    // 5150
        reinterpret_cast<void*>(&BoatCtrl_State1_Tick),     // 4ff0
        nullptr, nullptr);
    TableAddTransition(t, 2, nullptr);
    TableAddTransition(t, 4, nullptr);
    TableAddEvent(t, 1, reinterpret_cast<void*>(&BoatCtrl_Event1_Abort));

    // state 2 — docked (posts event 9 to the post on entry)
    TableRegisterState(t, 2,
        reinterpret_cast<void*>(&BoatCtrl_State2_Enter),    // 4f70
        nullptr, nullptr, nullptr);
    TableAddTransition(t, 3, nullptr);
    TableAddTransition(t, 4, nullptr);
    TableAddEvent(t, 0, reinterpret_cast<void*>(&BoatCtrl_State2_Event0));  // 4f00
    TableAddEvent(t, 1, reinterpret_cast<void*>(&BoatCtrl_Event1_Abort));

    // state 3 — departing
    TableRegisterState(t, 3,
        reinterpret_cast<void*>(&BoatCtrl_State3_Enter),    // 4ec0
        reinterpret_cast<void*>(&BoatCtrl_State3_Tick),     // 4d10
        nullptr, nullptr);
    TableAddTransition(t, 4, nullptr);

    // state 4 — gone
    TableRegisterState(t, 4,
        reinterpret_cast<void*>(&BoatCtrl_State4_Enter),    // 4cd0
        reinterpret_cast<void*>(&BoatCtrl_State4_Tick),     // 4c40
        nullptr, nullptr);

    std::fprintf(stderr, "[BoatCtrl] table built: %d states (0..4)\n", t.count);
    std::fflush(stderr);
}
