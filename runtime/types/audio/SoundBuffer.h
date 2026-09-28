#pragma once
// ============================================================================
// runtime/types/audio/SoundBuffer.h — Audio::SoundBuffer, 0x80 bytes, 9 slots
// ============================================================================
//
// ENDGAME B.3 step 7, block B4. The anonymous-namespace `SoundBuffer` struct in
// runtime/src/audio/vtbatch7_soundbuffer_soundeffect.cpp and the raw `kSb*`
// offsets in audio_play_platform.cpp, vtbatch7b_stream_run_win.cpp and
// batchc_audio_leaves.cpp MOVE here under the retail name (B6-8).
//
//   sizeof 0x80   `MOV EDX,0x80` in its own slot-0 vector deleting destructor
//                 (0x18000bef0) — B37_SLOTS.md section 1.
//   parent        "Object::CompiledExternal" (`?_ParentTypeName@SoundBuffer@
//                 Audio@@` 0x18000b880, RULE B7-2) — the ctor stamps Base,
//                 External, CompiledExternal, then SoundBuffer.
//
// `??_7SoundBuffer@Audio@@6B@` @ 0x1801549e0, NINE slots:
//   [0] +0x00 0x18000bef0  (unexported) vector deleting dtor — RT_DELDTOR row
//   [1] +0x08 0x18000b8b0  ?TypeName@SoundBuffer@Audio@@UEBAPEBDXZ
//   [2] +0x10 0x18000b890  ?Type@SoundBuffer@Audio@@UEBAIXZ
//   [3] +0x18 0x1800e0470  ?GetFileVersion@SoundBuffer@Audio@@UEBAHXZ (MOV EAX,0xa)
//   [4] +0x20 0x1800066e0  ?Compile@External@Object@@ — INHERITED (B7-1.7)
//   [5] +0x28 0x1800e0480  ?Serialize@SoundBuffer@Audio@@UEAA_NAEAVStream@IO@@@Z
//   [6] +0x30 0x18000bb30  ?Reconstruct@SoundBuffer@Audio@@UEAAPEAVExternal@Object@@XZ
//   [7] +0x38 0x18000bc70  ?Destruct@SoundBuffer@Audio@@UEAAXXZ
//   [8] +0x40 0x18000bbd0  ?Construct@SoundBuffer@Audio@@UEAAPEAVExternal@Object@@XZ
//                          — introduced here (B7-1.6)
//
// FIELDS, each named by the getter that returns it (read as bytes, retail
// 0x18000bc80..0x18000bcf0, one instruction each):
//   +0x20 u8     IsLooping    `MOVZX EAX,byte [RCX+0x20]`
//   +0x21 u8     IsStreamed   `MOVZX EAX,byte [RCX+0x21]`
//   +0x24 float  the loop/play duration in SECONDS (PlayPlatform divides it by
//                _secondsPerTick; SoundEffect::Compile adds it to a start time)
//   +0x28 String serialized first of the two; no getter
//   +0x30 String GetStreamPath `LEA RAX,[RCX+0x30]` (Stream::Run opens it)
//   +0x38 void*  GetData      `MOV RAX,[RCX+0x38]`
//   +0x40 int32  GetLength    `MOV EAX,[RCX+0x40]`
//   +0x48 void*  GetWMAData   `MOV RAX,[RCX+0x48]` — the uint32 cumulative-
//                             byte (xWMA seek) table
//   +0x50 int32  GetWMALength `MOV EAX,[RCX+0x50]` — that table's BYTE length
//   +0x54 WAVEFORMATEX Format `LEA RAX,[RCX+0x54]`; Serialize streams 0x28 of
//                the 0x2c bytes up to the end of the object
//
// ENDGAME B.3 step 9, block ENG (B39_RULES.md D1/D2): CONCRETE. Retail's real
// public constructor (??0SoundBuffer 0x1800e0320), its own slot 0, and the
// factory pair as its own statics: every live SoundBuffer wears the COMPILER's
// table. The minted row, its RT_DELDTOR row 0x18000bef0, the ??1 door and the
// eight slot doors are retired (tools/rt_mint_retired/eng.txt).
//
// THREADS (B7-7): owned by the main thread; the Stream worker reads +0x20,
// +0x30, +0x48 and +0x50 of the one being streamed.
//
// PORTABILITY: no <windows.h>.
// ============================================================================

