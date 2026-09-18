// code/Game/Component/boatcomponent_route.cpp
// The merchant-boat ROUTE BUILDER — retail BoatComponent::SetDestination (0x1400456e0)
// and the leaves it needs. This is layer 2 of the TradeComponent controller wave: layer 1
// (tradecomponent_states.cpp) made a trading post arm its merchant timer; this is what
// the timer's expiry can finally call.
//
// ── What a "route" is ────────────────────────────────────────────────────────────
// The world holds a set of FEATURE POLYLINES at worldGeom+0x1e0 (worldGeom = gameWorld
// +0x110), filled by worldgen.cpp step 8. Retail's boat route uses POLYLINE 0 only — the
// record at the array base, with no index arithmetic (`**(longlong**)(wg+0x1e0)` for the
// points, `*(*(wg+0x1e0)+8)` for the count). Those points run from OUTSIDE the map, in
// across the world, and back out again.
//
// The finished route (BoatComponent +0x60 array / +0x68 count / +0x6c capacity, stride
// 0xc Vector3) is three pieces spliced together:
//
//   1. every polyline point BEFORE the polyline first enters the map — the approach lane
//      the boat sails in along, taken straight from the polyline with z = 0;
//   2. two A* legs through the water grid — map-entry -> dock, and dock -> map-exit —
//      each cell converted to a world position and height-sampled;
//   3. every polyline point from the first OUT-of-map point onward — the departure lane.
//
// If EITHER A* leg comes back empty, retail appends NEITHER (the `0 < a && 0 < b` gate at
// 140045e60) and the route degenerates to the polyline alone. That is retail behaviour,
// not an error path: do not "fix" it into appending the leg that did succeed.
//
// ── The decompile of 0x1400456e0 cannot be read literally ────────────────────────
// The function has two 4096-int stack maps and heavy register/stack-slot aliasing, so
// Ghidra folds several distinct variables onto one name — `local_res8` is by turns a
// point, a loop index and a Vector3-ctor argument slot — and it types the polyline's
// int32 point COUNT as a float. Everything below is read from the DISASSEMBLY:
//   140045840  MOV EBX,[RAX+0x8]        the count is an int32, not a float
//   1400458df  the in-bounds test that BREAKS loop 1 (first point inside the map)
//   140046268  a7310(&comp[0x50], &comp[0x40]) — nearest route point to the dock
//   14004636b  the unary Vector3 minus that flips the dock approach heading
//
// ── Deliberate, behaviour-preserving deviations ──────────────────────────────────
//  * Retail's forward scan (loop 2) is unrolled x4 by the compiler. It is written here as
//    one plain loop; the unrolled form's `local_80f8` shuffling exists only to keep "last
//    in-map point" correct across the 4-wide body, which the plain loop gets for free.
//    Checked branch-by-branch against 140045a18..140045cd5.
//  * Vector3 construction, subtraction, dot and unary minus are inlined as float
//    arithmetic rather than dispatched through the Math slots. A Vector3 is exactly three
//    floats (the route array's 0xc stride and the 3x MOV copy at 1400460b8 prove it) and
//    these are pure arithmetic, so the result is identical. Normalize / Heading /
//    FromYawPitch / the Transform and Quaternion ops are NOT inlined — those carry angle
//    and handedness conventions worth taking from the engine.
//
// GATED: reached only through BANISHED_LIVE_TRADE_CTRL (the state-2 merchant spawn) and
// the controller's shared teardown. Nothing else in the build calls it.

#include "engine_funcs.h"
#include "system/String.h"
#include "math/Math.h"
#include "resolve.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B

