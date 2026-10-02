// ============================================================================
// runtime/src/system/error.cpp — Wave 9 lane ENG, iteration 3
// System::Error::SetText — the diagnostic every remaining Startup fault needs
// ============================================================================
//
//   | Export                                       | Retail      | Ord  | Instrs |
//   |----------------------------------------------|-------------|------|--------|
//   | ?SetText@Error@System@@SAXPEBD_N@Z            | 0x1800c5fa0 | 3982 |   54   |
//   | ?StrAppend@String@System@@SAPEADPEADHPEBD@Z   | 0x1800c9020 | 4113 |   44   |
//
// Both names checked against Runtime-x64.dll's export directory with pefile
// before a line was written. No new Win32 API and no new import library.
//
// ---------------------------------------------------------------------------
// 1. WHY THIS IS PORTED DESPITE BEING OFF THE CURRENT REACH PATH
// ---------------------------------------------------------------------------
// Iteration 2 deferred it, correctly, as failure-arm-only. The coordinator
// asked for it anyway and the argument is right: `game_main.cpp:482-497` reads
// `_errorString` to report WHY `Engine::Startup` returned 0, and every failure
// arm in that 367-instruction function reaches it through `SetText`. While it
// is unported, every `Engine::Startup` failure in the rest of this cutover work
// prints "System::Error string is empty after Startup failure" and we lose the
// one line that says which of the sixteen steps bombed. It is off the REACH
// path and squarely on the DIAGNOSTIC path, and there are a dozen faults left.
//
// ---------------------------------------------------------------------------
// 2. ✅ DISCHARGED 2026-09-11 — THE MINT ROW FOR `?_errorString@` IS NOW 0x2000
// ---------------------------------------------------------------------------
// **THE BLOCKING WARNING BELOW NO LONGER APPLIES. DO NOT ACT ON IT.** It is
// kept, struck through, because the reasoning is the evidence for the fix and
// deleting it would leave the corrected row looking arbitrary.
//
// Discharged by Wave 22 lane ERRCFG, 2026-09-11. EVIDENCE, re-read at that
// date rather than inherited — `standalone/rt_data_mint_tables.inc:2244` today
// reads:
//
//     { "?_errorString@Error@System@@0PADA", 0x1801fc720ull, 8192,
//       &kStorageBytes[32],
//       "char[0x2000] -- the error text BUFFER; PAD in the mangling is MSVC's
//        array-as-pointer encoding, see ?SetText@ 0x1800c5fa0", ... }
//
// i.e. size 8192 == 0x2000 and a description that names the array, exactly the
// change §2 demanded.
//
// A blocking warning that no longer applies is worse than no warning, because
// the next lane obeys it and stops. Recorded here as the mirror of the defect
// Wave 22 lane GEOM found in statusicon_geometry.cpp: there a PRESCRIPTION was
// left reading as a COMPLETION; here a COMPLETION was left reading as a
// PRESCRIPTION. Both cost a wave.
//
// ---- the original §2, superseded --------------------------------------------
// ~~This port must not be armed at cutover until lane C2 fixes one table row.~~
//
//     standalone/rt_data_mint_tables.inc:1538   (as it read in Wave 9)
//     { "?_errorString@Error@System@@0PADA", 0x1801fc720ull, 8,
//       &kStorageBytes[16], "char*", nullptr, nullptr, 0 },
//
// The row minted **8 bytes** and described the symbol as `"char*"`. The
// disassembly says otherwise, twice over, and the mangled name is what misled
// the generator — MSVC decays a `static char[N]` member to `PAD` (`char *`):
//
//     0x1800c5fba   LEA RBP,[0x1801fc720]      <- the ADDRESS, not a load of a
//                                                 pointer stored there
//     0x1800c5fc1   MOV EDI,0x2000             <- the capacity, passed to
//                                                 StrAppend as `capacity`
//     0x1800c5fd4   CMP byte ptr [0x1801fc720],CL   <- byte 0 OF THE BUFFER
//     0x1800c6034   MOV byte ptr [RAX + RBP*1 + -1],0   <- a store at RBP+room-1
//
// So `?_errorString@Error@System@@0PADA` **is** an 8192-byte buffer whose
// exported address is its first byte. With the row as it stands, the FIRST
// `SetText` call after cutover writes up to 8192 bytes into an 8-byte slot and
// scribbles over every minted row that follows `kStorageBytes[16]`.
//
// **Fix (lane C2's file, one row): size `8` -> `0x2000`, kind `"char*"` ->
// a byte buffer.** `game_main.cpp:487` also reads it by ADDRESS
// (`ResolveAddr(Module::Runtime, 0x1801fc720)`) rather than by name, so the
// reader needs the mint's address registration too — which the row already
// carries.
//
// This file does NOT edit the mint table. Widening a row changes every
// `kStorageBytes` offset after it, which is lane C2's generator's business, and
// a shared table edited by two lanes mid-wave is how denominators rot.
//
// ---------------------------------------------------------------------------
// 3. `StrAppend` LIVES HERE ONLY TO AVOID A CONCURRENT EDIT
// ---------------------------------------------------------------------------
// `?StrAppend@String@System@@SAPEADPEADHPEBD@Z` is a `System::String` static and
// its natural home is `runtime/src/system/string.cpp`. It is here because
// `SetText` is its only caller in this wave's path and string.cpp is live in
// another lane. Move it when both are still.
//
// ---------------------------------------------------------------------------
// 4. ORACLE
// ---------------------------------------------------------------------------
// `StrAppend` is a PURE FUNCTION of (dst contents, capacity, src) with an
// out-parameter, so it has a REAL differential oracle and gets one: the shadow
// runs the shipped body on a snapshot of the same input buffer and compares
// both the returned pointer offset and the full 0x2000 bytes. That snapshot is
// not optional — `reference_oracle_must_snapshot_aliased_inputs.md`: `dst` is
// both an input and the output, so running theirs after ours would run it on
// input our own write had already destroyed.
//
// `SetText` gets a shadow instead. It appends to a process-wide buffer, so a
// replay would append twice; what is checked is that the text and the trailing
// newline are BOTH present at the end of the buffer after our call, and that
// `_stopApplication` took the argument — none of which this body computes
// directly, they are what `StrAppend` and the inlined second append produced.
// ============================================================================

