// standalone/stress_harness.h
// BANISHED_STRESS — large-scale stress test + performance baseline harness.
//
// One env var (BANISHED_STRESS=1) turns a fresh autostart new game into a stress town:
// a MEDIUM map, three sets of every placeable building, 800 citizens spread over enough
// houses, running at 10x speed (the most stressful mix measured, see perf/README.md), every workplace staffed through the real seat
// cascade, the camera parked over the town, the FPS cap off, and a machine-readable
// per-interval metrics CSV (standalone_stress.csv) written next to the other logs.
// The run ends itself through the in-game pause menu after BANISHED_STRESS_FRAMES.
//
// Knobs (all optional):
//   BANISHED_STRESS=1              enable (implies BANISHED_AUTOSTART=1)
//   BANISHED_STRESS_CITIZENS=N     target population, default 800
//   BANISHED_STRESS_SETS=N         copies of the full building set, default 3
//   BANISHED_STRESS_FRAMES=N       sim frames before the harness quits, default 6000
//                                  (= 60000 sim ticks at the default 10x; see perf/README.md)
//                                  (0 = run forever, quit by hand)
//   BANISHED_STRESS_INTERVAL=N     frames per CSV row, default 600
//   BANISHED_STRESS_SPEED=N        game speed 1/2/5/10/20/50, default 10
//   BANISHED_STRESS_FOOD_MULT=N    starting-stock multiplier, default citizens/10
//   BANISHED_STRESS_STAFF=0        skip the staffing pass (everyone stays a laborer)
//   BANISHED_STRESS_COMPPROF=0     skip the per-component-type Update profile
//   BANISHED_STRESS_CSV=<path>     CSV output path, default standalone_stress.csv
//
// Anything the harness sets by default (map size, climate, seed, FPS cap, camera pin, the
// water-sited demo buildings, …) can be overridden by setting that env var yourself:
// StressHarness_Init only fills in variables that are UNSET.
#pragma once
#include <cstdint>

extern "C" {

// WinMain, before anything reads env vars: applies the default env bundle.
void StressHarness_Init();
bool StressHarness_Enabled();

int  StressHarness_TargetCitizens();   // 0 when disabled
int  StressHarness_Sets();
int  StressHarness_FoodMult();

// GameUpdateLoop::Update / Draw brackets (gameupdateloop.cpp).
void StressHarness_UpdateBegin(int frameNum);
void StressHarness_UpdateEnd(void* gameUpdateLoop, int frameNum);
void StressHarness_DrawBegin();
void StressHarness_DrawEnd();

// Per-component-type Update profile. g_compProfOn is the ONLY thing the hot loop in
// EntityList_Update reads when the profile is off (one predictable branch).
extern int g_compProfOn;
// Begin() looks the component's type up WHILE IT IS STILL ALIVE (its Update may free it)
// and returns a slot; End() charges the elapsed ticks to that slot.
int  CompProf_Begin(void* comp);
int  CompProf_BeginKind(void* comp, int kind);   // kind 1 = per-frame slot-8 tick ("/tree")
void CompProf_End(int slot, long long tscTicks);   // ticks from CompProf_Now (RDTSC)
#include <x86intrin.h>
static inline long long CompProf_Now() { return (long long)__rdtsc(); }
// One line of sim context (world frame, component Update in flight) for diagnostics
// that fire from another thread (the redzone heap watchdog).
void StressHarness_Context(char* buf, int n);

}