// ── Resolved leaves ──────────────────────────────────────────────────────────────
namespace {

typedef void* (*fn_Find3ea0_t)(int64_t entity, void* outIBP);              // 0x140003ea0
typedef void* (*fn_FindInteract_t)(int64_t entity, void* outIBP);          // 0x140002ed0
typedef float*(*fn_GridToWorld_t)(int* terrain, float* out, const float* v,
                                  char flag, void* outNormal);             // 0x14009f3e0
typedef void  (*fn_PfSetCost_t)(void* pf, int idx, int cost, uint64_t ctx); // 0x1400a6a30
typedef void  (*fn_PfFlags_t)(void* pf, char a, char b, char c);           // 0x1400a68b0
typedef int   (*fn_PfRun_t)(int64_t pf, uint32_t from, uint64_t to,
                            void* mode, int64_t outBuf, int maxLen);       // 0x1400a6e30

fn_Find3ea0_t         s_Find3ea0       = nullptr;
fn_FindInteract_t     s_FindInteract   = nullptr;
fn_GridToWorld_t      s_GridToWorld    = nullptr;
fn_PfSetCost_t        s_PfSetCost      = nullptr;
fn_PfFlags_t          s_PfFlags        = nullptr;
fn_PfRun_t            s_PfRun          = nullptr;
const uint32_t*       s_pTid_InteractDescription = nullptr;   // 0x1401c7938
const uint32_t*       s_pTid_ModelComponent      = nullptr;   // 0x1401c7a50
const uint32_t*       s_pTid_BoatComponent       = nullptr;   // 0x1401c4020
const float*          s_pHalfTileBias            = nullptr;   // 0x140172114 == 0.5f
bool                  s_resolved = false;

template <typename T>
T SlotFn(uintptr_t slotAddr) {
    const uintptr_t slot = ResolveAddr(Module::Exe, slotAddr);
    return slot ? *reinterpret_cast<T*>(slot) : nullptr;
}

void ResolveOnce() {
    if (s_resolved) return;
    s_resolved = true;
    s_Find3ea0      = reinterpret_cast<fn_Find3ea0_t>(ResolveAddr(Module::Exe, 0x140003ea0));
    s_FindInteract  = reinterpret_cast<fn_FindInteract_t>(ResolveAddr(Module::Exe, 0x140002ed0));
    s_GridToWorld   = reinterpret_cast<fn_GridToWorld_t>(ResolveAddr(Module::Exe, 0x14009f3e0));
    s_PfSetCost     = reinterpret_cast<fn_PfSetCost_t>(ResolveAddr(Module::Exe, 0x1400a6a30));
    s_PfFlags       = reinterpret_cast<fn_PfFlags_t>(ResolveAddr(Module::Exe, 0x1400a68b0));
    s_PfRun         = reinterpret_cast<fn_PfRun_t>(ResolveAddr(Module::Exe, 0x1400a6e30));
    s_pTid_InteractDescription =
        reinterpret_cast<const uint32_t*>(ResolveAddr(Module::Exe, 0x1401c7938));
    s_pTid_ModelComponent =
        reinterpret_cast<const uint32_t*>(ResolveAddr(Module::Exe, 0x1401c7a50));
    s_pTid_BoatComponent =
        reinterpret_cast<const uint32_t*>(ResolveAddr(Module::Exe, 0x1401c4020));
    s_pHalfTileBias = reinterpret_cast<const float*>(ResolveAddr(Module::Exe, 0x140172114));
}


}  // namespace

