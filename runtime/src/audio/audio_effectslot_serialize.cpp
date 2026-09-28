// ============================================================================
// runtime/src/audio/audio_effectslot_serialize.cpp — BATCH 8b (2026-09-12)
//
//   0x180052640   the EffectSlot BULK element serialize      186 bytes
//   0x180051e00   EffectSlot::positions, the inner Array     148 bytes
//   0x180052330   Array<EmitterInstance>::SetSize(int)       644 bytes
//                 (ported in runtime/src/collection/array.cpp as
//                  kSpec_EmitterInstance — a Collection row, not an Audio one)
//
// All three are UNEXPORTED — measured against the retail export directory with
// pefile before this file was written, not read off an `AEAA`
// (reference_aeaa_is_private_not_unexported.md) — so all three are ADDRESS
// rows. Renamed in retail Ghidra as `Audio_Scene_SerializeEffectSlots`,
// `Audio_Scene_SerializeEmitterInstances` and
// `Collection_Array_EmitterInstance_SetSize`.
//
// ---------------------------------------------------------------------------
// WHY BATCH 8 DECLINED THEM, AND WHAT WAS ACTUALLY MISSING
// ---------------------------------------------------------------------------
// BATCH 8 held these back because they "need a new EffectSlot layout this batch
// did not reverse-engineer". The layout was already in the tree:
// `runtime/types/audio/Audio.h` has pinned `Audio::Scene::EffectSlot` (0x40,
// nine fields) and `Audio::EmitterInstance` (0x10, four floats) since Wave 8,
// from the emitter API and the array helpers.
//
// These two bodies are an INDEPENDENT WITNESS to that layout and they agree
// with it field for field. The serialize picks a ReaderWriter vtable slot per
// field, and each slot's width is fixed by `runtime/types/io/Serializer.h`'s
// `rw_vtbl` table, so the offsets AND the widths fall out of the instructions:
//
//   retail                       slot          field (Audio.h)
//   ------------------------------------------------------------------------
//   1800526_72  [rax+0x28] n=3   kArrayFloat   posX/posY/posZ   +0x00..+0x0b
//   1800526_83  [rax+0x98]       kScalarUInt   soundType        +0x0c
//   18005268f   CALL 0x180051790               soundEffectRef   +0x10 (8)
//   1800526a2   [rax+0x90]       kScalarInt64  startTime        +0x18
//   1800526b6   [rax+0x98]       kScalarUInt   idTag            +0x24
//   1800526ca   [rax+0x80]       kScalarFloat  baseVolume       +0x28
//   1800526d7   CALL 0x180051e00               positions        +0x30
//
// TWO FIELDS ARE DELIBERATELY NOT SERIALIZED and that is the interesting half:
//   * `channelHandle` (+0x20) — a live handle into the Channel table. A save
//     that restored it would point at a channel that is not playing.
//   * `_pad2c` (+0x2c) — the four bytes array.cpp:596 already records as NEVER
//     WRITTEN by either the default-init or the copy-assign arm. Three
//     unrelated bodies now agree that this window is dead; that agreement is
//     the point, and it is why an oracle must not compare it
//     (reference_oracle_must_not_compare_unwritten_bytes.md).
//
// And the inner element, from 0x180051e60/0x180051e75: kArrayFloat n=3 at
// +0x00 then kScalarFloat at +0x0c, stride 0x10 — `EmitterInstance{x,y,z,
// volume}` exactly. Audio.h only had `sizeof` asserted for it; this commit adds
// the four offset asserts, because these two bodies are what pins them.
//
// ---------------------------------------------------------------------------
// THE READ-SIDE RESIZE IS THE ONLY DIRECTION-DEPENDENT STEP
// ---------------------------------------------------------------------------
// 0x180051e2f: `MOV RAX,[RDI] / CMP dword [RAX+8],0 / JNE past` — the inner
// array is resized to the serialized count ONLY when `stream->direction == 0`
// (reading). On the write path the count goes out and the array is left alone,
// the same rule `Audio::Scene::Serialize` (audio.cpp) already follows for the
// outer effect array. `kDirectionReading` is 0 in Serializer.h.
//
// NOTE the count is re-read from the STACK SLOT after the resize
// (0x180051e41 `MOV EAX,[RSP+0x30]`), not from `arr->_size`, and the element
// walk uses `rbx = arr->_data` loaded at 0x180051e45 — i.e. AFTER the resize
// may have reallocated. Both are transcribed in that order.
//
// ---------------------------------------------------------------------------
// ORACLE — none of its own, for the reason audio.cpp gives for
// `Audio::Scene::Serialize`: a Serialize cannot be replayed into one stream
// twice. The WRITE path is a pure read of the object, so the existing
// save-stream A/B (the Wave 9 lane S emission oracle), which already covers
// `Audio::Scene::Serialize` and therefore covers this bulk pass underneath it,
// is the instrument. The emission oracle already named 0x180052640 as the body
// "the bulk element pass ... derives its own reader/writer from" — that note
// is corrected in this commit to say it is ours.
// ============================================================================

#include <cstdint>

#include "../../funcs/rt_port.h"
#include "../../types/audio/Audio.h"
#include "../../types/io/Serializer.h"
#include "../collection/array_internal.h"  // ENDGAME B.3.2 step 2: AsArray bridge

