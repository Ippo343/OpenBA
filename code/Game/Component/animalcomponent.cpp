// code/Game/Component/animalcomponent.cpp
// Genuine C++ reimplementation of the AnimalComponent per-animal simulation —
// the deer/livestock wander + aging + death pathway.
//
// AnimalComponent is the per-creature component. Its retail vtable (0x140162110)
// overrides six slots; the four this file implements are:
//
//   [4]  Startup      0x140106d50  seed the death age from the description
//   [6]  OnAdded      0x140106c20  latch the spawn cell, size the wander box,
//                                  then run one Update(t=0) through slot 9
//   [9]  Update       0x140106420  the 452-instruction per-tick body
//   [11] OnUpdate2    0x140106dd0  (not ported — see "Deferred" below)
//
// plus the three private helpers Update leans on:
//
//   AnimalComponent_FindHabitat     0x140106020  expanding ring scan for a free
//                                                terrain-type-2 (grass) cell
//   AnimalComponent_SetWanderTimer  0x140106250  play an idle animation, arm the
//                                                move timer, pick a facing
//   AnimalComponent_UpdateBehavior  0x140107320  age -> mesh visual scale
//
// AnimalComponent_SetBounds (0x140105e20) is NOT re-ported here: it already has a
// faithful port as PastureComponent_AnimalSetBounds (pasturecomponent.cpp) and is
// registered by reg_component_gap_D.cpp:71, so we call it through ResolveAddr.
//
// ---- Struct fields (size 0x58, ctor 0x140106fc0) --------------------------
//   +0x10 Entity IBP (owner)          +0x18 AnimalDescription (Object::Base)
//   +0x20 component flag word         +0x28 int32  habitat cell index
//   +0x30 int64 Time  move timer      +0x38 short  wander box x0
//   +0x3a short wander box y0         +0x3c short  wander box width
//   +0x3e short wander box height     +0x40 uint8  "wanders" enable
//   +0x44 float age (hours)           +0x48 float  age of death (hours)
//   +0x50 int64 Time  graze timer
//
//   flag bits read by Update: 20 = penned/immobile (skip pathing entirely),
//   21 = graze latch (set once the graze timer first expires), 22 = habitat is
//   revalidated each repath, 23 = grazing enabled.
//
// ---- AnimalDescription fields (type id DAT_1401c6fd0) ---------------------
//   +0x20 int   wander box size (tiles)   +0x28 float lifespan base (hours)
//   +0x2c float lifespan variance          +0x30 float adult age (hours)
//   +0x34 float graze interval (hours)     +0x3c uint8 picks a random facing
//   +0x40 float wander retry interval (hours)
//
// ---- CharacterComponent fields (found via 0x1400012d0) -------------------
//   +0x18 CharacterDescription   +0x28 uint8 facing-override
//   +0x2c..+0x34 facing vector   +0x3c float heading
//   +0x48 action stack           +0x60 int stack-top index (-1 == empty)
//   +0x68 ModelComponent IBP -> +0x134..+0x140 Vector4 visual scale,
//                               +0x158/+0x15c/+0x160 world position
//
// Decompiled from Application-x64.exe using Ghidra.

#include "Component/animalcomponent_funcs.h"
#include "Component/coll_bridge.h"
#include "Component/AnimalDescription.h"      // ENDGAME B.3.9 C1: 0x1401c6fd0 cell class
#include "Component/CharacterDescription.h"   // ENDGAME B.3.9 C1: 0x1401c70b0 cell class
#include "engine_funcs.h"
#include "resolve.h"
#include "math/Math.h"
#include "system/Value.h"   // ENDGAME B.3.4b System caller batch
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B

