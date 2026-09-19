// ============================================================================
// runtime/src/system/value_types.cpp — Random/Time/DateTime/Color/Configuration
// ============================================================================
//
// Wave 2, Lane B of the Runtime-x64.dll port (docs/runtime_port/PLAN.md §4.2/§6).
// Every function below is a byte-faithful port of the decompilation at the
// retail address named in its comment (Runtime-x64.dll, image base
// 0x180000000, decompiled 2026-09-06). See runtime/types/system/Value.h for
// the governing layouts.
//
// STRING NOTE: four functions in this lane's scope return System::String
// (DateTime::AsString, Configuration::CurrentBuildId/CurrentVersion/
// GetVersionString). System::String's refcounted layout is Lane A's
// (runtime/types/system/String.h) — this file never models or touches it.
// Every String these functions produce is built by calling the DLL's own
// String::Set / String::Format / String::operator+= / String::Release
// through the DLL's own String routines, exactly as retail's own code does; we
// only ever
// read a String's PUBLIC content (its data pointer used as a plain
// null-terminated C string), never its private refcount/length header.
//
// Those calls go through RT_EXPORT_ONCE. Set / Format / operator+= / Release
// are all PORTED.
//
// ENDGAME B.3.1 step 2 REPLACES THE PARAGRAPH ABOVE. Runtime-x64.dll is gone,
// `System::String` is a real class over the same eight bytes, and the
// file-local opaque `StrSlot` — whose whole purpose was to avoid modelling a
// type that belonged to another binary — is deleted. The four bodies below
// hold real `System::String` locals with RAII, and the Set / Format /
// operator+= / Release name doors are the class's own members. The retail
// addresses on each call stay: they are the provenance.
// ============================================================================

#include <cstdio>
#include <cstring>
#include <ctime>
// WAVE 22 LANE SYSBODY: DateAsString/TimeAsString are Win32 date formatting
// (FileTimeToLocalFileTime / FileTimeToSystemTime / GetDateFormatW /
// GetTimeFormatW), so this file now needs the platform header. LEAN_AND_MEAN
// plus NOMINMAX because <wingdi.h> declares a function called `Rectangle` and
// <windef.h> #defines `near`/`far` away -- the collision compression.cpp's
// probe documents one directory over. Nothing in this file uses those names
// today; the narrowing is so that stays true.
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <new>          // placement new into the retail hidden return slot
#include "../../types/system/Value.h"
#include "../../types/system/String.h"
#include "../../types/io/Serializer.h"   // ENDGAME B.3.4 callers batch H
#include "../../funcs/rt_port.h"
#include "widestring.h"

namespace {

// ---- Random: seeding, twist and core draw (retail algorithm, see Value.h) --

// Standard MT19937 init_genrand recurrence. Shared by both ctors.
// Pinned by ??0Random@System@@QEAA@I@Z (0x1800c7ed0).
void SeedMT(Random* self, uint32_t seed) {
    self->index = 0;
    self->mt[0] = seed;
    for (int i = 1; i < 624; ++i) {
        uint32_t prev = self->mt[i - 1];
        self->mt[i] = (prev ^ (prev >> 30)) * 0x6c078965u + static_cast<uint32_t>(i);
    }
}

// ?NextGroup@Random@System@@AEAAXXZ  retail 0x1800c7dd0 (private, not exported;
// no dispatch slot exists for it — it is only ever reached through Rand()).
// Standard MT19937 twist (UPPER_MASK/LOWER_MASK/MATRIX_A), confirmed against
// the disassembly instruction-by-instruction.
void NextGroupImpl(Random* self) {
    constexpr uint32_t kUpper = 0x80000000u, kLower = 0x7fffffffu, kMatrixA = 0x9908b0dfu;
    for (int i = 0; i < 624; ++i) {
        uint32_t y = (self->mt[i] & kUpper) | (self->mt[(i + 1) % 624] & kLower);
        uint32_t next = self->mt[(i + 397) % 624] ^ (y >> 1);
        if (y & 1u) next ^= kMatrixA;
        self->mt[i] = next;
    }
}

// Core of ?Rand@Random@System@@QEAAIXZ (0x1800c7f90), factored out so every
// overload below (which all draw one uint32 and post-process it) shares one
// transcription of the tempering constants instead of five copies.
uint32_t RandImpl(Random* self) {
    if (self->index == 0) NextGroupImpl(self);
    uint32_t y = self->mt[self->index];
    y ^= y >> 11;
    y ^= (y & 0xff3a58adu) << 7;
    y ^= (y & 0xffffdf8cu) << 15;
    int next = self->index + 1;
    self->index = (next == 0x270) ? 0 : next;
    return y ^ (y >> 18);
}

}  // namespace