// ============================================================
// 0x140123880 — Entity::FindAttachPoint(name, outPos, outYaw)
// ============================================================
// Looks up a NAMED attach point ("boat" for a trading post's dock) on a component's
// description and resolves it to a WORLD position and heading through the owning entity's
// ModelComponent transform. Recurses into child components when the name is not on this
// one; returns false when no child has it either.
//
// Attach-point table: comp(+0x18) = description -> +0x20 -> { +0x30 array, +0x38 count },
// stride 0x18 per entry:
//     +0x00  System::String  name       (8 bytes)
//     +0x08  Vector3         localPos
//     +0x14  float           localYaw
// The stride and both field offsets come from 1401239d7 (`LEA RDI,[RAX+RAX*2]` = idx*3,
// then `[RBX+8+RDI*8]`) and 140123a3c (`[RCX+RDI*8+0x14]`) — not guessed.
extern "C" bool BoatRoute_FindAttachPoint(void* compV, const void* nameStr,
                                          float* outPos, float* outYaw) {
    ResolveOnce();
    const uintptr_t comp = reinterpret_cast<uintptr_t>(compV);
    if (!comp) return false;

    // The description, IsDerivedFrom-gated exactly as retail. A failed cast yields NULL
    // and retail then derefs it — faithful: it relies on this component always carrying
    // an InteractDescription. The null-check below is on OUR side of that only to keep
    // the recursion honest, and it is the same branch retail takes when the table is
    // empty (count 0 -> no name matched -> recurse).
    void* descRaw = *reinterpret_cast<void**>(comp + 0x18);
    void* desc = nullptr;
    if (fn_IsDerivedFrom && s_pTid_InteractDescription &&
        fn_IsDerivedFrom(descRaw, *s_pTid_InteractDescription)) {
        desc = descRaw;
    }

    int hit = -1;
    uintptr_t table = 0;
    if (desc) {
        table = *reinterpret_cast<uintptr_t*>(reinterpret_cast<uintptr_t>(desc) + 0x20);
        const int n = table ? *reinterpret_cast<int*>(table + 0x38) : 0;
        for (int i = 0; i < n; ++i) {
            const uintptr_t entry = *reinterpret_cast<uintptr_t*>(table + 0x30) +
                                    static_cast<uintptr_t>(i) * 0x18;
            if (*reinterpret_cast<const System::String*>(entry) ==
                *reinterpret_cast<const System::String*>(nameStr)) {   // B.3.1 bridge
                hit = i;
                break;
            }
        }
    }

    if (hit < 0) {
        // Not on this component — try each child (comp+0x28 array of IBPs, +0x30 count).
        const int childCount = *reinterpret_cast<int*>(comp + 0x30);
        for (int i = 0; i < childCount; ++i) {
            const uintptr_t childSlot = *reinterpret_cast<uintptr_t*>(comp + 0x28) +
                                        static_cast<uintptr_t>(i) * 8;
            if (fn_InternalBasePtr_CheckPtr)
                fn_InternalBasePtr_CheckPtr(reinterpret_cast<void*>(childSlot));
            void* child = *reinterpret_cast<void**>(childSlot);
            if (BoatRoute_FindAttachPoint(child, nameStr, outPos, outYaw)) return true;
        }
        return false;
    }

    // Found the name. Its position is relative to the owning entity's ModelComponent.
    if (fn_InternalBasePtr_CheckPtr)
        fn_InternalBasePtr_CheckPtr(reinterpret_cast<void*>(comp + 0x10));
    const uintptr_t entity = *reinterpret_cast<uintptr_t*>(comp + 0x10);
    const uint32_t wantedTid = s_pTid_ModelComponent ? *s_pTid_ModelComponent : 0;
    void* model = nullptr;
    const int compCount = entity ? *reinterpret_cast<int*>(entity + 0x38) : 0;
    for (int i = 0; i < compCount; ++i) {
        const uintptr_t slot = *reinterpret_cast<uintptr_t*>(entity + 0x30) +
                               static_cast<uintptr_t>(i) * 8;
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(reinterpret_cast<void*>(slot));
        const uintptr_t obj = *reinterpret_cast<uintptr_t*>(slot);
        if (!obj) continue;
        typedef uint32_t (*GetTypeId_t)(void*);
        GetTypeId_t getTypeId =
            *reinterpret_cast<GetTypeId_t*>(*reinterpret_cast<uintptr_t*>(obj) + 0x10);
        if (getTypeId && getTypeId(reinterpret_cast<void*>(obj)) == wantedTid) {
            void* cand = *reinterpret_cast<void**>(slot);
            model = (fn_IsDerivedFrom && fn_IsDerivedFrom(cand, wantedTid)) ? cand : nullptr;
            break;
        }
    }

    uint64_t modelIBP[2] = {0, 0};
    if (fn_InternalBasePtr_Ctor) new (modelIBP) ::Object::InternalBasePtr(
        reinterpret_cast<RtObject::Internal*>(model));   // B.3.3 bridge
    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(modelIBP);
    const uintptr_t m = static_cast<uintptr_t>(modelIBP[0]);
    if (!m) return false;   // no ModelComponent: retail would fault on m+0x148

    const uintptr_t entry = *reinterpret_cast<uintptr_t*>(table + 0x30) +
                            static_cast<uintptr_t>(hit) * 0x18;

    // worldPos = modelTransform * entry.localPos   (the transform lives at model+0x148)
    // Transform copy-ctor (slot 0x140151418), Transform::operator*(Vector3) (slot 0x140151470).
    const Math::Transform xform = *reinterpret_cast<const Math::Transform*>(m + 0x148);   // B.3.1 bridge
    const Math::Vector3 worldPos =
        xform * *reinterpret_cast<const Math::Vector3*>(entry + 0x08);   // B.3.1 bridge
    outPos[0] = worldPos.x;
    outPos[1] = worldPos.y;
    outPos[2] = worldPos.z;

    // worldYaw = Heading(modelRotation * FromYawPitch(entry.localYaw, 0)).
    // Retail copies model+0x148..+0x160 (7 floats: quaternion + translation) and hands
    // Quaternion::Rotate the head of that copy, so the rotate sees the quaternion only.
    // Vector3::FromYawPitch slot 0x1401512a0, Quaternion::Rotate slot 0x1401525f0,
    // Vector3::Heading slot 0x1401512d8.
    const float localYaw  = *reinterpret_cast<const float*>(entry + 0x14);
    const float zeroPitch = 0.0f;
    const Math::Transform rot = *reinterpret_cast<const Math::Transform*>(m + 0x148);   // B.3.1 bridge
    const Math::Vector3 yawVec = Math::Vector3::FromYawPitch(localYaw, zeroPitch);
    const Math::Vector3 rotated = rot.rotation.Rotate(yawVec);
    *outYaw = rotated.Heading();
    return true;
}

