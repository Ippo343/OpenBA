// ============================================================================
// runtime/src/object/external_reftable_probe.cpp — Wave 11 follow-up 3, lane AVL
//   The WRITE-PATH oracle for ExternalFactory::SerializeReferenceTable.
// ============================================================================
//
// "NO ORACLE POSSIBLE" WAS WRONG FOR THIS FAMILY FOUR TIMES ACROSS THREE WAVES,
// and this subject is the one `runtime/src/io/save_trace.h` names first among
// the four it was built for. **On the WRITE path a Serialize is a read of the
// object**: it walks `_resources` and emits, and calling ours and then theirs
// IS a legitimate replay provided the two calls emit into DIFFERENT SINKS.
// That is what `RtSaveTrace::Compare` provides, and this file is the four lines
// of glue plus the three things that are specific to this subject.
//
// ---------------------------------------------------------------------------
// 1. THE ADAPTER — the second parameter is a `Stream&`, not a `Serializer&`
// ---------------------------------------------------------------------------
// `Compare` hands each side `&sink.serializer`, because its first subject
// (`Audio::Scene::Serialize`) takes an `IO::Serializer&`. This subject takes an
// `IO::Stream&`: the mangled name says `AEAVStream@IO@@` and the body reads the
// parameter at +0x08 (direction) and +0x10 (encoding), which are Stream fields.
// Passing the Serializer straight through would read 16 bytes past the end of a
// 16-byte object. `Serializer+0x00` IS the `Stream*`, so `StreamOf` recovers
// it. This is not a workaround for a bug in lane S; it is the two calling
// conventions this family genuinely has, and goal_serialize.cpp records the
// same adapter for the same reason.
//
// ---------------------------------------------------------------------------
// 2. NO PREFIX STOP — measured, not assumed
// ---------------------------------------------------------------------------
// `WAVE8_5.md` proposed running this subject behind lane S's `stopAfter`.
// `docs/runtime_port/WAVE8_5_PREFIX_MEASUREMENT.md` measured what that would
// buy and the answer was **2 emissions out of roughly 3 + 3N ≈ 17,000, or
// 0.012%** — and the two are the codec factory and the entry count, i.e. a
// header check wearing an oracle's clothes. It also found the premise wrong in
// our favour: the write path's only mutations are the `_references` teardown
// (whose second run reads an already-null root and returns immediately — NO
// DOUBLE FREE) and `ResourceEntry::referenceId = id`, which a replay rewrites
// with the IDENTICAL value into the identical node, because the ids run 1..N in
// in-order key order over a tree neither call modifies.
//
// So `stopAfter` is 0 here and the FULL emission sequence is compared. That is
// a measured decision with a citation, not a default.
//
// ---------------------------------------------------------------------------
// 3. THE TEARDOWN IS INVISIBLE TO AN EMISSION COMPARE — stated, not hidden
// ---------------------------------------------------------------------------
// MUTATION A emits nothing, so a port that dropped it, or freed the wrong slot,
// would pass a perfectly clean emission trace. The measurement asked whoever
// ported this to cover it with an explicit post-condition rather than trusting
// the trace, and `NoteTeardown` below is that check: it records the tree size
// on entry and asserts root and count are zero afterwards.
//
// **What that check can and cannot say.** It can say the body called a teardown
// with the right set pointer and zeroed the header. It CANNOT say the teardown
// released the right slot — that is `0x18002ebf0`'s own property, ported by
// Wave 11 lane OBJLIFE and verified by `factory_dtors_probe.cpp`, which is the
// probe that knows `+0x08` is the released slot and `+0x00` is its sibling's.
// This file does not re-verify it and does not claim to.
//
// ---------------------------------------------------------------------------
// 4. WHY THE PROBE RUNS ON THE LOAD CALL TOO
// ---------------------------------------------------------------------------
// The sink is always in the WRITING direction, so the compared body is always
// the write path — including when the real call that triggered it is a load.
// That is deliberate, because our harness has an `AUTOLOAD` route and no
// unattended `save` route, and "no gameplay route" is not "no oracle"
// (reference_no_route_is_not_no_oracle.md).
//
// THE PRICE, stated plainly because it is a real mutation of live state: on a
// LOAD, the two probe passes stamp `referenceId = 1..N` into `_resources`
// before the real read path runs, and the read path only re-stamps the entries
// the save actually mentions. A resource present in memory but absent from the
// save therefore keeps a rank number instead of whatever it had. The only
// reader of that field is `GetReferenceId`, consumed by `ExternalPtr::
// Serialize` on the SAVE path — which re-stamps every entry from scratch before
// it reads any of them. So the window is closed, but it is a window, and it is
// why this whole file is gated on `BANISHED_RT_SAVE_TRACE` and does nothing
// otherwise.
//
// PORTABILITY: no <windows.h>, per PLAN.md §4.1 decision 1.
// ============================================================================

#include <cstdint>
#include <cstdio>
#include <cstdlib>