// ============================================================
// System::Random
// ============================================================

// ??0Random@System@@QEAA@XZ  retail 0x1800c7f20  (time-seeded default ctor).
// Not diffed: two calls made at different wall-clock times legitimately seed
// to different states, so there is no meaningful "original" to compare
// against (same rationale as Math::Vector2's default ctor in vector.cpp).
// Retail seeds from `(uint)_time64(&t)`; std::time's 64-bit time_t on this
// target truncates to unsigned the same way.
Random* RtSystem_Random_Ctor(Random* self) {
    SeedMT(self, static_cast<uint32_t>(std::time(nullptr)));
    return self;
}

// ??0Random@System@@QEAA@I@Z  retail 0x1800c7ed0
Random* RtSystem_Random_CtorSeed(Random* self, uint32_t seed) {
    SeedMT(self, seed);
    return self;
}

// ?NextGroup@Random@System@@AEAAXXZ  retail 0x1800c7dd0, ordinal 3245.
//
// WAVE 28 LANE MISC.  The BODY for this row has existed since Wave 2 as the
// file-local `NextGroupImpl` above; what was missing was the REGISTRATION, so a
// callee census could not tell it from an absent body and Wave 27's board
// carried it as a named unported System BODY row.  This wrapper is the whole of
// the port -- it adds no logic, it opens the door.
//
// AEAA is the PRIVATE spelling, NOT the unexported one.  Verified against the
// export directory with pefile, by name AND by RVA: both lookups return
// (?NextGroup@Random@System@@AEAAXXZ, 0x1800c7dd0, ord 3245).  Registering the
// name is therefore the plain door, not a workaround.
//
extern "C" void RtSystem_Random_NextGroup(Random* self) { NextGroupImpl(self); }

// ?Rand@Random@System@@QEAAIXZ  retail 0x1800c7f90
uint32_t RtSystem_Random_Rand(Random* self) {
    Random before;
    uint32_t ours = RandImpl(self);
    return ours;
}

// ?Rand@Random@System@@QEAAII@Z  retail 0x1800c8040  (bound==0 short-circuits
// to 0 WITHOUT drawing — state must stay untouched, hence the ternary rather
// than an early `if`+call).
uint32_t RtSystem_Random_RandBound(Random* self, uint32_t bound) {
    Random before;
    uint32_t ours = (bound == 0) ? 0u : (RandImpl(self) % bound);
    return ours;
}

// ?Rand@Random@System@@QEAAHHH@Z  retail 0x1800c7d70
int32_t RtSystem_Random_RandRange(Random* self, int32_t lo, int32_t hi) {
    Random before;
    uint32_t r = RandImpl(self);
    int32_t ours = lo + static_cast<int32_t>(r % (static_cast<uint32_t>(hi - lo) + 1u));
    return ours;
}

// ?RandFloat@Random@System@@QEAAMXZ  retail 0x1800c7d40
// DAT_18016165c = 1/2^32 exactly (read as bytes: 0x2f800000): maps a full
// 32-bit draw to [0,1).
namespace { constexpr float kRandUnit = 2.3283064365386963e-10f; }  // 0x2f800000
float RtSystem_Random_RandFloat(Random* self) {
    Random before;
    float ours = static_cast<float>(RandImpl(self)) * kRandUnit;
    return ours;
}

