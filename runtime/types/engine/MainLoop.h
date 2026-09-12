#pragma once
// ============================================================================
// runtime/types/engine/MainLoop.h — Engine::MainLoop, binary-compatible layout
// ============================================================================
//
// Wave 8 lane B. `Engine::MainLoop` is the frame driver: it owns the video
// context, the current/next `UpdateLoop`, the fixed-timestep clock, and the
// video-resource bookkeeping that survives a device loss.
//
// Recovered source file name (profile build, Runtime-x64-profile.dll, the
// assert at 0x1800d54dc): `Engine\MainLoop.cpp`.
//
// ---------------------------------------------------------------------------
// THIS SUPERSEDES engine/types/MainLoop.h (the hook-era header)
// ---------------------------------------------------------------------------
// That header got the SIZE and every OFFSET right — 0xa0 bytes, and all 21
// offsets below match it. It got two things wrong, both of which matter to a
// port and neither of which matters to a reader who only wants to peek:
//
//   1. It calls +0x0c, +0x8c and +0x9c padding. They are not padding: each of
//      +0x00, +0x80 and +0x90 is a `Collection::Array<T>` — `{T* _data;
//      int32 _size; int32 _capacity}` — and those three "pad" dwords are the
//      CAPACITY. Proof: `~MainLoop` (0x1800dd640) calls a distinct Array
//      destructor on each of the three (0x180033a40 for +0x90, 0x180033960
//      for +0x80, and tail-jumps to 0x180033880 for +0x00), and 0x180033880
//      ends with `_data = 0` followed by a single qword zero covering BOTH
//      +0x08 and +0x0c. Code that grows either callback array through
//      `AddPostUpdate`/`AddPostDraw` reads the capacity; a port that treats it
//      as padding would reallocate on every append.
//   2. Its `MainLoopAddr::IsRunning = 0x1800314b0` is commented as a no-arg
//      query. The shipped export is
//      `?IsRunning@MainLoop@Engine@@QEBA_NPEBVUpdateLoop@2@@Z` (ordinal 3158)
//      — it takes a `const UpdateLoop*`. The no-arg `IsRunning` in the export
//      table is `UpdateLoop`'s (0x1800dd7b0, ordinal 3160), a different class.
//
// The hook-era header is left in place for the exe tree that includes it; the
// two corrections are applied there as well (PLAN §6 step 8). New runtime-port
// code includes THIS header.
//
// ---------------------------------------------------------------------------
// THERE IS NO VTABLE
// ---------------------------------------------------------------------------
// `??_7MainLoop@Engine@@6B@` is not exported, and per WAVE8.md §2 that is not
// proof on its own. The proof is the ctor (0x1800dd6b0): its first store into
// `this` is
//        1800dd6c6  XOR EAX,EAX
//        1800dd6c8  MOV qword ptr [RCX],RAX
// a ZERO — the `_videoResources._data` pointer — not a vtable address. The
// profile build's ctor (0x1800d5420) opens with the identical zero store. No
// `MainLoop` method is virtual and nothing in the class is reached through a
// vtable, so PLAN §4.3 risk 3 does not apply here either.
//
// ---------------------------------------------------------------------------
// EVIDENCE FOR EVERY OFFSET
// ---------------------------------------------------------------------------
//   off    field                  proven by                             retail
//   -----  ---------------------  ------------------------------------  -----------
//   0x00   _videoResources        ~MainLoop's third Array dtor;         0x1800dd640
//                                 OnVideoError appends 0x18-byte
//                                 entries to it, OnVideoRestart walks   0x1800dc5e0
//                                 it with `ADD RSI,0x18`                0x1800dc3b0
//   0x10   _context               CreateVideoContext stores the         0x1800dd500
//                                 0x2168-byte Video::Context here;
//                                 ?GetContext@ returns *(this+0x10)     0x1800314d0
//   0x18   _contextDirty          ctor zeroes +0x18 and +0x19 as one
//                                 word; Frame/Draw read it              0x1800dd100
//   0x19   _quit                  Run's loop condition,
//                                 `CMP byte ptr [RSI+0x19],0`           0x1800dd44a
//   0x20   _currentLoop           Run sets it from its argument;
//                                 ?GetCurrentLoop@ returns it           0x1800314e0
//   0x28   _nextLoop              the ENTIRE body of ?SetNextLoop@:
//                                 `MOV [RCX+0x28],RDX; RET`             0x1800dd7a0
//   0x30   _startTicks            Run's first QPC conversion            0x1800dd404
//   0x38   _accumulator           Frame                                 0x1800dd100
//   0x40   _totalTime             Frame                                 0x1800dd100
//   0x48   _deltaTime             ctor: SecondsToTicks(1.0f/60.0f);
//                                 ?GetDeltaTime@ returns it             0x1800314f0
//   0x50   _maxAccumulator        ctor: SecondsToTicks(1.0f/15.0f)      0x1800dd751
//   0x58   _frameTime             Run: _frameEndTicks - _frameStartTicks;
//                                 ?GetFrameTime@                        0x180031500
//   0x60   _frameStartTicks       Run, per iteration;
//                                 ?GetFrameStartTime@                   0x180031510
//   0x68   _frameUpdateTime       ?GetFrameUpdateTime@ /
//                                 ?MarkFrameUpdateTime@                 0x180031520
//   0x70   _frameEndTicks         Run, per iteration                    0x1800dd4ae
//   0x78   _engine                ctor arg (RDX); Run reads the
//                                 subsystems through it                 0x1800dd40e
//   0x80   _postDraw              ?AddPostDraw@ does `ADD RCX,0x80`
//                                 then Array::SetSize                   0x1800dd040
//   0x90   _postUpdate            ?AddPostUpdate@ does `ADD RCX,0x90`   0x1800dcf60
//
// ---------------------------------------------------------------------------
// THE TWO CLOCK CONSTANTS ARE FLOATS WIDENED TO DOUBLE — DO NOT RETYPE THEM
// ---------------------------------------------------------------------------
// Retail's ctor computes the two timing fields as
//        MOVSD XMM0, [0x1801616a8]      ; 0x3f91111120000000
//        DIVSD XMM0, [0x1801fe728]      ; seconds per QPC tick
//        CVTTSD2SI RAX, XMM0
// and the profile build shows what that inlined FROM:
//        MOVSS XMM0, [0x1801b04e4]      ; a 32-bit float
//        CALL  System::Timer::SecondsToTicks(float)
// 0x3f91111120000000 is exactly `(double)(1.0f/60.0f)` and 0x3fb1111120000000
// is exactly `(double)(1.0f/15.0f)` — i.e. the source wrote a float, and the
// compiler widened it. Writing `1.0/60.0` in a port would produce a different
// double and a different tick count. Use the bit patterns.
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "../common.h"
#include "../collection/Collection.h"

