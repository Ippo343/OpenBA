// code/Game/Component/citizencomponent_lifecycle.cpp
// Citizen-lifecycle quartet (openfuncs step 2.5a) — sibling of the (huge)
// citizencomponent.cpp. Faithful ports, Ghidra-verified against Application-x64.exe:
//
//   0x140112040  CitizenComp_LeaveRoster_2040     — unlink from the workplace worker
//                roster (doubly-linked ring at node +0x268/+0x270, head cache self
//                +0x260), then clear the workplace slot (AssignWorkplace(null,-1)) and
//                the current-workplace link (+0x258).
//   0x1401168a0  CitizenComp_JoinRoster_68a0      — inverse: splice a citizen into the
//                roster before the head (or seed a singleton ring), then read the
//                workplace descriptor (self +0x238 IBP / +0x240 slot) and AssignWorkplace.
//   0x1400302a0  CitizenComp_SortByProfession_02a0 — sort an entity-handle array by
//                profession priority (see note on the introsort below).
//   0x140117080  CitizenComp_DeferredInit_117080  — per-citizen deferred init:
//                name/age/gender/appearance-variant/lifespan/housing/employment.
//
// Registered by standalone/reg_citizen_25.cpp; the callers already ResolveAddr these
// four addresses in citizencomponent.cpp (s_fn_sub_2040 / s_fn_helper_68a0 /
// s_fn_helper_02a0 / s_fn_FUN_140117080), so registering wires them live.
//
// The 0x1400302a0 sort is MSVC's inlined std::sort over 8-byte entity handles:
// median-of-3 introsort with an explicit range stack, comparator 0x140110330 and an
// insertion-sort fallback 0x140030b90 for ranges <= 8. Both comparator and fallback
// reduce a handle to a profession priority (resolve the entity's WorkComponent, read
// its profession key at comp+0x28, look that key up in the world priority RB-tree at
// GameWorld+0x118 and read priority at record+0x24). Both the introsort and the
// fallback are unstable, so a std::sort over the same strict-weak priority ordering
// reproduces the result exactly — only equal-priority tie order may differ, which no
// caller observes (same rationale/precedent as Pickup_SortByDistance in pickup.cpp).
// 0x140110330 / 0x140030b90 are therefore reimplemented (their shared priority
// predicate is ProfessionPriorityOf below), not registered.

#include <new>
#include "Game/exe_bodies.h"
#include "Game/GameUpdateLoop.h"   // B3R STORAGE: GameUpdateLoop::_singleton
#include "Game/exe_constants.h"   // kRdata_* (B3R Leftovers: the .rdata constants)
#include "resolve.h"
#include "system/String.h"
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B
#include "system/Value.h"   // ENDGAME B.3.4b System caller batch
#include "Component/CitizenComponent.h"
#include "Component/exe_bodies.h"
#include "Work/exe_bodies.h"
#include "Player/exe_bodies.h"
#include "Action/exe_bodies.h"

// Clean C++ name generator (code/Game/Game/citizen_names.cpp) — replaces the retail
// name-gen chain (FUN_1400a4c60/a4b30/a4880) that 0x140117080 would otherwise call.
extern "C" bool GenerateCitizenName(void* rng, bool female, char* out, int cap);

// Exe sub-callees are called by name (Component/Work/Player/Action exe_bodies.h).

