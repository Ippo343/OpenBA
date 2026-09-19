// code/Game/Game/gamegrid_serialize.cpp
//
// Game/map-cell-grid serializer. Called during SAVE/LOAD by the master
// GameUpdateLoop serializer (0x140095a90) on the object at GameUpdateLoop+0xF8
// ("m_game"). Ported from Application-x64.exe 0x14009f100 (Ghidra's stale
// auto-name for this function is "CreateNaturalResource_impl_f100" -- ignore
// that label, it has nothing to do with CreateNaturalResource; the function
// actually serializes the map cell grid).
//
// Shape (verified against the disassembly, not just the decompiler's pseudocode --
// several offsets the decompiler reported turned out to be off by 0x10 due to
// Ghidra mistyping a couple of locals; the asm below is what this file follows):
//   - Serializer ctor (stream, version=4)
//   - int at self+0x00                                  (grid width)
//   - int cell-array count at self+0x88 (header @ self+0x80: +0x00 base ptr,
//     +0x08 count, +0x0c capacity), LOAD-only resize via 0x14001aa90
//   - bulk cell-array serialize via 0x14001ac10 (stride 0x30 bytes/cell)
//   - 9 shorts at self+0x90
//   - LOAD-only fixup block:
//       self+0xa2 = 1
//       version<2 back-fill of per-cell byte elem+0x29 via the CreateNaturalResource
//         terrain-check/classify helper pair (0x14009cb00 / 0x14009e2c0)
//       per-cell flag normalize pass (elem+0x28 sentinel fix, elem+0x2a AVL lookup
//         against the tree at self+0xe8)
//       Map_Invalidate(self)                              (0x14009c480, 1 arg)
//       region/pathfinder rebuild(self)                    (0x14009e8c0, 1 arg -- see
//         DEVIATION below)
//       TradeWork_impl_f370(*(gameWorld+0x110), self)      (0x1400af370)
//
// Per-cell element layout (0x30 bytes), offsets confirmed identical between the
// resize helper and the bulk-serialize helper:
//   +0x00 InternalBasePtr A (8B, cleared to null if not derived from ComponentEntity)
//   +0x08 InternalBasePtr B (8B, same clearing rule)
//   +0x10 ExternalPtr resource/description ref (8B, string-table serialize)
//   +0x18 opaque uint32 (4B, NOT individually serialized -- copied verbatim on resize)
//   +0x1c opaque 9-byte raw block (sized-raw serialize, spans 0x1c..0x24)
//   +0x26 ushort (2B)
//   +0x28 flags byte   (terrain-fill flag; the outer fixup's sentinel/AVL pass)
//   +0x29 byte         (sub-terrain classification written by the version<2 back-fill)
//   +0x2a byte         (sentinel: 0xff = "unset")
//   +0x2b, +0x2c bytes
//   +0x2d signed char
//
// DEVIATION (one, documented): 0x14009e8c0 is a large thread/region-pathfinder
// system rebuild (allocates + starts a worker Thread, builds a TradeComponent,
// and rebuilds the region pair-cache) -- out of scope for this pass. It is
// called via ResolveAddr(Module::Exe, ...), which is a safe no-op (returns
// null, guarded) until its own port lands. This only affects LOAD; existing
// saves will simply not get their region/pathfinder cache rebuilt by this
// call until 0x14009e8c0 is ported (other callers may still build it).

#include "engine_funcs.h"
#include "perf_terrain.h"
#include "resolve.h"
#include "collection/Collection.h"
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch B
#include <cstdint>
#include <new>

// ============================================================
// External declarations -- functions already ported elsewhere.
// ============================================================

// CreateNaturalResource_helper_cb00 (0x14009cb00) -- terrain-occupied check.
// Ported + registered in code/Game/Action/createnaturalresource.cpp /
// standalone/reg_action_cnr.cpp.
extern uint64_t ActCNR_CheckTerrain_cb00(void* self, int cellIndex);

// CreateNaturalResource_helper_e2c0 (0x14009e2c0) -- per-cell terrain classify.
// Ported + registered in code/Game/Component/keystone_a_map.cpp /
// standalone/reg_keystone_a_map.cpp.
extern int CreateNaturalResource_helper_e2c0(int* self, int cellIndex);

