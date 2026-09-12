#pragma once
// ============================================================================
// runtime/src/object/deleting_dtor_internal.h — Wave 9 lane K
// ============================================================================
//
// The three things deleting_dtor.cpp and deleting_dtor_probe.cpp must agree on:
// the row layout, the body's signature, and which rows the probe may safely
// use as subjects. Only those two files include this.
//
// The full commentary — what the family is, why it is the VECTOR deleting
// destructor and not the scalar one the brief assumed, and what the two halves
// of the oracle each cover — is the banner of deleting_dtor.cpp.
// ============================================================================

#include <cstdint>

#include "../../funcs/rt_diff.h"

namespace rtcoll {
// Lane Q's by-address call to ?MemFree@MemoryAllocator@System@@SAXPEAX@Z
// (0x1800c7ad0), declared rather than re-derived so lane E has ONE place to
// repoint when the allocator moves. Defined in
// runtime/src/collection/array.cpp; declared identically in
// runtime/src/collection/array_internal.h, and the linker checks the two agree.
void* MemAlloc(uint64_t bytes, int align);
void  MemFree(void* p);
}  // namespace rtcoll

namespace rtdtor {

// `~Class`. Every one of the 37 destructors this family calls takes `this` and
// nothing else, verified per address in the disassembly — not assumed from the
// mangling, because lane Q's SetSize correction was precisely a family whose
// arity varied behind one typedef.
using DtorFn = void (*)(void* self);

// The registered entry point's shape: `??_E<Class>@@UEAAPEAXI@Z`.
// Bit 0 of `flags` = free the block, bit 1 = this was a `delete[]`.
using DeletingDtorFn = void* (*)(void* self, unsigned flags);

// One row per registered address. `dtorVa` and `elemSize` are the ONLY two
// values that vary across the 37; both are re-derived from the shipped DLL by
// tools/rt_deleting_dtor_verify.py.
struct DelDtorSpec {
    uintptr_t   addr;       // the deleting destructor's own Ghidra VA
    uintptr_t   dtorVa;     // ~Class, reached through ResolveAddr (port-aware)
    uint32_t    elemSize;   // sizeof(Class), the array branch's stride
    const char* name;
};

// 37 as surveyed by Wave 9 lane K; the 38th (UI::Dialog, 0x180038b20) was added
// by Wave 10C lane DLG8 when it closed `??_7Dialog@UI@@6B@`, and the 39th
// (UI::ToolTipDialog, 0x18004d270) by Wave 10D lane TIP for the same reason on
// `??_7ToolTipDialog@UI@@6B@`. Wave 19 lane CSTREAM added the 40th and 41st
// (IO::CompressedStream 0x180023fd0, IO::CompressedFileStream 0x180024100) on
// `??_7CompressedStream@IO@@6B@` and `??_7CompressedFileStream@IO@@6B@` --
// making that four lanes now that have appended, which is the whole reason this
// constant exists. Wave 21 lane ENC added the 42nd (IO::TextFileStream,
// 0x18002ba90) closing slot 0 of `??_7TextFileStream@IO@@6B@`, and the 43rd
// (IO::ReaderWriter, 0x180023bd0) closing the single non-purecall slot of
// `??_7ReaderWriter@IO@@6B@` (Wave 21 §8 item 12 — that row stays
// RT_UNPROMOTABLE regardless; see reg_mint_promoted.cpp). Prose in
// deleting_dtor.cpp that says "37" is describing lane K's original survey and
// stays true of it; the TABLE is the authority and this constant tracks it,
// and the static_assert next to the table is what forces an appender to
// notice (it caught CSTREAM's addition, at 41 vs 39).
constexpr int kSpecCount = 81;   // 71-80: VTABLE BATCH 1 -- UI::{Border,Button,Check,
                                 //     ClipRegion,Combo,Container,DocumentText,Edit,Graph,
                                 //     Label}Description (2026-09-12); renumbered at the
                                 //     merge, batch 9 keeps 61-66 and batch 8 67-70
                                 //     Picking::PickingMesh (vtable batch 8, 2026-09-12); 61-66 batch 9
                                 // 67-70: Collision2D::CollisionMesh/Surface, Pathfinding::PointList,
                                 //     Picking::PickingMesh (vtable batch 8, 2026-09-12); 61-66 batch 9
                                 // 61: UI::ImageDescription + AnimatedImageDescription (2026-09-12)   // 59: UI::RibbonDescription (2026-09-12)   // 58: UI::GroupDescription (2026-09-12)   // 57: Video::ShaderProgram + VertexProgram + PixelProgram + RenderState (2026-09-12)   // 53: Video::VertexLayout (2026-09-12)   // 52: Video::VideoProgram (2026-09-12)   // 51: Graphics::Material (2026-09-12)
                                 // (was 50)   // 45-47: Graphics::EdgeMesh / EdgeMeshInstance /
                                 // MeshInstance (Wave 27 lane GFXMESH)
                                 // 48-49: Graphics::Light / SunLight (Wave 27 lane GFXPART)
                                 // 50:    UI::ElementDescription (Wave 27 lane IOTEXT)
                                 //
                                 // *** MERGE HAZARD, PAID FOR AT WAVE 27 INTEGRATION ***
                                 // The table is POSITIONAL: DeletingDtorCore(self,
                                 // flags, N) indexes kSpecs[N]. THREE Wave 27 lanes
                                 // each appended starting at index 44 on their own
                                 // branch. A keep-both merge silently gave two of
                                 // them the FIRST lane's rows. The static_assert
                                 // below checks the COUNT ONLY -- it cannot see an
                                 // index collision, and a wrong index dispatches
                                 // THE WRONG DESTRUCTOR with no diagnostic.
                                 // If you append here, re-derive every appended
                                 // lane's index after the merge, not before.

// *** A Site MUST HAVE STATIC STORAGE DURATION. *** RtDiffRegister keeps the
// POINTER and rt_diff.cpp re-reads it at summary time, so a stack Site dangles
// and the summary reports it as "sites were CORRUPTED -- a stray write hit the
// harness's own statics". Lane Q's probe did that 144 times and it cost the
// integrator most of an escalation; the account is in
// runtime/src/collection/array.cpp. Every Site here is file-scope.
struct Site { RtDiffSite d; };

// The body, and the table the probe drives it against.
void* DeletingDtorCore(void* self, unsigned flags, int i);
void  EhVectorDtor(uint8_t* base, uint32_t elemSize, int count, DtorFn dtor);
const DelDtorSpec* Specs();

// The rows the probe may use as subjects: those whose `~Class` is a pure vtable
// restamp, and which can therefore be run on a fabricated object without
// touching engine state. 13 of the 37. Everything else in the family destroys
// members, releases String references or (for ~Internal) swap-removes a
// handle-table entry, and running one of those on a synthetic object would be
// a fabricated crash rather than a measurement.
const int* ProbeSubjects(int* count);

// The oracle. `ProbeRunStatic` allocates nothing and is called from
// RtObjectDeletingDtorPortInit(); `ProbeRunAllocating` needs the Runtime heap
// and is opt-in. Both no-op unless BANISHED_RT_DELDTOR_PROBE is set.
void ProbeRunStatic();
void ProbeRunAllocating();

// Armed at init when BANISHED_RT_DELDTOR_PROBE=all; the body clears it on the
// first real call. See deleting_dtor_probe.cpp's "WHEN IT RUNS".
extern bool g_allocProbePending;

}  // namespace rtdtor

// Declared in runtime/funcs/rt_port.h too; this is the definition's home.
void RtObjectDeletingDtorPortInit();