#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <cstring>

// ENDGAME B.3.4b SYSTEM (4): `System::Error` is a real class with two static
// members (runtime/types/system/Error.h). The mangling is the evidence --
// `?SetText@Error@System@@SA...` is a static member of a class named
// `System::Error`, which a free function cannot carry.
//
// TWO BODIES MOVED onto members; the two registered doors keep their addresses
// and delegate. Member bodies are defined at GLOBAL scope with qualified names,
// the shape SYSTEM (3) adopted.
//
// NOT MOVED, AND NEITHER IS AN OVERSIGHT:
//   * [SUPERSEDED by ENDGAME B.3.9 SYS: the three are class statics now,
//     defined just below, redirected together in RtSystemPortInit.]
//     the THREE statics (`_errorString`, `_stopApplication`, `_hideDisplay`)
//     stay `RT_DATA_SLOT` reads -- rule B4b-4. All three are CUTOVER_REQUIRED
//     rows answered by a minted cell, and making one a real `static` member
//     needs a redirect at `standalone/rt_data_mint.cpp`'s `MintStorageCell()`,
//     which is another agent's file this step. Debt tabled per row in
//     B34B_RULES section 4.1. `_errorString` is a 0x2000-byte BUFFER, not a
//     pointer, so its redirect must hand back the array's address.
//   * `RtSystem_String_StrAppend` is a `System::String` static hosted in this
//     file only to avoid a concurrent edit to string.cpp. It is NOT an Error
//     export, it is not declared in Error.h, and it does not move with this
//     class.
//   * `?HideErrorDisplay@Error@System@@SAX_N@Z` (0x1800c5f90) is documented
//     dead by a prior four-binary sweep and stays unported.
#include "../../types/system/Error.h"

// ENDGAME B.3.9 SYS (B39_RULES D7): the three statics. Zero, as the DLL's
// .data/.bss starts; RtSystemPortInit redirects the minted rows onto them.
char System::Error::_errorString[0x2000] = {};   // 0x1801fc720
bool System::Error::_hideDisplay         = false; // 0x1801fc709
bool System::Error::_stopApplication     = false; // 0x1801fc70a