// ?RandFloatTol@Random@System@@QEAAMXZ  retail 0x1800c8000
// return 1.0f - (u * (1/2^32)) * 2.0f, left-associated exactly as retail.
float RtSystem_Random_RandFloatTol(Random* self) {
    Random before;
    float ours = 1.0f - (static_cast<float>(RandImpl(self)) * kRandUnit) * 2.0f;
    return ours;
}

// ?RandTol@Random@System@@QEAAHH@Z  retail 0x1800c7da0
int32_t RtSystem_Random_RandTol(Random* self, int32_t tol) {
    Random before;
    uint32_t r = RandImpl(self);
    int32_t ours = static_cast<int32_t>(r % (static_cast<uint32_t>(tol) * 2u + 1u)) - tol;
    return ours;
}

// ?Serialize@Random@System@@QEAAXAEAVSerializer@IO@@@Z  retail 0x1800c7cf0
// Two calls: slot +0x98 serializes the plain `index` (same slot
// Color::Serialize uses for its own 4-byte scalar), slot +0x40 serializes the
// 624-entry uint32 array. I/O side effect on a live stream — not diffed, same
// rationale as Vector2::operator*= in vector.cpp (running the original would
// mean writing the record twice).
//
// ENDGAME B.3.4 callers batch H. `serializer + 8` was called "the stream"
// here; it is the READERWRITER (the Stream is at Serializer+0x00). +0x98 is
// rw_vtbl::kScalarUInt and +0x40 is rw_vtbl::kArrayUInt, so the doors name the
// types: `unsigned int&` and `(int, unsigned int*)`. The old trailing `0` was
// the ignored `const char* name`.
void RtSystem_Random_Serialize(Random* self, void* serializer) {
    reinterpret_cast<::IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<unsigned int*>(&self->index), nullptr);   // B.3.4 bridge  (0x98 = uint door)
    reinterpret_cast<::IO::Serializer*>(serializer)->Serialize(0x270, reinterpret_cast<unsigned int*>(self->mt), nullptr);   // B.3.4 bridge  (0x40 = uint-array door)
}

// ============================================================
// System::Time
// ============================================================

// ??0Time@System@@QEAA@M@Z  retail 0x1800078c0
// Delegates to Timer::SecondsToTicks. Not diffed here: the delegation is one
// call and the arithmetic it delegates to now carries its OWN diff site
// (runtime/src/system/timer.cpp), so diffing it again would count the same
// comparison twice under a second name.
//
Time* RtSystem_Time_Ctor(Time* self, float seconds) {
    using SecondsToTicksFn = int64_t(*)(float);
    // Resolved ONCE per call site either way: this body runs on every
    // Update(const Time&), and resolving per call put a GetProcAddress under
    // the loader lock on the engine's hottest value type -- 5.6 points of sim
    // time, most of the Wave 2 perf-gate breach, and invisible to both
    // profilers because it lands in ntdll self time
    // (reference_rtoriginal_is_getprocaddress.md, WAVE3_LANE_P.md).
    self->ticks = RT_EXPORT_ONCE(SecondsToTicksFn,
                                 "?SecondsToTicks@Timer@System@@SA_JM@Z")(seconds);
    return self;
}

// ?AsSeconds@Time@System@@QEBAMXZ  retail 0x180007900 — same delegation, same
// RT_EXPORT_ONCE reasoning as the ctor above.
float RtSystem_Time_AsSeconds(const Time* self) {
    using TicksToSecondsFn = float(*)(int64_t);
    return RT_EXPORT_ONCE(TicksToSecondsFn,
                          "?TicksToSeconds@Timer@System@@SAM_J@Z")(self->ticks);
}

