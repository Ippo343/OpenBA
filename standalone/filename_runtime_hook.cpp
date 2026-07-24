// standalone/filename_runtime_hook.cpp
//
// Session 4 (2026-06-15) diagnostic for the intermittent menu heap crash.
//
// Session 3 proved the corrupting refcount op is RUNTIME-INTERNAL: it wrapped
// the GLOBAL function pointers fn_Filename_Dtor / fn_Filename_AssignOp
// (standalone/filename_dtor_check.cpp) and they fired 0x across 8 full-speed
// crashes. Those globals only intercept OUR explicit calls; the over-release /
// AddRef-on-freed happens inside Runtime code calling its OWN ~Filename /
// operator= (or a String op) on a freed bucket-12 path String.
//
// This instrument detours the ACTUAL Runtime functions at their addresses, so
// EVERY caller — including Runtime-internal ones — routes through a refcount
// check BEFORE the dec/inc executes. The exact corrupting instructions
// (disassembled from Runtime-x64.dll this session):
//
//   System::Filename::~Filename  @ 0x180006980
//     mov rax,[rcx]              ; rax = String* (Filename+0)
//     test rax,rax / je end
//     dec DWORD PTR [rax-0x4]    ; <-- release; UAF if [rax-4] already 0
//     ...
//   System::Filename::operator=  @ 0x180006770   (dst=rcx, src=rdx)
//     mov rax,[rcx] ; cmp rax,[rdx] ; je end
//     dec DWORD PTR [rax-0x4]    ; <-- release dst's old String
//     ...
//     mov rax,[rdx] ; mov [rcx],rax ; test rax,rax / je end
//     inc DWORD PTR [rax-0x4]    ; <-- AddRef src; the +0x100000000 bit-32 flip
//                                 ;     when src is a FREED String (refcount==0)
//
// String layout: a String* points at the char data; refcount = *(int*)(str-4);
// the allocator user pointer = str-8, so str-4 == user+4 == the freed block's
// free-list-link HIGH dword. A `dec`/`inc` on a freed String (whose link high
// dword is 0) writes -1/+1 there -> wild free-list link -> the crash.
//
// So: refcount == 0 (or < 0, or absurdly large) immediately BEFORE the op = the
// use-after-free. We log the operand String, its block, the path content, and a
// deep symbolized stack (which names the geometry/FindOrLoad caller). Default
// log-and-continue (so the existing redzone watchdog still confirms the same
// block crashes); BANISHED_FNHOOK_ABORT=1 ExitProcess()es at the first catch for
// a clean deterministic capture.
//
// Env: BANISHED_FNHOOK=1 enable (default OFF). stderr is the reliable channel
// (stdout/standalone_debug.log is lost on a hard-kill crash).


#include "resolve.h"

#include <windows.h>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

typedef void  (*FilenameDtorFn)(void* self);
typedef void* (*FilenameAssignFn)(void* dst, const void* src);

FilenameDtorFn   g_origDtor    = nullptr;  // trampoline -> real ~Filename body
FilenameAssignFn g_origAssign  = nullptr;  // trampoline -> real operator=(String&)
FilenameAssignFn g_origAssignFn = nullptr; // trampoline -> real operator=(Filename&)
FilenameAssignFn g_origCtorStr = nullptr;  // trampoline -> real Filename(const String&)

volatile LONG g_events   = 0;
bool          g_abort    = false;
int           g_maxEvents = 80;

// Resolve a code address to "module.dll+0xOFF" (names the Runtime caller).
void LogModOff(const char* label, uint64_t addr) {
    HMODULE mod = nullptr;
    if (addr && GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                                   GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                                   (LPCSTR)addr, &mod) && mod) {
        char path[MAX_PATH] = {0};
        GetModuleFileNameA(mod, path, MAX_PATH);
        const char* nm = strrchr(path, '\\');
        fprintf(stderr, "[FNHook]   %s %s+0x%llX\n",
                label, nm ? nm + 1 : path,
                (unsigned long long)(addr - (uintptr_t)mod));
    } else {
        fprintf(stderr, "[FNHook]   %s 0x%016llx (no module)\n",
                label, (unsigned long long)addr);
    }
}

// --- fs+0x08 data-dir String trajectory watch -----------------------------
// The UAF victim is the IO::FileSystem data-directory Filename (fs+0x08), whose
// String GetDataDirectory AddRefs during every FindOrLoad path resolution. To
// find WHO over-releases it (drives its refcount to 0), we log every refcount
// op on that exact String when its count is low (<=2 = the danger zone: the
// freeing release 1->0 and the UAF AddRef on 0). fs = Runtime 0x1801fc6a8.
volatile LONG g_ddEvents = 0;
bool          g_watchDataDir = false;

void* volatile g_ddUserPtr = nullptr;  // cached data-dir String user ptr (String-8)