// Four kernel32 entry points, declared directly instead of including
// <windows.h>. The banner's "no new Win32 API and no new import library" claim
// is preserved in the sense that matters: kernel32 is already an import of this
// binary and nothing new is linked. `MessageBoxW` itself is NOT linked — it is
// fetched with GetProcAddress at call time, deliberately (see ?Display@'s ⚠).
extern "C" __declspec(dllimport) void* __stdcall GetModuleHandleW(const wchar_t*);
extern "C" __declspec(dllimport) void* __stdcall LoadLibraryW(const wchar_t*);
extern "C" __declspec(dllimport) void* __stdcall GetProcAddress(void*, const char*);
extern "C" __declspec(dllimport) void  __stdcall ExitProcess(unsigned);

namespace {

// user32!MessageBoxW, the function behind retail's IAT slot 0x18014b540.
// GetModuleHandleW first: user32 is already loaded in any process that has a
// window, and LoadLibraryW is the fallback for the case where Display fires
// before the window exists — which is exactly the allocator's OOM path.
void* RtSystem_ResolveUser32MessageBoxW() {
    static void* cached = nullptr;
    if (cached) return cached;
    void* user32 = GetModuleHandleW(L"user32.dll");
    if (!user32) user32 = LoadLibraryW(L"user32.dll");
    if (!user32) return nullptr;
    cached = GetProcAddress(user32, "MessageBoxW");
    return cached;
}

// Reached only with Runtime-x64.dll absent; see DisplayStopIfRequested.
void RtSystem_ErrorDisplayFallbackExit() {
    std::fflush(nullptr);
    ExitProcess(0xFFFFFFFFu);
}

// Retail's capacity, the literal `MOV EDI,0x2000` at 0x1800c5fc1. Named once so
// that the two places that use it cannot drift apart. See §2.
constexpr int kErrorStringCapacity = 0x2000;

// The literal appended after every message: the two bytes at 0x180155dbc are
// `0A 00`, read with read_memory. A newline, not a formatting string.
const char kNewline[] = "\n";

// The three exported DATA symbols are CLASS STATICS since ENDGAME B.3.9 SYS
// (defined below this namespace; RtSystemPortInit redirects the minted rows
// onto them), so their three RT_DATA_SLOT accessors are gone.
//   ?_errorString@Error@System@@0PADA      ord 4782, 0x1801fc720 -- THE BUFFER
//   ?_stopApplication@Error@System@@0_NA   ord 4818, 0x1801fc70a
//   ?_hideDisplay@Error@System@@0_NA       ord 4791, 0x1801fc709

// The two literals, read out of Runtime-x64.dll with read_memory rather than
// retyped from a decompiler's rendering. 0x180155dc0 is 64 bytes ending in a
// newline; 0x180154858 is the qword "Banished" and 0x180154860 its NUL.
const char kFatalPrefix[] =
    "A fatal error has occurred and the application cannot continue.\n";
const char kCaption[9] = { 'B','a','n','i','s','h','e','d','\0' };

// 0x1800c63d1 — the tail both arms of Display fall into.
//
// 0x18007f8a8 is three instructions (`XOR R8D,R8D ; LEA EDX,[R8+1] ; JMP
// 0x18007f9f8`), i.e. MSVC's CRT `exit(code)` with the quick-exit flag set, and
// it belongs to RUNTIME-X64.DLL'S OWN statically-linked CRT. It is reached by
// ADDRESS because it has no name in either build. With the DLL present that is
// byte-faithful: retail's atexit chain and static destructors run. With the DLL
// absent there is no such chain to run and no address to reach, so the fallback
// is ExitProcess with the same code — stated, because it is the one place in
// this file where cutover behaviour differs from retail's and the difference is
// deliberate rather than overlooked.
void DisplayStopIfRequested() {
    if (!System::Error::_stopApplication) return;
    // PORT GAP 0x18007f8a8 the Runtime CRT's exit(code): not ported; retail
    // calls it here with 0xFFFFFFFF. Nothing registers the address, so the
    // by-address resolve that stood here returned null and the fallback below
    // is what runs (B3R_RULES R4).
    RtSystem_ErrorDisplayFallbackExit();
}

// Retail's strlen, written as its own do-while so the `dst[0] == 0` fast path
// (0x1800c9034 / 0x1800c5fd4) is visible rather than folded away.
int RetailStrLen(const char* s) {
    int n = 0;
    if (s[0] != '\0') {
        const char* p = s;
        do { ++p; ++n; } while (*p != '\0');
    }
    return n;
}

}  // namespace