// ?Serialize@Time@System@@QEAAXAEAVSerializer@IO@@@Z  retail 0x1800ca740
// Slot +0x90 serializes the 8-byte `ticks`. Not diffed (I/O side effect, see
// Random::Serialize above).
//
// ENDGAME B.3.4 callers batch H LEFT THIS ONE BYTE-IDENTICAL, deliberately.
// `serializer + 8` is the READERWRITER here too, and +0x90 is
// rw_vtbl::kScalarInt64 — but `IO::Serializer` does NOT declare the int64
// scalar overload. runtime/types/io/Serializer.h says why: retail's int64 and
// uint64 forms "are ABSENT ON PURPOSE: no call site in this tree reaches them,
// and an uncalled port is an unverified port. Adding one is a porting
// decision, not a tidy-up." This site is that call site, so the overload is
// now justified — but minting it is a receiver change, not a caller batch's,
// and it belongs with whoever next opens `IO::Serializer`. Reported, not
// converted, and not "fixed" by routing the 8 bytes through a 4-byte door.
void RtSystem_Time_Serialize(Time* self, void* serializer) {
    void* stream = *reinterpret_cast<void**>(reinterpret_cast<char*>(serializer) + 8);
    void* vtbl = *reinterpret_cast<void**>(stream);
    using SerializeInt64Fn = void(*)(void*, void*, int);
    SerializeInt64Fn fn = *reinterpret_cast<SerializeInt64Fn*>(reinterpret_cast<char*>(vtbl) + 0x90);
    fn(stream, &self->ticks, 0);
}

// ============================================================
// System::Color
// ============================================================

// ?Serialize@Color@System@@QEAAXAEAVSerializer@IO@@@Z  retail 0x1800c4150
// Slot +0x98 serializes the packed 4-byte `value` (same slot Random uses for
// its plain int). Not diffed (I/O side effect, see Random::Serialize above).
// ENDGAME B.3.4 callers batch H: `serializer + 8` is the READERWRITER, +0x98
// is rw_vtbl::kScalarUInt, and the trailing `0` was the ignored `name`.
void RtSystem_Color_Serialize(Color* self, void* serializer) {
    reinterpret_cast<::IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<unsigned int*>(&self->value), nullptr);   // B.3.4 bridge  (0x98 = uint door)
}

// ============================================================
// System::DateTime / System::Configuration — String-returning functions.
// See the file header for why StrSlot is opaque and why these delegate to
// the DLL's own String routines rather than modeling System::String.
// ============================================================

// ENDGAME B.3.1 step 2: `struct StrSlot { void* p; };` is gone — the four
// bodies below hold `System::String` itself.

// ?AsString@DateTime@System@@QEBA?AVString@2@XZ  retail 0x1800c54d0
// retail: Format(ret, "%s %s", DateAsString(this), TimeAsString(this)), then
// releases both temporaries. DateAsString/TimeAsString are themselves
// exported (0x1800c56a0 / 0x1800c55b0), so `this` (DateTime) never needs a
// layout here — it is only ever handed straight through to them.
// WAVE 9 LANE V2, 2026-09-07: every raw door in the three functions below is
// RT_EXPORT_ONCE. `?Release@String@System@@AEAAXXZ`, DateAsString and
// TimeAsString are all ported and registered below, so these lines reach our
// own bodies through the registry.
System::String* RtSystem_DateTime_AsString(const void* self, System::String* ret) {
    using StringRetFn = System::String*(*)(const void*, System::String*);

    // ENDGAME B.3.1 step 2. THE DECLARATION ORDER IS DELIBERATELY THE REVERSE
    // OF THE FILL ORDER: retail releases dateStr FIRST and timeStr second
    // (0x1800c5546 / 0x1800c5555), and reverse-order destruction of these two
    // reproduces that. They are separate one-reference allocations, so both
    // are freed here and the ORDER of the two MemFree calls is visible to the
    // allocator's free list — which is why it is matched rather than assumed
    // not to matter.
    System::String timeStr;
    System::String dateStr;
    RT_EXPORT_ONCE(StringRetFn, "?DateAsString@DateTime@System@@QEBA?AVString@2@XZ")(self, &dateStr);
    RT_EXPORT_ONCE(StringRetFn, "?TimeAsString@DateTime@System@@QEBA?AVString@2@XZ")(self, &timeStr);

    // ?Format@String@System@@SA?AV12@PEBDZZ (0x1800c9730), spelled as itself.
    // `ret` is the hidden return slot — raw bytes — so the result is
    // placement-constructed into it; C++17 elides straight into the slot, so
    // there is no extra AddRef/Release pair retail did not spend.
    new (ret) System::String(System::String::Format("%s %s", dateStr._data, timeStr._data));

    // The two `?Release@String@System@@AEAAXXZ` calls are the destructors now.
    return ret;
}

