#pragma once
// tests/unit/system_abi_stubs.h — the one observable the stub file exposes.
// See tests/unit/system_abi_stubs.cpp for why `System::Thread::OnCompleted` is
// the only stub with a body instead of an `abort()`.
extern int g_systemStub_OnCompletedCalls;