// ============================================================
// 0x14000eb20 — Entity::FindComponent(BoatComponent)
// ============================================================
// The standard exe FindComponent shape: walk the entity's component array
// (+0x30 base / +0x38 count, stride 8), compare each object's vtable-slot-2 type id
// against a .rdata constant, and hand the match back through an InternalBasePtr
// out-param — which is also the return value.
//
// The constant is DAT_1401c4020 = "BoatComponent" (BoatComponent_RegisterType writes it
// at 14014a8a5; BoatComponent_GetType reads it at 1400036c0). Unregistered it reads 0,
// nothing matches, and the merchant spawn quietly routes nothing — the exact failure
// mode of reference_gated_element_typeid_disables_controller. reg_tradeui_typeids.cpp
// seeds it behind BANISHED_LIVE_TRADE_TIDS.
extern "C" void* BoatRoute_FindBoatComponent(int64_t entity, void* outIBP) {
    ResolveOnce();
    const uint32_t wanted = s_pTid_BoatComponent ? *s_pTid_BoatComponent : 0;
    void* found = nullptr;
    const int n = entity ? *reinterpret_cast<int*>(entity + 0x38) : 0;
    for (int i = 0; i < n; ++i) {
        const uintptr_t slot = *reinterpret_cast<uintptr_t*>(entity + 0x30) +
                               static_cast<uintptr_t>(i) * 8;
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(reinterpret_cast<void*>(slot));
        const uintptr_t obj = *reinterpret_cast<uintptr_t*>(slot);
        if (!obj) continue;
        typedef uint32_t (*GetTypeId_t)(void*);
        GetTypeId_t getTypeId =
            *reinterpret_cast<GetTypeId_t*>(*reinterpret_cast<uintptr_t*>(obj) + 0x10);
        if (getTypeId && getTypeId(reinterpret_cast<void*>(obj)) == wanted) {
            void* cand = *reinterpret_cast<void**>(slot);
            found = (fn_IsDerivedFrom && fn_IsDerivedFrom(cand, wanted)) ? cand : nullptr;
            break;
        }
    }
    if (fn_InternalBasePtr_Ctor) new (outIBP) ::Object::InternalBasePtr(
        reinterpret_cast<RtObject::Internal*>(found));   // B.3.3 bridge
    return outIBP;
}

