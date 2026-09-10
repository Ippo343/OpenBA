// ============================================================================
// runtime/src/object/external_add.cpp — Wave 11 follow-up 2, lane IOSTREAM
//   `Object::ExternalFactory::Add`                              retail 0x1800be570
// ============================================================================
//
//   ?Add@ExternalFactory@Object@@QEAAXPEAVExternal@2@@Z
//
// THE ROW LANE OBJEXT OPENED. Porting `FindOrLoad` meant naming `Add`, because
// `FindOrLoad` calls it unconditionally on both branches — so `Add` entered the
// unported set in the same commit that closed four other rows
// (WAVE11_LANE_OBJEXT.md §5.2). This closes it.
//
// ---------------------------------------------------------------------------
// WHY IT WAS BLOCKED, AND WHAT ACTUALLY UNBLOCKED IT
// ---------------------------------------------------------------------------
// OBJEXT recorded the blocker as "stack-constructs an IO::VisitorStream by
// value, which has no layout or constructor in the tree". Half of that is
// right. `??0VisitorStream@IO@@QEAA@XZ` (ordinal 511) and
// `??1VisitorStream@IO@@UEAA@XZ` (ordinal 715) are both EXPORTED — this DLL
// exports its private and protected members, and reading `QEAA`/`AEAA` as
// "unexported" is the mistake reference_aeaa_is_private_not_unexported.md
// records. What was genuinely missing was the LAYOUT, i.e. how many bytes to
// reserve and which fields the inlined destructor touches.
//
// runtime/types/io/FileStream.h now pins it: `IO::VisitorStream` is 0x18
// bytes, adds NO fields to `IO::Stream`, and its `Write` is literally
// `return true`. It is a NULL SINK, and that is the whole mechanism of this
// function's interesting branch — see below.
//
// ---------------------------------------------------------------------------
// WHAT THE FUNCTION DOES
// ---------------------------------------------------------------------------
// Register `external` in the factory's path-keyed resource tree, and mark it
// used by the CURRENT clean generation:
//
//   1. copy the External's own path (`GetPath`, a by-value return with a
//      hidden slot) into a local,
//   2. descend `_resources` for that path,
//   3. MISS  -> insert {path, external, usageMask = 1 << _cleanBitIndex,
//               referenceId = -1},
//      HIT   -> OR the current clean bit into the existing row's usageMask,
//               and IF THAT BIT WAS NOT ALREADY SET, run the reference-visit
//               pass over the object,
//   4. release the local path.
//
// STEP 3'S "IF THAT BIT WAS NOT ALREADY SET" IS THE POINT OF THE WHOLE
// FUNCTION. The visit pass runs exactly once per resource per clean
// generation, not once per Add — and `FindOrLoad` reaches Add ~22.6k times in
// one menu-to-gameplay run (lane FNHOOK's `[ResLoad] census: FindOrLoad=22591`),
// so a port that ran the pass unconditionally would do tens of thousands of
// full object walks instead of a few thousand. That is a behavioural
// difference, not just a slow one: the pass has side effects.
//
// THE VISIT PASS. `VisitorStream` accepts every byte and stores none, so
// calling the External's own `Serialize` (vtable +0x28) against it walks the
// object exactly as a save would and writes nothing. The only thing that
// actually happens is the side effect in the Encoding the VisitorStream's
// constructor installs at +0x10 (vtable 0x1801559f8), which records the
// resource references the walk passes through. Serializer.h:154 and
// serialize_family.cpp:125 describe the same pass from the Serializer end;
// this is the stream end of it.
//
// ---------------------------------------------------------------------------
// EVERYTHING ELSE IT NEEDS WAS ALREADY IN THE TREE
// ---------------------------------------------------------------------------
//   ?GetPath@External@Object@@         0x180006250   ported, external.cpp
//   Set<ResourceEntry>::Insert         0x18002ca00   ported, external_set.cpp
//   the AVL node layout / EntryValue                 external_set.h
//   ResourceEntry field offsets                      types/object/External.h
//   Filename copy-ctor and the String release idiom  external_set.cpp
//
// so the only new code here is the descent, the clean-bit test, and the
// stack-constructed VisitorStream.
//
// PORTABILITY: no <windows.h>, per PLAN.md §4.1 decision 1.
// ============================================================================

#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <cstdlib>

#include "external_set.h"
#include "../system/string.h"
#include "../../funcs/rt_port.h"
#include "../../funcs/rt_diff.h"
#include "../../types/object/External.h"
#include "../../types/io/FileStream.h"

