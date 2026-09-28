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
#include "resolve.h"
#include "system/String.h"
#include "collection/Collection.h"
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch B
#include "object/Internal.h" // Object::detail::InternalMemAlloc / InternalMemFree
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <new>

// FUN_14001b030 / FUN_14001b0a0 — destroy the 0x18-byte ExternalRef and the
// 0x30-byte InitialStructure pools (game_leaf_helpers.cpp; registered at those
// addresses in reg_game_leaf.cpp). Called directly by ~MapGenerator.
void Game_DestroyStringArray18(void* arrayHeader);
void Game_DestroyStringArray30(void* arrayHeader);

// ============================================================
// MapGenerator::MapGenerator (0x1400a3030)
// ============================================================
// External() and the 0x14015ed80 stamp are the compiler's; then the defaults.
MapGenerator::MapGenerator() {
    // ExternalPtr at +0x20
    _field20 = nullptr;

    // Strings at +0x28, +0x30 (zeroed = empty string)
    _field28 = nullptr;
    _field30 = nullptr;

    // Generation parameters
    enabled = 1;          // bool, default true
    subdivisions = 8;     // grid subdivision count
    scale = 2.0f;         // terrain scale

    // ExternalRef at +0x48 (String + factoryGen + resolvedPtr)
    templateRef.strPtr = nullptr;
    templateRef.factoryGen = -1;  // 0xFFFFFFFF = unresolved
    templateRef.resolved = nullptr;

    // BoolInt array at +0x60
    boolIntArray = nullptr;
    boolIntCount = 0;

    // Dynamic arrays at +0x70 through +0xD8 (all zeroed)
    memset(&refArray1, 0, sizeof(DynArrayHeader));  // +0x70
    memset(&refArray2, 0, sizeof(DynArrayHeader));  // +0x80
    memset(&refArray3, 0, sizeof(DynArrayHeader));  // +0x90
    memset(&refArray4, 0, sizeof(DynArrayHeader));  // +0xA0
    memset(&refArray5, 0, sizeof(DynArrayHeader));  // +0xB0
    memset(&refArray6, 0, sizeof(DynArrayHeader));  // +0xC0
    memset(&refArray7, 0, sizeof(DynArrayHeader));  // +0xD0

    // Scalar generation parameters (+0xE0 through +0x114)
    paramE0 = 0;
    paramE4 = 0;
    paramE8 = 0;
    paramEC = 0.0f;
    paramF0 = 0.0f;
    paramF4 = 0.0f;
    paramF8 = 0;
    paramFC = 0;
    param100 = 0;
    param104 = 0;
    param108 = 0;
    param10C = 0;
    param110 = 0;
    param114 = 0;

    // ExternalRef at +0x118 (terrain description)
    descRef1.strPtr = nullptr;
    descRef1.factoryGen = -1;
    descRef1.resolved = nullptr;

    // Terrain amplitude
    terrainAmplitude = 6.0f;

    // ExternalRef at +0x138 (climate description)
    descRef2.strPtr = nullptr;
    descRef2.factoryGen = -1;
    descRef2.resolved = nullptr;

    // Chunk size
    chunkSize = 16;

    // ExternalRef at +0x158 (vegetation description)
    descRef3.strPtr = nullptr;
    descRef3.factoryGen = -1;
    descRef3.resolved = nullptr;
}

