// ============================================================================
// runtime/src/object/external_set_probe.cpp — Wave 9 lane RES
//   The lockstep synthetic-subject oracle for the twelve
//   `Collection::Set<ResourceEntry>` bodies in external_set.cpp.
// ============================================================================
//
// WHY A PROBE AND NOT A LIVE DIFF
// -------------------------------
// Every body under test MUTATES THE ONE SHARED FACTORY TREE. Running ours and
// then the shipped one at a live call site inserts the same key twice — and
// retail's second insert RETURNS EARLY (0x18002cf36), making the comparison
// vacuously green. That is the exact trap lane D recorded building
// `KeyMap::RegisterInput`'s oracle, and it is the reason Wave 3 wrote this
// family off as "no oracle possible". It is not: the subject does not have to
// be the live tree.
//
// So: two PRIVATE `Set`s with byte-identical starting state, the same script
// applied to both, ours through the ported bodies and theirs through the
// shipped ones (reference_synthetic_subject_beats_a_fake.md).
//
// HOW EACH SIDE REACHES ITS OWN BODIES
// ------------------------------------
//   ours   : the `rtresset::` functions, called directly.
//   theirs : `ResolveRuntimeRaw(0x18002ca00)` / `(0x18002cf60)` /
//            `(0x18002dd70)`. RAW, never `ResolveAddr` — `ResolveAddr` consults
//            the PORT REGISTRY FIRST and would hand back our own body, so the
//            probe would compare ours to ours and always agree
//            (reference_resolveaddr_consults_port_registry_first.md). That is
//            the same mistake lane FNHOOK found in thirteen detour installers.
//
// The shipped insert and erase call their own retrace and rotations as NEAR
// calls inside the DLL, so nothing of ours can intercept them: driving the
// shipped 0x18002ca00 exercises the shipped 0x18002ce60 / 0x18002d370 /
// 0x18002d860 / 0x18002d990 / 0x18002e740 / 0x18002e6e0 as one unit. Six of the
// twelve bodies are therefore covered TRANSITIVELY, and that is stated rather
// than implied. Two are NOT covered by this probe at all — `ArrayResize` and
// `ArrayDestroyRange`, whose only caller is `Clean` — and that limit is in the
// lane doc's HONEST LIMITS section, not buried here.
//
// WHY LOCKSTEP AND NOT A FINAL COMPARISON
// ---------------------------------------
// **AVL defects cancel.** A rotation wrong in one direction and wrong again in
// the other leaves the same key set and frequently the same final shape while
// every intermediate state differed. Both trees are fingerprinted and compared
// AFTER EVERY STEP, and the fingerprint carries SHAPE — which side of its
// parent each node hangs off, its balance and its height — not just the keys.
// Parent linkage is checked separately, because a fingerprint built by walking
// left/right cannot see a wrong `parent`, and a wrong parent is exactly what a
// mis-transcribed rotation produces and exactly what breaks the NEXT retrace.
//
// THE ELEMENT IS COMPARED TOO, unlike lane INPUT's E24 probe. Five bodies here
// read or write it, so a `PairCtor` that copied the wrong qword would be
// invisible to a shape-only check. What is NOT compared is the `Filename`
// POINTER — the two sides allocate their own strings and the addresses differ
// by construction. The STRING CONTENTS are compared instead, which is the
// observable thing (reference_oracle_must_not_compare_unwritten_bytes.md:
// never compare bytes neither side controls).
//
// THE POPULATION IS ADVERSARIAL ON PURPOSE
// ----------------------------------------
// A sorted or a random population exercises one rotation shape and never the
// pre-rotation. The script below is built to force all four rebalance arms, the
// equal-key early-out, both null-key arms of the comparator, the case-folding
// path of `_stricmp`, and all three erase cases INCLUDING the two-child
// predecessor splice. The EFFECTIVENESS CONTROL is `rtresset::g_rc` — counters
// that live INSIDE the bodies under test, so "the probe ran N steps" cannot be
// mistaken for "the probe exercised the rotations". If a counter the script is
// designed to drive comes back ZERO, the probe SAYS SO and reports UNDERPOWERED
// rather than PASS (reference_probe_needs_an_effectiveness_control.md).
//
// NEGATIVE CONTROLS. Four deliberate defects, applied one at a time to our
// side's finished tree, each of which MUST make the comparison fail. A control
// that cannot fire is worse than none — lane COLL caught one of its own this
// way — so every control's firing is reported, and a control that MISSES is
// reported as a probe defect rather than explained away.
//
// WHEN IT RUNS
// ------------
// Self-scheduled on the first real call into `rtresset::Insert` or
// `ExternalFactory::Clean`, NOT from `RtObjectPortInit`. It allocates through
// `System::MemoryAllocator`, and at port-init time that allocator has been
// CONSTRUCTED but not STARTED — a probe that allocates from a `*PortInit`
// dereferences a null `_singleton` and faults at `+0x640`, which is lane PKG2's
// recorded failure and is invisible in the crash log.
//
// Opt-in: `BANISHED_RT_RESSET_PROBE=1`.
// ============================================================================

