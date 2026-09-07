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
// through RtOriginal<>(), exactly as retail's own code does; we only ever
// read a String's PUBLIC content (its data pointer used as a plain
// null-terminated C string), never its private refcount/length header. The
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

#include <cstring>
#include <ctime>
#include "../../types/system/Value.h"
#include "../../funcs/rt_port.h"

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
// Delegates to Timer::SecondsToTicks (Timer is out of this lane's scope; its
// _secondsPerTick is a runtime QPC-derived global, not a constant we can
// hardcode). Not diffed: both "ours" and "theirs" would call the identical
// DLL export, so there is nothing independent to compare.
Time* RtSystem_Time_Ctor(Time* self, float seconds) {
    using SecondsToTicksFn = int64_t(*)(float);
    // RT_ORIGINAL_ONCE, not RtOriginal: this body runs on every Update(const
    // Time&). Resolving per call put a GetProcAddress under the loader lock on
    // the engine's hottest value type and cost 5.6 points of sim time -- most
    // of the Wave 2 perf-gate breach, and invisible to both profilers because
    // it lands in ntdll self time. See docs/runtime_port/WAVE3_LANE_P.md.
    self->ticks = RT_ORIGINAL_ONCE(SecondsToTicksFn,
                                   "?SecondsToTicks@Timer@System@@SA_JM@Z")(seconds);
    return self;
}

// ?AsSeconds@Time@System@@QEBAMXZ  retail 0x180007900 — same delegation, same
// "nothing independent to compare" reasoning.
float RtSystem_Time_AsSeconds(const Time* self) {
    using TicksToSecondsFn = float(*)(int64_t);
    // Cached for the same reason as the ctor above.
    return RT_ORIGINAL_ONCE(TicksToSecondsFn,
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
StrSlot* RtSystem_DateTime_AsString(const void* self, StrSlot* ret) {
    using SubStringFn = StrSlot*(*)(const void*, StrSlot*);
    StrSlot dateStr{};
    RtOriginal<SubStringFn>("?DateAsString@DateTime@System@@QEBA?AVString@2@XZ")(self, &dateStr);
    StrSlot timeStr{};
    RtOriginal<SubStringFn>("?TimeAsString@DateTime@System@@QEBA?AVString@2@XZ")(self, &timeStr);

    using FormatFn = StrSlot*(*)(StrSlot*, const char*, const char*, const char*);
    RtOriginal<FormatFn>("?Format@String@System@@SA?AV12@PEBDZZ")(
        ret, "%s %s", static_cast<const char*>(dateStr.p), static_cast<const char*>(timeStr.p));

    using ReleaseFn = void(*)(StrSlot*);
    ReleaseFn release = RtOriginal<ReleaseFn>("?Release@String@System@@AEAAXXZ");
    release(&dateStr);
    release(&timeStr);
    return ret;
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
    SetFn set = RtOriginal<SetFn>("?Set@String@System@@QEAAXPEBD@Z");
    using ReleaseFn = void(*)(StrSlot*);
    ReleaseFn release = RtOriginal<ReleaseFn>("?Release@String@System@@AEAAXXZ");

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
    RtOriginal<FormatFn>("?Format@String@System@@SA?AV12@PEBDZZ")(
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
    StrSlot tmp{};
    RtOriginal<OpAppendFn>("??YString@System@@QEAA?AV01@PEBD@Z")(ret, &tmp, text);
    RtOriginal<void(*)(StrSlot*)>("?Release@String@System@@AEAAXXZ")(&tmp);
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

    RegisterRuntimeExport("?CurrentBuildId@Configuration@System@@SA?AVString@2@XZ", (void*)&RtSystem_Configuration_CurrentBuildId); ++n;
    RegisterRuntimeExport("?CurrentVersion@Configuration@System@@SA?AVString@2@XZ", (void*)&RtSystem_Configuration_CurrentVersion); ++n;
    RegisterRuntimeExport("?GetVersionString@Configuration@System@@SA?AVString@2@I_N@Z", (void*)&RtSystem_Configuration_GetVersionString); ++n;

    return n;
}
