// ============================================================================
// runtime/src/audio/batchc_audio_leaves.cpp — BATCH C
//   ??1SoundBuffer@Audio@@UEAA@XZ                   retail 0x1800e03b0  ord 679
//   ?OnSoundBufferDeleted@Device@Audio@@QEAAXPEBVSoundBuffer@2@@Z
//                                                   retail 0x1800de0e0  ord 3282
// ============================================================================
//
// The two halves of "a sound buffer goes away": the destructor, and the device
// callback it opens with. `??1SoundBuffer@` was a row in
// runtime/src/object/deleting_dtor.cpp's table (0x18000bef0 -> 0x1800e03b0,
// elemSize 0x80) reaching the DLL's body; that row now lands here, which is the
// intent (ResolveAddr is registry-first).
//
// Both read from the shipped retail DLL's DISASSEMBLY. Both export names read
// back out of the export directory by VA with pefile.
//
// ---------------------------------------------------------------------------
// THE CHANNEL TABLE IS NOT RE-DERIVED HERE
// ---------------------------------------------------------------------------
// `Audio::Device::AudioChannel` — 0x28 bytes, 64 of them at Device+0x00 — was
// pinned by runtime/src/audio/audio_device.cpp (Wave 9 lane IOA) from THREE
// independent functions: `Device::Update`'s loop, `Device::Stop`'s
// pointer-to-index division by 0x28, and `Device::StopPlatform`. This file
// reproduces that layout locally (it must, to static_assert it) and its own
// reap epilogue is the SAME five instructions those two carry — which is the
// cross-check that the reading is right, not a coincidence.
//
// ---------------------------------------------------------------------------
// TWO THINGS THE DECOMPILER WOULD GET WRONG
// ---------------------------------------------------------------------------
// 1. `OnSoundBufferDeleted`'s completion callback is invoked with the CHANNEL
//    RECORD as `this` (RSI, which walks in step with the loop), NOT with the
//    Device. At channel 0 the two coincide, which is exactly how a wrong
//    reading would survive testing.
// 2. The reap order is load-bearing and matches `Device::Update` exactly:
//    `onComplete` is read BEFORE the flag/source clears and the callback's 2nd
//    and 3rd arguments AFTER them; the `&= ~3` clears bits 0 AND 1 though only
//    bit 0 was tested here (Update tests bit 1).
// ============================================================================

#include <cstddef>
#include <cstdint>

#include "../../funcs/rt_port.h"

enum class Module : int { Exe = 0, Runtime = 1, VideoDX11 = 2, COUNT = 3 };
uintptr_t ResolveAddr(Module mod, uintptr_t ghidra_addr);

// runtime/src/audio/audio_device.cpp (Wave 9 lane IOA). Declared rather than
// re-derived: there is ONE FreeChannelPlatform body in this tree and this file
// must reach it, not a copy of it.
// (C++ linkage, matching its definition — audio_device.cpp does not declare it
// extern "C"; the linker checks the two spellings agree.)
void RtAudio_Device_FreeChannelPlatform(void* self, int index);