// `kSpec_EmitterInstance` (0x180052330, BATCH 8b) is defined in
// runtime/src/collection/array.cpp but was never added to array_internal.h's
// `extern` list (unlike its siblings) — declared locally rather than editing
// that shared header, which this batch does not own.
namespace rtcoll { extern const ArraySpec kSpec_EmitterInstance; }

using RtAudio::EffectSlot;
using RtAudio::EmitterInstance;

// 0x180051790 — `Object::ExternalPtr<Audio::SoundEffect>::Serialize`, ported
// and registered by runtime/src/object/external_ptr_serialize.cpp. Declared
// here rather than by address so the call cannot silently walk past our port
// (reference_resolveaddr_consults_port_registry_first.md is about the other
// direction; a direct call needs no resolver at all).
extern "C" void RtObject_ExternalPtrSerialize_SoundEffect(void** self,
                                                          rt::io::Serializer* ser);

namespace {

// A ReaderWriter member call, by name on the real class. Retail loads the
// ReaderWriter fresh from `ser->readerWriter` on EVERY field
// (`MOV RCX,[RDI+8] / MOV RAX,[RCX]`), so the reload is transcribed rather
// than hoisted. The `rt::io::ReaderWriter` POD and `IO::ReaderWriter` are
// asserted interchangeable (Serializer.h).
inline IO::ReaderWriter* Rw(rt::io::Serializer* ser) {
    return reinterpret_cast<IO::ReaderWriter*>(ser->readerWriter);   // B.3.x bridge
}

}  // namespace

// ============================================================================
// 0x180051e00 — serialize one `EffectSlot::positions`.
//   void (Collection::Array<EmitterInstance>* arr /*RCX*/,
//         IO::Serializer* ser /*RDX*/)
// ============================================================================
extern "C" void RtAudio_Scene_SerializeEmitterInstances(void* arrv, void* serv) {
    auto* arr = static_cast<Collection::Array<EmitterInstance>*>(arrv);
    auto* ser = static_cast<rt::io::Serializer*>(serv);

    // The count lives in a STACK SLOT for the whole body; `arr->GetSize()` is
    // read once, into it, and never again (0x180051e0a).
    int32_t count = arr->GetSize();

    Rw(ser)->SerializeInt(count, nullptr);                    // +0xa0

    // READING ONLY (0x180051e2f `CMP dword [stream+8],0 / JNE`).
    // ENDGAME B.3.2 step 2 — the 0x180052330 by-address forwarder is GONE:
    // `kSpec_EmitterInstance` zero-fills the whole 16-byte stride
    // (`InitZero10`), which a trivially-constructible `EmitterInstance`'s trait
    // row would leave UNINITIALISED, so it stays a hand-derived bridge rather
    // than a plain `arr->SetSize(count)`.
    if (ser->stream->direction == rt::io::kDirectionReading) {
        rtcoll::AsArray<&rtcoll::kSpec_EmitterInstance>(arr)->SetSize(count);  // 0x180051e3c
    }

    // `_data` is re-loaded HERE, after the resize may have reallocated.
    EmitterInstance* e = arr->GetData();  // 0x180051e45

    for (int32_t i = 0; i < count; ++i, ++e) {                 // 0x180051e53
        Rw(ser)->SerializeFloatArray(3, &e->x, nullptr);      // +0x28
        Rw(ser)->SerializeFloat(e->volume, nullptr);          // +0x80
    }
}

// ============================================================================
// 0x180052640 — the bulk element pass over `Audio::Scene::_effects`.
//   void (IO::Serializer* ser /*RCX*/, int32_t count /*EDX*/,
//         EffectSlot* data /*R8*/)
//
// `count <= 0` returns before the prologue (`TEST EDX,EDX / JLE ret`).
// ============================================================================
extern "C" void RtAudio_Scene_SerializeEffectSlots(void* serv, int32_t count,
                                                   void* datav) {
    if (count <= 0) return;                                    // 0x180052640

    auto* ser = static_cast<rt::io::Serializer*>(serv);
    EffectSlot* s = static_cast<EffectSlot*>(datav);

    do {
        Rw(ser)->SerializeFloatArray(3, &s->posX, nullptr);   // 0x180052672 +0x28
        Rw(ser)->SerializeUInt(s->soundType, nullptr);        // 0x180052683 +0x98

        // 0x18005268f — the ExternalPtr<SoundEffect> at +0x10, `(this, ser)`.
        RtObject_ExternalPtrSerialize_SoundEffect(
            reinterpret_cast<void**>(&s->soundEffectRef), ser);

        Rw(ser)->SerializeInt64(s->startTime, nullptr);       // 0x1800526a2 +0x90

        // +0x20 `channelHandle` is SKIPPED; see the banner.
        Rw(ser)->SerializeUInt(s->idTag, nullptr);            // 0x1800526b6 +0x98
        Rw(ser)->SerializeFloat(s->baseVolume, nullptr);      // 0x1800526ca +0x80

        // 0x1800526d7 — the inner array at +0x30, `(arr, ser)`.
        RtAudio_Scene_SerializeEmitterInstances(&s->positions, ser);

        ++s;                                                   // ADD RBX,0x40
    } while (--count != 0);                                    // DEC RSI / JNE
}
