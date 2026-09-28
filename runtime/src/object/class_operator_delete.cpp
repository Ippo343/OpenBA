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

#include "../../types/system/MemoryAllocator.h"

// B3R W0-A0: the allocator's static, called directly (it went through
// rtcoll::MemFree -> rtcoll::DoorFree(), which answers this same function).
extern "C" void RtClass_OperatorDelete(void* p) {
    System::MemoryAllocator::MemFree(p);    // if (p) { Enter; Free; Leave; }
}

// B3R W0-A0 (R3): the 28 rows that registered RtClass_OperatorDelete under these
// names and addresses had NO reader by name or by address anywhere in the tree
// (code lines, `ull`-suffixed and padded spellings included), so they are
// deleted. The body stays (a body is provenance, R3); the table survives as the
// record of what was read from the shipped DLL's export directory:
//
//   0x180065000  ??3CollisionMesh@Collision2D@@SAXPEAX@Z
//   0x18001f040  ??3FrameCopyImageBuffer@Graphics@@SAXPEAX@Z
//   0x1800010d0  ??3Achievement@0@SAXPEAX@Z  // Achievement::Achievement (self-named class)
//   0x180022260  ??3ReflectionImageBuffer@Graphics@@SAXPEAX@Z
//   0x1800163a0  ??3AnimationGroup@Graphics@@SAXPEAX@Z
//   0x1800643b0  ??3SunLight@Graphics@@SAXPEAX@Z
//   0x1800503d0  ??3SoundEffect@Audio@@SAXPEAX@Z
//   0x180007400  ??3ExternalList@Object@@SAXPEAX@Z
//   0x180012420  ??3GraphicsMesh@Graphics@@SAXPEAX@Z
//   0x180017460  ??3EdgeMeshInstance@Graphics@@SAXPEAX@Z
//   0x180020470  ??3ParticleSystem@Graphics@@SAXPEAX@Z
//   0x18004e5f0  ??3VertexProgram@Video@@SAXPEAX@Z
//   0x18004b600  ??3RenderState@Video@@SAXPEAX@Z
//   0x180001850  ??3AchievementInstance@Achievement@@SAXPEAX@Z
//   0x18000b940  ??3SoundBuffer@Audio@@SAXPEAX@Z
//   0x180011960  ??3Material@Graphics@@SAXPEAX@Z
//   0x18004e970  ??3PixelProgram@Video@@SAXPEAX@Z
//   0x1800169c0  ??3MeshInstance@Graphics@@SAXPEAX@Z
//   0x180064b00  ??3Surface@Collision2D@@SAXPEAX@Z
//   0x180071b40  ??3PickingMesh@Picking@@SAXPEAX@Z
//   0x180020b70  ??3ParticleSystemInstance@Graphics@@SAXPEAX@Z
//   0x18004ece0  ??3VideoProgram@Video@@SAXPEAX@Z
//   0x180012d20  ??3EdgeMesh@Graphics@@SAXPEAX@Z
//   0x18004dd60  ??3VertexLayout@Video@@SAXPEAX@Z
//   0x180063e00  ??3SpotLight@Graphics@@SAXPEAX@Z
//   0x180058ed0  ??3AmbientLight@Graphics@@SAXPEAX@Z
//   0x180070ed0  ??3PointList@Pathfinding@@SAXPEAX@Z
//   0x180001ef0  ??3GoalInstance@Achievement@@SAXPEAX@Z

// Now registers nothing; the coordinator removes its call (runtime/src/object/rt_object_port.cpp).
int RtClassOperatorDeleteRegister() {
    return 0;
}
