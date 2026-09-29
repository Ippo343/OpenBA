// code/Game/Game/unreg_lane5_musicplayer.cpp
// Code-fn wave LANE 5 — MusicPlayer::Update (exe 0x1400a40e0).
//
// WHAT WAS BROKEN WITH IT UNREGISTERED
// ------------------------------------
// GameUpdateLoop::Update step 7 (code/Game/Game/gameupdateloop.cpp, the
// `post_simulation:` block) does
//
//     if (fn_MusicPlayer_Update) fn_MusicPlayer_Update(self->m_musicPlayer);
//
// with `fn_MusicPlayer_Update = ResolveAddr(Module::Exe, 0x1400a40e0)`. That
// address is unregistered, so the pointer is null and the call is SKIPPED
// every frame — a null-guard-and-SKIP consumer. The observable effect is that
// **background music never plays at all**: this function is the one and only
// writer of the music-player state and the one and only caller of
// Audio::Scene::PlayEffect for the music channel. Retail's sole caller is
// GameUpdateLoop::Update itself (Ghidra xrefs to 0x1400a40e0: one
// UNCONDITIONAL_CALL from FUN_140096380 plus its own .pdata unwind record).
//
// The Ghidra/our-tree name "AmbientSound::Update" (gameupdateloop.cpp:131) is a
// MISNOMER, already flagged in engine/types/GameUpdateLoop.h +0x80. The body is
// a music-track selector: pick a track whose [monthLo..monthHi] and
// [moodLo..moodHi] windows both contain the current month and the town mood.
//
// WHY IT WAS HELD BACK — and why that blocker is now gone
// -------------------------------------------------------
// gameupdateloop_load.cpp step 27 builds the MusicPlayer struct at
// GameUpdateLoop+0x80 with `+0x08 MusicTrackList* = nullptr (Find deferred)`
// and carries a comment naming this exact function as blocked, because the
// retail body reads `*(int*)(musicPlayer[+0x08] + 0x28)` (the track count) with
// NO null check — wiring it against a null +0x08 is an instant read-of-0x28.
//
// That premise held when the comment was written but no longer does: MusicList
// (the "Game/MusicTracks.rsc" resource type) has since been fully ported —
// factory, 9-slot vtable @0x14015ee40, faithful Serialize, REGISTER_TYPE — in
// code/Game/Game/musiclist.cpp, and RegisterMusicListVtable() is called from
// standalone/address_registry.cpp:1797. So the resource is loadable today.
//
// DEVIATION FROM RETAIL (deliberate, documented)
// ----------------------------------------------
// Retail does the Find once, in GameUpdateLoop::Load. Doing it there means
// editing gameupdateloop_load.cpp, which this lane must not touch (six lanes
// share one tree). Instead this file does the SAME FindOrLoad lazily, once, on
// the first Update where +0x08 is still null, and caches the result in +0x08 —
// which is exactly the field retail's Load would have filled. Nothing else in
// the tree reads +0x08, so a first-frame load is observationally equivalent to
// a load-time one. **If the integrator prefers it in Load, move the
// EnsureTrackList() body into step 27 and delete it here; the rest of this file
// is unchanged either way.**
//
// GATE: DEFAULT-ON as of the lane D close-out (2026-09-06); BANISHED_LIVE_MUSIC=0
// is the kill-switch. It was opt-in because the authoring lane could not build or
// run the game and the port was therefore UNPROVEN. It is proven now -- see the
// measured PLAY line quoted above.
//
// FAITHFULNESS NOTE (an unguarded deref left in on purpose)
// ---------------------------------------------------------
// The mood normalisers come from citizen[0]'s description:
//     desc = citizen[0]->+0x18;
//     if (!IsDerivedFrom(desc, CitizenDescription)) desc = nullptr;
//     mood = (avg/desc->+0x70 + avg2/desc->+0xa4) * 0.5f;
// Retail dereferences `desc` unconditionally after that select — the decompile
// even carries the retail ASSERT text
// "entity.IsPtrValid() && entity->GetComponent<NaturalResourceComponent>()".
// Per CLAUDE.md we do NOT paper over that with a null check: if it ever fires,
// citizen[0] handed us a non-CitizenDescription and the fix is upstream.
//
// STRUCT LAYOUTS (from the 0x1400a40e0 decompile)
// -----------------------------------------------
// MusicPlayer (GameUpdateLoop+0x80, 0x18 bytes, MemAlloc'd by Load step 27):
//   +0x00 uint32  currentEffectHandle   (seeded -1)
//   +0x08 MusicList*                    (seeded null — see EnsureTrackList)
//   +0x10 uint64  lastTrackExternalPtr  (seeded 0)
// MusicList (code/Game/Game/musiclist.cpp): +0x20 data, +0x28 count, stride 0x18
//   entry +0x00 ExternalPtr<SoundEffect>   +0x08 float moodLo   +0x0c float moodHi
//         +0x10 int   monthLo              +0x14 int   monthHi
//   (matches musiclist.cpp's default-init {+0x08=0, +0x0c=1.0f, +0x14=11}:
//    mood is normalised to 0..1 and months are 0..11.)
// GameUpdateLoop fields read: +0x78 Audio::Scene*, +0xd8 System::Random*,
//   +0xf0 citizen list (+0xa0 data / +0xa8 count, 8-byte InternalBasePtrs),
//   +0x128 dayCounter (its +0x08 is the accumulated System::Time).
// .rdata constants verified with read_memory:
//   DAT_140172114 = 0.5f    (mood average weight)
//   DAT_14017225c = 3600.0f (seconds per game year)
//   DAT_1401721b8 = 12.0f   (months per year)
//   DWORD_1401c7190 = the "CitizenDescription" type id (same global the
//   citizenai_* files resolve; see code/Game/Component/citizenai_gather.cpp:63)

