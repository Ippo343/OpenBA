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
// 2. ⛔ THE MINT ROW FOR `?_errorString@` IS 8 BYTES AND THE BUFFER IS 0x2000
// ---------------------------------------------------------------------------
// **This port must not be armed at cutover until lane C2 fixes one table row.**
//
//     standalone/rt_data_mint_tables.inc:1538
//     { "?_errorString@Error@System@@0PADA", 0x1801fc720ull, 8,
//       &kStorageBytes[16], "char*", nullptr, nullptr, 0 },
//
// The row mints **8 bytes** and describes the symbol as `"char*"`. The
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
// Until that lands, hold this file with **`BANISHED_RT_ERROR=0`**. That switch
// exists for exactly this and nothing else. DLL-present is unaffected: the slot
// resolves to the DLL's own 0x2000-byte buffer and the port is byte-faithful.
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

#include "../../funcs/rt_port.h"

namespace {

// Retail's capacity, the literal `MOV EDI,0x2000` at 0x1800c5fc1. Named once so
// that the two places that use it cannot drift apart. See §2.
constexpr int kErrorStringCapacity = 0x2000;

// The literal appended after every message: the two bytes at 0x180155dbc are
// `0A 00`, read with read_memory. A newline, not a formatting string.
const char kNewline[] = "\n";

// The two exported DATA symbols, through the PORT-AWARE door.
//   ?_errorString@Error@System@@0PADA      ord 4782, 0x1801fc720 -- THE BUFFER
//   ?_stopApplication@Error@System@@0_NA   ord 4818, 0x1801fc70a
char* ErrorStringBuffer() {
    return RT_DATA_SLOT(char*, "?_errorString@Error@System@@0PADA");
}
bool* StopApplicationSlot() {
    return RT_DATA_SLOT(bool*, "?_stopApplication@Error@System@@0_NA");
}

typedef char* (*StrAppendFn)(char*, int, const char*);

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
    RT_DIFF_BEGIN("System::String::StrAppend");

    // Snapshot BEFORE our write: `dst` is both input and output.
    // reference_oracle_must_snapshot_aliased_inputs.md.
    char  theirsBuf[kErrorStringCapacity];
    char* theirsRet = nullptr;
    bool  canDiff = rt_diff_on__ && capacity > 0 && capacity <= kErrorStringCapacity;
    if (canDiff) std::memcpy(theirsBuf, dst, static_cast<size_t>(capacity));

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

    if (canDiff) {
        StrAppendFn theirs = RtOriginal<StrAppendFn>(
            "?StrAppend@String@System@@SAPEADPEADHPEBD@Z");
        if (theirs) {
            theirsRet = theirs(theirsBuf, capacity, src);
            const bool offsetOk = (result - dst) == (theirsRet - theirsBuf);
            const bool bytesOk  = std::memcmp(dst, theirsBuf,
                                              static_cast<size_t>(capacity)) == 0;
            char a[96], b[128];
            std::snprintf(a, sizeof(a), "ret=+%lld", static_cast<long long>(result - dst));
            std::snprintf(b, sizeof(b), "ret=+%lld [offsetOk=%d bytesOk=%d]",
                          static_cast<long long>(theirsRet - theirsBuf),
                          static_cast<int>(offsetOk), static_cast<int>(bytesOk));
            RtDiffReport(&rt_diff_site__, offsetOk && bytesOk, a, b);
            RT_DIFF_WORK(true);
        }
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
extern "C" void RtSystem_Error_SetText(const char* text, bool stopApplication) {
    RT_DIFF_BEGIN("System::Error::SetText");

    char* buf = ErrorStringBuffer();
    bool* stop = StopApplicationSlot();

    // Our difference from retail, not a guard over its logic: retail's buffer is
    // a fixed address in its own .data and always exists. With no DLL it exists
    // only if lane C2's mint provides it -- and see §2, today that row is the
    // wrong SIZE, which is why this whole file has its own kill switch.
    if (buf == nullptr) {
        if (stop) *stop = stopApplication;
        return;
    }

    RtSystem_String_StrAppend(buf, kErrorStringCapacity, text);

    const int used = RetailStrLen(buf);
    const int room = kErrorStringCapacity - used;
    if (room > 0) {
        char* end = buf + used;
        const int n = RetailStrLen(kNewline) + 1;
        const int cnt = (room < n) ? room : n;
        std::memmove(end, kNewline, static_cast<size_t>(cnt));
        if (stop) *stop = stopApplication;
        if (n > room) end[room - 1] = '\0';
    } else {
        if (stop) *stop = stopApplication;
    }

    if (rt_diff_on__) {
        // Read back what StrAppend and the inlined second append PRODUCED --
        // this body computed none of it directly.
        const int textLen = RetailStrLen(text);
        const int now     = RetailStrLen(buf);
        const bool textOk = room <= 0 ||
                            (now >= textLen + 1 &&
                             std::memcmp(buf + now - textLen - 1, text,
                                         static_cast<size_t>(textLen)) == 0);
        const bool nlOk   = room <= 0 || (now >= 1 && buf[now - 1] == '\n');
        const bool stopOk = (stop == nullptr) || (*stop == stopApplication);
        char a[128], b[160];
        std::snprintf(a, sizeof(a), "len=%d tail=%c stop=%d", now,
                      now ? buf[now - 1] : '?', stop ? static_cast<int>(*stop) : -1);
        std::snprintf(b, sizeof(b),
                      "text then newline at the end, stop==arg "
                      "[textOk=%d nlOk=%d stopOk=%d]",
                      static_cast<int>(textOk), static_cast<int>(nlOk),
                      static_cast<int>(stopOk));
        RT_DIFF_SHADOW(textOk && nlOk && stopOk, a, b);
        RT_DIFF_WORK(true);
    }
}

// ============================================================================
// Registration
// ============================================================================
int RtSystemErrorRegister() {
    int n = 0;

    RegisterRuntimeExport("?SetText@Error@System@@SAXPEBD_N@Z", (void*)&RtSystem_Error_SetText); ++n;
    RegisterRuntimeAddr(0x1800c5fa0, (void*)&RtSystem_Error_SetText);

    RegisterRuntimeExport("?StrAppend@String@System@@SAPEADPEADHPEBD@Z", (void*)&RtSystem_String_StrAppend); ++n;
    RegisterRuntimeAddr(0x1800c9020, (void*)&RtSystem_String_StrAppend);

    // DELIBERATELY ABSENT:
    //   ?Display@Error@System@@SAXXZ  -- the message box. Not on any path this
    //   wave reaches: Engine::Startup never calls it (only SetText), and
    //   allocator.cpp's FatalOutOfMemory calls it solely when
    //   stopApplication is true, which Engine::Startup never passes.
    return n;
}