#include <cstdint>
#include <cstdio>
#include <cstring>

#include "external_set.h"
#include "../../funcs/rt_port.h"
#include "../system/string.h"

// Declared locally rather than including standalone/resolve.h, which drags in
// <windows.h> (PLAN 4.1 decision 1).
uintptr_t ResolveRuntimeRaw(uintptr_t ghidra_addr);

// Ported in Wave 2 (runtime/src/system/filename.cpp), registered at 0x1800c6970.
namespace RtSystem { void RtSystem_Filename_AdjustDelimiters(Filename* self); }

namespace {

using rtresset::Node;
using rtresset::Set;
using rtresset::Elem;
using rtresset::EntryValue;
using RtObject::ResourceEntry;
using RtSystem::Filename;

bool g_armed   = false;
bool g_running = false;

// The shipped entry points, by ADDRESS and RAW.
using InsertFn = Node** (*)(Set*, Node**, const Filename*, const EntryValue*);
using EraseFn  = void   (*)(Set*, Node*);
using FindFn   = Node*  (*)(const Set*, const Filename*);

// ---------------------------------------------------------------------------
// The fingerprint. A PRE-ORDER walk that emits an explicit marker for a null
// child, so two different shapes cannot collide. Carries balance and height
// because a wrong Recompute leaves the links right and the metadata wrong,
// which the NEXT insert then acts on.
// ---------------------------------------------------------------------------
struct Fp {
    char buf[16384];
    int  len;
    void put(const char* s) {
        while (*s && len < static_cast<int>(sizeof(buf)) - 1) buf[len++] = *s++;
        buf[len] = 0;
    }
};

void Emit(Fp& fp, const Node* n, char side) {
    char tmp[320];
    if (n == nullptr) { std::snprintf(tmp, sizeof tmp, "%c.", side); fp.put(tmp); return; }
    const ResourceEntry* e = Elem(n);
    std::snprintf(tmp, sizeof tmp, "%c[%s|%p|%08x|%08x|b%d|h%d]",
                  side,
                  e->path.data ? e->path.data : "<null>",
                  static_cast<const void*>(e->external),
                  e->usageMask, e->referenceId,
                  static_cast<int>(n->balance), static_cast<int>(n->height));
    fp.put(tmp);
    Emit(fp, n->left,  'L');
    Emit(fp, n->right, 'R');
}

void Fingerprint(Fp& fp, const Set* s) {
    char tmp[64];
    fp.len = 0; fp.buf[0] = 0;
    std::snprintf(tmp, sizeof tmp, "n%d:", s->count);
    fp.put(tmp);
    Emit(fp, s->root, 'T');
}

// Parent linkage, checked separately: a left/right walk cannot see it.
bool ParentsOk(const Node* n, const Node* expect) {
    if (n == nullptr) return true;
    if (n->parent != expect) return false;
    return ParentsOk(n->left, n) && ParentsOk(n->right, n);
}

// ---------------------------------------------------------------------------
// The script. Keys are deliberately mixed-case and share prefixes so that
// `_stricmp` does real work: a comparator that returned 0 for every pair
// sharing a first byte passed a whole wave once
// (reference_uniform_population_is_not_a_verification.md).
// ---------------------------------------------------------------------------
const char* const kKeys[] = {
    "Terrain/Grass.rsc",      "terrain/GRASS.rsc",     // equal under _stricmp
    "Terrain/Dirt.rsc",       "Terrain/Rock.rsc",
    "Terrain/Water.rsc",      "Terrain/Snow.rsc",
    "Building/House.rsc",     "Building/Barn.rsc",
    "Building/Well.rsc",      "Building/Market.rsc",
    "AAA", "AAB", "AAC", "AAD", "AAE",                 // right spine -> rotate left
    "zzz", "zzy", "zzx", "zzw", "zzv",                 // left  spine -> rotate right
    "MMM", "MMA", "MMZ", "MMB", "MMY",                 // zig-zag -> pre-rotations
    "Ui/Dialog/Main.rsc",     "Ui/Dialog/Mods.rsc",
    "Ui/Dialog/Goals.rsc",    "Ui/Dialog/Ach.rsc",
    "Material/Stone.rsc",     "Material/Wood.rsc",
    "Material/Iron.rsc",      "Material/Coal.rsc",
    "a", "b", "c", "d", "e", "f", "g", "h",
    "Z", "Y", "X", "W", "V", "U", "T", "S",
};
const int kKeyCount = static_cast<int>(sizeof(kKeys) / sizeof(kKeys[0]));

// Erase order chosen to hit all three cases, including two-child nodes near the
// root (which forces the predecessor splice with `s != z->left`).
const int kEraseOrder[] = {
    0, 11, 40, 20, 2, 34, 25, 6, 15, 44, 3, 22, 30, 8, 41, 17, 27, 5,
};
const int kEraseCount = static_cast<int>(sizeof(kEraseOrder) / sizeof(kEraseOrder[0]));

// ---------------------------------------------------------------------------
// Negative controls. Each returns false when it could NOT apply (tree too
// small, no left child, ...) — a control that never got the chance is reported
// as MISSED, not silently counted as fired.
// ---------------------------------------------------------------------------
bool BreakBalance(Set* s) { if (!s->root) return false; s->root->balance = static_cast<int16_t>(s->root->balance ^ 1); return true; }
bool BreakHeight (Set* s) { if (!s->root) return false; s->root->height  = static_cast<int16_t>(s->root->height  ^ 1); return true; }
bool BreakElement(Set* s) { if (!s->root) return false; Elem(s->root)->referenceId ^= 1u; return true; }

Node* g_savedParent = nullptr;
bool BreakParent(Set* s) {
    if (!s->root || !s->root->left) return false;
    g_savedParent = s->root->left->parent;
    s->root->left->parent = nullptr;
    return true;
}
void RestoreParent(Set* s) { if (s->root && s->root->left) s->root->left->parent = g_savedParent; }

}  // namespace