void* CurrentDataDirString() {
    static void** s_fsSlot = nullptr;
    if (!s_fsSlot)
        s_fsSlot = reinterpret_cast<void**>(ResolveAddr(Module::Runtime, 0x1801fc6a8));
    if (!s_fsSlot || IsBadReadPtr(s_fsSlot, 8)) return nullptr;
    uint8_t* fs = reinterpret_cast<uint8_t*>(*s_fsSlot);
    if (!fs || IsBadReadPtr(fs + 8, 8)) return nullptr;
    void* str = *reinterpret_cast<void**>(fs + 8);  // fs+0x08 = data-dir Filename's String*
    if (str && !g_ddUserPtr)
        g_ddUserPtr = reinterpret_cast<uint8_t*>(str) - 8;  // cache once for the free watch
    return str;
}

// MemFreeStatic (Runtime 0x1800c7ad0) detour — the inline String-release idiom
// (`dec [str-4]; if 0 { mov rcx,str-8; call 0x1800c7ad0 }`) frees a block by
// calling this directly (bypassing our global fn_MemFreeStatic). When the block
// being freed IS the data-dir String (user ptr = String-8 = g_ddUserPtr), log
// the caller's stack = the OVER-RELEASE site we're hunting. The freeing dec is
// inlined, so this is the only chokepoint that names it.
typedef void (*MemFreeFn)(void*);
MemFreeFn g_origMemFreeBody = nullptr;  // trampoline: push rdi;sub;cookie; JMP MemFree+0x13
volatile LONG g_ddFreeEvents = 0;

void MemFreeWrapper(void* ptr) {
    if (!ptr) return;  // reproduce the original `test rcx,rcx; je epilogue`
    if (g_watchDataDir && ptr == g_ddUserPtr) {
        LONG n = InterlockedIncrement(&g_ddFreeEvents);
        if (n <= 20) {
            void* str = reinterpret_cast<uint8_t*>(ptr) + 8;
            int rc = -999;
            if (!IsBadReadPtr(reinterpret_cast<int*>(str) - 1, 4))
                rc = *(reinterpret_cast<int*>(str) - 1);
            fprintf(stderr, "[FNHook] *** DATADIR-FREE *** block-user=%p (String=%p "
                    "refcount=%d) being FREED — the over-release! #%ld\n",
                    ptr, str, rc, (long)n);
            void* frames[20] = {0};
            USHORT got = RtlCaptureStackBackTrace(1, 20, frames, nullptr);
            for (USHORT i = 0; i < got; ++i) {
                char lbl[12]; snprintf(lbl, sizeof(lbl), "  ff[%2u]", i);
                LogModOff(lbl, reinterpret_cast<uint64_t>(frames[i]));
            }
            fflush(stderr);
        }
    }
    g_origMemFreeBody(ptr);
}

// isRelease = this op is a dec (release). The baseline refcount of fs+0x08 sits
// at 2 (GetDataDirectory AddRefs to 3, the caller releases back to 2). The
// over-release is a DEC that pushes it BELOW baseline (2->1, then 1->0), so we
// log releases at refcount<=2 (the danger decs) and ANY op at refcount<=0 (the
// UAF). Benign AddRefs at the steady-state 2 are suppressed (pure noise).
void MaybeLogDataDir(const char* op, void* str, void* caller, bool isRelease) {
    if (!g_watchDataDir || !str || str != CurrentDataDirString()) return;
    int* rc = reinterpret_cast<int*>(str) - 1;
    if (IsBadReadPtr(rc, 4)) return;
    int v = *rc;
    if (!((isRelease && v <= 2) || v <= 0)) return;
    LONG n = InterlockedIncrement(&g_ddEvents);
    if (n > 250) return;
    HMODULE mod = nullptr;
    uint64_t a = reinterpret_cast<uint64_t>(caller); uint64_t off = a;
    if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
            GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)a, &mod) && mod)
        off = a - (uintptr_t)mod;
    fprintf(stderr, "[FNHook] DATADIR %s refcount=%d string=%p caller=+0x%llX #%ld\n",
            op, v, str, (unsigned long long)off, (long)n);
    void* frames[16] = {0};
    USHORT got = RtlCaptureStackBackTrace(2, 16, frames, nullptr);
    for (USHORT i = 0; i < got; ++i) {
        char lbl[12]; snprintf(lbl, sizeof(lbl), "  dd[%2u]", i);
        LogModOff(lbl, reinterpret_cast<uint64_t>(frames[i]));
    }
    fflush(stderr);
}

// Read the refcount of a String*; return true if it looks USE-AFTER-FREE
// (refcount 0 = freed block linked with link-high-dword 0; <0 = already
// flipped; or absurdly large = garbage/reused). Live strings are 1..~1e6.
bool IsUAF(void* str, int& rcOut) {
    if (!str) return false;
    int* rc = reinterpret_cast<int*>(str) - 1;  // str - 4
    if (IsBadReadPtr(rc, 4)) return false;
    int v = *rc;
    rcOut = v;
    return (v <= 0 || v > 0x00400000);
}