// ============================================================
// 0x1400a7310 — index of the route point nearest a position
// ============================================================
// `arrV` points at BoatComponent+0x50, which is the head of the route's array struct:
// +0x10 from there is the element pointer (comp+0x60) and +0x18 the count (comp+0x68).
// Retail seeds the running minimum from element 0 and then re-tests element 0 in the
// loop; kept, because it makes the empty-array case return 0 rather than -1 and the
// caller indexes with the result unguarded.
extern "C" int BoatRoute_NearestPointIndex(void* arrV, const float* pt) {
    const uintptr_t arr = reinterpret_cast<uintptr_t>(arrV);
    const float* base = *reinterpret_cast<const float* const*>(arr + 0x10);
    if (!base) return 0;

    float dx = pt[0] - base[0], dy = pt[1] - base[1], dz = pt[2] - base[2];
    float best = dx * dx + dy * dy + dz * dz;
    int bestIdx = 0;

    const int n = *reinterpret_cast<const int*>(arr + 0x18);
    for (int i = 0; i < n; ++i) {
        const float* e = *reinterpret_cast<const float* const*>(arr + 0x10) + i * 3;
        dx = pt[0] - e[0]; dy = pt[1] - e[1]; dz = pt[2] - e[2];
        const float d = dx * dx + dy * dy + dz * dz;
        if (d < best) { best = d; bestIdx = i; }
    }
    return bestIdx;
}

// ============================================================
// 0x14009ffc0 — run one WATER A* leg
// ============================================================
// Loads the boat movement-cost profile into the shared pathfinder worker, then runs it.
// The eleven costs and the 0x380 mode word are retail's, verified against
// 1400a0000..1400a00a8. Returns the number of cells written to `outBuf` (capacity 0x1000
// ints, which is why 56e0's two stack maps are int[4096]).
//
// ⚠ RETURN VALUE: Ghidra types both this and 0x1400a6e30 `void`, but 6e30 TAIL-CALLS
// 0x1400a6d50 (at 1400a6edf) whose EAX is the copied path length, and 56e0 uses that
// length to decide whether either leg found anything. keystone_a_zone.cpp's port of 6e30
// was declared void for the same reason and is widened to int alongside this file — its
// own caller discards the value, so nothing else changes.
// See reference_discarded_return_reenables_gated_branch.
extern "C" int BoatRoute_FindWaterPath(int64_t terrain, void* /*unused2*/, uint32_t fromCell,
                                       uint64_t toCell, void* /*unused5*/, int64_t outBuf) {
    ResolveOnce();
    void* pf = *reinterpret_cast<void**>(terrain + 0x68);
    if (!pf || !s_PfSetCost || !s_PfFlags || !s_PfRun) {
        // A missing worker and "no water route" both end as 0 cells at the caller, and
        // they are completely different bugs. Say which one this was.
        std::fprintf(stderr, "[BoatRoute] A* SKIPPED: pf=%p setCost=%p flags=%p run=%p\n",
                     pf, reinterpret_cast<void*>(s_PfSetCost),
                     reinterpret_cast<void*>(s_PfFlags), reinterpret_cast<void*>(s_PfRun));
        std::fflush(stderr);
        return 0;
    }

    static const int kCost[11] = {10, 10, 10, 10, 10, 10, 10, 5, 1, 5, 10};
    for (int i = 0; i < 11; ++i) s_PfSetCost(pf, i, kCost[i], toCell);
    s_PfFlags(pf, 1, 0, 0);
    const int n = s_PfRun(reinterpret_cast<int64_t>(pf), fromCell, toCell & 0xffffffffu,
                          reinterpret_cast<void*>(0x380), outBuf, 0x1000);
    return n;
}

