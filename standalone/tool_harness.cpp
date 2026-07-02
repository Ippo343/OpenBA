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

// Standalone PlacementTool bring-up (GUI_PLAN Keystone F, Phase 1).
//   PlacementTool_Factory_SA          — MemAlloc'd 0x70 PlacementTool + std vtable.
//   PlacementTool_HarnessStartupProbe — logs descriptionPtr's component array +
//     the 4 *Description type-ids, confirms ModelDescription present, then calls
//     the faithful Startup (bails safely if Model absent).
//   StartTown_ResolveTemplateByPath   — resolve "Template\\<Name>.rsc" to a
//     building DESCRIPTION ComponentEntity for descriptionPtr (+0x20).
extern "C" void* PlacementTool_Factory_SA(void);
extern "C" int   PlacementTool_HarnessStartupProbe(void* toolSelf);
extern "C" void* StartTown_ResolveTemplateByPath(const char* path);
// Phase 2: drive the faithful CREATE path (helper_d650) for a chosen centre cell.
extern "C" int   PlacementTool_HarnessPlaceAtCell(void* toolSelf, int centerCell);
// Phase 3B: drive the ghost-preview pipeline (Scene::AddObject + TransformFromYaw +
// Geometry::SetTransform) for a fixed cell — a cursor-free stand-in for Serialize.
extern "C" int   PlacementTool_HarnessPreviewAtCell(void* toolSelf, int centerCell);

// Phase 3: the SetEditMode reimpl the toolbar case-1 click drives (entityui.cpp).
// Walks the tool-mode tree at entityUI+0x10 (built by EntityUI_BuildToolModeTree_SA),
// resolves PlacementTool from the descriptor's CreatePlacedDescription, CreateObject's
// it + Startup, installs it at entityUI+0x28. C++ linkage (not extern "C").
void EntityUI_SetEditMode_Impl(void* playerController, uint64_t mode);

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

// GUI_PLAN Keystone F, Phase 1+2: construct a PlacementTool, point descriptionPtr
// (+0x20) at a resolved simple-building template, run the (probe-guarded) Startup,
// then (Phase 2) drive the faithful CREATE path (helper_d650) at a valid centre
// cell so a real building entity materializes in the world — harness-driven, no
// cursor/keypress. dialogPtr (+0x10) is supplied a zeroed controller by the create
// driver (skips the existing-placement constraint).
void FirePlacementTool() {
    // A simple placeable building the Easy start-town already spawns.
    const char* kTemplate = "Template\\WoodHouse.rsc";
    printf("[ToolHarness] resolving building template \"%s\"...\n", kTemplate);
    void* tmpl = StartTown_ResolveTemplateByPath(kTemplate);
    printf("[ToolHarness] template -> %p\n", tmpl);
    if (!tmpl) {
        printf("[ToolHarness] ERROR: template resolve returned null\n");
        return;
    }

    void* tool = PlacementTool_Factory_SA();
    if (!tool) {
        printf("[ToolHarness] ERROR: PlacementTool_Factory_SA returned null\n");
        return;
    }
    uint8_t* t = static_cast<uint8_t*>(tool);

    // descriptionPtr (+0x20) = the building description entity Startup searches.
    *reinterpret_cast<void**>(t + 0x20) = tmpl;
    printf("[ToolHarness] PlacementTool=%p descriptionPtr(+0x20)=%p — running Startup probe...\n",
           tool, tmpl);

    int ok = PlacementTool_HarnessStartupProbe(tool);
    printf("[ToolHarness] PlacementTool Startup probe -> %d\n", ok);
    if (!ok) {
        printf("[ToolHarness] Startup probe failed — skipping CREATE (Phase 2)\n");
        return;
    }

    // Phase 2: place the building at map centre (same valley cell the RemovalTool
    // harness targets). 256-wide grid; cell(124,124).
    const int centerCell = Cell(124, 124);
    printf("[ToolHarness] PlacementTool CREATE at cell %d (124,124)...\n", centerCell);
    int placed = PlacementTool_HarnessPlaceAtCell(tool, centerCell);
    printf("[ToolHarness] PlacementTool CREATE -> %d (1=building materialized)\n", placed);
    printf("[ToolHarness] PlacementTool sequence complete.\n");
}