// Object_ExternalPtr_BuildComponentRef_Serialize (0x140002340) -- serializes an
// ExternalPtr through the save-file string table. Ported in
// code/Game/Game/serialize_externals.cpp.
extern "C" void Reimpl_ExternalPtr_Serialize_StringTable(void* refField, void* serializer);

// Pf_RoadMeshRebuild_f370 (0x1400af370, aka TradeWork_impl_f370) -- ported +
// registered in code/Game/Action/action_pf_road.cpp / standalone/reg_pf_road.cpp.
void Pf_RoadMeshRebuild_f370(int64_t param_1, uint32_t* param_2);

// ============================================================
// Local dispatch-slot resolution -- the two IO::Serializer scalar overloads
// this function needs (ushort, signed char) aren't in engine_funcs.h yet.
// Resolved the same way address_registry.cpp resolves every other Serializer
// dispatch slot: by mangled Runtime DLL export name.
// ============================================================
typedef void (*fn_Ser_SerUShort_t)(void* serializer, uint16_t* field, const char* name);
typedef void (*fn_Ser_SerSChar_t)(void* serializer, int8_t* field, const char* name);

static fn_Ser_SerUShort_t s_fn_SerUShort = nullptr;  // 0x140152408
static fn_Ser_SerSChar_t  s_fn_SerSChar  = nullptr;  // 0x140152418
static bool s_localSlotsResolved = false;

static void EnsureLocalSlots() {
    if (s_localSlotsResolved) return;
    s_localSlotsResolved = true;
    s_fn_SerUShort = reinterpret_cast<fn_Ser_SerUShort_t>(
        ResolveExport("?Serialize@Serializer@IO@@QEAAXAEAGPEBD@Z"));
    s_fn_SerSChar = reinterpret_cast<fn_Ser_SerSChar_t>(
        ResolveExport("?Serialize@Serializer@IO@@QEAAXAEACPEBD@Z"));
}

// 0x14009e8c0 region/pathfinder rebuild -- large, out of scope (see file header
// DEVIATION note). Resolved via ResolveAddr; unregistered today, so this stays
// a guarded no-op until it gets its own port.
static void (*s_fn_RegionSystemBuild)(void*) = nullptr;
static bool s_regionSystemBuildResolveAttempted = false;

static void EnsureRegionSystemBuild() {
    if (s_regionSystemBuildResolveAttempted) return;
    s_regionSystemBuildResolveAttempted = true;
    s_fn_RegionSystemBuild = reinterpret_cast<void (*)(void*)>(
        ResolveAddr(Module::Exe, 0x14009e8c0));
}

