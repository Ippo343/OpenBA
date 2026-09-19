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

// ============================================================================
// ENDGAME B.3.4b SYSTEM (3) — THE CLASSES ARE REAL; THE DOORS DELEGATE.
// ============================================================================
// `System::Random`, `System::Time`, `System::Color` and `System::DateTime` are
// real C++ classes since B.3.4b (runtime/types/system/Value.h). Unlike
// `Thread.h`, NO POD MOVED: `::Random` and `::Color` were already at global
// scope and `::Time` is `engine/types/common.h`'s, shared with the exe tree.
// Rule B4b-1 therefore does not fire here, and this commit touches nothing
// outside `runtime/`.
//
// NONE OF THE FOUR GETS A REAL CONSTRUCTOR — Value.h's banner gives the
// per-class reason, and for `Random` it is concrete: `Object::InternalFactory`
// holds one BY VALUE at +0x20 (runtime/types/object/Internal.h:662) while
// `Ctor_InternalFactory` (runtime/src/engine/singleton_ctors.cpp:352-361, not
// this step's file) seeds it through the `??0Random@System@@QEAA@XZ` door. A
// real constructor would seed the object factory's RNG twice, from two
// different `time(nullptr)` values, with no build error.
//
// MEMBER BODIES ARE DEFINED AT GLOBAL SCOPE with qualified names
// (`uint32_t System::Random::Rand() {...}`) rather than inside a
// `namespace System { ... }` block. Both are legal — the global namespace
// encloses `System` — and the qualified form is chosen DELIBERATELY: inside a
// `namespace System` block a bare `Random`, `Time` or `Color` binds the CLASS,
// not the POD, and in this file the two are layout-identical, so the wrong
// spelling would compile and behave correctly right up until one of them gains
// a member (B34_RULES §7.2). Keeping the block out removes the hazard instead
// of documenting it.
// ============================================================================