// ============================================================
// CitizenComp_LeaveRoster_2040 (0x140112040)
// Unlink the citizen (param2 = InternalBasePtr* -> node) from the workplace worker
// roster (ring links node +0x268/+0x270, head cache self +0x260), re-init its own
// links to empty IBPs, clear its workplace assignment and its +0x258 workplace link.
// ============================================================
void CitizenComp_LeaveRoster_2040(int64_t selfV, void* param2V) {
    uint8_t* base = reinterpret_cast<uint8_t*>(selfV);
    int64_t* ibp  = reinterpret_cast<int64_t*>(param2V);   // *ibp == citizen node ptr
    uint64_t empty = 0;

    if (*reinterpret_cast<int64_t*>(base + 0x260) == *ibp) {
        *reinterpret_cast<int64_t*>(base + 0x260) = *reinterpret_cast<int64_t*>(*ibp + 0x268);
    }
    // node.prev.next = node.next
    int64_t nextLink = *reinterpret_cast<int64_t*>(*ibp + 0x268);
    int64_t node = *ibp;
    *reinterpret_cast<int64_t*>(*reinterpret_cast<int64_t*>(node + 0x270) + 0x268) = nextLink;
    // node.next.prev = node.prev
    int64_t prevLink = *reinterpret_cast<int64_t*>(*ibp + 0x270);
    node = *ibp;
    *reinterpret_cast<int64_t*>(*reinterpret_cast<int64_t*>(node + 0x268) + 0x270) = prevLink;
    // node's own links -> empty IBPs
    new (&empty) ::Object::InternalBasePtr(nullptr); 
    *reinterpret_cast<int64_t*>(*ibp + 0x270) = (int64_t)empty;
    new (&empty) ::Object::InternalBasePtr(nullptr); 
    *reinterpret_cast<int64_t*>(*ibp + 0x268) = (int64_t)empty;
    // if this node was the head, clear the head cache
    if (*reinterpret_cast<int64_t*>(base + 0x260) == *ibp) {
        new (&empty) ::Object::InternalBasePtr(nullptr);   // B.3.3 bridge
        *reinterpret_cast<int64_t*>(base + 0x260) = (int64_t)empty;
    }
    // clear the workplace slot + the current-workplace link (+0x258)
    new (&empty) ::Object::InternalBasePtr(nullptr); 
    CitizenComponent_AssignWorkplace(reinterpret_cast<void*>(*ibp), &empty, -1);
    new (&empty) ::Object::InternalBasePtr(nullptr); 
    *reinterpret_cast<int64_t*>(*ibp + 0x258) = (int64_t)empty;
}

// ============================================================
// CitizenComp_JoinRoster_68a0 (0x1401168a0)
// Inverse of LeaveRoster: splice the citizen (param2) into the roster before the
// current head (or seed a singleton ring when empty), store self as the citizen's
// +0x258 workplace link, then AssignWorkplace using self's +0x238 IBP / +0x240 slot.
// ============================================================
void CitizenComp_JoinRoster_68a0(void* selfV, void* param2V) {
    uint8_t* base = reinterpret_cast<uint8_t*>(selfV);
    int64_t* ibp  = reinterpret_cast<int64_t*>(param2V);
    int64_t head  = *reinterpret_cast<int64_t*>(base + 0x260);
    int64_t v, node;

    if (head == 0) {
        // empty roster -> singleton ring (prev = next = node)
        *reinterpret_cast<int64_t*>(base + 0x260) = *ibp;
        v = *ibp;
        *reinterpret_cast<int64_t*>(*ibp + 0x268) = v;
        v = *ibp;
        node = *ibp;
    } else {
        v = *reinterpret_cast<int64_t*>(head + 0x270);      // head.prev
        *reinterpret_cast<int64_t*>(*ibp + 0x270) = v;      // node.prev = head.prev
        v = *reinterpret_cast<int64_t*>(base + 0x260);      // head
        *reinterpret_cast<int64_t*>(*ibp + 0x268) = v;      // node.next = head
        v = *ibp;
        node = *ibp;
        *reinterpret_cast<int64_t*>(*reinterpret_cast<int64_t*>(node + 0x270) + 0x268) = v; // node.prev.next = node
        v = *ibp;
        node = *ibp;
        node = *reinterpret_cast<int64_t*>(node + 0x268);   // node.next
    }
    *reinterpret_cast<int64_t*>(node + 0x270) = v;

    // store self as an IBP at node +0x258, then AssignWorkplace via self's descriptor
    uint64_t selfIBP = 0;
    new (&selfIBP) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>(base));   // B.3.3 bridge
    *reinterpret_cast<int64_t*>(*ibp + 0x258) = (int64_t)selfIBP;
    int64_t slotHost = *ibp;
    slotHost = *reinterpret_cast<int64_t*>(slotHost + 0x258);   // self (deref stored IBP)
    int64_t wpHost = *ibp;
    uint64_t wpIBP = (uint64_t)*reinterpret_cast<int64_t*>(*reinterpret_cast<int64_t*>(wpHost + 0x258) + 0x238);
    CitizenComponent_AssignWorkplace(reinterpret_cast<void*>(*ibp), &wpIBP,
                                     *reinterpret_cast<int32_t*>(slotHost + 0x240));
}

