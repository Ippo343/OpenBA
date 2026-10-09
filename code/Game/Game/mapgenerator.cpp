// code/Game/Game/mapgenerator.cpp
// Clean reimplementation of MapGenerator game logic.
//
// MapGenerator (0x178 bytes) — world map generation parameters.
// Extends Object::External (0x20 base). Vtable at 0x14015ed80.
//
// ENDGAME B.3.8 block LOOP: a REAL class (code/Game/Game/MapGenerator.h,
// field layout and retail slot map there) with a compiler-generated vtable.
// Gone: the 11-slot `g_MapGenerator_Vtable_Standalone` (two over-read guards
// past retail's 9), its trampolines and stubs, the lazy vtable resolve in the
// old Init, the lazily-resolved dtor-body pointers (a non-atomic first-call
// cache, B6-5) and the C entry points.
//
// MapGenerator defines all procedural world generation settings:
// terrain parameters, resource lists (trees, stones, ores, herbs, animals),
// biome configs, and references to description files.
//
// Decompiled from Application-x64.exe.

#include "Game/MapGenerator.h"
#include "object/External.h"
#include "object/TypeInfo.h"
#include "Game/exe_bodies.h"
#include "system/String.h"
#include "collection/Collection.h"
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch B
#include "object/Internal.h" // Object::detail::InternalMemAlloc / InternalMemFree
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <new>

// ============================================================
// MapGenerator::MapGenerator (0x1400a3030)
// ============================================================
// External() and the 0x14015ed80 stamp are the compiler's; then every member in
// declaration order, as retail builds them (B4 S4 RESOLVE). The four by-name
// refs are NamedExternalRef members: String() qword 0, generation -1, resolved
// 0 (retail 0x1400a3080 / 0x1400a312f / 0x1400a3153 / 0x1400a3177). The Strings
// +0x28 / +0x30 are String() and the InitialFamily array at +0x60 and the pools
// +0x70..+0xD8 zero both qwords of each (retail 0x1400a308e..0x1400a30eb).
// Retail zeroes +0xE0..+0x114 as seven qwords (0x1400a30f2..0x1400a311c); the
// tree writes the fourteen named fields (H7).
MapGenerator::MapGenerator()
    : _field20(nullptr),       // +0x20 ExternalPtr
      enabled(1),              // +0x38 bool, default true
      subdivisions(8),         // +0x3C grid subdivision count
      scale(2.0f),             // +0x40 terrain scale
      paramE0(0), paramE4(0), paramE8(0),
      paramEC(0.0f), paramF0(0.0f), paramF4(0.0f),
      paramF8(0), paramFC(0), param100(0), param104(0),
      param108(0), param10C(0), param110(0), param114(0),
      terrainAmplitude(6.0f),  // +0x130, after descRef1
      chunkSize(16) {}         // +0x150, after descRef2

// ============================================================
// MapGenerator::~MapGenerator (body 0x14001aea0) / DeletingDtor (slot 0)
// ============================================================
// Retail body (0x14001aea0, verified against the disassembly's CALL list —
// three String dtors, 5x b030, 1x b0a0, 1x b030, MemFree, three String dtors,
// then a tail JMP to External::~External, which is the compiler's chain here).
// That is exactly reverse member order, so the members' own destructors run it
// (B4 P2 GAME-3): descRef3 / descRef2 / descRef1 (~String of each ref's name),
// refArray7 .. refArray1 (the pool destructors below), families (+0x60),
// templateRef, _field30, _field28, then ~External.
MapGenerator::~MapGenerator() {}

// The pool destructors 0x14001b030 / 0x14001b0a0 are the String pools' own
// `~Array` (MapGenerator.h, the rows below); the InitialFamily pool's, which
// retail inlines at 0x14001af47 (`MOV RCX,[RBX+0x60]; CALL [0x140152b68]`, then
// both qwords zeroed), is that pool's `~Array` too: the row has no destroy, so
// ClearCallback is the unconditional MemFree and the three zero stores.

// ============================================================
// MapGenerator::Reconstruct (vtable +0x30, Slot6) — Reset to defaults
// ============================================================
// Original: 0x14001ae30. Destroys the object (slot 0, flags 0: no free) then
// re-constructs in place.
Object::External* MapGenerator::Reconstruct() {
    this->~MapGenerator();
    return ::new (static_cast<void*>(this)) MapGenerator();
}

