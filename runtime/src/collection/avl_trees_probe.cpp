// ============================================================================
// runtime/src/collection/avl_trees_probe.cpp — Wave 9 lane COLL
//   The lockstep synthetic-subject oracle for the keyed Set/Map inserts, the
//   recursive subtree free, the packfile sort and the two element assigns.
// ============================================================================
//
// WHY A PROBE AND NOT A LIVE DIFF
// -------------------------------
// Every insert MUTATES THE ONE SHARED TREE. Running ours and then the shipped
// one at a live call site inserts the same key twice — and the duplicate arm
// returns EARLY without inserting, so the second call does nothing and the
// comparison is vacuously green. That is the exact trap lane D recorded for
// `KeyMap::RegisterInput`. So: two PRIVATE trees with byte-identical starting
// state, the same script applied to both, ours through the ported bodies and
// theirs through the shipped ones.
//
// The shipped bodies are reached through **`ResolveRuntimeRaw`, never
// `ResolveAddr`** — the latter consults the port registry FIRST and this lane
// registers every one of these addresses, so it would have handed back our own
// functions and the probe would have compared ours to ours and always agreed
// (reference_resolveaddr_consults_port_registry_first.md).
//
// WHY LOCKSTEP AND NOT A FINAL COMPARISON
// ---------------------------------------
// **AVL defects CANCEL.** A rotation wrong in one direction and wrong again in
// the other leaves a tree with the same key set and often the same final shape,
// while every intermediate state differed. Both trees are fingerprinted and
// compared AFTER EVERY INSERT, and the fingerprint carries SHAPE — pre-order,
// each node's side-of-parent, balance and height, with nulls emitted — not just
// the key sequence.
//
// WHAT IS DELIBERATELY NOT COMPARED
// ---------------------------------
//  * `TypeNode + 0x04`. Neither the wrapper nor the pair ctor writes it (the
//    key is one dword and the value starts at +0x08), so it carries whatever
//    the allocator's block held. Comparing it would fail ~100% of calls
//    (reference_oracle_must_not_compare_unwritten_bytes.md).
//  * `EffectSlot + 0x2c .. +0x2f`, for the same reason, recorded independently
//    at array.cpp:437.
//  * Node ADDRESSES. Two trees allocated from the same heap in interleaved
//    order never get the same pointers, and nothing in the algorithm depends on
//    them. Shape and linkage are compared; identity is not.
//
// WHEN IT RUNS
// ------------
// **Self-scheduled on first use.** It ALLOCATES, and a probe that allocates
// cannot run from a `*PortInit` — that init runs before the allocator publishes
// its singleton, and the first allocation dereferences a null `_singleton` and
// lands on the critical section at `+0x640` (WAVE9.md, lane PKG2). So the flag
// is armed at registration and the first call into any registered entry point
// runs the probe first; that moment is provably safe because the caller is
// itself about to allocate.
//
// Opt-in: `BANISHED_RT_AVLTREES_PROBE=1`.
// ============================================================================

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "../../funcs/rt_port.h"
#include "../../types/object/TypeInfo.h"
#include "avl_trees.h"
#include "set_avl.h"

// The door that does NOT consult the port registry. Declared locally; see
// array_destroy_probe.cpp:108 for why the NAME form is not redeclared here.
uintptr_t ResolveRuntimeRaw(uintptr_t ghidra_addr);

// The port-aware door, for the allocator the probe's own strings come from.
enum class Module : int { Exe = 0, Runtime = 1, VideoDX11 = 2, COUNT = 3 };
uintptr_t ResolveAddr(Module mod, uintptr_t ghidra_addr);

namespace rtsort {
struct SortCounters {
    uint64_t calls, smallPasses, smallSwaps, medianSwaps;
    uint64_t partitions, partitionSwaps, pivotFixups;
    uint64_t eqRunA, eqRunB, pushes, pops;
};
extern SortCounters g_sc;
void  SortByKey70(void** first, int32_t count);
void* Array16Assign(void* dst, const void* src);
void* EffectSlotAssign(void* dst, const void* src);
}

using RtObject::TypeNode;
using RtObject::TypeRecord;
using rtsetavl::Node;
using rtsetavl::Set;