void RtObjectExternalSetProbeArm(bool on) { g_armed = on; }
bool RtObjectExternalSetProbeArmed()      { return g_armed; }

void RtObjectExternalSetProbeMaybeRun() {
    if (!g_armed || g_running) return;
    g_armed   = false;        // cleared BEFORE anything runs: no re-entry
    g_running = true;
    RtObjectExternalSetProbeRun();
    g_running = false;
}

void RtObjectExternalSetProbeRun() {
    auto theirInsert = reinterpret_cast<InsertFn>(ResolveRuntimeRaw(0x18002ca00));
    auto theirErase  = reinterpret_cast<EraseFn> (ResolveRuntimeRaw(0x18002cf60));
    auto theirFind   = reinterpret_cast<FindFn>  (ResolveRuntimeRaw(0x18002dd70));
    if (!theirInsert || !theirErase || !theirFind) {
        std::printf("[ResSet] SKIP -- the shipped 0x18002ca00/cf60/dd70 are not "
                    "reachable (cutover, or Runtime-x64.dll is absent). "
                    "NO COMPARISON WAS MADE.\n");
        std::fflush(stdout);
        return;
    }

    const rtresset::ProbeCounters before = rtresset::g_rc;

    Set ours   = {nullptr, 0, 0};
    Set theirs = {nullptr, 0, 0};

    int  steps = 0, mismatches = 0, parentFails = 0;
    static Fp fa, fb;                       // static: 32 KB is too much for a frame
    char firstBad[640]; firstBad[0] = 0;

    auto compare = [&](const char* what, int idx) {
        ++steps;
        Fingerprint(fa, &ours);
        Fingerprint(fb, &theirs);
        const bool pOurs   = ParentsOk(ours.root,   nullptr);
        const bool pTheirs = ParentsOk(theirs.root, nullptr);
        if (!pOurs || !pTheirs) {
            ++parentFails;
            if (!firstBad[0])
                std::snprintf(firstBad, sizeof firstBad,
                              "parent linkage broken after %s #%d (ours=%d theirs=%d)",
                              what, idx, pOurs ? 1 : 0, pTheirs ? 1 : 0);
        }
        if (std::strcmp(fa.buf, fb.buf) != 0) {
            ++mismatches;
            if (!firstBad[0])
                std::snprintf(firstBad, sizeof firstBad,
                              "shape/payload differ after %s #%d\n"
                              "[ResSet]     ours   : %.200s\n"
                              "[ResSet]     theirs : %.200s",
                              what, idx, fa.buf, fb.buf);
        }
    };

    // ---- insert phase ------------------------------------------------------
    for (int i = 0; i < kKeyCount; ++i) {
        Filename key; key.data = nullptr;
        RtSystem::RtSystem_String_Set(reinterpret_cast<RtSystem::String*>(&key), kKeys[i]);

        // A varied payload, so a wrong qword copy in PairCtor is visible.
        EntryValue v;
        v.external    = reinterpret_cast<RtObject::External*>(
                            static_cast<uintptr_t>(0x1000 + i * 0x40));
        v.usageMask   = static_cast<uint32_t>(1u << (i & 31));
        v.referenceId = static_cast<uint32_t>(0xA5A50000u + i);

        Node* outA = nullptr; Node* outB = nullptr;
        rtresset::Insert(&ours,   &outA, &key, &v);
        theirInsert     (&theirs, &outB, &key, &v);
        compare("insert", i);

        // Both sides took their own reference through their own copy ctor; the
        // probe's own reference is dropped here. Deliberately WITHOUT freeing
        // at zero: the trees still hold references, so this local is never the
        // last owner.
        if (key.data) { int32_t* rc = reinterpret_cast<int32_t*>(key.data) - 1; --(*rc); }
    }

    // ---- find phase --------------------------------------------------------
    int findHits = 0, findMisses = 0, findDisagree = 0;
    for (int i = 0; i < kKeyCount; ++i) {
        Filename key; key.data = nullptr;
        RtSystem::RtSystem_String_Set(reinterpret_cast<RtSystem::String*>(&key), kKeys[i]);
        RtSystem::RtSystem_Filename_AdjustDelimiters(&key);   // every retail caller does
        Node* a = rtresset::Find(&ours,   &key);
        Node* b = theirFind     (&theirs, &key);
        if ((a == nullptr) != (b == nullptr)) ++findDisagree;
        else if (a && b && Elem(a)->path.data && Elem(b)->path.data &&
                 std::strcmp(Elem(a)->path.data, Elem(b)->path.data) != 0) ++findDisagree;
        if (a) ++findHits; else ++findMisses;
        if (key.data) { int32_t* rc = reinterpret_cast<int32_t*>(key.data) - 1; --(*rc); }
    }
    // A key that is in NEITHER tree — otherwise `Find` is never seen to miss,
    // and a `Find` that returned a node for everything would read as agreement.
    {
        Filename key; key.data = nullptr;
        RtSystem::RtSystem_String_Set(reinterpret_cast<RtSystem::String*>(&key),
                                      "NoSuchResource/Absent.rsc");
        RtSystem::RtSystem_Filename_AdjustDelimiters(&key);   // every retail caller does
        Node* a = rtresset::Find(&ours,   &key);
        Node* b = theirFind     (&theirs, &key);
        if (a != nullptr || b != nullptr) ++findDisagree; else ++findMisses;
        if (key.data) { int32_t* rc = reinterpret_cast<int32_t*>(key.data) - 1; --(*rc); }
    }

    // ---- the two NULL-KEY arms of the comparator ---------------------------
    // The only way to reach them is a key whose `data` is null. Retail's
    // descent treats that as "orders below everything" on one side and "orders
    // below any non-null" on the other, and the asymmetry is the thing checked.
    //
    // ORDERING IS LOAD-BEARING AND WAS MEASURED, NOT GUESSED. This block sits
    // AFTER the find phase and is followed by five more inserts, for two
    // reasons the probe's first run made visible:
    //   * a node whose key is null makes every descent that reaches it turn
    //     RIGHT, so it renders part of the tree unreachable to `Find`. Run
    //     before the find phase it dropped the hit count from 49 to 31 and made
    //     the find coverage read like a defect. Both sides agreed throughout —
    //     it is retail's behaviour, not ours — but a coverage number that looks
    //     like a finding is a bad instrument.
    //   * `nullNodeKey` (the arm where the NODE's key is null) can only fire
    //     when a LATER insert descends past the null-key node. With the null
    //     key inserted last it read 0, and the probe correctly reported itself
    //     UNDERPOWERED. The five inserts below are what make it fire.
    // ------------------------------------------------------------------------
    {
        Filename nullKey; nullKey.data = nullptr;
        EntryValue v = {nullptr, 0u, 0xFFFFFFFFu};
        Node* outA = nullptr; Node* outB = nullptr;
        rtresset::Insert(&ours,   &outA, &nullKey, &v);
        theirInsert     (&theirs, &outB, &nullKey, &v);
        compare("insert-nullkey", 0);
    }
    static const char* const kAfterNull[] = {
        "PostNull/One.rsc", "PostNull/Two.rsc", "PostNull/Three.rsc",
        "postnull/ONE.rsc",                       // equal again, past the null node
        "PostNull/Four.rsc",
        // Two keys that sort BELOW every printable key in the script. _stricmp
        // folds case, so the lowest folded first byte among the others is 'a';
        // punctuation is not folded, so these two descend all the way to the
        // LEFTMOST node -- which after the insert above is the NULL-KEY node.
        // That is the only way to reach FilenameLess with a null RIGHT-hand
        // side (`nullNodeKey`); with printable keys only it read 0 and the
        // probe reported itself UNDERPOWERED rather than PASS.
        "!lowest.rsc",
        "#lower.rsc",
    };
    for (int i = 0; i < static_cast<int>(sizeof(kAfterNull)/sizeof(kAfterNull[0])); ++i) {
        Filename key; key.data = nullptr;
        RtSystem::RtSystem_String_Set(reinterpret_cast<RtSystem::String*>(&key),
                                      kAfterNull[i]);
        EntryValue v;
        v.external    = reinterpret_cast<RtObject::External*>(
                            static_cast<uintptr_t>(0x9000 + i * 0x40));
        v.usageMask   = 0x55550000u ^ static_cast<uint32_t>(i);
        v.referenceId = static_cast<uint32_t>(0xB00B0000u + i);
        Node* outA = nullptr; Node* outB = nullptr;
        rtresset::Insert(&ours,   &outA, &key, &v);
        theirInsert     (&theirs, &outB, &key, &v);
        compare("insert-past-null", i);
        if (key.data) { int32_t* rc = reinterpret_cast<int32_t*>(key.data) - 1; --(*rc); }
    }

    // ---- erase phase -------------------------------------------------------
    for (int i = 0; i < kEraseCount; ++i) {
        Filename key; key.data = nullptr;
        RtSystem::RtSystem_String_Set(reinterpret_cast<RtSystem::String*>(&key),
                                      kKeys[kEraseOrder[i]]);
        RtSystem::RtSystem_Filename_AdjustDelimiters(&key);   // every retail caller does
        Node* a = rtresset::Find(&ours,   &key);
        Node* b = theirFind     (&theirs, &key);
        if (key.data) { int32_t* rc = reinterpret_cast<int32_t*>(key.data) - 1; --(*rc); }
        if (!a || !b) continue;               // already gone (the equal-key pair)
        rtresset::Erase(&ours,   a);
        theirErase      (&theirs, b);
        compare("erase", i);
    }

    // ---- the erase case the SCRIPT CANNOT GUARANTEE ------------------------
    // Case 2 (`z->right != null && z->left == null`) is the one arm a fixed
    // erase list cannot promise to hit: whether any given node has exactly a
    // right child depends on every rotation that ran before it. The first run
    // of this probe reported `erase1Child` = 0 and correctly called itself
    // UNDERPOWERED rather than PASS.
    //
    // So it is HUNTED rather than scripted: walk OUR tree for a node with a
    // right child and no left child, take its KEY, and erase that key from
    // both sides. Picking the node from our tree and re-finding it by key in
    // theirs is what keeps the two sides driving the same subject — handing
    // theirs a pointer into our tree would be a wild write, and picking
    // independently would silently compare two different operations.
    for (int round = 0; round < 6; ++round) {
        Node* victim = nullptr;
        // iterative in-order walk; first node with right-only children wins
        Node* w = ours.root;
        while (w && w->left) w = w->left;
        while (w) {
            if (w->left == nullptr && w->right != nullptr) { victim = w; break; }
            if (w->right) { w = w->right; while (w->left) w = w->left; }
            else {
                Node* p = w->parent;
                while (p && p->right == w) { w = p; p = p->parent; }
                w = p;
            }
        }
        if (!victim) break;
        Filename key; key.data = nullptr;
        // The victim's key may be null (the null-key node). `Find` handles that
        // the same way on both sides, so it is a legitimate subject.
        key.data = Elem(victim)->path.data;
        Node* b = theirFind(&theirs, &key);
        if (!b) break;
        rtresset::Erase(&ours,   victim);
        theirErase      (&theirs, b);
        compare("erase-right-only", round);
    }

    // ---- the four negative controls ---------------------------------------
    struct Ctl { const char* name; bool (*apply)(Set*); void (*undo)(Set*); };
    const Ctl controls[] = {
        {"balance^1 at the root",  &BreakBalance, nullptr},
        {"height^1 at the root",   &BreakHeight,  nullptr},
        {"referenceId^1 at root",  &BreakElement, nullptr},
        {"root->left->parent = 0", &BreakParent,  &RestoreParent},
    };
    const int nControls = static_cast<int>(sizeof(controls) / sizeof(controls[0]));
    int fired = 0, missed = 0;
    char missedNames[320]; missedNames[0] = 0;
    static char saved[sizeof(Fp::buf)];
    for (int i = 0; i < nControls; ++i) {
        const Ctl& c = controls[i];
        Fingerprint(fa, &ours);
        std::memcpy(saved, fa.buf, sizeof saved);
        const bool savedParents = ParentsOk(ours.root, nullptr);

        if (!c.apply(&ours)) {
            ++missed;
            std::snprintf(missedNames + std::strlen(missedNames),
                          sizeof(missedNames) - std::strlen(missedNames),
                          "%s(INAPPLICABLE) ", c.name);
            continue;
        }
        Fingerprint(fb, &ours);
        const bool nowParents = ParentsOk(ours.root, nullptr);
        const bool detected = (std::strcmp(saved, fb.buf) != 0) ||
                              (savedParents != nowParents);
        if (detected) ++fired;
        else {
            ++missed;
            std::snprintf(missedNames + std::strlen(missedNames),
                          sizeof(missedNames) - std::strlen(missedNames),
                          "%s ", c.name);
        }
        if (c.undo) c.undo(&ours); else c.apply(&ours);   // self-inverse, or undo
    }

    // ---- the effectiveness control ----------------------------------------
    const rtresset::ProbeCounters& a2 = rtresset::g_rc;
#define DELTA(f) (static_cast<unsigned long long>(a2.f - before.f))
    struct Req { const char* name; unsigned long long got; };
    const Req required[] = {
        {"rootInsert",      DELTA(rootInsert)},
        {"descLeft",        DELTA(descLeft)},
        {"descRight",       DELTA(descRight)},
        {"equalHit",        DELTA(equalHit)},
        {"cmpLt",           DELTA(cmpLt)},
        {"cmpGt",           DELTA(cmpGt)},
        {"cmpEq",           DELTA(cmpEq)},
        {"nullSearchKey",   DELTA(nullSearchKey)},
        {"nullNodeKey",     DELTA(nullNodeKey)},
        {"rotL",            DELTA(rotL)},
        {"rotR",            DELTA(rotR)},
        {"preRotL",         DELTA(preRotL)},
        {"preRotR",         DELTA(preRotR)},
        {"erase0Child",     DELTA(erase0Child)},
        {"erase1Child",     DELTA(erase1Child)},
        {"erase2Child",     DELTA(erase2Child)},
        {"erasePredSplice", DELTA(erasePredSplice)},
    };
    char zeroes[640]; zeroes[0] = 0;
    int nZero = 0;
    for (const Req& r : required) {
        if (r.got == 0) {
            ++nZero;
            std::snprintf(zeroes + std::strlen(zeroes),
                          sizeof(zeroes) - std::strlen(zeroes), "%s ", r.name);
        }
    }

    // ---- verdict -----------------------------------------------------------
    std::printf("[ResSet] LOCKSTEP PROBE -- %d steps, %d shape/payload mismatches, "
                "%d parent-linkage failures\n", steps, mismatches, parentFails);
    std::printf("[ResSet]   find: hits=%d misses=%d disagreements=%d\n",
                findHits, findMisses, findDisagree);
    std::printf("[ResSet]   effectiveness (branches taken INSIDE the ported bodies): "
                "root=%llu L=%llu R=%llu eq=%llu | cmp </=/> = %llu/%llu/%llu | "
                "nullKey search/node = %llu/%llu | rot L/R = %llu/%llu  pre L/R = %llu/%llu | "
                "retraceIters=%llu | erase 0/1/2 = %llu/%llu/%llu  predSplice=%llu rootSplice=%llu\n",
                DELTA(rootInsert), DELTA(descLeft), DELTA(descRight), DELTA(equalHit),
                DELTA(cmpLt), DELTA(cmpEq), DELTA(cmpGt),
                DELTA(nullSearchKey), DELTA(nullNodeKey),
                DELTA(rotL), DELTA(rotR), DELTA(preRotL), DELTA(preRotR),
                DELTA(retraceIters),
                DELTA(erase0Child), DELTA(erase1Child), DELTA(erase2Child),
                DELTA(erasePredSplice), DELTA(eraseRootSplice));
    std::printf("[ResSet]   negative controls: %d of %d FIRED%s%s\n",
                fired, nControls, missed ? "  *** MISSED: " : "",
                missed ? missedNames : "");
    if (nZero) {
        std::printf("[ResSet]   *** UNDERPOWERED -- %d branch(es) the script is "
                    "designed to drive were NEVER TAKEN: %s\n"
                    "[ResSet]   A green run that never reached a branch says NOTHING "
                    "about that branch.\n", nZero, zeroes);
    }
    if (firstBad[0])
        std::printf("[ResSet]   first divergence: %s\n", firstBad);

    if (mismatches == 0 && parentFails == 0 && findDisagree == 0 && nZero == 0 &&
        missed == 0) {
        std::printf("[ResSet]   VERDICT: PASS -- %d lockstep steps, every watched "
                    "branch taken, every negative control fired.\n", steps);
    } else {
        std::printf("[ResSet]   VERDICT: NOT A PASS -- see the lines above.\n");
    }
    std::fflush(stdout);
#undef DELTA

    // The two private trees are deliberately NOT torn down. Freeing them would
    // exercise the very `Erase` under test in order to clean up after the test,
    // and a teardown defect would then surface as a probe crash rather than as
    // a finding. The leak is bounded — ~50 nodes and their strings, once per
    // process, only under BANISHED_RT_RESSET_PROBE=1.
}