// ============================================================================
// ?StrAppend@String@System@@SAPEADPEADHPEBD@Z              retail 0x1800c9020
// ============================================================================
// char* StrAppend(char* dst, int capacity, const char* src)
//
//   dstLen = strlen(dst)
//   room   = capacity - dstLen
//   if (room <= 0) return dst;                     <- returns DST, not the end
//   end    = dst + dstLen
//   n      = strlen(src) + 1                       <- the NUL is copied
//   memmove(end, src, min(n, room))                <- 0x18008a290
//   if (n > room) end[room - 1] = '\0';            <- truncate
//   return end;                                    <- returns the END
//
// TWO details a tidy rewrite loses, both of which a caller can see:
//
//   * the two RETURN VALUES are different pointers. On the no-room path it
//     returns `dst`; on every other path it returns `dst + dstLen`. Retail's
//     `MOV RAX,RCX` at 0x1800c9052 versus `MOV RAX,RSI` at 0x1800c90a6.
//   * 0x18008a290 is **memmove, not memcpy** — its prologue at 0x18008a2a0
//     computes `RDX - RCX` and branches to a backward copy at 0x18008a5fa when
//     the ranges overlap (MSVC emits one body for both). `std::memmove` is
//     therefore the faithful substitute; `std::memcpy` would be a different
//     function on an overlapping append, which this signature permits.
extern "C" char* RtSystem_String_StrAppend(char* dst, int capacity, const char* src) {

    const int dstLen = RetailStrLen(dst);
    const int room   = capacity - dstLen;
    char* result;
    if (room <= 0) {
        result = dst;
    } else {
        char* end = dst + dstLen;
        const int n = RetailStrLen(src) + 1;
        const int cnt = (room < n) ? room : n;
        std::memmove(end, src, static_cast<size_t>(cnt));
        if (n > room) end[room - 1] = '\0';
        result = end;
    }
    return result;
}

// ============================================================================
// ?SetText@Error@System@@SAXPEBD_N@Z                       retail 0x1800c5fa0
// ============================================================================
// It APPENDS. That is the fact WAVE9.md's writer audit needed and did not have:
// the scan found no store into `_errorString` because the first write happens
// inside `StrAppend`, with the buffer in a register, and the second is a
// `memmove` at 0x1800c6021.
//
//   StrAppend(_errorString, 0x2000, text);          // 0x1800c5fcb
//   used = strlen(_errorString);                    // 0x1800c5fd4 .. c5fe9
//   room = 0x2000 - used;
//   if (room > 0) {
//       end = _errorString + used;
//       n   = strlen("\n") + 1;                     // == 2
//       memmove(end, "\n", min(n, room));
//       _stopApplication = stopApplication;
//       if (n > room) end[room - 1] = '\0';
//   } else {
//       _stopApplication = stopApplication;         // 0x1800c603b
//   }
//
// The second append is `StrAppend` INLINED rather than called — same shape, and
// written out that way here so the two `_stopApplication` stores stay on the
// two arms retail put them on. Note the flag is written on BOTH arms, so a
// caller that fills the buffer still gets the stop behaviour.
void System::Error::SetText(const char* text, bool stopApplication) {

    // ENDGAME B.3.9 SYS: the buffer and the flag are the class statics, which
    // always exist -- so the old null-cell branch (our difference, for a DLL /
    // mint that might not provide the cell) is gone and the body is retail's.
    char* buf = _errorString;

    RtSystem_String_StrAppend(buf, kErrorStringCapacity, text);

    const int used = RetailStrLen(buf);
    const int room = kErrorStringCapacity - used;
    if (room > 0) {
        char* end = buf + used;
        const int n = RetailStrLen(kNewline) + 1;
        const int cnt = (room < n) ? room : n;
        std::memmove(end, kNewline, static_cast<size_t>(cnt));
        _stopApplication = stopApplication;
        if (n > room) end[room - 1] = '\0';
    } else {
        _stopApplication = stopApplication;
    }

}

