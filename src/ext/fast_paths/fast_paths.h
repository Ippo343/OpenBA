// ============================================================================
// src/ext/fast_paths/fast_paths.h -- ext.fast_paths (class B, default OFF)
// ============================================================================
// NOT RETAIL. The game glue around fast_search.h: the once-per-game selection,
// the case-5 dispatch, the per-type cell counts the heuristic scale needs, and
// the shadow check the stress harness compares the two searches with.
// docs/extensions/handins/FASTPATHS.md is the hand-in.
//
// THE SEAMS (src/game/Action/action_pf_astar.cpp, PathfinderThread::ApplyCommand
// 0x1400a6430; both on the pathfinder's worker thread):
//   case 5 (a path request): `Engaged() ? FastPaths::Run(grid, ...)
//                                       : grid.Run(...)` -- retail's line when
//          the game did not select the extension. The result publish after it
//          stays retail's (row 961's race untouched).
//   case 2 (a cell's terrain type): `if (Engaged()) NoteCellTerrain(...)`
//          before retail's store, so the per-type counts follow the grid.
// Faithful cost: one relaxed byte load and a not-taken branch per command 5
// and per command 2. Nothing else; retail's search bodies are untouched.
//
// THE SELECTION. `g_mode` lives here, outside the pinned 0x1F8 PathfinderThread
// (INVENTORY §5.2). GameUpdateLoop::Load calls SelectForGame() twice: right
// after Ext::LockForGame (final for a new game) and again after step 31
// (worldgen or SaveRestore), where a load's manifest has replaced the requested
// set (EXTSAVE §2, Ext::LockForGameWithSet) -- the second call is the one that
// counts. Every command 5 of a game is issued by the running sim or the player
// tools (MoveTo::PathSetup, the road tool, boat routes), all after Load
// returns, so every request of the game sees the final selection. runInline
// (+0x5C) is never set in the tree; were it set, the inline caller would read
// the same global.
// ============================================================================
#pragma once

#include <atomic>
#include <cstdint>

struct GridPathSearch;

namespace FastPaths {

enum : uint8_t {
    kModeFast   = 1,   // the game runs fast_search (ext.fast_paths on)
    kModeShadow = 2,   // the stress harness compares both searches per request
};
// 0 = retail's line. Written by SelectForGame / SetShadowArmed (the loader /
// main thread, never while a game's worker is busy), read per command 5 / 2.
extern std::atomic<uint8_t> g_mode;

inline bool Engaged() { return g_mode.load(std::memory_order_relaxed) != 0; }

// Reads Ext::Ext().fast_paths (final once the class B lock is) and the shadow
// switch into g_mode; marks the per-type counts for a rescan. One stderr line
// (standalone_error.log) per call when anything but retail is selected (Load
// calls it twice, so a game with the extension logs it twice):
//   [FastPaths] selected for this game: fast=<0|1> shadow=<0|1>
void SelectForGame();

// The stress harness's switch (block PFBENCH). Takes effect at once and at
// every later SelectForGame.
void SetShadowArmed(bool armed);

// Command 5 when Engaged(): fills `path` and returns the count exactly as
// GridPathSearch::Run 0x14001ce00 would (retail's own Run when ext.fast_paths
// is off for this game, or when fast_search reports Unsupported), then, in
// shadow mode, calls FastPaths_ShadowHook.
int Run(GridPathSearch& grid, int start, int goal, int32_t* path);

// Command 2 when Engaged(): the cell's flag byte before retail's store and
// command 2's arg2 (the new type: retail stores (old & 0x80) | (uint8)arg2).
void NoteCellTerrain(uint8_t oldFlags, int newType);

// ---------------------------------------------------------------------------
// The shadow check (PATHRESEARCH §5): both searches on the same inputs.
// ---------------------------------------------------------------------------
struct Request {
    GridPathSearch* grid;
    int start;
    int goal;
};

struct ShadowResult {
    int     retailCount = 0, ourCount = 0;       // Run's return values
    int64_t retailCost = -1, ourCost = -1;       // g of the goal, -1 = no path
    bool    sameCells = false;                   // same count and same cells
    bool    ourUnsupported = false;              // fast_search refused (ours = retail's)
    int     retailExpanded = 0, ourExpanded = 0; // cells closed
    int     retailInserted = 0, ourInserted = 0; // relaxations (PathSearch::sequence)
    int32_t scale = 0;                           // the heuristic scale used
    double  retailUs = 0.0, ourUs = 0.0;         // wall time of each search
};

// Runs retail's GridPathSearch::Run and fast_search on the request, on their
// own path buffers (retail's search state is left as its own run leaves it;
// nothing in it outlives a request). Worker thread only. Retail's expansion
// count is read after its run from the node stamps (closedStamp == generation),
// outside the timed span.
ShadowResult Shadow(const Request& req);

}  // namespace FastPaths

// The hook point in command 5 (shadow mode only): runs Shadow(req). Block
// PFBENCH forwards the result to its StressHarness_PathShadow here (the one
// line marked in fast_paths.cpp); this file does not include the harness.
void FastPaths_ShadowHook(const FastPaths::Request& req);
