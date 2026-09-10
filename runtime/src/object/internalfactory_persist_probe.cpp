// ============================================================================
// runtime/src/object/internalfactory_persist_probe.cpp — WAVE 11 LANE OBJINT
//
//   A SYNTHETIC-SUBJECT ORACLE for the two InternalFactory persist-state rows:
//     ?ClearPersistState@InternalFactory@Object@@QEAAXXZ        0x1800bfe30
//     ?ResetPersistState@InternalFactory@Object@@QEAAXAEAVStream@IO@@@Z
//                                                               0x1800bfe40
// ============================================================================
//
// WHY A SECOND INSTRUMENT WHEN A SHADOW ALREADY EXISTS.
//
// `runtime/src/object/internal.cpp` has carried a shadow wrapper for each of
// these since Wave 3, and WAVE3.md's defect E records that NEITHER HAS EVER
// EXECUTED — they live only on the save/load path. This lane runs them. But a
// live shadow run, even a green one, does not finish the job, because each of
// those two shadows has a STRUCTURAL BLIND SPOT that no number of calls closes:
//
//   ClearPersistState_Shadow  calls the DLL and then asserts the post-state is
//                             `data=null size=0 cap=0`. OUR BODY'S STORES NEVER
//                             RUN. In particular our `MemFree(persistIds._data)`
//                             never runs, so a port that forgot the free
//                             entirely would pass that shadow on every call —
//                             a silent leak under a green verdict, which is
//                             exactly reference_a_written_recipe_can_be_wrong's
//                             dtor-that-only-frees-the-block.
//
//   ResetPersistState_Shadow  calls the DLL and then checks the two arrays'
//                             sizes, the predicted capacities and the -1 fill.
//                             It says NOTHING about the three stream calls our
//                             body makes — the encoding-factory slot choice
//                             (+0x18 read vs +0x20 write), the int slot (retail
//                             0xa0; the PROFILE build's 0xb8 would write the
//                             right value through the wrong slot and desync
//                             every save), or the destroy through slot 0.
//
// So the shadow answers "is our MODEL of retail's post-state right", and this
// file answers "does our BODY do what retail's body does". They are genuinely
// two screens and not one screen read twice: the shadow never executes our
// stores, and this file never touches the live factory or the real save stream.
//
// ----------------------------------------------------------------------------
// SUBJECT
// ----------------------------------------------------------------------------
// A private `InternalFactory`-shaped scratch, never registered anywhere and
// never reachable from the game. `_objects` is given a real N-element buffer
// with `_size == _capacity == N`, so `ResetPersistState`'s `Reserve(count)` is
// a no-op on it and the only array that moves is `_persistIds` — which is the
// array both bodies are about.
//
// `reference_no_route_is_not_no_oracle` is the precedent: a synthesised subject
// for a body with no convenient live route found a real defect that a byte pin
// and an entry counter were both structurally blind to. Here there IS a live
// route as well, and both are used.
//
// ----------------------------------------------------------------------------
// THE SYMMETRY FIX, because an A/B whose two legs start from different states
// is not an A/B
// ----------------------------------------------------------------------------
// `RtSaveTrace::Compare` runs `ours(obj, sinkA)` and then `theirs(obj, sinkB)`
// on ONE object. For a body that mutates its subject, leg B would then start
// where leg A finished: leg A grows `_persistIds` from empty and leg B finds it
// already grown, so leg B never exercises the growth at all and the capacities
// are not comparable. Both adapters below therefore RESTORE THE SUBJECT TO A
// PRISTINE TEMPLATE at entry (freeing whatever the previous leg allocated), so
// the two legs start byte-identical. Without that the comparison would be
// green for a reason that has nothing to do with the port.
//
// ----------------------------------------------------------------------------
// WHAT THIS FILE STILL DOES NOT COVER, said here rather than left implied
// ----------------------------------------------------------------------------
//   * The encoding factory's THIRD argument (retail's `XOR R8D,R8D` at
//     0x1800bfe6c, the serializer version). `save_trace.cpp`'s `EncThunk`
//     records the SLOT of an Encoding call and deliberately not its arguments,
//     and adding them there would compare a stack address that legitimately
//     differs between the two sinks. That argument is pinned twice from the
//     bytes instead — Wave 9 lane Y read the `XOR R8D,R8D` directly, and Wave 4
//     independently pinned both factory slots at arity 3 from the out-of-line
//     `??0Serializer@IO@@` — so it is not unverified, it is verified by a
//     different method than this one.
//   * The READ direction. The sink drives `direction = 1` (writing) only, which
//     is `save_trace.h`'s own documented limit.
//
// ----------------------------------------------------------------------------
// ARMING
// ----------------------------------------------------------------------------
//   BANISHED_RT_PERSIST_AB=1   run both A/Bs once, at registration time.
//   BANISHED_RT_PERSIST_AB=2   ... and run the NEGATIVE CONTROLS first, each of
//                              which the oracle MUST catch. A probe whose
//                              deliberate defect slips through was never
//                              measuring anything.
//
// The emission half additionally needs BANISHED_RT_SAVE_TRACE (save_trace.h's
// own gate); this file sets nothing on its behalf and reports plainly when it
// is off, rather than printing a green line from an instrument that did not run.
// ============================================================================

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "../../types/object/Internal.h"
#include "../../funcs/rt_port.h"
#include "../io/save_trace.h"

