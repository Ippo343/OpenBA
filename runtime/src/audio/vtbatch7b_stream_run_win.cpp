// ============================================================================
// runtime/src/audio/vtbatch7b_stream_run_win.cpp — VTABLE BATCH 7b
//   ?Run@Stream@Audio@@UEAAXXZ            retail 0x1800a0f60  ord 3602
// ============================================================================
//
// The real-time xWMA streaming decode loop: the body of the worker thread
// `Audio::Stream`'s base `System::Thread` starts. `Audio::Stream::Play`
// (audio_stream_play_win.cpp, already ours) is what wakes it; the ctor/dtor
// (audio_stream_win.cpp) are what create and join it.
//
// EXPORTED, checked against the retail export directory with pefile by RVA
// before anything was written (not taken from a worklist):
//     0x1800a0f60  ord 3602  ?Run@Stream@Audio@@UEAAXXZ
//
// HOW IT IS DISPATCHED. `System::Thread`'s vtable slot for `Run` is reached
// from the thread trampoline inside `??0Thread@System@@QEAA@I@Z`
// (runtime/src/system/thread_win.cpp), so registering BOTH doors matters: the
// EXPORT door serves a `ResolveExport` caller and the ADDRESS door serves the
// vtable slot, which is a raw address until the mint owns
// `??_7Stream@Audio@@6B@` (0x180154950 — audio_stream_win.cpp's standing
// HANDOVER, unchanged by this file).
//
// Its own `_win.cpp` for the reason both sibling files give (PLAN.md §4.1
// decision 1): it calls SetFilePointer, WaitForMultipleObjects and the two
// critical-section imports directly.
//
// ---------------------------------------------------------------------------
// WHAT THE BODY ACTUALLY IS — and every offset in it has a named writer
// ---------------------------------------------------------------------------
// Read from Ghidra's `disassemble_function` over 0x1800a0f60..0x1800a1667
// (Ghidra's C output drops arguments on indirect calls —
// reference_ghidra_c_output_vs_disassembly.md — and this body is almost
// entirely indirect calls).
//
//   dev       = *(Device**)0x1801fe760            ?_singleton@Device@Audio@@
//   slot      = self->_channel        (+0x7c, stamped by Stream::Play)
//   voiceRec  = dev + 0xa98 + slot*0x20           the embedded
//               IXAudio2VoiceCallback record (audio_voice_callback_win.cpp §2)
//   chan      = dev + slot*0x28                   Device::AudioChannel[slot]
//               (audio_device.cpp: 64 of them at Device+0x00, stride 0x28)
//   buf       = chan->source          (+0x08)     Audio::SoundBuffer*
//
// and from `buf`, the three fields this body reads are the three
// vtbatch7_soundbuffer_soundeffect.cpp already pins from that class's OWN ctor
// and Serialize:
//   +0x20 loopForever      +0x30 the asset path String     +0x48/+0x50 the
//   xWMA seek table and its BYTE length (hence `>> 2` for its entry count).
//
// THE STREAM BUFFER TABLE. `Stream::Play` grows `self+0x30` to FOUR elements of
// 0x8ba0 bytes (0x18000c300, audio_stream_play_win.cpp's `GrowStreamBuffers`).
// This body is what gives those numbers meaning, and the arithmetic closes
// exactly:
//     16 packets * 0x8b6 bytes  = 0x8b60   the audio window
//     16 packets *    4 bytes   =   0x40   the per-packet cumulative table
//                                 ------
//                                 0x8ba0   = the element stride
// so each slot is `[0x40 bytes of XAUDIO2_BUFFER_WMA seek table][0x8b60 bytes
// of xWMA packets]`, the audio starting at +0x40. 0x8ba0/4 == 0x22e8, which is
// the constant in retail's index arithmetic at 0x1800a1212.
//
// THE COM CALLS. Two, both on the `IXAudio2SourceVoice` at voiceRec+0x08:
//   +0xc8  GetState(&XAUDIO2_VOICE_STATE)
//   +0xa8  SubmitSourceBuffer(&XAUDIO2_BUFFER, &XAUDIO2_BUFFER_WMA)
// The indices are NOT guessed from a header this project does not carry: they
// are forced by the two already-measured anchors in this tree —
// audio_voice_callback_win.cpp reads `*0x90` on a voice as DestroyVoice
// (IXAudio2Voice index 18) and audio_scene_update.cpp reads `*0x60` as
// SetVolume (index 12). With IXAudio2Voice ending at +0x90, IXAudio2SourceVoice
// continues Start(+0x98) Stop(+0xa0) SubmitSourceBuffer(+0xa8)
// FlushSourceBuffers(+0xb0) Discontinuity(+0xb8) ExitLoop(+0xc0) GetState(+0xc8),
// and the ARGUMENT SHAPES agree: +0xa8 takes two pointers, +0xc8 takes one
// out-pointer whose +0x08 dword this body compares against 3 and 0 — i.e.
// XAUDIO2_VOICE_STATE::BuffersQueued.
//
// THE TWO EVENTS IT WAITS ON, in retail's order:
//   handles[0] = self->+0x48   signalled by the VoiceCallback's OnBufferEnd
//                              (audio_voice_callback_win.cpp slot 4) — "a slot
//                              freed up". This body is also the WRITER that
//                              publishes the handle into voiceRec+0x18
//                              (0x1800a114d), which is the fact that file's §2
//                              cites for that field.
//   handles[1] = self->+0x40   Stream::Play's "drop what you are doing" wake.
// `WaitForMultipleObjects(2, handles, FALSE, INFINITE)` returning NON-ZERO
// therefore means "the track changed", and every such return abandons the pass.
//
// ---------------------------------------------------------------------------
// FOUR THINGS THAT LOOK WRONG IN THE TRANSCRIPTION AND ARE NOT
// ---------------------------------------------------------------------------
//  1. `curSeg` (the starting packet) is the index of the FIRST cumulative entry
//     STRICTLY GREATER than `self->_bytePos` (+0x78), and it stays 0 when no
//     entry exceeds it — because retail only stores the counter on the `JA`
//     exit (0x1800a1009), never on loop fall-through.
//  2. `file.cursor = curSeg * 0x8b6` is written BEFORE the seek and on the path
//     that does NOT seek. That is deliberate, for exactly the reason
//     closure_batch2b_win.cpp states about the same pair: a PACKED file has no
//     Win32 handle, and `FileSystem::ReadFile` reads from `cursor`, so the
//     store IS the seek. The unsigned `handle > 0xfffffffd` test selects
//     handle == -1 or -2 (FileSystem.h's encoding note) — "not packed".
//  3. The buffer-slot index is kept TWICE, as an int at +0x68 and an int64 at
//     +0x78, and both are reset to 0 together when the int reaches 4. They are
//     always equal; retail keeps two because one feeds a 32-bit IMUL and the
//     other a 64-bit one. Written as two variables here so a reader comparing
//     against the listing finds both.
//  4. The END_OF_STREAM flag is set only when the LAST chunk of a NON-LOOPING
//     buffer is submitted, and the test is inverted in the listing
//     (`CMP byte[buf+0x20],0 ; MOV EDX,0x40 ; CMOVZ EAX,EDX`) — loopForever ==
//     0 selects 0x40.
//
// ---------------------------------------------------------------------------
// EVERY CALLEE, AND ITS DOOR. All six are ALREADY OURS; this port opens no new
// address door at all.
// ---------------------------------------------------------------------------
//   0x1800aaa10  ??0FileStream@IO@@QEAA@XZ              file_stream.cpp
//   0x1800aac70  ?Open@FileStream@IO@@                  file_stream.cpp
//   0x180007bd0  ?GetDataDirectory@FileSystem@IO@@      filesystem.cpp
//   0x1800c9440  ??HSystem@@ (String operator+)         string.cpp
//   0x1800c6970  ?AdjustDelimiters@Filename@System@@    (registered)
//   0x1800ace00  ?ReadFile@FileSystem@IO@@              filesystem_openfile_win.cpp
//   0x1800accc0  ?CloseFile@FileSystem@IO@@             filesystem_createfile_win.cpp
//   0x1800aaad0  ??1FileStream@IO@@UEAA@XZ              file_stream.cpp
// Retail INLINES the FileStream ctor and dtor here (0x1800a100c..0x1800a106a
// and 0x1800a14b3..0x1800a1654); this port calls the exported bodies instead,
// the same deliberate departure closure_batch2b_win.cpp states for the same two
// functions — what is computed is identical, and a second hand copy of
// ~FileStream's String/FileInfo teardown in this tree is exactly what that
// choice avoids. Their inlined store lists were compared field for field first
// (FileStream.h's ctor note: +0x18 q, +0x20 d, +0x28 q, +0x30 d, +0x38 q(-1),
// +0x40 q, +0x48 q, and +0x24 left alone) and agree store for store.
//
// The two String temporaries around `GetDataDirectory() + buf->path` ARE
// written out, because retail's refcount traffic there is observable: the
// operator+ result is copy-constructed into a SECOND Filename with an
// `INC [rax-4]` (0x1800a10a6) and BOTH are released.
//
// ---------------------------------------------------------------------------
// ORACLE — NONE, and for a stronger reason than its two siblings
// ---------------------------------------------------------------------------
// This is a thread body that BLOCKS on real OS events and drives a live
// XAudio2 voice. All five probe shapes are ruled out by a fact, not by
// convenience: it is not pure (input sweep), a fabricated Stream would need a
// real voice and a real Device singleton (synthesised oracle), it is not a
// Serialize (write-path replay), and a two-sided A/B would submit every packet
// to the voice twice and consume the file handle twice. Attendance only —
// counters below, and no RT_DIFF site that could only ever print
// `divergences=0` (reference_shadow_marker_is_not_an_oracle.md).
// ============================================================================

