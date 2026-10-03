// code/Game/Game/terraingenerator.cpp
// Clean reimplementation of TerrainGenerator game logic.
//
// TerrainGenerator (0x90 bytes) -- terrain height/noise generation parameters.
// Extends Object::External (0x20 base). Vtable at 0x14015f170.
//
// ENDGAME B.3.8 block LOOP: a REAL class (code/Game/Game/TerrainGenerator.h,
// field layout and retail slot map there) with a compiler-generated vtable.
// Gone: the 11-slot `g_TerrainGenerator_Vtable_Standalone` (two over-read
// guards past retail's 9), its trampolines and stubs, the lazy vtable resolve
// in the old Init, the lazily-resolved door pointers + EnsureInit (a non-atomic
// first-call cache reached on the LOADER thread, B6-5) and the C entry points.
//
// Contains ExternalPtr, 3 noise layer filename strings (each with a -1 sentinel int),
// and a dynamic array of terrain data entries (0x44 bytes each).
//
// Field layout (from constructor + serialize + dtor):
//   +0x00: vtable
//   +0x08: External base (0x18 bytes)
//   +0x20: ExternalPtr (8 bytes)
//   +0x28: System::String noiseFile1 (8 bytes)
//   +0x30: bool enabled (default 1)
//   +0x31: pad (7 bytes)
//   +0x38: System::String noiseFile2 (8 bytes)
//   +0x40: int32_t sentinel1 (default -1)
//   +0x44: pad (4 bytes)
//   +0x48: void* field48 (default 0)
//   +0x50: System::String noiseFile3 (8 bytes)
//   +0x58: int32_t sentinel2 (default -1)
//   +0x5C: pad (4 bytes)
//   +0x60: void* field60 (default 0)
//   +0x68: System::String noiseFile4 (8 bytes)
//   +0x70: int32_t sentinel3 (default -1)
//   +0x74: pad (4 bytes)
//   +0x78: void* field78 (default 0)
//   +0x80: void* terrainDataArray (dynamic array ptr, freed in dtor)
//   +0x88: void* terrainDataInfo (count/capacity)
//
// Decompiled from Application-x64.exe.

#include "Game/TerrainGenerator.h"
#include "Game/TerrainHeightfield.h"   // TerrainSizeRow
#include "object/Internal.h"   // Object::detail::InternalMemAlloc / InternalMemFree
#include "object/External.h"
#include "object/TypeInfo.h"
#include "Game/exe_bodies.h"
#include "system/Filename.h"
#include "system/String.h"
#include "collection/Collection.h"
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch B
#include <cstdint>
#include <cstring>
#include <new>

// FUN_14001fd10 -- resolve a deserialized noise-layer filename String to its
// External. Registered at that address to this body (standalone/reg_game_leaf.cpp);
// called directly now, as are the two array helpers below (0x1400206b0 /
// 0x140020870), whose registered bodies live in this file.
void Game_TerrainResolveMaterial(void* refTriple);   // game_leaf_helpers.cpp

// ============================================================
// Standalone reimplementations of the +0x80 terrain-data-array helpers.
// In standalone these exe addresses aren't mapped, so ResolveAddr returns
// null and the size table never deserializes (gen+0x80 stays null -> worldgen
// reads dim=0). These faithful reimpls (decompiled from FUN_1400206b0 /
// FUN_140020870) populate the size table so the in-game terrain build works.
// Each row is a SizeRow: 17 ints, stride 0x44; row[0] = map dimension.
// ============================================================

// FUN_1400206b0 -- stride-0x44 dynamic-array resize. New rows get the engine
// default SizeRow template (row[0]=256, etc.).
//
// ENDGAME B.3.2 step 2: the body is now `Collection::Array<>::SetSize` over the
// `kSpec_TerrainSizeRow` row. Compared arm for arm against rtcoll::SetSizeCore
// and the retail body 0x1400206b0 (re-decompiled 2026-09-13): exact-equal early
// return, `_size` first, treble-on-single-append-from-non-empty, cap*0x44 at
// align 4, null element address skipped, 17-dword copy of the prefix only when
// the block moved, default-row init for the tail, old block freed only when it
// moved, no destroy. Every arm matched, nothing changed.
namespace {
void AssignTerrainSizeRow(uint8_t* d, const uint8_t* s) {
    std::memcpy(d, s, 0x44);                         // 17 dwords, the whole stride
}
void InitTerrainSizeRow(uint8_t* d) {
    static const int32_t kDefaultRow[17] = {
        0x100, 0x32, 10, 0x14, 0, 10, 0x14, 3, 1, 1000000, 0,
        1, 10, 0x14, 0x23, 6, 0xc
    };
    std::memcpy(d, kDefaultRow, 0x44);
}
// stays explicit: `init` copies the 17-dword default SizeRow template, a
// patterned fill no element type's value-initialisation can produce.
const rtcoll::ArraySpec kSpec_TerrainSizeRow = {     // exe 0x1400206b0
    "Array<SizeRow>", 0x44, 4,
    &AssignTerrainSizeRow, &InitTerrainSizeRow, nullptr, /*byteCopy*/true,
    /*assign unwritten*/ 0, 0,
    /*init   unwritten*/ 0, 0,
    /*seed zero       */ 0, 0 };
using TerrainSizeRowArray = Collection::Array<TerrainSizeRow, Collection::DefaultAllocator,
                                              Collection::ExplicitSpec<&kSpec_TerrainSizeRow>>;
}  // namespace