// ============================================================
// 0x1400456e0 — BoatComponent::SetDestination(entity)
// ============================================================
namespace {

// The route array's append, inlined by retail at four sites (140045909, 140045fa9,
// 140046184, and the tail of the second A* loop). All four are the same body:
//   if (count == 0) { comp+0x50 = pt; comp+0x70 = 0; }        // first point + cursor
//   grow to count+1 (capacity = count+1, or (count+1)*3 when growing from non-zero)
//   copy the surviving elements, write pt into the new tail
// The `if (dst)` inside retail's copy loop is a NULL-BASE test (JZ on base+offset), kept.
void AppendRoutePoint(uintptr_t comp, const float pt[3]) {
    if (*reinterpret_cast<int*>(comp + 0x68) == 0) {
        *reinterpret_cast<float*>(comp + 0x50) = pt[0];
        *reinterpret_cast<float*>(comp + 0x54) = pt[1];
        *reinterpret_cast<float*>(comp + 0x58) = pt[2];
        *reinterpret_cast<uint32_t*>(comp + 0x70) = 0;
    }

    const int oldCount = *reinterpret_cast<int*>(comp + 0x68);
    const int newCount = oldCount + 1;
    uint8_t* oldBase = *reinterpret_cast<uint8_t**>(comp + 0x60);
    *reinterpret_cast<int*>(comp + 0x68) = newCount;

    if (newCount > *reinterpret_cast<int*>(comp + 0x6c)) {
        *reinterpret_cast<int*>(comp + 0x6c) = newCount;
        if (oldCount != 0) *reinterpret_cast<int*>(comp + 0x6c) = newCount * 3;
        void* fresh = fn_MemAllocStatic
                    ? fn_MemAllocStatic(static_cast<uint64_t>(*reinterpret_cast<int*>(comp + 0x6c)) * 0xc, 4)
                    : nullptr;
        *reinterpret_cast<void**>(comp + 0x60) = fresh;
    }

    uint8_t* newBase = *reinterpret_cast<uint8_t**>(comp + 0x60);
    int i = (oldBase == newBase) ? oldCount : 0;
    for (; i < newCount; ++i) {
        if (!newBase) continue;          // retail's JZ on base+offset: skip, do not break
        float* dst = reinterpret_cast<float*>(newBase + static_cast<size_t>(i) * 0xc);
        const float* src = (i < oldCount)
                         ? reinterpret_cast<const float*>(oldBase + static_cast<size_t>(i) * 0xc)
                         : pt;
        dst[0] = src[0];
        dst[1] = src[1];
        dst[2] = src[2];
    }
    if (oldBase != newBase && oldBase && fn_MemFreeStatic) fn_MemFreeStatic(oldBase);
}

// (int)(coord + mapSize/2) clamped into [0, mapSize-1] — retail's tile conversion,
// repeated verbatim at every cell computation in 56e0.
inline int ToTileClamped(float coord, int mapSize) {
    int t = static_cast<int>(coord + static_cast<float>(mapSize >> 1));
    if (t < 0) t = 0;
    if (t >= mapSize) t = mapSize - 1;
    return t;
}

inline bool TileInMap(float x, float y, int mapSize) {
    const float half = static_cast<float>(mapSize >> 1);
    const int ix = static_cast<int>(x + half);
    const int iy = static_cast<int>(y + half);
    return ix >= 0 && ix < mapSize && iy >= 0 && iy < mapSize;
}

}  // namespace

