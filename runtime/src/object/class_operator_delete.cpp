// ============================================================================
// runtime/src/object/class_operator_delete.cpp — batch A_operator_delete, the
// 28 remaining `??3<Class>@<Ns>@@SAXPEAX@Z` class-scoped operator deletes
// ============================================================================
//
// Same discovery as runtime/src/ui/ui_operator_delete.cpp, extended past UI to
// the other 28 classes closure listed under WORKLIST.md's A_operator_delete
// section: Collision2D (2), Graphics (14), Achievement (3), Audio (2),
// Object (1), Video (5), Picking (1), Pathfinding (1) — none of them were
// registered before this file.
//
// NOT ASSUMED FROM THE UI RESULT. Each of the 28 addresses below was read out
// of the shipped Runtime-x64.dll and checked independently for this batch:
//
//   - all 28 bodies are 93 bytes (96-byte export-to-export gap, 3 bytes of
//     alignment padding after), same as every UI row;
//   - byte-compared against runtime/src/ui/ui_operator_delete.cpp's reference
//     body (UI::AnimatedImageDescription, 0x180036760): every one of the 28
//     is byte-identical outside of RIP-relative displacement fields;
//   - disassembled and every RIP-relative operand resolved to an absolute
//     target: all 28 name the SAME three targets the UI batch found —
//       0x1801fc718  ?_singleton@MemoryAllocator@System@@0PEAV12@EA
//       0x18014b390  KERNEL32!EnterCriticalSection (IAT slot)
//       0x1800c6c00  MemoryAllocator::Free
//       0x18014b388  KERNEL32!LeaveCriticalSection (IAT slot)
//
// So these 28 are the same single body as the UI sixty: `if (!p) return;
// Enter(lock); Free(p); Leave(lock);` — instruction for instruction
// `?MemFree@MemoryAllocator@System@@SAXPEAX@Z` (0x1800c7ad0). This file reuses
// rtcoll::MemFree (declared in types/collection/Collection.h, defined in
// runtime/src/collection/array.cpp) rather than duplicating the body, for the
// same reason the UI file gives: one place to repoint when the allocator port
// goes default-on.
//
// Mangled names were read from the export directory of the shipped
// Runtime-x64.dll (pefile), matched by RVA against the closure worklist's 28
// addresses — the export table IS the authority Ghidra's list_exports would
// also read, so a name here is exactly what RegisterRuntimeExport needs.
//
// Hooked from runtime/src/object/rt_object_port.cpp: no dedicated rt_*_port.cpp
// exists for Collision2D or Pathfinding (their factories are registered
// through rt_factories_core.cpp, not a namespace port file), so rather than
// split one shared body across eight call sites this batch takes the one hook
// point common to all of them — Object namespace's port file already owns the
// shared allocator/MemFree awareness these 28 rows are built on.
// ============================================================================

#include <cstdint>

#include "../../funcs/rt_port.h"
#include "../../types/system/MemoryAllocator.h"

// B3R W0-A0: the allocator's static, called directly (it went through
// rtcoll::MemFree -> rtcoll::DoorFree(), which answers this same function).
extern "C" void RtClass_OperatorDelete(void* p) {
    System::MemoryAllocator::MemFree(p);    // if (p) { Enter; Free; Leave; }
}

namespace {

struct OpDelRow {
    uintptr_t   addr;
    const char* mangled;
};

// 28 rows, in worklist order. Each `addr` was checked against the export
// directory of the shipped Runtime-x64.dll (the name is the authority; the
// address is what a by-address call site uses).
const OpDelRow kRows[] = {
    { 0x180065000ull, "??3CollisionMesh@Collision2D@@SAXPEAX@Z" },
    { 0x18001f040ull, "??3FrameCopyImageBuffer@Graphics@@SAXPEAX@Z" },
    { 0x1800010d0ull, "??3Achievement@0@SAXPEAX@Z" },  // Achievement::Achievement (self-named class)
    { 0x180022260ull, "??3ReflectionImageBuffer@Graphics@@SAXPEAX@Z" },
    { 0x1800163a0ull, "??3AnimationGroup@Graphics@@SAXPEAX@Z" },
    { 0x1800643b0ull, "??3SunLight@Graphics@@SAXPEAX@Z" },
    { 0x1800503d0ull, "??3SoundEffect@Audio@@SAXPEAX@Z" },
    { 0x180007400ull, "??3ExternalList@Object@@SAXPEAX@Z" },
    { 0x180012420ull, "??3GraphicsMesh@Graphics@@SAXPEAX@Z" },
    { 0x180017460ull, "??3EdgeMeshInstance@Graphics@@SAXPEAX@Z" },
    { 0x180020470ull, "??3ParticleSystem@Graphics@@SAXPEAX@Z" },
    { 0x18004e5f0ull, "??3VertexProgram@Video@@SAXPEAX@Z" },
    { 0x18004b600ull, "??3RenderState@Video@@SAXPEAX@Z" },
    { 0x180001850ull, "??3AchievementInstance@Achievement@@SAXPEAX@Z" },
    { 0x18000b940ull, "??3SoundBuffer@Audio@@SAXPEAX@Z" },
    { 0x180011960ull, "??3Material@Graphics@@SAXPEAX@Z" },
    { 0x18004e970ull, "??3PixelProgram@Video@@SAXPEAX@Z" },
    { 0x1800169c0ull, "??3MeshInstance@Graphics@@SAXPEAX@Z" },
    { 0x180064b00ull, "??3Surface@Collision2D@@SAXPEAX@Z" },
    { 0x180071b40ull, "??3PickingMesh@Picking@@SAXPEAX@Z" },
    { 0x180020b70ull, "??3ParticleSystemInstance@Graphics@@SAXPEAX@Z" },
    { 0x18004ece0ull, "??3VideoProgram@Video@@SAXPEAX@Z" },
    { 0x180012d20ull, "??3EdgeMesh@Graphics@@SAXPEAX@Z" },
    { 0x18004dd60ull, "??3VertexLayout@Video@@SAXPEAX@Z" },
    { 0x180063e00ull, "??3SpotLight@Graphics@@SAXPEAX@Z" },
    { 0x180058ed0ull, "??3AmbientLight@Graphics@@SAXPEAX@Z" },
    { 0x180070ed0ull, "??3PointList@Pathfinding@@SAXPEAX@Z" },
    { 0x180001ef0ull, "??3GoalInstance@Achievement@@SAXPEAX@Z" },
};

constexpr int kRowCount = static_cast<int>(sizeof(kRows) / sizeof(kRows[0]));
static_assert(kRowCount == 28, "the 28 rows of WORKLIST.md's A_operator_delete batch");

}  // namespace

int RtClassOperatorDeleteRegister() {
    int n = 0;
    for (const OpDelRow& r : kRows) {
        RegisterRuntimeExport(r.mangled, (void*)&RtClass_OperatorDelete);
        RegisterRuntimeAddr(r.addr, (void*)&RtClass_OperatorDelete);
        ++n;
    }
    return n;
}