namespace {

bool g_armed   = false;
bool g_running = false;

int g_steps = 0, g_mismatches = 0, g_parentBad = 0, g_gaps = 0;

void Fail(const char* what, const char* detail) {
    if (g_mismatches < 6)
        printf("[AvlColl] MISMATCH %s: %s\n", what, detail);
    ++g_mismatches;
}

// ---------------------------------------------------------------------------
// Fingerprints. `side` turns a key list into a shape; balance and height are
// included because a rotation that produces the right SHAPE with the wrong
// bookkeeping breaks the very NEXT operation and nothing else would see it. A
// null child is emitted as '.', so two differently-shaped trees with the same
// key sequence cannot collide.
// ---------------------------------------------------------------------------
void Fp50(const TypeNode* n, char side, char* out, int cap, int* len) {
    if (*len >= cap - 64) return;
    if (!n) { *len += std::snprintf(out + *len, cap - *len, "."); return; }
    const int16_t bal = *reinterpret_cast<const int16_t*>(
        reinterpret_cast<const uint8_t*>(n) + 0x48);
    const int16_t hgt = *reinterpret_cast<const int16_t*>(
        reinterpret_cast<const uint8_t*>(n) + 0x4a);
    // value.typeId and value.info are carried too: the pair ctor copies them
    // field by field and a dropped field would otherwise be invisible.
    *len += std::snprintf(out + *len, cap - *len, "(%c%u:t%u:i%llx:c%d:b%d:h%d",
                          side, n->key, n->value.typeId,
                          static_cast<unsigned long long>(
                              reinterpret_cast<uintptr_t>(n->value.info)),
                          n->value.childCount, static_cast<int>(bal), static_cast<int>(hgt));
    Fp50(n->left,  'L', out, cap, len);
    Fp50(n->right, 'R', out, cap, len);
    if (*len < cap - 2) *len += std::snprintf(out + *len, cap - *len, ")");
}

// The 0x30-node fingerprint. The element is printed as TEXT for the string map
// and the packfile index — the pointers differ between the two trees by
// construction, the CHARACTERS must not.
void Fp30(const Node* n, char side, char* out, int cap, int* len, bool valueIsString) {
    if (*len >= cap - 96) return;
    if (!n) { *len += std::snprintf(out + *len, cap - *len, "."); return; }
    const char* k = *reinterpret_cast<char* const*>(n);
    *len += std::snprintf(out + *len, cap - *len, "(%c'%s'", side, k ? k : "<null>");
    if (valueIsString) {
        const char* v = *reinterpret_cast<char* const*>(
            reinterpret_cast<const uint8_t*>(n) + 8);
        *len += std::snprintf(out + *len, cap - *len, "='%s'", v ? v : "<null>");
    } else {
        const uint64_t v = *reinterpret_cast<const uint64_t*>(
            reinterpret_cast<const uint8_t*>(n) + 8);
        *len += std::snprintf(out + *len, cap - *len, "=%llx",
                              static_cast<unsigned long long>(v));
    }
    *len += std::snprintf(out + *len, cap - *len, ":b%d:h%d",
                          static_cast<int>(n->balance), static_cast<int>(n->height));
    Fp30(n->left,  'L', out, cap, len, valueIsString);
    Fp30(n->right, 'R', out, cap, len, valueIsString);
    if (*len < cap - 2) *len += std::snprintf(out + *len, cap - *len, ")");
}

// Parent linkage is checked SEPARATELY, because a fingerprint built by walking
// left/right cannot see a wrong `parent` — and a wrong parent is exactly what a
// mis-transcribed rotation produces, and exactly what breaks the NEXT retrace
// rather than this one.
bool Parents50(const TypeNode* n, const TypeNode* expect) {
    if (!n) return true;
    if (n->parent != expect) return false;
    return Parents50(n->left, n) && Parents50(n->right, n);
}
bool Parents30(const Node* n, const Node* expect) {
    if (!n) return true;
    if (n->parent != expect) return false;
    return Parents30(n->left, n) && Parents30(n->right, n);
}
int Depth50(const TypeNode* n) {
    if (!n) return 0;
    const int l = Depth50(n->left), r = Depth50(n->right);
    return 1 + (l > r ? l : r);
}
int Depth30(const Node* n) {
    if (!n) return 0;
    const int l = Depth30(n->left), r = Depth30(n->right);
    return 1 + (l > r ? l : r);
}

// The container header both 0x30 trees use: {root, count}. The 0x50 tree's is
// {TypeNode* root, int32 count} — Factory, which TypeInfo.h declares as 8 bytes
// (correct as a VIEW, never as storage), so the probe allocates 0x10.
struct Factory16 { TypeNode* root; int32_t count; int32_t pad; };
static_assert(sizeof(Factory16) == 0x10, "Object::Factory is MemAlloc(0x10,4)");

// ---------------------------------------------------------------------------
// A System::String the probe owns.
//
// *** THIS WAS A STATIC BUFFER IN THE FIRST VERSION AND THAT WAS A DEFECT IN
// THE PROBE, WHICH THE FIRST RUN FOUND BY CRASHING. *** `str-4` is the
// refcount and `str-8` is the allocation base; that header belongs to
// System::String, not the allocator. Release drives the refcount to zero and
// then FREES `str-8` — so a static buffer handed to any body that releases is
// a free of a non-heap pointer. The first run died at Runtime+0xC6D00 with
// `READ of 0x8`, four frames deep in the SHIPPED 0x180008340 — i.e. the
// crash was in RETAIL's body, on RETAIL's tree, which is exactly how you tell a
// bad CORPUS from a bad PORT. A port defect cannot make the shipped body fault
// on a tree the shipped body built.
//
// The fix is not a guard, it is the right input: every probe string is a REAL
// allocation from `System::MemoryAllocator::MemAlloc`, laid out exactly as
// System::String lays one out, so releasing it to zero is legal and the free
// path is genuinely exercised rather than avoided. A fresh block per call —
// never a reused one — because Filename's copy constructor normalises '/' to
// '\\' IN PLACE through the shared buffer, so two sides sharing a buffer would
// have the second one see the first's normalisation.
// ---------------------------------------------------------------------------
char* MakeString(const char* text) {
    using AllocFn = void* (*)(uint64_t, int);
    static AllocFn alloc = nullptr;
    if (!alloc)
        alloc = reinterpret_cast<AllocFn>(ResolveAddr(Module::Runtime, 0x1800c7a50));
    const size_t n = std::strlen(text);
    uint8_t* block = static_cast<uint8_t*>(alloc(n + 16, 8));
    if (!block) return nullptr;
    std::memset(block, 0, n + 16);
    char* data = reinterpret_cast<char*>(block) + 8;
    reinterpret_cast<int32_t*>(data)[-1] = 1;      // refcount, as String lays it
    std::memcpy(data, text, n);
    data[n] = 0;
    return data;
}

}  // namespace

// ============================================================================
void RtCollectionAvlProbeArm(bool on) { g_armed = on; }
bool RtCollectionAvlProbeArmed()      { return g_armed; }

extern "C" void RtCollectionAvlProbeMaybeRun() {
    if (!g_armed || g_running) return;
    g_armed = false;
    g_running = true;
    RtCollectionAvlProbeRun();
    g_running = false;
}