// ============================================================================
// WAVE 22 LANE SYSBODY — DateAsString / TimeAsString
// ============================================================================
//   ?DateAsString@DateTime@System@@QEBA?AVString@2@XZ   0x1800c56a0  ord 2149
//   ?TimeAsString@DateTime@System@@QEBA?AVString@2@XZ   0x1800c55b0  ord 4131
//
// These are the two functions AsString above has been forwarding to since
// Wave 9. Registering them therefore lights up a route that already exists:
// AsString resolves both BY NAME through RT_EXPORT_ONCE (the registry), so the
// bodies below become the ones it calls the moment RtSystemValueRegister runs.
// That is a real caller, not a synthetic driver.
//
// THE HIDDEN RETURN SLOT IS REAL, AND GHIDRA DROPPED IT. `decompile_function`
// prints both of these as `void __thiscall DateAsString(DateTime *this)` — no
// second parameter and no return — which is the documented failure mode
// (reference_ghidra_c_output_vs_disassembly.md: whole arguments dropped). The
// disassembly is unambiguous:
//     1800c56a0  MOV qword ptr [RSP+0x10],RDX    ; RDX is a REAL argument
//     1800c56b6  MOV RBX,RDX                     ; kept across the whole body
//     1800c5773  MOV RAX,RBX / RET               ; and returned again in RAX
// so the signature is `String* f(const DateTime* this, String* ret)` — RCX =
// this, RDX = ret — and the callee returns the slot. Writing this as a C++
// method returning `String` by value is the wild write in
// reference_hidden_retslot_wild_write.md, which "works" because the right
// answer still lands in the right place while a second, uninitialised pointer
// is scribbled through. Spelled out as a parameter here, exactly as every
// other String-returning port in this tree does, and `StrSlot` is the same
// opaque 8-byte pass-through the rest of this file uses.
//
// THE TWO BODIES ARE THE SAME FUNCTION WITH TWO CONSTANTS. Instruction for
// instruction, DateAsString and TimeAsString differ in exactly two places:
//     1800c56fc  LEA EDX,[R9+0x1] / CALL [0x18014b130]   GetDateFormatW, flag 1
//     1800c560c  LEA EDX,[R9+0x2] / CALL [0x18014b138]   GetTimeFormatW, flag 2
// (1 = DATE_SHORTDATE, 2 = TIME_NOSECONDS; locale 0x400 = LOCALE_USER_DEFAULT
// in both, cchOut 0x100 in both.) They share one helper below rather than
// being transcribed twice, because two copies of a 20-line body is how the
// second one drifts.
//
// `this` IS A FILETIME. Retail's first instruction hands `this` straight to
// FileTimeToLocalFileTime, so System::DateTime is an 8-byte FILETIME and
// needs no layout of its own — the same reason the file header gives for
// AsString. Left as `const void*` for that reason.
//
// THE WIDESTRING TEMPORARY IS RELEASED INLINE, AND WE ALREADY OWN THAT.
// 0x1800c5742..0x1800c5773 is `WideString::~WideString` inlined: decrement the
// refcount at data-4, and on zero rewind to data-8 and MemFree. That is
// bit-for-bit the sequence `RtSystem_WideString_Dtor` (widestring.cpp, Wave 9
// lane FS) already implements, including its asymmetry, so this calls that
// rather than open-coding a third copy of the idiom.
static System::String* RtSystem_DateTime_FormatPart(const void* self, System::String* ret,
                                                    bool timePart) {
    // DELIBERATELY UNINITIALISED, and this is the one thing the sweep below
    // CAUGHT rather than confirmed. Retail's prologue reserves 0x250 bytes and
    // writes NOTHING into these three slots before the API calls
    // (0x1800c56a5..0x1800c56c1 is a PUSH, a SUB RSP,0x250 and two stores of a
    // frame cookie -- no zeroing of [RSP+0x40], [RSP+0x50] or [RSP+0x270]).
    // It then IGNORES both BOOL results. So when the conversion FAILS --
    // FILETIME 0, the 1601 epoch, is the easy case -- retail formats from an
    // uninitialised stack buffer and returns whatever was lying there.
    //
    // The first draft of this port wrote `FILETIME local{}; SYSTEMTIME st{};`
    // and the sweep reported a divergence at exactly that input: ours produced
    // one piece of mojibake and retail another. That is not a defect in either
    // body; it is two different stacks. Zero-initialising is nevertheless the
    // WRONG transcription -- a behaviour change on the failure path, invisible
    // on the success path, and exactly the sort of tidy-up this project's
    // rules exist to prevent. Left uninitialised so the port says what retail
    // does. Do NOT "fix" a -Wmaybe-uninitialized warning here by adding `{}`.
    //
    // The failure path is UNCOMPARABLE BY CONSTRUCTION rather than merely
    // untested, and the sweep says so explicitly instead of scoring it.
    FILETIME local;
    SYSTEMTIME st;
    // Unguarded, and faithful: retail passes `this` straight through with no
    // null test and ignores both BOOL results. A guard here would change what
    // the function returns rather than protect anything.
    FileTimeToLocalFileTime(static_cast<const FILETIME*>(self), &local);
    FileTimeToSystemTime(&local, &st);

    wchar_t buf[256];   // 0x100 WCHARs, retail's [RSP+0x50] scratch -- also uninitialised
    if (timePart)
        GetTimeFormatW(0x400, 2, &st, nullptr, buf, 0x100);
    else
        GetDateFormatW(0x400, 1, &st, nullptr, buf, 0x100);

    // Retail zeroes the WideString slot before Set, because Set releases
    // whatever is there first (0x1800c570b: MOV qword [RSP+0x260],0).
    RtSystem::WideString ws{};
    RtSystem::RtSystem_WideString_Set(&ws, buf);
    // TRANSITIONAL CAST, the class->POD direction: `WideString::AsUTF8` is a
    // registered C entry point whose hidden return slot is the POD. Same eight
    // bytes, and it placement-constructs into this raw slot.
    RtSystem::RtSystem_WideString_AsUTF8(&ws, reinterpret_cast<RtSystem::String*>(ret));
    RtSystem::RtSystem_WideString_Dtor(&ws);
    return ret;   // MOV RAX,RBX
}