// ============================================================
// MapGenerator::~MapGenerator (body 0x14001aea0) / DeletingDtor (slot 0)
// ============================================================
// Retail body (0x14001aea0, verified against the disassembly's CALL list —
// three String dtors, 5x b030, 1x b0a0, 1x b030, MemFree, three String dtors,
// then a tail JMP to External::~External, which is the compiler's chain here).
// Field order is load-bearing (reverse construction order), so it is kept
// literal rather than folded into a loop. Moved here from
// unreg_lane1_mapgenerator_dtorbody.cpp, whose registered door now delegates.
MapGenerator::~MapGenerator() {
    // Three trailing description-file references.
    reinterpret_cast<System::String*>(&descRef3.strPtr)->~String();   // B.3.1 bridge: the COW pointers
    reinterpret_cast<System::String*>(&descRef2.strPtr)->~String();
    reinterpret_cast<System::String*>(&descRef1.strPtr)->~String();

    // Seven resource/biome arrays, high offset first.
    Game_DestroyStringArray18(&refArray7);
    Game_DestroyStringArray18(&refArray6);
    Game_DestroyStringArray18(&refArray5);
    Game_DestroyStringArray18(&refArray4);
    Game_DestroyStringArray18(&refArray3);
    Game_DestroyStringArray30(&refArray2);   // 0x30-byte elements
    Game_DestroyStringArray18(&refArray1);

    // The plain bool/int buffer at +0x60 (header is {data, count} — retail
    // zeroes both qwords, so +0x68 goes with it).
    if (boolIntArray) Object::detail::InternalMemFree(boolIntArray);
    boolIntArray = nullptr;
    boolIntCount = 0;

    // templateRef / _field30 / _field28.
    reinterpret_cast<System::String*>(&templateRef.strPtr)->~String();
    reinterpret_cast<System::String*>(&_field30)->~String();
    reinterpret_cast<System::String*>(&_field28)->~String();
}

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
bool MG_IsLoading(void* serializer) {
    void* stream = *(void**)serializer;
    return stream && static_cast<int>(reinterpret_cast<IO::Serializer*>(serializer)->GetDirection()) == 0;   // B.3.4 bridge  (IO::Serializer::GetDirection, retail 0x180006210)
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
void AssignMGFamily(uint8_t* d, const uint8_t* s) {
    *reinterpret_cast<uint64_t*>(d) = *reinterpret_cast<const uint64_t*>(s);
}
void MG_InitFamily(uint8_t* e) {
    *e = 0;
    *(int32_t*)(e + 4) = 0;
}
// stays explicit: `init` writes byte 0 and dword 4 and deliberately leaves
// [1,4) untouched — an `init unwritten` extent, i.e. a probe contract, not a
// trait. (An element with zeroing NSDMIs would also write the padding.)
const rtcoll::ArraySpec kSpec_MGFamily = {           // exe 0x14001b270
    "Array<InitialFamily>", 8, 4,
    &AssignMGFamily, &MG_InitFamily, nullptr, /*byteCopy*/true,
    /*assign unwritten*/ 0, 0,
    /*init   unwritten*/ 1, 4,
    /*seed zero       */ 0, 0 };
using MGFamilyArray = Collection::Array<uint8_t, Collection::DefaultAllocator,
                                        Collection::ExplicitSpec<&kSpec_MGFamily>>;

// stride 0x18 — ExternalRef {String@0, gen@8, resolved@0x10}, exe 0x14001b340.
// Neither arm writes [0x0c,0x10); the tail seed of -1 is load-bearing (see
// goal_exe_helpers.cpp).
void AssignMGRef(uint8_t* d, const uint8_t* s) {
    new (d) System::String(*reinterpret_cast<const System::String*>(s));     // +0x00
    *reinterpret_cast<uint32_t*>(d + 0x08) = *reinterpret_cast<const uint32_t*>(s + 0x08);
    *reinterpret_cast<uint64_t*>(d + 0x10) = *reinterpret_cast<const uint64_t*>(s + 0x10);
}
void MG_InitRef(uint8_t* e) {
    *(uint64_t*)e          = 0;
    *(int32_t*)(e + 8)     = -1;
    *(uint64_t*)(e + 0x10) = 0;
}
void DestroyMGRef(void* /*ignoredThis*/, uint8_t* base, int count) {
    for (int i = 0; i < count; ++i)
        reinterpret_cast<System::String*>(base + static_cast<int64_t>(i) * 0x18)->~String();
}
// stays explicit: String copy-ctor `assign` (byteCopy=false) plus a real
// `destroy`, a -1 generation seed, and an unwritten extent at [0x0c,0x10).
const rtcoll::ArraySpec kSpec_MGRef = {              // exe 0x14001b340
    "Array<ExternalRef>", 0x18, 8,
    &AssignMGRef, &MG_InitRef, &DestroyMGRef, /*byteCopy*/false,
    /*assign unwritten*/ 0x0c, 0x10,
    /*init   unwritten*/ 0x0c, 0x10,
    /*seed zero       */ 0, 0 };
using MGRefArray = Collection::Array<uint8_t, Collection::DefaultAllocator,
                                     Collection::ExplicitSpec<&kSpec_MGRef>>;

// stride 0x30 — InitialStructure {ExternalRef@0, int _offsetX@0x18, _offsetY@0x1c,
// _width@0x20, _height@0x24, _rotation@0x28}, exe 0x14001b4e0. Retail's assign
// copies +0x08 (dword), +0x10 (qword), +0x18..+0x28 (five dwords) after the String
// copy-ctor; its init leaves [0x0c,0x10) and [0x2c,0x30) untouched, as does assign.
void AssignMGStructure(uint8_t* d, const uint8_t* s) {
    AssignMGRef(d, s);
    for (int off = 0x18; off <= 0x28; off += 4)
        *reinterpret_cast<uint32_t*>(d + off) = *reinterpret_cast<const uint32_t*>(s + off);
}
void MG_InitStructure(uint8_t* e) {
    MG_InitRef(e);
    *(uint64_t*)(e + 0x18) = 0;                        // _offsetX, _offsetY
    *(uint64_t*)(e + 0x20) = 0xFFFFFFFFFFFFFFFFULL;    // _width = _height = -1
    *(int32_t*)(e + 0x28)  = 0;                        // _rotation
}
void DestroyMGStructure(void* /*ignoredThis*/, uint8_t* base, int count) {
    for (int i = 0; i < count; ++i)
        reinterpret_cast<System::String*>(base + static_cast<int64_t>(i) * 0x30)->~String();
}
// stays explicit: same as kSpec_MGRef, plus a -1 seed for _width/_height at
// +0x20 and a second unwritten extent at [0x2c,0x30).
const rtcoll::ArraySpec kSpec_MGStructure = {        // exe 0x14001b4e0
    "Array<InitialStructure>", 0x30, 8,
    &AssignMGStructure, &MG_InitStructure, &DestroyMGStructure, /*byteCopy*/false,
    /*assign unwritten*/ 0x0c, 0x10,      // (and [0x2c,0x30); one range per row)
    /*init   unwritten*/ 0x0c, 0x10,
    /*seed zero       */ 0, 0 };
using MGStructureArray = Collection::Array<uint8_t, Collection::DefaultAllocator,
                                           Collection::ExplicitSpec<&kSpec_MGStructure>>;

// Dispatch on the stride the caller names; the three are the three retail bodies.
void MG_Resize(void* header, int newCount, int stride) {
    // B.3.2 bridge: the MapGenerator pools are still raw DynArrayHeader fields.
    switch (stride) {
    case 8:    reinterpret_cast<MGFamilyArray*>(header)->SetSize(newCount);    break;
    case 0x18: reinterpret_cast<MGRefArray*>(header)->SetSize(newCount);       break;
    case 0x30: reinterpret_cast<MGStructureArray*>(header)->SetSize(newCount); break;
    default:   break;   // no such pool exists; the three callers below are the only ones
    }
}

// Serialize a count the way retail does: through a LOCAL seeded from the struct,
// resizing only on the load leg. Returns the count to iterate.
int MG_SerializeCount(void* serializer, void* header, void* countField, int stride) {
    int n = *(int32_t*)countField;
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(n, nullptr);   // B.3.4 bridge (cast dropped: n is int)
    if (MG_IsLoading(serializer)) MG_Resize(header, n, stride);
    return n;
}

// A pool of ExternalRef entries (stride 0x18). header/countField are the array
// header and its count int in the struct.
void MG_ReadRefPool(void* serializer, void* header, void* countField) {
    const int n = MG_SerializeCount(serializer, header, countField, 0x18);
    uint8_t* e = *(uint8_t**)header;
    if (!e) return;
    for (int i = 0; i < n; ++i, e += 0x18) {
        reinterpret_cast<System::String*>(e)->Serialize(serializer);  // in place
    }
}
} // namespace

