#pragma once
// ============================================================================
// code/Game/Player/Tool.h — ENDGAME B.3.8 TOOL (B38_RULES.md §2.1, D1-D3)
//
// `Tool`, the exe's base of the player's world tools (the object EntityUI
// holds at +0x28 and drives every frame), as a REAL C++ class whose vtable the
// compiler generates. Global namespace, as in retail (reflected "Tool", parent
// literal "Object::Internal"; DEFINE_EXE_TYPE with the abstract factory,
// ENDGAME B.3.9 RF).
//
// RETAIL (Application-x64.exe, B38_SLOTS.md §1, bytes read 2026-09-26):
//   ctor   inlined into every leaf ctor (e.g. BridgeTool 0x1400fb890):
//          `??0Internal@Object@@`; stamp 0x140161f10; +0x18 = 0; +0x20 = 0.
//          +0x10 is NOT written (SetEditMode stores the owner there before
//          slot 4); the tree zero-filled it and this class keeps that.
//   table  0x140161f10, 13 slots, create = 0x140003730 (abstract)
//   [0]  0x14002e9b0 scalar deleting dtor: stamp 0x140161f10, `~Internal`,
//                    `if (flags & 1) MemFree`. ~Tool has no body.
//   [1]  0x14002e950 TypeName   [2] 0x14002e940 Type
//   [3]  Object::Internal::Persist (import) — INHERITED
//   [4]-[9] _purecall
//   [10] 0x1400268a0 `XOR AL,AL; RET`       (shared by 8 tables)
//   [11] 0x140004950 `MOV dword [RDX],0; MOV RAX,RDX; RET` — the no-setting
//        answer through the hidden return slot (shared by 19 tables)
//   [12] 0x140105860 `MOV EAX,1; RET`
//
// SLOT NAMES 4-12 have no retail symbol. They are named from the tree's
// callers: Toolbar_ShowGroup_Impl (toolbardialog.cpp: [4] start, [5] close,
// [11] setting type), EntityUI_ToolTick_SA / _ToolDraw_SA
// (tool_activation_standalone.cpp: [6] key, [7] update, [8] draw, [9] cancel,
// [12] cursor), EditTool/PathTool's [10] "is this my selected entity".
// Argument lists are those callers' (a `char` flag is a byte in AL/CL/DL/R8B
// exactly as retail passes it).
//
// LAYOUT: Object::Internal (0x10) + three pointers = 0x28. The field NAMES are
// the ones the tree's slot bodies already use (the old PODs'): +0x10 the tool's
// owner handle (DragTool bodies pass it as the placement dialog's
// InternalBasePtr, "dialogPtr"), +0x18 the ToolSetting IBP SetEditMode hands
// over, +0x20 the tool's description (group) entity.
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "object/Internal.h"
#include "object/Type.h"
#include "Object/exe_type.h"   // DECLARE_EXE_TYPE (ENDGAME B.3.9 RF)

class Tool : public Object::Internal {
    DECLARE_EXE_TYPE(Tool)

public:
    // ---- the 13 slots, in retail order -------------------------------------
    // no body in retail (slot 0 only restamps)
    ~Tool() override = default;                                        // [0]  0x14002e9b0
    // [1] TypeName 0x14002e950 and [2] Type 0x14002e940: DECLARE_EXE_TYPE (retail `MOV RAX,[ti+8]` / `MOV EAX,[ti]`).
    // [3] Persist — Object::Internal::Persist, inherited (retail IMPORT).
    virtual void  Startup() = 0;                                       // [4]  pure
    virtual void  Shutdown() = 0;                                      // [5]  pure
    virtual char  OnKey(char active) = 0;                              // [6]  pure
    virtual void  Update(void* time, char active) = 0;                 // [7]  pure
    virtual void  Draw(void* context, char active) = 0;                // [8]  pure
    virtual char  OnCancel() = 0;                                      // [9]  pure
    virtual bool  IsSelected(uint64_t* entityRef);                     // [10] 0x1400268a0 false
    // [11] RETSLOT (B8-11): the ToolSetting type this tool wants, 0 = none.
    virtual Object::Type* GetSettingType(Object::Type* ret) const;     // [11] 0x140004950
    virtual int   GetCursor() const;                                   // [12] 0x140105860 1

    // ---- the 0x18 bytes Tool adds, retail order -----------------------------
    // PUBLIC while the slot bodies (free functions over the tool pointer) read
    // them by name (decision 1).
    void* dialogPtr;        // +0x10 owner handle (SetEditMode writes it before [4])
    void* toolSetting;      // +0x18 InternalBasePtr<ToolSetting> (ctor zeroes it)
    void* descriptionPtr;   // +0x20 the tool's description entity (ctor zeroes it)

protected:
    Tool() : Object::Internal(), dialogPtr(nullptr), toolSetting(nullptr), descriptionPtr(nullptr) {}
};


static_assert(sizeof(Tool) == 0x28, "Tool is 0x28 bytes (0x10 Internal + 0x18)");
static_assert(alignof(Tool) == 8, "Tool is 8-aligned");
static_assert(offsetof(Tool, dialogPtr)      == 0x10, "Tool +0x10 owner (Toolbar_ShowGroup_Impl)");
static_assert(offsetof(Tool, toolSetting)    == 0x18, "Tool +0x18 setting IBP (ctor zeroes, 0x1400fb8b9)");
static_assert(offsetof(Tool, descriptionPtr) == 0x20, "Tool +0x20 description (ctor zeroes, 0x1400fb8bd)");