System::String* RtSystem_DateTime_DateAsString(const void* self, System::String* ret) {
    return RtSystem_DateTime_FormatPart(self, ret, /*timePart=*/false);
}

System::String* RtSystem_DateTime_TimeAsString(const void* self, System::String* ret) {
    return RtSystem_DateTime_FormatPart(self, ret, /*timePart=*/true);
}

// ?CurrentBuildId@Configuration@System@@SA?AVString@2@XZ  retail 0x1800c4c60
// Builds a 6-character stamp "<last 2 digits of year><2-letter month><day,
// zero-padded>" from the DLL's own compiled-in date string (e.g.
// "Sep 10 2017" -> "17Se10"), read via String::Set/Release so the DLL's own
// allocation is what we read characters out of, not a layout we assume.
System::String* RtSystem_Configuration_CurrentBuildId(System::String* ret) {
    static const char* const kMonths[12] = {
        "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
    };

    // ENDGAME B.3.1 step 2: the Set (0x1800c95d0) and Release (0x1800c9870)
    // name doors are the class's own member and destructor now.
    System::String dateStr;
    dateStr.Set("Sep 10 2017");  // 0x180155c50 -- the DLL's own __DATE__-style literal
    const char* d = dateStr._data;  // "Sep 10 2017"

    char scratch[7] = "000000";
    scratch[0] = d[9];   // last two digits of the year ("17")
    scratch[1] = d[10];
    for (const char* month : kMonths) {
        if (d[0] == month[0] && d[1] == month[1] && d[2] == month[2]) {
            scratch[2] = month[0];
            scratch[3] = month[1];
            break;
        }
    }
    // Day-of-month digits overwrite the '0' padding only where retail's
    // source is actually a digit (a single-digit day is space-padded, e.g.
    // "Jan 5 2017", and that space must NOT clobber the zero).
    if (d[4] >= '0' && d[4] <= '9') scratch[4] = d[4];
    if (d[5] >= '0' && d[5] <= '9') scratch[5] = d[5];

    // NOTE, TRANSCRIBED NOT TIDIED: retail calls Set on `ret` WITHOUT zeroing
    // the hidden return slot first, so this is `Set` on the raw slot and not a
    // placement-construct. Set's own release runs on whatever the caller left
    // there. Left exactly as retail spells it — the caller is the one that
    // guarantees the slot; changing it here would be a behaviour change.
    ret->Set(scratch);
    // `release(&dateStr)` is dateStr's destructor now.
    return ret;
}

