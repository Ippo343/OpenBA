// ============================================================================
// tools/audio_tail/sdl_pull_check.cpp -- offline: the SDL backend's pull path, no device
// ============================================================================
// A synthetic model of audio_sdl.cpp's voice: an SDL_AudioStream with a get-callback
// that hands over what SDL asks for from a Platform::AudioFeed (the game's queue, the
// real one), pulled the way SDL's device thread pulls (SDL_GetAudioStreamData of one
// device period per pass). Checks, per pass:
//   * the pull returned the full period: a short pull is silence the device splices
//     into that period (a notch, a click);
//   * the output is continuous: a seamless sine's largest sample step never exceeds
//     its slope, with the device's silence fill modelled.
// Covers the looping ambience (LoopCount 255) and a one-shot to its END (the last
// pass short by design, then OnStreamEnd), at the game's three formats, two device
// rates, five period sizes and the pitch ratios the game sets.
//
// `--prime=N` hands over N extra input frames on a voice's FIRST pull, which is what
// audio_sdl.cpp's kFirstPullMarginFrames does. Measured 2026-10-11 (SDL 3.4.18, SSE):
//   --prime=0   375 scenarios short on pass 0 (7-8 output frames), 0 short elsewhere
//   --prime=8   0 problems
// The unit binary never links SDL (D4_RULES C7), so this lives in tools/.
//
// Build (from the repo root, after `mingw32-make standalone-debug` installed SDL3):
//   g++ -std=c++17 -O1 -D_USE_MATH_DEFINES -I. -Isrc/platform/common //       -Ibuild/third_party/install/include tools/audio_tail/sdl_pull_check.cpp //       src/platform/common/audio_feed.cpp build/third_party/install/lib/libSDL3.a //       -lm -lkernel32 -luser32 -lgdi32 -lwinmm -limm32 -lole32 -loleaut32 -lversion //       -luuid -ladvapi32 -lsetupapi -lshell32 -o build/sdl_pull_check.exe
// Run:  build/sdl_pull_check.exe [--prime=N] [-v]      exit 0 = no problems
// ============================================================================
#include <SDL3/SDL.h>

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#include "audio_feed.h"

struct Voice {
    SDL_AudioStream* stream = nullptr;
    Platform::AudioFeed feed;
    bool drainPending = false, ended = false;
    int streamEndPass = -1;
    long putBytes = 0, shortPuts = 0, pulls = 0;
    uint32_t frameBytes;
    explicit Voice(uint32_t fb) : feed(fb), frameBytes(fb) {}
};

struct Sink final : Platform::AudioFeed::Sink {
    Voice* v;
    uint32_t want;
    explicit Sink(Voice* vv, uint32_t w) : v(vv), want(w) {}
    void Put(const uint8_t* d, uint32_t n) override {
        SDL_PutAudioStreamData(v->stream, d, (int)n);
        v->putBytes += n;
    }
    void BufferStart() override {}
    void LoopEnd() override {}
    void BufferEnd() override {}
    void EndOfStreamPut() override {
        SDL_FlushAudioStream(v->stream);
        v->drainPending = true;
    }
};

static int g_pass = 0;
static int g_primeFrames = 0;   // extra input frames handed over on a voice's FIRST pull

static void SDLCALL GetCallback(void* ud, SDL_AudioStream* s, int additional, int) {
    Voice* v = (Voice*)ud;
    if (v->drainPending && !v->ended && SDL_GetAudioStreamAvailable(s) == 0) {
        v->ended = true;
        v->streamEndPass = g_pass;
    }
    Sink sink(v, additional);
    long before = v->putBytes;
    uint32_t want = (uint32_t)(additional > 0 ? additional : 0);
    if (v->pulls == 0 && want > 0) want += (uint32_t)g_primeFrames * v->frameBytes;
    v->feed.Pull(want, sink);
    if (v->putBytes - before < additional && !v->drainPending) ++v->shortPuts;
    ++v->pulls;
}