namespace {

// ---- Random: seeding, twist and core draw (retail algorithm, see Value.h) --
//
// B.3.4b: these three retyped from the POD `::Random*` to the class
// `System::Random*` (and `index`/`mt` to `_index`/`_mt`). Same bytes, same
// arithmetic, same loop bounds — the alternative was a `reinterpret_cast` at
// every one of the nine call sites below.

// Standard MT19937 init_genrand recurrence. Shared by both ctors.
// Pinned by ??0Random@System@@QEAA@I@Z (0x1800c7ed0).
void SeedMT(System::Random* self, uint32_t seed) {
    self->_index = 0;
    self->_mt[0] = seed;
    for (int i = 1; i < 624; ++i) {
        uint32_t prev = self->_mt[i - 1];
        self->_mt[i] = (prev ^ (prev >> 30)) * 0x6c078965u + static_cast<uint32_t>(i);
    }
}

// ?NextGroup@Random@System@@AEAAXXZ  retail 0x1800c7dd0 (private, not exported;
// no dispatch slot exists for it — it is only ever reached through Rand()).
// Standard MT19937 twist (UPPER_MASK/LOWER_MASK/MATRIX_A), confirmed against
// the disassembly instruction-by-instruction.
void NextGroupImpl(System::Random* self) {
    constexpr uint32_t kUpper = 0x80000000u, kLower = 0x7fffffffu, kMatrixA = 0x9908b0dfu;
    for (int i = 0; i < 624; ++i) {
        uint32_t y = (self->_mt[i] & kUpper) | (self->_mt[(i + 1) % 624] & kLower);
        uint32_t next = self->_mt[(i + 397) % 624] ^ (y >> 1);
        if (y & 1u) next ^= kMatrixA;
        self->_mt[i] = next;
    }
}

// Core of ?Rand@Random@System@@QEAAIXZ (0x1800c7f90), factored out so every
// overload below (which all draw one uint32 and post-process it) shares one
// transcription of the tempering constants instead of five copies.
uint32_t RandImpl(System::Random* self) {
    if (self->_index == 0) NextGroupImpl(self);
    uint32_t y = self->_mt[self->_index];
    y ^= y >> 11;
    y ^= (y & 0xff3a58adu) << 7;
    y ^= (y & 0xffffdf8cu) << 15;
    int next = self->_index + 1;
    self->_index = (next == 0x270) ? 0 : next;
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
// B.3.4b: NOT a real constructor, deliberately — see the banner at the top of
// this file and Value.h's. It keeps the free-function shape and the door's
// registered address.
System::Random* RtSystem_Random_Ctor(System::Random* self) {
    SeedMT(self, static_cast<uint32_t>(std::time(nullptr)));
    return self;
}

// ??0Random@System@@QEAA@I@Z  retail 0x1800c7ed0
// B.3.4b: NOT a real constructor either, same reason.
System::Random* RtSystem_Random_CtorSeed(System::Random* self, uint32_t seed) {
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
extern "C" void RtSystem_Random_NextGroup(System::Random* self) { self->NextGroup(); }

// ?Rand@Random@System@@QEAAIXZ  retail 0x1800c7f90
uint32_t RtSystem_Random_Rand(System::Random* self) { return self->Rand(); }

// ?Rand@Random@System@@QEAAII@Z  retail 0x1800c8040  (bound==0 short-circuits
// to 0 WITHOUT drawing — state must stay untouched, hence the ternary rather
// than an early `if`+call).
uint32_t RtSystem_Random_RandBound(System::Random* self, uint32_t bound) {
    return self->Rand(bound);
}

// ?Rand@Random@System@@QEAAHHH@Z  retail 0x1800c7d70
int32_t RtSystem_Random_RandRange(System::Random* self, int32_t lo, int32_t hi) {
    return self->Rand(lo, hi);
}

// ?RandFloat@Random@System@@QEAAMXZ  retail 0x1800c7d40
// DAT_18016165c = 1/2^32 exactly (read as bytes: 0x2f800000): maps a full
// 32-bit draw to [0,1).
namespace { constexpr float kRandUnit = 2.3283064365386963e-10f; }  // 0x2f800000
float RtSystem_Random_RandFloat(System::Random* self) {
    return self->RandFloat();
}

// ?RandFloatTol@Random@System@@QEAAMXZ  retail 0x1800c8000
float RtSystem_Random_RandFloatTol(System::Random* self) {
    return self->RandFloatTol();
}

// ?RandTol@Random@System@@QEAAHH@Z  retail 0x1800c7da0
int32_t RtSystem_Random_RandTol(System::Random* self, int32_t tol) {
    return self->RandTol(tol);
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
void RtSystem_Random_Serialize(System::Random* self, void* serializer) {
    self->Serialize(*reinterpret_cast<::IO::Serializer*>(serializer));
}

// ============================================================================
// ENDGAME B.3.4b — the `System::Random` member BODIES.
// ============================================================================
// Defined at GLOBAL scope with qualified names rather than inside a
// `namespace System { ... }` block; the file banner says why. Every body is the
// one that used to sit in the door directly above it.
//
// ONE THING DELETED, AND IT IS REPORTED RATHER THAN SMUGGLED. Six of these
// bodies opened with `Random before;` — a 2500-byte uninitialised stack local
// that was never written and never read, left over from the Wave-2 diff harness
// that compared our draw against the DLL's. It is dead in every build and its
// removal changes no instruction that touches `this`. It is called out here
// because a keyword tally of this commit shows `Random before;` going 6 -> 0
// and that delta must have a stated reason rather than being explained away as
// a rename.
// ============================================================================

uint32_t System::Random::Rand() {
    return RandImpl(this);
}

uint32_t System::Random::Rand(uint32_t bound) {
    // bound==0 short-circuits to 0 WITHOUT drawing — the state must stay
    // untouched, hence the ternary rather than an early `if` + call.
    return (bound == 0) ? 0u : (RandImpl(this) % bound);
}

int32_t System::Random::Rand(int32_t lo, int32_t hi) {
    uint32_t r = RandImpl(this);
    return lo + static_cast<int32_t>(r % (static_cast<uint32_t>(hi - lo) + 1u));
}

float System::Random::RandFloat() {
    return static_cast<float>(RandImpl(this)) * kRandUnit;
}

float System::Random::RandFloatTol() {
    // 1.0f - (u * (1/2^32)) * 2.0f, left-associated exactly as retail.
    return 1.0f - (static_cast<float>(RandImpl(this)) * kRandUnit) * 2.0f;
}

int32_t System::Random::RandTol(int32_t tol) {
    uint32_t r = RandImpl(this);
    return static_cast<int32_t>(r % (static_cast<uint32_t>(tol) * 2u + 1u)) - tol;
}

void System::Random::NextGroup() {
    NextGroupImpl(this);
}

void System::Random::Serialize(::IO::Serializer& s) {
    s.Serialize(*reinterpret_cast<unsigned int*>(&_index), nullptr);   // B.3.4 bridge  (0x98 = uint door)
    s.Serialize(0x270, reinterpret_cast<unsigned int*>(_mt), nullptr);   // B.3.4 bridge  (0x40 = uint-array door)
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
// B.3.4b: NOT a real constructor. Its ~132 call sites construct into RAW BYTE
// BUFFERS and convert as placement-new (B34_RULES section 7.3), which is
// caller-batch work gated behind the 4a closing pass; the constructor converts
// WITH them, not before. Value.h's banner restates this.
System::Time* RtSystem_Time_Ctor(System::Time* self, float seconds) {
    using SecondsToTicksFn = int64_t(*)(float);
    // Resolved ONCE per call site either way: this body runs on every
    // Update(const Time&), and resolving per call put a GetProcAddress under
    // the loader lock on the engine's hottest value type -- 5.6 points of sim
    // time, most of the Wave 2 perf-gate breach, and invisible to both
    // profilers because it lands in ntdll self time
    // (reference_rtoriginal_is_getprocaddress.md, WAVE3_LANE_P.md).
    self->_ticks = RT_EXPORT_ONCE(SecondsToTicksFn,
                                 "?SecondsToTicks@Timer@System@@SA_JM@Z")(seconds);
    return self;
}

// ?AsSeconds@Time@System@@QEBAMXZ  retail 0x180007900 — same delegation, same
// RT_EXPORT_ONCE reasoning as the ctor above.
float RtSystem_Time_AsSeconds(const System::Time* self) {
    return self->AsSeconds();
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
void RtSystem_Time_Serialize(System::Time* self, void* serializer) {
    self->Serialize(*reinterpret_cast<::IO::Serializer*>(serializer));
}

// ============================================================================
// ENDGAME B.3.4b — the `System::Time` member BODIES.
// ============================================================================

float System::Time::AsSeconds() const {
    using TicksToSecondsFn = float(*)(int64_t);
    return RT_EXPORT_ONCE(TicksToSecondsFn,
                          "?TicksToSeconds@Timer@System@@SAM_J@Z")(_ticks);
}

// ⚠ BYTE-IDENTICAL, DELIBERATELY. See the declaration's comment in Value.h and
// B34B_RULES section 3: `IO::Serializer` has no int64 scalar overload yet,
// retail DOES export one (0x180035000), and adding it is sequenced after the 4a
// closing pass because the member body belongs in a file another agent owns.
// The `+ 8` is the READERWRITER (the Stream is at Serializer+0x00) and +0x90 is
// `rw_vtbl::kScalarInt64`. Do NOT route these eight bytes through the four-byte
// int door.
void System::Time::Serialize(::IO::Serializer& s) {
    void* stream = *reinterpret_cast<void**>(reinterpret_cast<char*>(&s) + 8);
    void* vtbl = *reinterpret_cast<void**>(stream);
    using SerializeInt64Fn = void(*)(void*, void*, int);
    SerializeInt64Fn fn = *reinterpret_cast<SerializeInt64Fn*>(reinterpret_cast<char*>(vtbl) + 0x90);
    fn(stream, &_ticks, 0);
}

// ============================================================
// System::Color
// ============================================================

// ?Serialize@Color@System@@QEAAXAEAVSerializer@IO@@@Z  retail 0x1800c4150
// Slot +0x98 serializes the packed 4-byte `value` (same slot Random uses for
// its plain int). Not diffed (I/O side effect, see Random::Serialize above).
// ENDGAME B.3.4 callers batch H: `serializer + 8` is the READERWRITER, +0x98
// is rw_vtbl::kScalarUInt, and the trailing `0` was the ignored `name`.
void RtSystem_Color_Serialize(System::Color* self, void* serializer) {
    self->Serialize(*reinterpret_cast<::IO::Serializer*>(serializer));
}

// ENDGAME B.3.4b — the `System::Color` member BODY.
void System::Color::Serialize(::IO::Serializer& s) {
    s.Serialize(*reinterpret_cast<unsigned int*>(&_value), nullptr);   // B.3.4 bridge  (0x98 = uint door)
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
// ENDGAME B.3.4b: the DOOR keeps retail's two-argument hidden-retslot shape
// `String* f(const DateTime* this, String* ret)` and placement-news the member's
// prvalue into the caller's slot. The MEMBER returns by value, which is what
// retail's source said. Registering the member directly, or reaching one from a
// raw slot caller, is `reference_hidden_retslot_wild_write.md`.
System::String* RtSystem_DateTime_AsString(const void* self, System::String* ret) {
    new (ret) System::String(
        reinterpret_cast<const System::DateTime*>(self)->AsString());
    return ret;
}

System::String System::DateTime::AsString() const {
    using StringRetFn = System::String*(*)(const void*, System::String*);

    // ENDGAME B.3.4b: the two inner calls STAY on the registry doors. They are
    // what retail's own AsString does (it CALLs the two exported siblings) and
    // re-pointing them at the members would be caller work, not this commit's.
    // The doors below now delegate to those same members anyway, so the route
    // is one indirection longer and one body deep, exactly as before.
    //
    // ENDGAME B.3.1 step 2. THE DECLARATION ORDER IS DELIBERATELY THE REVERSE
    // OF THE FILL ORDER: retail releases dateStr FIRST and timeStr second
    // (0x1800c5546 / 0x1800c5555), and reverse-order destruction of these two
    // reproduces that. They are separate one-reference allocations, so both
    // are freed here and the ORDER of the two MemFree calls is visible to the
    // allocator's free list — which is why it is matched rather than assumed
    // not to matter.
    System::String timeStr;
    System::String dateStr;
    RT_EXPORT_ONCE(StringRetFn, "?DateAsString@DateTime@System@@QEBA?AVString@2@XZ")(this, &dateStr);
    RT_EXPORT_ONCE(StringRetFn, "?TimeAsString@DateTime@System@@QEBA?AVString@2@XZ")(this, &timeStr);

    // ?Format@String@System@@SA?AV12@PEBDZZ (0x1800c9730), spelled as itself.
    //
    // B.3.4b: this was `new (ret) String(Format(...))` writing the caller's
    // hidden slot directly; it is now a `return` of the same prvalue, and the
    // DOOR does the placement-new. THE STRING TRAFFIC IS UNCHANGED: under
    // C++17 a prvalue initialises the object being created, so `Format`'s
    // result constructs straight into this function's return object and that
    // return object constructs straight into the door's `ret`. No temporary, no
    // extra AddRef/Release pair retail did not spend — the same guarantee the
    // old comment relied on, one frame further out.
    return System::String::Format("%s %s", dateStr._data, timeStr._data);

    // The two `?Release@String@System@@AEAAXXZ` calls are the destructors now.
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
// B.3.4b: still a file-static helper over the retslot, and still shared by the
// two members below, for the reason its own banner gives — two copies of a
// 20-line body is how the second one drifts. `self` is now typed.
static System::String* RtSystem_DateTime_FormatPart(const System::DateTime* self,
                                                    System::String* ret,
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
    // B.3.4b: `self` is a `System::DateTime*` now instead of a `void*`, and the
    // cast is the one place in the tree that spells the class as the FILETIME
    // it is. The header keeps <windows.h> out and splits the two DWORDs; this
    // file is the platform-side half that may put them back together.
    static_assert(sizeof(System::DateTime) == sizeof(FILETIME),
                  "System::DateTime IS a FILETIME -- Value.h's layout pin, "
                  "restated where the real type is actually visible");
    FileTimeToLocalFileTime(reinterpret_cast<const FILETIME*>(self), &local);
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

// The two doors keep retail's hidden-retslot shape and delegate to the members.
System::String* RtSystem_DateTime_DateAsString(const void* self, System::String* ret) {
    new (ret) System::String(
        reinterpret_cast<const System::DateTime*>(self)->DateAsString());
    return ret;
}

System::String* RtSystem_DateTime_TimeAsString(const void* self, System::String* ret) {
    new (ret) System::String(
        reinterpret_cast<const System::DateTime*>(self)->TimeAsString());
    return ret;
}

// ============================================================================
// ENDGAME B.3.4b — the two `System::DateTime` member BODIES.
// ============================================================================
// They return `System::String` BY VALUE, which is what retail's source said;
// the helper above still fills a slot, so each member hands it a local and
// returns that.
//
// THE STRING TRAFFIC, STATED PRECISELY RATHER THAN OPTIMISTICALLY. The door's
// `new (ret) String(member())` is guaranteed by C++17 to build the member's
// return object AT `ret` — that part is the standard, not a hope. The member's
// local into its own return object is NRVO, which is permitted but NOT
// guaranteed; if the compiler declines it, the cost is exactly one MOVE
// construction of a `System::String`, which steals the pointer and leaves a
// null behind, so the moved-from local's destructor is a no-op and no refcount
// moves. Either way the allocator sees the same one allocation retail's body
// made. The local is default-constructed (data == nullptr) before
// `FormatPart` placement-constructs over it, which releases nothing because an
// empty String owns nothing.
System::String System::DateTime::DateAsString() const {
    System::String ret;
    RtSystem_DateTime_FormatPart(this, &ret, /*timePart=*/false);
    return ret;
}

System::String System::DateTime::TimeAsString() const {
    System::String ret;
    RtSystem_DateTime_FormatPart(this, &ret, /*timePart=*/true);
    return ret;
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
