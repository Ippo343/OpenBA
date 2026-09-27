#pragma once
// tests/unit/system_abi_stubs.h — the observables the stub file exposes.
// See tests/unit/system_abi_stubs.cpp for why `System::Thread::OnCompleted` is
// the only stub with a body instead of an `abort()` — and, since ENDGAME B.3.9
// SYS W1, why `InitFields` / `ReleaseFields` RECORD instead (the real ctor and
// dtor call them, and the tests pin WHEN).
extern int g_systemStub_OnCompletedCalls;

struct SystemStubThreadCall {
    int          calls;        // how many times the body ran
    const void*  self;         // `this` of the last call
    void*        vptrAtCall;   // the object's +0x00 when the body ran
    unsigned int stackSize;    // InitFields only
};
extern SystemStubThreadCall g_systemStub_InitFields;
extern SystemStubThreadCall g_systemStub_ReleaseFields;
