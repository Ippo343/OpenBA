// ============================================================================
// runtime/src/audio/audio_platform_startup_win.cpp — Wave 9 lane PLAT
//   ?PlatformStartup@Device@Audio@@AEAA_NXZ   retail 0x1800de600  ord 3403
// ============================================================================
//
// The cutover boot wall AFTER Video's. Our ported `Engine::Startup` calls it at
// engine_lifecycle.cpp:730 and a null there ends the boot with
// `g_startupFailAt == 12`. `AEAA` is PRIVATE, not unexported; ordinal and
// address were read out of the shipped export directory with pefile before a
// line was written.
//
// WHAT IT ACTUALLY IS: XAudio2 2.7 BRING-UP
// -----------------------------------------
// Two GUIDs settle it, and both were read out of the shipped .rdata as raw
// bytes rather than recognised by eye:
//
//   0x180157cb8  5A508685-A254-4FBA-9B82-9A24B00306AF  CLSID_XAudio2
//   0x180157c98  8BCF1F58-9FE7-4583-8AC6-E2ADC465C8BB  IID_IXAudio2
//
// Those are the XAudio2 **2.7** (June 2010 DirectX SDK) identifiers, not the
// XAudio2_8/2_9 in-box ones. They are reproduced below as byte arrays copied
// from that dump, so nothing depends on a DirectX SDK header being installed.
//
// THE VTABLE SLOTS ARE COUNTED, NOT ASSUMED
// -----------------------------------------
// IXAudio2 (2.7) after IUnknown's three:
//   +0x18 GetDeviceCount   +0x20 GetDeviceDetails      +0x28 Initialize
//   +0x30 RegisterForCallbacks   +0x38 UnregisterForCallbacks
//   +0x40 CreateSourceVoice +0x48 CreateSubmixVoice    +0x50 CreateMasteringVoice
// The count is CHECKED by argument arity at both call sites rather than by
// memory: 0x1800502b4 calls +0x28 with two arguments after `this` (Initialize's
// Flags/XAudio2Processor), and 0x1800de65c calls +0x50 with SIX
// (CreateMasteringVoice's). A vtable missing `UnregisterForCallbacks` would put
// CreateMasteringVoice at +0x48 and make the second of those calls a
// CreateSubmixVoice with the wrong arity, so the two constraints agree only on
// this layout.
//
// THE DEVICE FIELDS
// -----------------
// Only two offsets of `Audio::Device` are touched, and they are the only two
// this file claims to know:
//   +0xa88  IXAudio2*               (`LEA RDI,[RCX+0xa88]` at 0x1800de60a)
//   +0xa90  IXAudio2MasteringVoice* (`LEA RSI,[RBX+0xa90]` at 0x1800de63a)
// `Engine::Startup` allocates the object as 0x1298 bytes (engine_lifecycle.cpp,
// the 0x1800dbdd4 block), which is the only other extent fact in evidence.
// runtime/types/audio/Audio.h has no `Device` struct and this lane does not
// invent one — an unproven layout that LOOKS authoritative is worse than raw
// offsets with the instruction beside them.
//
// IT ALWAYS RETURNS TRUE
// ----------------------
// Every failure path — CoCreateInstance failing, Initialize failing,
// CreateMasteringVoice failing — nulls the slots and returns TRUE. A machine
// with no XAudio2 2.7 redistributable boots silently without audio rather than
// failing `Engine::Startup`. That is retail's design and it is why this
// function is safe to port even where the component is absent.
// ============================================================================

#include <windows.h>
#include <objbase.h>

#include <cstdint>
#include <cstdio>
#include <cstring>

#include "../../funcs/rt_port.h"