// ============================================================
// .rdata float constants (exact retail bit patterns, verified by reading
// Application-x64.exe .rdata through Ghidra). Each is also looked up via
// ResolveAddr so a registered override wins; the literal is the fallback.
// ============================================================
namespace {

inline float BitsToFloat(uint32_t bits) {
    float f;
    std::memcpy(&f, &bits, sizeof f);
    return f;
}

// 0x1401720b0 = 0x3991A2B4 ~ 1/3600 — seconds -> age-hours conversion.
// 0x14017225c = 0x45610000 = 3600.0  — age-hours -> seconds.
// 0x14017210c = 0x3EA8F5C3 = 0.33    — visual scale of a newborn.
// 0x140172120 = 0x3F2B851E = 0.67    — visual scale gained by adulthood.
// 0x14017214c = 0x3F800000 = 1.0     — visual scale ceiling.
// 0x140172110 = 0x3ECCCCCD = 0.4     — wander destination jitter radius.
// 0x140172114 = 0x3F000000 = 0.5     — cell-centre bias.
// 0x140172280 = 0xBF800000 = -1.0    — MoveTo "use the mover's own speed".
const float* s_pfAgeRate    = nullptr;  // 0x1401720b0
const float* s_pfHoursToSec = nullptr;  // 0x14017225c
const float* s_pfScaleBase  = nullptr;  // 0x14017210c
const float* s_pfScaleGain  = nullptr;  // 0x140172120
const float* s_pfOne        = nullptr;  // 0x14017214c
const float* s_pfJitter     = nullptr;  // 0x140172110
const float* s_pfHalfCell   = nullptr;  // 0x140172114
const float* s_pfMoveSpeed  = nullptr;  // 0x140172280

inline float AgeRate()    { return s_pfAgeRate    ? *s_pfAgeRate    : BitsToFloat(0x3991A2B4); }
inline float HoursToSec() { return s_pfHoursToSec ? *s_pfHoursToSec : 3600.0f; }
inline float ScaleBase()  { return s_pfScaleBase  ? *s_pfScaleBase  : BitsToFloat(0x3EA8F5C3); }
inline float ScaleGain()  { return s_pfScaleGain  ? *s_pfScaleGain  : BitsToFloat(0x3F2B851E); }
inline float ScaleMax()   { return s_pfOne        ? *s_pfOne        : 1.0f; }
inline float Jitter()     { return s_pfJitter     ? *s_pfJitter     : BitsToFloat(0x3ECCCCCD); }
inline float HalfCell()   { return s_pfHalfCell   ? *s_pfHalfCell   : 0.5f; }
inline float MoveSpeed()  { return s_pfMoveSpeed  ? *s_pfMoveSpeed  : -1.0f; }

// --- registered exe sub-callees -------------------------------------------
typedef void* (*fn_FindCharacter_t)(int64_t entity, void* outIBP);          // 0x1400012d0
typedef void* (*fn_FindModel_t)(int64_t entity, void* outIBP);              // 0x140003f60
typedef void  (*fn_MarkForRemoval_t)(void* entity);                         // 0x1400c2ea0
// 0x140003110 was Collection::Array<void*>::SetSize(n, fill) monomorphised into
// the exe (ENDGAME B.3.2 step 2); ActHunt_DynArraySetCount_3110 confirmed the
// 3-arg fill form matches SetSizeCore exactly, so the call site below now goes
// through GameColl::AsArray<GameColl::Ptr8>(...).SetSizeFill(n, &entity).
typedef void  (*fn_ModifyFlags_t)(void* comp, uint32_t orMask,
                                  uint32_t andNotMask);                     // 0x140118950
typedef void  (*fn_SetBounds_t)(void* self, short* x, short* y,
                                short* w, short* h, int size);              // 0x140105e20
typedef void  (*fn_SetAnimation_t)(uintptr_t model, uint32_t animId,
                                   float a, float b, float c);              // 0x1401250f0
typedef void  (*fn_ActStackClear_t)(int64_t* stackAt48);                    // 0x1400c5400
typedef void  (*fn_ActStackPush_t)(int64_t* stackAt48, void* srcIBP);       // 0x1400c54e0
typedef void* (*fn_MoveToCtor_t)(void* mem, void* moverIBP, uint32_t pathType,
                                 int a4, float* dest, int a6, int a7,
                                 float speed);                              // 0x1400c9170

fn_FindCharacter_t     s_fnFindCharacter  = nullptr;
fn_FindModel_t         s_fnFindModel      = nullptr;
fn_MarkForRemoval_t    s_fnMarkForRemoval = nullptr;
fn_ModifyFlags_t       s_fnModifyFlags    = nullptr;
fn_SetBounds_t         s_fnSetBounds      = nullptr;
fn_SetAnimation_t      s_fnSetAnimation   = nullptr;
fn_ActStackClear_t     s_fnActStackClear  = nullptr;
fn_ActStackPush_t      s_fnActStackPush   = nullptr;
fn_MoveToCtor_t        s_fnMoveToCtor     = nullptr;


bool s_helpersResolved = false;

} // namespace

