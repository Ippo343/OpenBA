// ============================================================================
// runtime/src/object/external_find.cpp — Wave 9 lane RRES, follow-on rung
//   `?Find@ExternalFactory@Object@@AEAAPEAVExternal@2@AEBVFilename@System@@@Z`
//   retail 0x1800be0e0, 165 instructions.
// ============================================================================
//
// The private cache lookup. `FindOrLoad` (0x1800bf320) calls it once, at
// 0x1800bf366, and it is the ONLY caller in the DLL.
//
// WHY THIS ROW IS CHEAP NOW AND WAS NOT BEFORE
//
// Its closure is complete and was completed by two other lanes, not this one:
// `MemFree`, `AdjustDelimiters`, `GetPathIdentifer` and
// `String::operator+(const char*)` were already ours, and `0x18002dd70` — the
// 0x18-element `Collection::Set::Find` this body descends — is lane RES's
// (`runtime/src/object/external_set.cpp`). **Nothing new is opened by this
// file.** That is the whole reason it was picked: WAVE9_LANE_RRES.md §6 named
// it "the cheapest next rung by a wide margin" and this is that rung.
//
// PORTABLE: no <windows.h> (PLAN.md §4.1 decision 1). The detour seam lives in
// `standalone/res_path_detour.cpp` with the one this lane already shipped.
//
// ---------------------------------------------------------------------------
// THE TRAP THIS BODY EXISTS TO EXPLAIN — lane RES's finding, now with its cause
// ---------------------------------------------------------------------------
// `WAVE9_LANE_RES.md` §2 recorded that `Find` at `0x18002dd70` **requires a
// delimiter-adjusted key**: a lookup with a raw `/` path silently misses, and
// misses identically in the shipped body, because `Filename`'s copy
// constructor adjusts delimiters on the way INTO the node and nothing adjusts
// the search key. Lane RES paid one run to discover it and wrote that "retail
// guarantees the adjustment at every call site — `ExternalFactory`'s own lookup
// at 0x1800be0e0 calls `AdjustDelimiters` FOUR times before it descends."
//
// This is that function, and the four calls are now accounted for individually.
// They are TWO PAIRS, and each pair is one `Filename x = y` assignment:
// `Filename::operator=` (0x180006770) already ends in `AdjustDelimiters`, and
// retail then calls it AGAIN, explicitly, on the same object:
//
//     0x1800be140 / 0x1800be145   on `key`   after `key = path`
//     0x1800be244 / 0x1800be24d   on `probe` after `probe = key`
//
// So the "four calls" are not four normalisations of four different strings.
// They are two assignments each followed by one redundant re-normalisation, and
// `AdjustDelimiters` is idempotent, so three of the four are no-ops. All four
// are transcribed anyway: the cost is a function call on an already-normalised
// buffer, and dropping them would make this body stop matching the disassembly
// for no benefit.
//
// ---------------------------------------------------------------------------
// THE SECOND COPY IS ALSO REDUNDANT, AND IS ALSO KEPT
// ---------------------------------------------------------------------------
// `probe` ([RBP+0x30], 0x1800be1fe onward) is a fresh `Filename` that is
// assigned from `key` and then handed to `Set::Find`. `key` was already
// normalised and would have served. Retail builds the copy; so does this port.
// A tidied version would be observably identical and would lose the property
// that makes this file checkable against the listing.
//
// ---------------------------------------------------------------------------
// RETAIL USES ITS CALLER'S SHADOW SPACE AS SCRATCH
// ---------------------------------------------------------------------------
// `MOV RBP,RSP` sits after four pushes, so `[RBP+0x20]` is the return address
// and `[RBP+0x28]` / `[RBP+0x30]` / `[RBP+0x38]` — the three `Filename` locals
// — are the CALLER's 32-byte home space. That is legal in the MS x64 ABI and it
// matters for one reason only: when the diff replays the original through the
// detour trampoline, the original scribbles on THIS function's own home space,
// which the compiler has already reserved for the call. Noted because a reader
// checking offsets against the listing will otherwise read `[RBP+0x28]` as an
// argument and it is not one.
// ============================================================================

#include <cstdio>
#include <cstdlib>

#include "external_find.h"
#include "external_set.h"

#include "../../funcs/rt_port.h"
#include "../../funcs/rt_diff.h"
#include "../../types/system/String.h"
#include "../system/string.h"

// Declared locally rather than including standalone/resolve.h, which drags in
// <windows.h> — the same reason external_set_probe.cpp:99 declares it.
extern "C" bool  RtResFindInstallDetour(void* ourFn);
extern "C" void* RtResFindTrampoline();

namespace RtSystem {
void      RtSystem_Filename_AdjustDelimiters(Filename* self);              // 0x1800c6970
Filename* RtSystem_Filename_GetPathIdentifer(const Filename* self, Filename* ret);
                                                                           // 0x1800c6610
}  // namespace RtSystem