// Everything below lives in `RtEngine`, matching runtime/types/engine/UpdateLoop.h
// (lane A) and the RtVideo / RtObject convention the earlier waves set. This is
// NOT cosmetic: engine/types/MainLoop.h defines a GLOBAL `struct MainLoop`,
// engine/types/UpdateLoop.h a GLOBAL `struct UpdateLoop`, and `engine/types` is
// on the include path AHEAD of `runtime/types` — at global scope these two
// headers would be a hard redefinition in any TU that included both trees, and
// `_currentLoop`/`_nextLoop` would point at a DIFFERENT, incomplete UpdateLoop
// from the one lane A defines.
namespace RtEngine {

struct UpdateLoop;   // defined by lane A in runtime/types/engine/UpdateLoop.h
struct Engine;       // defined by lane B in runtime/types/engine/Engine.h

// Retail addresses in Runtime-x64.dll (Ghidra space, image base 0x180000000).
// Ordinals are from the shipped export directory, read with pefile; entries
// with no ordinal are DLL-internal (not exported) and must be reached with
// ResolveAddr(Module::Runtime, addr).
namespace MainLoopAddr {
    constexpr uintptr_t Singleton           = 0x1801fe750;  // ord 4813 (DATA)

    constexpr uintptr_t Ctor                = 0x1800dd6b0;  // ord 263
    constexpr uintptr_t Dtor                = 0x1800dd640;  // ord 627
    constexpr uintptr_t Run                 = 0x1800dd3c0;  // ord 3601
    constexpr uintptr_t SetNextLoop         = 0x1800dd7a0;  // ord 3929
    constexpr uintptr_t OnVideoError        = 0x1800dc5e0;  // ord 3291
    constexpr uintptr_t OnVideoRestart      = 0x1800dc3b0;  // ord 3295
    constexpr uintptr_t Get                 = 0x180031570;  // ord 2418
    constexpr uintptr_t IsRunning           = 0x1800314b0;  // ord 3158, takes UpdateLoop*
    constexpr uintptr_t GetContext          = 0x1800314d0;  // ord 2511
    constexpr uintptr_t GetCurrentLoop      = 0x1800314e0;  // ord 2531
    constexpr uintptr_t GetDeltaTime        = 0x1800314f0;  // ord 2560
    constexpr uintptr_t GetFrameTime        = 0x180031500;  // ord 2677
    constexpr uintptr_t GetFrameStartTime   = 0x180031510;  // ord 2676
    constexpr uintptr_t GetFrameUpdateTime  = 0x180031520;  // ord 2678
    constexpr uintptr_t MarkFrameUpdateTime = 0x180031530;  // ord 3220
    constexpr uintptr_t AddPostDraw         = 0x1800dd040;
    constexpr uintptr_t AddPostUpdate       = 0x1800dcf60;
    constexpr uintptr_t RemovePostDraw      = 0x1800dcf90;
    constexpr uintptr_t RemovePostUpdate    = 0x1800dceb0;

