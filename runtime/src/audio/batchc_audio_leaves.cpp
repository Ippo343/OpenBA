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
#include "../../types/audio/Device.h"
#include "../../types/audio/SoundBuffer.h"

// ENDGAME B.3.7 block B4: this file's private copy of `AudioChannel` and its
// `ChannelAt` / `FreeChannelPlatform` forward declaration are gone — the
// record is `Audio::Device::AudioChannel` (runtime/types/audio/Device.h) and
// the call is the member. The SoundBuffer offsets are the class's fields.

namespace {

using VoidThisFn = void (*)(void*);

// `?_singleton@Device@Audio@@0PEAV12@EA` @ 0x1801fe760 — ordinal 4805, an
// exported DATA symbol (carry-over 5: it stays a slot). Retail derefs it
// UNGUARDED (`MOV RCX,[0x1801fe760]`) and hands the result to
// OnSoundBufferDeleted as `this`; the null test below exists only because
// RT_DATA_SLOT can return a slot the mint has not filled.
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
// comparison. the diff oracle records how many calls matched a channel at all;
// the overwhelming majority of SoundBuffer deletions match none.
// ############################################################################
namespace Audio {

void Device::OnSoundBufferDeleted(const SoundBuffer* buffer) {
    for (int i = 0; i < 64; ++i) {              // `CMP EDI,0x40 ; JL` at 0x1800de154
        AudioChannel* ch = &_channels[i];
        if (ch->source != buffer) continue;
        if ((ch->flags & 1u) == 0) continue;

        ch->flags |= 2u;
        FreeChannelPlatform(i);

        Callback cb = ch->onComplete;           // read BEFORE the clears
        ch->flags &= 0xFFFFFFFCu;
        ch->source = nullptr;
        if (cb)
            cb(ch, ch->userData, ch->userValue);  // `this` is the CHANNEL
    }
}

}  // namespace Audio

extern "C" void RtAudio_Device_OnSoundBufferDeleted(void* selfv, const void* buffer) {
    reinterpret_cast<Audio::Device*>(selfv)->Audio::Device::OnSoundBufferDeleted(          // B.3.7 bridge
        static_cast<const Audio::SoundBuffer*>(buffer));
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
// Layout: `Audio::SoundBuffer` (runtime/types/audio/SoundBuffer.h, 0x80).
// The dtor stays a DOOR (the class's own `~SoundBuffer` is empty, B4b-2).
// ############################################################################
extern "C" void RtAudio_SoundBuffer_Dtor(void* selfv) {
    Audio::SoundBuffer* const self = reinterpret_cast<Audio::SoundBuffer*>(selfv);   // B.3.7 bridge

    *reinterpret_cast<void**>(self) = VtSoundBuffer();

    reinterpret_cast<Audio::Device*>(AudioDeviceSingleton())->OnSoundBufferDeleted(self);   // B.3.7 bridge

    if (self->_data != nullptr)
        MemFree(self->_data);                                // +0x38
    if (self->_wmaData != nullptr)
        MemFree(self->_wmaData);                             // +0x48

    // Descending, the mirror of the Serialize order. `~String` IS retail's
    // inlined release idiom (the slot keeps `str-8` on the last reference and
    // is nulled on a surviving one, runtime/types/system/String.h).
    self->_streamPath.~String();                             // +0x30
    self->_string28.~String();                               // +0x28

    *reinterpret_cast<void**>(self) = VtCompiledExternal();
    ExternalDtor(self);                                      // JMP 0x1800bc950
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