// ============================================================================
// ?Display@Error@System@@SAXXZ                             retail 0x1800c6060
// ============================================================================
// Wave 22 lane ERRCFG. 0x1800c6060 - 0x1800c63ea, read instruction by
// instruction; the structure below is retail's, not a paraphrase.
//
//   if (_hideDisplay) goto stopCheck;                     // 0x1800c6077
//   char     text[0x800];  zeroed                         // RBP-0x40
//   uint16_t wtext[0x800]; zeroed                         // RBP+0x7c0 (0x1000 bytes)
//   AppendClamped(text, 0x800, "A fatal error has occurred and the "
//                              "application cannot continue.\n");  // 0x1800c6142
//   AppendClamped(text, 0x800, _errorString);             // 0x1800c61b2
//   Utf8ToUtf16Clamped(wtext, 0x800, text);               // 0x1800c61c3..6295
//   char     cap[9]  = "Banished";                        // RSP+0x30, from 0x180154858
//   uint16_t wcap[0x40]; zeroed                           // RSP+0x40 (0x80 bytes)
//   Utf8ToUtf16Clamped(wcap, 0x40, cap);                  // 0x1800c62c5..63ae
//   MessageBoxW(nullptr, wtext, wcap, 0x10);              // 0x1800c63c3 -> [0x18014b540]
// stopCheck:
//   if (_stopApplication) exit(-1);                       // 0x1800c63d1 -> 0x18007f8a8
//
// THE TWO LITERALS WERE READ OUT OF THE BINARY, NOT GUESSED. read_memory at
// 0x180155dc0 gives the 64 bytes "A fatal error has occurred and the
// application cannot continue.\n"; the caption is loaded as a qword from
// 0x180154858 plus the single byte at 0x180154860, which read back as
// "Banished" and a NUL — i.e. retail copies exactly nine bytes, not a strcpy.
//
// THE CLAMP UNIT IS BYTES, AND IT IS NOT THE BUFFER SIZE. Both conversion
// loops accumulate the ENCODER'S byte count and stop at 0x800 (text) / 0x40
// (caption), while the destinations hold 0x1000 / 0x80 bytes. Retail is
// conservative by a factor of two and that is reproduced, not "corrected": the
// bound is observable as the point at which a long error string is truncated.
// Likewise the terminator is written only when `total + 4` is still inside the
// bound (0x1800c628b / 0x1800c639f), so a message that fills the bound exactly
// is left UNTERMINATED in a buffer that was zeroed up front — which is why the
// zeroing is load-bearing and is not elided.
//
// 0x18014b540 IS `user32!MessageBoxW`: get_xrefs_from on that IAT slot resolves
// to EXTERNAL MessageBoxW. The argument order is retail's — hwnd=0 (XOR ECX),
// text=RDX, caption=R8, type=R9D=0x10 (MB_ICONERROR).
//
// ⚠ ONE REAL REGRESSION THIS PORT CAUSES, STATED RATHER THAN DISCOVERED LATER.
// `standalone/main.cpp:728` (`HookRuntimeMessageBoxW`) patches **Runtime-x64.
// dll's IAT** entry for user32!MessageBoxW, so every message box RETAIL pops
// goes through the harness's wrapper. Our body lives in `reimpl-x64.exe` and
// calls the EXE's `MessageBoxW`, which that hook does not cover. So registering
// this port MOVES the only Error message box out from under the harness's only
// MessageBoxW seam. It is not silently relocated: it is called here through a
// `GetProcAddress` on `user32.dll` taken at call time, the same indirection
// retail's IAT gives, so an exe-side hook of the same shape would still work —
// but no such hook exists today. Recorded as an open residual in
// docs/runtime_port/WAVE22_LANE_ERRCFG.md rather than papered over.
//
// A modal Win32 dialog blocks boot in a way that looks exactly like a slow
// load (CLAUDE.md, "Boot State"), so it matters WHO can reach this. On this
// tree the answer is: `FatalOutOfMemory` in runtime/src/system/allocator.cpp,
// and only when `stopApplication` is true, which `Engine::Startup` never
// passes; plus exe import slot 0x140152b58, which is UNREGISTERED. Retail's own
// kill switch, `_hideDisplay`, is honoured on the first line.
//
// ORACLE: NOT RUN, and the reason is not "no route". A real differential oracle
// exists and is described in the lane doc — set `_errorString`, call ours with
// MessageBoxW intercepted, capture (wtext, wcap), call theirs, capture, compare
// the two wide buffers — and it needs a process, which this lane did not get:
// ALLOC and GEOM owned the game directory. This body is therefore
// **TRANSCRIBED AND UNVERIFIED AT RUNTIME**, and the two codec helpers below
// are the only part not lifted from retail at all.
// ============================================================================