    // Internal, NOT exported.
    constexpr uintptr_t Update              = 0x1800dd070;  // bool Update(const Time&)
    constexpr uintptr_t Frame               = 0x1800dd100;  // bool Frame(bool)
    constexpr uintptr_t Draw                = 0x1800dcd40;
    constexpr uintptr_t CreateVideoContext  = 0x1800dd500;
    constexpr uintptr_t ReleaseVideoContext = 0x1800dd5e0;

    // The two STATIC thunks the ctor installs into Video::Device so the device
    // can call back on loss/restore. Each is a one-line
    // `MainLoop::OnVideoXxx(_singleton)`, and each is the ONLY DLL-internal
    // caller of its member (get_xrefs_to). See WAVE8_LANE_B.md, detour verdict.
    constexpr uintptr_t OnVideoError_static   = 0x1800dcd20;
    constexpr uintptr_t OnVideoRestart_static = 0x1800dc5c0;

    // Video::Device::SetVideoErrorFuncs(err, restart) — the name is the
    // profile build's (0x1800fd860); retail inlines it to two stores at
    // Device +0x50 / +0x58.
    constexpr uintptr_t VideoDeviceSingleton = 0x1801fe7a8;
    constexpr uintptr_t DeviceErrorFuncSlot   = 0x50;
    constexpr uintptr_t DeviceRestartFuncSlot = 0x58;

    // System::Timer statics the ctor's inlined SecondsToTicks reads.
    constexpr uintptr_t SecondsPerTick        = 0x1801fe728;  // double
    constexpr uintptr_t OneOverSixty          = 0x1801616a8;  // (double)(1.0f/60.0f)
    constexpr uintptr_t OneOverFifteen        = 0x1801616b0;  // (double)(1.0f/15.0f)
}

// One entry of MainLoop::_videoResources. Built by OnVideoError from the live
// object graph and consumed by OnVideoRestart:
//   OnVideoError  0x1800dca4d: entry._typeId = obj->vtable[0x10]()   // GetTypeId
//                 0x1800dca50: entry._object = obj
//                 0x1800dcac6: entry._name   = <the object's resource name>
//   OnVideoRestart 0x1800dc416: reads _typeId, 0x1800dc440: _object,
//                 0x1800dc4b3: _name, and strides by 0x18.
struct VideoResourceEntry {
    int32_t _typeId;     // +0x00
    int32_t _pad04;      // +0x04  never written by either function
    void*   _object;     // +0x08  Object::External*
    char*   _name;       // +0x10  System::String (refcounted; data-8 = length,
                         //        data-4 = refcount — see the String header)
};
static_assert(sizeof(VideoResourceEntry) == 0x18, "VideoResourceEntry is 0x18 bytes");
static_assert(offsetof(VideoResourceEntry, _typeId) == 0x00, "VideoResourceEntry::_typeId");
static_assert(offsetof(VideoResourceEntry, _object) == 0x08, "VideoResourceEntry::_object");
static_assert(offsetof(VideoResourceEntry, _name)   == 0x10, "VideoResourceEntry::_name");

// One entry of _postUpdate / _postDraw. Both AddPostUpdate (0x1800dcf60) and
// AddPostDraw (0x1800dd040) build the element from RDX and R8 in a 16-byte
// stack temp and hand it to Array::SetSize, so the element is exactly this
// pair. The two arrays use DIFFERENT Array instantiations (0x18004f8e0 vs
// 0x18004f790) because the callback signatures differ — Update takes a
// `const Time&`, Draw takes a `float` — but the element WIDTH is the same.
struct MainLoopCallback {
    void* _func;      // +0x00
    void* _context;   // +0x08  passed back as the callback's first argument
};
static_assert(sizeof(MainLoopCallback) == 0x10, "MainLoopCallback is 0x10 bytes");

struct MainLoop {
    // Video resources captured on device loss and restored on device restart.
    Collection::Array<VideoResourceEntry> _videoResources;  // +0x00