// runtime/src/io/file_stream_probe.cpp -- the oracle for the LAYOUTS this file
// depends on, armed from the first live call below. Declared at FILE scope and
// `extern "C"`: a block-scope `extern void` would bind the name into namespace
// RtObject and link against a symbol nothing defines.
extern "C" void RtIoProbeRunFileStreamLayoutProbe();
extern "C" void RtIoProbeRunTextFileStreamProbe();   // Wave 20 lane SAVETEXT

// runtime/src/object/external_savetext_probe.cpp -- the oracle for
// External::SaveText, which no gameplay route reaches. It needs a live
// `External*`, and this body is handed one ~7.9k times a run.
extern "C" void RtObjectSaveTextProbeStepC(void* subject);

enum class Module : int { Exe = 0, Runtime = 1, VideoDX11 = 2, COUNT = 3 };
uintptr_t ResolveAddr(Module mod, uintptr_t ghidra_addr);

// `??0Filename@System@@QEAA@AEBV01@@Z` — ported in Wave 2
// (runtime/src/system/filename.cpp), registered at 0x1800067d0. Declared here
// rather than pulled from a header because that lane's file exposes none of its
// own; external.cpp and external_set.cpp both declare it exactly this way.
namespace RtSystem {
Filename* RtSystem_Filename_CtorCopy(Filename* self, const Filename* other);
}