bool MapGenerator::Serialize(void* stream) {
    // Caller batch CB6: the fields are read by name (was `s + off` over `this`).
    alignas(8) uint8_t serializer[24];
    ::new (static_cast<void*>(serializer)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge

    // --- scalar header (order matches Easy.rsc: _stringTable,_name,_description,
    //     _exposed,_startAreaSize,_citizenRadius,_citizen) ---
    Reimpl_ExternalPtr_Serialize_StringTable(&_field20, serializer);   // +0x20 _stringTable ref
    reinterpret_cast<System::String*>(&_field28)->Serialize(serializer);       // +0x28 _name
    reinterpret_cast<System::String*>(&_field30)->Serialize(serializer);       // +0x30 _description
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<bool*>(&enabled), nullptr);   // B.3.4 bridge  +0x38 _exposed
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(subdivisions, nullptr);   // B.3.4 bridge  +0x3c _startAreaSize (townSize)
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(scale, nullptr);   // B.3.4 bridge (cast dropped: scale is float)  +0x40 _citizenRadius
    reinterpret_cast<System::String*>(&templateRef.strPtr)->Serialize(serializer);       // +0x48 _citizen template ref

    // --- _citizens: InitialFamily[] (stride 8: bool _educated@0, int _maxChildCount@4) ---
    {
        const int n = MG_SerializeCount(serializer, &boolIntArray, &boolIntCount, 8);
        uint8_t* e = static_cast<uint8_t*>(boolIntArray);
        for (int i = 0; e && i < n; ++i, e += 8) {
            reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<bool*>(e), nullptr);        // B.3.4 bridge  _educated
            reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<int*>(e + 4), nullptr);     // B.3.4 bridge  _maxChildCount
        }
    }

    // --- refArray1 (+0x70): ExternalRef pool ---
    MG_ReadRefPool(serializer, &refArray1, &refArray1.count);

    // --- _structures: InitialStructure[] (stride 0x30: String@0, gen@8, ptr@0x10,
    //     int _offsetX@0x18, _offsetY@0x1c, _width@0x20, _height@0x24, _rotation@0x28) ---
    {
        const int n = MG_SerializeCount(serializer, &refArray2, &refArray2.count, 0x30);
        uint8_t* e = static_cast<uint8_t*>(refArray2.data);
        for (int i = 0; e && i < n; ++i, e += 0x30) {   // FUN_14001b6c0, in place
            reinterpret_cast<System::String*>(e)->Serialize(serializer);            // _structure ref name
            reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<int*>(e + 0x18), nullptr);  // B.3.4 bridge  _offsetX
            reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<int*>(e + 0x1c), nullptr);  // B.3.4 bridge  _offsetY
            reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<int*>(e + 0x20), nullptr);  // B.3.4 bridge  _width
            reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<int*>(e + 0x24), nullptr);  // B.3.4 bridge  _height
            reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<int*>(e + 0x28), nullptr);  // B.3.4 bridge  _rotation
        }
    }

    // --- refArray3..refArray7 (+0x90,+0xA0,+0xB0,+0xC0,+0xD0): ExternalRef pools ---
    MG_ReadRefPool(serializer, &refArray3, &refArray3.count);
    MG_ReadRefPool(serializer, &refArray4, &refArray4.count);
    MG_ReadRefPool(serializer, &refArray5, &refArray5.count);
    MG_ReadRefPool(serializer, &refArray6, &refArray6.count);
    MG_ReadRefPool(serializer, &refArray7, &refArray7.count);

    // --- scalar tail (cull counts, initial resource counts, livestock params) ---
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(paramE0, nullptr);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(paramE4, nullptr);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(paramE8, nullptr);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(paramEC, nullptr);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(paramF0, nullptr);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(paramF4, nullptr);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(paramF8, nullptr);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(paramFC, nullptr);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(param100, nullptr);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(param104, nullptr);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(param108, nullptr);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(param10C, nullptr);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(param110, nullptr);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(param114, nullptr);   // B.3.4 bridge

    // --- env template refs + interleaved scalars (EXACT stream order) ---
    reinterpret_cast<System::String*>(&descRef1.strPtr)->Serialize(serializer);       // descRef1 (terrain)
    reinterpret_cast<System::String*>(&descRef2.strPtr)->Serialize(serializer);       // descRef2 (climate)
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(terrainAmplitude, nullptr);   // B.3.4 bridge  terrainAmplitude
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(chunkSize, nullptr);   // B.3.4 bridge  chunkSize
    reinterpret_cast<System::String*>(&descRef3.strPtr)->Serialize(serializer);       // descRef3 (vegetation)
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(param170, nullptr);   // B.3.4 bridge

    reinterpret_cast<IO::Serializer*>(serializer)->~Serializer();   // B.3.4 bridge
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
void* MapGenerator::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(MapGenerator), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(::new (p) MapGenerator());
}
void* MapGenerator::CreateInPlaceObject(void* memory) {
    if (!memory) return nullptr;
    return static_cast<Object::Base*>(::new (memory) MapGenerator());
}


DEFINE_EXE_TYPE(MapGenerator, "Object::External")