// [7] 0x140001640 — the shared body `MOV RAX,[RCX]; XOR EDX,EDX; JMP [RAX]`:
// slot 0 with flags 0 (destroy, no free).
void MapGenerator::Destruct() { this->~MapGenerator(); }

// ============================================================
// MapGenerator::Serialize (vtable +0x28, Slot5)
// ============================================================
// Original: 0x1400a2980
//
// This is a very complex serialization function with:
// - ExternalPtr serialization (+0x20)
// - String serialization (+0x28, +0x30, +0x48, +0x118, +0x138, +0x158)
// - Bool/Int scalar serialization
// - 7 dynamic array resize+serialize loops with different element sizes
// - ExternalRef resolution callbacks (FUN_14001b1d0) on load
// - Complex array resize helpers (FUN_14001b270, FUN_14001b340, FUN_14001b4e0)
//
// The original (0x1400a2980) isn't in our process (we ARE the
// exe), so the FULL deserialize is reimplemented in clean C++ below. This reads
// the entire MapGenerator stream — scalar header AND every dynamic pool — so the
// start-condition data (citizen families, structures, resource/cull lists, env
// template refs) is actually present for the start-town ECS spawn.
//
// SERIALIZE RUNS MORE THAN ONCE. Object::External::Load re-invokes Serialize on an
// ALREADY-LOADED external on every ExternalFactory::FindOrLoad, so a MapGenerator
// parsed for the New Game combo box is parsed AGAIN when anything else re-requests
// StartConditions.rsc (the tutorial's ScenarioDialog does). The second pass does not
// write the pool names back, so anything this function clears before the element
// vcall is what survives. That is exactly what emptied every start-condition pool
// after a tutorial: an earlier version allocated a FRESH buffer and zeroed each
// element on every pass, and the next generated world scattered no resources and
// placed no structures ([startTown::res] name="(null)").
// Same family as CreatePlacedDescription::Serialize / _placeBitArray.
//
// Retail's shape, which MG_Resize/MG_Serialize* below reproduce:
//   * the count goes through a LOCAL seeded from the struct's current count;
//   * the resize runs ONLY on the load leg (`*(int*)(*(void**)serializer + 8) == 0`);
//   * the resize is a complete NO-OP when the new count equals the old one
//     (FUN_14001b270/340/4e0 all start `if (newCount != oldCount)`), so a re-parse
//     of the same .rsc never touches an element;
//   * only elements PAST the old count are default-initialized; existing ones —
//     including the resolved-template cache at +0x10 — are preserved verbatim;
//   * the element loop then serializes each element IN PLACE, zeroing nothing.
//
// RESOLVE DEFERRED: the original resolves each ExternalRef name to a template
// during deserialize (FUN_14001b1d0). We SKIP that here — every start-town
// spawn pass re-resolves each entry lazily via ResolveExternalByName before use
// (e.g. ComponentEntity_impl_09e0/_1fc0/_02f0, FUN_1400a12b0). So the pools just
// store the names; resolution happens at spawn time.
//
// Stream order mirrors the original 0x1400a2980 EXACTLY (note the non-monotonic
// tail: +0x118, +0x138, +0x130, +0x150, +0x158, +0x170).
namespace {
// True on the LOAD leg. The Serializer's FIRST qword is the Stream, and Stream+8
// is its mode — 0 means loading. NOT serializer+8, which is the encoding.
// (B.4 GAME-b: the old null test of that first qword is gone; the serializer
// holds the stream reference it was built from.)
bool MG_IsLoading(IO::Serializer& serializer) {
    return static_cast<int>(serializer.GetDirection()) == 0;   // IO::Serializer::GetDirection, retail 0x180006210
}

// Faithful port of the exe's three array-resize helpers: FUN_14001b270 (stride 8,
// InitialFamily), FUN_14001b340 (stride 0x18, ExternalRef) and FUN_14001b4e0
// (stride 0x30, InitialStructure). Header is {void* data@0; int count@8; int cap@0xc}.
//
// The MG_Init* functions below default-construct ONE new element; retail calls
// them only for indices at or past the old count.
//
// ENDGAME B.3.2 step 2: the one generic stride-parameterised grow that used to
// sit here (`MG_Resize`) is now three `Collection::Array<>::SetSize` calls, one
// per retail body, each driven by its own `ArraySpec` row. Every row's ops were
// re-derived from the retail bodies (decompiled 2026-09-13), not from the old
// generic copy, and three things changed as a result — all of them the old
// copy's approximations, none of them reachable on the load leg (a freshly
// constructed or Cleanup'd MapGenerator, count 0 -> n):
//   * alignment: the stride-8 body (0x14001b270) allocates at align 4, not 8.
//   * the two String-bearing rows copy-CONSTRUCT the preserved prefix and
//     destroy the old elements afterwards (retail's order) instead of moving
//     the bytes; the refcount goes +1 then -1, so the end state is identical.
//   * those rows destroy a truncated tail in place; the old copy leaked it.
// The structural arms (exact-equal early return, `_size` first, treble rule,
// copy prefix only when the block moved, init the tail, free only the moved
// block) matched arm for arm. The stride-0x18 row is the same row
// goal_exe_helpers.cpp carries as kSpec_GoalRefEntry for the same retail body
// (0x14001b340); it is repeated here because that one is file-local.
// stride 8 — InitialFamily {bool _educated@0, int _maxChildCount@4}, exe 0x14001b270.
// Retail's assign is one qword; its init writes byte 0 and dword 4, leaving
// [1,4) untouched.
using ExternalRef      = MapGenerator::ExternalRef;
using InitialFamily    = MapGenerator::InitialFamily;
using InitialStructure = MapGenerator::InitialStructure;

void CopyMGFamily(InitialFamily* d, const InitialFamily* s) {
    *d = *s;   // the eight bytes, padding included (it is a declared member)
}
void ZeroMGFamily(InitialFamily* e) {
    e->educated = false;     // byte 0
    e->maxChildCount = 0;    // dword 4
}
void AssignMGFamily(uint8_t* d, const uint8_t* s) {
    rtcoll::AssignAs<InitialFamily, &CopyMGFamily>(d, s);
}
void MG_InitFamily(uint8_t* e) {
    rtcoll::InitAs<InitialFamily, &ZeroMGFamily>(e);
}
// stays explicit: `init` writes byte 0 and dword 4 and deliberately leaves
// [1,4) untouched — an `init unwritten` extent, i.e. a probe contract, not a
// trait. (An element with zeroing NSDMIs would also write the padding.)
}  // namespace
const rtcoll::ArraySpec MapGeneratorRows::kSpec_InitialFamily = {   // exe 0x14001b270
    "Array<InitialFamily>", 8, 4,
    &AssignMGFamily, &MG_InitFamily, nullptr, /*byteCopy*/true,
    /*assign unwritten*/ 0, 0,
    /*init   unwritten*/ 1, 4,
    /*seed zero       */ 0, 0 };