namespace {

// Mirrors runtime/src/audio/audio_device.cpp's AudioChannel exactly; the two
// are checked against each other by these static_asserts and by the shared
// FreeChannelPlatform they both index.
struct AudioChannel {
    uint32_t flags;       // +0x00  bit 0 = "live"; bit 1 = "finished, reap me"
    uint32_t _pad04;      // +0x04
    void*    source;      // +0x08  the Audio::SoundBuffer this channel plays
    void*    userData;    // +0x10  2nd argument to the completion callback
    uint32_t userValue;   // +0x18  3rd argument (a DWORD, read as R8D)
    uint32_t _pad1c;      // +0x1c
    void*    onComplete;  // +0x20  the stored completion callback
};
static_assert(sizeof(AudioChannel) == 0x28, "Audio::Device::AudioChannel must be 0x28");
static_assert(offsetof(AudioChannel, source)     == 0x08, "AudioChannel::source @ +0x08");
static_assert(offsetof(AudioChannel, userData)   == 0x10, "AudioChannel::userData @ +0x10");
static_assert(offsetof(AudioChannel, userValue)  == 0x18, "AudioChannel::userValue @ +0x18");
static_assert(offsetof(AudioChannel, onComplete) == 0x20, "AudioChannel::onComplete @ +0x20");

constexpr int kChannelCount  = 0x40;   // `CMP EDI,0x40 ; JL` at 0x1800de154
constexpr int kChannelStride = 0x28;

using AudioCompletionFn = void (*)(void* channel, void* userData, uint32_t userValue);

inline AudioChannel* ChannelAt(void* self, int index) {
    return reinterpret_cast<AudioChannel*>(
        static_cast<uint8_t*>(self) + static_cast<int64_t>(index) * kChannelStride);
}

using VoidThisFn = void (*)(void*);

// `?_singleton@Device@Audio@@0PEAV12@EA` @ 0x1801fe760 — ordinal 4805, an
// exported DATA symbol. Retail derefs it UNGUARDED (`MOV RCX,[0x1801fe760]`)
// and hands the result to OnSoundBufferDeleted as `this`; the null test below
// exists only because RT_DATA_SLOT can return a slot the mint has not filled,
// which retail's own static could not be.
void* AudioDeviceSingleton() {
    void** slot = RT_DATA_SLOT(void**, "?_singleton@Device@Audio@@0PEAV12@EA");
    return slot ? *slot : nullptr;
}

void* VtSoundBuffer()        { return RT_DATA_SLOT(void*, "??_7SoundBuffer@Audio@@6B@"); }
void* VtCompiledExternal()   { return RT_DATA_SLOT(void*, "??_7CompiledExternal@Object@@6B@"); }

// `??1External@Object@@UEAA@XZ` 0x1800bc950 — PORTED and registered by both
// doors (object/external.cpp). ~SoundBuffer tail-jumps it.
void ExternalDtor(void* p) { RT_EXPORT_ONCE(VoidThisFn, "??1External@Object@@UEAA@XZ")(p); }

// `?MemFree@MemoryAllocator@System@@SAXPEAX@Z` 0x1800c7ad0 — PORTED. Retail's
// four call sites here test for null OUTSIDE the call; MemFree tests again
// inside, so the behaviour is identical.
void MemFree(void* p) { RT_EXPORT_ONCE(VoidThisFn, "?MemFree@MemoryAllocator@System@@SAXPEAX@Z")(p); }

// The System::String release idiom, spelled the way
// vtbatch7b_stream_run_win.cpp's ReleaseString already spells it: on the LAST
// reference the slot is left holding `str-8` (dangling, deliberately — that is
// `ADD qword [slot],-8` at 0x1800e040e/0x1800e0438) and only a SURVIVING
// reference nulls it.
void ReleaseString(void** slot) {
    uint8_t* s = static_cast<uint8_t*>(*slot);
    if (s == nullptr) return;
    int32_t* rc = reinterpret_cast<int32_t*>(s - 4);
    --(*rc);
    if (*rc != 0) { *slot = nullptr; return; }
    *slot = s - 8;
    MemFree(*slot);
}

}  // namespace

// ############################################################################
// Audio::Device::OnSoundBufferDeleted(const SoundBuffer*)
// retail 0x1800de0e0, ordinal 3282
// ############################################################################
//
//   for (i = 0; i < 0x40; ++i) {
//       ch = this + i*0x28;
//       if (ch->source != buffer) continue;
//       if ((ch->flags & 1) == 0) continue;        // not a live playback
//       ch->flags |= 2;                            // mark it finished ...
//       FreeChannelPlatform(this, i);              // ... then release the voice
//       cb = ch->onComplete;                       // read BEFORE the clears
//       ch->flags &= ~3;
//       ch->source = nullptr;
//       if (cb) cb(ch, ch->userData, ch->userValue);   // `this` is the CHANNEL
//   }
//
// ORACLE: NONE, for `Device::Update`'s reason — this fires stored completion
// callbacks and releases a COM voice. A replay is a second side effect, not a
// comparison. RT_DIFF_WORK records how many calls matched a channel at all;
// the overwhelming majority of SoundBuffer deletions match none.
// ############################################################################
extern "C" void RtAudio_Device_OnSoundBufferDeleted(void* selfv, const void* buffer) {
    RT_DIFF_BEGIN("Audio::Device::OnSoundBufferDeleted");

    int matched = 0;
    for (int i = 0; i < kChannelCount; ++i) {
        AudioChannel* ch = ChannelAt(selfv, i);
        if (ch->source != buffer) continue;
        if ((ch->flags & 1u) == 0) continue;
        ++matched;

        ch->flags |= 2u;
        RtAudio_Device_FreeChannelPlatform(selfv, i);

        void* cb = ch->onComplete;
        ch->flags &= 0xFFFFFFFCu;
        ch->source = nullptr;
        if (cb)
            reinterpret_cast<AudioCompletionFn>(cb)(ch, ch->userData, ch->userValue);
    }

    RT_DIFF_WORK(matched != 0);
    RT_DIFF_SHADOW(true, "every channel playing this buffer stopped",
                   "not diffable (fires stored completion callbacks)");
}

