// standalone/tool_harness.cpp
// See tool_harness.h. Env-gated in-game tool-injection harness for the Player
// last-8 debugger pass.
//
// Phase 1 (current): construct a RemovalTool, wire its fields against the live
// EntityUI + a small valid cell rectangle, call Startup, then exercise the
// already-reimplemented CalcCost (and the not-yet-ported CalcPath). This proves
// the harness drives a dormant tool path end-to-end so the debugger can
// validate it. CalcPath (0x1401035e0) is a passthrough (null in standalone)
// until ported — calling it now is a safe no-op that returns 0.
//
// All tool invocation is via raw vtable slots (no C++ decls / name mangling):
//   Tool/DragTool vtable index = byte-offset / 8.
//     +0x20 Startup  = idx 4
//     +0x68 CalcPath = idx 13   (DragTool slot13)
//     +0x70 CalcCost = idx 14   (DragTool slot14)

#ifdef STANDALONE_EXE

#include "tool_harness.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>

// Standalone factory from code/Game/Player/removaltool.cpp (extern "C").
// Returns a freshly MemAlloc'd RemovalTool (0x40 bytes) with its standalone
// vtable installed and drag-cell defaults (startCell/endCell/lastEndCell = -1).
extern "C" void* RemovalTool_Factory_SA(void);

namespace {

// RemovalTool field offsets (DragTool base + RemovalTool).
//   +0x10  entityUI         (set by EntityUI::SetEditMode in the real flow)
//   +0x20  description       (left null here — Startup -> removalDesc=null, guarded)
//   +0x28  dragState (byte)  / +0x29 singleClick — word write 1 = dragging, no single-click
//   +0x2c  startCell (int)
//   +0x30  endCell   (int)
//   +0x38  removalDesc (set by Startup)
constexpr int kOff_EntityUI  = 0x10;
constexpr int kOff_DragState = 0x28;
constexpr int kOff_StartCell = 0x2c;
constexpr int kOff_EndCell   = 0x30;

// Vtable slot indices (byte-offset / 8).
constexpr int kSlot_Startup  = 4;    // +0x20
constexpr int kSlot_CalcPath = 13;   // +0x68
constexpr int kSlot_CalcCost = 14;   // +0x70

bool        s_initialized = false;
bool        s_enabled     = false;
bool        s_fired       = false;
int         s_settleFrame = 60;      // frames after world-build before firing
const char* s_toolName    = "removal";

void Init() {
    if (s_initialized) return;
    s_initialized = true;

    const char* env = std::getenv("BANISHED_TOOL_HARNESS");
    if (!env || env[0] == '\0' ||
        std::strcmp(env, "0") == 0 || std::strcmp(env, "false") == 0) {
        s_enabled = false;
        printf("[ToolHarness] disabled (BANISHED_TOOL_HARNESS unset)\n");
        return;
    }
    s_enabled  = true;
    s_toolName = env;   // currently only "removal" is wired
    printf("[ToolHarness] ENABLED (BANISHED_TOOL_HARNESS=\"%s\") — will fire "
           "the tool sequence at frame %d\n", env, s_settleFrame);
}

// Build a cell index from (row,col) on the 256-wide grid (verified live:
// **(int**)(gameWorld+0xf8) == 256). A modest rectangle near map centre, where
// the start-town valley + scattered resources (trees/stone) live.
inline int Cell(int row, int col) { return 256 * row + col; }

void FireRemovalTool(void* entityUI) {
    printf("[ToolHarness] constructing RemovalTool (entityUI=%p)...\n", entityUI);
    void* tool = RemovalTool_Factory_SA();
    if (!tool) {
        printf("[ToolHarness] ERROR: RemovalTool_Factory_SA returned null\n");
        return;
    }
    uint8_t* t = static_cast<uint8_t*>(tool);

    // Wire fields: entityUI, dragging state, a cell rectangle around centre.
    *reinterpret_cast<void**>(t + kOff_EntityUI) = entityUI;
    *reinterpret_cast<uint16_t*>(t + kOff_DragState) = 1;   // dragState=1, singleClick=0
    const int startCell = Cell(124, 124);
    const int endCell   = Cell(131, 131);                  // 8x8 selection
    *reinterpret_cast<int*>(t + kOff_StartCell) = startCell;
    *reinterpret_cast<int*>(t + kOff_EndCell)   = endCell;
    printf("[ToolHarness] RemovalTool=%p startCell=%d endCell=%d (8x8 @ ~centre)\n",
           tool, startCell, endCell);

    void** vt = *reinterpret_cast<void***>(tool);

    // Startup (+0x20): finds RemovalDescription on description entity (+0x20,
    // null here) -> removalDesc=null. Safe, guarded downstream.
    printf("[ToolHarness] calling Startup (vtable[%d])...\n", kSlot_Startup);
    reinterpret_cast<void(*)(void*)>(vt[kSlot_Startup])(tool);

    // CalcCost (+0x70): already reimplemented — computes the removal-area dims.
    // Validates the harness pipeline live (breakpoint here in the debugger).
    uint32_t outValid = 0xdead;
    int outB = -1, outA = -1;
    uint32_t outLen = 0xbeef;
    printf("[ToolHarness] calling CalcCost (vtable[%d])...\n", kSlot_CalcCost);
    uint64_t cc = reinterpret_cast<uint64_t(*)(void*, int, int, uint32_t*, int*, int*, uint32_t*)>(
        vt[kSlot_CalcCost])(tool, startCell, endCell, &outValid, &outB, &outA, &outLen);
    printf("[ToolHarness] CalcCost -> ret=%llu valid=%u widthB=%d heightA=%d len=%u "
           "(expect width=8 height=8 valid=0 ret=0)\n",
           (unsigned long long)cc, outValid, outB, outA, outLen);

    // CalcPath (+0x68): the gap-8 target 0x1401035e0. Passthrough/no-op until
    // ported; calling it now is safe and proves the trigger site.
    printf("[ToolHarness] calling CalcPath (vtable[%d]) = gap target 0x1401035e0...\n",
           kSlot_CalcPath);
    int cp = reinterpret_cast<int(*)(void*, int, int)>(
        vt[kSlot_CalcPath])(tool, startCell, endCell);
    printf("[ToolHarness] CalcPath -> ret=%d (0 expected while passthrough/unported)\n", cp);

    printf("[ToolHarness] RemovalTool sequence complete.\n");
}

}  // namespace

extern "C" void ToolHarness_Tick(void* entityUI, int frameNum) {
    Init();
    if (!s_enabled || s_fired) return;
    if (!entityUI) return;
    if (frameNum < s_settleFrame) return;

    s_fired = true;
    printf("[ToolHarness] firing at frame %d (tool=\"%s\")\n", frameNum, s_toolName);
    // Only "removal" is wired for now; default to it.
    FireRemovalTool(entityUI);
}

#endif  // STANDALONE_EXE