namespace {

// stride 0x18 — ExternalRef {String@0, gen@8, resolved@0x10}, exe 0x14001b340.
// Neither arm writes [0x0c,0x10); the tail seed of -1 is load-bearing (see
// goal_exe_helpers.cpp).
void CopyMGRef(ExternalRef* d, const ExternalRef* s) {
    new (&d->name) System::String(s->name);   // +0x00
    d->generation = s->generation;                // +0x08 dword
    d->resolved = s->resolved;                    // +0x10 qword
}
void ZeroMGRef(ExternalRef* e) {
    e->name._data = nullptr;   // +0x00 qword 0 (raw element memory)
    e->generation = -1;          // +0x08
    e->resolved = nullptr;       // +0x10 qword 0
}
void DestroyMGRefs(ExternalRef* elems, int count) {
    for (int i = 0; i < count; ++i)
        elems[i].name.~String();
}
void AssignMGRef(uint8_t* d, const uint8_t* s) {
    rtcoll::AssignAs<ExternalRef, &CopyMGRef>(d, s);
}
void MG_InitRef(uint8_t* e) {
    rtcoll::InitAs<ExternalRef, &ZeroMGRef>(e);
}
// stays explicit: String copy-ctor `assign` (byteCopy=false) plus a real
// `destroy`, a -1 generation seed, and an unwritten extent at [0x0c,0x10).
}  // namespace
const rtcoll::ArraySpec MapGeneratorRows::kSpec_ExternalRef = {   // exe 0x14001b340 / 0x14001b030
    "Array<ExternalRef>", 0x18, 8,
    &AssignMGRef, &MG_InitRef, &rtcoll::DestroyAs<ExternalRef, &DestroyMGRefs>, /*byteCopy*/false,
    /*assign unwritten*/ 0x0c, 0x10,
    /*init   unwritten*/ 0x0c, 0x10,
    /*seed zero       */ 0, 0 };