#include <windows.h>
#include <xaudio2.h>

#include <cstdint>
#include <cstdio>
#include <cstring>

#include "../../types/system/Filename.h"
#include "../../types/io/FileStream.h"
#include "../../types/io/FileSystem.h"
#include "../../types/audio/Device.h"
#include "../../types/audio/SoundBuffer.h"
#include "../../types/audio/Stream.h"

namespace {

// ENDGAME B.3.7 block B4: the `kStream*`, `kChannel*`, `kVoiceCallback*` and
// `kSb*` byte offsets that lived here MOVED into runtime/types/audio/Stream.h,
// Device.h and SoundBuffer.h (B6-8). `Run` is `Audio::Stream::Run`, the slot-0
// override; the registered door delegates with a QUALIFIED call (a live Stream
// wears the minted table, whose slot 0 resolves back to that door).

// ---- the streaming geometry, all from this body ---------------------------
constexpr int32_t kPacketBytes   = 0x8b6;   // 0x1800a1172 IMUL EDX,EDX,0x8b6
constexpr int32_t kPacketsPerSlot = 16;     // 0x1800a1178 MOV R15D,0x10
static_assert(sizeof(Audio::Stream::Block) == 0x8ba0, "0x1800a11c0 IMUL RCX,RCX,0x8ba0");
constexpr int     kSlotCount     = 4;       // 0x1800a1380 CMP ECX,0x4
static_assert(offsetof(Audio::Stream::Block, packets) == 0x40, "0x1800a11d0 ADD RCX,0x40");
constexpr uint32_t kQueueHighWater = 3;     // 0x1800a1275 CMP …,0x3

// ---- XAudio2 (2.7): GetState (+0xc8) and SubmitSourceBuffer (+0xa8) on the
// source voice, through `<xaudio2.h>`'s IXAudio2SourceVoice; the three records
// are its (packed) XAUDIO2_VOICE_STATE / XAUDIO2_BUFFER / XAUDIO2_BUFFER_WMA.
constexpr uint32_t kXAudio2EndOfStream = 0x40;

uint64_t g_calls         = 0;
uint64_t g_opened        = 0;
uint64_t g_chunks        = 0;
uint64_t g_abandoned     = 0;   // the track changed mid-pass

struct RunSummary {
    ~RunSummary() {
        if (!g_calls) return;
        printf("[RtPort] Audio::Stream::Run ATTENDANCE: calls=%llu opened=%llu "
               "chunksSubmitted=%llu abandoned=%llu (NO ORACLE: a blocking "
               "thread body driving a live XAudio2 voice; see the banner)\n",
               (unsigned long long)g_calls, (unsigned long long)g_opened,
               (unsigned long long)g_chunks, (unsigned long long)g_abandoned);
        fflush(stdout);
    }
};
RunSummary g_summary;

::IO::FileSystem* FileSystemSingleton() {
    // ENDGAME B.3.9 SS1 (B39_RULES D7): the class static now -- same one deref.
    return ::IO::FileSystem::_singleton;
}

// The inlined System::String refcount idioms. Written out rather than routed
// through ~String because the ASYMMETRY is observable: the freeing branch
// leaves the slot holding `str-8` and the surviving branch nulls it — retail's
// 0x1800a10cf..0x1800a10ef, and the same shape collection/array_destroy.cpp's
// Body B documents.
// ENDGAME B.3.1 step 2: the adopt at 0x1800a1096..0x1800a10a6 and the three
// releases at 0x1800a10c6 / 0x1800a10ef / 0x1800a1118 are System::String's own
// AddRef and destructor; the hand-written copies that lived here are gone.

}  // namespace