#include "object/TypeInfo.h"
#include "Game/GameUpdateLoop.h"   // B3R STORAGE: GameUpdateLoop::_singleton
#include "Game/GameUpdateLoop.h"   // caller batch CB6: the world's named fields
#include "system/Filename.h"
#include "system/String.h"
#include "Component/CitizenDescription.h"   // caller batch CB6: TypeId()
#include "object/External.h"   // ENDGAME B.3.3 step 6A
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include "system/Value.h"      // ENDGAME B.3.4b System caller batch (Random/Time/Color)
#include "audio/Audio.h"       // ENDGAME B.3.7 callers (K4)
#include "Game/exe_bodies.h"

namespace {

constexpr float kMoodAverageWeight = 0.5f;      // DAT_140172114
constexpr float kSecondsPerYear    = 3600.0f;   // DAT_14017225c
constexpr float kMonthsPerYear     = 12.0f;     // DAT_1401721b8

// DEFAULT-ON since the lane D close-out run of 2026-09-06, which is the first run that
// proved this function reaches the mixer:
//     [MusicPlayer] MusicTracks.rsc -> 00000271db93d780 tracks=15
//     [MusicPlayer] PLAY track=12/15 ext=271db912ee8 scene=00000271db7aa138 -> handle=1536
// (autostart new game, standalone_error.log:2565-2566). The lazy EnsureTrackList Find
// resolved, all 15 tracks parsed, the month/mood filter selected one and
// Audio::Scene::PlayEffect returned a live handle.

// DWORD_1401c7190 is CitizenDescription's type-id cell (Table 1).
uint32_t CitizenDescriptionTypeId() {
    return CitizenDescription::TypeId();
}

// See the DEVIATION note in the file header. Retail's GameUpdateLoop::Load
// fills musicPlayer+0x08; ours defers it, so fill it here on first use.
void EnsureTrackList(uint8_t* mp) {
    if (*reinterpret_cast<void**>(mp + 0x08)) return;
    static bool attempted = false;
    if (attempted) return;              // one shot — never re-hit the loader
    attempted = true;

    if (!::Object::ExternalFactory::_singleton) {
        fprintf(stderr, "[MusicPlayer] SKIP — factory unavailable (factory=%p)\n",
                ::Object::ExternalFactory::_singleton);
        fflush(stderr);
        return;
    }

    System::Filename fname("Game/MusicTracks.rsc");
    // ENDGAME B.3.3 step 6A: member call on the real Object::ExternalFactory.
    void* list = const_cast<::Object::External*>(
        ::Object::ExternalFactory::_singleton
            ->FindOrLoad(fname));                                            // B.3.3 bridge
    *reinterpret_cast<void**>(mp + 0x08) = list;

    fprintf(stderr, "[MusicPlayer] MusicTracks.rsc -> %p tracks=%d\n", list,
            list ? *reinterpret_cast<int32_t*>(reinterpret_cast<uint8_t*>(list) + 0x28) : -1);
    fflush(stderr);
}

} // namespace