namespace {

// stride 0x30 — InitialStructure {ExternalRef@0, int _offsetX@0x18, _offsetY@0x1c,
// _width@0x20, _height@0x24, _rotation@0x28}, exe 0x14001b4e0. Retail's assign
// copies +0x08 (dword), +0x10 (qword), +0x18..+0x28 (five dwords) after the String
// copy-ctor; its init leaves [0x0c,0x10) and [0x2c,0x30) untouched, as does assign.
void CopyMGStructure(InitialStructure* d, const InitialStructure* s) {
    CopyMGRef(&d->structure, &s->structure);
    d->offsetX  = s->offsetX;    // +0x18
    d->offsetY  = s->offsetY;    // +0x1c
    d->width    = s->width;      // +0x20
    d->height   = s->height;     // +0x24
    d->rotation = s->rotation;   // +0x28
}
void ZeroMGStructure(InitialStructure* e) {
    ZeroMGRef(&e->structure);
    e->offsetX = 0;              // _offsetX, _offsetY (one qword 0 before; H7)
    e->offsetY = 0;
    e->width = -1;               // _width = _height = -1 (one qword before; H7)
    e->height = -1;
    e->rotation = 0;             // _rotation
}
void DestroyMGStructures(InitialStructure* elems, int count) {
    for (int i = 0; i < count; ++i)
        elems[i].structure.name.~String();
}
void AssignMGStructure(uint8_t* d, const uint8_t* s) {
    rtcoll::AssignAs<InitialStructure, &CopyMGStructure>(d, s);
}
void MG_InitStructure(uint8_t* e) {
    rtcoll::InitAs<InitialStructure, &ZeroMGStructure>(e);
}
// stays explicit: same as kSpec_MGRef, plus a -1 seed for _width/_height at
// +0x20 and a second unwritten extent at [0x2c,0x30).
}  // namespace
const rtcoll::ArraySpec MapGeneratorRows::kSpec_InitialStructure = {   // exe 0x14001b4e0 / 0x14001b0a0
    "Array<InitialStructure>", 0x30, 8,
    &AssignMGStructure, &MG_InitStructure, &rtcoll::DestroyAs<InitialStructure, &DestroyMGStructures>, /*byteCopy*/false,
    /*assign unwritten*/ 0x0c, 0x10,      // (and [0x2c,0x30); one range per row)
    /*init   unwritten*/ 0x0c, 0x10,
    /*seed zero       */ 0, 0 };
namespace {

// The resize of each pool: 0x14001b270 (stride 8), 0x14001b340 (0x18) and
// 0x14001b4e0 (0x30), the three retail bodies, each the core through its row.
void MG_Resize(MapGenerator::InitialFamilyArray& pool, int newCount) { pool.SetSize(newCount); }
void MG_Resize(MapGenerator::ExternalRefArray& pool, int newCount) { pool.SetSize(newCount); }
void MG_Resize(MapGenerator::InitialStructureArray& pool, int newCount) { pool.SetSize(newCount); }

// Serialize a count the way retail does: through a LOCAL seeded from the struct,
// resizing only on the load leg. Returns the count to iterate.
template <class Pool>
int MG_SerializeCount(IO::Serializer& serializer, Pool& pool, int32_t count) {
    int n = count;
    serializer.Serialize(n, nullptr);
    if (MG_IsLoading(serializer)) MG_Resize(pool, n);
    return n;
}

// A pool of ExternalRef entries (stride 0x18).
void MG_ReadRefPool(IO::Serializer& serializer, MapGenerator::ExternalRefArray& pool) {
    const int n = MG_SerializeCount(serializer, pool, pool._size);
    MapGenerator::ExternalRef* e = pool._data;
    if (!e) return;
    for (int i = 0; i < n; ++i) {
        e[i].name.Serialize(serializer);  // in place
    }
}
} // namespace