// ============================================================================
// Audio::Stream::Run() — retail 0x1800a0f60, ord 3602
// ============================================================================
namespace Audio {

void Stream::Run() {
    ++g_calls;

    // `?_singleton@Device@Audio@@` (0x1801fe760), one load.
    Device* const dev = Device::_singleton;

    // Retail computes both record addresses from the singleton with no null
    // test of the singleton itself, then tests the SUMS (1800a0fa0 /
    // 1800a0fb1); the tests stay.
    const int64_t slot = _channel;                                      // MOVSXD
    Device::PlatformChannel* const voiceRec = &dev->_platformChannels[slot]; // 1800a0f9d
    if (!voiceRec) return;                                              // 1800a0fa0
    Device::AudioChannel* const chan = &dev->_channels[slot];           // 1800a0faa
    if (!chan) return;                                                  // 1800a0fb1
    const SoundBuffer* const buf = chan->source;
    if (!buf) return;                                                   // 1800a0fc2

    // _wmaLength is the table's BYTE length; >> 2 is its ENTRY count.
    const int32_t segCount =
        static_cast<int32_t>(static_cast<uint64_t>(
            static_cast<int64_t>(buf->_wmaLength)) >> 2);
    const uint32_t* const segTable = buf->_wmaData;

    // curSeg = first cumulative entry STRICTLY above the requested byte
    // position; 0 when none is (retail stores the counter only on the JA exit).
    int32_t curSeg = 0;                                                 // [RBP+0x60]
    if (segCount > 0) {                                                 // 1800a0ff2
        const uint32_t bytePos = static_cast<uint32_t>(_bytePosition);
        for (int32_t i = 0; i < segCount; ++i) {                        // 1800a0ff7
            if (segTable[i] > bytePos) { curSeg = i; break; }           // JA 1800a1009
        }
    }

    // ---- IO::FileStream st;  st.Open(FileSystem::GetDataDirectory() + buf->path)
    // Retail inlines the ctor here (1800a100c..6a) and the dtor at the end
    // (1800a14b3..1654); the local's scope is the function's.
    IO::FileStream st;

    ::IO::FileSystem* const fsys = FileSystemSingleton();

    // The three temporaries are declared so that reverse-order destruction is
    // retail's release order — path (1800a10c6), joined (1800a10ef), dataDir
    // (1800a1118) — and the inner scope ends them where retail ends them.
    bool opened;
    {
        // IO::FileSystem::GetDataDirectory (0x180007bd0), called by name
        // (B3R); its by-value result is constructed in place.
        System::Filename dataDir = fsys->GetDataDirectory();           // 1800a107f

        const System::String joined =
            dataDir.AsString() + buf->GetStreamPath();                  // 1800a1090

        System::Filename path;
        if (char* const d = joined._data) {                             // 1800a1096
            path._data = d;
            ++RtSystem::StringRefCount(d);
        }
        System::FilenamePrivateAccess::AdjustDelimiters(path);

        opened = st.IO::FileStream::Open(path);                         // 1800a10bc
    }

    if (opened) {                                                       // 1800a1147
        ++g_opened;

        // The VoiceCallback's OnBufferEnd signals THIS handle. Published here
        // and nowhere else — audio_voice_callback_win.cpp §2 cites this store.
        voiceRec->bufferEvent = _bufferEvent;                           // 1800a114d

        auto* const lock = reinterpret_cast<CRITICAL_SECTION*>(_lock);  // CAST-OK: win32
        HANDLE handles[2];                                              // [RBP-0x50]
        XAUDIO2_VOICE_STATE state{};                                    // [RBP-0x78]

        int32_t curSeg32   = curSeg;      // EBX,  mirrors [RBP+0x60]
        int64_t curSeg64   = curSeg;      // R12
        int     slotIndex  = 0;           // [RBP+0x68], int
        int64_t slotIndex64 = 0;          // [RBP+0x78], int64 (always equal)

        enum Exit { kExitStop, kExitDrain, kExitBail } how = kExitStop;

        for (;;) {                                                      // 1800a1170
            const int32_t byteOffset = curSeg32 * kPacketBytes;         // 1800a1172
            int32_t nPackets   = kPacketsPerSlot;                       // R15D
            int64_t nPackets64 = kPacketsPerSlot;                       // R13
            if (curSeg32 + kPacketsPerSlot > segCount) {                // 1800a1184
                nPackets   = segCount - curSeg32;
                nPackets64 = static_cast<int64_t>(segCount) - curSeg64;
            }
            const int32_t byteCount = nPackets * kPacketBytes;          // 1800a1197

            // Note 2 in the banner: the cursor store IS the seek for a packed
            // file, and it happens on both paths.
            st._file.cursor = byteOffset;                               // 1800a11a0
            if (static_cast<uint32_t>(st._file.handle) > 0xfffffffdu)   // 1800a11a4
                SetFilePointer(st._file.winHandle,
                               static_cast<LONG>(byteOffset), nullptr, FILE_BEGIN);

            // `slotIndex * 0x8ba0` (1800a11c0): the Block of the ring.
            Block* const block = &_buffers[slotIndex];
            fsys->ReadFile(st._file, byteCount, block->packets);        // 1800a11e7, 0x1800ace00

            // Rebase this chunk's cumulative-byte entries onto the chunk, and
            // write them into the slot's first 0x40 bytes.
            const uint32_t base =
                (curSeg32 == 0) ? 0u : segTable[curSeg64 - 1];          // 1800a11f2/f9
            if (nPackets64 > 0) {                                       // 1800a1201
                for (int64_t k = 0; k < nPackets64; ++k) {              // 1800a1230
                    _buffers[slotIndex64].cumulativeBytes[k] = segTable[curSeg64 + k] - base;
                }
            }

            // ---- wait until the voice has fewer than 3 buffers queued ------
            EnterCriticalSection(lock);                                 // 1800a124b
            IXAudio2SourceVoice* voice = voiceRec->voice;
            if (!voice) { how = kExitBail; break; }                     // 1800a1258
            voice->GetState(&state);                                    // +0xc8
            LeaveCriticalSection(lock);                                 // 1800a126f

            bool bailed = false;
            while (state.BuffersQueued >= kQueueHighWater) {            // 1800a1279
                handles[0] = static_cast<HANDLE>(_bufferEvent);    // 1800a1280
                handles[1] = static_cast<HANDLE>(_wakeEvent);      // 1800a1288
                const DWORD woke = WaitForMultipleObjects(2, handles, FALSE, INFINITE);
                EnterCriticalSection(lock);                             // 1800a12ab
                if (woke != 0) { bailed = true; break; }                // 1800a12b3
                voice = voiceRec->voice;
                if (!voice) { bailed = true; break; }                   // 1800a12c0
                voice->GetState(&state);                                // +0xc8
                LeaveCriticalSection(lock);                             // 1800a12d7
            }
            if (bailed) { how = kExitBail; break; }
            curSeg32 = curSeg;   // 1800a12e3 — restores EBX after the wait

            // ---- build and submit the chunk --------------------------------
            // Retail: a dword zero at +0x00, then +0x04..+0x2b zeroed as ONE
            // 0x28-byte memset (1800a12e6 / 1800a12f7): every field zero.
            XAUDIO2_BUFFER xb;
            xb.Flags = 0;                                               // 1800a12e6
            xb.AudioBytes = 0;                                          // 1800a12f7 ..
            xb.pAudioData = nullptr;
            xb.PlayBegin = 0;
            xb.PlayLength = 0;
            xb.LoopBegin = 0;
            xb.LoopLength = 0;
            xb.LoopCount = 0;
            xb.pContext = nullptr;                                      // .. +0x2b
            xb.AudioBytes = static_cast<uint32_t>(byteCount);           // 1800a12fc
            xb.pAudioData = block->packets;                             // 1800a1302 / 1800a130f

            curSeg32 += nPackets;                                       // 1800a1313
            curSeg = curSeg32;                                          // 1800a1316
            if (curSeg32 >= segCount &&                                 // 1800a131f
                buf->_loopForever == 0)                // 1800a1328
                xb.Flags = kXAudio2EndOfStream;                         // CMOVZ

            XAUDIO2_BUFFER_WMA wma;
            wma.PacketCount = static_cast<uint32_t>(nPackets);          // 1800a1337
            wma.pDecodedPacketCumulativeBytes = block->cumulativeBytes; // 1800a133b

            EnterCriticalSection(lock);                                 // 1800a1343
            voice = voiceRec->voice;
            if (!voice) { how = kExitBail; break; }                     // 1800a1350
            voice->SubmitSourceBuffer(&xb, &wma);                       // 1800a1361 +0xa8
            LeaveCriticalSection(lock);                                 // 1800a136b
            ++g_chunks;

            ++slotIndex;                                                // 1800a1374
            ++slotIndex64;                                              // 1800a1379
            if (slotIndex >= kSlotCount) {                              // 1800a1380
                slotIndex   = 0;
                slotIndex64 = 0;
            }

            curSeg64 += nPackets64;                                     // 1800a138d
            if (curSeg64 >= static_cast<int64_t>(segCount)) {           // 1800a1397
                if (buf->_loopForever == 0) {          // 1800a13a0
                    how = kExitDrain; break;                            // -> 1800a13bd
                }
                curSeg32 = 0; curSeg = 0; curSeg64 = 0;                 // 1800a13a6..ac
            }
            if (_stopping != 0) {            // 1800a13af
                how = kExitStop; break;                                 // -> 1800a13da
            }
        }

        if (how == kExitBail) {
            // 1800a1487 — every bail leaves the lock HELD; this is its release.
            ++g_abandoned;
            LeaveCriticalSection(lock);
        } else if (how == kExitDrain) {
            // 1800a13bd — the track finished and does not loop: wait for the
            // voice to play out what is already queued, unless the track
            // changes underneath us.
            for (;;) {
                EnterCriticalSection(lock);                             // 1800a13c1
                IXAudio2SourceVoice* voice = voiceRec->voice;
                if (!voice) { LeaveCriticalSection(lock); break; }      // 1800a13ce
                voice->GetState(&state);                                // +0xc8
                LeaveCriticalSection(lock);                             // 1800a140c
                if (state.BuffersQueued == 0) break;                    // 1800a1412 JBE

                handles[0] = static_cast<HANDLE>(_bufferEvent);    // 1800a1420
                handles[1] = static_cast<HANDLE>(_wakeEvent);      // 1800a1428
                const DWORD woke = WaitForMultipleObjects(2, handles, FALSE, INFINITE);
                EnterCriticalSection(lock);                             // 1800a144b
                if (woke != 0) {                                        // 1800a1453
                    ++g_abandoned;
                    LeaveCriticalSection(lock);
                    break;
                }
                voice = voiceRec->voice;
                if (!voice) { LeaveCriticalSection(lock); break; }      // 1800a1460
                voice->GetState(&state);                                // +0xc8
                LeaveCriticalSection(lock);                             // 1800a1477
                if (state.BuffersQueued == 0) break;                    // 1800a147d JA
            }
        }
        // kExitStop (1800a13af -> 1800a13da) holds no lock and needs no release.

        if (st._file.handle != -1)                                      // 1800a13da
            fsys->CloseFile(st._file);                                  // 0x1800accc0
    }
}   // ~FileStream: 1800a14b3..1654

}  // namespace Audio

// ENDGAME B.3.9 ENG W2: the `?Run@Stream@Audio@@UEAAXXZ` slot door and its
// registration are gone with the minted `??_7Stream@Audio@@6B@`: the live
// Stream wears the compiler table, whose slot 0 is Audio::Stream::Run above.