namespace {

// Copied from the pefile dump of the shipped .rdata, byte for byte, so this
// file needs no DirectX SDK header and cannot pick up a DIFFERENT XAudio2.
//   8586505a 54a2 ba4f 9b82 9a24b00306af
const GUID kClsidXAudio2 =
    { 0x5A508685, 0xA254, 0x4FBA, { 0x9B, 0x82, 0x9A, 0x24, 0xB0, 0x03, 0x06, 0xAF } };
//   581fcf8b e79f 8345 8ac6 e2adc465c8bb
const GUID kIidIXAudio2 =
    { 0x8BCF1F58, 0x9FE7, 0x4583, { 0x8A, 0xC6, 0xE2, 0xAD, 0xC4, 0x65, 0xC8, 0xBB } };

constexpr size_t kOffXAudio2       = 0xa88;
constexpr size_t kOffMasteringVoice = 0xa90;

inline void** SlotAt(void* self, size_t off) {
    return reinterpret_cast<void**>(static_cast<uint8_t*>(self) + off);
}

inline void* const* VtblOf(void* obj) {
    return *reinterpret_cast<void* const* const*>(obj);
}

template <typename Fn>
inline Fn Slot(void* obj, size_t byteOffset) {
    return reinterpret_cast<Fn>(VtblOf(obj)[byteOffset / sizeof(void*)]);
}

bool EnvOn(const char* name) {
    const char* e = std::getenv(name);
    return e && e[0] && std::strcmp(e, "0") != 0 && std::strcmp(e, "false") != 0;
}

// ---------------------------------------------------------------------------
// 0x180050270 — the unexported helper. `CoCreateInstance` then `Initialize`,
// storing the interface into `*out` ONLY when both succeed.
// ---------------------------------------------------------------------------
// Three details from the disassembly:
//   * `TEST EAX,EAX ; JS` at 0x18005029f and again at 0x1800502c0 — the tests
//     are SIGN tests (FAILED), not equality tests. The CALLER's test is the
//     equality one.
//   * on the CoCreateInstance failure path `*out` is NOT written at all; on the
//     Initialize failure path the interface is Released and `*out` is still not
//     written. The caller does the nulling.
//   * the success path returns Initialize's HRESULT, not S_OK — so a
//     succeeded-with-info Initialize would make the CALLER take its failure
//     branch. Reproduced rather than normalised.
}  // namespace

extern "C" long RtAudio_CreateXAudio2(void** out) {
    void* obj = nullptr;
    const HRESULT hr = CoCreateInstance(kClsidXAudio2, nullptr, CLSCTX_INPROC_SERVER,
                                        kIidIXAudio2, &obj);
    if (hr < 0) return hr;                                 // 0x18005029f
    if (obj == nullptr) return hr;                         // 0x1800502a1 loads it back

    using InitFn = long (STDMETHODCALLTYPE*)(void*, unsigned, unsigned);
    const long hr2 = Slot<InitFn>(obj, 0x28)(obj, 0, 0xFFFFFFFFu);  // 0x1800502b4
    if (hr2 < 0) {                                          // 0x1800502c0
        using ReleaseFn = unsigned long (STDMETHODCALLTYPE*)(void*);
        Slot<ReleaseFn>(obj, 0x10)(obj);                    // 0x1800502d3
        return hr2;
    }
    *out = obj;                                             // 0x1800502c2
    return hr2;
}

namespace {
// (the anonymous namespace resumes; nothing below is exported by address)
}  // namespace