// ============================================================
// MusicPlayer::Update — faithful port of exe 0x1400a40e0
// ============================================================
extern "C" void MusicPlayer_Update(void* musicPlayer) {
    if (!musicPlayer) return;
    if (GameUpdateLoop::_singleton == nullptr) return;

    uint8_t* mp = reinterpret_cast<uint8_t*>(musicPlayer);
    uint8_t* gw = reinterpret_cast<uint8_t*>(GameUpdateLoop::_singleton);
    void*    audioScene = reinterpret_cast<GameUpdateLoop*>(gw)->m_audioScene;   // B.3.8 bridge

    // --- Early-out: a track is still playing on our handle. -----------------
    if (!audioScene) return;
    if (reinterpret_cast<Audio::Scene*>(audioScene)->IsEffectPlaying(
            *reinterpret_cast<uint32_t*>(mp + 0x00))) return;   // B.3.7 bridge

    EnsureTrackList(mp);
    uint8_t* trackList = *reinterpret_cast<uint8_t**>(mp + 0x08);
    if (!trackList) return;   // deviation-only guard: retail's Load guaranteed it

    // --- Town mood: average health and happiness over the citizen list, -----
    // each normalised by the corresponding max on citizen[0]'s description.
    uint8_t* citizens   = static_cast<uint8_t*>(reinterpret_cast<GameUpdateLoop*>(gw)->m_citizenList);   // B.3.8 bridge
    float    moodA = 0.0f, moodB = 0.0f;
    if (citizens) {
        int      count = *reinterpret_cast<int32_t*>(citizens + 0xa8);
        int64_t* arr   = *reinterpret_cast<int64_t**>(citizens + 0xa0);
        if (count > 0 && arr) {
            for (int i = 0; i < count; ++i) {
                uint64_t ibp = static_cast<uint64_t>(arr[i]);
                uint8_t* c = reinterpret_cast<uint8_t*>(ibp);
                moodA += *reinterpret_cast<float*>(c + 0x50);
                moodB += *reinterpret_cast<float*>(c + 0x80);
            }
            // Retail re-reads count and citizen[0] twice (once per normaliser);
            // both reads are identical, so hoist the description lookup.
            uint64_t ibp0 = static_cast<uint64_t>(arr[0]);
            void* desc = *reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(ibp0) + 0x18);
            uint32_t tid = CitizenDescriptionTypeId();
            if (!(tid && ::Object::IsDerivedFromNullSafe(desc, tid))) desc = nullptr;
            // Faithful: retail derefs `desc` here with no null test (ASSERT path).
            uint8_t* d = reinterpret_cast<uint8_t*>(desc);
            moodA = (moodA / static_cast<float>(count)) / *reinterpret_cast<float*>(d + 0x70);
            moodB = (moodB / static_cast<float>(count)) / *reinterpret_cast<float*>(d + 0xa4);
        }
    }
    const float mood = (moodB + moodA) * kMoodAverageWeight;

    // --- Current month from the accumulated day-counter time. ---------------
    int month = 0;
    if (void* dayCounter = reinterpret_cast<GameUpdateLoop*>(gw)->m_dayCounter) {   // B.3.8 bridge
        float secs = reinterpret_cast<const System::Time*>(reinterpret_cast<uint8_t*>(dayCounter) + 8)->AsSeconds();   // B.3.4b bridge
        int m = static_cast<int>((secs / kSecondsPerYear) * kMonthsPerYear);
        month = (m > 11) ? 0 : m;   // faithful: >11 wraps to 0, not to 11
    }

    // --- Collect every track whose month AND mood windows contain us. -------
    const int      trackCount = *reinterpret_cast<int32_t*>(trackList + 0x28);
    uint8_t* const tracks     = *reinterpret_cast<uint8_t**>(trackList + 0x20);
    if (trackCount < 1 || !tracks) return;

    std::vector<uint64_t> matches;   // retail hand-rolls a MemAlloc'd grow array
    matches.reserve(4);
    for (int i = 0; i < trackCount; ++i) {
        const uint8_t* e = tracks + static_cast<size_t>(i) * 0x18;
        const int   monthLo = *reinterpret_cast<const int32_t*>(e + 0x10);
        const int   monthHi = *reinterpret_cast<const int32_t*>(e + 0x14);
        const float moodLo  = *reinterpret_cast<const float*>(e + 0x08);
        const float moodHi  = *reinterpret_cast<const float*>(e + 0x0c);
        if (monthLo <= month && month <= monthHi && moodLo <= mood && mood <= moodHi) {
            matches.push_back(*reinterpret_cast<const uint64_t*>(e + 0x00));
        }
    }

    const int n = static_cast<int>(matches.size());
    int pick;
    if (n == 1) {
        pick = 0;
    } else if (n < 2) {
        return;                       // nothing eligible this month/mood
    } else {
        void* rng = reinterpret_cast<GameUpdateLoop*>(gw)->m_random;   // B.3.8 bridge
        if (!rng) return;
        pick = static_cast<int>(reinterpret_cast<System::Random*>(rng)->Rand(static_cast<uint32_t>(n)));   // B.3.4b bridge
        // Don't immediately repeat the last track: step one past it, wrapping.
        if (matches[static_cast<size_t>(pick)] == *reinterpret_cast<uint64_t*>(mp + 0x10)) {
            ++pick;
        }
        if (pick >= n) pick = 0;
        if (pick < 0)  return;
    }

    // Retail passes a zero-initialised System::Time by reference (SoundType 2).
    uint64_t zeroTime = 0;
    uint32_t handle = reinterpret_cast<Audio::Scene*>(audioScene)->PlayEffect(
        reinterpret_cast<Audio::SoundEffect*>(matches[static_cast<size_t>(pick)]),
        static_cast<Audio::Scene::SoundType>(2),
        *reinterpret_cast<const System::Time*>(&zeroTime));   // B.3.7 bridge
    // PROOF-OF-PLAY (lane D): the one line that says the music channel was actually
    // handed a track. Every track change logs; a run that never prints this has not
    // played music, whatever the rest of the trace says.
    fprintf(stderr, "[MusicPlayer] PLAY track=%d/%d ext=%llx scene=%p -> handle=%u\n",
            pick, n, (unsigned long long)matches[static_cast<size_t>(pick)],
            audioScene, handle);
    fflush(stderr);
    *reinterpret_cast<uint32_t*>(mp + 0x00) = handle;
    *reinterpret_cast<uint64_t*>(mp + 0x10) = matches[static_cast<size_t>(pick)];
}