// ============================================================
// GameGrid_ResizeCellArray_aa90 (0x14001aa90)
//
// Resizes the cell array to `newCount` elements (stride 0x30 bytes).
// `header` is the 0x10-byte array-header embedded in the Game object:
//   +0x00 base ptr (int64), +0x08 count (int32), +0x0c capacity (int32).
// Grow-only allocation: capacity only ever increases, x3 on a "+1" grow step
// (a classic amortized-growth heuristic), verbatim from the disassembly.
//
// ENDGAME B.3.2 step 2: the body WAS a hand-written copy of
// `rtcoll::SetSizeCore` (runtime/src/collection/array.cpp) and is now one
// `Collection::Array<>::SetSize` over the row below. Compared arm for arm:
// exact-equal early return, `_size` stored before the capacity test,
// treble-on-single-append-from-non-empty, copy the preserved prefix only when
// the block moved, init the new tail, no element destroy, free the old block
// on a realloc (both go through the null-tolerant 0x1800c7ad0). The one
// difference: the allocation now goes through the LOCKING static `MemAlloc`
// (0x1800c7a50) rather than `fn_MemAllocStatic` — the same export, reached
// through rtcoll's own resolver.
// ============================================================
namespace {

// One map cell, stride 0x30, align 8. Both arms write +0x00..+0x24 and
// +0x26..+0x2d: byte +0x25 and bytes +0x2e..+0x2f are touched by NEITHER
// (the `assign unwritten` pair below names the trailing hole; +0x25 is the
// second, non-contiguous one). Partial copy, so byteCopy=false; trivially
// destructible.
void AssignMapCell(uint8_t* dst, const uint8_t* src) {
    *reinterpret_cast<uint64_t*>(dst + 0x00) = *reinterpret_cast<const uint64_t*>(src + 0x00);
    *reinterpret_cast<uint64_t*>(dst + 0x08) = *reinterpret_cast<const uint64_t*>(src + 0x08);
    *reinterpret_cast<uint64_t*>(dst + 0x10) = *reinterpret_cast<const uint64_t*>(src + 0x10);
    *reinterpret_cast<uint32_t*>(dst + 0x18) = *reinterpret_cast<const uint32_t*>(src + 0x18);
    *reinterpret_cast<uint64_t*>(dst + 0x1c) = *reinterpret_cast<const uint64_t*>(src + 0x1c);
    dst[0x24] = src[0x24];
    *reinterpret_cast<uint16_t*>(dst + 0x26) = *reinterpret_cast<const uint16_t*>(src + 0x26);
    dst[0x28] = src[0x28];
    dst[0x29] = src[0x29];
    dst[0x2a] = src[0x2a];
    dst[0x2b] = src[0x2b];
    dst[0x2c] = src[0x2c];
    dst[0x2d] = src[0x2d];
}
// Brand-new element: zero the refs/raw block, sentinel the flag bytes.
void InitMapCell(uint8_t* dst) {
    *reinterpret_cast<uint64_t*>(dst + 0x00) = 0;
    *reinterpret_cast<uint64_t*>(dst + 0x08) = 0;
    *reinterpret_cast<uint64_t*>(dst + 0x10) = 0;
    *reinterpret_cast<uint32_t*>(dst + 0x18) = 0xffffffffu;
    *reinterpret_cast<uint64_t*>(dst + 0x1c) = 0;
    dst[0x24] = 0;
    *reinterpret_cast<uint32_t*>(dst + 0x26) = 0;         // zeroes +0x26..+0x29
    *reinterpret_cast<uint32_t*>(dst + 0x2a) = 0x000000ffu; // +0x2a=0xff, +0x2b..+0x2d=0
}
// stays explicit: `init` is a patterned fill (0xffffffff at +0x18, 0xff at
// +0x2a), `assign` is partial (byteCopy=false), and both arms carry unwritten
// extents — a probe contract, not a trait.
const rtcoll::ArraySpec kSpec_MapCell = {                  // exe 0x14001aa90
    "Array<MapCell>", 0x30, 8,
    &AssignMapCell, &InitMapCell, nullptr, false,
    /*assign unwritten*/ 0x2e, 0x30,   // plus the single byte +0x25
    /*init   unwritten*/ 0x2e, 0x30,   // plus the single byte +0x25
    /*seed zero       */ 0,    0 };
using MapCellArray = Collection::Array<uint8_t, Collection::DefaultAllocator,
                                       Collection::ExplicitSpec<&kSpec_MapCell>>;

}  // namespace

void GameGrid_ResizeCellArray_aa90(int64_t* header, int32_t newCount) {
    // B.3.2 bridge: Game+0x80 is still a raw 0x10-byte header in an
    // unconverted exe struct.
    reinterpret_cast<MapCellArray*>(header)->SetSize(newCount);
}