// The {data +0x80, count +0x88, capacity +0x8C} triple is raw storage in
// TerrainGenerator.h (H5: an Array member would make its ctor / dtor run where
// the tree runs them by hand), so the two Array operations take one view of it.
static void Reimpl_TerrainDataResize(TerrainSizeRow** arrayField, int newCount) {
    if (!arrayField) return;
    reinterpret_cast<TerrainSizeRowArray*>(arrayField)->SetSize(newCount);   // LEFT: H5 raw Array triple (+0x80)
}

// FUN_140020870 -- serialize `count` rows of 17 ints each. NOTE row[9]
// (+0x24, erosionIterations) is serialized LAST (after row[16]) -- this field
// order must match the .rsc write order exactly or the stream misaligns.
static void Reimpl_TerrainDataSerialize(IO::Serializer* serializer, uint32_t count, TerrainSizeRow* rows) {
    if (static_cast<int>(count) <= 0 || !rows) return;
    TerrainSizeRow* row = rows;
    for (uint32_t r = 0; r < count; ++r) {
        serializer->Serialize(row->mapSize, nullptr);             // [0]
        serializer->Serialize(row->riverCount2, nullptr);         // [1]
        serializer->Serialize(row->riverMinEdge2, nullptr);       // [2]
        serializer->Serialize(row->riverMaxEdge2, nullptr);       // [3]
        serializer->Serialize(row->riverCount1, nullptr);         // [4]
        serializer->Serialize(row->riverMinEdge1, nullptr);       // [5]
        serializer->Serialize(row->riverMaxEdge1, nullptr);       // [6]
        serializer->Serialize(row->field_0x1C, nullptr);          // [7]
        serializer->Serialize(row->secondaryPathCount, nullptr);  // [8]
        serializer->Serialize(row->minRangeCount, nullptr);       // [10]
        serializer->Serialize(row->maxRangeCount, nullptr);       // [11]
        serializer->Serialize(row->minRangeBand, nullptr);        // [12]
        serializer->Serialize(row->maxRangeBand, nullptr);        // [13]
        serializer->Serialize(row->basinsPerRange, nullptr);      // [14]
        serializer->Serialize(row->basinRadiusMax, nullptr);      // [15]
        serializer->Serialize(row->basinRadiusMin, nullptr);      // [16]
        serializer->Serialize(row->erosionIterations, nullptr);   // [9]
        row += 1;
    }
}

// ============================================================
// Helper: Get serializer version
// ============================================================
static inline int GetSerializerVersion(IO::Serializer* serializer) {
    return static_cast<int>(serializer->GetDirection());   // IO::Serializer::GetDirection, retail 0x180006210
}

// ============================================================
// Helper: Serialize a filename string with version-0 resolution
// ============================================================
// Pattern from TerrainGenerator Slot5: Filename ctor, String::Serialize,
// if version==0 resolve filename (FUN_14001fd10), Filename dtor.
static void SerializeFilenameString(void* self_offset, IO::Serializer* serializer) {
    System::Filename filenameObj;   // retail's scope temp; only ctor + dtor
    (void)filenameObj;

    reinterpret_cast<System::String*>(self_offset)->Serialize(serializer);   // LEFT: H5 raw String storage

    if (GetSerializerVersion(serializer) == 0) {
        Game_TerrainResolveMaterial(self_offset);
    }
}

// ============================================================
// TerrainGenerator::TerrainGenerator (0x1400b1810)
// ============================================================
// External() and the 0x14015f170 stamp are the compiler's; then the defaults.
TerrainGenerator::TerrainGenerator() {
    _field20 = nullptr;          // ExternalPtr
    _field28 = nullptr;          // String
    _field30 = 1;                // enabled

    // Noise layer 1: String, sentinel, ptr
    _field38 = nullptr;
    _field40 = -1;
    _field48 = nullptr;
    // Noise layer 2
    _field50 = nullptr;
    _field58 = -1;
    _field60 = nullptr;
    // Noise layer 3
    _field68 = nullptr;
    _field70 = -1;
    _field78 = nullptr;

    // Terrain data array at +0x80, count +0x88, capacity +0x8C (retail: one qword zero; H7)
    _field80 = nullptr;
    _sizeRowCount = 0;
    _sizeRowCapacity = 0;
}