namespace {

void EnsureHelpers() {
    if (s_helpersResolved) return;
    s_helpersResolved = true;


    s_fnFindCharacter  = reinterpret_cast<fn_FindCharacter_t >(ResolveAddr(Module::Exe, 0x1400012d0));
    s_fnFindModel      = reinterpret_cast<fn_FindModel_t     >(ResolveAddr(Module::Exe, 0x140003f60));
    s_fnMarkForRemoval = reinterpret_cast<fn_MarkForRemoval_t>(ResolveAddr(Module::Exe, 0x1400c2ea0));
    s_fnModifyFlags    = reinterpret_cast<fn_ModifyFlags_t   >(ResolveAddr(Module::Exe, 0x140118950));
    s_fnSetBounds      = reinterpret_cast<fn_SetBounds_t     >(ResolveAddr(Module::Exe, 0x140105e20));
    s_fnSetAnimation   = reinterpret_cast<fn_SetAnimation_t  >(ResolveAddr(Module::Exe, 0x1401250f0));
    s_fnActStackClear  = reinterpret_cast<fn_ActStackClear_t >(ResolveAddr(Module::Exe, 0x1400c5400));
    s_fnActStackPush   = reinterpret_cast<fn_ActStackPush_t  >(ResolveAddr(Module::Exe, 0x1400c54e0));
    s_fnMoveToCtor     = reinterpret_cast<fn_MoveToCtor_t    >(ResolveAddr(Module::Exe, 0x1400c9170));


    s_pfAgeRate    = reinterpret_cast<const float*>(ResolveAddr(Module::Exe, 0x1401720b0));
    s_pfHoursToSec = reinterpret_cast<const float*>(ResolveAddr(Module::Exe, 0x14017225c));
    s_pfScaleBase  = reinterpret_cast<const float*>(ResolveAddr(Module::Exe, 0x14017210c));
    s_pfScaleGain  = reinterpret_cast<const float*>(ResolveAddr(Module::Exe, 0x140172120));
    s_pfOne        = reinterpret_cast<const float*>(ResolveAddr(Module::Exe, 0x14017214c));
    s_pfJitter     = reinterpret_cast<const float*>(ResolveAddr(Module::Exe, 0x140172110));
    s_pfHalfCell   = reinterpret_cast<const float*>(ResolveAddr(Module::Exe, 0x140172114));
    s_pfMoveSpeed  = reinterpret_cast<const float*>(ResolveAddr(Module::Exe, 0x140172280));
}

// --- small accessors -------------------------------------------------------
inline uint8_t* Raw(void* p) { return reinterpret_cast<uint8_t*>(p); }

template <typename T>
inline T& At(void* base, ptrdiff_t off) {
    return *reinterpret_cast<T*>(Raw(base) + off);
}
template <typename T>
inline T& At(uintptr_t base, ptrdiff_t off) {
    return *reinterpret_cast<T*>(base + off);
}

inline uintptr_t GameWorld() {
    return (g_pGameWorldSingleton && *g_pGameWorldSingleton)
               ? reinterpret_cast<uintptr_t>(*g_pGameWorldSingleton)
               : 0;
}

// The map grid lives at gameWorld+0xf8. Its cell array is at grid+0x80 (Ghidra
// renders that as `*(int*)(grid + 0x20)` because the pointer is typed int* —
// ELEMENTS, not bytes). Cell stride is 0x30: +0x00 occupant IBP, +0x29 terrain
// type (0x02 == the grass an animal will stand on).
inline int* MapGrid() {
    uintptr_t w = GameWorld();
    return w ? *reinterpret_cast<int**>(w + 0xf8) : nullptr;
}
inline uintptr_t CellArray(int* grid) {
    return *reinterpret_cast<uintptr_t*>(reinterpret_cast<uintptr_t>(grid) + 0x80);
}
inline uint8_t CellTerrain(uintptr_t cells, int64_t index) {
    return *reinterpret_cast<uint8_t*>(cells + index * 0x30 + 0x29);
}
inline int64_t CellOccupant(uintptr_t cells, int64_t index) {
    return *reinterpret_cast<int64_t*>(cells + index * 0x30);
}
inline void* Rng() {
    uintptr_t w = GameWorld();
    return w ? *reinterpret_cast<void**>(w + 0xd8) : nullptr;
}

// Resolve self->_description as an AnimalDescription (the retail IsDerivedFrom guard).
inline void* AnimalDesc(AnimalComponent* self) {
    void* raw = At<void*>(self, 0x18);
    if (!raw || !fn_IsDerivedFrom) return nullptr;
    return fn_IsDerivedFrom(raw, AnimalDescription::TypeId()) ? raw : nullptr;
}

inline int ClampCell(int v, int dim) {
    if (v < 0) v = 0;
    if (v >= dim) v = dim - 1;
    return v;
}

// --- rate-limited liveness tracing (stderr -> standalone_error.log) ---------
int s_traceFirstUpdate = 0;
int s_traceDeath       = 0;
int s_traceMoveTo      = 0;
int s_traceMissing     = 0;
constexpr int kTraceCap = 24;

} // namespace

// ============================================================
// AnimalComponent::Startup (vtable slot [4], 0x140106d50)
// ------------------------------------------------------------
// Rolls this animal's age of death once, at creation:
//   deathAge = desc->lifespanBase + RandFloatTol() * desc->lifespanVariance
// RandFloatTol is symmetric in [-1,1), so the lifespan straddles the base.
//
// This is load-bearing for Update: the inert factory zeroes the body, so an
// animal that never runs Startup has deathAge == 0 and dies on its first tick.
// ============================================================

// ============================================================
// AnimalComponent_FindHabitat (0x140106020)
// ------------------------------------------------------------
// Expanding-ring search around the animal's current cell for a free grass tile.
// Ring side length grows 5, 7, 9, ... and only the ring's PERIMETER is tested
// (top+bottom rows first, then the left+right columns of the interior). Gives up
// at side length > 30 and returns the original cell unchanged.
//
// A candidate qualifies when terrain == 0x02 AND the cell has no occupant.
// ============================================================
int AnimalComponent_FindHabitat(AnimalComponent* self) {
    EnsureHelpers();

    int* grid = MapGrid();
    if (!self || !grid) return 0;

    const int startCell = At<int>(self, 0x28);
    int side = 3;

    for (;;) {
        int dim = *grid;
        side += 2;
        if (dim <= 0) return startCell;

        const short sideS = static_cast<short>(side);
        const short half  = static_cast<short>(side / 2);
        short y0 = static_cast<short>(startCell / dim) - half;
        short x0 = static_cast<short>(startCell % dim) - half;
        if (x0 < 0) x0 = 0;
        if (y0 < 0) y0 = 0;

        // Width and height are clamped independently, both against the unclamped
        // side length (retail keeps `side` live in the register while writing the
        // clamped width to a second one).
        short w = sideS;
        if (dim <= sideS + static_cast<int>(x0)) w = static_cast<short>(static_cast<short>(dim) - x0) - 1;
        short h = sideS;
        if (dim <= sideS + static_cast<int>(y0)) h = static_cast<short>(static_cast<short>(dim) - y0) - 1;

        const int xMax = static_cast<int>(x0) + w - 1;
        const int yMax = static_cast<int>(y0) + h - 1;

        const uintptr_t cells = CellArray(grid);
        if (!cells) return startCell;

        // Top and bottom edges.
        for (int x = x0; x <= xMax; ++x) {
            int64_t top = static_cast<int64_t>(y0) * *grid + x;
            if (CellTerrain(cells, top) == 0x02 && CellOccupant(cells, top) == 0)
                return static_cast<int>(y0) * *grid + x;
            int64_t bottom = static_cast<int64_t>(*grid) * yMax + x;
            if (CellTerrain(cells, bottom) == 0x02 && CellOccupant(cells, bottom) == 0)
                return *grid * yMax + x;
        }

        // Left and right edges, interior rows only.
        for (int y = static_cast<int>(y0) + 1; y <= yMax - 1; ++y) {
            int64_t left = static_cast<int64_t>(*grid) * y + x0;
            if (CellTerrain(cells, left) == 0x02 && CellOccupant(cells, left) == 0)
                return static_cast<int>(x0) + *grid * y;
            int64_t right = static_cast<int64_t>(*grid) * y + xMax;
            if (CellTerrain(cells, right) == 0x02 && CellOccupant(cells, right) == 0)
                return y * *grid + xMax;
        }

        if (side > 0x1e) return startCell;
    }
}