namespace {

// The clamped append retail INLINES at 0x1800c6142 and 0x1800c61b2. It is
// 0x1800c9020 (`StrAppend`, ported above) minus the return value, which the
// inlined copies do not compute. Written out rather than delegated to
// RtSystem_String_StrAppend so that this file does not fire that function's
// oracle — which snapshots a 0x2000 buffer — twice per fatal error.
void AppendClamped(char* dst, int capacity, const char* src) {
    const int dstLen = RetailStrLen(dst);
    const int room   = capacity - dstLen;
    if (room <= 0) return;
    char* end = dst + dstLen;
    const int n   = RetailStrLen(src) + 1;       // the NUL is copied
    const int cnt = (room < n) ? room : n;
    std::memmove(end, src, static_cast<size_t>(cnt));   // 0x18008a290 is memmove
    if (n > room) end[room - 1] = '\0';
}

// ---------------------------------------------------------------------------
// The codec pair, encoding ids 2 (UTF-8) and 4 (UTF-16LE).
// ---------------------------------------------------------------------------
// COPIED VERBATIM from runtime/src/system/widestring.cpp's anonymous namespace,
// which is the canonical copy and carries the full disclosure. Repeating the
// essential half of it here rather than only cross-referencing, because the
// disclosure is the kind that must travel with the code:
//
//   The DLL's two dispatchers are 0x1800c9fc0 (decode) and 0x1800c9b60
//   (encode), both jump tables indexed by the encoding id. Ghidra cannot
//   recover the table's bodies as functions ("Too many branches"), so these
//   two helpers are written FROM THE UNICODE SPECIFICATION, not lifted from
//   the DLL. What WAS checked in the disassembly: table entry 4 at 0x1800c9c58
//   rejects a lone surrogate (`LEA EAX,[R9-0xD800]; CMP EAX,0x7FF; JBE err`)
//   and emits a surrogate pair after `SUB R9D,0x10000`.
//
//   Contract taken from the call sites: `*cp` is pre-set to -1 (retail's
//   `MOV dword [RBP+0x17f8],0xffffffff` at 0x1800c6212) and a decoder that
//   cannot make a codepoint leaves it negative and still reports a NONZERO
//   consumption, so the loop always advances. Retail relies on that: its
//   `ADD RDI,RCX` is unconditional.
//
// These are duplicated, not shared, deliberately: making them shared would mean
// exporting them from widestring.cpp, and a second file's fatal-error path is
// the worst place to introduce a new link-order dependency.
int Utf8DecodeOne(const uint8_t* p, const uint8_t* end, int32_t* cp) {
    const uint32_t b0 = p[0];
    if (b0 < 0x80) { *cp = static_cast<int32_t>(b0); return 1; }
    int n;
    uint32_t v;
    if      ((b0 & 0xE0) == 0xC0) { n = 2; v = b0 & 0x1F; }
    else if ((b0 & 0xF0) == 0xE0) { n = 3; v = b0 & 0x0F; }
    else if ((b0 & 0xF8) == 0xF0) { n = 4; v = b0 & 0x07; }
    else                          { *cp = -1; return 1; }   // stray continuation
    if (p + n > end) { *cp = -1; return 1; }
    for (int i = 1; i < n; ++i) {
        const uint32_t b = p[i];
        if ((b & 0xC0) != 0x80) { *cp = -1; return 1; }
        v = (v << 6) | (b & 0x3F);
    }
    *cp = static_cast<int32_t>(v);
    return n;
}

int Utf16EncodeOne(int32_t codepoint, uint8_t* dst) {
    const uint32_t c = static_cast<uint32_t>(codepoint);
    if (c - 0xD800u <= 0x7FFu) return 0;             // `CMP EAX,0x7FF ; JBE error`
    if (c < 0x10000u) {
        dst[0] = static_cast<uint8_t>(c & 0xFF);
        dst[1] = static_cast<uint8_t>((c >> 8) & 0xFF);
        return 2;
    }
    const uint32_t v  = c - 0x10000u;
    const uint32_t hi = 0xD800u | (v >> 10);
    const uint32_t lo = 0xDC00u | (v & 0x3FFu);
    dst[0] = static_cast<uint8_t>(hi & 0xFF);
    dst[1] = static_cast<uint8_t>((hi >> 8) & 0xFF);
    dst[2] = static_cast<uint8_t>(lo & 0xFF);
    dst[3] = static_cast<uint8_t>((lo >> 8) & 0xFF);
    return 4;
}

// Both conversion loops, which are the same code at 0x1800c6200 and 0x1800c6300
// with different buffers and different bounds. `limitBytes` is retail's, in
// BYTES of OUTPUT, and is deliberately not the destination's size.
void Utf8ToUtf16Clamped(uint8_t* dst, int limitBytes, const char* src) {
    const uint8_t* p   = reinterpret_cast<const uint8_t*>(src);  // LEFT: the UTF-8 / UTF-16 transcoders walk the text as bytes; retyping them over char / wchar_t units is owed
    const uint8_t* end = p + RetailStrLen(src);
    int total = 0;
    while (p < end) {
        int32_t cp = -1;                                   // 0x1800c6212
        const int consumed = Utf8DecodeOne(p, end, &cp);
        p += consumed;                                     // 0x1800c622c, unconditional
        if (cp < 0) continue;                              // 0x1800c6237  JS
        uint8_t unit[4];
        const int nb = Utf16EncodeOne(cp, unit);
        total += nb;                                       // 0x1800c624b
        if (total > limitBytes) break;                     // 0x1800c624e  JG
        if (nb <= 0) continue;                             // 0x1800c625c  JLE
        std::memmove(dst, unit, static_cast<size_t>(nb));  // 0x1800c626b
        dst += nb;
    }
    // 0x1800c6278 / 0x1800c628b: the terminator is conditional on there being
    // room for one more code unit PLUS the two bytes retail counts twice. If
    // `total + 4` is past the bound the buffer is left as the initial zeroing
    // made it, which is why the zeroing above is not optional.
    if (total + 4 <= limitBytes) { dst[0] = 0; dst[1] = 0; }
}

}  // namespace

