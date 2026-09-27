// ============================================================================
// runtime/src/achievement/goal_serialize.cpp — Wave 9 lane P (tier 0)
// Achievement::Goal::Serialize and Achievement::GoalInstance::Persist
// ============================================================================
//
//   ?Serialize@Goal@Achievement@@UEAA_NAEAVStream@IO@@@Z    0x18009ee20
//   ?Persist@GoalInstance@Achievement@@UEAAXAEAVStream@IO@@@Z 0x18009ef70
//
// These are the TWO functions Wave 5 lane A explicitly held back
// (runtime/src/achievement/goal.cpp's banner: "the two that move a stream
// cursor ... because no oracle can run both implementations without
// double-consuming the stream"). That reason was correct when it was written
// and is no longer the whole truth: Wave 9 lane S built
// the Wave 9 lane S emission oracle, which replayed a WRITE-path Serialize into two
// INDEPENDENT SYNTHETIC SINKS and compares the emission sequence. A write-path
// Serialize is a pure read of the object, so the replay is legitimate; what
// made it impossible before was the shared stream, not the function.
//
// So this file exists because a blocker dissolved, not because the earlier
// judgement was wrong. goal.cpp's banner is corrected in place (PLAN §6 step 8).
//
// ----------------------------------------------------------------------------
// TWO CORRECTIONS TO WAVE 9 LANE B'S INVENTORY
// ----------------------------------------------------------------------------
// 1. `wave9_laneB_inventory.csv` lists `Goal::Serialize` as `ghidra-exact, 3`
//    instructions and "likely delegates to a shared base Serialize". It is
//    **33 instructions** and it delegates to nothing: it builds an
//    `IO::Serializer` on its own stack, asks the stream's `IO::Encoding` for a
//    reader or a writer, drives two field serializers, and destroys the codec.
//    A "3-instruction" reading of it would have been ported as a one-line
//    forwarder and silently emitted nothing.
//
// 2. The CSV's callee note claims `GoalInstance::Persist` shares the unnamed
//    internal `FUN_180009d70` with `GoalList::Initialize`, and lane P's brief
//    made "port them together to pay for that dependency once" the reason to
//    pair them. **Persist does not call FUN_180009d70, or FUN_180006d10, or
//    FUN_180009fa0.** Its only direct callee is `FUN_180005dc0`. Read at
//    0x18009ef70 the call list is: two `IO::Encoding` vtable slots (+0x18 /
//    +0x20), `FUN_180005dc0`, one `ReaderWriter` slot (+0x98), and the codec's
//    deleting destructor. The pairing rationale was false; the two functions
//    are paired here only because they are the same shape.
//
// ----------------------------------------------------------------------------
// NEW LANE-D ROWS THIS FILE CREATES: TWO, and they are named
// ----------------------------------------------------------------------------
//   FUN_1800042a0  — reached by Goal::Serialize     (over Goal+0x20)
//   FUN_180005dc0  — reached by GoalInstance::Persist (over GoalInstance+0x18)
//
// Both are instantiations of the SAME template body — `Object::ExternalBasePtr
// ::Serialize` for two different pointee types; the two decompilations are
// line-for-line identical apart from the `_typeInfo` they compare against.
// Neither appears in `wave9_residue_gameplay.txt` today, so these really are
// two new rows in the hardest bucket, exactly the growth WAVE9.md measured for
// `KeyMap::RegisterInput` (823/504 -> 825/505). They are called BY ADDRESS, the
// way `ClearAll`, `RegisterInput` and `Audio::Scene::Serialize` already call
// `Collection` internals — what is needed from them is the ENTRY POINT, not a
// port. That is a real cost and it is stated rather than hidden.
//
// A CAUTION FOR WHOEVER PORTS THEM, and it is stronger than this comment's own
// first draft said. That draft read "on the READING side (`Encoding` slot +0x10
// returns 3) they call ExternalFactory::FindOrLoad". **That conflated two
// different discriminators and understated the problem.** Reading the whole
// body, there are TWO `FindOrLoad` call sites, selected by two unrelated tests:
//
//   1. `encoding->vtable[+0x10]() == 3` -- NOT "reading". It is a THIRD mode,
//      the resource-reference VISIT pass (WAVE3.md finding 5, IO::VisitorStream
//      / ??_7Visitor@IO@@6B@). It calls GetPath -> FindOrLoad -> IsDerivedFrom
//      and THROWS THE RESULT AWAY; the call is made for its side effect of
//      forcing the referenced resource to be loaded.
//   2. The common tail, guarded by `stream->direction == 0` -- the ORDINARY
//      READING path. This one calls FindOrLoad and **STORES the result into
//      `*param_1`**. It is the actual resolution of the reference, on every
//      save load, not an exotic mode.
//
// So `ExternalFactory::FindOrLoad` -- which WAVE9.md rows 27-28 record as having
// NO POSSIBLE ORACLE, because it rewrites the one shared AVL tree -- is reached
// by the plain load path, and a port of these two internals inherits that
// squarely. It is not a corner.
//
// NONE OF THAT IS REACHABLE UNDER THIS FILE'S OWN ORACLE, which is why the two
// ports below are verifiable and the two internals are not. The lane-S sink
// pins `stream.direction = 1` (writing) and binds all 16 Encoding slots to a
// recording thunk that returns the stub ReaderWriter POINTER -- never 3. Both
// tests therefore fail on both sides of every comparison, and neither
// FindOrLoad site is entered.
//
// THE ONE REPLAY HAZARD ON THE WRITE PATH, checked rather than hedged: that path
// emits `ExternalFactory::GetReferenceId(_singleton, ext)` as a uint. If that
// ASSIGNED an id on first use, replaying would emit N then N+1 and the harness
// would manufacture a divergence it caused itself -- the exact shape of the
// longjmp/dtor defect lane S found in its own prefix stop. It does not:
// `?GetReferenceId@ExternalFactory@Object@@QEBAIPEBVExternal@2@@Z` (0x1800bf100)
// is `QEBA` -- const -- and its body is a pure lookup (build a Filename, find it
// in the map at `this+0x10`, return `[node+0x14]`, else -1). No insert, no
// counter. The write path is idempotent and the replay is sound.
// ============================================================================