// ============================================================
// GameGrid_SerializeCellArray_ac10 (0x14001ac10)
//
// Bulk-serializes `count` cells starting at `arrayBase` (stride 0x30 bytes).
// Per element: two InternalBasePtr refs (each nulled if not derived from
// ComponentEntity after load), one string-table ExternalPtr ref, a 9-byte raw
// block, a ushort, five bytes, and a signed char.
// ============================================================
void GameGrid_SerializeCellArray_ac10(void* serializer, int32_t count, int64_t arrayBase) {
    if (count <= 0) return;
    EnsureLocalSlots();

    uint8_t* elem = reinterpret_cast<uint8_t*>(arrayBase);
    for (int32_t i = 0; i < count; ++i) {
        if (fn_InternalBasePtr_Serialize) {
            fn_InternalBasePtr_Serialize(elem + 0x00, serializer);
        }
        void* refA = *reinterpret_cast<void**>(elem + 0x00);
        if (refA != nullptr && fn_IsDerivedFrom && g_TypeId_ComponentEntity &&
            !fn_IsDerivedFrom(refA, *g_TypeId_ComponentEntity)) {
            *reinterpret_cast<void**>(elem + 0x00) = nullptr;
        }

        if (fn_InternalBasePtr_Serialize) {
            fn_InternalBasePtr_Serialize(elem + 0x08, serializer);
        }
        void* refB = *reinterpret_cast<void**>(elem + 0x08);
        if (refB != nullptr && fn_IsDerivedFrom && g_TypeId_ComponentEntity &&
            !fn_IsDerivedFrom(refB, *g_TypeId_ComponentEntity)) {
            *reinterpret_cast<void**>(elem + 0x08) = nullptr;
        }

        Reimpl_ExternalPtr_Serialize_StringTable(elem + 0x10, serializer);

        if (fn_Ser_Slot_SerSizedRaw_400) {
            reinterpret_cast<IO::Serializer*>(serializer)->Serialize(9, elem + 0x1c, nullptr);   // B.3.4 bridge
        }

        if (s_fn_SerUShort) {
            reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<uint16_t*>(elem + 0x26), nullptr);   // B.3.4 bridge
        }

        int version = g_fn_Serializer_GetVersion ? reinterpret_cast<IO::Serializer*>(serializer)->GetVersion() : 4;   // B.3.4 bridge
        if (version < 3) {
            // Retail reads/writes a throwaway ushort here for old save versions
            // (a field that no longer exists past version 3); we mirror that
            // with a scratch local so the stream position stays in sync.
            uint16_t scratch = 0;
            if (s_fn_SerUShort) {
                reinterpret_cast<IO::Serializer*>(serializer)->Serialize(scratch, nullptr);   // B.3.4 bridge
            }
        }

        if (fn_Ser_Slot_SerByte_410) {
            reinterpret_cast<IO::Serializer*>(serializer)->Serialize(elem[0x28], nullptr);   // B.3.4 bridge
            reinterpret_cast<IO::Serializer*>(serializer)->Serialize(elem[0x29], nullptr);   // B.3.4 bridge
            reinterpret_cast<IO::Serializer*>(serializer)->Serialize(elem[0x2a], nullptr);   // B.3.4 bridge
            reinterpret_cast<IO::Serializer*>(serializer)->Serialize(elem[0x2b], nullptr);   // B.3.4 bridge
            reinterpret_cast<IO::Serializer*>(serializer)->Serialize(elem[0x2c], nullptr);   // B.3.4 bridge
        }
        if (s_fn_SerSChar) {
            reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<signed char*>(elem + 0x2d), nullptr);   // B.3.4 bridge
        }

        elem += 0x30;
    }
}