// ============================================================
// TerrainGenerator::~TerrainGenerator / DeletingDtor (vtable +0x00)
// ============================================================
// Original: 0x140020600 -> body FUN_140020630: frees the terrain data array
// at +0x80, destroys the 4 Strings at +0x68, +0x50, +0x38, +0x28; then
// External::~External (the compiler's chain) and MemFree if flags & 1.
TerrainGenerator::~TerrainGenerator() {
    // ENDGAME B.3.2 step 2: `~Array` for the +0x80 SizeRow array — MemFree the
    // block, then zero data and the count/capacity qword (trivial element, so
    // no destroy step). B.3.2 bridge: +0x80/+0x88 are still two void* fields.
    reinterpret_cast<TerrainSizeRowArray*>(&_field80)->~Array();   // LEFT: H5 raw Array triple (+0x80)

    // Destroy strings in reverse order: +0x68, +0x50, +0x38, +0x28
    reinterpret_cast<System::String*>(&_field68)->~String();   // LEFT: H5 raw String storage (the COW pointers)
    reinterpret_cast<System::String*>(&_field50)->~String();   // LEFT: H5 raw String storage
    reinterpret_cast<System::String*>(&_field38)->~String();   // LEFT: H5 raw String storage
    reinterpret_cast<System::String*>(&_field28)->~String();   // LEFT: H5 raw String storage
}

// ============================================================
// TerrainGenerator::Reconstruct (vtable +0x30, Slot6)
// ============================================================
// Original: 0x1400205e0. Destroys the object (slot 0, flags 0: no free) then
// re-constructs in place.
Object::External* TerrainGenerator::Reconstruct() {
    this->~TerrainGenerator();
    return ::new (static_cast<void*>(this)) TerrainGenerator();
}

// [7] 0x140001640 — the shared body `MOV RAX,[RCX]; XOR EDX,EDX; JMP [RAX]`:
// slot 0 with flags 0 (destroy, no free).
void TerrainGenerator::Destruct() { this->~TerrainGenerator(); }

// ============================================================
// TerrainGenerator::Serialize (vtable +0x28, Slot5)
// ============================================================
// Original: 0x1400b1690. ExternalPtr, String, bool, 3 filename strings with
// version-0 resolution, and the dynamic terrain data array. Returns AL=1.
bool TerrainGenerator::Serialize(IO::Stream& stream) {
    IO::Serializer s(stream, 0);
    IO::Serializer* serializer = &s;   // destroyed at the return (retail's order)

    // Serialize ExternalPtr at +0x20
    Reimpl_ExternalPtr_Serialize_StringTable(&_field20, serializer);

    // Serialize String at +0x28
    reinterpret_cast<System::String*>(&_field28)->Serialize(serializer);   // LEFT: H5 raw String storage

    // Serialize bool at +0x30 — declared uint8_t, not exactly bool: cast kept
    s.Serialize(*reinterpret_cast<bool*>(&_field30), nullptr);   // LEFT: H8 (a bool field would normalise the byte)

    // 3 filename strings with the version-0 resolution: +0x38, +0x50, +0x68
    SerializeFilenameString(&_field38, serializer);
    SerializeFilenameString(&_field50, serializer);
    SerializeFilenameString(&_field68, serializer);

    // Terrain data array count: read the current count from +0x88, serialize
    // it, resize when reading (FUN_1400206b0), then the rows (FUN_140020870).
    int32_t count = _sizeRowCount;
    s.Serialize(count, nullptr);   // the int overload

    if (GetSerializerVersion(serializer) == 0)
        Reimpl_TerrainDataResize(&_field80, count);

    Reimpl_TerrainDataSerialize(serializer, static_cast<uint32_t>(count), _field80);

    return true;   // ~Serializer
}

// ============================================================
// TerrainGenerator::Construct (vtable +0x40)
// ============================================================
// Original: 0x1400205d0 — placement-new of the class on `this` (MSVC's
// placement new null-checks `this`, as the old slot did).
Object::External* TerrainGenerator::Construct() {
    return ::new (static_cast<void*>(this)) TerrainGenerator();
}

// ============================================================
// Reflection: create / createInPlace, the registration
// ============================================================
// TerrainGenerator is loaded as each element of Game/Terrain/TerrainList.rsc
// (Valleys/Mountains .rsc). Without the reflected type ExternalFactory::
// FindOrLoad's per-element load hits a NullFactory and crashes in Runtime
// IsDerivedFrom. GetFileVersion MUST return 4: Object::External::Load gates
// the element Serialize on `fileVersion == obj->GetFileVersion()`.
void* TerrainGenerator::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(TerrainGenerator), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(::new (p) TerrainGenerator());
}
void* TerrainGenerator::CreateInPlaceObject(void* memory) {
    if (!memory) return nullptr;
    return static_cast<Object::Base*>(::new (memory) TerrainGenerator());
}

DEFINE_EXE_TYPE(TerrainGenerator, "Object::External")
