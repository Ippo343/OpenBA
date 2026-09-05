// standalone/reg_w2_lane1.cpp
// `.rdata` census wave 2, LANE 1 — the single registration this lane recommends.
//
// Lane 1's six addresses are all exe .rdata VTABLES (no constants — nothing in
// this lane can be silently 6.7x off). Five of the six are verdict
// "deliberate — leave" or "dead"; see the lane-1 report. This file exists for
// the one address where registering is provably safe AND matches retail
// byte-for-byte:
//
//   0x14015f070 — MSVC purecall placeholder vtable.
//
// Read out of the binary: NINE consecutive identical qwords
// (0x14015f070-0x14015f0b7, terminated by a zero qword at 0x14015f0b8), every
// one of them 0x14003addc = `_purecall` (Ghidra names it exactly that; body
// 0x14003addc-0x14003ae0e). It is the table MSVC installs into an abstract
// base's vtable slot while a derived object is under construction or
// destruction. It is never dispatched through — dispatching through it is the
// abort path.
//
// Retail xrefs (all [DATA] stores, no calls): StateMachineComponent_Ctor,
// WorkPlaceComponent_Ctor, MarketLocationComponent_Ctor, NomadComponent_Ctor,
// ClearAreaComponent_Ctor, ResourceProduceComponent_Ctor{,4,5}, and the
// *_Controller_Slot10 dtors. Every one writes it into the controller
// sub-object at self+0x28 and then overwrites it with the real controller
// vtable (ctor) or frees the object (dtor).
//
// Our only consumer is `s_vtable_purecall` in
// code/Game/Component/statemachinecomponent.cpp:100, stored unguarded into
// self+0x28 by StateMachineComponent_ScalarDtor. With the address
// unregistered that store writes NULL. It is a dead store either way — retail
// itself sets the primary vtable to the *Component base* immediately after and
// then calls Object::Internal::~Internal, so no derived virtual can run in
// that window, and SMCs are destroyed routinely (demolition, teardown, the
// 800-citizen stress run) without an EXEC-of-0. Registering therefore changes
// no observable behaviour; it only makes the faithful dtor byte-identical to
// retail and turns a would-be EXEC-of-0 into the same clean abort retail gets.
//
// Safety argument, in full: every slot of this table is the *same* abort stub,
// so unlike every other vtable in this lane there is no unported exe function
// pointer to hand a live caller. That is what makes this one registration
// different from the five leaves.
//
// INTEGRATOR: this file is OPTIONAL — it eliminates one census row and buys
// exact parity, nothing more. Add `RegisterW2Lane1();` to the standalone
// registration sequence (alongside the other reg_*.cpp init calls) if you want
// it; dropping the file entirely is also a correct outcome.

#include <cstdint>
#include <cstdio>
#include <cstdlib>

#include "resolve.h"

namespace {

// Mirror of exe `_purecall` (0x14003addc): a pure virtual was dispatched, which
// is a program error in retail too. Fail loudly rather than returning.
void W2L1_Purecall() {
    std::fprintf(stderr, "[w2_lane1] pure virtual function call (purecall vtable "
                         "0x14015f070 dispatched)\n");
    std::fflush(stderr);
    std::abort();
}

// Nine identical slots, exactly as the binary holds them (the controller
// interface's slot count).
void* g_PurecallVtable[9] = {
    reinterpret_cast<void*>(&W2L1_Purecall),
    reinterpret_cast<void*>(&W2L1_Purecall),
    reinterpret_cast<void*>(&W2L1_Purecall),
    reinterpret_cast<void*>(&W2L1_Purecall),
    reinterpret_cast<void*>(&W2L1_Purecall),
    reinterpret_cast<void*>(&W2L1_Purecall),
    reinterpret_cast<void*>(&W2L1_Purecall),
    reinterpret_cast<void*>(&W2L1_Purecall),
    reinterpret_cast<void*>(&W2L1_Purecall),
};

}  // namespace

void RegisterW2Lane1() {
    RegisterExeAddr(0x14015f070, reinterpret_cast<uintptr_t>(g_PurecallVtable));
}