enum class Module : int { Exe = 0, Runtime = 1, VideoDX11 = 2, COUNT = 3 };
uintptr_t ResolveAddr(Module mod, uintptr_t ghidra_addr);

namespace RtObject {
// The two ports under test. Defined in internal.cpp, in this namespace.
void RtObject_InternalFactory_ClearPersistState(InternalFactory* self);
void RtObject_InternalFactory_ResetPersistState(InternalFactory* self, void* stream);
}

namespace {

constexpr int      kObjCount    = 7;     // _objects._size for the subject
constexpr uint64_t kIntArrBytes = 16 * sizeof(int32_t);

// ---- the shipped bodies, through the RAW door ------------------------------
// `RtOriginal` is GetProcAddress on the shipped DLL and nothing else. It MUST
// be the raw door here: `ResolveAddr(Module::Runtime, …)` consults the port
// registry first, so once internal.cpp's table tier is armed it would hand back
// OUR OWN prologue and the oracle would be comparing retail with itself — the
// failure that left a Wave 5 probe dark for five waves
// (reference_registering_an_address_converts_a_selfcheck_into_an_oracle.md).
using ClearFn = void (*)(RtObject::InternalFactory*);
using ResetFn = void (*)(RtObject::InternalFactory*, void*);

ClearFn TheirClear() {
    static ClearFn f = RtOriginal<ClearFn>("?ClearPersistState@InternalFactory@Object@@QEAAXXZ");
    return f;
}
ResetFn TheirReset() {
    static ResetFn f = RtOriginal<ResetFn>(
        "?ResetPersistState@InternalFactory@Object@@QEAAXAEAVStream@IO@@@Z");
    return f;
}

// ---- the DLL's allocator, for building and tearing down the subject --------
void* MemAllocLocked(uint64_t size, int align) {
    using Fn = void* (*)(uint64_t, int);
    static Fn fn = nullptr;
    if (!fn) fn = reinterpret_cast<Fn>(ResolveAddr(Module::Runtime, 0x1800c7a50));
    return fn ? fn(size, align) : nullptr;
}
void MemFreeLocked(void* p) {
    using Fn = void (*)(void*);
    static Fn fn = nullptr;
    if (!fn) fn = reinterpret_cast<Fn>(ResolveAddr(Module::Runtime, 0x1800c7ad0));
    if (fn && p) fn(p);
}

// ---- the allocator's own accounting ----------------------------------------
// `allocCount` (+0x638) is the live-block count and `allocatedBytes` (+0x630)
// the live byte total; both are pinned in runtime/types/system/MemoryAllocator.h.
// Reading them across a call is how this file SEES A FREE, which is the one
// thing ClearPersistState's shadow is structurally unable to observe.
struct AllocCounters { int64_t bytes; int32_t blocks; bool valid; };

AllocCounters ReadCounters() {
    AllocCounters c{0, 0, false};
    void** slot = RT_DATA_SLOT(void**, "?_singleton@MemoryAllocator@System@@0PEAV12@EA");
    if (!slot || !*slot) return c;
    uint8_t* a = static_cast<uint8_t*>(*slot);
    std::memcpy(&c.bytes,  a + 0x630, sizeof(c.bytes));
    std::memcpy(&c.blocks, a + 0x638, sizeof(c.blocks));
    c.valid = true;
    return c;
}

// ---- the subject -----------------------------------------------------------
// Only the two arrays are touched by either body, so only they are built.
// `random` at +0x20 is left zeroed: neither body reads it.
RtObject::InternalFactory* g_subject   = nullptr;
RtObject::Internal**       g_objBuffer = nullptr;

bool BuildSubject() {
    if (g_subject) return true;
    void* mem = std::calloc(1, sizeof(RtObject::InternalFactory));
    if (!mem) return false;
    g_subject = static_cast<RtObject::InternalFactory*>(mem);

    // _objects: N live entries with capacity == size, so Reserve(N) is a no-op
    // and this array does not move during the test. The pointers are never
    // dereferenced by either body.
    g_objBuffer = static_cast<RtObject::Internal**>(
        std::calloc(kObjCount, sizeof(RtObject::Internal*)));
    if (!g_objBuffer) return false;
    g_subject->objects._data     = g_objBuffer;
    g_subject->objects._size     = kObjCount;
    g_subject->objects._capacity = kObjCount;
    return true;
}

// Reset `_persistIds` to the pristine template, FREEING whatever the previous
// leg left there through the same allocator that produced it.
void RestorePristine() {
    if (!g_subject) return;
    if (g_subject->persistIds._data) MemFreeLocked(g_subject->persistIds._data);
    g_subject->persistIds._data     = nullptr;
    g_subject->persistIds._size     = 0;
    g_subject->persistIds._capacity = 0;
    g_subject->objects._data        = g_objBuffer;
    g_subject->objects._size        = kObjCount;
    g_subject->objects._capacity    = kObjCount;
}

// ---- post-state snapshots, one per leg -------------------------------------
struct Post {
    int32_t pSize, pCap, oSize, oCap;
    bool    allMinusOne;
    bool    taken;
};
Post g_post[2];
int  g_side = 0;

void SnapshotPost() {
    Post& p = g_post[g_side];
    p.pSize = g_subject->persistIds._size;
    p.pCap  = g_subject->persistIds._capacity;
    p.oSize = g_subject->objects._size;
    p.oCap  = g_subject->objects._capacity;
    p.allMinusOne = true;
    for (int i = 0; i < p.pSize; ++i)
        if (g_subject->persistIds._data[i] != -1) { p.allMinusOne = false; break; }
    p.taken = true;
}

// ---- negative controls -----------------------------------------------------
//   1  ClearPersistState with the MemFree omitted   -> the counter check must
//                                                      catch it (and ONLY that
//                                                      check can)
//   3  ResetPersistState filling with 0 instead of -1 -> the post-state check
//                                                        must catch it
//
// The numbering is deliberately not 1,2: a control for the encoding-factory
// SLOT choice (+0x18 read vs +0x20 write) would have to be injected inside the
// port body, and this file does not edit the subject it measures. That screen
// is left unexercised by a control and the report says which screen each
// control validated, rather than scoring the union
// (reference_union_control_hides_a_blind_screen.md).
// Each control corrupts a value the corresponding screen ACTUALLY COMPARES —
// reference_control_must_pick_a_compared_subject.md is about a control that did
// not, reported NOT CAUGHT, and indicted a sound instrument.
int g_negCtrl = 0;

void ClearBody_Control1(RtObject::InternalFactory* self) {
    // Identical to the port EXCEPT the free. Post-state is byte-identical, so
    // every field check in the tree passes; only the allocator counters move.
    self->persistIds._data     = nullptr;
    self->persistIds._size     = 0;
    self->persistIds._capacity = 0;
}

// ---- adapters for RtSaveTrace::Compare -------------------------------------
// `Compare` hands `(obj, Serializer*)`. `Serializer+0x00` IS the `Stream*`
// (runtime/types/io/Serializer.h, pinned by the ctor at 0x1800b9960), which is
// what both bodies take. Nothing else about the Serializer is used.
void* StreamOf(void* ser) { return *reinterpret_cast<void**>(ser); }

void ResetOurs(void* obj, void* ser) {
    g_side = 0;
    RestorePristine();
    RtObject::RtObject_InternalFactory_ResetPersistState(
        static_cast<RtObject::InternalFactory*>(obj), StreamOf(ser));
    if (g_negCtrl == 3)
        for (int i = 0; i < g_subject->persistIds._size; ++i)
            g_subject->persistIds._data[i] = 0;
    SnapshotPost();
}

void ResetTheirs(void* obj, void* ser) {
    g_side = 1;
    RestorePristine();
    TheirReset()(static_cast<RtObject::InternalFactory*>(obj), StreamOf(ser));
    SnapshotPost();
}

// ============================================================================
// A/B 1 — ClearPersistState
// ============================================================================
// Not run through `RtSaveTrace::Compare`: this body emits nothing, so an
// emission oracle would compare two empty sequences and report green forever
// (reference_oracle_must_not_compare_unwritten_bytes.md is the same shape from
// the other direction). What it does have is a SIDE EFFECT and three fields,
// and both are compared here.
bool RunClearAB() {
    if (!BuildSubject()) { printf("[PersistAB] Clear: subject alloc FAILED\n"); return false; }
    if (!TheirClear())   { printf("[PersistAB] Clear: shipped export NOT RESOLVED — "
                                       "no oracle, and this run proves nothing\n"); return false; }

    bool ok = true;
    Post p[2];
    AllocCounters d[2];

    for (int side = 0; side < 2; ++side) {
        RestorePristine();
        // Give the subject a real block, from the allocator whose counters we
        // are about to read, so a free is visible as a -1 on `blocks`.
        void* blk = MemAllocLocked(kIntArrBytes, 4);
        if (!blk) { printf("[PersistAB] Clear: MemAlloc FAILED — no subject\n"); return false; }
        g_subject->persistIds._data     = static_cast<int32_t*>(blk);
        g_subject->persistIds._size     = 16;
        g_subject->persistIds._capacity = 16;

        const AllocCounters before = ReadCounters();
        if (side == 0) {
            if (g_negCtrl == 1) ClearBody_Control1(g_subject);
            else RtObject::RtObject_InternalFactory_ClearPersistState(g_subject);
        } else {
            TheirClear()(g_subject);
        }
        const AllocCounters after = ReadCounters();

        g_side = side; SnapshotPost(); p[side] = g_post[side];
        d[side].valid  = before.valid && after.valid;
        d[side].blocks = after.blocks - before.blocks;
        d[side].bytes  = after.bytes  - before.bytes;
    }

    const bool fieldsEqual = p[0].pSize == p[1].pSize && p[0].pCap == p[1].pCap;
    const bool zeroed      = p[0].pSize == 0 && p[0].pCap == 0 &&
                             g_subject->persistIds._data == nullptr;
    const bool freeMatches = d[0].valid && d[1].valid &&
                             d[0].blocks == d[1].blocks && d[0].bytes == d[1].bytes;
    ok = fieldsEqual && zeroed && freeMatches;

    printf("[PersistAB] ClearPersistState: fields ours{size=%d cap=%d} "
                "theirs{size=%d cap=%d} | allocator delta ours{blocks=%d bytes=%lld} "
                "theirs{blocks=%d bytes=%lld} | %s\n",
                p[0].pSize, p[0].pCap, p[1].pSize, p[1].pCap,
                d[0].blocks, (long long)d[0].bytes,
                d[1].blocks, (long long)d[1].bytes,
                ok ? "IDENTICAL" : "*** DIVERGED ***");
    if (!d[0].valid || !d[1].valid)
        printf("[PersistAB] ClearPersistState: allocator counters UNREADABLE — "
                    "the free half of this comparison did not run.\n");
    return ok;
}

// ============================================================================
// A/B 2 — ResetPersistState
// ============================================================================
bool RunResetAB() {
    if (!BuildSubject()) { printf("[PersistAB] Reset: subject alloc FAILED\n"); return false; }
    if (!TheirReset())   { printf("[PersistAB] Reset: shipped export NOT RESOLVED — "
                                       "no oracle, and this run proves nothing\n"); return false; }
    if (!RtSaveTrace::Enabled()) {
        printf("[PersistAB] Reset: BANISHED_RT_SAVE_TRACE is OFF, so the EMISSION "
                    "half did not run. Set it to get the stream-sequence reading.\n");
        return false;
    }

    g_post[0].taken = g_post[1].taken = false;
    const bool emissionsEqual =
        RtSaveTrace::Compare("Object::InternalFactory::ResetPersistState",
                             g_subject, &ResetOurs, &ResetTheirs, 0);

    bool stateEqual = g_post[0].taken && g_post[1].taken &&
                      g_post[0].pSize == g_post[1].pSize &&
                      g_post[0].pCap  == g_post[1].pCap  &&
                      g_post[0].oSize == g_post[1].oSize &&
                      g_post[0].oCap  == g_post[1].oCap  &&
                      g_post[0].allMinusOne == g_post[1].allMinusOne;

    printf("[PersistAB] ResetPersistState: emissions %s | post-state "
                "ours{pSize=%d pCap=%d oSize=%d oCap=%d fill-1=%d} "
                "theirs{pSize=%d pCap=%d oSize=%d oCap=%d fill-1=%d} | %s\n",
                emissionsEqual ? "IDENTICAL" : "*** DIVERGED ***",
                g_post[0].pSize, g_post[0].pCap, g_post[0].oSize, g_post[0].oCap,
                (int)g_post[0].allMinusOne,
                g_post[1].pSize, g_post[1].pCap, g_post[1].oSize, g_post[1].oCap,
                (int)g_post[1].allMinusOne,
                stateEqual ? "IDENTICAL" : "*** DIVERGED ***");
    // The subject keeps whatever the last leg allocated; hand it back so this
    // probe does not leave a block on the arena the run goes on to use.
    RestorePristine();
    return emissionsEqual && stateEqual;
}

}  // namespace