// ============================================================
// AnimalComponent_UpdateBehavior (0x140107320)
// ------------------------------------------------------------
// Maps age onto the mesh's visual scale so a fawn grows into a deer:
//   scale = min(1.0, age / desc->adultAge * 0.67 + 0.33)
// The scalar is broadcast into a Vector4 and stored at MeshInstance+0x134.
// ============================================================
void AnimalComponent_UpdateBehavior(AnimalComponent* self) {
    EnsureHelpers();
    if (!self) return;

    void* desc = AnimalDesc(self);
    if (!desc) return;                       // retail trusts the type check; see notes

    const float adultAge = At<float>(desc, 0x30);
    if (adultAge == 0.0f) return;

    float scale = (At<float>(self, 0x44) / adultAge) * ScaleGain() + ScaleBase();
    if (scale > ScaleMax()) scale = ScaleMax();

    int64_t entity = At<int64_t>(self, 0x10);
    if (!entity || !s_fnFindCharacter) return;

    int64_t character = 0;
    s_fnFindCharacter(entity, &character);
    if (!character) return;

    int64_t model = At<int64_t>(static_cast<uintptr_t>(character), 0x68);
    if (!model) return;

    const Math::Scalar scaleS(scale);
    Math::Vector4 vec4(scaleS);   // Vector4(const Scalar&) splat, slot 0x140152958
    At<float>(static_cast<uintptr_t>(model), 0x134) = vec4.x;
    At<float>(static_cast<uintptr_t>(model), 0x138) = vec4.y;
    At<float>(static_cast<uintptr_t>(model), 0x13c) = vec4.z;
    At<float>(static_cast<uintptr_t>(model), 0x140) = vec4.w;
}

// ============================================================
// AnimalComponent_SetWanderTimer (0x140106250)
// ------------------------------------------------------------
// The "stand still for a while" path. Plays idle animation `animId` on the
// model, arms the move timer at +0x30 with `seconds`, and — when the description
// says this species picks a random facing (desc+0x3c) — points the character at a
// random normalized direction and records the matching heading.
//
// Note the retail double-normalize: the random 2D vector is normalized into a
// scratch, then the scratch's XY are re-normalized into the character's facing.
// Reproduced as-is (the second pass is a no-op on an already-unit 2D vector).
// ============================================================
void AnimalComponent_SetWanderTimer(AnimalComponent* self, uint32_t animId, float seconds) {
    EnsureHelpers();
    if (!self) return;

    int64_t entity = At<int64_t>(self, 0x10);
    if (!entity || !s_fnFindCharacter || !s_fnFindModel) return;

    int64_t character = 0;
    s_fnFindCharacter(entity, &character);
    int64_t model = 0;
    s_fnFindModel(entity, &model);

    if (!character) return;

    void* charDescRaw = At<void*>(static_cast<uintptr_t>(character), 0x18);
    void* charDesc = nullptr;
    if (charDescRaw && fn_IsDerivedFrom &&
        fn_IsDerivedFrom(charDescRaw, CharacterDescription::TypeId())) {
        charDesc = charDescRaw;
    }

    if (s_fnSetAnimation && model && charDesc) {
        s_fnSetAnimation(static_cast<uintptr_t>(model), animId, 0.0f,
                         At<float>(charDesc, 0x2c), ScaleMax());
    }

    // System::Time::Time writes EIGHT bytes — the destination must be int64-sized.
    int64_t timeVal = 0;
    if (fn_Time_Ctor_Float) fn_Time_Ctor_Float(&timeVal, seconds);
    At<int64_t>(self, 0x30) = timeVal;

    void* desc = AnimalDesc(self);
    if (!desc || At<uint8_t>(desc, 0x3c) == 0) return;

    void* rng = Rng();
    if (!rng) return;

    const float r1 = fn_Random_RandFloatTol(rng)   /* B.3.4b bridge */;
    const float r0 = fn_Random_RandFloatTol(rng)   /* B.3.4b bridge */;
    const Math::Vector3 src(r0, r1, 0.0f);

    Math::Vector3 dir = src.Normalize();

    At<uint8_t>(static_cast<uintptr_t>(character), 0x28) = 0;

    const Math::Vector3 src2(dir.x, dir.y, 0.0f);
    Math::Vector3 facing = src2.Normalize();
    At<float>(static_cast<uintptr_t>(character), 0x2c) = facing.x;
    At<float>(static_cast<uintptr_t>(character), 0x30) = facing.y;
    At<float>(static_cast<uintptr_t>(character), 0x34) = facing.z;

    At<float>(static_cast<uintptr_t>(character), 0x3c) = dir.Heading();
}

