#pragma once
// MarketWork — ENDGAME B.3.8: the ctor, the dtor and the slot bodies are members of
// the class (Work/MarketWork.h). What is left is the file's helper resolver, which
// resourceproduce_creatework.cpp (block MI-B) also calls by name.
//
// CRITICAL: MarketWork's tree uses a DIFFERENT node layout than
// HuntWork/PastureWork/HerbalistWork:
//   +0x00: key (InternalBasePtr)   +0x08: data pointer   +0x10: data2
//   +0x18: parent   +0x20: left child   +0x28: right child   (node size 0x38)
// vs HerbalistWork's nodes: +0x08 parent, +0x10 left, +0x18 right.
//
// Execute (slot 4, retail 0x1400eff80 — Ghidra misnames it "MarketWork_Serialize";
// slot 3 0x1400f1d70 is the real Persist). Signature from the ONLY call site, the
// citizen idle claim-walk 0x1400f7b60 (Helper_sub_7b60, citizenai_housing_idle.cpp):
// (citizenHolder, sched+0x130), and a TRI-STATE return:
//     2 = claimed, the caller's walk STOPS
//     1 = examined but declined; the caller stamps work+0x2c with sched+8
//     0 = keep walking, no stamp
// Slot 9 (Validate) is dispatched immediately before it, unconditionally.

#include "Work/MarketWork.h"
#include <cstdint>

bool MarketWork_ResolveFuncs();