    void*    _context;            // +0x10  Video::Context*, owned; 0x2168 bytes
    uint8_t  _contextDirty;       // +0x18  loop switch asks for a fresh context
    uint8_t  _quit;               // +0x19  Run's exit flag
    uint8_t  _pad1a[6];           // +0x1a  ctor zeroes +0x18/+0x19 as one word;
                                  //        +0x1a..+0x1f are never written

    UpdateLoop* _currentLoop;     // +0x20
    UpdateLoop* _nextLoop;        // +0x28  SetNextLoop's only store

    // All times are QPC ticks converted through System::Timer's
    // (qpc - base) * mul / div, NOT raw QPC counts.
    // Run converts QPC once, before the frame loop, and stores it here
    // (0x1800dd404).
    //
    // *** ANSWERED, Wave 25 lane BODY. *** Lane B left open "whether
    // MainLoop::Frame also rewrites it per frame". IT DOES — every call,
    // unconditionally, at 0x1800dd146, and it uses the PREVIOUS value as the
    // delta base:
    //       now  = CurrentTimeTicks()
    //       R8   = now - _startTicks        ; elapsed since the last Frame
    //       _startTicks   = now             ; 0x1800dd146
    //       _accumulator += R8              ; 0x1800dd14a
    // So this field means "the tick at which the LAST FRAME was sampled", not
    // "the tick Run started". Run's single pre-loop store is the seed for the
    // first frame's delta, not a start timestamp anything later reads as one.
    // Re-derive: objdump -d -M intel --start-address=0x1800dd100
    //            --stop-address=0x1800dd3ac Runtime-x64.dll
    int64_t  _startTicks;         // +0x30
    int64_t  _accumulator;        // +0x38  fixed-step carry
    int64_t  _totalTime;          // +0x40
    int64_t  _deltaTime;          // +0x48  SecondsToTicks((float)1/60)
    int64_t  _maxAccumulator;     // +0x50  SecondsToTicks((float)1/15) — 4 steps
    int64_t  _frameTime;          // +0x58  _frameEndTicks - _frameStartTicks
    int64_t  _frameStartTicks;    // +0x60
    int64_t  _frameUpdateTime;    // +0x68
    int64_t  _frameEndTicks;      // +0x70

    Engine*  _engine;             // +0x78  ctor arg; NOT owned

    Collection::Array<MainLoopCallback> _postDraw;    // +0x80
    Collection::Array<MainLoopCallback> _postUpdate;  // +0x90
};

static_assert(sizeof(MainLoop) == 0xa0, "MainLoop is 0xa0 bytes");
static_assert(offsetof(MainLoop, _videoResources)  == 0x00, "MainLoop::_videoResources");
static_assert(offsetof(MainLoop, _context)         == 0x10, "MainLoop::_context");
static_assert(offsetof(MainLoop, _contextDirty)    == 0x18, "MainLoop::_contextDirty");
static_assert(offsetof(MainLoop, _quit)            == 0x19, "MainLoop::_quit");
static_assert(offsetof(MainLoop, _currentLoop)     == 0x20, "MainLoop::_currentLoop");
static_assert(offsetof(MainLoop, _nextLoop)        == 0x28, "MainLoop::_nextLoop");
static_assert(offsetof(MainLoop, _startTicks)      == 0x30, "MainLoop::_startTicks");
static_assert(offsetof(MainLoop, _accumulator)     == 0x38, "MainLoop::_accumulator");
static_assert(offsetof(MainLoop, _totalTime)       == 0x40, "MainLoop::_totalTime");
static_assert(offsetof(MainLoop, _deltaTime)       == 0x48, "MainLoop::_deltaTime");
static_assert(offsetof(MainLoop, _maxAccumulator)  == 0x50, "MainLoop::_maxAccumulator");
static_assert(offsetof(MainLoop, _frameTime)       == 0x58, "MainLoop::_frameTime");
static_assert(offsetof(MainLoop, _frameStartTicks) == 0x60, "MainLoop::_frameStartTicks");
static_assert(offsetof(MainLoop, _frameUpdateTime) == 0x68, "MainLoop::_frameUpdateTime");
static_assert(offsetof(MainLoop, _frameEndTicks)   == 0x70, "MainLoop::_frameEndTicks");
static_assert(offsetof(MainLoop, _engine)          == 0x78, "MainLoop::_engine");
static_assert(offsetof(MainLoop, _postDraw)        == 0x80, "MainLoop::_postDraw");
static_assert(offsetof(MainLoop, _postUpdate)      == 0x90, "MainLoop::_postUpdate");

}  // namespace RtEngine
