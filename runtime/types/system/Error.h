#pragma once
// ============================================================================
// runtime/types/system/Error.h — the REAL `System::Error`
// ============================================================================
//
// ENDGAME B.3.4b SYSTEM (4). Design: B34B_RULES.md §1.1, rule B4b-4.
//
// *** IT IS A CLASS WITH STATIC MEMBERS. *** Same test as `Timer.h`'s:
//
//   `SA` = PUBLIC STATIC member function
//       ?SetText@Error@System@@SAXPEBD_N@Z   0x1800c5fa0   — PORTED, R + A
//       ?Display@Error@System@@SAXXZ         0x1800c6060   — PORTED, R + A
//       ?HideErrorDisplay@Error@System@@SAX_N@Z 0x1800c5f90 — UNPORTED, and
//           documented DEAD by a prior four-binary sweep (`error.cpp`'s own
//           comment). Not declared: an uncalled port is an unverified port.
//   `0` = PRIVATE STATIC DATA member
//       ?_errorString@Error@System@@0PADA      0x1801fc720  (char[0x2000])
//       ?_stopApplication@Error@System@@0_NA   0x1801fc70a
//       ?_hideDisplay@Error@System@@0_NA       0x1801fc709
//
// *** NO INSTANCE STATE, SO NO LAYOUT AND NO `sizeof` ASSERT. ***
//
// *** THE THREE STATICS STAY `RT_DATA_SLOT` READS — RULE B4b-4. *** All three
// are `CUTOVER_REQUIRED` rows (manifest lines 71, 81, 73) answered by a minted
// storage cell, and `error.cpp:167-179` reads each through `RT_DATA_SLOT` by
// name. Turning one into a real `static` member REDIRECTS that cell at
// `standalone/rt_data_mint.cpp`'s `MintStorageCell()`, and `standalone/` is
// another agent's file set. FUNCTIONS convert here; STATICS do not. The debt is
// tabled per row in B34B_RULES §4.1.
//
// `_errorString` is the one of the eleven that is NOT a pointer: it is a
// 0x2000-byte BUFFER, so its eventual redirect must hand back the array's
// address, not the address of a pointer to it. Said here as well as in §4.1
// because that is the kind of detail a redirect gets wrong once.
//
// *** WHAT LIVES IN `error.cpp` AND IS NOT AN `Error` MEMBER. ***
// `?StrAppend@String@System@@SAPEADPEADHPEBD@Z` (0x1800c9020) is registered in
// `error.cpp` and is a `System::String` static, hosted there only to avoid a
// concurrent edit to `string.cpp`. It is NOT declared here, it is NOT an Error
// export, and it must not be counted as one or moved with this class.
//
// *** `Display` IS UNVERIFIED AT RUNTIME. *** Its own banner in `error.cpp`
// says so: it is the fatal `MessageBoxW` path and no oracle run has ever
// exercised it. Declaring it changes nothing about that; recorded so the next
// reader does not infer coverage from the conversion.
// ============================================================================

namespace System {

class Error {
public:
    // ?SetText@Error@System@@SAXPEBD_N@Z  0x1800c5fa0
    // Appends `text` and a `\n` to `_errorString`, and sets `_stopApplication`
    // from the flag. The newline is the two bytes at 0x180155dbc, read with
    // read_memory rather than inferred.
    static void SetText(const char* text, bool stopApplication);

    // ?Display@Error@System@@SAXXZ        0x1800c6060
    // The fatal MessageBoxW, suppressed when `_hideDisplay` is set.
    static void Display();

    // ---- the three statics (`0…A`: PRIVATE STATIC DATA in retail) ---------
    // ENDGAME B.3.9 SYS (B39_RULES D7): class statics, defined zero in
    // error.cpp; RtSystemPortInit redirects the three minted rows onto them
    // TOGETHER. `_errorString` is the 0x2000-byte BUFFER itself (MSVC decays
    // the member to `PAD` in the mangling), so the redirect hands the ARRAY's
    // address. PUBLIC while foreign readers remain.
    static char _errorString[0x2000];   // ?_errorString@Error@System@@0PADA     0x1801fc720
    static bool _hideDisplay;           // ?_hideDisplay@Error@System@@0_NA      0x1801fc709
    static bool _stopApplication;       // ?_stopApplication@Error@System@@0_NA  0x1801fc70a
};

}  // namespace System
