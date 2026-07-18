// code/Game/Player/tool_base_standalone.h
// Shared standalone factory/vtable helpers for the Tool family
// (Object::Internal -> Tool / DragTool / ToolSetting). See
// notes/FACTORY_CASCADE_145_BRIEF.md.
//
// Tool subtypes come in three vtable shapes:
//   Tool-derived      : 13 slots (EditTool, PathTool, PlacementTool)
//   DragTool-derived  : 15 slots (BridgeTool, DragPlacementTool, FencedTool,
//                       RemovalTool, RoadTool) — adds slot13 CalcPath / slot14 CalcCost
//   ToolSetting-derived:  4 slots (RotationSetting)
//
// Shared base slots reused by every subtype's standalone vtable:
//   slot1 (+0x08)           -> Tool_Slot1    (returns null)
//   slot3 (+0x18) Serialize -> Tool_Persist  (Object::Internal::Persist base nop)
//   any non-reimplemented   -> Tool_Ret0     (safe stub, returns 0)
// Per-type slots (Dtor, GetTypeId, Startup, OnKey, Draw, Serialize, CalcPath,
// CalcCost, ...) point at the type's own reimpl functions in its .cpp.
#pragma once
#include <cstdint>

extern "C" {

// slot0: generic Internal scalar-deleting dtor. DeletePtr base IBP @+0x10,
// Object::Internal::~Internal, MemFree if (flags&1). Used by ToolSetting types;
// the Tool subclasses use their own reimpl Dtor (more faithful vtable-chain reset).
void* Tool_ScalarDtor(void* self, unsigned int flags);

// slot1 (+0x08): exe returns DAT_1401c3df8, which is statically null.
void* Tool_Slot1(void);

// slot3 (+0x18) Serialize: exe Object::Internal::Persist base (no-op for a fresh
// dormant object). Returns 0 so it is safe whether dispatched as void or value.
void* Tool_Persist(void);

// generic safe stub for any non-reimplemented slot (void or value-returning).
void* Tool_Ret0(void);

// Shared in-place init: Object::Internal::Internal(p); zero type fields
// [0x10, size); if dragBase, set the DragTool base drag-cell defaults
// (bytes [0x2c,0x38) = 0xFF, matching <Tool>_Ctor's startCell/endCell/lastEndCell
// = -1); install the per-type vtable. Type-specific field init beyond this is
// elided (these types are dormant — instantiated for completeness, not driven).
void* Tool_InitInPlace(void* p, int size, void* vtable, int dragBase);

// Shared factory: MemAlloc(size, 8) then Tool_InitInPlace.
void* Tool_Alloc(int size, void* vtable, int dragBase);

}  // extern "C"