#include <cstdint>
#include <cstddef>

#include "../object/External.h"
#include "../object/Internal.h"                // Object::detail::InternalMemFree
#include "../system/String.h"

namespace System { class Filename; class String; }
namespace IO     { class Serializer; }

namespace Audio {

class SoundBuffer : public Object::CompiledExternal {
public:
    // ??0SoundBuffer@Audio@@QEAA@XZ 0x1800e0320 -- the REAL constructor. Its
    // stores are the ones retail's factory pair inlines (0x18000ba03..a6a):
    // CompiledExternal() (Base's, External's and CompiledExternal's vptr and
    // External's zero stores), SoundBuffer's vptr (the compiler's), then a
    // WORD at +0x20, a dword at +0x24, the two Strings' null, 0x28 zero bytes
    // at +0x54 (the last four of `_format` and the pads at +0x22 / +0x44 /
    // +0x4c are NOT written), a qword +0x38, a dword +0x40, a qword +0x48 and a
    // dword +0x50.
    SoundBuffer() : Object::CompiledExternal(), _string28(), _streamPath() {
        _loopForever = 0;
        _streamed = 0;
        _durationSeconds = 0.0f;
        for (int i = 0; i < 0x28; ++i) _format[i] = 0;
        _data = nullptr;
        _length = 0;
        _wmaData = nullptr;
        _wmaLength = 0;
    }

    // ---- the nine slots, in measured order ----
    // [0] 0x18000bef0 -- MSVC's VECTOR deleting destructor (`TEST DL,2`,
    // `MOV EDX,0x80`, the eh-vector dtor over ~SoundBuffer 0x1800e03b0), both
    // arms as the retired RT_DELDTOR row ran them. Nothing `new[]`s one.
    Object::Base* DeletingDtor(unsigned flags) override {
        if (flags & 2u) {
            uint8_t* const self  = reinterpret_cast<uint8_t*>(this);
            const int32_t count  = *reinterpret_cast<const int32_t*>(self - 8);
            for (int32_t i = count; --i >= 0;)
                reinterpret_cast<SoundBuffer*>(self + static_cast<int64_t>(i) * 0x80)->~SoundBuffer();
            if (flags & 1u) Object::detail::InternalMemFree(self - 8);
            return reinterpret_cast<Object::Base*>(self - 8);
        }
        this->~SoundBuffer();
        if (flags & 1u) Object::detail::InternalMemFree(this);
        return this;
    }

    // [1] 0x18000b8b0 TypeName / [2] 0x18000b890 Type, the static TypeId
    // 0x18000b8a0, `_TypeName` 0x18000b870 / `_ParentTypeName` 0x18000b880 and
    // the factory pair 0x18000b9a0 / 0x18000ba90: retail's macro (ENDGAME B.3.9
    // TI-REST, B39_RULES D6); the record is `DEFINE_TYPE`d in
    // runtime/src/audio/audio_type_records.cpp.
    DECLARE_TYPE(SoundBuffer)
public:
    int32_t       GetFileVersion() const override;             // [3] 0x1800e0470
    // [4] Compile — inherited from Object::External (0x1800066e0), NOT declared.
    bool          Serialize(void* stream) override;            // [5] 0x1800e0480
    Object::External* Reconstruct() override;                  // [6] 0x18000bb30
    void          Destruct() override;                         // [7] 0x18000bc70
    virtual Object::External* Construct();                     // [8] 0x18000bbd0 — B7-1.6

    // ?PlatformSerialize@SoundBuffer@Audio@@AEAAXAEAVSerializer@IO@@@Z
    // 0x1800e05c0 (AEAA = private in retail; public here because its door
    // calls it, and access control is not ABI). `void*` is an IO::Serializer*.
    void PlatformSerialize(void* serializer);

    // ---- exported one-line accessors: faithful inline bodies, NOT REGISTERED
    bool          IsLooping() const   { return _loopForever != 0; }          // 0x18000bc80
    bool          IsStreamed() const  { return _streamed != 0; }             // 0x18000bc90
    const System::String& GetStreamPath() const { return _streamPath; }     // 0x18000bca0
    const void*   Format() const      { return _format; }                    // 0x18000bcb0  tWAVEFORMATEX const*
    int32_t       GetLength() const   { return _length; }                    // 0x18000bcc0
    const void*   GetData() const     { return _data; }                      // 0x18000bcd0
    const void*   GetWMAData() const  { return _wmaData; }                   // 0x18000bce0
    int32_t       GetWMALength() const { return _wmaLength; }                // 0x18000bcf0

