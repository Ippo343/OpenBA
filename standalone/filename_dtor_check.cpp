// standalone/filename_dtor_check.cpp
//
// Session 3 (2026-06-15) diagnostic: catch the System::String use-after-free
// that plants the bucket-12 free-list bit-32 corruption (the residual full-speed
// menu heap crash).
//
// Mechanism (decoded from the Runtime DLL this session):
//   - System::Filename is 8 bytes = a single System::String* at +0x00.
//   - A System::String pointer points at the CHAR data; its refcount lives at
//     string-4; the alloc base is string-8.
//   - ~Filename / operator= do `decl dword[string-4]` (release) and free when 0.
//   - The crash is a `decl`/`incl` on an ALREADY-FREED String (refcount slot,
//     block+4, becomes 0xffffffff or 1 -> corrupts the free-list link high dword
//     -> +0x100000000 bit-32 flip -> wild Allocate write later).
//
// This wrapper interposes the global fn_Filename_Dtor: before delegating to the
// real ~Filename, it reads the String's refcount. A refcount <= 0 (or absurdly
// large) means the String was ALREADY freed and we are about to do the UAF
// decrement -> we log the caller (return address in reimpl-x64.exe) so the
// offending helper can be addr2line'd. Quiet on every healthy Dtor (refcount>=1).
//
// Env-gated by BANISHED_FNDTOR_CHECK (default OFF). stderr is the reliable log
// channel in standalone (stdout/standalone_debug.log is lost on hard-kill).

#include "engine_funcs.h"
#include "options.h"
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <windows.h>

namespace {
fn_Filename_Dtor_t s_realDtor = nullptr;
int s_fired = 0;
int s_calls = 0;

void CheckingFilenameDtor(void* self) {
    if (self) {
        void* s = *reinterpret_cast<void**>(self);
        if (s) {
            uint8_t* rcAddr = reinterpret_cast<uint8_t*>(s) - 4;
            if (!IsBadReadPtr(rcAddr, 4)) {
                int rc = *reinterpret_cast<int32_t*>(rcAddr);
                if (rc <= 0 || rc > 0x00100000) {
                    if (s_fired < 60) {
                        ++s_fired;
                        void* ret = __builtin_return_address(0);
                        uintptr_t base =
                            reinterpret_cast<uintptr_t>(GetModuleHandleA(nullptr));
                        fprintf(stderr,
                            "[FNDtorCheck] UAF self=%p string=%p refcount=%d "
                            "caller=%p (reimpl+0x%llx) call#%d\n",
                            self, s, rc, ret,
                            (unsigned long long)((uintptr_t)ret - base), s_calls);
                        fflush(stderr);
                    }
                }
            }
        }
    }
    ++s_calls;
    if (s_realDtor) s_realDtor(self);
}
} // namespace

// Same UAF check for fn_Filename_AssignOp (operator=). operator= will both
// RELEASE dst's old string (decl) and ADDREF src's string (incl); if either
// operand's string already has refcount<=0 it is a use-after-free at our call
// site. Catches the `incl`-on-stale case that the Dtor wrapper (release only)
// cannot.
namespace {
fn_Filename_AssignOp_t s_realAssign = nullptr;
int s_assignFired = 0;

void CheckOne(void* fnameBuf, const char* which, void* ret) {
    if (!fnameBuf) return;
    void* s = *reinterpret_cast<void**>(fnameBuf);
    if (!s) return;
    uint8_t* rcAddr = reinterpret_cast<uint8_t*>(s) - 4;
    if (IsBadReadPtr(rcAddr, 4)) return;
    int rc = *reinterpret_cast<int32_t*>(rcAddr);
    if (rc <= 0 || rc > 0x00100000) {
        if (s_assignFired < 60) {
            ++s_assignFired;
            uintptr_t base = reinterpret_cast<uintptr_t>(GetModuleHandleA(nullptr));
            fprintf(stderr,
                "[FNDtorCheck] UAF(assign %s) buf=%p string=%p refcount=%d "
                "caller=%p (reimpl+0x%llx)\n",
                which, fnameBuf, s, rc, ret,
                (unsigned long long)((uintptr_t)ret - base));
            fflush(stderr);
        }
    }
}

void* CheckingFilenameAssign(void* dst, const void* src) {
    void* ret = __builtin_return_address(0);
    CheckOne(dst, "dst-old", ret);
    CheckOne(const_cast<void*>(src), "src", ret);
    return s_realAssign ? s_realAssign(dst, src) : dst;
}
} // namespace

extern "C" void InstallFilenameDtorCheck() {
    if (!Opt().fnDtorCheck) return;
    if (!fn_Filename_Dtor) {
        fprintf(stderr, "[FNDtorCheck] fn_Filename_Dtor null — not installed\n");
        fflush(stderr);
        return;
    }
    s_realDtor = fn_Filename_Dtor;
    fn_Filename_Dtor = &CheckingFilenameDtor;
    if (fn_Filename_AssignOp) {
        s_realAssign = fn_Filename_AssignOp;
        fn_Filename_AssignOp = &CheckingFilenameAssign;
    }
    fprintf(stderr, "[FNDtorCheck] installed (dtor=%p assign=%p)\n",
            (void*)s_realDtor, (void*)s_realAssign);
    fflush(stderr);
}