namespace RtObject {

using RtSystem::Filename;
using RtSystem::String;
using RtSystem::RtString_AdoptInline;
using RtSystem::RtString_ReleaseInline;
using RtSystem::RtSystem_String_ConcatCStr;
using RtSystem::RtSystem_Filename_AdjustDelimiters;
using RtSystem::RtSystem_Filename_GetPathIdentifer;

FindCounters g_fc = {};

namespace {

inline String*       AsStr(Filename* f)       { return reinterpret_cast<String*>(f); }
inline const String* AsStr(const Filename* f) { return reinterpret_cast<const String*>(f); }

inline int32_t BufLength(const char* d) {
    return reinterpret_cast<const int32_t*>(d)[-2];
}

// Retail's inlined `Filename::operator=`: the `!=` guard is its own, and
// skipping the whole block on a self-assign is what keeps the refcount from
// reaching zero mid-assignment.
inline void ReleaseThenAdopt(Filename* dst, char* src) {
    if (dst->data == src) return;
    RtString_ReleaseInline(AsStr(dst));
    RtString_AdoptInline(AsStr(dst), src);
}

// 0x180155ae0. Read out of .rdata, not guessed:
//   180155ae0: 3a 72 65 73 6f 75 72 63 65 00   ":resource"
// The neighbouring literal at 0x180155af0 is the bare "resource" that
// `ExpandRelativePath` uses for the same defaulting decision — two different
// strings for the same concept, four bytes apart, and using the wrong one here
// would produce a key that never matches anything the factory inserted.
const char* const kResourceSuffix = ":resource";

// ---------------------------------------------------------------------------
// THE NEGATIVE CONTROLS. Three, and every one of them is reachable on any call
// that finds something — which is 22.5k of 22.6k calls in a normal boot. Lane
// RRES's previous rung shipped two controls that could not fire; these were
// chosen by asking FIRST what makes them fire.
//
//   1  append ":resources" instead of ":resource"  -> the defaulted-identifier
//      lookups miss. Fires on `identDefaulted` calls only.
//   2  descend on the UNSUFFIXED key               -> same population, other
//      direction.
//   3  return the NODE instead of node->external   -> wrong pointer on EVERY
//      hit. This one cannot fail to fire if anything is ever found.
// ---------------------------------------------------------------------------
int g_negCtl = 0;

}  // namespace

// ============================================================================
// ?Find@ExternalFactory@Object@@AEAAPEAVExternal@2@AEBVFilename@System@@@Z
//                                                          retail 0x1800be0e0
//
//     External* ExternalFactory::Find(const Filename& path)
//
// AEAA — PRIVATE, and private is not the same as unexported: this DLL exports
// its private members and the name above is ordinal 2336 in the retail export
// directory (reference_aeaa_is_private_not_unexported.md). Verified with
// pefile before this body was written, not inferred from the mangling.
// ============================================================================
External* RtObject_ExternalFactory_Find(ExternalFactory* self, const Filename* path) {
    RT_DIFF_BEGIN("Object::ExternalFactory::Find");
    ++g_fc.calls;

    const int ctl = g_negCtl;

    // ---- 0x1800be0fc-0x1800be149: `Filename key = path`, then adjust again --
    Filename key{nullptr};                               // [RBP+0x28]
    ReleaseThenAdopt(&key, path->data);
    RtSystem_Filename_AdjustDelimiters(&key);            // 0x1800be140 (op= 's own)
    RtSystem_Filename_AdjustDelimiters(&key);            // 0x1800be145 (retail's extra)

    // ---- 0x1800be14f: the identifier comes from `path`, NOT from `key` ------
    // Retail passes RBX (the argument) here, not the normalised copy. It cannot
    // matter — AdjustDelimiters does not touch ':' — but it is transcribed the
    // way it is written, because "it cannot matter" is how a port acquires a
    // difference nobody can find later.
    Filename ident{nullptr};                             // [RBP+0x38]
    RtSystem_Filename_GetPathIdentifer(path, &ident);

    // ---- 0x1800be15c-0x1800be1f6: no identifier -> append ":resource" -------
    if (ident.data == nullptr || BufLength(ident.data) == 0) {
        ++g_fc.identDefaulted;
        String tmp{nullptr};                             // [RBP+0x30]
        RtSystem_String_ConcatCStr(&tmp, AsStr(&key),
                                   ctl == 1 ? ":resources" : kResourceSuffix);
        ReleaseThenAdopt(&key, tmp.data);                // 0x1800be186
        RtSystem_Filename_AdjustDelimiters(&key);        // 0x1800be1c7
        RtString_ReleaseInline(&tmp);                    // 0x1800be1cd
    } else {
        ++g_fc.identPresent;
    }

    // ---- 0x1800be1f6-0x1800be251: `Filename probe = key`, adjusted twice ----
    // The redundant second copy; see the file header.
    Filename probe{nullptr};                             // [RBP+0x30], reused
    ReleaseThenAdopt(&probe, key.data);
    RtSystem_Filename_AdjustDelimiters(&probe);          // 0x1800be244
    RtSystem_Filename_AdjustDelimiters(&probe);          // 0x1800be24d

    // ---- 0x1800be257: descend `this->_resources` (this + 0x10) -------------
    rtsetavl24::Set* const tree =
        reinterpret_cast<rtsetavl24::Set*>(&self->_resources);
    rtsetavl24::Node* const node =
        rtresset::Find(tree, ctl == 2 ? &key : &probe);

    RtString_ReleaseInline(AsStr(&probe));               // 0x1800be26f

    // ---- 0x1800be298: `return node ? node->element.external : nullptr` ------
    // `MOV RBX,qword ptr [RBX + 0x8]` — ResourceEntry::external at +0x08, which
    // is what External.h asserts. THE DEREF IS UNGUARDED IN RETAIL BEYOND THE
    // null test, and it stays that way here: a node in this tree always carries
    // an External, and a guard would hide the desync rather than survive it.
    External* result = nullptr;
    if (node != nullptr) {
        ++g_fc.hits;
        result = (ctl == 3) ? reinterpret_cast<External*>(node)
                            : rtresset::Elem(node)->external;
    } else {
        ++g_fc.misses;
    }

    RtString_ReleaseInline(AsStr(&ident));               // 0x1800be2a6
    RtString_ReleaseInline(AsStr(&key));                 // 0x1800be2cf

    // ---- the oracle --------------------------------------------------------
    // READ-ONLY on the tree and on the factory: it allocates and releases only
    // its own three `Filename`s and returns a pointer it did not create. So the
    // original can be replayed on the same arguments, and the comparison is
    // POINTER EQUALITY — not text, not bytes. That is a stronger predicate than
    // anything the previous rung had: two independent descents of the same live
    // AVL tree must arrive at the same node or the port is wrong.
    //
    // `theirs` is the DETOUR TRAMPOLINE. With the detour installed,
    // `GetProcAddress` on this name returns the PATCHED address and an
    // "original" call would re-enter our own body forever
    // (reference_rtoriginal_is_getprocaddress.md). With no detour there is no
    // route to the original at all, and the site correctly makes NO comparison
    // rather than comparing ours to ours.
    if (rt_diff_on__) {
        typedef External* (*FindFn)(ExternalFactory*, const Filename*);
        FindFn theirs = reinterpret_cast<FindFn>(RtResFindTrampoline());
        if (theirs) {
            External* mirror = theirs(self, path);
            RT_DIFF_SHADOW(mirror == result, result ? "ours=<ptr>" : "ours=(null)",
                           mirror ? "theirs=<ptr>" : "theirs=(null)");
            if (mirror != result) ++g_fc.divergent;
            RT_DIFF_WORK(true);
        }
    }
    return result;
}

namespace {

void DumpFindCounters() {
    std::printf("[ResFind] counters: calls=%llu hits=%llu misses=%llu "
                "| ident def/present=%llu/%llu | pointer disagreements=%llu\n",
                (unsigned long long)g_fc.calls,
                (unsigned long long)g_fc.hits,
                (unsigned long long)g_fc.misses,
                (unsigned long long)g_fc.identDefaulted,
                (unsigned long long)g_fc.identPresent,
                (unsigned long long)g_fc.divergent);
    std::fflush(stdout);
}

}  // namespace

}  // namespace RtObject

