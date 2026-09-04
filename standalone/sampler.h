// Sampling profiler for the sim thread (env BANISHED_SAMPLE=<hz>, default 1000).
//
// A helper thread suspends the main thread on a timer, unwinds its stack, and records
// (tag, frames). Samples are appended to standalone_samples.txt in the working
// directory; tools/sample_report.py aggregates them (self/inclusive per function,
// per tag). The tag says what the sim was doing: 0 = outside GameUpdateLoop::Update
// (draw, message pump), 1 = inside Update but no phase, 2+ph = the SIMPROF phase
// (2 PairCache, 3 Animation, 4 EntityList, 5 Weather, 6 CitizenList, 7 PostSim).
//
// Why: the per-component profile (COMPPROF) only sees the vtable calls the entity
// list makes; time inside Runtime-x64.dll (allocator, containers, math) and inside
// our own helpers is invisible to it. Sampling sees everything.
#pragma once

extern volatile int g_samplerTag;

void Sampler_Init();    // no-op unless BANISHED_SAMPLE is set; call after HwWatch_Init
void Sampler_Flush();   // append pending samples to the file (also run at exit)