extern "C" void BoatComponent_SetDestination(int64_t compI, int64_t* destEntityIBP) {
    ResolveOnce();
    const uintptr_t comp = static_cast<uintptr_t>(compI);
    const int64_t dest = destEntityIBP ? *destEntityIBP : 0;
    *reinterpret_cast<int64_t*>(comp + 0x28) = dest;

    // ── Null destination: RELEASE the boat ───────────────────────────────────────
    // The controller's teardown (0x14005b160) calls in with a null entity to hand the
    // merchant back. Retail finds a component on the boat's own entity and dispatches
    // its +0x48 owner through vtable slot 2, passing the byte at +0x51.
    if (dest == 0) {
        if (fn_InternalBasePtr_CheckPtr)
            fn_InternalBasePtr_CheckPtr(reinterpret_cast<void*>(comp + 0x10));
        uint64_t outIBP[2] = {0, 0};
        void* found = s_Find3ea0 ? s_Find3ea0(*reinterpret_cast<int64_t*>(comp + 0x10), outIBP)
                                 : nullptr;
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(found);
        if (!found) return;
        const uintptr_t state = *reinterpret_cast<uintptr_t*>(found);
        if (!state) return;
        uintptr_t* ownerSlot = reinterpret_cast<uintptr_t*>(state + 0x48);
        const uintptr_t owner = *ownerSlot;
        if (!owner) return;
        typedef void (*Release_t)(void*, void*, int, int);
        Release_t release = *reinterpret_cast<Release_t*>(*reinterpret_cast<uintptr_t*>(owner) + 0x10);
        if (release)
            release(reinterpret_cast<void*>(owner), ownerSlot,
                    static_cast<int>(*reinterpret_cast<int8_t*>(state + 0x51)), 1);
        return;
    }

    // ── Where the boat is going: the destination's "boat" attach point ───────────
    if (fn_InternalBasePtr_CheckPtr)
        fn_InternalBasePtr_CheckPtr(reinterpret_cast<void*>(comp + 0x28));
    uint64_t interactIBP[2] = {0, 0};
    if (s_FindInteract) s_FindInteract(*reinterpret_cast<int64_t*>(comp + 0x28), interactIBP);
    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(interactIBP);

    System::String boatName("boat");
    float* const destPos = reinterpret_cast<float*>(comp + 0x40);   // Vector3
    float* const destYaw = reinterpret_cast<float*>(comp + 0x4c);
    BoatRoute_FindAttachPoint(reinterpret_cast<void*>(interactIBP[0]),
                              &boatName, destPos, destYaw);

    // The dock's facing, as a direction vector. Flipped at the end if the route
    // approaches from the opposite side. (Vector3::FromYawPitch, slot 0x1401512a0.)
    const float zeroPitch = 0.0f;
    Math::Vector3 dockForward = Math::Vector3::FromYawPitch(*destYaw, zeroPitch);

    void* gw = (g_pGameWorldSingleton && *g_pGameWorldSingleton) ? *g_pGameWorldSingleton : nullptr;
    if (!gw) return;
    int* terrain   = *reinterpret_cast<int**>(reinterpret_cast<uintptr_t>(gw) + 0xf8);
    uintptr_t wgeo = *reinterpret_cast<uintptr_t*>(reinterpret_cast<uintptr_t>(gw) + 0x110);
    if (!terrain || !wgeo) return;

    const uintptr_t poly = *reinterpret_cast<uintptr_t*>(wgeo + 0x1e0);   // record 0 — retail
    const int   polyCount = poly ? *reinterpret_cast<int*>(poly + 8) : 0; // int32 (140045840)
    const float* polyPts  = poly ? *reinterpret_cast<const float**>(poly) : nullptr;

    // ── 1. the approach lane: every polyline point before the map ────────────────
    int consumed = 0;      // points examined, including the first in-map one
    int resumeIdx = 0;     // where piece 3 restarts
    float entryX = 0.0f, entryY = 0.0f;
    if (polyPts) {
        for (int i = 0; i < polyCount; ++i) {
            const float px = polyPts[i * 2 + 0];
            const float py = polyPts[i * 2 + 1];
            ++consumed;
            entryX = px;
            entryY = py;
            if (TileInMap(px, py, *terrain)) { resumeIdx = i; break; }
            const float pt[3] = {px, py, 0.0f};
            AppendRoutePoint(comp, pt);
            entryX = 0.0f;
            entryY = 0.0f;
            resumeIdx = 0;
        }
    }

    // ── 2a. the exit: scan forward for the first point back OUTSIDE the map ──────
    // Retail resumes piece 3 at (index of that point) + 1 — the transition point itself
    // is skipped. That is retail's arithmetic (uVar26 is pre-incremented at 140045c9e
    // before the point is read), not an off-by-one here.
    float exitX = 0.0f, exitY = 0.0f;
    if (polyPts) {
        for (int i = consumed; i < polyCount; ++i) {
            const float px = polyPts[i * 2 + 0];
            const float py = polyPts[i * 2 + 1];
            resumeIdx = i + 1;
            if (!TileInMap(px, py, *terrain)) break;
            exitX = px;
            exitY = py;
        }
    }

    // ── 2b. the two A* legs ─────────────────────────────────────────────────────
    static int s_cellsIn[4096];
    static int s_cellsOut[4096];

    int mapSize = *terrain;
    const int destTx  = ToTileClamped(destPos[0], mapSize);
    const int destTy  = ToTileClamped(destPos[1], mapSize);
    const int entryTx = ToTileClamped(entryX, mapSize);
    const int entryTy = ToTileClamped(entryY, mapSize);

    const int nIn = BoatRoute_FindWaterPath(reinterpret_cast<int64_t>(terrain), nullptr,
                                            static_cast<uint32_t>(mapSize * entryTy + entryTx),
                                            static_cast<uint64_t>(static_cast<uint32_t>(mapSize * destTy + destTx)),
                                            nullptr, reinterpret_cast<int64_t>(s_cellsIn));

    mapSize = *terrain;                                   // retail re-reads it
    const int exitTx  = ToTileClamped(exitX, mapSize);
    const int exitTy  = ToTileClamped(exitY, mapSize);
    const int destTx2 = ToTileClamped(destPos[0], mapSize);
    const int destTy2 = ToTileClamped(destPos[1], mapSize);
    const int nOut = BoatRoute_FindWaterPath(reinterpret_cast<int64_t>(terrain), nullptr,
                                             static_cast<uint32_t>(mapSize * destTy2 + destTx2),
                                             static_cast<uint64_t>(static_cast<uint32_t>(mapSize * exitTy + exitTx)),
                                             nullptr, reinterpret_cast<int64_t>(s_cellsOut));

    // CONTROL PROBE (BANISHED_BOAT_TRACE only, no behaviour): both real legs end at the
    // dock, which sits on LAND by construction — so "0 cells" there is ambiguous between
    // "this dock is unreachable" and "water pathing does not work in standalone at all".
    // Query between two points that are unambiguously ON the polyline centreline instead.
    // A non-zero result here means the A* is fine and the dock is the problem; a zero
    // means the water profile or the nav grid is, and the route builder is being blamed
    // for someone else's gap.

    // BOTH legs or NEITHER — retail's gate at 140045e60.
    if (nIn > 0 && nOut > 0) {
        const float bias = static_cast<float>(*terrain >> 1) -
                           (s_pHalfTileBias ? *s_pHalfTileBias : 0.5f);
        for (int pass = 0; pass < 2; ++pass) {
            const int* cells = pass == 0 ? s_cellsIn : s_cellsOut;
            const int  n     = pass == 0 ? nIn : nOut;
            for (int i = 0; i < n; ++i) {
                const float cellPos[3] = {
                    static_cast<float>(cells[i] % *terrain) - bias,
                    static_cast<float>(cells[i] / *terrain) - bias,
                    0.0f
                };
                float snapped[3] = {0.0f, 0.0f, 0.0f};
                if (s_GridToWorld) s_GridToWorld(terrain, snapped, cellPos, '\0', nullptr);
                AppendRoutePoint(comp, snapped);
            }
        }
    }

    // ── 3. the departure lane: the rest of the polyline ─────────────────────────
    if (polyPts) {
        for (int i = resumeIdx; i < polyCount; ++i) {
            const float pt[3] = {polyPts[i * 2 + 0], polyPts[i * 2 + 1], 0.0f};
            AppendRoutePoint(comp, pt);
        }
    }

    // ── the dock approach heading ───────────────────────────────────────────────
    // Take the route direction near the dock (nearest point minus the one two back) and
    // flip the dock's own facing if the boat arrives against it.
    // ⚠ `nearest - 2` is retail's index arithmetic (LEA EAX,[RBX-0x2] at 1400462bc) and it
    // is NOT bounds-checked there either: retail relies on the dock never being the first
    // two points of a route. Left faithful — a negative index here means the route is
    // degenerate (an empty polyline, or both A* legs failed), and the fix belongs upstream
    // at whichever of those produced it, not in a guard that silently yields a wrong yaw.
    const int nearest = BoatRoute_NearestPointIndex(reinterpret_cast<void*>(comp + 0x50), destPos);
    const float* route = *reinterpret_cast<const float* const*>(comp + 0x60);
    if (route) {
        const float* a = route + static_cast<size_t>(nearest) * 3;
        const float* b = route + static_cast<size_t>(nearest - 2) * 3;
        const Math::Vector3 dir(a[0] - b[0], a[1] - b[1], 0.0f);
        const Math::Vector3 normalized = dir.Normalize();   // slot 0x1401512c0
        const float dot = normalized.x * dockForward.x +
                          normalized.y * dockForward.y +
                          normalized.z * dockForward.z;
        if (dot < 0.0f) {
            dockForward.x = -dockForward.x;
            dockForward.y = -dockForward.y;
            dockForward.z = -dockForward.z;
        }
        *destYaw = dockForward.Heading();   // slot 0x1401512d8
    }

}