#include <atomic>
#include <cstdint>
#include <cstdio>

#include "../../funcs/rt_port.h"
#include "../../types/achievement/Goal.h"
#include "../../types/io/Serializer.h"

using rt::io::Encoding;
using rt::io::ReaderWriter;
using rt::io::Serializer;
using rt::io::Stream;

// Forward-declared rather than pulled in from standalone/resolve.h, for the
// reason rt_port.h gives: that header drags in <windows.h> and runtime/src/
// must stay platform-neutral. Byte-identical to resolve.h's declaration.
enum class Module : int { Exe = 0, Runtime = 1, VideoDX11 = 2, COUNT = 3 };
uintptr_t ResolveAddr(Module mod, uintptr_t ghidra_addr);

namespace {

// The two ExternalBasePtr::Serialize instantiations (see the banner). Both take
// (ExternalBasePtr* self, Serializer* ser) — RCX/RDX at the call sites — and
// return nothing the caller reads. Both addresses are REGISTERED by
// runtime/src/object/external_ptr_serialize.cpp, so ResolveAddr (registry
// first) lands on the port.
//
// ENDGAME B.3.7: the caches are namespace-scope ATOMIC slots, not
// function-local statics — `GoalInstance::Persist` runs on the LOADER thread
// on every save load (B37_RULES §5, RULE B6-5).
using PtrSerializeFn = void (*)(void*, Serializer*);
std::atomic<void*> g_ptrSerGoal{nullptr};           // 0x1800042a0
std::atomic<void*> g_ptrSerGoalInstance{nullptr};   // 0x180005dc0

PtrSerializeFn RuntimeAddr(std::atomic<void*>& slot, uintptr_t addr) {
    void* p = slot.load(std::memory_order_acquire);
    if (p == nullptr) {
        p = reinterpret_cast<void*>(ResolveAddr(Module::Runtime, addr));
        if (p != nullptr) slot.store(p, std::memory_order_release);
    }
    return reinterpret_cast<PtrSerializeFn>(p);
}

// The shared prologue of both bodies, transcribed once.
//
//   Serializer ser{ &stream, nullptr };
//   Encoding* enc = stream.encoding;                       // [stream+0x10]
//   ser.readerWriter = stream.direction == 0               // [stream+0x08]
//        ? enc->vtable[+0x18](enc, &ser, 0)                // CreateReader
//        : enc->vtable[+0x20](enc, &ser, 0);               // CreateWriter
//
// The third argument is the save-format VERSION and it is ZERO here: retail
// zeroes R8D at 0x18009ee44 / 0x18009ef94 for the `CMP [stream+8],R8D` and
// never reloads it before the factory call.
ReaderWriter* MakeCodec(Stream* stream, Serializer* ser) {
    ser->stream = stream;
    ser->readerWriter = nullptr;

    Encoding* enc = stream->encoding;
    void** encVt = enc->vtable;
    const int slot = (stream->direction == rt::io::kDirectionReading)
                         ? rt::io::encoding_vtbl::kCreateReader
                         : rt::io::encoding_vtbl::kCreateWriter;

    using FactoryFn = ReaderWriter* (*)(Encoding*, Serializer*, int);
    ReaderWriter* rw = reinterpret_cast<FactoryFn>(encVt[slot / 8])(enc, ser, 0);
    ser->readerWriter = rw;
    return rw;
}

// The shared epilogue: `if (rw) rw->vtable[0](rw, 1)` — the scalar DELETING
// destructor (flag 1), which is how retail frees the codec the factory just
// allocated. Skipping it leaks 0x30 bytes per Serialize.
void DestroyCodec(ReaderWriter* rw) {
    if (!rw) return;
    using DtorFn = void (*)(ReaderWriter*, int);
    reinterpret_cast<DtorFn>(rw->vtable[rt::io::rw_vtbl::kDtor / 8])(rw, 1);
}

}  // namespace

