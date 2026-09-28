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
#include "io/Stream.h"      // ENDGAME B.3.4 callers batch H: IO::Stream::GetDirection
#include <cstdint>
#include <new>
#include "object/Internal.h"   // ENDGAME B.3.4 caller batch G
#include "Entity/ComponentEntity.h"
#include "Game/exe_bodies.h"
#include "Action/exe_bodies.h"      // ActCNR_CheckTerrain_cb00, Pf_RoadMeshRebuild_f370, Pf_ZoneRefresh_9c480
#include "Component/exe_bodies.h"   // CreateNaturalResource_helper_e2c0

// ============================================================
// Functions ported elsewhere, called by name through their declaring headers:
//   0x14009cb00 ActCNR_CheckTerrain_cb00            terrain-occupied check (Action/exe_bodies.h)
//   0x14009e2c0 CreateNaturalResource_helper_e2c0   per-cell terrain classify (Component/exe_bodies.h)
//   0x140002340 Reimpl_ExternalPtr_Serialize_StringTable  ExternalPtr via the string table (Game/exe_bodies.h)
//   0x1400af370 Pf_RoadMeshRebuild_f370             road mesh rebuild (Action/exe_bodies.h)
// 0x14009e8c0 region/pathfinder rebuild -- large, out of scope (see file header
// DEVIATION note): not ported, a PORT GAP at its call site below.
// ============================================================

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

    uint8_t* elem = reinterpret_cast<uint8_t*>(arrayBase);
    for (int32_t i = 0; i < count; ++i) {
        reinterpret_cast<Object::InternalBasePtr*>(elem + 0x00)->Serialize(*reinterpret_cast<IO::Serializer*>(serializer));   // B.3.4 bridge
        void* refA = *reinterpret_cast<void**>(elem + 0x00);
        if (refA != nullptr && !::Object::IsDerivedFromNullSafe(refA, ComponentEntity::TypeId())) {
            *reinterpret_cast<void**>(elem + 0x00) = nullptr;
        }

        reinterpret_cast<Object::InternalBasePtr*>(elem + 0x08)->Serialize(*reinterpret_cast<IO::Serializer*>(serializer));   // B.3.4 bridge
        void* refB = *reinterpret_cast<void**>(elem + 0x08);
        if (refB != nullptr && !::Object::IsDerivedFromNullSafe(refB, ComponentEntity::TypeId())) {
            *reinterpret_cast<void**>(elem + 0x08) = nullptr;
        }

        Reimpl_ExternalPtr_Serialize_StringTable(elem + 0x10, serializer);

        reinterpret_cast<IO::Serializer*>(serializer)->Serialize(9, elem + 0x1c, nullptr);   // B.3.4 bridge

        reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<uint16_t*>(elem + 0x26), nullptr);   // B.3.4 bridge

        int version = reinterpret_cast<IO::Serializer*>(serializer)->GetVersion();   // B.3.4 bridge
        if (version < 3) {
            // Retail reads/writes a throwaway ushort here for old save versions
            // (a field that no longer exists past version 3); we mirror that
            // with a scratch local so the stream position stays in sync.
            uint16_t scratch = 0;
            reinterpret_cast<IO::Serializer*>(serializer)->Serialize(scratch, nullptr);   // B.3.4 bridge
        }

        reinterpret_cast<IO::Serializer*>(serializer)->Serialize(elem[0x28], nullptr);   // B.3.4 bridge
        reinterpret_cast<IO::Serializer*>(serializer)->Serialize(elem[0x29], nullptr);   // B.3.4 bridge
        reinterpret_cast<IO::Serializer*>(serializer)->Serialize(elem[0x2a], nullptr);   // B.3.4 bridge
        reinterpret_cast<IO::Serializer*>(serializer)->Serialize(elem[0x2b], nullptr);   // B.3.4 bridge
        reinterpret_cast<IO::Serializer*>(serializer)->Serialize(elem[0x2c], nullptr);   // B.3.4 bridge
        reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<signed char*>(elem + 0x2d), nullptr);   // B.3.4 bridge

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
    ::new (static_cast<void*>(serializerStorage)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 4);   // B.3.4 bridge

    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<int32_t*>(self + 0x00), nullptr);   // B.3.4 bridge

    // Cell-array header at self+0x80: +0x00 base ptr, +0x08 count, +0x0c capacity.
    uint8_t* header = self + 0x80;
    int32_t cellCount = *reinterpret_cast<int32_t*>(header + 0x08);
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(cellCount, nullptr);   // B.3.4 bridge

    bool isSaving = static_cast<int>(reinterpret_cast<IO::Stream*>(stream)->GetDirection()) != 0;   // B.3.4 bridge  (IO::Stream::GetDirection, retail 0x180006200)
    if (!isSaving) {
        GameGrid_ResizeCellArray_aa90(reinterpret_cast<int64_t*>(header), cellCount);
    }

    int64_t arrayBase = *reinterpret_cast<int64_t*>(header);
    GameGrid_SerializeCellArray_ac10(serializer, cellCount, arrayBase);

    // 9 shorts at self+0x90.
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(9, reinterpret_cast<short*>(self + 0x90), nullptr);   // B.3.4 bridge

    if (!isSaving) {
        // ---- LOAD-only fixup block ----
        self[0xa2] = 1;

        int version = reinterpret_cast<IO::Serializer*>(serializer)->GetVersion();   // B.3.4 bridge

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
        // 0x14009c480 zone refresh. Retail's call (0x14009f29e) sets only RCX = this;
        // the body reads no other argument register: RDX / R8D are written before any
        // read (0x14009c4bd, 0x14009c520/524) and R9 never, and the flood fill it calls
        // (0x14009c170) spills R9D to a home slot that is rewritten before it is read
        // (0x14009c365 / 0x14009c3e8) and overwrites R9D at 0x14009c26f. The port's
        // three trailing parameters carry nothing, so they are passed as null / 0.
        Pf_ZoneRefresh_9c480(reinterpret_cast<int*>(selfPtr), nullptr, nullptr, 0);

        // PORT GAP 0x14009e8c0 RegionSystem build: not ported; retail calls it here
        // (0x14009f2a6, RCX = this grid). The null arm is: nothing.

        if (g_pGameWorldSingleton && *g_pGameWorldSingleton) {
            void* gameWorld = *g_pGameWorldSingleton;
            void* worldGeometry = *reinterpret_cast<void**>(
                reinterpret_cast<uint8_t*>(gameWorld) + 0x110);
            Pf_RoadMeshRebuild_f370(reinterpret_cast<int64_t>(worldGeometry),
                                     reinterpret_cast<uint32_t*>(selfPtr));
        }
    }

    reinterpret_cast<IO::Serializer*>(serializer)->~Serializer();   // B.3.4 bridge
}

// ============================================================
// Registration
// ============================================================
void RegisterGameGridSerialize() {
    RegisterExeAddr(0x14009f100, reinterpret_cast<uintptr_t>(&GameGrid_Serialize_9f100));
}
