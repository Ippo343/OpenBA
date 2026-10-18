// ============================================================================
// src/ext/background_save/background_save.cpp -- ext.background_save (class A, default OFF)
// ============================================================================
// NOT RETAIL. background_save.h has the design; docs/extensions/handins/HITCH_SAVESNAP.md the
// measurements it rests on:
//   * the serialise has no output-visible side effect: a replay at the save point writes the
//     same bytes as the save (three replays, the stress town), so the image is a function of
//     the world state alone;
//   * a Draw changes no serialised byte (the image at the next Update's start equals the one at
//     the save Update's end), and Draw makes no ExternalFactory call (0 in 3000 Draw windows,
//     ~7000 per Update), so the worker's reference mode and its reference-id lookups meet no
//     resource load on the main thread;
//   * what the camera input writes is either serialised in the head (PlayerController, on the
//     main thread before the worker starts) or kept on a shadow (the tool manager's +0x3c /
//     +0x40 / +0x44, which the body serialises) and copied back after the join.
// ============================================================================
#include "ext/background_save/background_save.h"

#include "Game/GameUpdateLoop.h"
#include "Game/serialize_scope.h"
#include "Game/PlayerController.h"
#include "Game/PlayerToolManager.h"
#include "Entity/ComponentEntity.h"
#include "Entity/EntityList.h"
#include "save/SaveLoad.h"
#include "io/Stream.h"
#include "ui/UserInterface.h"
#include "system/Value.h"
#include "math/Math.h"
#include "ext/registry.h"
#include "ext/eye_camera/eye_camera.h"
#include "err_line.h"
#include "load_time.h"
#include "stress_harness.h"

#include <atomic>
#include <cstdint>
#include <cstring>
#include <new>
#include <thread>
#include <vector>