// One scenario. Returns the number of problems.
static int Run(int srcRate, int srcCh, int dstRate, int dstCh, int period, float ratio, bool loop,
               int passes, bool verbose) {
    const double f0 = 440.0;
    // The "sample": a sine whose length is NOT a multiple of anything, as a shipped sound.
    // 1.7371 s, but a whole number of cycles so a loop wrap is seamless (f0 is re-tuned a hair).
    const int frames = (int)(srcRate * 1.7371) + 13;
    const double cycles = std::floor(f0 * frames / srcRate);
    const double f = cycles * srcRate / frames;
    std::vector<int16_t> pcm((size_t)frames * srcCh);
    for (int n = 0; n < frames; ++n)
        for (int c = 0; c < srcCh; ++c)
            pcm[(size_t)n * srcCh + c] = (int16_t)std::lrint(20000.0 * std::sin(2 * M_PI * f * n / srcRate));
    // The sine's largest step per OUTPUT frame (its slope at the output rate), with margin.
    const double outF = f0 * ratio;
    const double maxStep = 20000.0 * 2 * M_PI * outF / dstRate * 1.35 / 32768.0 + 0.002;

    SDL_AudioSpec src{SDL_AUDIO_S16LE, srcCh, srcRate};
    SDL_AudioSpec dst{SDL_AUDIO_F32LE, dstCh, dstRate};
    Voice v((uint32_t)(2 * srcCh));
    v.stream = SDL_CreateAudioStream(&src, &dst);
    SDL_SetAudioStreamGetCallback(v.stream, GetCallback, &v);
    SDL_SetAudioStreamFrequencyRatio(v.stream, ratio);
    SDL_SetAudioStreamGain(v.stream, 0.7f);
    v.feed.Submit((const uint8_t*)pcm.data(), (uint32_t)(pcm.size() * 2), 0,
                  loop ? Platform::AudioFeed::kLoopInfinite : 0, true);

    const int dstFrame = 4 * dstCh;
    const int len = period * dstFrame;
    std::vector<float> out((size_t)period * dstCh);
    int problems = 0, shortPulls = 0;
    float prev = 0.0f;
    bool havePrev = false;
    double worstStep = 0;
    int worstPass = -1;
    int endPass = -1;
    long totalOut = 0;
    for (g_pass = 0; g_pass < passes; ++g_pass) {
        const int got = SDL_GetAudioStreamData(v.stream, out.data(), len);
        if (got < 0) { std::printf("  pass %d: error %s\n", g_pass, SDL_GetError()); return 1; }
        totalOut += got / dstFrame;
        if (got < len) {
            if (loop || !v.drainPending) {
                ++shortPulls;
                if (verbose || shortPulls <= 3)
                    std::printf("  pass %d: SHORT pull %d of %d bytes (queued input %d, available %d)\n",
                                g_pass, got, len, SDL_GetAudioStreamQueued(v.stream),
                                SDL_GetAudioStreamAvailable(v.stream));
            } else if (endPass < 0) {
                endPass = g_pass;
            }
        }
        // The device fills what the pull did not return with silence (SDL_audio.c,
        // SDL_PlaybackAudioThreadIterate); a short pull mid-sound is therefore a notch.
        const bool fillSilence = got < len && (loop || !v.drainPending);
        if (fillSilence) for (int n = got / dstFrame; n < period; ++n) out[(size_t)n * dstCh] = 0.0f;
        for (int n = 0; n < (fillSilence ? period : got / dstFrame); ++n) {
            const float x = out[(size_t)n * dstCh];
            if (havePrev) {
                const double d = std::fabs((double)x - prev);
                if (d > worstStep) { worstStep = d; worstPass = g_pass; }
            }
            prev = x;
            havePrev = true;
        }
        if (!loop && v.ended) break;
    }
    problems += shortPulls;
    const bool stepBad = worstStep > maxStep;
    if (stepBad) ++problems;
    std::printf("%5d Hz %dch -> %5d Hz %dch period %4d ratio %.4f %-8s: pulls %d, short %d, worst step %.4f @pass %d (allowed %.4f)%s",
                srcRate, srcCh, dstRate, dstCh, period, ratio, loop ? "loop" : "one-shot", g_pass,
                shortPulls, worstStep, worstPass, maxStep, stepBad ? "  STEP!" : "");
    if (!loop)
        std::printf(", out %ld frames (expect ~%ld), last-data pass %d, OnStreamEnd pass %d", totalOut,
                    (long)std::llround((double)frames * dstRate / srcRate / ratio), endPass, v.streamEndPass);
    std::printf("\n");
    SDL_DestroyAudioStream(v.stream);
    return problems;
}

int main(int argc, char** argv) {
    bool verbose = false;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "-v") == 0) verbose = true;
        else if (std::strncmp(argv[i], "--prime=", 8) == 0) g_primeFrames = std::atoi(argv[i] + 8);
    }
    std::printf("prime frames on the first pull: %d\n", g_primeFrames);
    if (!SDL_Init(0)) { std::printf("SDL_Init: %s\n", SDL_GetError()); return 2; }
    int problems = 0;
    const float ratios[] = {1.0f, 0.9438743f, 1.0594631f, 1.1224620f, 0.8908987f, 1.2599211f, 0.5f, 2.0f};
    const int periods[] = {1024, 480, 441, 512, 1000};
    // The device: 48 kHz stereo (WASAPI's usual shared-mode mix) and 44.1 kHz stereo.
    for (int dstRate : {48000, 44100})
        for (int period : periods)
            for (float r : ratios) {
                problems += Run(44100, 1, dstRate, 2, period, r, true, 20000, verbose);    // animal / tool / UI format, looped for length
                problems += Run(44100, 2, dstRate, 2, period, r, true, 20000, verbose);    // Field / Wetland / River ambience
                problems += Run(48000, 2, dstRate, 2, period, r, true, 20000, verbose);    // Forest / Winter ambience
                problems += Run(44100, 1, dstRate, 2, period, r, false, 20000, verbose);   // a one-shot to its end
                problems += Run(48000, 2, dstRate, 2, period, r, false, 20000, verbose);
            }
    std::printf("problems: %d\n", problems);
    SDL_Quit();
    return problems ? 1 : 0;
}