namespace Achievement {

// ============================================================================
// bool Goal::Serialize(IO::Stream&) — SLOT 5
// ?Serialize@Goal@Achievement@@UEAA_NAEAVStream@IO@@@Z   retail 0x18009ee20
//
// Serializes exactly Goal's own two fields, in declaration order:
//   +0x20 _stringTable  ExternalPtr<UI::StringTable>  -> 0x1800042a0
//   +0x28 _text         System::String                -> String::Serialize
// A load therefore consumes precisely those two records. Getting the ORDER or
// the COUNT wrong is the silent save-desync this project has paid for four
// times (reference_slot3_wrapper_must_resolve_first.md). Always `MOV AL,1`.
//
// The slot's parameter is `void*` because `Object::External` declares it so
// (External.h); the object behind it is retail's `IO::Stream`.
// ============================================================================
bool Goal::Serialize(void* streamv) {
    Stream* stream = static_cast<Stream*>(streamv);
    Serializer ser;
    ReaderWriter* rw = MakeCodec(stream, &ser);

    RuntimeAddr(g_ptrSerGoal, 0x1800042a0)(&_stringTable, &ser);   // Goal+0x20
    _text.Serialize(&ser);                                          // Goal+0x28

    DestroyCodec(rw);
    return true;
}

// ============================================================================
// void GoalInstance::Persist(IO::Stream&) — SLOT 3
// ?Persist@GoalInstance@Achievement@@UEAAXAEAVStream@IO@@@Z  retail 0x18009ef70
//
// Same prologue and epilogue; the middle is two records, NOT in declaration
// order — the ExternalPtr at +0x18 goes BEFORE the uint at +0x10. That is what
// the bytes do; "tidying" it to ascending offset would desync every save.
//   +0x18 _goal   ExternalPtr<Achievement::Goal> -> 0x180005dc0
//   +0x10 _flags  uint                           -> ReaderWriter +0x98
// +0x98 is `uint` in RETAIL (+0xb0 in the profile build — never copy offsets
// from it). The record's NAME argument is NULL (`XOR R8D,R8D` at 0x18009efcc).
// ============================================================================
void GoalInstance::Persist(void* streamv) {
    Stream* stream = static_cast<Stream*>(streamv);
    Serializer ser;
    ReaderWriter* rw = MakeCodec(stream, &ser);

    RuntimeAddr(g_ptrSerGoalInstance, 0x180005dc0)(&_goal, &ser);   // GoalInstance+0x18

    using UIntFn = void (*)(ReaderWriter*, uint32_t*, const char*);
    reinterpret_cast<UIntFn>(rw->vtable[rt::io::rw_vtbl::kScalarUInt / 8])(
        rw, &_flags, nullptr);                                      // GoalInstance+0x10

    DestroyCodec(rw);
}

}  // namespace Achievement

// ---- The registered doors — QUALIFIED delegations --------------------------
// ENDGAME B.3.9 ENG: the two slot doors (`?Serialize@Goal@Achievement@@`
// 0x18009ee20 and `?Persist@GoalInstance@Achievement@@` 0x18009ef70) and their
// registration are gone -- their only reader was the minted `??_7Goal` /
// `??_7GoalInstance` tables. The exe subclasses call the members QUALIFIED.