// ============================================================
// Profession-priority predicate (shared by comparator 0x140110330 and insertion-sort
// fallback 0x140030b90). Resolve the entity's WorkComponent, read its profession key
// (comp+0x28), then look that key up in the world priority RB-tree (GameWorld+0x118).
// ============================================================
static int ProfessionKeyOfEntity(uint64_t handle) {
    int64_t outIBP = 0;
    void* ibp = WorkBase_FindComponent_3050(static_cast<int64_t>(handle), &outIBP);  // returns &outIBP
    int64_t comp = ibp ? *reinterpret_cast<int64_t*>(ibp) : 0;
    return comp ? *reinterpret_cast<int32_t*>(comp + 0x28) : 0;
}

// GameWorld+0x118 is a red-black tree keyed on profession id; node key at node[0],
// left node+0x18, right node+0x20, value ptr node+0x8, priority at value+0x24.
static int LookupProfessionPriority(int key) {
    if (GameUpdateLoop::_singleton == nullptr) return 0;
    uint8_t* world = reinterpret_cast<uint8_t*>(GameUpdateLoop::_singleton);
    int64_t tree = *reinterpret_cast<int64_t*>(world + 0x118);
    if (!tree) return 0;
    int* node = *reinterpret_cast<int**>(tree + 0x30);   // root
    while (node) {
        if (key < node[0])      node = *reinterpret_cast<int**>(node + 6);   // left  (node+0x18)
        else if (key > node[0]) node = *reinterpret_cast<int**>(node + 8);   // right (node+0x20)
        else {
            int64_t value = *reinterpret_cast<int64_t*>(node + 2);           // node+0x8
            return value ? *reinterpret_cast<int*>(value + 0x24) : 0;
        }
    }
    return 0;   // absent: retail builds an empty IBP and derefs; guarded to 0 here
}

static inline int ProfessionPriorityOf(uint64_t handle) {
    return LookupProfessionPriority(ProfessionKeyOfEntity(handle));
}

// ============================================================
// CitizenComp_SortByProfession_02a0 (0x1400302a0) — see file-header note.
// ============================================================
void CitizenComp_SortByProfession_02a0(void* arrV, int n, void* /*tmpV*/) {
    uint64_t* array = reinterpret_cast<uint64_t*>(arrV);
    if (!array || n < 2) return;
    std::sort(array, array + n, [](uint64_t a, uint64_t b) {
        return ProfessionPriorityOf(a) < ProfessionPriorityOf(b);
    });
}

