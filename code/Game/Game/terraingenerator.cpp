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
#include "object/Internal.h"   // Object::detail::InternalMemAlloc / InternalMemFree
#include "engine_funcs.h"
#include "system/Filename.h"
#include "resolve.h"
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
using TerrainSizeRowArray = Collection::Array<uint8_t, Collection::DefaultAllocator,
                                              Collection::ExplicitSpec<&kSpec_TerrainSizeRow>>;
}  // namespace

static void Reimpl_TerrainDataResize(void* arrayField, int newCount) {
    if (!arrayField) return;
    // B.3.2 bridge: TerrainGenerator+0x80 is still a pair of void* fields.
    reinterpret_cast<TerrainSizeRowArray*>(arrayField)->SetSize(newCount);
}

// FUN_140020870 -- serialize `count` rows of 17 ints each. NOTE row[9] is
// serialized LAST (after row[16]) -- this field order must match the .rsc write
// order exactly or the stream misaligns.
static void Reimpl_TerrainDataSerialize(void* serializer, uint32_t count, void* arrayPtr) {
    if (static_cast<int>(count) <= 0 || !arrayPtr) return;
    static const int kOrder[17] = { 0,1,2,3,4,5,6,7,8,10,11,12,13,14,15,16,9 };
    int32_t* row = reinterpret_cast<int32_t*>(arrayPtr);
    for (uint32_t r = 0; r < count; ++r) {
        for (int k = 0; k < 17; ++k)
            reinterpret_cast<IO::Serializer*>(serializer)->Serialize(row[kOrder[k]], nullptr);   // B.3.4 bridge (cast dropped: row[] is int32_t)
        row += 17;
    }
}

// ============================================================
// Helper: Get serializer version
// ============================================================
static inline int GetSerializerVersion(void* serializer) {
    return static_cast<int>(reinterpret_cast<IO::Serializer*>(serializer)->GetDirection());   // B.3.4 bridge  (IO::Serializer::GetDirection, retail 0x180006210)
}

// ============================================================
// Helper: Serialize a filename string with version-0 resolution
// ============================================================
// Pattern from TerrainGenerator Slot5: Filename ctor, String::Serialize,
// if version==0 resolve filename (FUN_14001fd10), Filename dtor.
static void SerializeFilenameString(void* self_offset, void* serializer) {
    System::Filename filenameObj;   // retail's scope temp; only ctor + dtor
    (void)filenameObj;

    reinterpret_cast<System::String*>(self_offset)->Serialize(serializer);

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

    // Terrain data array at +0x80, count/info at +0x88
    _field80 = nullptr;
    _field88 = nullptr;
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
    reinterpret_cast<TerrainSizeRowArray*>(&_field80)->~Array();

    // Destroy strings in reverse order: +0x68, +0x50, +0x38, +0x28
    reinterpret_cast<System::String*>(&_field68)->~String();   // B.3.1 bridge: the COW pointers
    reinterpret_cast<System::String*>(&_field50)->~String();
    reinterpret_cast<System::String*>(&_field38)->~String();
    reinterpret_cast<System::String*>(&_field28)->~String();
}

Object::Base* TerrainGenerator::DeletingDtor(unsigned flags) {
    this->~TerrainGenerator();
    if (flags & 1) Object::detail::InternalMemFree(this);
    return this;
}

// [4] PORT GAP (B38 D3.1): retail's slot is the imported Object::External::Compile;
// the old table held a `return 0` stub. Kept until the family's titled commit.
Object::External::CompileResult TerrainGenerator::Compile(const System::Filename&) { return 0; }

// ============================================================
// TerrainGenerator::Reconstruct (vtable +0x30, Slot6)
// ============================================================
// Original: 0x1400205e0. Destroys the object (slot 0, flags 0: no free) then
// re-constructs in place.
Object::External* TerrainGenerator::Reconstruct() {
    DeletingDtor(0);
    return ::new (static_cast<void*>(this)) TerrainGenerator();
}

// [7] 0x140001640 — PORT GAP (B38 D3.2): retail runs DeletingDtor(0); the old
// table held a `return 0` stub. Kept until the family's titled commit.
void TerrainGenerator::Destruct() {}

// ============================================================
// TerrainGenerator::Serialize (vtable +0x28, Slot5)
// ============================================================
// Original: 0x1400b1690. ExternalPtr, String, bool, 3 filename strings with
// version-0 resolution, and the dynamic terrain data array. Returns AL=1.
bool TerrainGenerator::Serialize(void* stream) {
    alignas(8) uint8_t serializer[24];
    ::new (static_cast<void*>(serializer)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge
    IO::Serializer& s = *reinterpret_cast<IO::Serializer*>(serializer);   // B.3.4 bridge

    // Serialize ExternalPtr at +0x20
    if (fn_ExternalPtr_Serialize2)
        fn_ExternalPtr_Serialize2(&_field20, serializer);

    // Serialize String at +0x28
    reinterpret_cast<System::String*>(&_field28)->Serialize(serializer);

    // Serialize bool at +0x30 — declared uint8_t, not exactly bool: cast kept
    s.Serialize(*reinterpret_cast<bool*>(&_field30), nullptr);

    // 3 filename strings with the version-0 resolution: +0x38, +0x50, +0x68
    SerializeFilenameString(&_field38, serializer);
    SerializeFilenameString(&_field50, serializer);
    SerializeFilenameString(&_field68, serializer);

    // Terrain data array count: read the current count from +0x88, serialize
    // it, resize when reading (FUN_1400206b0), then the rows (FUN_140020870).
    uint32_t count = *reinterpret_cast<uint32_t*>(&_field88);
    s.Serialize(*reinterpret_cast<int*>(&count), nullptr);   // cast kept: count is uint32_t, not int

    if (GetSerializerVersion(serializer) == 0)
        Reimpl_TerrainDataResize(&_field80, count);

    Reimpl_TerrainDataSerialize(serializer, count, _field80);

    s.~Serializer();
    return true;
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

DEFINE_EXE_VTABLE(TerrainGenerator, "_ZTV16TerrainGenerator")

// B38_RULES D5: nothing in the tree reads 0x14015f170 back (swept by hex and
// owner); this row is reader-less and serves the compiler table only so the
// coordinator-owned call in address_registry.cpp keeps linking. The closing
// pass deletes it with the call.
extern "C" void RegisterTerrainGeneratorVtable() {
    RegisterExeAddr(0x14015f170, reinterpret_cast<uintptr_t>(TerrainGenerator::Vtable()));
}

// Register the two terrain-data-array helpers to the faithful reimpls above
// (FUN_1400206b0 resize / FUN_140020870 serialize), so the ResolveAddr call
// sites resolve them.
void RegisterGameTerrainReimpls() {
    RegisterExeAddr(0x1400206b0, reinterpret_cast<uintptr_t>(&Reimpl_TerrainDataResize));
    RegisterExeAddr(0x140020870, reinterpret_cast<uintptr_t>(&Reimpl_TerrainDataSerialize));
}

DEFINE_EXE_TYPE(TerrainGenerator, "Object::External")