// GUI_PLAN Keystone F, Phase 3: exercise the REAL toolbar-click activation path.
// Instead of hand-building the tool (FirePlacementTool), resolve a building descriptor
// and call SetEditMode — exactly what ToolbarDialog::OnEvent case-1 does on a click.
// SetEditMode walks the tool-mode tree (EntityUI_BuildToolModeTree_SA must have run,
// i.e. BANISHED_LIVE_TOOLS=1), matches CreatePlacedDescription -> PlacementTool,
// CreateObject's + Startup's the tool, and installs it at entityUI+0x28. From there the
// per-frame tick (EntityUI_ToolTick_SA, also LIVE_TOOLS-gated) drives OnKey/Serialize:
// the CreateBar dialog opens and the ghost preview follows the mouse; a left-click
// (KeyMap action 5) then places the building. This proves the click->activate->preview
// path without hand-wiring the tool.
void FireActivatePlacement(void* entityUI) {
    if (!entityUI) { printf("[ToolHarness] ERROR: entityUI null\n"); return; }
    const char* kTemplate = "Template\\WoodHouse.rsc";
    printf("[ToolHarness] resolving building template \"%s\"...\n", kTemplate);
    void* tmpl = StartTown_ResolveTemplateByPath(kTemplate);
    printf("[ToolHarness] template -> %p\n", tmpl);
    if (!tmpl) { printf("[ToolHarness] ERROR: template resolve returned null\n"); return; }

    void* before = *reinterpret_cast<void**>(static_cast<uint8_t*>(entityUI) + 0x28);
    printf("[ToolHarness] SetEditMode(entityUI=%p, descriptor=%p) — activating PlacementTool "
           "via the tool-mode tree (activeTool before=%p)...\n", entityUI, tmpl, before);
    EntityUI_SetEditMode_Impl(entityUI, reinterpret_cast<uint64_t>(tmpl));

    void* tool = *reinterpret_cast<void**>(static_cast<uint8_t*>(entityUI) + 0x28);
    printf("[ToolHarness] after SetEditMode: activeTool(entityUI+0x28)=%p\n", tool);
    if (tool) {
        uint8_t* t = static_cast<uint8_t*>(tool);
        printf("[ToolHarness]   tool vtable=%p back-ptr(+0x10)=%p descriptionPtr(+0x20)=%p "
               "descComp(+0x28)=%p meshInstance(+0x68)=%p\n",
               *reinterpret_cast<void**>(t), *reinterpret_cast<void**>(t + 0x10),
               *reinterpret_cast<void**>(t + 0x20), *reinterpret_cast<void**>(t + 0x28),
               *reinterpret_cast<void**>(t + 0x68));
        printf("[ToolHarness] PlacementTool ACTIVE — per-frame tick (LIVE_TOOLS) now drives "
               "OnKey/Serialize; move the mouse over the map + left-click to place.\n");
    } else {
        printf("[ToolHarness] activation FAILED — no tool at entityUI+0x28 (tree miss or "
               "CreateObject null). Ensure BANISHED_LIVE_TOOLS=1.\n");
    }
}

// GUI_PLAN Keystone F, Phase 3B: activate the PlacementTool via the real
// SetEditMode path (as "click" does), then drive the ghost-PREVIEW pipeline for a
// fixed map cell. Serialize's preview normally follows the OS cursor (sub_da00),
// which the headless harness lacks; this proves the preview transform + scene
// submission work by planting a visible ghost mesh at a known cell. Confirms the
// Phase-3B TransformFromYaw fix (was null in standalone → invisible ghost).
void FirePreviewPlacement(void* entityUI) {
    FireActivatePlacement(entityUI);
    void* tool = entityUI ? *reinterpret_cast<void**>(static_cast<uint8_t*>(entityUI) + 0x28) : nullptr;
    if (!tool) {
        printf("[ToolHarness] preview: no active tool — cannot drive ghost preview\n");
        return;
    }
    // Prerequisite check for the CreateBar populate (Phase 3B item 2): drive OnKey
    // (+0x30) with param2=1 — its dialog-open branch calls OpenToolDialog(*(tool+0x10))
    // which loads Dialog/CreateBar.rsc + AddDialog's it, writing the dialog IBP into
    // the controller's +0x00. tool+0x10 = the EntityUI/controller (SetEditMode sets it).
    // Log the controller's +0x00 before/after so we can confirm the CreateBar opens.
    {
        uint8_t* t = static_cast<uint8_t*>(tool);
        void* controller = *reinterpret_cast<void**>(t + 0x10);   // = EntityUI (dialog controller)
        uint64_t before = controller ? *reinterpret_cast<uint64_t*>(controller) : 0;
        void** vt = *reinterpret_cast<void***>(tool);
        typedef char (*OnKey_t)(void*, char);
        ((OnKey_t)vt[6])(tool, 1);   // vt[6] = +0x30 OnKey; param2=1 -> open CreateBar
        uint64_t after = controller ? *reinterpret_cast<uint64_t*>(controller) : 0;
        printf("[ToolHarness] CreateBar open probe: controller(tool+0x10)=%p dialogIBP(+0x00) "
               "before=%llx after=%llx -> %s\n", controller,
               (unsigned long long)before, (unsigned long long)after,
               (after != 0) ? "OPENED" : "not opened");
    }

    const int centerCell = Cell(124, 124);
    printf("[ToolHarness] driving ghost PREVIEW at cell %d (124,124)...\n", centerCell);
    int r = PlacementTool_HarnessPreviewAtCell(tool, centerCell);
    printf("[ToolHarness] ghost PREVIEW -> %d (1=mesh submitted with a valid transform)\n", r);
}

}  // namespace

extern "C" void ToolHarness_Tick(void* entityUI, int frameNum) {
    Init();
    if (!s_enabled || s_fired) return;
    if (frameNum < s_settleFrame) return;

    s_fired = true;
    printf("[ToolHarness] firing at frame %d (tool=\"%s\")\n", frameNum, s_toolName);
    if (std::strcmp(s_toolName, "click") == 0) {
        FireActivatePlacement(entityUI);
    } else if (std::strcmp(s_toolName, "preview") == 0) {
        FirePreviewPlacement(entityUI);
    } else if (std::strcmp(s_toolName, "placement") == 0) {
        FirePlacementTool();
    } else {
        // Default / "removal": the original DragTool path (needs the live EntityUI).
        if (!entityUI) return;
        FireRemovalTool(entityUI);
    }
}

#endif  // STANDALONE_EXE