// ============================================================
// CitizenComp_DeferredInit_117080 (0x140117080)
// Per-citizen deferred init. Signature matches the citizencomponent.cpp typedef
// fn_FUN_140117080_t: (self, spawnPos, gender, isElder, arg5, forceEmploy,
// parentIBP, spawnKey). Callers: reproduction/birth path (citizencomponent.cpp),
// Game_SpawnCitizens. Turning it on gives colonists real names/ages/lifespans.
// ============================================================
void CitizenComp_DeferredInit_117080(int64_t selfV, void* pos, uint32_t gender,
                                     char isElder, float age, char forceEmploy,
                                     int64_t* parentIBP, int64_t spawnKey) {
    uint8_t* p = reinterpret_cast<uint8_t*>(selfV);
    void* world = (GameUpdateLoop::_singleton != nullptr) ? GameUpdateLoop::_singleton : nullptr;
    void* rng = world ? *reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(world) + 0xd8) : nullptr;

    // 1-3. reset path/target, set gender mesh variant, place at spawn transform
    void* modelIBP = nullptr;
    ActCNR_FindComponent_3f60(*reinterpret_cast<uintptr_t*>(p + 0x10), &modelIBP);
    if (modelIBP) {
        ActCNR_NR_SetVariant_1253d0(reinterpret_cast<int64_t*>(modelIBP), static_cast<uint16_t>(gender == 0), 0);
        ToolTile_SetTransformFromYaw_3870(reinterpret_cast<uintptr_t>(modelIBP), pos, 0.0f);
    }

    // 4. gender
    *reinterpret_cast<int32_t*>(p + 0x38) = static_cast<int32_t>(gender);

    // 5. random name -> self+0x28 (faithful: temp String, operator=, ~String)
    char nameBuf[0x100] = {0};
    if (rng) GenerateCitizenName(rng, gender == 0, nameBuf, static_cast<int>(sizeof(nameBuf)));
    {
        System::String tmpStr(nameBuf);
        *reinterpret_cast<System::String*>(p + 0x28) = tmpStr;   // B.3.1 bridge
    }

    // 6. appearance variant 0..7 — faithful (Rand()+Rand())&7 (two draws; (a%8+b%8)&7==(a+b)&7)
    if (rng)
        *reinterpret_cast<uint32_t*>(p + 0x34) = (reinterpret_cast<System::Random*>(rng)->Rand(8u) + reinterpret_cast<System::Random*>(rng)->Rand(8u)) & 7;   // B.3.4b bridge

    // 7. employment: unemployed unless forced or the world "start-employed" flag (bit 9) is set
    int64_t desc = *reinterpret_cast<int64_t*>(p + 0x288);
    uint32_t worldFlags = world ? *reinterpret_cast<uint32_t*>(reinterpret_cast<uint8_t*>(world) + 0xd0) : 0;
    if (forceEmploy == 0 && ((worldFlags >> 9) & 1) == 0) {
        *(p + 0x220) = 0;
        *reinterpret_cast<int32_t*>(p + 0x210) = 0;
    } else {
        *(p + 0x220) = 1;
        *reinterpret_cast<int32_t*>(p + 0x210) = desc ? *reinterpret_cast<int32_t*>(desc + 0x118) : 0;
    }

    // 8. age — a FLOAT in retail (the field the census and the adult-age gate compare
    // against desc+0x44). This argument was long declared `int`, which only worked
    // because every caller passed literal 0 (same bit pattern as 0.0f); the founding
    // spawn port (open H) passes real ages, so the typedef trap is now fixed at every
    // declaration (see NEXT_SESSION_BRIEF.md section 4).
    *reinterpret_cast<float*>(p + 0x3c) = age;

    // 9. link into family (parent IBP dereffed once, as retail does)
    {
        int64_t parentVal = parentIBP ? *parentIBP : 0;
        CitizenComp_sub_1fa0(selfV, &parentVal);
    }

    // 10. age-mesh setup
    if (isElder == 0) { CitizenComponent_SetupYoung_Reimpl(reinterpret_cast<void*>(selfV)); }
    else              { CitizenComponent_SetupElder_Reimpl(reinterpret_cast<void*>(selfV)); }

    // 11. housing: (re)assign unless spawn-key and housing IBP already match
    uint64_t housingIBP = 0;
    new (&housingIBP) ::Object::InternalBasePtr(nullptr);   // B.3.3 bridge
    bool needAssign = (*reinterpret_cast<int64_t*>(p + 0x1f0) != spawnKey);
    if (!needAssign) {
        needAssign = (*reinterpret_cast<int64_t*>(p + 0x248) != static_cast<int64_t>(housingIBP));
    }
    if (needAssign) {
        uint64_t arg = housingIBP;
        CitizenComp_AssignHousing(reinterpret_cast<CitizenComponent*>(selfV),
                                  reinterpret_cast<int64_t*>(&arg));
        *reinterpret_cast<int64_t*>(p + 0x1f0) = spawnKey;
    }

    // 12. lifespan = RandFloatTol(rng) * desc+0x3c (age span) + desc+0x38 (base age)
    float rf    = rng ? fn_Random_RandFloatTol(rng) : 0.0f;
    float baseA = desc ? *reinterpret_cast<float*>(desc + 0x38) : 0.0f;
    float spanA = desc ? *reinterpret_cast<float*>(desc + 0x3c) : 0.0f;
    *reinterpret_cast<float*>(p + 0x40) = rf * spanA + baseA;

    // 13. sprite/anim init on the AnimalComponent (+0x278)
    ActMv_SetAnimation_a1f0(*reinterpret_cast<void**>(p + 0x278), 0,
                            kRdata_14017214c, 0.0f);
}