// ============================================================
// AnimalComponent::Update (vtable slot [9], 0x140106420)
// ------------------------------------------------------------
//  1. Age the animal; past its rolled death age, mark the entity for removal and
//     append it to the world's corpse list (gameWorld+0xe8, array at +0xd0 /
//     count at +0xd8). Bit 10 of the entity's flag word means "already removed".
//  2. Graze latch: while flag bit 21 is clear and bit 23 is set, run down the
//     graze timer at +0x50. A juvenile (age <= adultAge) or an already-running
//     timer just ticks; an adult with an idle timer re-arms it to a randomized
//     (1 + rand) * desc->grazeInterval hours. On expiry, set bit 21 (the latch)
//     and zero the timer.
//  3. Refresh the visual scale from age.
//  4. Bail if the character already has a queued action, or if the move timer at
//     +0x30 has not run out yet.
//  5. Pick a destination. Penned animals (bit 20) never path. Otherwise: if bit
//     22 is set, revalidate the habitat cell (must be terrain 2 with no building)
//     and re-size the wander box. Then, if the animal has left its box OR a 1-in-4
//     roll comes up, search for a new destination — up to 100 tries of a random
//     tile inside the box, terrain-resolved and jittered by +/-0.4, re-running
//     FindHabitat every 11 consecutive misses. On success, clear the action stack
//     and push a 0x480-byte MoveTo toward the point; on failure, idle via
//     SetWanderTimer.
// ============================================================

// ============================================================
// AnimalComponent::OnAdded (vtable slot [6], 0x140106c20)
// ------------------------------------------------------------
// Register-time one-shot, dispatched by ComponentEntity_OnAddedPass. Only runs
// when flag bit 22 is set. Latches the spawn tile from the model's world
// position, sizes the wander box from desc->wanderSize, then runs a single
// Update with a zero delta (retail calls vtable+0x48, i.e. slot 9).
// ============================================================

// ============================================================
// Init: resolve function pointers
// ============================================================
void AnimalComponent_InitFuncs() {
    EnsureHelpers();
#ifdef _DEBUG
    printf("[AnimalComponent] find=%p moveTo=%p setBounds=%p\n",
           reinterpret_cast<void*>(s_fnFindCharacter),
           reinterpret_cast<void*>(s_fnMoveToCtor),
           reinterpret_cast<void*>(s_fnSetBounds));
#endif
}

// ============================================================================
// ENDGAME B.3.8 CIT — AnimalComponent as a REAL class (AnimalComponent.h).
//
// Before: a 48-slot hand-built AnimalComponent_LiveVtable filled at every create
// (slots 3 Serialize, 4 Startup, 6 OnAdded, 9 Update), Inert_ScalarDtor in slot
// 0. After: the compiler's 16-slot vtable and a real ctor/dtor chain (B38_RULES
// D1, D2).
// ============================================================================
#include "Object/factory_registration.h"
#include "Component/AnimalComponent.h"


// 0x140106fc0. Component() is the CALL 0x1401188b0; the 0x140162110 stamp is
// the compiler's; +0x28 (dword), +0x30, +0x38, +0x40 (byte), +0x44 (qword),
// +0x50 = 0; flags |= 0xc9.
AnimalComponent::AnimalComponent()
    : data28(0), _pad2C{}, data30(0), data38(0), data40(0), _pad41{},
      data44(0), data48(0), _pad4C{}, data50(0) {
    OrFlagsInCtor(0xc9);
}

void AnimalComponent::Startup() {
    EnsureHelpers();

    void* desc = AnimalDesc(this);
    void* rng  = Rng();
    if (!desc || !rng) {
        if (s_traceMissing++ < kTraceCap) {
            fprintf(stderr, "[AnimalUpd] Startup skipped: desc=%p rng=%p this=%p\n",
                    desc, rng, static_cast<void*>(this));
        }
        return;
    }

    float tol = fn_Random_RandFloatTol(rng)   /* B.3.4b bridge */;
    At<float>(this, 0x48) = tol * At<float>(desc, 0x2c) + At<float>(desc, 0x28);
}
void AnimalComponent::OnAdded() {
    EnsureHelpers();

    const uint32_t flags = At<uint32_t>(this, 0x20);
    if (((flags >> 0x16) & 1) == 0) return;

    int64_t entity = At<int64_t>(this, 0x10);
    if (!entity || !s_fnFindCharacter) return;

    int64_t character = 0;
    s_fnFindCharacter(entity, &character);
    if (!character) return;

    const int64_t model = At<int64_t>(static_cast<uintptr_t>(character), 0x68);
    if (!model) return;

    int* grid = MapGrid();
    if (!grid) return;
    const int dim = *grid;
    if (dim <= 0) return;

    const float half = static_cast<float>(dim >> 1);
    const int x = ClampCell(static_cast<int>(At<float>(static_cast<uintptr_t>(model), 0x158) + half), dim);
    const int y = ClampCell(static_cast<int>(At<float>(static_cast<uintptr_t>(model), 0x15c) + half), dim);
    At<int>(this, 0x28) = dim * y + x;

    void* desc = AnimalDesc(this);
    if (s_fnSetBounds && desc) {
        s_fnSetBounds(this,
                      reinterpret_cast<short*>(Raw(this) + 0x38),
                      reinterpret_cast<short*>(Raw(this) + 0x3a),
                      reinterpret_cast<short*>(Raw(this) + 0x3c),
                      reinterpret_cast<short*>(Raw(this) + 0x3e),
                      At<int>(desc, 0x20));
    }

    // Retail: System::Time::Time(&t, 0.0f) then (*vtable[9])(this, &t). Slot 9 is
    // this component's own Update.
    int64_t zeroTime = 0;
    if (fn_Time_Ctor_Float) fn_Time_Ctor_Float(&zeroTime, 0.0f);
    Update(&zeroTime);   // slot 9, virtual as retail
}

