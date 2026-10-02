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
//     `~Internal`. (The tree's hand-rolled dtor never freed the three arrays.)
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
#include "collection/Collection.h"   // Collection::Array (the three route arrays)

class ComponentEntity;

// One route of the +0x30 array (stride 0x200c, align 4; push 0x140026ba0):
// the two end cells and the A* path between them, capped at 0x800 cells.
struct alignas(4) PathRoute {
    int32_t srcCell;          // +0x00 the citizen's home door (0x1400de5d0)
    int32_t dstCell;          // +0x04 the workplace's anchor cell (0x140087fb0)
    int32_t cellCount;        // +0x08
    int32_t cells[0x800];     // +0x0C
};
static_assert(sizeof(PathRoute) == 0x200c, "PathRoute stride 0x200c (0x140026ba0)");
static_assert(offsetof(PathRoute, dstCell) == 0x04, "PathRoute::dstCell @ +0x04");
static_assert(offsetof(PathRoute, cellCount) == 0x08, "PathRoute::cellCount @ +0x08");
static_assert(offsetof(PathRoute, cells) == 0x0C, "PathRoute::cells @ +0x0C");

// One step of the +0x50 array: two adjacent route cells, lower index first
// (8 bytes at align 4: 0x1400dd770 allocates `cap << 3, 4`).
struct PathSegment {
    int32_t minCell;          // +0x00
    int32_t maxCell;          // +0x04
};
static_assert(sizeof(PathSegment) == 8 && alignof(PathSegment) == 4, "PathSegment is 8 bytes at align 4");
static_assert(offsetof(PathSegment, maxCell) == 0x04, "PathSegment::maxCell @ +0x04");

class PathTool final : public Tool {
    DECLARE_EXE_TYPE(PathTool)

public:
    PathTool() : selectedEntity(nullptr) {}                                // 0x1400de9e0 (the Arrays zero themselves)

    // 0x1400de950: releases the three Collection::Array buffers (+0x50, +0x40,
    // +0x30, in that order) — the inlined ~Array bodies, here the members'
    // own destructors (reverse declaration order = retail's order).
    ~PathTool() override = default;                                        // [0]  0x140026b70
    void Startup() override {}                                             // [4]  0x1400de940 RET
    void Shutdown() override;                                              // [5]  0x1400de930
    char OnKey(char active) override;                                      // [6]  0x1400de6b0
    void Update(void*, char) override {}                                   // [7]  0x1400dd700 RET
    void Draw(Video::Context* context, char active) override;                        // [8]  0x1400dd3b0
    char OnCancel() override { return 0; }                                 // [9]  0x1400de920
    bool IsSelected(uint64_t* ref) override;                               // [10] 0x1400de8d0
    int  GetCursor() const override { return 4; }                         // [12] 0x1400de910
    // [13] 0x140026b60 `MOV RAX,[RCX+0x28]; MOV [RDX],RAX; MOV RAX,RDX; RET`:
    // PathTool's own new virtual — the selected-entity handle, by value through
    // the hidden return slot (B8-11 shape: `this` in RCX, `ret` in RDX).
    virtual uint64_t* GetSelected(uint64_t* ret) const;                    // [13] 0x140026b60

    // SetSelectedEntity 0x1400dd710 (OnKey's pick): a different entity clears
    // the old highlight, stores the entity, highlights it and rebuilds the routes.
    void SetSelected(ComponentEntity* const* entity);                      // 0x1400dd710

    // B4 PLAYER: the three Collection::Arrays retail's dtor 0x1400de950 frees
    // (+0x50, +0x40, +0x30: the inlined ~Array bodies, member order reversed).
    ComponentEntity*                 selectedEntity;   // +0x28 InternalPtr<ComponentEntity>: the selected entity
    Collection::Array<PathRoute>     routes;           // +0x30 the home / workplace routes (push 0x140026ba0)
    Collection::Array<int32_t>       points;           // +0x40 the route cells, deduplicated, ascending
    Collection::Array<PathSegment>   segments;         // +0x50 the route steps, deduplicated, ascending

private:
    void ClearHighlight();   // 0x1400dd250 (slot 5 is a JMP here)
    void SetHighlight();     // 0x1400dd0f0
    void BuildRoutes();      // 0x1400dd770
};


static_assert(sizeof(PathTool) == 0x60, "PathTool is 0x60 (create 0x140026b10 MemAlloc(0x60))");
static_assert(alignof(PathTool) == 8, "PathTool is 8-aligned");
static_assert(offsetof(PathTool, selectedEntity) == 0x28, "PathTool +0x28 (ctor 0x1400dea1b)");
static_assert(offsetof(PathTool, routes)         == 0x30, "PathTool +0x30 (dtor 0x1400de99b)");
static_assert(offsetof(PathTool, points)         == 0x40, "PathTool +0x40 (dtor 0x1400de989)");
static_assert(offsetof(PathTool, segments)       == 0x50, "PathTool +0x50 (dtor 0x1400de975)");