void ReportUAF(const char* op, void* str, int rc, void* caller) {
    LONG n = InterlockedIncrement(&g_events);
    if (n > g_maxEvents) return;

    // The allocator user pointer = str-8; the boundary-tag block = str-16.
    uint8_t* user  = reinterpret_cast<uint8_t*>(str) - 8;
    uint8_t* block = reinterpret_cast<uint8_t*>(str) - 16;

    fprintf(stderr,
        "\n[FNHook] *** UAF refcount op (%s) #%ld ***  string=%p refcount=%d "
        "(user=%p block=%p)\n", op, (long)n, str, rc, (void*)user, (void*)block);

    // Dump the String char data — names the resource path being touched.
    if (!IsBadReadPtr(str, 48)) {
        char asc[49];
        const uint8_t* s = reinterpret_cast<const uint8_t*>(str);
        for (int i = 0; i < 48; ++i) {
            char c = (char)s[i];
            asc[i] = (c >= 32 && c < 127) ? c : '.';
        }
        asc[48] = 0;
        fprintf(stderr, "[FNHook]   path/data: '%s'\n", asc);
    }
    LogModOff("immediate caller:", reinterpret_cast<uint64_t>(caller));

    // Deep stack — reveals the geometry / MaterialInstance::Serialize /
    // ExternalFactory::FindOrLoad call chain that triggered the Runtime-internal
    // refcount op (RtlCaptureStackBackTrace needs no symbols / dbghelp lock).
    void* frames[24] = {0};
    USHORT got = RtlCaptureStackBackTrace(1, 24, frames, nullptr);
    fprintf(stderr, "[FNHook]   stack (%u frames):\n", got);
    for (USHORT i = 0; i < got; ++i) {
        char lbl[16];
        snprintf(lbl, sizeof(lbl), "[%2u]", i);
        LogModOff(lbl, reinterpret_cast<uint64_t>(frames[i]));
    }
    fflush(stderr);

    if (g_abort) {
        fprintf(stderr, "[FNHook] BANISHED_FNHOOK_ABORT=1 -> ExitProcess(0x130) "
                "for a clean source capture\n");
        fflush(stderr);
        ExitProcess(0x130);
    }
}

// ~Filename wrapper: check the to-be-released String before delegating.
void CheckedDtor(void* self) {
    if (self) {
        void* str = *reinterpret_cast<void**>(self);
        int rc = 0;
        if (IsUAF(str, rc))
            ReportUAF("~Filename dec", str, rc, __builtin_return_address(0));
        MaybeLogDataDir("~Filename dec(release)", str, __builtin_return_address(0), true);
    }
    g_origDtor(self);
}

// operator= wrapper: the AddRef of src (inc) is the prime suspect (the +1 that
// makes link-high-dword 1 -> +0x100000000). Also check dst's old String (dec).
void* CheckedAssign(void* dst, const void* src) {
    void* caller = __builtin_return_address(0);
    if (dst) {
        void* dstStr = *reinterpret_cast<void**>(dst);
        int rc = 0;
        // Only a UAF if dst != src (operator= skips the dec when equal).
        if (src && dstStr != *reinterpret_cast<void* const*>(src)) {
            if (IsUAF(dstStr, rc))
                ReportUAF("operator= dec(dst-old)", dstStr, rc, caller);
            MaybeLogDataDir("operator=(String) dec(dst-old release)", dstStr, caller, true);
        }
    }
    if (src) {
        void* srcStr = *reinterpret_cast<void* const*>(src);
        int rc = 0;
        if (IsUAF(srcStr, rc))
            ReportUAF("operator= inc(src AddRef)", srcStr, rc, caller);
        MaybeLogDataDir("operator=(String) inc(src)", srcStr, caller, false);
    }
    return g_origAssign(dst, src);
}

// operator=(const Filename&) @ 0x180007d40 — same dec(dst-old)+inc(src) idiom
// as operator=(String&); GetPath/GetPathForReferenceId assignments flow here.
void* CheckedAssignFilename(void* dst, const void* src) {
    void* caller = __builtin_return_address(0);
    if (dst && src) {
        void* dstStr = *reinterpret_cast<void**>(dst);
        int rc = 0;
        if (dstStr != *reinterpret_cast<void* const*>(src)) {
            if (IsUAF(dstStr, rc))
                ReportUAF("op=(Filename) dec(dst-old)", dstStr, rc, caller);
            MaybeLogDataDir("op=(Filename) dec(dst-old release)", dstStr, caller, true);
        }
    }
    if (src) {
        void* srcStr = *reinterpret_cast<void* const*>(src);
        int rc = 0;
        if (IsUAF(srcStr, rc))
            ReportUAF("op=(Filename) inc(src AddRef)", srcStr, rc, caller);
        MaybeLogDataDir("op=(Filename) inc(src)", srcStr, caller, false);
    }
    return g_origAssignFn(dst, src);
}