// ============================================================================
// Registration
// ============================================================================
int RtObjectExternalFindRegister() {
    using namespace RtObject;
    int n = 0;

    // Literal mangled name, not a constant: tools/runtime_census.py matches
    // `RegisterRuntimeExport("<literal>"` and cannot see through a named
    // constant (reference_census_cannot_see_nonliteral_registrations.md).
    RegisterRuntimeExport(
        "?Find@ExternalFactory@Object@@AEAAPEAVExternal@2@AEBVFilename@System@@@Z",
        (void*)&RtObject_ExternalFactory_Find); ++n;
    RegisterRuntimeAddr(0x1800be0e0, (void*)&RtObject_ExternalFactory_Find);

    // The live-coverage seam. OPT-IN, exactly as the ExpandRelativePath detour
    // is: this body's ONLY caller is retail's own `FindOrLoad` through a direct
    // rel32 CALL, so registration alone leaves it with zero callers and an
    // honest `calls=0`.
    bool detoured = false;
    if (RtWaveEnabled("BANISHED_RT_RESFIND_DETOUR") &&
        std::getenv("BANISHED_RT_RESFIND_DETOUR") != nullptr) {
        detoured = RtResFindInstallDetour((void*)&RtObject_ExternalFactory_Find);
    }

    if (const char* e = std::getenv("BANISHED_RT_RESFIND_NEGCTL"))
        g_negCtl = std::atoi(e);

    std::printf("[ResFind] registered %d door + 1 address (detour=%s negctl=%d)\n",
                n, detoured ? "ON" : "off", g_negCtl);
    std::fflush(stdout);

    std::atexit(&DumpFindCounters);
    return n;
}