// ============================================================
// GameGrid_Serialize_9f100 (0x14009f100) -- top-level entry point.
// ============================================================
extern "C" void GameGrid_Serialize_9f100(void* selfPtr, void* stream) {
    uint8_t* self = reinterpret_cast<uint8_t*>(selfPtr);

    alignas(8) uint8_t serializerStorage[24];
    void* serializer = serializerStorage;
    if (g_fn_Serializer_Ctor) {
        ::new (static_cast<void*>(serializerStorage)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 4);   // B.3.4 bridge
    }

    if (g_fn_Serialize_Int) {
        reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<int32_t*>(self + 0x00), nullptr);   // B.3.4 bridge
    }

    // Cell-array header at self+0x80: +0x00 base ptr, +0x08 count, +0x0c capacity.
    uint8_t* header = self + 0x80;
    int32_t cellCount = *reinterpret_cast<int32_t*>(header + 0x08);
    if (g_fn_Serialize_Int) {
        reinterpret_cast<IO::Serializer*>(serializer)->Serialize(cellCount, nullptr);   // B.3.4 bridge
    }

    bool isSaving = *reinterpret_cast<int32_t*>(reinterpret_cast<uint8_t*>(stream) + 8) != 0;
    if (!isSaving) {
        GameGrid_ResizeCellArray_aa90(reinterpret_cast<int64_t*>(header), cellCount);
    }

    int64_t arrayBase = *reinterpret_cast<int64_t*>(header);
    GameGrid_SerializeCellArray_ac10(serializer, cellCount, arrayBase);

    // 9 shorts at self+0x90.
    if (fn_Ser_Slot_SerSizedRaw_3F8) {
        reinterpret_cast<IO::Serializer*>(serializer)->Serialize(9, reinterpret_cast<short*>(self + 0x90), nullptr);   // B.3.4 bridge
    }

    if (!isSaving) {
        // ---- LOAD-only fixup block ----
        self[0xa2] = 1;

        int version = g_fn_Serializer_GetVersion ? reinterpret_cast<IO::Serializer*>(serializer)->GetVersion() : 4;   // B.3.4 bridge

        if (version < 2) {
            int32_t width = *reinterpret_cast<int32_t*>(self + 0x00);
            for (int32_t i = 0; i < width * width; ++i) {
                uint8_t* base = *reinterpret_cast<uint8_t**>(header);
                uint8_t* e = base + static_cast<int64_t>(i) * 0x30;
                if (*reinterpret_cast<int64_t*>(e + 0x00) == 0 &&
                    *reinterpret_cast<int64_t*>(e + 0x08) == 0) {
                    bool occupied = ActCNR_CheckTerrain_cb00(selfPtr, i) != 0;
                    if (!occupied) {
                        int cls = CreateNaturalResource_helper_e2c0(
                            reinterpret_cast<int*>(selfPtr), i);
                        e[0x29] = static_cast<uint8_t>(cls);
                    }
                }
                // Faithful to retail: the width*width bound is re-read from
                // self+0x00 every iteration (loop-invariant unless a helper
                // above mutates the grid width, which none do here).
                width = *reinterpret_cast<int32_t*>(self + 0x00);
            }
        }

        // Per-cell flag normalize pass (runs regardless of version).
        {
            int32_t width = *reinterpret_cast<int32_t*>(self + 0x00);
            for (int32_t i = 0; i < width * width; ++i) {
                uint8_t* base = *reinterpret_cast<uint8_t**>(header);
                uint8_t* e = base + static_cast<int64_t>(i) * 0x30;

                if (e[0x28] == 0xff) {
                    e[0x28] = 0;
                }

                if (e[0x28] == 0) {
                    uint8_t mask = static_cast<uint8_t>(1u << (e[0x29] & 0x1f));
                    if ((mask & 2) == 0 && (mask & 1) == 0 && e[0x2a] == 0xff) {
                        // Walk the AVL tree rooted at self+0xe8 for key == i.
                        uint8_t* node = *reinterpret_cast<uint8_t**>(self + 0xe8);
                        while (node != nullptr) {
                            int32_t key = *reinterpret_cast<int32_t*>(node);
                            if (i < key) {
                                node = *reinterpret_cast<uint8_t**>(node + 0x10);  // left
                            } else if (key >= i) {
                                e[0x28] = 1;
                                break;
                            } else {
                                node = *reinterpret_cast<uint8_t**>(node + 0x18);  // right
                            }
                        }
                    } else {
                        e[0x28] = 1;
                    }
                }

                width = *reinterpret_cast<int32_t*>(self + 0x00);
            }
        }

        PerfX::TerrainInvalidate();   // [PerfX] the whole array was just streamed in
        if (fn_Map_Invalidate) {
            fn_Map_Invalidate(reinterpret_cast<int*>(selfPtr));
        }

        EnsureRegionSystemBuild();
        if (s_fn_RegionSystemBuild) {
            s_fn_RegionSystemBuild(selfPtr);
        }

        if (g_pGameWorldSingleton && *g_pGameWorldSingleton) {
            void* gameWorld = *g_pGameWorldSingleton;
            void* worldGeometry = *reinterpret_cast<void**>(
                reinterpret_cast<uint8_t*>(gameWorld) + 0x110);
            Pf_RoadMeshRebuild_f370(reinterpret_cast<int64_t>(worldGeometry),
                                     reinterpret_cast<uint32_t*>(selfPtr));
        }
    }

    if (g_fn_Serializer_Dtor) {
        reinterpret_cast<IO::Serializer*>(serializer)->~Serializer();   // B.3.4 bridge
    }
}

// ============================================================
// Registration
// ============================================================
void RegisterGameGridSerialize() {
    RegisterExeAddr(0x14009f100, reinterpret_cast<uintptr_t>(&GameGrid_Serialize_9f100));
    RegisterExeAddr(0x14001aa90, reinterpret_cast<uintptr_t>(&GameGrid_ResizeCellArray_aa90));
    RegisterExeAddr(0x14001ac10, reinterpret_cast<uintptr_t>(&GameGrid_SerializeCellArray_ac10));
}