// Filename(const String&) ctor @ 0x1800067d0 — AddRefs (inc) the source String.
// dst is uninitialized on entry (the ctor zeroes it), so only src is checked.
// This is how GetPath/GetPathForReferenceId build the by-value Filename they
// return — a prime AddRef-of-a-freed-path-String suspect.
void* CheckedCtorFromString(void* dst, const void* src) {
    if (src) {
        void* srcStr = *reinterpret_cast<void* const*>(src);
        int rc = 0;
        if (IsUAF(srcStr, rc))
            ReportUAF("Filename(String&) inc(src AddRef)", srcStr, rc,
                      __builtin_return_address(0));
        MaybeLogDataDir("Filename(String&) inc(src)", srcStr, __builtin_return_address(0), false);
    }
    return g_origCtorStr(dst, src);
}

// Patch a Runtime function entry with `MOV RAX,&wrapper ; JMP RAX ; NOP...`
// (patchLen >= 12, must land on an instruction boundary) and build an
// executable trampoline holding the original patchLen bytes + an absolute JMP
// back to addr+patchLen. Returns the trampoline (the real body) or nullptr.
void* DetourRuntimeFunc(uintptr_t addr, void* wrapper, int patchLen,
                        const uint8_t* expected, int expectedLen,
                        const char* name) {
    if (!addr) {
        fprintf(stderr, "[FNHook] could not resolve %s\n", name);
        return nullptr;
    }
    if (memcmp(reinterpret_cast<void*>(addr), expected, expectedLen) != 0) {
        fprintf(stderr, "[FNHook] %s prologue at %p mismatch — refusing to patch\n",
                name, reinterpret_cast<void*>(addr));
        return nullptr;
    }
    uint8_t* tramp = reinterpret_cast<uint8_t*>(
        VirtualAlloc(nullptr, 64, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
    if (!tramp) {
        fprintf(stderr, "[FNHook] VirtualAlloc failed for %s (err=%lu)\n",
                name, GetLastError());
        return nullptr;
    }
    memcpy(tramp, reinterpret_cast<void*>(addr), patchLen);
    tramp[patchLen + 0] = 0xFF;  // JMP qword ptr [RIP+0]
    tramp[patchLen + 1] = 0x25;
    tramp[patchLen + 2] = 0x00; tramp[patchLen + 3] = 0x00;
    tramp[patchLen + 4] = 0x00; tramp[patchLen + 5] = 0x00;
    uint64_t cont = static_cast<uint64_t>(addr + patchLen);
    memcpy(tramp + patchLen + 6, &cont, 8);
    FlushInstructionCache(GetCurrentProcess(), tramp, patchLen + 14);

    uint8_t patch[24];
    patch[0] = 0x48; patch[1] = 0xB8;  // MOV RAX, imm64
    uint64_t w = reinterpret_cast<uint64_t>(wrapper);
    memcpy(patch + 2, &w, 8);
    patch[10] = 0xFF; patch[11] = 0xE0;  // JMP RAX
    for (int i = 12; i < patchLen; ++i) patch[i] = 0x90;  // NOP pad to boundary

    DWORD oldProtect = 0;
    if (!VirtualProtect(reinterpret_cast<void*>(addr), patchLen,
                        PAGE_EXECUTE_READWRITE, &oldProtect)) {
        fprintf(stderr, "[FNHook] VirtualProtect failed for %s (err=%lu)\n",
                name, GetLastError());
        return nullptr;
    }
    memcpy(reinterpret_cast<void*>(addr), patch, patchLen);
    VirtualProtect(reinterpret_cast<void*>(addr), patchLen, oldProtect, &oldProtect);
    FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<void*>(addr), patchLen);
    fprintf(stderr, "[FNHook] detoured %s @%p (tramp=%p, %d-byte patch)\n",
            name, reinterpret_cast<void*>(addr), tramp, patchLen);
    return tramp;
}

} // namespace

// Shared entry point to the detour helper above, for other translation units
// that need to interpose a Runtime function (see standalone/runtime_detour.h).
// The implementation stays in the anonymous namespace where it grew up; this is
// only a linkage bridge, deliberately not a second copy.
void* RuntimeDetourInstall(uintptr_t addr, void* wrapper, int patchLen,
                           const uint8_t* expected, int expectedLen,
                           const char* name) {
    return DetourRuntimeFunc(addr, wrapper, patchLen, expected, expectedLen, name);
}