void RtCollectionAvlProbeRun() {
    // ---- the shipped bodies, RAW ------------------------------------------
    typedef TypeNode** (*TypeInsertFn)(void*, TypeNode**, const uint32_t*, const TypeRecord*);
    typedef Node**     (*MapInsertFn) (void*, Node**, char* const*, char* const*);
    typedef Node**     (*FnInsertFn)  (void*, Node**, const void*, const uint64_t*);
    typedef void       (*SubFreeFn)   (void*, Node*);
    typedef void       (*SortFn)      (void**, int32_t);
    typedef void*      (*FilenameCopyFn)(void*, const void*);
    typedef void*      (*Assign2Fn)   (void*, const void*);

    TypeInsertFn   shType = reinterpret_cast<TypeInsertFn>(ResolveRuntimeRaw(0x180030140));
    MapInsertFn    shMap  = reinterpret_cast<MapInsertFn> (ResolveRuntimeRaw(0x18002a090));
    FnInsertFn     shFn   = reinterpret_cast<FnInsertFn>  (ResolveRuntimeRaw(0x180025a80));
    SubFreeFn      shFree = reinterpret_cast<SubFreeFn>   (ResolveRuntimeRaw(0x180008340));
    SortFn         shSort = reinterpret_cast<SortFn>      (ResolveRuntimeRaw(0x1800264a0));
    FilenameCopyFn shFnCp = reinterpret_cast<FilenameCopyFn>(ResolveRuntimeRaw(0x1800067d0));
    Assign2Fn      shA16  = reinterpret_cast<Assign2Fn>   (ResolveRuntimeRaw(0x180050d30));
    Assign2Fn      shSlot = reinterpret_cast<Assign2Fn>   (ResolveRuntimeRaw(0x180051650));
    Assign2Fn      shPtrA = reinterpret_cast<Assign2Fn>   (ResolveRuntimeRaw(0x180009350));

    if (!shType || !shMap || !shFn || !shSort) {
        printf("[AvlColl] SKIP -- the shipped bodies are not reachable "
                    "(cutover, or the DLL is absent). NO COMPARISON WAS MADE.\n");
        return;
    }

    const rtavl::ProbeCounters pc0 = rtavl::g_pc;
    const rtsort::SortCounters sc0 = rtsort::g_sc;

    static char fa[8192], fb[8192];

    // =====================================================================
    // SECTION 1 — Set<Pair<uint32,TypeRecord>>::Insert, the type tree
    // =====================================================================
    //
    // THE POPULATION IS ADVERSARIAL AND EACH GROUP IS HERE FOR A REASON
    // (reference_uniform_population_is_not_a_verification.md):
    //
    //  * ASCENDING forces a right-heavy tree, i.e. a rotate-left on almost
    //    every insert. Without rotations 24 ascending keys are a spine of depth
    //    24; with them, depth 5. That number is the retrace's effectiveness
    //    control and it cannot be satisfied by noise.
    //  * DESCENDING forces rotate-right the same way.
    //  * ZIG-ZAG forces the DOUBLE rotations, the only path through the
    //    pre-rotation predicates at 0x1800305c0 / 0x1800305f4. A purely
    //    ascending population never enters them, so a port that dropped the
    //    pre-rotation entirely would pass.
    //  * KEYS ABOVE 0x7FFFFFFF, because the descent compare is UNSIGNED (`JC`,
    //    not `JL`) and the real keys are TypeNameCRCs, half of which are above
    //    it. With only small keys a signed compare orders identically and the
    //    defect is invisible.
    //  * A DUPLICATE, which must return without incrementing the count.
    // =====================================================================
    {
        static Factory16 ours, theirs;
        std::memset(&ours, 0, sizeof(ours));
        std::memset(&theirs, 0, sizeof(theirs));

        static const uint32_t kKeys[] = {
            1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,
            900,890,880,870,860,850,840,830,820,810,
            500,300,400,250,270,260,600,550,560,555,
            0x80000005u, 0xFFFFFFF0u, 0x7FFFFFFFu, 0x80000000u, 0xC0000000u,
            0x90000000u, 0xA0000001u, 0x8000000Au,
        };
        int depthAfter24 = 0;
        int rootMoved = 0;

        for (size_t idx = 0; idx < sizeof(kKeys) / sizeof(kKeys[0]); ++idx) {
            const uint32_t k = kKeys[idx];

            // The record. `children` is a REAL Array with a real buffer on some
            // records, so the nested Array copy-assign (0x180009350) is
            // exercised with a non-empty source rather than only the trivial
            // null case — otherwise its whole realloc-and-copy body would be
            // dead in this probe and `ptrAssignRealloc` would read 0.
            static TypeRecord* kids[3];
            kids[0] = reinterpret_cast<TypeRecord*>(static_cast<uintptr_t>(0x1000u + k));
            kids[1] = reinterpret_cast<TypeRecord*>(static_cast<uintptr_t>(0x2000u + k));
            kids[2] = reinterpret_cast<TypeRecord*>(static_cast<uintptr_t>(0x3000u + k));

            TypeRecord rec;
            std::memset(&rec, 0, sizeof(rec));
            rec.typeId = k;
            rec.info   = reinterpret_cast<RtObject::BaseTypeInfo*>(
                             static_cast<uintptr_t>(0xB0000000u + k));
            rec.parent = nullptr;
            if ((idx % 3) == 0) {
                rec.children      = kids;
                rec.childCount    = 3;
                rec.childCapacity = 4;
            }

            TypeNode* outO = nullptr;
            TypeNode* outT = nullptr;
            TypeNode* rootBefore = ours.root;
            rtavl::TypeSetInsert(reinterpret_cast<RtObject::Factory*>(&ours), &outO, &k, &rec);
            shType(&theirs, &outT, &k, &rec);
            if (ours.root != rootBefore) ++rootMoved;

            int la = 0, lb = 0; fa[0] = fb[0] = 0;
            Fp50(ours.root,   '*', fa, sizeof(fa), &la);
            Fp50(theirs.root, '*', fb, sizeof(fb), &lb);
            if (std::strcmp(fa, fb) != 0 || ours.count != theirs.count) {
                char buf[512];
                std::snprintf(buf, sizeof(buf),
                              "type insert key=%08x step=%d ours count=%d / theirs count=%d",
                              k, g_steps, ours.count, theirs.count);
                Fail("SECTION1", buf);
                if (g_mismatches <= 2)
                    printf("[AvlColl]   ours   %s\n[AvlColl]   theirs %s\n", fa, fb);
            }
            if (!Parents50(ours.root, nullptr)) ++g_parentBad;
            ++g_steps;
            if (idx == 23) depthAfter24 = Depth50(ours.root);
        }

        // The duplicate. Both sides must return the EXISTING node and leave the
        // count alone.
        const uint32_t dup = 13;
        TypeRecord rec; std::memset(&rec, 0, sizeof(rec)); rec.typeId = dup;
        TypeNode *o1 = nullptr, *t1 = nullptr;
        const int32_t before = ours.count;
        rtavl::TypeSetInsert(reinterpret_cast<RtObject::Factory*>(&ours), &o1, &dup, &rec);
        shType(&theirs, &t1, &dup, &rec);
        if (ours.count != before || theirs.count != before)
            Fail("SECTION1", "a duplicate key changed the element count");
        if (!o1 || !t1 || o1->key != dup || t1->key != dup)
            Fail("SECTION1", "a duplicate key did not return the existing node");
        ++g_steps;

        printf("[AvlColl] S1 type-tree: steps=%d depthAfter24Ascending=%d "
                    "(a spine would be 24) rootMoved=%d finalCount=%d\n",
                    g_steps, depthAfter24, rootMoved, ours.count);
        if (depthAfter24 >= 24) {
            printf("[AvlColl] *** GAP: depth == the insert count, NO ROTATION "
                        "EVER RAN, so section 1 verified only the descent ***\n");
            ++g_gaps;
        }
    }

    // =====================================================================
    // SECTION 2 — Map<String,String>::Insert
    //
    // THE COMPARATOR IS THE HAZARD. The historical defect this project paid for
    // was an inline strcmp that returned 0 for every distinct pair SHARING A
    // FIRST BYTE, and it survived four waves because the population was
    // uniform. So the keys below are grouped into families with SHARED
    // PREFIXES of length 0, 1, 2 and 3, plus a pair that differ only in the
    // last character, plus a pair where one is a PROPER PREFIX of the other,
    // plus keys carrying a byte >= 0x80 — which is where the SIGNED-char
    // subtraction disagrees with an unsigned one.
    // =====================================================================
    {
        static Set ours, theirs;
        std::memset(&ours, 0, sizeof(ours));
        std::memset(&theirs, 0, sizeof(theirs));

        static const char* kKeys[] = {
            "video", "videoWidth", "videoHeight", "vid", "v",
            "audio", "audioVolume", "audioMusic", "aud",
            "zzz", "zza", "zzb", "zz",
            "Alpha", "alpha", "ALPHA",
            "\xC3\xA9tat", "\xFF\xFE", "\x80zz", "\x7Fzz",
            "shared", "sharedA", "sharedB", "shareda",
            "b", "a", "c",
        };
        int rootMoved = 0;
        int lastDepth = 0;

        for (size_t i = 0; i < sizeof(kKeys) / sizeof(kKeys[0]); ++i) {
            char vbuf[64];
            std::snprintf(vbuf, sizeof(vbuf), "val%02u", static_cast<unsigned>(i));
            char* k = MakeString(kKeys[i]);
            char* v = MakeString(vbuf);

            Node* rootBefore = ours.root;
            Node *outO = nullptr, *outT = nullptr;
            rtavl::StringMapInsert(&ours, &outO, &k, &v);
            shMap(&theirs, &outT, &k, &v);
            if (ours.root != rootBefore) ++rootMoved;

            int la = 0, lb = 0; fa[0] = fb[0] = 0;
            Fp30(ours.root,   '*', fa, sizeof(fa), &la, true);
            Fp30(theirs.root, '*', fb, sizeof(fb), &lb, true);
            if (std::strcmp(fa, fb) != 0 || ours.count != theirs.count) {
                char buf[512];
                std::snprintf(buf, sizeof(buf), "map insert key='%s' step=%d "
                              "ours count=%d / theirs count=%d",
                              kKeys[i], g_steps, ours.count, theirs.count);
                Fail("SECTION2", buf);
                if (g_mismatches <= 3)
                    printf("[AvlColl]   ours   %s\n[AvlColl]   theirs %s\n", fa, fb);
            }
            if (!Parents30(ours.root, nullptr)) ++g_parentBad;
            ++g_steps;
            lastDepth = Depth30(ours.root);
        }

        // A duplicate key with a DIFFERENT value: retail returns the existing
        // node and does NOT overwrite the value. That is the whole reason
        // `ReadSettings` can be called twice without losing a setting, and a
        // port that "helpfully" assigned would be silently wrong.
        {
            char* k = MakeString("video");
            char* v = MakeString("OVERWRITTEN");
            Node *o = nullptr, *t = nullptr;
            const int32_t before = ours.count;
            rtavl::StringMapInsert(&ours, &o, &k, &v);
            shMap(&theirs, &t, &k, &v);
            if (ours.count != before || theirs.count != before)
                Fail("SECTION2", "a duplicate key changed the element count");
            const char* ov = o ? *reinterpret_cast<char* const*>(
                                    reinterpret_cast<uint8_t*>(o) + 8) : nullptr;
            const char* tv = t ? *reinterpret_cast<char* const*>(
                                    reinterpret_cast<uint8_t*>(t) + 8) : nullptr;
            if (!ov || !tv || std::strcmp(ov, tv) != 0)
                Fail("SECTION2", "duplicate-insert value disagreed with retail");
            ++g_steps;
        }

        // A NULL key and a NULL value, which take the asymmetric arms of the
        // descent (a null search key goes LEFT, a null node key goes RIGHT).
        {
            char* nullKey = nullptr;
            char* v = MakeString("nullkeyval");
            Node *o = nullptr, *t = nullptr;
            rtavl::StringMapInsert(&ours, &o, &nullKey, &v);
            shMap(&theirs, &t, &nullKey, &v);
            int la = 0, lb = 0; fa[0] = fb[0] = 0;
            Fp30(ours.root,   '*', fa, sizeof(fa), &la, true);
            Fp30(theirs.root, '*', fb, sizeof(fb), &lb, true);
            if (std::strcmp(fa, fb) != 0 || ours.count != theirs.count)
                Fail("SECTION2", "a NULL key diverged");
            ++g_steps;
        }

        printf("[AvlColl] S2 string-map: depth=%d rootMoved=%d finalCount=%d\n",
                    lastDepth, rootMoved, ours.count);
    }

    // =====================================================================
    // SECTION 3 — Set<Pair<Filename,uint64>>::Insert, the packfile index
    //
    // The comparator here is `_stricmp` (0x18007eefc), NOT section 2's signed
    // strcmp, so the population includes CASE-DIFFERING pairs that must compare
    // EQUAL and a byte >= 0x80 where the two comparators disagree in sign.
    //
    // NOTE the keys are Filenames, so the port's Filename copy ctor normalises
    // '/' to '\\' IN PLACE THROUGH THE SHARED BUFFER — which means the probe
    // must build a FRESH string for each side or the second call would see an
    // already-normalised key. It does (`Init` is called twice per step).
    // =====================================================================
    {
        static Set ours, theirs;
        std::memset(&ours, 0, sizeof(ours));
        std::memset(&theirs, 0, sizeof(theirs));

        static const char* kPaths[] = {
            "models/house.rsc", "Models/House.rsc", "models/barn.rsc",
            "MODELS/BARN.RSC", "ui/toolbar.rsc", "ui/Toolbar.rsc",
            "a", "A", "b", "ab", "aa", "az",
            "textures/\x80odd.png", "textures/\xFFodd.png",
            "z/y/x.rsc", "z/y/w.rsc", "z/a.rsc",
            "sound/effect01.wav", "sound/effect02.wav", "sound/effect1.wav",
        };
        int rootMoved = 0;

        for (size_t i = 0; i < sizeof(kPaths) / sizeof(kPaths[0]); ++i) {
            const uint64_t value = (static_cast<uint64_t>(i) << 32) | (i * 7u);

            Node* rootBefore = ours.root;
            Node *o = nullptr, *t = nullptr;

            char* k1 = MakeString(kPaths[i]);
            uint8_t fn1[8]; *reinterpret_cast<char**>(fn1) = k1;
            rtavl::FilenameSetInsert(&ours, &o, fn1, &value);

            char* k2 = MakeString(kPaths[i]);
            uint8_t fn2[8]; *reinterpret_cast<char**>(fn2) = k2;
            shFn(&theirs, &t, fn2, &value);

            if (ours.root != rootBefore) ++rootMoved;

            int la = 0, lb = 0; fa[0] = fb[0] = 0;
            Fp30(ours.root,   '*', fa, sizeof(fa), &la, false);
            Fp30(theirs.root, '*', fb, sizeof(fb), &lb, false);
            if (std::strcmp(fa, fb) != 0 || ours.count != theirs.count) {
                char buf[512];
                std::snprintf(buf, sizeof(buf), "packfile insert '%s' step=%d "
                              "ours count=%d / theirs count=%d",
                              kPaths[i], g_steps, ours.count, theirs.count);
                Fail("SECTION3", buf);
                if (g_mismatches <= 4)
                    printf("[AvlColl]   ours   %s\n[AvlColl]   theirs %s\n", fa, fb);
            }
            if (!Parents30(ours.root, nullptr)) ++g_parentBad;
            ++g_steps;
        }

        printf("[AvlColl] S3 packfile-index: depth=%d rootMoved=%d finalCount=%d\n",
                    Depth30(ours.root), rootMoved, ours.count);

        // ---- SECTION 3b: the recursive subtree free -----------------------
        //
        // WEAKER CLAIM, STATED AS SUCH. This cannot be a lockstep replay — it
        // frees everything it touches, so there is no second run. What it DOES
        // check is that ours and theirs, run on the two trees the section above
        // built to identical shape, visit the SAME NUMBER OF NODES and leave
        // both roots unwound. The node-visit count comes from the counter
        // INSIDE our body; retail's is inferred from the tree it was handed,
        // which the fingerprint above already proved identical.
        {
            const uint64_t freedBefore = rtavl::g_pc.freeNodes;
            Node* ro = ours.root;
            Node* rt = theirs.root;
            if (ro && rt) {
                rtavl::SubtreeFree(&ours,   ro->left);
                rtavl::SubtreeFree(&ours,   ro->right);
                shFree(&theirs, rt->left);
                shFree(&theirs, rt->right);
                const uint64_t visited = rtavl::g_pc.freeNodes - freedBefore;
                const int expect = ours.count - 1;   // every node but the root
                if (static_cast<int>(visited) != expect) {
                    char buf[256];
                    std::snprintf(buf, sizeof(buf),
                                  "subtree free visited %llu nodes, the tree held %d",
                                  static_cast<unsigned long long>(visited), expect);
                    Fail("SECTION3b", buf);
                }
                printf("[AvlColl] S3b subtree-free: visited=%llu expected=%d "
                            "(NOT a lockstep replay -- see the comment)\n",
                            static_cast<unsigned long long>(visited), expect);
                ours.root = nullptr;  ours.count = 0;
                theirs.root = nullptr; theirs.count = 0;
            }
        }
    }

    // =====================================================================
    // SECTION 4 — 0x1800264a0, the packfile sort
    //
    // A sort IS replayable: it is a pure permutation of a buffer the caller
    // owns. So both implementations get the SAME input in two copies and the
    // resulting PERMUTATION is compared element for element — not the
    // sortedness, which every wrong sort also satisfies.
    //
    // THE CORPUS IS TIE-HEAVY ON PURPOSE. The live population is four packages
    // that all carry the SAME +0x70 (`0x2710`, seeded by the constructor), so
    // the only thing the live call can distinguish is the tie rule — and four
    // all-equal elements never enter the big-range path at all. Cases 0..3
    // below reproduce the live shape exactly; cases 4.. exist because that
    // shape ALONE would leave `partitions`, `pivotFixups` and the two equal-run
    // walks at zero and the pass would be vacuous.
    // =====================================================================
    {
        struct Elem { uint8_t pad[0x70]; int32_t key; int32_t id; };
        static Elem   pool[256];
        static void*  ao[256];
        static void*  at[256];

        struct Case { const char* name; int n; const int32_t* keys; };

        static const int32_t kAllEq4[]   = { 10000, 10000, 10000, 10000 };
        static const int32_t kAllEq8[]   = { 1,1,1,1,1,1,1,1 };
        static const int32_t kAllEq9[]   = { 1,1,1,1,1,1,1,1,1 };
        static const int32_t kAllEq40[40] = { 0 };
        static const int32_t kOneOff4[]  = { 10000, 10000, 20000, 10000 };
        static const int32_t kAsc9[]     = { 1,2,3,4,5,6,7,8,9 };
        static const int32_t kDesc9[]    = { 9,8,7,6,5,4,3,2,1 };
        static const int32_t kZig17[]    = { 5,1,9,2,8,3,7,4,6,5,1,9,2,8,3,7,4 };
        static const int32_t kMostlyTie33[33] = {
            7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,  9,
            7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7 };
        static const int32_t kNeg9[]     = { -3, 5, -100, 0, 2147483647, -2147483648, 7, -3, 0 };
        static const int32_t kTwo[]      = { 4, 4 };
        static const int32_t kOne[]      = { 42 };

        static const Case kCases[] = {
            { "live-shape 4 all-equal",   4,  kAllEq4 },
            { "8 all-equal (boundary)",   8,  kAllEq8 },
            { "9 all-equal (boundary+1)", 9,  kAllEq9 },
            { "40 all-equal",             40, kAllEq40 },
            { "4 with one distinct",      4,  kOneOff4 },
            { "9 ascending",              9,  kAsc9 },
            { "9 descending",             9,  kDesc9 },
            { "17 zig-zag with ties",     17, kZig17 },
            { "33 mostly-tie one spike",  33, kMostlyTie33 },
            { "9 signed extremes",        9,  kNeg9 },
            { "2 equal",                  2,  kTwo },
            { "1",                        1,  kOne },
            { "65 pseudo-random",         65, nullptr },   // keys filled below
        };

        int cases = 0, permDiffs = 0;
        for (const Case& c : kCases) {
            uint32_t seed = 0x9E3779B9u;
            for (int i = 0; i < c.n; ++i) {
                std::memset(&pool[i], 0, sizeof(pool[i]));
                if (c.keys != nullptr) {
                    pool[i].key = c.keys[i];
                } else {
                    // A deterministic mixed population, big enough to recurse.
                    // It exists to reach the PIVOT FIXUP branch, which none of
                    // the hand-written cases above enters -- the pivot slot is
                    // only followed when it is the element swapped out of `j`.
                    seed = seed * 1664525u + 1013904223u;
                    pool[i].key = static_cast<int32_t>((seed >> 16) % 11u);
                }
                pool[i].id  = i;                 // the identity the permutation
                                                 // comparison is made of
                ao[i] = &pool[i];
                at[i] = &pool[i];
            }
            rtsort::SortByKey70(ao, c.n);
            shSort(at, c.n);
            bool diff = false;
            for (int i = 0; i < c.n; ++i)
                if (ao[i] != at[i]) diff = true;
            if (diff) {
                char buf[512];
                int off = std::snprintf(buf, sizeof(buf), "sort '%s' n=%d ours=[", c.name, c.n);
                for (int i = 0; i < c.n && off < 300; ++i)
                    off += std::snprintf(buf + off, sizeof(buf) - off, "%d ",
                                         static_cast<Elem*>(ao[i])->id);
                off += std::snprintf(buf + off, sizeof(buf) - off, "] theirs=[");
                for (int i = 0; i < c.n && off < 460; ++i)
                    off += std::snprintf(buf + off, sizeof(buf) - off, "%d ",
                                         static_cast<Elem*>(at[i])->id);
                std::snprintf(buf + off, sizeof(buf) - off, "]");
                Fail("SECTION4", buf);
                ++permDiffs;
            }
            ++cases;
            ++g_steps;
        }

        const rtsort::SortCounters& sc = rtsort::g_sc;
        printf("[AvlColl] S4 sort: cases=%d permutationDiffs=%d\n"
                    "[AvlColl] S4 effectiveness (branches INSIDE the body): "
                    "smallPasses=%llu smallSwaps=%llu medianSwaps=%llu\n"
                    "[AvlColl] S4                 partitions=%llu partSwaps=%llu "
                    "pivotFixups=%llu eqRunA=%llu eqRunB=%llu pushes=%llu pops=%llu\n",
                    cases, permDiffs,
                    (unsigned long long)(sc.smallPasses - sc0.smallPasses),
                    (unsigned long long)(sc.smallSwaps  - sc0.smallSwaps),
                    (unsigned long long)(sc.medianSwaps - sc0.medianSwaps),
                    (unsigned long long)(sc.partitions  - sc0.partitions),
                    (unsigned long long)(sc.partitionSwaps - sc0.partitionSwaps),
                    (unsigned long long)(sc.pivotFixups - sc0.pivotFixups),
                    (unsigned long long)(sc.eqRunA - sc0.eqRunA),
                    (unsigned long long)(sc.eqRunB - sc0.eqRunB),
                    (unsigned long long)(sc.pushes - sc0.pushes),
                    (unsigned long long)(sc.pops   - sc0.pops));
        if (sc.partitions == sc0.partitions) {
            printf("[AvlColl] *** GAP: partitions=0 -- the corpus never left the "
                        "<=8 small-range path, so the whole median-of-3 and partition "
                        "transcription is UNVERIFIED ***\n");
            ++g_gaps;
        }
        if (sc.eqRunA == sc0.eqRunA && sc.eqRunB == sc0.eqRunB) {
            printf("[AvlColl] *** GAP: neither equal-run walk ran -- the tie "
                        "handling that decides package precedence is UNVERIFIED ***\n");
            ++g_gaps;
        }
        if (sc.pushes == sc0.pushes) {
            printf("[AvlColl] *** GAP: the pending-range stack was never used ***\n");
            ++g_gaps;
        }

        // ---- NEGATIVE CONTROL A: the tie rule ----------------------------
        // The small-range pass moves the tracked position ONLY on a STRICT
        // `>`, so among equal keys the EARLIEST position wins. Flip it to `>=`
        // and the permutation of a tie-heavy input changes while it stays just
        // as "sorted". If the comparison above cannot see that, it is checking
        // sortedness and the whole section is decoration.
        //
        // *** N MUST KEEP THE WHOLE RANGE ON THE SMALL PATH, AND THE FIRST
        // VERSION OF THIS CONTROL DID NOT. *** It used n=12 and compared a
        // small-range-ONLY `>=` variant against the FULL shipped sort. Twelve
        // elements go down the big-range path, and on an all-equal input both
        // that path and the `>=` variant happen to be the IDENTITY permutation,
        // so the control could not fire on any input and duly reported MISSED.
        // That is the "a control that cannot go to zero is not measuring
        // coverage" failure, caught by reading the MISSED rather than
        // explaining it away. n=8 keeps the whole range on the small path,
        // where `>` and `>=` genuinely disagree: `>` never moves the tracked
        // position among equals so it swaps lo with hi every pass, `>=` moves
        // it to hi so the swap is a no-op.
        bool negA = false;
        {
            static const int32_t keys[] = { 7,7,7,7,7,7,7,7 };
            const int nA = 8;
            for (int i = 0; i < nA; ++i) {
                std::memset(&pool[i], 0, sizeof(pool[i]));
                pool[i].key = keys[i]; pool[i].id = i;
                ao[i] = &pool[i]; at[i] = &pool[i];
            }
            shSort(at, nA);
            // the same algorithm with `>=` in the small pass
            void** lo = ao; void** hi = ao + (nA - 1);
            if (hi > lo) {
                void** const scanFrom = lo + 1;
                do {
                    void** best = lo;
                    for (void** p = scanFrom; p <= hi; ++p) {
                        const int32_t kb = *reinterpret_cast<int32_t*>(
                            static_cast<uint8_t*>(*best) + 0x70);
                        const int32_t kp = *reinterpret_cast<int32_t*>(
                            static_cast<uint8_t*>(*p) + 0x70);
                        if (kb >= kp) best = p;         // >= instead of >
                    }
                    void* a = *hi; void* b = *best; --hi; *best = a; *(hi + 1) = b;
                } while (hi > lo);
            }
            for (int i = 0; i < nA; ++i) if (ao[i] != at[i]) negA = true;
        }

        // ---- NEGATIVE CONTROL B: a "clean" std::sort-shaped body ---------
        // A stable ordering by key. It produces a correctly DESCENDING array
        // and a DIFFERENT permutation on ties -- exactly the defect
        // filesystem_packages.cpp:760 warned about. It must be caught.
        bool negB = false;
        {
            static const int32_t keys[] = { 5,5,5,9,5,5,1,5,5,5,9,5,5,5 };
            const int n = 14;
            for (int i = 0; i < n; ++i) {
                std::memset(&pool[i], 0, sizeof(pool[i]));
                pool[i].key = keys[i]; pool[i].id = i;
                ao[i] = &pool[i]; at[i] = &pool[i];
            }
            shSort(at, n);
            for (int i = 1; i < n; ++i) {            // stable insertion sort
                void* v = ao[i]; int j = i - 1;
                while (j >= 0 &&
                       *reinterpret_cast<int32_t*>(static_cast<uint8_t*>(ao[j]) + 0x70) <
                       *reinterpret_cast<int32_t*>(static_cast<uint8_t*>(v) + 0x70)) {
                    ao[j + 1] = ao[j]; --j;
                }
                ao[j + 1] = v;
            }
            for (int i = 0; i < n; ++i) if (ao[i] != at[i]) negB = true;
        }

        printf("[AvlColl] S4 negative-controls: tie-rule=%s stable-sort=%s\n",
                    negA ? "CAUGHT" : "*** MISSED ***",
                    negB ? "CAUGHT" : "*** MISSED (a stable sort agrees -- the tie "
                                      "population is too weak to falsify) ***");
        if (!negA || !negB) ++g_gaps;
    }

    // =====================================================================
    // SECTION 5 — the three copy-assigns, byte-compared
    //
    // Windows EXCLUDED, and each exclusion is a measurement, not a guess:
    //   * EffectSlot +0x2c..+0x2f  — neither arm of 0x180051650 writes them.
    // Everything else is compared, including the nested Array headers.
    // =====================================================================
    if (shA16 && shSlot && shPtrA) {
        int cmp = 0, bad = 0;

        // -- Array<T*>::operator=, 0x180009350 -----------------------------
        for (int n = 0; n <= 5; ++n) {
            static void* srcBuf[8];
            for (int i = 0; i < 8; ++i)
                srcBuf[i] = reinterpret_cast<void*>(static_cast<uintptr_t>(0x5000u + i));
            struct H { void* d; int32_t s, c; };
            H src = { srcBuf, n, 8 };
            H o = { nullptr, 0, 0 }, t = { nullptr, 0, 0 };
            rtavl::PtrArrayAssign(&o, &src);
            shPtrA(&t, &src);
            if (o.s != t.s || o.c != t.c) { ++bad; Fail("SECTION5", "Array<T*> header"); }
            for (int i = 0; i < n; ++i)
                if (static_cast<void**>(o.d)[i] != static_cast<void**>(t.d)[i]) {
                    ++bad; Fail("SECTION5", "Array<T*> element"); break;
                }
            ++cmp;
        }

        // -- Array<T16>::operator=, 0x180050d30 ----------------------------
        for (int n = 0; n <= 5; ++n) {
            static uint8_t srcBuf[8 * 0x10];
            for (size_t i = 0; i < sizeof(srcBuf); ++i) srcBuf[i] = static_cast<uint8_t>(i * 7 + 3);
            struct H { void* d; int32_t s, c; };
            H src = { srcBuf, n, 8 };
            H o = { nullptr, 0, 0 }, t = { nullptr, 0, 0 };
            rtsort::Array16Assign(&o, &src);
            shA16(&t, &src);
            if (o.s != t.s || o.c != t.c) { ++bad; Fail("SECTION5", "Array<T16> header"); }
            if (n > 0 && std::memcmp(o.d, t.d, static_cast<size_t>(n) * 0x10) != 0) {
                ++bad; Fail("SECTION5", "Array<T16> elements");
            }
            ++cmp;
        }

        // -- EffectSlot copy-assign, 0x180051650 ---------------------------
        {
            static uint8_t src[0x40], nested[4 * 0x10];
            for (size_t i = 0; i < sizeof(src); ++i)   src[i]    = static_cast<uint8_t>(i + 1);
            for (size_t i = 0; i < sizeof(nested); ++i) nested[i] = static_cast<uint8_t>(i * 3 + 5);
            *reinterpret_cast<void**>(src + 0x30)   = nested;
            *reinterpret_cast<int32_t*>(src + 0x38) = 4;
            *reinterpret_cast<int32_t*>(src + 0x3c) = 4;

            static uint8_t o[0x40], t[0x40];
            std::memset(o, 0xCD, sizeof(o));
            std::memset(t, 0xCD, sizeof(t));
            rtsort::EffectSlotAssign(o, src);
            shSlot(t, src);
            // +0x00..+0x2b, then +0x30..+0x37 (the Array data pointers differ by
            // construction, so only the sizes are compared) — +0x2c..+0x2f is
            // the measured hole.
            if (std::memcmp(o, t, 0x2c) != 0) { ++bad; Fail("SECTION5", "EffectSlot scalars"); }
            if (*reinterpret_cast<int32_t*>(o + 0x38) != *reinterpret_cast<int32_t*>(t + 0x38) ||
                *reinterpret_cast<int32_t*>(o + 0x3c) != *reinterpret_cast<int32_t*>(t + 0x3c)) {
                ++bad; Fail("SECTION5", "EffectSlot nested Array header");
            }
            if (std::memcmp(*reinterpret_cast<void**>(o + 0x30),
                            *reinterpret_cast<void**>(t + 0x30), 4 * 0x10) != 0) {
                ++bad; Fail("SECTION5", "EffectSlot nested Array elements");
            }
            // The measured hole must STILL be 0xCD on BOTH sides. If retail
            // wrote it, the exclusion above is wrong and must be removed.
            if (o[0x2c] != 0xCD || t[0x2c] != 0xCD) {
                printf("[AvlColl] *** the +0x2c..+0x2f hole was WRITTEN "
                            "(ours=%02x theirs=%02x) -- the oracle's exclusion is "
                            "wrong and must be removed ***\n", o[0x2c], t[0x2c]);
                ++g_gaps;
            }
            ++cmp;
        }
        printf("[AvlColl] S5 element-assigns: comparisons=%d bad=%d\n", cmp, bad);
    } else {
        printf("[AvlColl] S5 SKIPPED -- a shipped assign body was unreachable\n");
        ++g_gaps;
    }

    // =====================================================================
    // NEGATIVE CONTROLS for the trees. Both MUST fire; if either does not,
    // every green step above is void.
    // =====================================================================
    bool neg1 = false, neg2 = false;

    // NC1 — a SIGNED descent in the 0x50 tree. The real keys are CRCs and half
    // of them exceed 0x7FFFFFFF; a signed compare puts those on the wrong side.
    {
        static Factory16 bad, ref;
        std::memset(&bad, 0, sizeof(bad));
        std::memset(&ref, 0, sizeof(ref));
        static const uint32_t ids[] = { 100u, 0x80000005u, 50u, 0xFFFFFFF0u, 200u };
        for (uint32_t k : ids) {
            TypeRecord r; std::memset(&r, 0, sizeof(r)); r.typeId = k;
            TypeNode* out = nullptr;
            shType(&ref, &out, &k, &r);
        }
        for (uint32_t k : ids) {
            TypeRecord r; std::memset(&r, 0, sizeof(r)); r.typeId = k;
            TypeNode* out = nullptr;
            // ours, but with the key pre-mangled so the UNSIGNED descent sees
            // what a SIGNED one would have seen. Equivalent to flipping the
            // compare, without editing the port.
            uint32_t flipped = k ^ 0x80000000u;
            rtavl::TypeSetInsert(reinterpret_cast<RtObject::Factory*>(&bad),
                                 &out, &flipped, &r);
        }
        int la = 0, lb = 0; fa[0] = fb[0] = 0;
        Fp50(bad.root, '*', fa, sizeof(fa), &la);
        Fp50(ref.root, '*', fb, sizeof(fb), &lb);
        neg1 = std::strcmp(fa, fb) != 0;
    }

    // NC2 — the historical inline-strcmp defect: return 0 whenever the first
    // bytes match. It is reproduced by feeding the SAME first byte to a family
    // of distinct keys through a comparator that stops there, and asserting the
    // fingerprint notices. Built with a deliberately truncated comparator
    // rather than by editing the port.
    {
        static Set bad, ref;
        std::memset(&bad, 0, sizeof(bad));
        std::memset(&ref, 0, sizeof(ref));
        static const char* keys[] = { "sa", "sb", "sc", "sd", "se" };
        for (size_t i = 0; i < 5; ++i) {
            char* k = MakeString(keys[i]);
            char* v = MakeString("v");
            Node* out = nullptr;
            shMap(&ref, &out, &k, &v);
        }
        // A first-byte-only comparator: every one of these keys compares EQUAL
        // to every other, so only the first ever gets inserted.
        for (size_t i = 0; i < 5; ++i) {
            char* k = MakeString(keys[i]);
            char* v = MakeString("v");
            Node* out = nullptr;
            if (!bad.root) { rtavl::StringMapInsert(&bad, &out, &k, &v); continue; }
            const char* rk = *reinterpret_cast<char* const*>(bad.root);
            if (rk && rk[0] == k[0]) continue;   // "equal" -> no insert
            rtavl::StringMapInsert(&bad, &out, &k, &v);
        }
        int la = 0, lb = 0; fa[0] = fb[0] = 0;
        Fp30(bad.root, '*', fa, sizeof(fa), &la, true);
        Fp30(ref.root, '*', fb, sizeof(fb), &lb, true);
        neg2 = std::strcmp(fa, fb) != 0;
    }

    // =====================================================================
    // The report.
    // =====================================================================
    const rtavl::ProbeCounters& c = rtavl::g_pc;
    printf("[AvlColl] steps=%d mismatches=%d parent-linkage-bad=%d gaps=%d\n"
                "[AvlColl] effectiveness (branches INSIDE the ported bodies):\n"
                "[AvlColl]   50-node: rotL=%llu rotR=%llu preRotL=%llu preRotR=%llu "
                "retraceIters=%llu\n"
                "[AvlColl]   descent: left=%llu right=%llu equal=%llu rootInsert=%llu "
                "nullSearchKey=%llu nullNodeKey=%llu\n"
                "[AvlColl]   strcmp  : lt=%llu eq=%llu gt=%llu    "
                "stricmp: lt=%llu eq=%llu gt=%llu\n"
                "[AvlColl]   ptrArray: realloc=%llu treble=%llu self=%llu | "
                "free: nodes=%llu stringsReleased=%llu\n"
                "[AvlColl] negative-controls: signed-descent=%s  "
                "first-byte-strcmp=%s\n",
                g_steps, g_mismatches, g_parentBad, g_gaps,
                (unsigned long long)(c.rot50L - pc0.rot50L),
                (unsigned long long)(c.rot50R - pc0.rot50R),
                (unsigned long long)(c.preRot50L - pc0.preRot50L),
                (unsigned long long)(c.preRot50R - pc0.preRot50R),
                (unsigned long long)(c.retrace50Iters - pc0.retrace50Iters),
                (unsigned long long)(c.descLeft - pc0.descLeft),
                (unsigned long long)(c.descRight - pc0.descRight),
                (unsigned long long)(c.equalHit - pc0.equalHit),
                (unsigned long long)(c.rootInsert - pc0.rootInsert),
                (unsigned long long)(c.nullSearchKey - pc0.nullSearchKey),
                (unsigned long long)(c.nullNodeKey - pc0.nullNodeKey),
                (unsigned long long)(c.strcmpLt - pc0.strcmpLt),
                (unsigned long long)(c.strcmpEq - pc0.strcmpEq),
                (unsigned long long)(c.strcmpGt - pc0.strcmpGt),
                (unsigned long long)(c.icmpLt - pc0.icmpLt),
                (unsigned long long)(c.icmpEq - pc0.icmpEq),
                (unsigned long long)(c.icmpGt - pc0.icmpGt),
                (unsigned long long)(c.ptrAssignRealloc - pc0.ptrAssignRealloc),
                (unsigned long long)(c.ptrAssignTreble - pc0.ptrAssignTreble),
                (unsigned long long)(c.ptrAssignSelf - pc0.ptrAssignSelf),
                (unsigned long long)(c.freeNodes - pc0.freeNodes),
                (unsigned long long)(c.freeStringsReleased - pc0.freeStringsReleased),
                neg1 ? "CAUGHT" : "*** MISSED ***",
                neg2 ? "CAUGHT" : "*** MISSED ***");

    // Each of these is a branch whose absence makes some part of the sweep
    // vacuous. They are printed as GAPs, not swallowed.
    if (c.rot50L == pc0.rot50L || c.rot50R == pc0.rot50R) {
        printf("[AvlColl] *** GAP: a rotation direction never ran ***\n"); ++g_gaps;
    }
    if (c.preRot50L == pc0.preRot50L || c.preRot50R == pc0.preRot50R) {
        printf("[AvlColl] *** GAP: a PRE-rotation never ran -- the double-rotation "
                    "predicates are UNVERIFIED ***\n"); ++g_gaps;
    }
    if (c.strcmpLt == pc0.strcmpLt || c.strcmpGt == pc0.strcmpGt ||
        c.strcmpEq == pc0.strcmpEq) {
        printf("[AvlColl] *** GAP: the signed strcmp did not produce all three "
                    "signs -- exactly the uniform-population failure ***\n"); ++g_gaps;
    }
    if (c.icmpEq == pc0.icmpEq) {
        printf("[AvlColl] *** GAP: _stricmp never returned 0, so the "
                    "case-insensitive tie path is UNVERIFIED ***\n"); ++g_gaps;
    }
    if (c.nullSearchKey == pc0.nullSearchKey && c.nullNodeKey == pc0.nullNodeKey) {
        printf("[AvlColl] *** GAP: neither asymmetric null arm ran ***\n"); ++g_gaps;
    }
    if (!neg1 || !neg2) {
        printf("[AvlColl] *** A NEGATIVE CONTROL WAS MISSED: the comparison is not "
                    "sensitive to the thing it exists to catch, so the green steps "
                    "above are VOID ***\n");
    }
    printf("[AvlColl] VERDICT: %s (mismatches=%d gaps=%d)\n",
                (g_mismatches == 0 && g_parentBad == 0 && g_gaps == 0 && neg1 && neg2)
                    ? "PASS" : "REVIEW",
                g_mismatches, g_gaps);
}