// [9] 0x140106420. RDX is a pointer to the elapsed System::Time (EntityList
// bucket tick 0x1400c42af).
void AnimalComponent::Update(void* time) {
    int64_t* deltaTime = static_cast<int64_t*>(time);
    EnsureHelpers();
    if (!deltaTime) return;

    // ---- 1. aging + death ------------------------------------------------
    const float dtSeconds = reinterpret_cast<const System::Time*>(deltaTime)->AsSeconds();   // B.3.4b bridge
    const float prevAge = At<float>(this, 0x44);
    const float age = dtSeconds * AgeRate() + prevAge;
    At<float>(this, 0x44) = age;

    const float deathAge = At<float>(this, 0x48);
    if (age >= deathAge && age != deathAge) {
        int64_t entity = At<int64_t>(this, 0x10);
        const uintptr_t world = GameWorld();
        const int64_t corpseOwner = world ? At<int64_t>(world, 0xe8) : 0;
        if (entity == 0) return;

        if ((At<uint32_t>(static_cast<uintptr_t>(entity), 0x40) >> 10) & 1) return;

        if (s_fnMarkForRemoval) s_fnMarkForRemoval(reinterpret_cast<void*>(entity));
        if (corpseOwner) {
            // B.3.2 bridge: exe 0x140003110 (ActHunt_DynArraySetCount_3110) IS
            // Collection::Array<void*>::SetSize(n, fill).
            const int newCount = At<int>(static_cast<uintptr_t>(corpseOwner), 0xd8) + 1;
            GameColl::AsArray<GameColl::Ptr8>(reinterpret_cast<void*>(corpseOwner + 0xd0))
                .SetSizeFill(newCount, &entity);
        }
        // Running total, not a capped sample: at kTraceCap=24 this read as
        // "24 deaths" against a 47-animal drain and hid the fact that old age
        // accounts for the whole thing. Print a total every 10.
        ++s_traceDeath;
        if (s_traceDeath <= kTraceCap || (s_traceDeath % 10) == 0) {
            fprintf(stderr, "[AnimalUpd] death #%d: this=%p entity=%p age=%.2f deathAge=%.2f\n",
                    s_traceDeath, static_cast<void*>(this),
                    reinterpret_cast<void*>(entity), age, deathAge);
        }
        return;
    }

    void* rng  = Rng();
    void* desc = AnimalDesc(this);
    if (!desc) {
        if (s_traceMissing++ < kTraceCap) {
            fprintf(stderr, "[AnimalUpd] no AnimalDescription on this=%p (raw=%p) — tick skipped\n",
                    static_cast<void*>(this), At<void*>(this, 0x18));
        }
        return;
    }

    // prevAge == 0 only on this animal's very first tick (Startup seeds +0x48,
    // never +0x44, and the inert factory zeroes the body).
    if (prevAge == 0.0f && s_traceFirstUpdate++ < kTraceCap) {
        fprintf(stderr, "[AnimalUpd] first tick: this=%p desc=%p deathAge=%.2f flags=0x%08x cell=%d\n",
                static_cast<void*>(this), desc, deathAge,
                At<uint32_t>(this, 0x20), At<int>(this, 0x28));
    }

    const uint32_t flags = At<uint32_t>(this, 0x20);

    // ---- 2. graze latch --------------------------------------------------
    if (At<uint8_t>(this, 0x40) != 0 && ((flags >> 0x15) & 1) == 0 && ((flags >> 0x17) & 1) != 0) {
        const int64_t timer = At<int64_t>(this, 0x50);
        float seconds = 0.0f;
        bool rearm = true;

        if (timer != 0 || age <= At<float>(desc, 0x30)) {
            if (timer < 1) {
                rearm = false;                       // nothing to run down
            } else {
                const int64_t remaining = timer - *deltaTime;
                At<int64_t>(this, 0x50) = remaining;
                if (remaining > 0) {
                    rearm = false;
                } else {
                    if (s_fnModifyFlags) s_fnModifyFlags(this, 0x200000, 0);
                    seconds = 0.0f;
                }
            }
        } else {
            const float interval = At<float>(desc, 0x34);
            const float roll = rng ? reinterpret_cast<System::Random*>(rng)->RandFloat()   /* B.3.4b bridge */ : 0.0f;
            seconds = (roll * interval + interval) * HoursToSec();
        }

        if (rearm) {
            int64_t timeVal = 0;
            if (fn_Time_Ctor_Float) fn_Time_Ctor_Float(&timeVal, seconds);
            At<int64_t>(this, 0x50) = timeVal;
        }
    }

    // ---- 3. visual scale -------------------------------------------------
    AnimalComponent_UpdateBehavior(this);

    // ---- 4. busy / timer gates -------------------------------------------
    int64_t entity = At<int64_t>(this, 0x10);
    if (!entity || !s_fnFindCharacter) return;

    int64_t character = 0;
    s_fnFindCharacter(entity, &character);
    if (!character) {
        if (s_traceMissing++ < kTraceCap) {
            fprintf(stderr, "[AnimalUpd] no CharacterComponent on entity=%p — pathing skipped\n",
                    reinterpret_cast<void*>(entity));
        }
        return;
    }

    if (At<int>(static_cast<uintptr_t>(character), 0x60) + 1 > 0) return;  // action queued

    At<int64_t>(this, 0x30) -= *deltaTime;
    if (At<int64_t>(this, 0x30) > 0) return;

    // ---- 5. destination search -------------------------------------------
    int* grid = MapGrid();
    if (!grid) return;

    // Retail re-reads the flag word here (the graze block above may have set
    // bit 21 through ModifyFlags), so do not reuse the cached copy.
    const uint32_t liveFlags = At<uint32_t>(this, 0x20);
    uint32_t wanderDir = 1;

    if (((liveFlags >> 0x14) & 1) == 0) {      // not penned
        if (((liveFlags >> 0x16) & 1) != 0) {  // habitat is revalidated per repath
            const uintptr_t cells = CellArray(grid);
            const int cell = At<int>(this, 0x28);
            int64_t occupant = cells ? CellOccupant(cells, cell) : 0;

            bool blocked = false;
            if (occupant != 0) {
                int64_t buildingIBP = 0;
                fn_FindComponent_Building(reinterpret_cast<void*>(occupant), &buildingIBP);
                // ⚠ fn_FindComponent_Building is a MISLABEL — retail 0x140015140
                // matches DWORD_1401c4b80, which is TradeComponent, not any notion of
                // "building". Verified at instruction level, and retail's own animal
                // update (0x140106420) calls exactly this function at 0x140106650.
                // So the real rule is: a TRADING POST on the tile does not disqualify
                // it; every other occupant does. That also bounds the blast radius of
                // BANISHED_LIVE_TRADE_TIDS on animal pathing — the only tiles whose
                // verdict changes when 0x1401c4b80 is registered are trading-post
                // tiles. See reference_misnamed_component_finders.
                blocked = (buildingIBP == 0);
            }

            if ((cells && CellTerrain(cells, cell) != 0x02) || blocked) {
                At<int>(this, 0x28) = AnimalComponent_FindHabitat(this);
                if (s_fnSetBounds) {
                    s_fnSetBounds(this,
                                  reinterpret_cast<short*>(Raw(this) + 0x38),
                                  reinterpret_cast<short*>(Raw(this) + 0x3a),
                                  reinterpret_cast<short*>(Raw(this) + 0x3c),
                                  reinterpret_cast<short*>(Raw(this) + 0x3e),
                                  At<int>(desc, 0x20));
                }
            }
        }

        // Current tile from the model's world position.
        const int64_t model = At<int64_t>(static_cast<uintptr_t>(character), 0x68);
        if (!model) return;

        const int dim = *grid;
        if (dim <= 0) return;
        const float halfDim = static_cast<float>(dim >> 1);
        int curX = ClampCell(static_cast<int>(At<float>(static_cast<uintptr_t>(model), 0x158) + halfDim), dim);
        int curY = ClampCell(static_cast<int>(At<float>(static_cast<uintptr_t>(model), 0x15c) + halfDim), dim);

        const short boxX = At<short>(this, 0x38);
        const short boxY = At<short>(this, 0x3a);
        const short boxW = At<short>(this, 0x3c);
        const short boxH = At<short>(this, 0x3e);

        const bool outOfBox =
            (((liveFlags >> 0x14) & 1) == 0 && curX < boxX) || curY < boxY ||
            static_cast<int>(boxW) + static_cast<int>(boxX) <= curX ||
            static_cast<int>(boxH) + static_cast<int>(boxY) <= curY;

        bool repath = outOfBox;
        if (!repath) {
            // NOTE: retail keeps the roll in the same register it later passes as
            // the idle-animation id, so a non-zero roll becomes the wander dir.
            wanderDir = rng ? reinterpret_cast<System::Random*>(rng)->Rand(4u)   /* B.3.4b bridge */ : 1;
            repath = (wanderDir == 0);
        }

        // DEVIATION (bring-up guard, not retail): Random::Rand(0) divides by the
        // range, so a wander box that was never sized — OnAdded skipped because
        // flag bit 22 is clear, or SetBounds unresolved — would fault inside the
        // search loop. Retail always has a sized box here.
        if (repath && (boxW <= 0 || boxH <= 0)) {
            if (s_traceMissing++ < kTraceCap) {
                fprintf(stderr, "[AnimalUpd] wander box unsized (w=%d h=%d) on this=%p — idling\n",
                        static_cast<int>(boxW), static_cast<int>(boxH), static_cast<void*>(this));
            }
            repath = false;
            wanderDir = 1;
        }

        if (repath) {
            uint8_t discard[8] = {0};
            if (s_fnFindModel) s_fnFindModel(At<int64_t>(this, 0x10), discard);

            const float jitter   = Jitter();
            const float halfCell = HalfCell();
            float dest[3] = {0.0f, 0.0f, 0.0f};

            int misses   = 0;   // consecutive misses since the last habitat re-find
            int attempts = 0;   // total attempts, capped at 100
            bool valid   = false;

            do {
                int* g = MapGrid();
                if (!g) return;
                const int d = *g;
                if (d <= 0) return;

                const uint32_t ry = rng ? reinterpret_cast<System::Random*>(rng)->Rand(static_cast<uint32_t>(At<short>(this, 0x3e)))   /* B.3.4b bridge */ : 0;
                const short   by = At<short>(this, 0x3a);
                const uint32_t rx = rng ? reinterpret_cast<System::Random*>(rng)->Rand(static_cast<uint32_t>(At<short>(this, 0x3c)))   /* B.3.4b bridge */ : 0;
                const int candidate =
                    static_cast<int>(rx) + At<short>(this, 0x38) +
                    (static_cast<int>(ry) + static_cast<int>(by)) * d;

                const float bias = static_cast<float>(d >> 1) - halfCell;
                float tile[3] = {static_cast<float>(candidate % d) - bias,
                                 static_cast<float>(candidate / d) - bias,
                                 0.0f};

                float world[3] = {0.0f, 0.0f, 0.0f};
                if (fn_MapGetTerrainPos) fn_MapGetTerrainPos(g, world, tile, '\0', nullptr);
                dest[0] = world[0];
                dest[1] = world[1];
                dest[2] = world[2];

                const float jy = rng ? fn_Random_RandFloatTol(rng)   /* B.3.4b bridge */ : 0.0f;
                const float jx = rng ? fn_Random_RandFloatTol(rng)   /* B.3.4b bridge */ : 0.0f;
                dest[0] += jx * jitter;
                dest[1] += jy * jitter;

                if (misses > 10) {
                    At<int>(this, 0x28) = AnimalComponent_FindHabitat(this);
                    if (s_fnSetBounds) {
                        s_fnSetBounds(this,
                                      reinterpret_cast<short*>(Raw(this) + 0x38),
                                      reinterpret_cast<short*>(Raw(this) + 0x3a),
                                      reinterpret_cast<short*>(Raw(this) + 0x3c),
                                      reinterpret_cast<short*>(Raw(this) + 0x3e),
                                      At<int>(desc, 0x20));
                    }
                    misses = 0;
                }
                ++misses;
                ++attempts;

                const int dim2 = *grid;
                const float half2 = static_cast<float>(dim2 >> 1);
                const int cx = ClampCell(static_cast<int>(dest[0] + half2), dim2);
                const int cy = ClampCell(static_cast<int>(dest[1] + half2), dim2);
                const uintptr_t cells = CellArray(grid);
                valid = cells && CellTerrain(cells, static_cast<int64_t>(cy) * dim2 + cx) == 0x02;
            } while (!valid && attempts < 100);

            // Retail re-derives the final cell from the freshly-read grid rather
            // than reusing the loop's last test.
            int* g2 = MapGrid();
            const int dim3 = g2 ? *g2 : 0;
            bool accept = false;
            if (dim3 > 0) {
                const float half3 = static_cast<float>(dim3 >> 1);
                const int fx = ClampCell(static_cast<int>(dest[0] + half3), dim3);
                const int fy = ClampCell(static_cast<int>(dest[1] + half3), dim3);
                const uintptr_t cells2 = CellArray(g2);
                accept = cells2 && CellTerrain(cells2, static_cast<int64_t>(fy) * dim3 + fx) == 0x02;
            }

            if (accept) {
                int64_t* stack = reinterpret_cast<int64_t*>(character + 0x48);
                if (s_fnActStackClear) s_fnActStackClear(stack);

                void* action = nullptr;
                void* mem = (fn_MemAlloc && g_pMemAllocSingleton)
                                ? fn_MemAlloc(g_pMemAllocSingleton, 0x480, 8)
                                : nullptr;
                if (mem && s_fnMoveToCtor) {
                    int64_t moverIBP = character;
                    action = s_fnMoveToCtor(mem, &moverIBP, 0 /*Normal*/, 1,
                                            dest, 0xf9f, 1, MoveSpeed());
                }
                // If the alloc or the ctor is unavailable, `action` stays null and
                // we push a null IBP — the same degradation retail takes when
                // MemAlloc fails (0x140106a8e: MOV RAX,RDI with RDI == 0).

                int64_t actionIBP = 0;
                if (fn_InternalBasePtr_Ctor) new (&actionIBP) ::Object::InternalBasePtr(
                    reinterpret_cast<RtObject::Internal*>(action));   // B.3.3 bridge
                if (s_fnActStackPush) s_fnActStackPush(stack, &actionIBP);
                At<int64_t>(this, 0x30) = 0;

                if (s_traceMoveTo++ < kTraceCap) {
                    fprintf(stderr, "[AnimalUpd] MoveTo: this=%p dest=(%.2f, %.2f, %.2f) attempts=%d\n",
                            static_cast<void*>(this), dest[0], dest[1], dest[2], attempts);
                }
                return;
            }

            // No walkable destination after 100 tries — idle and try again later.
            wanderDir = (rng ? reinterpret_cast<System::Random*>(rng)->Rand(3u)   /* B.3.4b bridge */ : 0) + 1;
        }
    }

    const float retry = At<float>(desc, 0x40);
    const float roll = rng ? reinterpret_cast<System::Random*>(Rng())->RandFloat()   /* B.3.4b bridge */ : 0.0f;
    AnimalComponent_SetWanderTimer(this, wanderDir, roll * retry + retry);
}

// [11] 0x140106dd0 (104 B). PORT GAP (retail 0x140106dd0) — B38 D9: not ported
// (see "Deferred" at the top of this file); the tree's table held the inert
// stub here, kept.
void AnimalComponent::Slot11() {}

// create 0x14002f020: MemAlloc(0x58, 8), then the ctor.
void* AnimalComponent::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(AnimalComponent), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) AnimalComponent());
}
// Retail's createInPlace is `Internal::CreateInPlaceObject` (returns null); the
// tree's in-place entry built the object, and that is kept (B38 B8-13).
void* AnimalComponent::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) AnimalComponent());
}

DEFINE_EXE_TYPE(AnimalComponent, "Component")