namespace BackgroundSave {
namespace {

// The verify image's stream: a plain writing MemoryStream (its destructor is protected; this
// subclass is what ends it).
struct FlatStream : IO::MemoryStream {};

struct Job {
    GameUpdateLoop*   loop  = nullptr;
    Save::SavedState* state = nullptr;
    alignas(16) uint8_t cmsStorage[sizeof(IO::CompressedMemoryStream)];
    IO::CompressedMemoryStream* cms = nullptr;
    GameUpdateLoop::SerializeScope scope;
    std::thread       worker;
    std::atomic<bool> done{false};
    bool              async = false;
    double            t0 = 0, mainMs = 0, workerMs = 0;
    int               frozenUpdates = 0;
    // The tool manager's camera floats (+0x3c zoom distance, +0x40 pitch target, +0x44 manual
    // pitch): the frozen camera input writes these copies; the real ones are the body's.
    alignas(PlayerToolManager) uint8_t shadowStorage[sizeof(PlayerToolManager)];
    PlayerToolManager* shadow = nullptr;
    std::vector<uint8_t> verifyImage;   // g_verifyOn only
};

Job* g_job = nullptr;
bool g_installed = false;

bool VerifyDecompress(Save::SavedState* s, std::vector<uint8_t>& out) {
    alignas(16) uint8_t st[sizeof(IO::CompressedMemoryStream)];
    IO::CompressedMemoryStream* cms = ::new (static_cast<void*>(st)) IO::CompressedMemoryStream();
    s->Retrieve(cms->GetMemoryStream());
    bool ok = cms->Open();
    if (ok) {
        const uint32_t size = cms->IO::CompressedStream::GetSize();
        out.resize(size);
        ok = size == 0 || cms->IO::CompressedStream::Read(static_cast<int>(size), out.data());
        cms->IO::CompressedMemoryStream::Close();
    }
    cms->IO::CompressedMemoryStream::~CompressedMemoryStream();
    return ok;
}

// The whole save, synchronously, into a flat image: retail's SerializeAll at the save point
// (a replay of the write path writes the save's bytes, HITCH_SAVESNAP section 1).
void VerifyCapture(GameUpdateLoop* loop, std::vector<uint8_t>& out) {
    constexpr int kCap = 96 * 1024 * 1024;
    alignas(16) uint8_t st[sizeof(FlatStream)];
    FlatStream* fs = ::new (static_cast<void*>(st)) FlatStream();
    fs->Create(kCap);
    loop->SerializeAll(*fs);
    const uint8_t* data = static_cast<const uint8_t*>(fs->_data);
    out.assign(data, data + fs->GetCurrentPos());
    fs->~FlatStream();
}

void WorkerMain(Job* job) {
    const double t = LoadTime::NowMs();
    GameUpdateLoop_SaveRestoreBody(job->loop, job->cms, job->state, job->scope);
    job->workerMs = LoadTime::NowMs() - t;
    job->done.store(true, std::memory_order_release);
}

// The end of a save: the worker joined, the stream ended, the camera floats home, the lines.
void Finish(Job* job) {
    job->worker.join();
    job->cms->IO::CompressedMemoryStream::~CompressedMemoryStream();
    PlayerToolManager* tm = job->loop->m_toolManager;
    tm->field_0x3C = job->shadow->field_0x3C;
    tm->field_0x40 = job->shadow->field_0x40;
    tm->field_0x44 = job->shadow->field_0x44;

    if (g_verifyOn) {
        std::vector<uint8_t> image;
        const bool ok = VerifyDecompress(job->state, image);
        const bool same = ok && image == job->verifyImage;
        long long firstDiff = -1;
        if (ok && !same) {
            const size_t n = image.size() < job->verifyImage.size() ? image.size() : job->verifyImage.size();
            for (size_t i = 0; i < n; ++i)
                if (image[i] != job->verifyImage[i]) { firstDiff = (long long)i; break; }
            if (firstDiff < 0) firstDiff = (long long)n;
        }
        StressHarness_Verify("BackgroundSave", same, firstDiff);
        ErrLine("[BackgroundSave] verify %s: image %zu bytes, synchronous %zu bytes, firstDiff=%lld\n",
                same ? "IDENTICAL" : "DIFF", image.size(), job->verifyImage.size(), firstDiff);
    }
    if (g_pfProfOn) {
        const IO::CompressedStream::ExtAsyncStats as = IO::CompressedStream::ExtAsyncLastStats();
        ErrLine("[Stress::save] background main=%.2f ms worker=%.2f ms frozenUpdates=%d total=%.2f ms "
                "stored=%u async=%d jobs=%d endWait=%.2f\n",
                job->mainMs, job->workerMs, job->frozenUpdates, LoadTime::NowMs() - job->t0,
                job->state->_size, job->async ? 1 : 0, job->async ? as.jobs : 0,
                job->async ? as.endWaitMs : 0.0);
    }
    delete job;
}

// Save::SaveLoad::_extSaveStart: UpdateSave state 1, in place of SaveRestore(true, state).
bool Start(Save::SavedState* state) {
    GameUpdateLoop* loop = GameUpdateLoop::_singleton;
    if (!Ext::Ext().background_save || loop == nullptr || g_job != nullptr) return false;

    Job* job = new Job();
    job->loop = loop;
    job->state = state;
    job->t0 = LoadTime::NowMs();
    if (g_verifyOn) VerifyCapture(loop, job->verifyImage);   // before anything moves

    const double tMain = LoadTime::NowMs();
    // GameUpdateLoop::SaveRestore's save arm, up to SerializeAll, then the head.
    job->cms = ::new (static_cast<void*>(job->cmsStorage)) IO::CompressedMemoryStream();
    job->cms->Create();
    job->async = Ext::Ext().async_save_compress;
    if (job->async) job->cms->ExtBeginAsyncCompress();
    loop->SerializeAllHead(*job->cms, job->scope);
    // EntityList::Serialize's opening flush and removals: they mutate the entity list and
    // delete entities, so they run here, before the worker starts. Serialize's own call then
    // finds the lists empty.
    loop->m_entityList->FlushForSerialize(*job->cms);

    std::memcpy(job->shadowStorage, static_cast<const void*>(loop->m_toolManager), sizeof(PlayerToolManager));
    job->shadow = reinterpret_cast<PlayerToolManager*>(job->shadowStorage);   // CAST-OK: a byte copy only UpdateCamera's floats are read from or written to; never destroyed
    job->mainMs = LoadTime::NowMs() - tMain;

    g_job = job;
    job->worker = std::thread(WorkerMain, job);
    return true;
}

// Save::SaveLoad::_extSavePoll: true once the worker is done (and then finished here).
bool Poll() {
    Job* job = g_job;
    if (job == nullptr) return true;
    if (!job->done.load(std::memory_order_acquire)) return false;
    g_job = nullptr;
    Finish(job);
    if (!g_installed) {   // switched off while this save ran
        Save::SaveLoad::_extSaveStart = nullptr;
        Save::SaveLoad::_extSavePoll  = nullptr;
    }
    return true;
}

}  // namespace

void Sync() {
    g_installed = Ext::Ext().background_save;
    if (g_installed) {
        Save::SaveLoad::_extSaveStart = &Start;
        Save::SaveLoad::_extSavePoll  = &Poll;
    } else {
        Save::SaveLoad::_extSaveStart = nullptr;
        if (g_job == nullptr) Save::SaveLoad::_extSavePoll = nullptr;   // else: the poll clears it
    }
}

bool Busy() { return g_job != nullptr; }

void FrozenUpdate(GameUpdateLoop& loop, const System::Time& time) {
    Job* job = g_job;
    ++job->frozenUpdates;
    PlayerController* pc = loop.m_playerController;
    if (!(Ext::Ext().eye_camera && EyeCamera::Active())) {
        // PlayerToolManager::Update's camera half (UpdateCamera, 0x1400feec0) on the shadow: the
        // controller's yaw / target / distance (serialised in the head) and the shadow's floats.
        // Hotkeys, tools and the UI do not run: they act on the world the worker is reading.
        job->shadow->UpdateCamera(loop.m_userInterface->_mouseCaptured == 0, time._ticks);
        // GameUpdateLoop::Update step 6, the camera transform.
        Math::Transform transform = pc->CalcTransform();
        Math::Transform3 t3 = transform.AsTransform3();
        pc->camera.SetCameraToWorld(t3);
    }
    loop.m_saveLoad->Update(time);   // the poll: joins the worker and finishes state 1
}

void Join() {
    Job* job = g_job;
    if (job == nullptr) return;
    g_job = nullptr;
    Finish(job);
}

}  // namespace BackgroundSave