// ?CurrentVersion@Configuration@System@@SA?AVString@2@XZ  retail 0x1800c4f40
System::String* RtSystem_Configuration_CurrentVersion(System::String* ret);  // fwd, defined below GetVersionString

// ?GetVersionString@Configuration@System@@SA?AVString@2@I_N@Z  retail 0x1800c4f90
// version is packed major<<24 | minor<<16 | patch<<8 | suffix. suffix in
// {0xfb..0xfe} appends a pre-release tag (short form when `detailed`, e.g.
// "R"/"P"/"A"/"B", else the full word); any other suffix (incl. retail's
// shipped 0xff) appends nothing. Text taken from the DLL's own .rdata
// literals at 0x180155c14..0x180155c40, transcribed to source for
// readability -- content verified byte-for-byte against inspect_memory.
System::String* RtSystem_Configuration_GetVersionString(System::String* ret, uint32_t version,
                                                        bool detailed) {
    // ENDGAME B.3.1 step 2: ?Format@String@System@@SA?AV12@PEBDZZ (0x1800c9730)
    // spelled as itself, placement-constructed into the raw hidden return slot.
    new (ret) System::String(System::String::Format(
        "%i.%i.%i", version >> 24, (version >> 16) & 0xffu, (version >> 8) & 0xffu));

    const char* text = nullptr;
    switch (version & 0xffu) {
        case 0xfb: text = detailed ? "R" : " Prototype"; break;
        case 0xfc: text = detailed ? "P" : " Pre Alpha"; break;
        case 0xfd: text = detailed ? "A" : " Alpha"; break;
        case 0xfe: text = detailed ? "B" : " Beta"; break;
        default: return ret;
    }

    // ??YString@System@@QEAA?AV01@PEBD@Z (0x1800c9120) returns the new value BY
    // VALUE and the caller discards the copy; that discarded temporary IS
    // retail's `tmp`, and its destructor IS the 0x1800c9870 release below it.
    // Both lines collapse into this one (cookbook APPEND).
    *ret += text;
    return ret;
}

System::String* RtSystem_Configuration_CurrentVersion(System::String* ret) {
    return RtSystem_Configuration_GetVersionString(ret, 0x010007ffu, false);
}