bool MapGenerator::Serialize(IO::Stream& stream) {
    IO::Serializer serializer(stream, 0);

    // --- scalar header (order matches Easy.rsc: _stringTable,_name,_description,
    //     _exposed,_startAreaSize,_citizenRadius,_citizen) ---
    Reimpl_ExternalPtr_Serialize_StringTable(&_field20, &serializer);   // +0x20 _stringTable ref
    _field28.Serialize(serializer);       // +0x28 _name
    _field30.Serialize(serializer);       // +0x30 _description
    serializer.Serialize(enabled, nullptr);        // +0x38 _exposed
    serializer.Serialize(subdivisions, nullptr);   // +0x3c _startAreaSize (townSize)
    serializer.Serialize(scale, nullptr);          // +0x40 _citizenRadius
    templateRef.name.Serialize(serializer);       // +0x48 _citizen template ref

    // --- _citizens: InitialFamily[] (stride 8: bool _educated@0, int _maxChildCount@4) ---
    {
        const int n = MG_SerializeCount(serializer, families, families._size);
        InitialFamily* e = families._data;
        for (int i = 0; e && i < n; ++i) {
            serializer.Serialize(e[i].educated, nullptr);        // _educated
            serializer.Serialize(e[i].maxChildCount, nullptr);   // _maxChildCount
        }
    }

    // --- refArray1 (+0x70): ExternalRef pool ---
    MG_ReadRefPool(serializer, refArray1);

    // --- _structures: InitialStructure[] (stride 0x30: ExternalRef@0,
    //     int _offsetX@0x18, _offsetY@0x1c, _width@0x20, _height@0x24, _rotation@0x28) ---
    {
        const int n = MG_SerializeCount(serializer, refArray2, refArray2._size);
        InitialStructure* e = refArray2._data;
        for (int i = 0; e && i < n; ++i) {   // FUN_14001b6c0, in place
            e[i].structure.name.Serialize(serializer);   // _structure ref name
            serializer.Serialize(e[i].offsetX, nullptr);    // _offsetX
            serializer.Serialize(e[i].offsetY, nullptr);    // _offsetY
            serializer.Serialize(e[i].width, nullptr);      // _width
            serializer.Serialize(e[i].height, nullptr);     // _height
            serializer.Serialize(e[i].rotation, nullptr);   // _rotation
        }
    }

    // --- refArray3..refArray7 (+0x90,+0xA0,+0xB0,+0xC0,+0xD0): ExternalRef pools ---
    MG_ReadRefPool(serializer, refArray3);
    MG_ReadRefPool(serializer, refArray4);
    MG_ReadRefPool(serializer, refArray5);
    MG_ReadRefPool(serializer, refArray6);
    MG_ReadRefPool(serializer, refArray7);

    // --- scalar tail (cull counts, initial resource counts, livestock params) ---
    serializer.Serialize(paramE0, nullptr);
    serializer.Serialize(paramE4, nullptr);
    serializer.Serialize(paramE8, nullptr);
    serializer.Serialize(paramEC, nullptr);
    serializer.Serialize(paramF0, nullptr);
    serializer.Serialize(paramF4, nullptr);
    serializer.Serialize(paramF8, nullptr);
    serializer.Serialize(paramFC, nullptr);
    serializer.Serialize(param100, nullptr);
    serializer.Serialize(param104, nullptr);
    serializer.Serialize(param108, nullptr);
    serializer.Serialize(param10C, nullptr);
    serializer.Serialize(param110, nullptr);
    serializer.Serialize(param114, nullptr);

    // --- env template refs + interleaved scalars (EXACT stream order) ---
    descRef1.name.Serialize(serializer);       // descRef1 (terrain)
    descRef2.name.Serialize(serializer);       // descRef2 (climate)
    serializer.Serialize(terrainAmplitude, nullptr);   // terrainAmplitude
    serializer.Serialize(chunkSize, nullptr);          // chunkSize
    descRef3.name.Serialize(serializer);       // descRef3 (vegetation)
    serializer.Serialize(param170, nullptr);

    return true;   // retail returns AL=1
}

// ============================================================
// MapGenerator::Construct (vtable +0x40)
// ============================================================
// Original: 0x14001ae20 — placement-new of the class on `this` (MSVC's
// placement new null-checks `this`, as the old slot did).
Object::External* MapGenerator::Construct() {
    return ::new (static_cast<void*>(this)) MapGenerator();
}

// ============================================================
// Reflection: create / createInPlace, the registration
// ============================================================
// MapGenerator is loaded as each element of Game/StartCondition/StartConditions.rsc
// (and the per-start MapGenerator .rsc files: Easy/Medium/Hard). Without the
// reflected type ExternalFactory::FindOrLoad's per-element load hits a
// NullFactory and crashes in Runtime IsDerivedFrom. GetFileVersion MUST return
// 8: Object::External::Load gates the element Serialize on
// `fileVersion == obj->GetFileVersion()`; a wrong constant silently drops the
// element and the start-condition combo comes up empty.
::Object::Base* MapGenerator::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(MapGenerator), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(::new (p) MapGenerator());
}
::Object::Base* MapGenerator::CreateInPlaceObject(void* memory) {
    if (!memory) return nullptr;
    return static_cast<Object::Base*>(::new (memory) MapGenerator());
}


DEFINE_EXE_TYPE(MapGenerator, "Object::External")
