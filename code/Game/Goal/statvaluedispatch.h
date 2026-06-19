// code/Game/Goal/statvaluedispatch.h
// Declaration for the FinishTutorialInstance stat-value dispatcher (exe 0x1400da4e0).
// Portable — implemented in statvaluedispatch.cpp; depends only on engine_funcs.h.
#pragma once
#include <cstdint>

// Faithful port of FinishTutorialInstance_vslot17_sub_a4e0 (0x1400da4e0):
// a switch(statType) over ~0x1e goal-stat queries against the GameWorld.
//   self     = param_1 (the FinishTutorialInstance / goal object; only +0x78/+0x7c read)
//   statType = param_2 (the stat selector, 0..0x1d)
//   ext      = param_3 (a Base* external; profession/generator filter for some cases)
// Every case yields a numeric stat count returned via EAX; we return uint32_t.
uint32_t StatGoal_StatValueDispatch(void* self, uint32_t statType, void* ext);