// ============================================================================
// ?PlatformStartup@Device@Audio@@AEAA_NXZ              retail 0x1800de600
// ============================================================================
extern "C" bool RtAudio_Device_PlatformStartup(void* self) {
    RT_DIFF_BEGIN("Audio::Device::PlatformStartup [oracle behind BANISHED_PLAT_ORACLE]");

    void** xaSlot = SlotAt(self, kOffXAudio2);
    void** mvSlot = SlotAt(self, kOffMasteringVoice);

    const long hr = RtAudio_CreateXAudio2(xaSlot);          // 0x1800de617
    if (hr != 0) {                                          // 0x1800de61c, EQUALITY test
        *xaSlot = nullptr;                                  // 0x1800de624
        // NOTE: the mastering-voice slot is NOT touched on this path. Retail
        // leaves whatever the ctor put there.
        if (rt_diff_on__) {
            RT_DIFF_SHADOW(true, "no XAudio2 (retail's silent-no-audio path)",
                           "not diffable on the live device");
            RT_DIFF_WORK(false);
        }
        return true;                                        // 0x1800de622 MOV AL,1
    }

    void* xa2 = *xaSlot;                                    // 0x1800de632, re-read
    using CreateMasteringVoiceFn = long (STDMETHODCALLTYPE*)(
        void* self, void** ppMasteringVoice, unsigned InputChannels,
        unsigned InputSampleRate, unsigned Flags, unsigned DeviceIndex,
        const void* pEffectChain);
    const long hrMv = Slot<CreateMasteringVoiceFn>(xa2, 0x50)(
        xa2, mvSlot, 0, 0, 0, 0, nullptr);                  // 0x1800de65c

    if (hrMv != 0) {                                        // 0x1800de65f, EQUALITY test
        using ReleaseFn = unsigned long (STDMETHODCALLTYPE*)(void*);
        Slot<ReleaseFn>(*xaSlot, 0x10)(*xaSlot);            // 0x1800de669, re-reads the slot
        *xaSlot = nullptr;                                  // 0x1800de66c
        *mvSlot = nullptr;                                  // 0x1800de66f
    }

    if (rt_diff_on__) {
        const bool haveEngine = *xaSlot != nullptr;
        const bool haveVoice  = *mvSlot != nullptr;
        if (!EnvOn("BANISHED_PLAT_ORACLE")) {
            char a[128];
            std::snprintf(a, sizeof(a), "engine=%d voice=%d hr=0x%lX",
                          haveEngine ? 1 : 0, haveVoice ? 1 : 0,
                          static_cast<unsigned long>(hrMv));
            RT_DIFF_SHADOW(true, a,
                           "no oracle: a replay creates a SECOND XAudio2 engine "
                           "and mastering voice on the real device");
            RT_DIFF_WORK(haveEngine);
        } else {
            // A real comparison, and a safe one: XAudio2 permits a second,
            // independent engine object, so the shipped body can be run on a
            // SCRATCH device buffer and its result compared. The scratch is
            // released afterwards; the two mastering voices belong to different
            // engines and do not contend.
            //
            // What is compared is the OUTCOME SHAPE — did the shipped body end
            // up with an engine, with a voice, and with the same return value —
            // not the pointers, which are two different COM objects and would
            // fail 100% of calls for the one reason that is correct.
            using Fn = bool (*)(void*);
            Fn theirsFn = RtOriginal<Fn>("?PlatformStartup@Device@Audio@@AEAA_NXZ");
            if (!theirsFn) {
                RT_DIFF_SHADOW(false, "ours ran", "oracle skipped: shipped body did not resolve");
                RT_DIFF_WORK(true);
            } else {
                static uint8_t scratch[0x1298];
                std::memset(scratch, 0, sizeof(scratch));
                const bool theirsRet = theirsFn(scratch);
                void** txa = SlotAt(scratch, kOffXAudio2);
                void** tmv = SlotAt(scratch, kOffMasteringVoice);
                const bool tEngine = *txa != nullptr;
                const bool tVoice  = *tmv != nullptr;

                const bool equal = (theirsRet == true) &&
                                   (haveEngine == tEngine) && (haveVoice == tVoice);
                char a[128], b[128];
                std::snprintf(a, sizeof(a), "ret=1 engine=%d voice=%d",
                              haveEngine ? 1 : 0, haveVoice ? 1 : 0);
                std::snprintf(b, sizeof(b), "ret=%d engine=%d voice=%d",
                              theirsRet ? 1 : 0, tEngine ? 1 : 0, tVoice ? 1 : 0);
                RT_DIFF_SHADOW(equal, a, b);
                // Effectiveness: an all-false outcome (no XAudio2 on this
                // machine) makes the three comparisons agree trivially.
                RT_DIFF_WORK(tEngine || haveEngine);
                std::printf("[PlatOracle] Audio::Device::PlatformStartup ours=%s theirs=%s\n", a, b);

                if (*txa) {
                    using ReleaseFn = unsigned long (STDMETHODCALLTYPE*)(void*);
                    Slot<ReleaseFn>(*txa, 0x10)(*txa);
                    *txa = nullptr;
                    *tmv = nullptr;
                }
            }
        }
    }
    return true;                                            // 0x1800de67c MOV AL,1
}

int RtAudioPlatformStartupRegister() {
    int n = 0;
    RegisterRuntimeExport("?PlatformStartup@Device@Audio@@AEAA_NXZ",   // ord 3403
                          (void*)&RtAudio_Device_PlatformStartup); ++n;
    RegisterRuntimeAddr(0x1800de600, (void*)&RtAudio_Device_PlatformStartup);
    // The unexported helper, reachable only by address. Registered because a
    // by-address call into the DLL dies at cutover unless something registers
    // that address (reference_by_address_door_dies_at_cutover.md).
    RegisterRuntimeAddr(0x180050270, (void*)&RtAudio_CreateXAudio2);
    return n;
}