    // ---- exported, NOT PORTED, no caller: declared, deliberately undefined --
    float GetLengthSeconds() const;                            // 0x1800e06c0
    SoundBuffer(const SoundBuffer& src);                       // 0x18000bd00
    SoundBuffer& operator=(const SoundBuffer& src);            // 0x18000bdc0
    static void* operator new(std::size_t bytes);              // 0x18000b8c0
    static void* operator new(std::size_t bytes, void* place); // 0x18000b930
    static void  operator delete(void* p);                     // 0x18000b940
    // Retail's factory pair (B39_RULES D2), defined in
    // vtbatch7_soundbuffer_soundeffect.cpp. The class declares its own
    // placement `operator new` (not ported): placement spells `::new (p)`.
    // (Declared by DECLARE_TYPE above.)

private:
    void PlatformConstruct();                                 // 0x1800e0550
    void PlatformDestruct();                                   // 0x1800e0580

protected:
    // `??1SoundBuffer@Audio@@UEAA@XZ` 0x1800e03b0. NON-VIRTUAL (no virtual dtor
    // until the normal-virtual cutover). batchc_audio_leaves.cpp holds retail's
    // body (the Device callback, the two buffer frees); the compiler releases
    // both Strings and chains ~CompiledExternal -> ~External, retail's order.
    ~SoundBuffer();

public:
    // ---- the 0x60 bytes SoundBuffer adds (public: the doors, the Device and
    // the Stream worker reach them) ----
    uint8_t         _loopForever;            // +0x20  IsLooping
    uint8_t         _streamed;               // +0x21  IsStreamed
    uint8_t         _pad22[2];               // +0x22  never written
    float           _durationSeconds;        // +0x24  kScalarFloat, last scalar
    System::String  _string28;               // +0x28  serialized first; no getter
    System::String  _streamPath;             // +0x30  GetStreamPath
    void*           _data;                   // +0x38  GetData
    int32_t         _length;                 // +0x40  GetLength
    uint8_t         _pad44[4];               // +0x44  never written
    uint32_t*       _wmaData;                // +0x48  GetWMAData
    int32_t         _wmaLength;              // +0x50  GetWMALength (BYTES)
    uint8_t         _format[0x2c];           // +0x54  Format: WAVEFORMATEX(+extra)
};

static_assert(sizeof(SoundBuffer) == 0x80,
              "Audio::SoundBuffer is 0x80 bytes (MOV EDX,0x80 in slot 0, 0x18000bef0)");
static_assert(alignof(SoundBuffer) == 8, "Audio::SoundBuffer aligns to 8 (the vptr)");
static_assert(offsetof(SoundBuffer, _loopForever)     == 0x20, "IsLooping 0x18000bc80 reads +0x20");
static_assert(offsetof(SoundBuffer, _streamed)        == 0x21, "IsStreamed 0x18000bc90 reads +0x21");
static_assert(offsetof(SoundBuffer, _durationSeconds) == 0x24, "PlayPlatform: MOVSS [buf+0x24]");
static_assert(offsetof(SoundBuffer, _string28)        == 0x28, "Serialize's first String");
static_assert(offsetof(SoundBuffer, _streamPath)      == 0x30, "GetStreamPath 0x18000bca0 is LEA +0x30");
static_assert(offsetof(SoundBuffer, _data)            == 0x38, "GetData 0x18000bcd0 reads +0x38");
static_assert(offsetof(SoundBuffer, _length)          == 0x40, "GetLength 0x18000bcc0 reads +0x40");
static_assert(offsetof(SoundBuffer, _wmaData)         == 0x48, "GetWMAData 0x18000bce0 reads +0x48");
static_assert(offsetof(SoundBuffer, _wmaLength)       == 0x50, "GetWMALength 0x18000bcf0 reads +0x50");
static_assert(offsetof(SoundBuffer, _format)          == 0x54, "Format 0x18000bcb0 is LEA +0x54");
static_assert(offsetof(SoundBuffer, _loopForever) == sizeof(Object::CompiledExternal),
              "SoundBuffer's own bytes begin right after Object::CompiledExternal");

}  // namespace Audio
