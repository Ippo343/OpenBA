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
// UPDATED Wave 9 lane B3: those calls now go through RT_EXPORT_ONCE, not
// RtOriginal<>. Set / Format / operator+= / Release are all PORTED, and
// RtOriginal deliberately skips the port registry, so every one of them was a
// live-path port bypass. The
// local `StrSlot` type below is deliberately an opaque 8-byte pass-through
// (not a port of System::String) used only to give these calls a typed
// hidden-return-slot argument.
//
// DIFF HARNESS NOTE FOR System::Random: Random carries state (index + a
// 624-word array) that every non-ctor member mutates. Running the original
// on the SAME live object would advance it twice; running it on an
// independent copy seeded fresh would test a different RNG stream. The
// correct shape (docs/runtime_port/WAVE1.md's "oracle must not corrupt its
// own experiment"): snapshot the state BEFORE our call runs, let ours mutate
// the live object, then replay the original on a scratch copy of the
// snapshot and compare both the return value and the resulting state.
// RtDiffRandomShadow() below does exactly that and is used by every
// state-mutating Random member.
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
#include "../../types/system/Value.h"
#include "../../types/system/String.h"
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

// Shared shadow-diff step for every stateful Random member. `before` is a
// snapshot the caller took BEFORE its own call ran; this replays the mangled
// original on an independent copy and reports both the return value and the
// resulting state as one comparison. Called only under `if (rt_diff_on__)`,
// so it costs nothing in a normal run.
template <typename T, typename Fn, typename... Args>
void RtDiffRandomShadow(RtDiffSite* site, Random* self, const Random& before,
                         const char* mangled, T ours, Args... args) {
    // Guard the helper ITSELF, not just its six call sites.  Every caller today
    // wraps this in `if (rt_diff_on__)` and the comment above says so -- but
    // that invariant lives in six places and a seventh caller only has to
    // forget once.  What it would cost is not a wasted comparison: the
    // RtOriginal below is the RAW door, so an unguarded call is a live
    // cross-DLL call into retail on an ordinary run, and a call through NULL
    // once Runtime-x64.dll is gone.  That is precisely the shape lane V2 found
    // escaped in GraphElement::RemoveGraphs, which had sat between
    // RT_DIFF_BEGIN and RT_DIFF_SCALAR calling retail every frame.
    //
    // rt_diff_on__ is a macro-local of RT_DIFF_BEGIN and is not in scope here;
    // RtDiffEnabled() is the same predicate.  rawdoor_audit.py recognises this
    // early-out as a guard (its diff_only_fn_depth idiom), which it cannot do
    // for a guard that lives in the CALLER.
    if (!RtDiffEnabled()) return;

    Random scratch = before;
    T theirs = RtOriginal<Fn>(mangled)(&scratch, args...);
    bool eq = (ours == theirs) && std::memcmp(self, &scratch, sizeof(Random)) == 0;
    char a[32], b[32];
    RtDiffFormat(a, sizeof(a), ours);
    RtDiffFormat(b, sizeof(b), theirs);
    RtDiffReport(site, eq, a, b);
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
    RT_DIFF_BEGIN("System::Random::Random(uint)");
    if (rt_diff_on__) {
        Random theirs;
        RtOriginal<void(*)(Random*, uint32_t)>("??0Random@System@@QEAA@I@Z")(&theirs, seed);
        bool eq = std::memcmp(self, &theirs, sizeof(Random)) == 0;
        RT_DIFF_SHADOW(eq, "seeded", "seeded");
    }
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
// NOT DIFFED, deliberately.  Every other Random member here compares through
// RtDiffRandomShadow, which replays the original on a scratch COPY of a
// pre-call snapshot.  That shape works because those members RETURN a value.
// NextGroup returns void and its entire effect is the 624-word state, which
// `RtDiffRandomShadow`'s memcmp of `self` against the scratch already covers on
// every Rand* call that triggers a twist -- i.e. this body is under continuous
// comparison through its only retail caller, and a second oracle wrapped around
// the same bytes would be the self-check shape
// (reference_shadow_marker_is_not_an_oracle.md), not a new screen.
extern "C" void RtSystem_Random_NextGroup(Random* self) { NextGroupImpl(self); }

// ?Rand@Random@System@@QEAAIXZ  retail 0x1800c7f90
uint32_t RtSystem_Random_Rand(Random* self) {
    RT_DIFF_BEGIN("System::Random::Rand");
    Random before;
    if (rt_diff_on__) before = *self;
    uint32_t ours = RandImpl(self);
    if (rt_diff_on__)
        RtDiffRandomShadow<uint32_t, uint32_t(*)(Random*)>(
            &rt_diff_site__, self, before, "?Rand@Random@System@@QEAAIXZ", ours);
    return ours;
}

// ?Rand@Random@System@@QEAAII@Z  retail 0x1800c8040  (bound==0 short-circuits
// to 0 WITHOUT drawing — state must stay untouched, hence the ternary rather
// than an early `if`+call).
uint32_t RtSystem_Random_RandBound(Random* self, uint32_t bound) {
    RT_DIFF_BEGIN("System::Random::Rand(uint)");
    Random before;
    if (rt_diff_on__) before = *self;
    uint32_t ours = (bound == 0) ? 0u : (RandImpl(self) % bound);
    if (rt_diff_on__)
        RtDiffRandomShadow<uint32_t, uint32_t(*)(Random*, uint32_t)>(
            &rt_diff_site__, self, before, "?Rand@Random@System@@QEAAII@Z", ours, bound);
    return ours;
}

// ?Rand@Random@System@@QEAAHHH@Z  retail 0x1800c7d70
int32_t RtSystem_Random_RandRange(Random* self, int32_t lo, int32_t hi) {
    RT_DIFF_BEGIN("System::Random::Rand(int,int)");
    Random before;
    if (rt_diff_on__) before = *self;
    uint32_t r = RandImpl(self);
    int32_t ours = lo + static_cast<int32_t>(r % (static_cast<uint32_t>(hi - lo) + 1u));
    if (rt_diff_on__)
        RtDiffRandomShadow<int32_t, int32_t(*)(Random*, int32_t, int32_t)>(
            &rt_diff_site__, self, before, "?Rand@Random@System@@QEAAHHH@Z", ours, lo, hi);
    return ours;
}

// ?RandFloat@Random@System@@QEAAMXZ  retail 0x1800c7d40
// DAT_18016165c = 1/2^32 exactly (read as bytes: 0x2f800000): maps a full
// 32-bit draw to [0,1).
namespace { constexpr float kRandUnit = 2.3283064365386963e-10f; }  // 0x2f800000
float RtSystem_Random_RandFloat(Random* self) {
    RT_DIFF_BEGIN("System::Random::RandFloat");
    Random before;
    if (rt_diff_on__) before = *self;
    float ours = static_cast<float>(RandImpl(self)) * kRandUnit;
    if (rt_diff_on__)
        RtDiffRandomShadow<float, float(*)(Random*)>(
            &rt_diff_site__, self, before, "?RandFloat@Random@System@@QEAAMXZ", ours);
    return ours;
}

// ?RandFloatTol@Random@System@@QEAAMXZ  retail 0x1800c8000
// return 1.0f - (u * (1/2^32)) * 2.0f, left-associated exactly as retail.
float RtSystem_Random_RandFloatTol(Random* self) {
    RT_DIFF_BEGIN("System::Random::RandFloatTol");
    Random before;
    if (rt_diff_on__) before = *self;
    float ours = 1.0f - (static_cast<float>(RandImpl(self)) * kRandUnit) * 2.0f;
    if (rt_diff_on__)
        RtDiffRandomShadow<float, float(*)(Random*)>(
            &rt_diff_site__, self, before, "?RandFloatTol@Random@System@@QEAAMXZ", ours);
    return ours;
}

// ?RandTol@Random@System@@QEAAHH@Z  retail 0x1800c7da0
int32_t RtSystem_Random_RandTol(Random* self, int32_t tol) {
    RT_DIFF_BEGIN("System::Random::RandTol");
    Random before;
    if (rt_diff_on__) before = *self;
    uint32_t r = RandImpl(self);
    int32_t ours = static_cast<int32_t>(r % (static_cast<uint32_t>(tol) * 2u + 1u)) - tol;
    if (rt_diff_on__)
        RtDiffRandomShadow<int32_t, int32_t(*)(Random*, int32_t)>(
            &rt_diff_site__, self, before, "?RandTol@Random@System@@QEAAHH@Z", ours, tol);
    return ours;
}

// ?Serialize@Random@System@@QEAAXAEAVSerializer@IO@@@Z  retail 0x1800c7cf0
// Two calls through the stream's vtable: slot +0x98 serializes the plain
// `index` int (same slot Color::Serialize uses for its own 4-byte scalar),
// slot +0x40 serializes the 624-entry uint32 array. I/O side effect on a
// live stream — not diffed, same rationale as Vector2::operator*= in
// vector.cpp (running the original would mean writing the record twice).
void RtSystem_Random_Serialize(Random* self, void* serializer) {
    void* stream = *reinterpret_cast<void**>(reinterpret_cast<char*>(serializer) + 8);
    void* vtbl = *reinterpret_cast<void**>(stream);
    using SerializeIntFn = void(*)(void*, void*, int);
    SerializeIntFn serializeIndex = *reinterpret_cast<SerializeIntFn*>(reinterpret_cast<char*>(vtbl) + 0x98);
    serializeIndex(stream, &self->index, 0);

    using SerializeArrayFn = void(*)(void*, int, void*, int);
    SerializeArrayFn serializeMt = *reinterpret_cast<SerializeArrayFn*>(reinterpret_cast<char*>(vtbl) + 0x40);
    serializeMt(stream, 0x270, self->mt, 0);
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
// **RT_EXPORT_ONCE, not RT_ORIGINAL_ONCE, since Wave 9 lane B3.** The
// paragraph this replaced said Timer was out of scope because
// `_secondsPerTick` is "a runtime QPC-derived global, not a constant we can
// hardcode". Reading Timer::Startup's disassembly shows that is not so — it is
// the constant 1e-6, written by a store, with no QPC anywhere in it — and
// System::Timer is now ported. Leaving RT_ORIGINAL_ONCE here would have made
// the hottest value type in the engine a port bypass on the day the port
// landed, which is the exact shape WAVE9_LANE_V.md §1c catalogues.
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
void RtSystem_Color_Serialize(Color* self, void* serializer) {
    void* stream = *reinterpret_cast<void**>(reinterpret_cast<char*>(serializer) + 8);
    void* vtbl = *reinterpret_cast<void**>(stream);
    using SerializeUInt32Fn = void(*)(void*, void*, int);
    SerializeUInt32Fn fn = *reinterpret_cast<SerializeUInt32Fn*>(reinterpret_cast<char*>(vtbl) + 0x98);
    fn(stream, &self->value, 0);
}

// ============================================================
// System::DateTime / System::Configuration — String-returning functions.
// See the file header for why StrSlot is opaque and why these delegate to
// the DLL's own String routines rather than modeling System::String.
// ============================================================

namespace {
struct StrSlot { void* p; };  // opaque 8-byte pass-through, NOT a String port
}

// ?AsString@DateTime@System@@QEBA?AVString@2@XZ  retail 0x1800c54d0
// retail: Format(ret, "%s %s", DateAsString(this), TimeAsString(this)), then
// releases both temporaries. DateAsString/TimeAsString are themselves
// exported (0x1800c56a0 / 0x1800c55b0), so `this` (DateTime) never needs a
// layout here — it is only ever handed straight through to them.
// WAVE 9 LANE V2, 2026-09-07: every raw door in the three functions below is
// now RT_EXPORT_ONCE. None of them was an oracle -- they are the work these
// bodies do -- and two things were wrong with the raw spelling:
//   * `?Release@String@System@@AEAAXXZ` HAS BEEN PORTED (lane B3,
//     string.cpp:1113). All three of its uses here were live-path port
//     BYPASSES; port_bypass_audit.py named exactly these three. Nothing in
//     this file changed to cause that -- the port landed elsewhere, which is
//     why that gate says re-run after every LANDING, not after every edit.
//   * DateAsString / TimeAsString are unported, but the raw door is
//     ResolveExportRaw, which returns NULL once Runtime-x64.dll is absent. A
//     null call at cutover is a crash; a registry miss is a named row.
//     WAVE 22 LANE SYSBODY: "unported" is no longer true -- both are ported
//     and registered below, so these two RT_EXPORT_ONCE lines now reach our
//     own bodies. The RT_EXPORT_ONCE spelling is what makes that automatic
//     and is why it is left exactly as it is; only this sentence changed.
// RT_EXPORT_ONCE also removes a GetProcAddress per call, which bare RtOriginal
// was paying on every one of these.
StrSlot* RtSystem_DateTime_AsString(const void* self, StrSlot* ret) {
    using SubStringFn = StrSlot*(*)(const void*, StrSlot*);
    StrSlot dateStr{};
    RT_EXPORT_ONCE(SubStringFn, "?DateAsString@DateTime@System@@QEBA?AVString@2@XZ")(self, &dateStr);
    StrSlot timeStr{};
    RT_EXPORT_ONCE(SubStringFn, "?TimeAsString@DateTime@System@@QEBA?AVString@2@XZ")(self, &timeStr);

    // RT_EXPORT_ONCE: String::Format is ported (string.cpp), and bare RtOriginal is
    // GetProcAddress on every call as well as a port bypass.
    using FormatFn = StrSlot*(*)(StrSlot*, const char*, const char*, const char*);
    RT_EXPORT_ONCE(FormatFn, "?Format@String@System@@SA?AV12@PEBDZZ")(
        ret, "%s %s", static_cast<const char*>(dateStr.p), static_cast<const char*>(timeStr.p));

    using ReleaseFn = void(*)(StrSlot*);
    // RT_EXPORT_ONCE, for the same reason as GetVersionString below: String::Release
    // is PORTED (Wave 9 lane B3) and this is a live-path release of a temporary --
    // the WORK, not an oracle readback -- so it must reach our body.
    //
    // tools/port_bypass_audit.py could NOT see this one. Storing the pointer in a
    // local and calling it on the next line reads to the checker as
    // OK-RESOLVE-ONLY; only the resolve-and-call-in-one-expression site in
    // GetVersionString was reported. Two of the three bypasses this file had were
    // therefore invisible to the gate that caught the third.
    ReleaseFn release = RT_EXPORT_ONCE(ReleaseFn, "?Release@String@System@@AEAAXXZ");
    release(&dateStr);
    release(&timeStr);
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
static StrSlot* RtSystem_DateTime_FormatPart(const void* self, StrSlot* ret, bool timePart) {
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
    RtSystem::RtSystem_WideString_AsUTF8(&ws, reinterpret_cast<RtSystem::String*>(ret));
    RtSystem::RtSystem_WideString_Dtor(&ws);
    return ret;   // MOV RAX,RBX
}

StrSlot* RtSystem_DateTime_DateAsString(const void* self, StrSlot* ret) {
    return RtSystem_DateTime_FormatPart(self, ret, /*timePart=*/false);
}

StrSlot* RtSystem_DateTime_TimeAsString(const void* self, StrSlot* ret) {
    return RtSystem_DateTime_FormatPart(self, ret, /*timePart=*/true);
}

// ============================================================================
// WAVE 22 LANE SYSBODY — the DateAsString / TimeAsString sweep
// ============================================================================
// AN INPUT SWEEP, FOR THE SAME REASON THE COMPRESS ONE IS. Given a FILETIME
// these two are pure within a process: same locale, same clock, same answer.
// So they do not have to wait for a caller to be verified, and waiting would
// be a poor plan anyway — AsString (their only in-tree caller) is reached from
// save-file metadata, which an autostart run may or may not touch, and a
// `calls=0` row is not a verified row.
//
// WHAT IS COMPARED: the RESULTING STRING'S CHARACTERS, ours against the
// shipped export's, for the same FILETIME. Not the pointer (two calls
// necessarily allocate two buffers) and not the length alone.
//
// THE EFFECTIVENESS CONTROL, and it is the specific trap this row invites.
// A date formatter fed a narrow input range returns THE SAME STRING every
// time, and N identical agreeing answers is the uniform population that reads
// as a thorough test (reference_uniform_population_is_not_a_verification.md).
// A body that ignored `this` entirely and formatted the current time would
// pass such a sweep on the date half and fail nothing visibly. So the sweep
// spans FILETIMEs decades apart AND counts how many DISTINCT strings each side
// produced; fewer than half a dozen distinct values is reported as a failure
// of the sweep rather than a pass of the port.
//
// WHAT THIS DOES NOT CHECK, stated rather than left implied: the sweep runs
// under ONE locale (whatever the test machine has), so it cannot distinguish
// our GetDateFormatW flag `1` from any other flag that happens to render the
// same under that locale. The flags are read off the disassembly instead
// (0x1800c56fc `LEA EDX,[R9+0x1]`, 0x1800c560c `LEA EDX,[R9+0x2]`), which is a
// stronger source than a single-locale comparison would be.
namespace {

int RtDateTimeDistinct(char (*seen)[64], int seenCount, const char* s) {
    for (int i = 0; i < seenCount; ++i)
        if (std::strcmp(seen[i], s) == 0) return seenCount;
    if (seenCount < 32) {
        std::strncpy(seen[seenCount], s, 63);
        seen[seenCount][63] = '\0';
        ++seenCount;
    }
    return seenCount;
}

}  // namespace

void RtSystem_DateTimeProbe() {
    if (!RtDiffEnabled()) return;

    using PartFn = StrSlot* (*)(const void*, StrSlot*);
    using ReleaseFn = void (*)(StrSlot*);
    // RT_RAW_OK: ORACLE. RtOriginal is ResolveExportRaw, so these reach the
    // SHIPPED bodies even though ours are registered under the same names by
    // the time this runs. ResolveExport here would compare our body with
    // itself and the probe would be dark.
    PartFn theirDate = RtOriginal<PartFn>("?DateAsString@DateTime@System@@QEBA?AVString@2@XZ");
    PartFn theirTime = RtOriginal<PartFn>("?TimeAsString@DateTime@System@@QEBA?AVString@2@XZ");
    if (!theirDate || !theirTime) {
        printf("[SYSBODY/DateTime] sweep SKIPPED: no shipped exports (cutover build?)\n");
        return;
    }
    // The releases are the WORK of the probe, not an oracle readback, so they
    // go through the registry to our ported String::Release.
    ReleaseFn release = RT_EXPORT_ONCE(ReleaseFn, "?Release@String@System@@AEAAXXZ");

    long long cases = 0, dateDiff = 0, timeDiff = 0, uncomparable = 0;
    char seenDate[32][64]; int seenDateCount = 0;
    char seenTime[32][64]; int seenTimeCount = 0;

    // FILETIME is 100ns ticks since 1601-01-01. These span ~1601 to ~2100 and
    // sub-day steps, so both halves vary.
    static const unsigned long long kTicks[] = {
        0ULL,
        116444736000000000ULL,                       // 1970-01-01
        116444736000000000ULL + 36000000000ULL,      // +1h
        116444736000000000ULL + 863990000000ULL,     // +23:59:59
        128000000000000000ULL,                       // ~2006
        131000000000000000ULL,                       // ~2016
        131000000000000000ULL + 5000000000000ULL,
        150000000000000000ULL,                       // ~2076
        157000000000000000ULL,                       // ~2098
        131500000000000000ULL, 132000000000000000ULL, 132500000000000000ULL,
    };
    const int kN = static_cast<int>(sizeof(kTicks) / sizeof(kTicks[0]));

    for (int i = 0; i < kN; ++i) {
        FILETIME ft;
        ft.dwLowDateTime  = static_cast<DWORD>(kTicks[i] & 0xffffffffULL);
        ft.dwHighDateTime = static_cast<DWORD>(kTicks[i] >> 32);

        // Is this input COMPARABLE at all? Both bodies ignore the two BOOLs
        // and format from an UNINITIALISED stack buffer when the conversion
        // fails, so on a rejected FILETIME there is no deterministic answer on
        // EITHER side and a reported "divergence" would be two different
        // stacks, not a defect. The probe re-runs the conversions ITSELF to
        // decide, rather than inferring it from the answers -- a class derived
        // from the output would agree with any body by construction
        // (reference_recheck_must_match_its_own_proof.md). Both bodies are
        // still CALLED on these inputs, so "neither faults on a rejected
        // FILETIME" stays exercised; only the comparison is withheld, and the
        // count is printed rather than folded away.
        FILETIME probeLocal;
        SYSTEMTIME probeSt;
        const bool convertible =
            FileTimeToLocalFileTime(&ft, &probeLocal) != 0 &&
            FileTimeToSystemTime(&probeLocal, &probeSt) != 0;

        StrSlot ourD{}, theirD{}, ourT{}, theirT{};
        RtSystem_DateTime_DateAsString(&ft, &ourD);
        theirDate(&ft, &theirD);
        RtSystem_DateTime_TimeAsString(&ft, &ourT);
        theirTime(&ft, &theirT);
        ++cases;

        const char* od = static_cast<const char*>(ourD.p);
        const char* td = static_cast<const char*>(theirD.p);
        const char* ot = static_cast<const char*>(ourT.p);
        const char* tt = static_cast<const char*>(theirT.p);
        // Null on one side only is itself a divergence, so test that before
        // dereferencing either.
        const bool dOk = (od == nullptr) == (td == nullptr) &&
                         (od == nullptr || std::strcmp(od, td) == 0);
        const bool tOk = (ot == nullptr) == (tt == nullptr) &&
                         (ot == nullptr || std::strcmp(ot, tt) == 0);
        if (!convertible) {
            ++uncomparable;
            release(&ourD); release(&theirD); release(&ourT); release(&theirT);
            continue;
        }
        if (!dOk) {
            if (dateDiff < 6)
                printf("[SYSBODY/DateTime] DATE DIVERGENCE i=%d ours='%s' theirs='%s'\n",
                       i, od ? od : "(null)", td ? td : "(null)");
            ++dateDiff;
        }
        if (!tOk) {
            if (timeDiff < 6)
                printf("[SYSBODY/DateTime] TIME DIVERGENCE i=%d ours='%s' theirs='%s'\n",
                       i, ot ? ot : "(null)", tt ? tt : "(null)");
            ++timeDiff;
        }
        if (od) seenDateCount = RtDateTimeDistinct(seenDate, seenDateCount, od);
        if (ot) seenTimeCount = RtDateTimeDistinct(seenTime, seenTimeCount, ot);

        release(&ourD); release(&theirD); release(&ourT); release(&theirT);
    }

    printf("[SYSBODY/DateTime] sweep: %lld cases (%lld compared, %lld UNCOMPARABLE -- the "
           "Win32 conversion rejects the FILETIME and BOTH bodies then format from an "
           "uninitialised stack buffer), date divergences=%lld, time divergences=%lld\n",
           cases, cases - uncomparable, uncomparable, dateDiff, timeDiff);
    const bool varied = seenDateCount >= 6 && seenTimeCount >= 4;
    printf("[SYSBODY/DateTime] control: %d distinct dates, %d distinct times -- %s; %s\n",
           seenDateCount, seenTimeCount,
           varied ? "population NOT UNIFORM" : "*** UNIFORM: the sweep proves little ***",
           (dateDiff == 0 && timeDiff == 0 && varied) ? "VERIFIED"
                                                      : "*** NOT a verification ***");
}

// ?CurrentBuildId@Configuration@System@@SA?AVString@2@XZ  retail 0x1800c4c60
// Builds a 6-character stamp "<last 2 digits of year><2-letter month><day,
// zero-padded>" from the DLL's own compiled-in date string (e.g.
// "Sep 10 2017" -> "17Se10"), read via String::Set/Release so the DLL's own
// allocation is what we read characters out of, not a layout we assume.
StrSlot* RtSystem_Configuration_CurrentBuildId(StrSlot* ret) {
    static const char* const kMonths[12] = {
        "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
    };

    using SetFn = void(*)(StrSlot*, const char*);
    SetFn set = RT_EXPORT_ONCE(SetFn, "?Set@String@System@@QEAAXPEBD@Z");   // ported
    using ReleaseFn = void(*)(StrSlot*);
    // RT_EXPORT_ONCE, for the same reason as GetVersionString below: String::Release
    // is PORTED (Wave 9 lane B3) and this is a live-path release of a temporary --
    // the WORK, not an oracle readback -- so it must reach our body.
    //
    // tools/port_bypass_audit.py could NOT see this one. Storing the pointer in a
    // local and calling it on the next line reads to the checker as
    // OK-RESOLVE-ONLY; only the resolve-and-call-in-one-expression site in
    // GetVersionString was reported. Two of the three bypasses this file had were
    // therefore invisible to the gate that caught the third.
    ReleaseFn release = RT_EXPORT_ONCE(ReleaseFn, "?Release@String@System@@AEAAXXZ");

    StrSlot dateStr{};
    set(&dateStr, "Sep 10 2017");  // 0x180155c50 -- the DLL's own __DATE__-style literal
    const char* d = static_cast<const char*>(dateStr.p);  // "Sep 10 2017"

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

    set(ret, scratch);
    release(&dateStr);
    return ret;
}

// ?CurrentVersion@Configuration@System@@SA?AVString@2@XZ  retail 0x1800c4f40
StrSlot* RtSystem_Configuration_CurrentVersion(StrSlot* ret);  // fwd, defined below GetVersionString

// ?GetVersionString@Configuration@System@@SA?AVString@2@I_N@Z  retail 0x1800c4f90
// version is packed major<<24 | minor<<16 | patch<<8 | suffix. suffix in
// {0xfb..0xfe} appends a pre-release tag (short form when `detailed`, e.g.
// "R"/"P"/"A"/"B", else the full word); any other suffix (incl. retail's
// shipped 0xff) appends nothing. Text taken from the DLL's own .rdata
// literals at 0x180155c14..0x180155c40, transcribed to source for
// readability -- content verified byte-for-byte against inspect_memory.
StrSlot* RtSystem_Configuration_GetVersionString(StrSlot* ret, uint32_t version, bool detailed) {
    using FormatFn = StrSlot*(*)(StrSlot*, const char*, uint32_t, uint32_t, uint32_t);
    RT_EXPORT_ONCE(FormatFn, "?Format@String@System@@SA?AV12@PEBDZZ")(
        ret, "%i.%i.%i", version >> 24, (version >> 16) & 0xffu, (version >> 8) & 0xffu);

    const char* text = nullptr;
    switch (version & 0xffu) {
        case 0xfb: text = detailed ? "R" : " Prototype"; break;
        case 0xfc: text = detailed ? "P" : " Pre Alpha"; break;
        case 0xfd: text = detailed ? "A" : " Alpha"; break;
        case 0xfe: text = detailed ? "B" : " Beta"; break;
        default: return ret;
    }

    using OpAppendFn = StrSlot*(*)(StrSlot*, StrSlot*, const char*);
    using ReleaseFn  = void(*)(StrSlot*);
    StrSlot tmp{};
    RT_EXPORT_ONCE(OpAppendFn, "??YString@System@@QEAA?AV01@PEBD@Z")(ret, &tmp, text);   // ported
    // RT_EXPORT_ONCE, not RtOriginal: String::Release is PORTED (Wave 9 lane B3), and
    // RtOriginal deliberately skips the port registry so the diff oracle can reach the
    // shipped body. Using it here ran the DLL's Release while our port sat registered,
    // counted by the census and reported clean by the oracle -- invisible to both.
    // Caught by tools/port_bypass_audit.py, which is the only instrument that can see it.
    using ReleaseFn = void (*)(StrSlot*);
    RT_EXPORT_ONCE(ReleaseFn, "?Release@String@System@@AEAAXXZ")(&tmp);
    return ret;
}

StrSlot* RtSystem_Configuration_CurrentVersion(StrSlot* ret) {
    return RtSystem_Configuration_GetVersionString(ret, 0x010007ffu, false);
}

// ============================================================
// RtSystemValueRegister — called from RtSystemPortInit() under
// BANISHED_RT_VALUE (rt_system_port.cpp).
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