// [11] 0x1401140f0 — the LOAD-TIME visual re-apply (clothing colour, age scale,
// carried tool), dispatched by the load-only EntityList pass.
// ============================================================
// CitizenComponent vtable slot 11 (+0x58) — retail 0x1401140f0
// ============================================================
// ⚠ NOT an update hook, despite this project's "OnUpdate2" naming for slot 11.
// It is the LOAD-TIME RE-APPLY, dispatched once per component by the load-only
// EntityList pass FUN_1400c4730 (ported as EntityList_PostLoad_c4730), which
// GameUpdateLoop::Serialize (0x140095a90) calls at 0x140095d3f on the LOAD leg.
//
// It is the ONLY thing that restores a citizen's age scale and CLOTHING COLOUR:
// SetupYoung/Adult/Elder write MeshInstance +0x10c..+0x114 (scale) and +0x118 =
// (float)citizen+0x34 (the colour index, itself `(Rand()+Rand()) & 7` at spawn).
// Graphics::MeshInstance::Serialize (Runtime 0x1800ff680) serializes ONLY the
// AnimationState at +0xd8 and the +0xc8/+0xd0 array — it does NOT round-trip the
// visual-scale Vector4 — and ModelComponent::Shutdown (0x140124b70) does not
// either. So after a load the colour MUST be re-applied, and this is what does it.
// It also re-shows the carried-tool mesh when the restored toolId (+0x1d8) != -1.
//
// Symptom while unwired: citizens come back with the WRONG clothes colours after a
// load, and only some of them. The mix is the giveaway: CitizenAIComponent::Update
// (0x14010f6a0) reaches CheckAge -> SetupAgeMesh only for an IDLE citizen — its tail
// is `if (charComp+0x60 >= 0) return;` where +0x60 is the action-stack top — so the
// few idle-on-that-tick citizens self-heal and the busy majority keep the
// MeshInstance ctor default, drifting back one at a time as they go idle.
//
// Byte-faithful to retail, including the unguarded +0x288 description deref.
void CitizenComponent::Slot11() {
    uint8_t* p = reinterpret_cast<uint8_t*>(this);
    if (!p) return;

    const int64_t desc   = *reinterpret_cast<int64_t*>(p + 0x288);
    const float   thresh = desc ? *reinterpret_cast<float*>(desc + 0x44) : 0.0f;
    const float   age    = *reinterpret_cast<float*>(p + 0x3c);
    const bool    adult  = (*reinterpret_cast<int64_t*>(p + 0x208) != 0);

    if (!adult) {
        if (age < thresh) { CitizenComponent_SetupElder_Reimpl(p); }
        else              { CitizenComponent_SetupYoung_Reimpl(p); }
    } else {
        CitizenComponent_SetupAdult_Reimpl(p);
    }

    // ⚠ KNOWN DEVIATION. Retail's 0x1401140f0 unconditionally re-shows the carried tool
    // here (Entity_impl_73b0 0x1401173b0) when the restored toolId != -1. Our port of
    // 73b0 CRASHES on that path at post-load time: it walks the tool description chain
    // (MeshInstance+0x28 -> deref -> deref) and faults on a READ of 0x100000001, i.e.
    // that link is not yet valid when the load-only slot-11 pass runs. 73b0's own header
    // already notes this is "the crash-prone path the ShowTool call site deliberately
    // guards off", so the gap is in 73b0, not here. Restore the re-show once 73b0's
    // description lookup is fixed.
}