extern "C" void InstallFilenameRuntimeHook() {
    const char* e = getenv("BANISHED_FNHOOK");
    if (!e || !e[0] || e[0] == '0' || e[0] == 'f' || e[0] == 'F') return;

    const char* ab = getenv("BANISHED_FNHOOK_ABORT");
    g_abort = (ab && ab[0] && ab[0] != '0');
    if (const char* m = getenv("BANISHED_FNHOOK_MAX")) {
        int v = atoi(m);
        if (v > 0) g_maxEvents = v;
    }
    const char* dd = getenv("BANISHED_FNHOOK_DATADIR");
    g_watchDataDir = (dd && dd[0] && dd[0] != '0');

    // ~Filename @ 0x180006980 — 18-byte boundary (mov[rsp+8],rcx; sub rsp,0x38;
    // mov[rsp+0x20],-2), all position-independent.
    static const uint8_t kDtorPrologue[18] = {
        0x48,0x89,0x4c,0x24,0x08, 0x48,0x83,0xec,0x38,
        0x48,0xc7,0x44,0x24,0x20,0xfe,0xff,0xff,0xff
    };
    g_origDtor = reinterpret_cast<FilenameDtorFn>(DetourRuntimeFunc(
        ResolveAddr(Module::Runtime, 0x180006980),
        reinterpret_cast<void*>(&CheckedDtor), 18,
        kDtorPrologue, sizeof(kDtorPrologue), "Filename::~Filename"));

    // operator= @ 0x180006770 — 13-byte boundary (mov[rsp+8],rbx; push rdi;
    // sub rsp,0x20; mov rax,[rcx]), all position-independent.
    static const uint8_t kAssignPrologue[13] = {
        0x48,0x89,0x5c,0x24,0x08, 0x57, 0x48,0x83,0xec,0x20, 0x48,0x8b,0x01
    };
    g_origAssign = reinterpret_cast<FilenameAssignFn>(DetourRuntimeFunc(
        ResolveAddr(Module::Runtime, 0x180006770),
        reinterpret_cast<void*>(&CheckedAssign), 13,
        kAssignPrologue, sizeof(kAssignPrologue), "Filename::operator="));

    // operator=(const Filename&) @ 0x180007d40 — 20-byte boundary
    // (mov[rsp+8],rbx; mov[rsp+0x10],rbp; mov[rsp+0x18],rsi; push rdi; sub rsp,0x20).
    static const uint8_t kAssignFnPrologue[20] = {
        0x48,0x89,0x5c,0x24,0x08, 0x48,0x89,0x6c,0x24,0x10,
        0x48,0x89,0x74,0x24,0x18, 0x57, 0x48,0x83,0xec,0x20
    };
    g_origAssignFn = reinterpret_cast<FilenameAssignFn>(DetourRuntimeFunc(
        ResolveAddr(Module::Runtime, 0x180007d40),
        reinterpret_cast<void*>(&CheckedAssignFilename), 20,
        kAssignFnPrologue, sizeof(kAssignFnPrologue), "Filename::operator=(Filename)"));

    // Filename(const String&) @ 0x1800067d0 — 19-byte boundary
    // (mov[rsp+8],rcx; push rdi; sub rsp,0x30; mov[rsp+0x20],-2).
    static const uint8_t kCtorStrPrologue[19] = {
        0x48,0x89,0x4c,0x24,0x08, 0x57, 0x48,0x83,0xec,0x30,
        0x48,0xc7,0x44,0x24,0x20,0xfe,0xff,0xff,0xff
    };
    g_origCtorStr = reinterpret_cast<FilenameAssignFn>(DetourRuntimeFunc(
        ResolveAddr(Module::Runtime, 0x1800067d0),
        reinterpret_cast<void*>(&CheckedCtorFromString), 19,
        kCtorStrPrologue, sizeof(kCtorStrPrologue), "Filename(String&)"));

    // MemFreeStatic @ 0x1800c7ad0 — detour to catch the FREE of the data-dir
    // String's block (names the inlined over-release). Prologue:
    //   test rcx,rcx; je epilogue; push rdi; sub rsp,0x30; mov[rsp+0x20],-2
    // The je rel8 can't relocate, so (FreeLocked pattern) the wrapper does the
    // null-check itself and the trampoline reproduces only push/sub/cookie then
    // jumps to MemFree+0x13. Only installed when the data-dir watch is on.
    if (g_watchDataDir) {
        uintptr_t mf = ResolveAddr(Module::Runtime, 0x1800c7ad0);
        static const uint8_t kMfPrologue[19] = {
            0x48,0x85,0xc9, 0x74,0x56, 0x57, 0x48,0x83,0xec,0x30,
            0x48,0xc7,0x44,0x24,0x20,0xfe,0xff,0xff,0xff
        };
        if (mf && memcmp(reinterpret_cast<void*>(mf), kMfPrologue, sizeof(kMfPrologue)) == 0) {
            uint8_t* tramp = reinterpret_cast<uint8_t*>(
                VirtualAlloc(nullptr, 64, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
            if (tramp) {
                memcpy(tramp, reinterpret_cast<void*>(mf + 5), 14);  // push rdi;sub;cookie
                tramp[14] = 0xFF; tramp[15] = 0x25;
                tramp[16] = tramp[17] = tramp[18] = tramp[19] = 0x00;
                uint64_t cont = static_cast<uint64_t>(mf + 0x13);
                memcpy(tramp + 20, &cont, 8);
                FlushInstructionCache(GetCurrentProcess(), tramp, 28);
                g_origMemFreeBody = reinterpret_cast<MemFreeFn>(tramp);

                uint8_t patch[19];
                patch[0] = 0x48; patch[1] = 0xB8;
                uint64_t w = reinterpret_cast<uint64_t>(&MemFreeWrapper);
                memcpy(patch + 2, &w, 8);
                patch[10] = 0xFF; patch[11] = 0xE0;
                for (int i = 12; i < 19; ++i) patch[i] = 0x90;
                DWORD op = 0;
                if (VirtualProtect(reinterpret_cast<void*>(mf), 19, PAGE_EXECUTE_READWRITE, &op)) {
                    memcpy(reinterpret_cast<void*>(mf), patch, 19);
                    VirtualProtect(reinterpret_cast<void*>(mf), 19, op, &op);
                    FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<void*>(mf), 19);
                    fprintf(stderr, "[FNHook] detoured MemFreeStatic @%p (data-dir free watch, tramp=%p)\n",
                            reinterpret_cast<void*>(mf), tramp);
                }
            }
        } else {
            fprintf(stderr, "[FNHook] MemFreeStatic prologue mismatch/null @%p — free watch OFF\n",
                    reinterpret_cast<void*>(mf));
        }
    }

    fprintf(stderr, "[FNHook] installed (abort=%d maxEvents=%d) — watching "
            "Runtime-internal Filename refcount UAF\n", (int)g_abort, g_maxEvents);
    fflush(stderr);
}

// --- Session 5 (2026-06-16): resource-load serialize — THE MENU CRASH FIX ----
// ROOT CAUSE (validated): the entire intermittent menu heap crash is a
// CONCURRENCY RACE in the resource loader between the MAIN thread (menu
// dialog/scene load) and the RUNTIME BACKGROUND LOADER thread — both call
// Object::ExternalFactory::FindOrLoad (0x1800bf320) concurrently (confirmed:
// 2 distinct tids during the menu load). FindOrLoad / Object::External::Load
// (0x1800bc620) are NOT thread-safe: FindOrLoad mutates the factory's
// resource-cache list via Add() and walks the shared Factory::_singleton tree;
// External::Load walks the factory tree + does racy System::String/Filename
// refcount inc/dec while building paths. The race corrupts:
//   - the data-dir String refcount (lost-update erosion -> the bucket-12
//     free-list "+0x100000000 bit-32 flip" link crash, Runtime+0xC74C3); AND
//   - the factory cache (racy Add -> FindOrLoad returns NULL for a real
//     resource -> the texture-null / dialog-null derefs, Runtime+0x103933 /
//     +0x102C9F / +0x1148EB).
// Both faces — chased for 6 sessions as "use-after-free" / "over-release" —
// are the SAME race. A/B (full speed, process-death detector): baseline ~33%
// crash vs serialize ON **0 crashes / 24 (pure default), every run drawing the
// menu**. The data-dir pin (BANISHED_PIN_DATADIR) is NO LONGER NEEDED.
//
// We serialize with ONE global recursive CRITICAL_SECTION taken at BOTH
// FindOrLoad AND External::Load. Recursive => the FindOrLoad -> External::Load
// nesting (and recursive resource deps) on one thread re-enter freely; only
// cross-thread calls serialize. Locking External::Load too covers its other
// callers (SetupFileSystem / AreResourcesEqual / OnVideoRestart) and the
// in-game world-build load path, not just the menu's FindOrLoad. Default ON;
// opt out with BANISHED_NO_FOL_LOCK=1. Safe for the in-game build (just less
// load parallelism; 0 stalls observed).

typedef void* (*FindOrLoadFn)(void* self, void* fname);
typedef void* (*ExternalLoadFn)(void* fname, void* ext);
FindOrLoadFn     g_origFindOrLoad   = nullptr;
ExternalLoadFn   g_origExternalLoad = nullptr;
CRITICAL_SECTION g_resCS;  // shared recursive lock for FindOrLoad + External::Load

// BANISHED_FOL_TRACE=1: log every FindOrLoad/External::Load filename (capped) to
// find non-terminating / looping loads. Default OFF (no behavior change). A
// System::Filename's first member is a COW String = char* buffer, so the path
// text is *(char**)fname.
static int   g_folTrace = -1;
static LONG  g_folTraceN = 0;
static const char* FolFnameText(void* fname) {
    if (!fname || IsBadReadPtr(fname, 8)) return "(badfname)";
    char* p = *(char**)fname;
    if (!p || IsBadReadPtr(p, 1)) return "(null)";
    return p;
}
static inline void FolTrace(const char* tag, void* fname) {
    if (g_folTrace < 0) { const char* e = getenv("BANISHED_FOL_TRACE"); g_folTrace = (e && e[0] && e[0] != '0') ? 1 : 0; }
    if (!g_folTrace) return;
    LONG n = InterlockedIncrement(&g_folTraceN);
    if (n > 20000) return;
    fprintf(stderr, "[FOLTrace#%ld] %s tid=%lu \"%s\"\n", (long)n, tag,
            (unsigned long)GetCurrentThreadId(), FolFnameText(fname));
    fflush(stderr);
}

// Last resource path entering FindOrLoad/External::Load — recorded unconditionally
// (cheap) so Accelerator B's read guard can name the resource that was loading when
// a serialize drift hung CompressedStream::Read. See InstallReadGuard below.
static char g_lastFolName[260] = {0};
static inline void RecordFolName(void* fname) {
    const char* p = FolFnameText(fname);
    if (p) { strncpy(g_lastFolName, p, sizeof(g_lastFolName) - 1); g_lastFolName[sizeof(g_lastFolName) - 1] = 0; }
}

void* FindOrLoadWrapper(void* self, void* fname) {
    EnterCriticalSection(&g_resCS);
    FolTrace("FindOrLoad", fname);
    RecordFolName(fname);
    void* r = g_origFindOrLoad(self, fname);
    LeaveCriticalSection(&g_resCS);
    return r;
}

void* ExternalLoadWrapper(void* fname, void* ext) {
    EnterCriticalSection(&g_resCS);
    FolTrace("ExtLoad", fname);
    RecordFolName(fname);
    void* r = g_origExternalLoad(fname, ext);
    LeaveCriticalSection(&g_resCS);
    return r;
}

extern "C" void InstallFindOrLoadWatch() {
    // The serialize lock is the menu-crash FIX — default ON (opt out with
    // BANISHED_NO_FOL_LOCK=1).
    const char* noLock = getenv("BANISHED_NO_FOL_LOCK");
    if (noLock && noLock[0] && noLock[0] != '0') {
        fprintf(stderr, "[ResLoad] BANISHED_NO_FOL_LOCK set — resource loader NOT serialized\n");
        fflush(stderr);
        return;
    }
    InitializeCriticalSection(&g_resCS);

    // FindOrLoad @ 0x1800bf320 — 14-byte boundary: REX-prefixed push rbp
    // (40 55), push rbx/rsi/rdi (53 56 57), push r14 (41 56), mov rbp,rsp
    // (48 8b ec), sub rsp,0x70 (48 83 ec 70). All position-independent.
    static const uint8_t kFolPrologue[14] = {
        0x40,0x55, 0x53, 0x56, 0x57, 0x41,0x56,
        0x48,0x8b,0xec, 0x48,0x83,0xec,0x70
    };
    g_origFindOrLoad = reinterpret_cast<FindOrLoadFn>(DetourRuntimeFunc(
        ResolveAddr(Module::Runtime, 0x1800bf320),
        reinterpret_cast<void*>(&FindOrLoadWrapper), 14,
        kFolPrologue, sizeof(kFolPrologue), "ExternalFactory::FindOrLoad"));

    // External::Load @ 0x1800bc620 — 18-byte boundary: mov rax,rsp (48 8b c4),
    // push rbp/rdi/r14 (55 57 41 56), lea rbp,[rax-0x28] (48 8d 68 d8),
    // sub rsp,0x110 (48 81 ec 10 01 00 00). All position-independent. The
    // mov rax,rsp is captured into the trampoline verbatim — for a passthrough
    // entered via a normal CALL the captured RSP is self-consistent with the
    // frame the copied prologue builds, so no ADD RAX,8 compensation is needed.
    static const uint8_t kExtLoadPrologue[18] = {
        0x48,0x8b,0xc4, 0x55, 0x57, 0x41,0x56,
        0x48,0x8d,0x68,0xd8, 0x48,0x81,0xec,0x10,0x01,0x00,0x00
    };
    g_origExternalLoad = reinterpret_cast<ExternalLoadFn>(DetourRuntimeFunc(
        ResolveAddr(Module::Runtime, 0x1800bc620),
        reinterpret_cast<void*>(&ExternalLoadWrapper), 18,
        kExtLoadPrologue, sizeof(kExtLoadPrologue), "External::Load"));

    fprintf(stderr, "[ResLoad] serialized: FindOrLoad=%p External::Load=%p (recursive CS)\n",
            (void*)g_origFindOrLoad, (void*)g_origExternalLoad);
    fflush(stderr);
}

// --- Accelerator B (Phase 0, notes/PRELOAD_CASCADES_PLAN.md): ----------------
// CompressedStream::Read zero-progress guard. A serialize-misalignment drift
// makes a String::Serialize read a bogus huge wide-string length; that length
// flows into IO::CompressedStream::Read (Runtime 0x1800aa780) as `count`. Read's
// internal while-loop (decompiled): while(count>=1){ SetWindow(this,pos); n =
// min(count, winEnd-pos); memcpy; pos+=n; count-=n; }. When `count` over-reads
// past EOF, SetWindow keeps returning true but n becomes 0 (winEnd==pos), so the
// loop spins FOREVER copying 0 bytes — the documented loader hang. Because the
// spin is INSIDE one Read call, detouring + calling the original cannot break it;
// we REIMPLEMENT Read faithfully (same field layout/memcpy) and add an `n<=0`
// guard that LOGS the active resource (g_lastFolName) + a stack backtrace
// (names the drifting *_Serialize) and returns false to break the spin. The
// loader then limps forward to reveal the NEXT drift. Gate BANISHED_READ_GUARD=1
// (default OFF → not installed, original behavior). DIAGNOSTIC ONLY.
typedef bool (*SetWindowFn)(void* self, int pos);
static SetWindowFn   g_setWindow   = nullptr;
static volatile LONG g_readGuardN  = 0;

// Field offsets within CompressedStream (from the Read decompile):
//   +0x1c int pos; +0x20 int windowIdx (*0x18 stride); +0x30 ptr winArray.
//   window entry: +0x04 int winStart; +0x08 int winEnd; +0x10 char* winData.
bool ReadGuardWrapper(void* self, int count, void* dst) {
    while (true) {
        if (count < 1) return true;
        if (!g_setWindow || !g_setWindow(self, *(int*)((uint8_t*)self + 0x1c)))
            return false;
        uint8_t* win = *(uint8_t**)((uint8_t*)self + 0x30)
                     + (int64_t)(*(int*)((uint8_t*)self + 0x20)) * 0x18;
        int winEnd = *(int*)(win + 8);
        int pos    = *(int*)((uint8_t*)self + 0x1c);
        int n = count;
        if (winEnd < pos + count) n = winEnd - pos;
        if (n <= 0) {
            // THE DRIFT GUARD — over-read past EOF; the original spins here.
            LONG k = InterlockedIncrement(&g_readGuardN);
            if (k <= 200) {
                fprintf(stderr, "[ReadGuard] drift: zero-progress Read (count=%d "
                        "pos=%d winEnd=%d) loading \"%s\"\n",
                        count, pos, winEnd, g_lastFolName);
                void* frames[24] = {0};
                USHORT got = RtlCaptureStackBackTrace(1, 24, frames, nullptr);
                for (USHORT i = 0; i < got; ++i) {
                    char lbl[12]; snprintf(lbl, sizeof(lbl), "  rg[%2u]", i);
                    LogModOff(lbl, reinterpret_cast<uint64_t>(frames[i]));
                }
                fflush(stderr);
            }
            return false;  // break the spin; loader treats as read failure
        }
        int winStart = *(int*)(win + 4);
        memcpy(dst, *(char**)(win + 0x10) + (int64_t)(pos - winStart), (size_t)n);
        *(int*)((uint8_t*)self + 0x1c) = pos + n;
        count -= n;
        dst = (uint8_t*)dst + n;
    }
}

extern "C" void InstallReadGuard() {
    const char* e = getenv("BANISHED_READ_GUARD");
    if (!e || !e[0] || e[0] == '0' || e[0] == 'f' || e[0] == 'F') return;

    g_setWindow = reinterpret_cast<SetWindowFn>(
        ResolveAddr(Module::Runtime, 0x1800a9aa0));  // IO::CompressedStream::SetWindow

    // Read @0x1800aa780 — 15-byte instruction boundary: mov[rsp+8],rbx;
    // mov[rsp+0x10],rbp; mov[rsp+0x18],rsi. We REPLACE the body (never call the
    // original), so the returned trampoline is unused — only checked for non-null.
    static const uint8_t kReadPrologue[15] = {
        0x48,0x89,0x5c,0x24,0x08, 0x48,0x89,0x6c,0x24,0x10, 0x48,0x89,0x74,0x24,0x18
    };
    void* tramp = DetourRuntimeFunc(
        ResolveAddr(Module::Runtime, 0x1800aa780),
        reinterpret_cast<void*>(&ReadGuardWrapper), 15,
        kReadPrologue, sizeof(kReadPrologue), "CompressedStream::Read");

    fprintf(stderr, "[ReadGuard] installed: Read=%s SetWindow=%p (BANISHED_READ_GUARD)\n",
            tramp ? "patched" : "FAILED", (void*)g_setWindow);
    fflush(stderr);
}