namespace RtObject {

using RtSystem::Filename;

namespace {

// ---------------------------------------------------------------------------
// Doors. Every one of these is a NAME door except the Encoding delete, which
// has no export in either build.
// ---------------------------------------------------------------------------

// `?GetPath@External@Object@@QEBA?AVFilename@System@@XZ` returns a Filename BY
// VALUE, so there IS a hidden return slot and a typedef that omits it writes
// past the caller's frame while still returning the right answer -- the shipped
// defect reference_hidden_retslot_wild_write.md records.
//
// THE ORDER IS (this, retslot), NOT (retslot, this). The inherited draft's
// comment here said the opposite; its CODE was right and only the prose was
// wrong, which is the more dangerous way round because the next reader fixes
// the code to match the comment. Read off the call site rather than from a
// remembered ABI rule:
//     1800be594  lea rdx,[rbp+0x30]     <- the return slot, SECOND
//     1800be598  mov rcx,rsi            <- `external`, FIRST
//     1800be59b  call 0x180006250       (?GetPath@External@Object@@)
//     1800be5a1  mov rdx,rax            <- and the callee returns the slot
// so RCX is `this` and RDX is the slot, and the copy-ctor below takes the
// returned RAX as its source.
using GetPathFn = Filename* (*)(const External* self, Filename* ret);

// `??0VisitorStream@IO@@QEAA@XZ`. Returns `this` in RAX; no hidden slot.
using VisitorCtorFn = void* (*)(RtIo::VisitorStream* self);

// The Encoding's deleting destructor at 0x180006a10 — the same guarded object
// free `~MemoryStream`, `~CompressedStream` and `~VisitorStream` all tail into.
// Reached by address because it has no exported name; already registered by
// lane E, so this resolves to our own body and not to the DLL's.
void EncodingDelete(void* p) {
    using Fn = void (*)(void*);
    static Fn fn = nullptr;
    if (!fn) fn = reinterpret_cast<Fn>(ResolveAddr(Module::Runtime, 0x180006a10));
    fn(p);
}

// External vtable slot +0x28 — `Serialize(IO::Stream&)`. The same slot
// `External::Load` drives and the one External.h names.
constexpr size_t kExternalSerializeSlotBytes = 0x28;

void ExternalSerialize(External* self, void* stream) {
    void** vtbl = *reinterpret_cast<void***>(self);
    using Fn = void (*)(External*, void*);
    reinterpret_cast<Fn>(vtbl[kExternalSerializeSlotBytes / sizeof(void*)])(self, stream);
}

// ---------------------------------------------------------------------------
// The comparator, and why it is spelled out here again.
//
// This is the third copy in the tree (external.cpp's `StriCmpAscii` /
// `FilenameLess`, external_set.cpp's, and this one). They are duplicated rather
// than shared because the other two are `static` in files other lanes own, and
// because reference_a_rule_on_the_callers_side_gets_broken.md is about exactly
// what happens when a shared helper drifts from the call site that depends on
// it. The behaviour is pinned by the assertion below, not by inspection.
//
// Retail's comparisons go through the CRT `_stricmp` at 0x18007eefc. The DLL
// never calls `setlocale`, so that runs in the "C" locale: plain ASCII case
// folding. `runtime/src` must not depend on a CRT whose locale behaviour
// differs between toolchains (PLAN §4.1).
// ---------------------------------------------------------------------------
int StriCmpAscii(const char* a, const char* b) {
    for (;;) {
        unsigned char ca = static_cast<unsigned char>(*a++);
        unsigned char cb = static_cast<unsigned char>(*b++);
        if (ca >= 'A' && ca <= 'Z') ca = static_cast<unsigned char>(ca + 32);
        if (cb >= 'A' && cb <= 'Z') cb = static_cast<unsigned char>(cb + 32);
        if (ca != cb) return static_cast<int>(ca) - static_cast<int>(cb);
        if (ca == 0)  return 0;
    }
}

// `a < b`, with retail's ASYMMETRIC null arms transcribed from the branch
// structure rather than from an idea of what a comparator should do: a null
// RIGHT-hand side orders below everything, a null LEFT-hand side orders below
// any non-null. Identical to external_set.cpp's, minus its counters.
bool FilenameLess(const char* a, const char* b) {
    if (b == nullptr) return false;
    if (a == nullptr) return true;
    return StriCmpAscii(a, b) < 0;
}

// The String release idiom, on a Filename slot.
//
// UNLIKE external_set.cpp, this does NOT re-spell the idiom: it calls the
// tree's single ported copy, `RtSystem::RtString_ReleaseInline` (the body of
// `?Release@String@System@@AEAAXXZ`, 0x1800c9870). external_set.cpp repeated it
// because its call sites operate on a Filename embedded at a NODE offset;
// these operate on plain locals, so the shared body applies directly and a
// fourth copy would be one more place for the idiom to drift
// (reference_a_rule_on_the_callers_side_gets_broken.md).
//
// The cast is safe by construction: System::Filename's only field is the same
// `char* data` at +0x00 that System::String has, which is why every Filename
// port in this tree reuses the String primitives by pointer cast.
void ReleaseInline(Filename* slot) {
    RtSystem::RtString_ReleaseInline(reinterpret_cast<RtSystem::String*>(slot));
}

}  // namespace

// ---------------------------------------------------------------------------
// EFFECTIVENESS COUNTERS.
//
// WAVE9's standing rule and reference_calls_zero_can_mean_always_took_the_
// uncovered_branch.md: an entry count cannot tell a body that ran and did
// nothing interesting from one that did. The two numbers that matter here are
// `hitVisited` and `hitSkipped` — the clean-bit branch, which is the ONLY
// branch in this function with a side effect beyond the tree — and they are
// the ones a "run the pass every time" paraphrase would collapse into one.
// ---------------------------------------------------------------------------
struct AddCounters {
    uint64_t calls;
    uint64_t inserted;    // descent fell off the tree
    uint64_t hit;         // equal key found
    uint64_t hitVisited;  // ... and the clean bit was NOT already set -> visit pass ran
    uint64_t hitSkipped;  // ... and it WAS already set -> no pass
    uint64_t descLeft, descRight;
    uint64_t emptyTree;   // root was null on entry
};
AddCounters g_ac{};

// ============================================================================
// Object::ExternalFactory::Add — the body.               retail 0x1800be570
// ============================================================================
extern "C" void RtObject_ExternalFactory_Add(ExternalFactory* self, External* external) {
    RT_DIFF_BEGIN("Object::ExternalFactory::Add");
    ++g_ac.calls;

    // The layout probe's run point. It ALLOCATES (four of the five constructors
    // it drives install an IO::Encoding), so it cannot run from RtObjectPortInit
    // -- the MemoryAllocator singleton is constructed but not started there and
    // the first allocation faults at null+0x640 (WAVE9 lane PKG2). The first
    // real Add is provably safe because this body is itself about to allocate.
    //
    // Scheduling an instrument off a body is a CLAIM THAT THE BODY RUNS, and
    // lane OBJEXT lost a launch to that claim being false for PushPath
    // (WAVE11_LANE_OBJEXT.md 10.1). Here it is measured, not assumed: our own
    // ported FindOrLoad calls Add unconditionally on both branches, ~22.6k times
    // per menu-to-gameplay run, and g_ac.calls above is the positive control
    // that says so from inside this function.
    //
    // Opt-in, because it drives five constructor/destructor pairs through the
    // DLL and a normal run should not pay for that. Re-entry is impossible: the
    // arm flag is cleared BEFORE anything is driven.
    {
        static bool armed = (std::getenv("BANISHED_RT_IOSTREAM_PROBE") != nullptr);
        if (armed) {
            armed = false;
            RtIoProbeRunFileStreamLayoutProbe();
        }
    }

    // The SaveText oracle's subject feed. Unlike the layout probe this one is
    // NOT one-shot: it needs several distinct Externals, so it is stepped on
    // every call until it has its sample and then returns immediately. Opt-in
    // for the same reason -- it writes and re-reads two files per subject.
    {
        static bool stArmed = (std::getenv("BANISHED_RT_SAVETEXT_PROBE") != nullptr);
        if (stArmed) RtObjectSaveTextProbeStepC(external);
    }

    // Wave 20 lane SAVETEXT's oracle for the five TextFileStream vtable slots.
    // Same host, same one-shot shape and the same reason as the layout probe
    // above: this is the earliest point where the FileSystem singleton is up,
    // and `Add`'s own call counter is the positive control that the host runs.
    // Opt-in because screen D writes twelve files.
    {
        static bool tfsArmed = (std::getenv("BANISHED_RT_TFS_PROBE") != nullptr);
        if (tfsArmed) {
            tfsArmed = false;
            RtIoProbeRunTextFileStreamProbe();
        }
    }

    // ---- 1800be589..1800be5d9 -------------------------------------------
    // `Filename path = external->GetPath();`
    //
    // Retail spills GetPath's by-value result into one slot, COPY-CONSTRUCTS a
    // second Filename from it, and releases the first. It is not a wasted copy
    // that can be elided: `GetPath` hands back a Filename holding a NEW
    // reference, the copy takes a SECOND one, and the release drops the first —
    // so collapsing the pair would leave the refcount one too high on every one
    // of the ~22.6k calls a run makes. Transcribed as retail wrote it.
    Filename tmp{};
    tmp.data = nullptr;
    RT_EXPORT_ONCE(GetPathFn,
        "?GetPath@External@Object@@QEBA?AVFilename@System@@XZ")(external, &tmp);

    Filename path{};
    path.data = nullptr;
    RtSystem::RtSystem_Filename_CtorCopy(&path, &tmp);
    ReleaseInline(&tmp);

    // ---- 1800be5e2..1800be6b8 -------------------------------------------
    // The descent, inlined by MSVC rather than calling the exported
    // `Set<ResourceEntry>::Find` — and it has to be inlined here, because this
    // one needs the NODE (to update its usageMask in place) while `Find`
    // returns the `External*`.
    rtresset::Set* const set = reinterpret_cast<rtresset::Set*>(&self->_resources);
    rtresset::Node* node = set->root;
    if (node == nullptr) ++g_ac.emptyTree;

    while (node != nullptr) {
        const char* const nodeKey = rtresset::Elem(node)->path.data;
        if (FilenameLess(path.data, nodeKey)) {
            ++g_ac.descLeft;
            node = node->left;                                  // 1800be6bb
            continue;
        }
        if (FilenameLess(nodeKey, path.data)) {
            ++g_ac.descRight;
            node = node->right;                                 // 1800be6b2
            continue;
        }
        break;                                                  // equal key
    }

    // The clean-generation bit for THIS generation. `_cleanBitIndex` is a plain
    // int and retail masks the shift count with `& 0x1f` (`SHL EAX,CL` on x86
    // does that in hardware; the mask is explicit in the decompilation and is
    // reproduced rather than relied on, because C++ shifts by >= 32 are UB).
    const uint32_t bit = 1u << (static_cast<uint32_t>(self->_cleanBitIndex) & 31u);

    if (node != nullptr) {
        // ---- HIT: 1800be68b..1800be6c7 ----------------------------------
        ++g_ac.hit;
        ResourceEntry* const entry = rtresset::Elem(node);
        const uint32_t before = entry->usageMask;
        entry->usageMask = before | bit;                        // 1800be694

        if ((bit & before) == 0) {
            // First time this resource has been touched in this clean
            // generation. Run the reference-visit pass.
            ++g_ac.hitVisited;

            // ---- the stack-constructed IO::VisitorStream ----------------
            // 0x18 bytes, and DELIBERATELY not zero-initialised: the ctor
            // writes every field it uses (vptr, direction, encoding) and
            // `Stream::pad0c` is one retail never touches. A `= {}` here would
            // be a behavioural difference from retail, not tidiness — the same
            // position saveload_meta.cpp takes on CompressedStream +0x18/+0x24.
            RtIo::VisitorStream vs;
            RT_EXPORT_ONCE(VisitorCtorFn, "??0VisitorStream@IO@@QEAA@XZ")(&vs);

            ExternalSerialize(external, &vs);                   // 1800be6a9

            // ---- the INLINED ~VisitorStream -----------------------------
            // The inherited draft claimed retail's inlined copy makes ONE
            // vtable stamp and emitted only the second. It makes TWO, exactly
            // like the out-of-line `??1VisitorStream` (0x1800bb040):
            //     1800be673  lea rax,[rip+...]  # 0x180155a20  ??_7VisitorStream
            //     1800be67a  mov [rbp-0x18],rax
            //     1800be67e  lea rax,[rip+...]  # 0x1801549a0  ??_7Stream
            //     1800be685  mov [rbp-0x18],rax
            // Nothing between the two stores can observe the intermediate, so
            // this is not a behavioural fix -- but the draft's claim was a
            // statement about retail that was false, and the transcription is
            // free.
            vs.base.vptr = RT_DATA_SLOT(const void*, "??_7VisitorStream@IO@@6B@");  // 1800be67a
            vs.base.vptr = RT_DATA_SLOT(const void*, "??_7Stream@IO@@6B@");         // 1800be685
            if (vs.base.encoding) EncodingDelete(vs.base.encoding);
        } else {
            ++g_ac.hitSkipped;
        }
    } else {
        // ---- MISS: 1800be699..1800be6c2 ---------------------------------
        ++g_ac.inserted;

        // The 0x10-byte value half, built on the stack SEPARATELY from the key
        // and handed in a different register — which is why external_set.h
        // gives it its own type rather than passing a whole ResourceEntry.
        //
        // `referenceId = -1` is the seed External.h records; it is NOT a
        // "not found" sentinel invented here.
        rtresset::EntryValue value;
        value.external    = external;
        value.usageMask   = bit;
        value.referenceId = 0xffffffffu;

        rtresset::Node* out = nullptr;
        rtresset::Insert(set, &out, &path, &value);             // 1800be6bd
    }

    // ---- 1800be6c7..1800be6f7 -------------------------------------------
    // Released on EVERY path, hit and miss alike. `Insert` copies the key into
    // the node and takes its own reference, so skipping this on the insert path
    // would leak one string reference per resource.
    ReleaseInline(&path);

    RT_DIFF_WORK(true);
}

void DumpAddCounters() {
    printf("[ExtAdd] calls=%llu insert=%llu hit=%llu "
                "(visited=%llu skipped=%llu) descL=%llu descR=%llu empty=%llu\n",
                (unsigned long long)g_ac.calls,
                (unsigned long long)g_ac.inserted,
                (unsigned long long)g_ac.hit,
                (unsigned long long)g_ac.hitVisited,
                (unsigned long long)g_ac.hitSkipped,
                (unsigned long long)g_ac.descLeft,
                (unsigned long long)g_ac.descRight,
                (unsigned long long)g_ac.emptyTree);
}

}  // namespace RtObject

// ============================================================================
// Registration
// ============================================================================
int RtObjectExternalAddRegister() {
    int n = 0;
    RegisterRuntimeExport("?Add@ExternalFactory@Object@@QEAAXPEAVExternal@2@@Z",
                          (void*)&RtObject::RtObject_ExternalFactory_Add); ++n;
    RegisterRuntimeAddr(0x1800be570, (void*)&RtObject::RtObject_ExternalFactory_Add);

    printf("[RtPort]   Object::ExternalFactory::Add "
                "(wave 11f lane IOSTREAM): %d export registered\n", n);

    // WAS MISSING IN THE INHERITED DRAFT, AND IT WAS THE FILE'S OWN POSITIVE
    // CONTROL. `DumpAddCounters` was defined and never scheduled, so `g_ac` --
    // the counter this file's comments repeatedly call "the positive control
    // that says so from inside this function" -- could never print. An
    // instrument that is never reached prints a plausible ZERO, not an error;
    // here it printed nothing at all. Both sibling files in this directory
    // (external_find.cpp:302, external_path_win.cpp:993) schedule theirs the
    // same way.
    std::atexit(&RtObject::DumpAddCounters);
    return n;
}
