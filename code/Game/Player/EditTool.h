#pragma once
// ============================================================================
// code/Game/Player/EditTool.h — ENDGAME B.3.8 TOOL (B38_RULES.md D1-D5)
//
// The idle world-selection tool (left click picks an entity and opens its
// info panel), a REAL C++ class with a compiler vtable.
//
// RETAIL (Application-x64.exe, B38_SLOTS.md §6, bytes read 2026-09-26):
//   reflected "EditTool", parent "Tool", ti 0x1401c6dd8
//   create 0x14002daa0: MemAlloc(0x38, 8), ctor 0x1400fd340: `??0Internal`;
//     Tool part; stamp 0x140161ca0; `[0x140152a90](this+0x28, 0)` (the +0x28
//     handle constructed empty); +0x30 = 0
//   createInPlace 0x14002dae0: `if (storage) ctor(storage)`
//   dtor 0x1400fd300: restamps only, tail-JMP `~Internal`
//   table 0x140161ca0, 14 slots:
//     [0]  0x14002db00 scalar deleting dtor
//     [1]  0x14002da90 TypeName   [2] 0x14002da80 Type   [3] = parent
//     [4]  0x1400fd2f0 `RET`
//     [5]  0x1400fd2e0 JMP 0x1400fcc90: deselect + close the info dialog
//     [6]  0x1400fd030 OnKey: pick the entity under the cursor
//     [7]  0x1400fcda0 Update: keep the info dialog positioned / close it
//          when its entity died
//     [8]  0x1400fcd90 `RET`       [9] 0x1400fd2d0 `XOR AL,AL; RET`
//     [10] 0x1400fd290 IsSelected(entityRef)
//     [11] = Tool's (no setting)   [12] = Tool's (`MOV EAX,1` cursor)
//     [13] 0x14002daf0 EditTool's OWN new virtual: `*ret = [this+0x30]` —
//          the selected entity handle through the hidden return slot
//   sizeof 0x38.
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "Player/Tool.h"
#include "Object/exe_type.h"

class EditTool final : public Tool {
    DECLARE_EXE_TYPE(EditTool)

public:
    EditTool() : entityRef(0), lastEntityRef(0) {}                         // 0x1400fd340

    Object::Base* DeletingDtor(unsigned flags) override;                   // [0]  0x14002db00
    void Startup() override {}                                             // [4]  0x1400fd2f0 RET
    void Shutdown() override;                                              // [5]  0x1400fd2e0
    char OnKey(char active) override;                                      // [6]  0x1400fd030
    void Update(void* time, char active) override;                         // [7]  0x1400fcda0
    void Draw(void*, char) override {}                                     // [8]  0x1400fcd90 RET
    char OnCancel() override { return 0; }                                 // [9]  0x1400fd2d0
    bool IsSelected(uint64_t* ref) override;                       // [10] 0x1400fd290
    // [11] no setting / [12] cursor 1 (0x140105860): Tool's, inherited. The old
    // hand table put a `return 0` stub in slot 12 (cursor id 0).
    // [13] 0x14002daf0 `MOV RAX,[RCX+0x30]; MOV [RDX],RAX; MOV RAX,RDX; RET`:
    // EditTool's own new virtual — the selected-entity handle, by value through
    // the hidden return slot (B8-11 shape: `this` in RCX, `ret` in RDX).
    virtual uint64_t* GetSelected(uint64_t* ret) const { *ret = lastEntityRef; return ret; }

    uint64_t entityRef;       // +0x28 InternalBasePtr: the open info dialog
    uint64_t lastEntityRef;   // +0x30 InternalBasePtr: the selected entity

protected:
    ~EditTool() = default;                                                 // 0x1400fd300: no body
};

namespace EditToolAddr {
    constexpr uintptr_t Vtable = 0x140161ca0;   // retail table (documentation)
}

static_assert(sizeof(EditTool) == 0x38, "EditTool is 0x38 (create 0x14002daa0 MemAlloc(0x38))");
static_assert(alignof(EditTool) == 8, "EditTool is 8-aligned");
static_assert(offsetof(EditTool, entityRef)     == 0x28, "EditTool +0x28 (ctor 0x1400fd381)");
static_assert(offsetof(EditTool, lastEntityRef) == 0x30, "EditTool +0x30 (ctor 0x1400fd38d)");