// ############################################################################
// Audio::SoundBuffer::~SoundBuffer
// ??1SoundBuffer@Audio@@UEAA@XZ                 retail 0x1800e03b0, ordinal 679
// ############################################################################
//
//   this->vtable = ??_7SoundBuffer@Audio@@6B@
//   Device::OnSoundBufferDeleted(Audio::Device::_singleton, this)
//   MemFree(this->_audioData)      +0x38
//   MemFree(this->_segmentTable)   +0x48
//   ReleaseString(&this->_str2)    +0x30     <-- descending, the mirror of the
//   ReleaseString(&this->_str1)    +0x28         Serialize order
//   this->vtable = ??_7CompiledExternal@Object@@6B@
//   tail ~External(this)
//
// THE DEVICE CALL COMES FIRST, before a single byte is freed: a channel still
// playing this buffer must be stopped while `ch->source == this` still matches.
// Moving it after the frees would leave the mixer reading freed audio.
//
// Layout: the `SoundBuffer` in vtbatch7_soundbuffer_soundeffect.cpp (0x80
// bytes). Offsets are used numerically here rather than through that struct,
// which is file-local to its owner; the four this body touches are asserted
// against that file's own static_asserts by construction — they are the same
// four numbers, and any drift shows as a diff on this comment, not silently.
// ############################################################################
extern "C" void RtAudio_SoundBuffer_Dtor(void* selfv) {
    RT_DIFF_BEGIN("Audio::SoundBuffer::~SoundBuffer");
    uint8_t* const self = static_cast<uint8_t*>(selfv);

    *reinterpret_cast<void**>(self) = VtSoundBuffer();

    RtAudio_Device_OnSoundBufferDeleted(AudioDeviceSingleton(), self);

    if (*reinterpret_cast<void**>(self + 0x38) != nullptr)
        MemFree(*reinterpret_cast<void**>(self + 0x38));    // _audioData
    if (*reinterpret_cast<void**>(self + 0x48) != nullptr)
        MemFree(*reinterpret_cast<void**>(self + 0x48));    // _segmentTable

    ReleaseString(reinterpret_cast<void**>(self + 0x30));   // _str2
    ReleaseString(reinterpret_cast<void**>(self + 0x28));   // _str1

    *reinterpret_cast<void**>(self) = VtCompiledExternal();
    ExternalDtor(self);                                     // JMP 0x1800bc950

    RT_DIFF_SHADOW(true, "sound buffer torn down", "not diffable (frees the object)");
}

// ============================================================================
// Registration — BOTH DOORS, unconditionally.
//
// 0x1800e03b0 and 0x1800de0e0 were grepped across code/, engine/, runtime/,
// standalone/ and dx11/ first. 0x1800e03b0 appears only as a RT_DELDTOR row in
// runtime/src/object/deleting_dtor.cpp, which reaches it through ResolveAddr
// (registry-first) and therefore now lands on the body above — the intent.
// 0x1800de0e0 appears nowhere at all. Neither is an identity token.
// ============================================================================
extern "C" int RtAudioBatchCLeavesRegister() {
    int n = 0;

    RegisterRuntimeExport("??1SoundBuffer@Audio@@UEAA@XZ",
                          (void*)&RtAudio_SoundBuffer_Dtor); ++n;
    RegisterRuntimeAddr(0x1800e03b0ull, (void*)&RtAudio_SoundBuffer_Dtor);

    RegisterRuntimeExport("?OnSoundBufferDeleted@Device@Audio@@QEAAXPEBVSoundBuffer@2@@Z",
                          (void*)&RtAudio_Device_OnSoundBufferDeleted); ++n;
    RegisterRuntimeAddr(0x1800de0e0ull, (void*)&RtAudio_Device_OnSoundBufferDeleted);

    return n;
}
