#pragma once
// ============================================================================
// code/Game/Player/PathTool.h — ENDGAME B.3.8 TOOL (B38_RULES.md D1-D5)
//
// The route/path tool (multi-point selection with highlighting, NOT a drag
// tool), a REAL C++ class with a compiler vtable.
//
// RETAIL (Application-x64.exe, B38_SLOTS.md §6, bytes read 2026-09-26):
//   reflected "PathTool", parent "Tool", ti 0x1401c6390
//   create 0x140026b10: MemAlloc(0x60, 8), ctor 0x1400de9e0: `??0Internal`;
//     Tool part; stamp 0x140160680; +0x28..+0x58 = 0 (seven qwords)
//   createInPlace 0x140026b50: `if (storage) ctor(storage)`
//   dtor 0x1400de950 (Ghidra: "Tool_CopyCtor"): stamp 0x140160680;
//     MemFree([+0x50]), +0x50 = +0x58 = 0; MemFree([+0x40]), +0x40 = +0x48 = 0;
//     MemFree([+0x30]), +0x30 = +0x38 = 0; stamp 0x140161f10; tail-JMP
//     `~Internal`. The tree's dtor never freed the three arrays (PORT GAP).
//   table 0x140160680, 14 slots:
//     [0]  0x140026b70 scalar deleting dtor (CALL the dtor, MemFree on &1)
//     [1]  0x140026b00 TypeName   [2] 0x140026af0 Type   [3] = parent
//     [4]  0x1400de940 `RET`
//     [5]  0x1400de930 JMP 0x1400dd250: clear the selection highlight
//     [6]  0x1400de6b0 OnKey: pick / select the entity under the cursor
//     [7]  0x1400dd700 `RET`
//     [8]  0x1400dd3b0 Draw: the path preview
//     [9]  0x1400de920 `XOR AL,AL; RET`
//     [10] 0x1400de8d0 IsSelected(entityRef)
//     [11] = Tool's (no setting)
//     [12] 0x1400de910 `MOV EAX,4; RET` (cursor)
//     [13] 0x140026b60 PathTool's OWN new virtual: `*ret = [this+0x28]` —
//          the selected entity handle through the hidden return slot
//   sizeof 0x60.
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "Player/Tool.h"
#include "Object/exe_type.h"

class PathTool final : public Tool {
    DECLARE_EXE_TYPE(PathTool)

public:
    PathTool()                                                             // 0x1400de9e0
        : selectedEntity(0), _field30(nullptr), _field38(nullptr), _field40(nullptr),
          _field48(0), _pad4C{}, _field50(nullptr), _field58(0), _pad5C{} {}

    Object::Base* DeletingDtor(unsigned flags) override;                   // [0]  0x140026b70
    void Startup() override {}                                             // [4]  0x1400de940 RET
    void Shutdown() override;                                              // [5]  0x1400de930
    char OnKey(char active) override;                                      // [6]  0x1400de6b0
    void Update(void*, char) override {}                                   // [7]  0x1400dd700 RET
    void Draw(void* context, char active) override;                        // [8]  0x1400dd3b0
    char OnCancel() override { return 0; }                                 // [9]  0x1400de920
    bool IsSelected(uint64_t* ref) override;                               // [10] 0x1400de8d0
    int  GetCursor() const override { return 4; }                         // [12] 0x1400de910
    // [13] 0x140026b60 `MOV RAX,[RCX+0x28]; MOV [RDX],RAX; MOV RAX,RDX; RET`:
    // PathTool's own new virtual — the selected-entity handle, by value through
    // the hidden return slot (B8-11 shape: `this` in RCX, `ret` in RDX).
    virtual uint64_t* GetSelected(uint64_t* ret) const { *ret = selectedEntity; return ret; }

    uint64_t selectedEntity;   // +0x28 InternalBasePtr: the selected entity
    void*    _field30;         // +0x30 array data (freed by the retail dtor)
    void*    _field38;         // +0x38 its size/capacity pair
    void*    _field40;         // +0x40 int[] path-point cells
    int32_t  _field48;         // +0x48 path-point count
    uint8_t  _pad4C[4];        // +0x4c
    void*    _field50;         // +0x50 int-pair[] line segments
    int32_t  _field58;         // +0x58 segment count
    uint8_t  _pad5C[4];        // +0x5c

protected:
    // PORT GAP (retail 0x1400de950 frees +0x50/+0x40/+0x30) — B38 D9: the
    // tree's dtor kept the arrays; the retail body is its own behaviour commit.
    ~PathTool() = default;
};

namespace PathToolAddr {
    constexpr uintptr_t Vtable = 0x140160680;   // retail table (documentation)
}

static_assert(sizeof(PathTool) == 0x60, "PathTool is 0x60 (create 0x140026b10 MemAlloc(0x60))");
static_assert(alignof(PathTool) == 8, "PathTool is 8-aligned");
static_assert(offsetof(PathTool, selectedEntity) == 0x28, "PathTool +0x28 (ctor 0x1400dea1b)");
static_assert(offsetof(PathTool, _field30)       == 0x30, "PathTool +0x30 (dtor 0x1400de99b)");
static_assert(offsetof(PathTool, _field40)       == 0x40, "PathTool +0x40 (dtor 0x1400de989)");
static_assert(offsetof(PathTool, _field48)       == 0x48, "PathTool +0x48");
static_assert(offsetof(PathTool, _field50)       == 0x50, "PathTool +0x50 (dtor 0x1400de975)");
static_assert(offsetof(PathTool, _field58)       == 0x58, "PathTool +0x58");