// ============================================================
// RtSystemValueRegister — called from RtSystemPortInit() (rt_system_port.cpp).
// ============================================================
int RtSystemValueRegister() {
    int n = 0;

    RegisterRuntimeExport("??0Random@System@@QEAA@XZ", (void*)&RtSystem_Random_Ctor); ++n;
    RegisterRuntimeExport("??0Random@System@@QEAA@I@Z", (void*)&RtSystem_Random_CtorSeed); ++n;
    // WAVE 28 LANE MISC: the body has existed since Wave 2; this line is the port.
    RegisterRuntimeExport("?NextGroup@Random@System@@AEAAXXZ", (void*)&RtSystem_Random_NextGroup); ++n;
    RegisterRuntimeExport("?Rand@Random@System@@QEAAIXZ", (void*)&RtSystem_Random_Rand); ++n;
    RegisterRuntimeExport("?Rand@Random@System@@QEAAII@Z", (void*)&RtSystem_Random_RandBound); ++n;
    RegisterRuntimeExport("?Rand@Random@System@@QEAAHHH@Z", (void*)&RtSystem_Random_RandRange); ++n;
    RegisterRuntimeExport("?RandFloat@Random@System@@QEAAMXZ", (void*)&RtSystem_Random_RandFloat); ++n;
    RegisterRuntimeExport("?RandFloatTol@Random@System@@QEAAMXZ", (void*)&RtSystem_Random_RandFloatTol); ++n;
    RegisterRuntimeExport("?RandTol@Random@System@@QEAAHH@Z", (void*)&RtSystem_Random_RandTol); ++n;
    RegisterRuntimeExport("?Serialize@Random@System@@QEAAXAEAVSerializer@IO@@@Z", (void*)&RtSystem_Random_Serialize); ++n;

    RegisterRuntimeExport("??0Time@System@@QEAA@M@Z", (void*)&RtSystem_Time_Ctor); ++n;
    RegisterRuntimeExport("?AsSeconds@Time@System@@QEBAMXZ", (void*)&RtSystem_Time_AsSeconds); ++n;
    RegisterRuntimeExport("?Serialize@Time@System@@QEAAXAEAVSerializer@IO@@@Z", (void*)&RtSystem_Time_Serialize); ++n;

    RegisterRuntimeExport("?Serialize@Color@System@@QEAAXAEAVSerializer@IO@@@Z", (void*)&RtSystem_Color_Serialize); ++n;

    RegisterRuntimeExport("?AsString@DateTime@System@@QEBA?AVString@2@XZ", (void*)&RtSystem_DateTime_AsString); ++n;
    // WAVE 22 LANE SYSBODY -- the two AsString has been forwarding to since
    // Wave 9. Name only: grep over runtime/, engine/, code/ and standalone/
    // finds 0x1800c56a0 / 0x1800c55b0 in exactly one place each, a comment in
    // this file, so there is no by-address consumer to satisfy.
    RegisterRuntimeExport("?DateAsString@DateTime@System@@QEBA?AVString@2@XZ",
                          (void*)&RtSystem_DateTime_DateAsString); ++n;
    RegisterRuntimeExport("?TimeAsString@DateTime@System@@QEBA?AVString@2@XZ",
                          (void*)&RtSystem_DateTime_TimeAsString); ++n;

    RegisterRuntimeExport("?CurrentBuildId@Configuration@System@@SA?AVString@2@XZ", (void*)&RtSystem_Configuration_CurrentBuildId); ++n;
    RegisterRuntimeExport("?CurrentVersion@Configuration@System@@SA?AVString@2@XZ", (void*)&RtSystem_Configuration_CurrentVersion); ++n;
    RegisterRuntimeExport("?GetVersionString@Configuration@System@@SA?AVString@2@I_N@Z", (void*)&RtSystem_Configuration_GetVersionString); ++n;

    return n;
}