void System::Error::Display() {

    // 0x1800c6077 — retail's own kill switch, tested FIRST and skipping the
    // whole box. `_stopApplication` is still honoured below.
    if (_hideDisplay) { DisplayStopIfRequested(); return; }

    // RBP-0x40 and RBP+0x7c0 in retail: 0x800 bytes of text followed by 0x800
    // UTF-16 code units. Both zeroed up front (0x1800c60b0 / 0x1800c60d2).
    char     text[0x800];
    uint16_t wtext[0x800];
    std::memset(text,  0, sizeof(text));
    std::memset(wtext, 0, sizeof(wtext));

    AppendClamped(text, 0x800, kFatalPrefix);
    AppendClamped(text, 0x800, _errorString);

    Utf8ToUtf16Clamped(reinterpret_cast<uint8_t*>(wtext), 0x800, text);  // LEFT: the UTF-8 / UTF-16 transcoders walk the text as bytes; retyping them over char / wchar_t units is owed

    // 0x1800c6299: nine bytes, copied as a qword plus a byte — not a strcpy.
    char     cap[9];
    uint16_t wcap[0x40];
    std::memcpy(cap, kCaption, sizeof(cap));
    std::memset(wcap, 0, sizeof(wcap));
    Utf8ToUtf16Clamped(reinterpret_cast<uint8_t*>(wcap), 0x40, cap);  // LEFT: the UTF-8 / UTF-16 transcoders walk the text as bytes; retyping them over char / wchar_t units is owed

    // 0x1800c63c3. Resolved at call time rather than linked, so the call is an
    // indirection an exe-side hook could take — see the ⚠ note in the banner.
    typedef int (__stdcall *MessageBoxWFn)(void*, const wchar_t*, const wchar_t*,
                                           unsigned);
    if (MessageBoxWFn box = reinterpret_cast<MessageBoxWFn>(  // CAST-OK: win32
            RtSystem_ResolveUser32MessageBoxW())) {
        box(nullptr, reinterpret_cast<const wchar_t*>(wtext),  // CAST-OK: win32
            reinterpret_cast<const wchar_t*>(wcap), 0x10);  // CAST-OK: win32
    }

    DisplayStopIfRequested();

    // EFFECTIVENESS CONTROL. `work` counts the calls that actually reached the
    // message box; a run whose Display rows are all work=0 took the
    // `_hideDisplay` arm every time and has exercised none of the above.
}