#include "../../funcs/rt_port.h"
#include "../../types/io/Serializer.h"
#include "../../types/object/External.h"
#include "../collection/set_avl.h"
#include "../io/save_trace.h"

namespace RtObject {
void RtObject_ExternalFactory_SerializeReferenceTable(ExternalFactory* self,
                                                      rt::io::Stream* stream);
}

namespace {

using rt::io::Serializer;
using rt::io::Stream;
using RtObject::ExternalFactory;

constexpr const char* kMangled =
    "?SerializeReferenceTable@ExternalFactory@Object@@QEAAXAEAVStream@IO@@@Z";

using StreamMemberFn = void (*)(void*, Stream*);

Stream* StreamOf(void* serializer) {
    return static_cast<Serializer*>(serializer)->stream;
}

// Re-entrancy guard, and it is not optional. `Compare` calls `ours`, which is
// the very body that called this probe; a counter alone would let each nested
// entry start ANOTHER comparison, and `Compare` says in as many words that it
// is not reentrant (its sinks and recorders are function-level statics shared
// by both sides). One flag, set for the duration, is what makes the nested call
// fall straight through to the real work.
bool g_probing = false;

struct ProbeGuard {
    ProbeGuard()  { g_probing = true; }
    ~ProbeGuard() { g_probing = false; }
};

// `Compare` defends against `ours == theirs` — the vacuous pass
// reference_uncalled_port_is_not_verified.md is about — by comparing the two
// function pointers. Adapters defeat that check, so the equivalent check is
// made here instead, on the thing that actually matters: whether the export
// registry hands OUR body back for this name.
bool PortIsLive() {
    void* viaRegistry = ResolveExport(kMangled);
    void* ours = (void*)&RtObject::RtObject_ExternalFactory_SerializeReferenceTable;
    if (viaRegistry == ours) return true;
    std::fprintf(stderr,
                 "[RtSaveTrace] FAIL %s: the port is NOT registered (registry -> "
                 "%p, ours %p), so this comparison would prove nothing\n",
                 kMangled, viaRegistry, ours);
    return false;
}

int  g_probes      = 0;
int  g_teardownOk  = 0;
int  g_teardownBad = 0;
int  g_nodesFreed  = 0;

int CountNodes(const rtsetavl::Node* n) {
    return n ? 1 + CountNodes(n->left) + CountNodes(n->right) : 0;
}

}  // namespace

// ---------------------------------------------------------------------------
// The post-condition check for MUTATION A. Called from the ported body right
// after the teardown and the two zero stores.
// ---------------------------------------------------------------------------
void RtObjectExternalRefTableNoteTeardown(const void* setBefore, int nodesBefore) {
    const rtsetavl::Set* s = static_cast<const rtsetavl::Set*>(setBefore);
    g_nodesFreed += nodesBefore;
    if (s->root == nullptr && s->count == 0) ++g_teardownOk;
    else {
        ++g_teardownBad;
        std::fprintf(stderr,
            "[RefTable] TEARDOWN POST-CONDITION FAILED: root=%p count=%d after "
            "MUTATION A (%d nodes on entry)\n",
            (void*)s->root, static_cast<int>(s->count), nodesBefore);
    }
}

int RtObjectExternalRefTableCountNodes(const void* set) {
    return CountNodes(static_cast<const rtsetavl::Set*>(set)->root);
}

void RtObjectExternalRefTableProbeMaybeRun(ExternalFactory* self, Stream* stream) {
    (void)stream;
    if (!RtSaveTrace::Enabled() || g_probing || g_probes >= 8) return;
    ++g_probes;

    ProbeGuard guard__;   // Compare re-enters the body; this makes it skip.

    auto theirs = reinterpret_cast<StreamMemberFn>(ResolveExportRaw(kMangled));
    if (!theirs || !PortIsLive()) return;

    RtSaveTrace::Compare(
        "Object::ExternalFactory::SerializeReferenceTable", self,
        [](void* obj, void* ser) {
            RtObject::RtObject_ExternalFactory_SerializeReferenceTable(
                static_cast<ExternalFactory*>(obj), StreamOf(ser));
        },
        [](void* obj, void* ser) {
            reinterpret_cast<StreamMemberFn>(ResolveExportRaw(kMangled))(
                obj, StreamOf(ser));
        },
        /*stopAfter=*/0);   // measured; see the banner
}

void RtObjectExternalRefTableProbeSummary() {
    if (!RtSaveTrace::Enabled()) return;
    std::fprintf(stderr,
        "[RefTable] probe: comparisons=%d  teardown post-condition ok=%d bad=%d  "
        "nodes present at entry across all calls=%d\n",
        g_probes, g_teardownOk, g_teardownBad, g_nodesFreed);
    if (g_probes == 0)
        std::fprintf(stderr,
            "[RefTable] probe: ZERO comparisons -- the body never ran with "
            "BANISHED_RT_SAVE_TRACE set. That is not a pass.\n");
}