// ============================================================================
// Entry point — called from RtObjectPortInit's registration path.
// ============================================================================
extern "C" void RtObjectPersistABRun() {
    const char* v = std::getenv("BANISHED_RT_PERSIST_AB");
    if (!v || v[0] == '\0' || v[0] == '0') return;

    printf("[PersistAB] ARMED — synthetic-subject A/B for the two "
                "InternalFactory persist-state rows. The live SHADOW in "
                "internal.cpp is a separate screen; read both.\n");

    if (v[0] == '2') {
        // Controls first, on the same subject, so a control that does not fire
        // invalidates the measurement that follows rather than sitting beside it.
        int caught = 0;
        g_negCtrl = 1; if (!RunClearAB()) ++caught;
        printf("[PersistAB] CONTROL 1 (ClearPersistState with the MemFree removed): %s "
                    "-- and ONLY the allocator-counter check can see it.\n",
                    caught >= 1 ? "CAUGHT" : "*** NOT CAUGHT ***");
        g_negCtrl = 3; const bool c3 = !RunResetAB();
        printf("[PersistAB] CONTROL 3 (ResetPersistState filling 0 instead of -1): %s\n",
                    c3 ? "CAUGHT" : "*** NOT CAUGHT ***");
        g_negCtrl = 0;
        if (caught < 1 || !c3)
            printf("[PersistAB] *** A CONTROL DID NOT FIRE — the green readings below "
                        "prove NOTHING.\n");
    }

    const bool a = RunClearAB();
    const bool b = RunResetAB();
    printf("[PersistAB] VERDICT: ClearPersistState=%s ResetPersistState=%s\n",
                a ? "MATCHES SHIPPED" : "see above",
                b ? "MATCHES SHIPPED" : "see above");
}
