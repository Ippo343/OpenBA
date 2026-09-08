// ============================================================================
// runtime/src/achievement/goal.cpp — Wave 5 lane A — Achievement::Goal /
//                                    GoalInstance / GoalList
// ============================================================================
// Ports 13 of the 15 REAL exports in this family that our tree calls.
//
// *** THE TWO HOLDBACKS ARE NO LONGER HELD BACK — see goal_serialize.cpp. ***
// This banner used to end here saying that `Goal::Serialize` and
// `GoalInstance::Persist` "stay on the DLL", because no oracle could run both
// implementations without double-consuming the stream. That was true of the
// READ path and was believed of both. Wave 9 lane S showed it is false on the
// WRITE path — a write-path Serialize is a pure read of the object, so it
// replays into two independent synthetic sinks — and Wave 9 lane P ported both
// against that oracle (runtime/src/achievement/goal_serialize.cpp, which also
// records the two new lane-D rows the ports create).
//
// The desync WORRY behind the old note stands and is why those two bodies are
// commented field by field: a wrong record order or count there is the silent
// save corruption this project has paid for repeatedly (see reg_goal.cpp's own
// comment on slot 0x140152868, and reference_slot3_wrapper_must_resolve_first.md).
// Their reversed bodies are in docs/runtime_port/WAVE5_LANE_A.md, and Wave 9's
// reading of the same bytes agrees with them.
//
// *** THE BRIEF'S "20 EXPORTS" ARE NOT 20. Four of the names do not exist. ***
// `??1Goal@Achievement@@QEAA@XZ`, `??1GoalInstance@Achievement@@QEAA@XZ`,
// `?Persist@GoalInstance@Achievement@@QEAAXAEAVStream@IO@@@Z` and
// `?~Goal@Achievement@@QEAA@XZ` are ALL absent from the shipped DLL's export
// directory. The first three are the QEAA (non-virtual) spelling of members
// that are virtual and therefore exported as UEAA; the fourth is not a valid
// MSVC mangling at all. Two of them are live in `code/Game/Goal/` today and
// resolve to null forever — see the wave doc §5, this file cannot fix them.
//
// Every body below was disassembled at its retail address (all verified
// non-phantom against the shipped export table before a line was written) and
// is transcribed instruction-for-instruction. No null guards retail lacks.
//
// PLAN §6 step 8: this file corrects nothing outside itself; the stale claims
// this lane disproved are listed in the wave doc's "comments to correct".
// ============================================================================

#include <cstdint>
#include <cstdio>
#include <cstring>

#include "../../funcs/rt_port.h"
#include "../../types/achievement/Goal.h"
#include "../system/string.h"   // RtString_ReleaseInline (Wave 2, lane String)

using RtAchievement::Goal;
using RtAchievement::GoalInstance;
using RtAchievement::GoalList;
using RtSystem::StringRefCount;

// ----------------------------------------------------------------------------
// ResolveAddr, forward-declared rather than pulled in from standalone/resolve.h
// -----------------------------------------------------------------------------
// Same reasoning rt_port.h gives for declaring the seam locally: resolve.h drags
// in <windows.h>, and PLAN §4.1 requires runtime/src/ to stay platform-neutral.
// This declaration must stay byte-identical to resolve.h's; the two are checked
// against each other by the linker and by standalone/resolve_standalone.cpp.
// It has to sit at FILE scope, not inside the anonymous namespace below, or it
// would declare a different, internal-linkage function that never links.
enum class Module : int { Exe = 0, Runtime = 1, VideoDX11 = 2, COUNT = 3 };
uintptr_t ResolveAddr(Module mod, uintptr_t ghidra_addr);

namespace {

// ---------------------------------------------------------------------------
// Dependencies, and why each is reached the way it is
// ---------------------------------------------------------------------------
// Three of these bodies call a base-class ctor/dtor that ANOTHER LANE HAS
// ALREADY PORTED (`Object::Internal` and `Object::External`, Wave 3, registered
// by BOTH name and address — runtime/src/object/{internal,external}.cpp). So
// the choice of lookup here is not cosmetic:
//
//   * ResolveAddr / ResolveExport go through the runtime registry, so they
//     pick up whatever Wave 3 registered.
//   * ResolveExportRaw / RtOriginal is GetProcAddress: the SHIPPED DLL, always,
//     whatever any lane has registered.
//
// These three go through the REGISTRY, and that is a deliberate reversal of the
// position WAVE4_LANE_C.md took for `String::Format` ("always ask the DLL, so
// this lane is correct standing alone"). The reason is that here the DLL is not
// the only thing on the other side of the call — an ORACLE is:
//
//   `Object::Internal`'s ctor/dtor have a predict-and-verify shadow wrapper
//   (RtObject_Internal_Ctor_Shadow / _Dtor_Shadow, internal.cpp:655/681, armed
//   with BANISHED_RT_INTERNAL_SHADOW=1) that checks the handle table's size,
//   capacity, the object's factoryIndex, the cleared generation and the
//   swap-remove fixup. Wave 3 ran it clean at 93,387 calls. A raw
//   GetProcAddress here would step straight PAST that wrapper, so this lane's
//   teardown path — the one that reaches ~Internal — would be the one path in
//   the tree the handle-table oracle cannot see. That is a real cost and it is
//   the whole reason for the choice.
//
// The "correct standing alone" property mostly survives: the shadow wrappers
// call the DLL's own body and only observe. It is genuinely given up only if
// someone arms BANISHED_RT_INTERNAL_TABLE=1, Wave 3's real handle-table port,
// which carries a standing NO-GO — and at that point PLAN §4.1 decision 5
// ("stateful subsystems switch atomically as a class") says every caller should
// flip together anyway, this one included.
//
// Cached in a function-local static, so the registry lookup happens once per
// site per process, not once per call.
// ---------------------------------------------------------------------------

// (ResolveAddr is forward-declared at file scope, just above this namespace.)

// The three vtables these constructors and destructors stamp. All EXPORTED data
// symbols, so we install the DLL's own table and never own one of ours; an
// object built here is byte-identical to one built by the DLL and dispatches
// into the DLL for every slot this lane did not port.
void* BaseVtable() {
    static void* p = ResolveExportRaw("??_7Base@Object@@6B@");           // 0x180154788
    return p;
}
void* ExternalVtable() {
    static void* p = ResolveExportRaw("??_7External@Object@@6B@");       // 0x1801547a0
    return p;
}
void* GoalVtable() {
    static void* p = ResolveExportRaw("??_7Goal@Achievement@@6B@");      // 0x1801546b0
    return p;
}
void* GoalInstanceVtable() {
    static void* p = ResolveExportRaw("??_7GoalInstance@Achievement@@6B@"); // 0x180154678
    return p;
}

using VoidThisFn = void (*)(void*);
using PtrThisFn  = void* (*)(void*);

// ??0Internal@Object@@QEAA@XZ  retail 0x1800bfd10 — allocates this object's
// handle-table slot. GoalInstance's constructor CALLs it (not inlined).
void CallInternalCtor(void* self) {
    static const PtrThisFn fn =
        reinterpret_cast<PtrThisFn>(ResolveAddr(Module::Runtime, 0x1800bfd10));
    fn(self);
}
// ??1Internal@Object@@UEAA@XZ  retail 0x1800bfdb0 — releases it, by swap-remove
// out of the InternalFactory's table. Reached by a TAIL JMP from
// ~GoalInstance, so it runs with the frame already torn down; a plain call here
// is equivalent because it takes only `this`.
void CallInternalDtor(void* self) {
    static const VoidThisFn fn =
        reinterpret_cast<VoidThisFn>(ResolveAddr(Module::Runtime, 0x1800bfdb0));
    fn(self);
}
// ??1External@Object@@UEAA@XZ  retail 0x1800bc950 — same shape, tail-jumped
// from ~Goal. Wave 3 ported this one for real and DEFAULT-ON
// (RtObject_External_Dtor, external.cpp:207), so every other caller in the tree
// already reaches the port; going through the registry here keeps this lane
// from being the single anomalous caller that does not.
void CallExternalDtor(void* self) {
    static const VoidThisFn fn =
        reinterpret_cast<VoidThisFn>(ResolveAddr(Module::Runtime, 0x1800bc950));
    fn(self);
}

// The dword retail's Goal::GetInstanceType copies out. Retail reads
// `dword ptr [0x1801f9940]`, which is the ADDRESS OF THE EXPORTED DATA SYMBOL
// ?_typeInfo@GoalInstance@Achievement@@0VBaseTypeInfo@Object@@A — so this reads
// the same four bytes retail reads, without needing to model BaseTypeInfo at
// all. (Note the class asymmetry, which is not a typo: Goal::GetInstanceType
// returns GOALINSTANCE's type, because "the instance type" is the type
// GoalList::Initialize must ask the factory to create for this goal.)
const uint32_t* GoalInstanceTypeInfo() {
    static const uint32_t* p = static_cast<const uint32_t*>(
        ResolveExportRaw("?_typeInfo@GoalInstance@Achievement@@0VBaseTypeInfo@Object@@A"));
    return p;
}

}  // namespace

// ============================================================================
// Achievement::Goal::Goal()
// ??0Goal@Achievement@@QEAA@XZ   retail 0x18009ed50, ordinal 207
//
// ORACLE: FULL BYTE COMPARE. The constructor calls nothing and touches no
// global — it is six qword stores into `this` covering 0x00..0x2f exactly — so
// the original can be run into a private scratch buffer and the two objects
// compared byte for byte. That is a real oracle, not a self-check: it would
// catch a wrong vtable, a missed field, or a wrong store order's residue.
// ============================================================================
Goal* RtAch_Goal_Ctor(Goal* self) {
    RT_DIFF_BEGIN("Achievement::Goal::Goal");

    // The two dead vtable stores are MSVC's inlined base constructors. They are
    // overwritten below before anything can read them; transcribed anyway
    // because "byte-faithful first, readable second" (PLAN §6 step 4) and
    // because a future reader comparing against the disassembly should find
    // every instruction accounted for.
    *reinterpret_cast<void**>(self) = BaseVtable();
    *reinterpret_cast<void**>(self) = ExternalVtable();
    self->_base._path.data          = nullptr;   // +0x08
    self->_base._refs._data         = nullptr;   // +0x10
    self->_base._refs._size         = 0;         // +0x18 — one qword store covering
    self->_base._refs._capacity     = 0;         //         both size and capacity
    *reinterpret_cast<void**>(self) = GoalVtable();
    self->_stringTable.ptr          = nullptr;   // +0x20
    self->_text.data                = nullptr;   // +0x28

    if (rt_diff_on__) {
        // 0xCC so a field the original leaves untouched would show up as a
        // difference rather than as an accidental match on zero.
        alignas(16) uint8_t scratch[sizeof(Goal)];
        std::memset(scratch, 0xCC, sizeof(scratch));
        RT_ORIGINAL_ONCE(PtrThisFn, "??0Goal@Achievement@@QEAA@XZ")(scratch);
        RT_DIFF_BYTES(self, scratch, sizeof(Goal));
    }
    return self;   // retail returns `this` in RAX
}

// ============================================================================
// Achievement::Goal::~Goal()
// ??1Goal@Achievement@@UEAA@XZ   retail 0x18009edb0, ordinal 600
//
// ORACLE: SHADOW SELF-CHECK ONLY, and that is a real limitation, not a
// formality. The destructor releases a refcounted String (possibly freeing it)
// and then runs Object::External's destructor, which frees an owning array.
// Running the original as well would double-free. There is also no scratch
// stand-in to destroy: the String this object holds is SHARED with whoever else
// copied it, so a scratch copy would decrement the same refcount.
// So the check below verifies what this body itself is responsible for — the
// vtable re-stamp and the String release having done exactly one of its two
// legal things — and nothing about External's half. See the wave doc §4.
// ============================================================================
void RtAch_Goal_Dtor(Goal* self) {
    RT_DIFF_BEGIN("Achievement::Goal::~Goal");

    char* const  textBefore = self->_text.data;
    const int32_t rcBefore  = textBefore ? StringRefCount(textBefore) : 0;

    *reinterpret_cast<void**>(self) = GoalVtable();

    // The String release idiom, byte-identical to Wave 2's
    // RtString_ReleaseInline (runtime/src/system/string.cpp:98) — including
    // retail's asymmetry, which looks like a bug and is not ours to fix: on the
    // FREEING branch the field is left pointing at `data - 8`, on the SHARING
    // branch it is nulled.
    RtSystem::RtString_ReleaseInline(&self->_text);

    if (rt_diff_on__) {
        const bool vtOk = (*reinterpret_cast<void**>(self) == GoalVtable());
        bool strOk;
        if (textBefore == nullptr) {
            strOk = (self->_text.data == nullptr);
        } else if (rcBefore == 1) {
            // freed: field left at data-8, block handed back to the allocator
            strOk = (self->_text.data == textBefore - 8);
        } else {
            // shared: refcount down exactly one, field nulled
            strOk = (self->_text.data == nullptr) && (StringRefCount(textBefore) == rcBefore - 1);
        }
        char a[96], b[64];
        std::snprintf(a, sizeof(a), "vt=%d text=%d rc:%d->%d", (int)vtOk, (int)strOk,
                      (int)rcBefore, textBefore ? (int)StringRefCount(textBefore) : 0);
        std::snprintf(b, sizeof(b), "self-check (original not run: would double-free)");
        RT_DIFF_SHADOW(vtOk && strOk, a, b);
    }

    // Retail TAIL-JUMPS here (`JMP 0x1800bc950`), so ~External finishes the job
    // and its return is ours.
    CallExternalDtor(self);
}

// ============================================================================
// Achievement::GoalInstance::GoalInstance()
// ??0GoalInstance@Achievement@@QEAA@XZ   retail 0x18009ef00, ordinal 209
//
// ORACLE: SHADOW SELF-CHECK. Object::Internal's constructor claims a slot in
// the global handle table, so a scratch construction would consume one — and
// the scratch object's factoryIndex/generation would legitimately differ from
// the real one's, so even a byte compare of the base would be meaningless. The
// check below covers the three stores this body owns.
// ============================================================================
GoalInstance* RtAch_GoalInstance_Ctor(GoalInstance* self) {
    RT_DIFF_BEGIN("Achievement::GoalInstance::GoalInstance");

    CallInternalCtor(self);
    *reinterpret_cast<void**>(self) = GoalInstanceVtable();
    self->_flags    = 0;         // +0x10, a DWORD store — +0x14 is left alone
    self->_goal.ptr = nullptr;   // +0x18

    if (rt_diff_on__) {
        const bool ok = (*reinterpret_cast<void**>(self) == GoalInstanceVtable()) &&
                        (self->_flags == 0) && (self->_goal.ptr == nullptr);
        char a[96];
        std::snprintf(a, sizeof(a), "vt=%p flags=%u goal=%p idx=%d",
                      *reinterpret_cast<void**>(self), self->_flags,
                      self->_goal.ptr, (int)self->_base.factoryIndex);
        RT_DIFF_SHADOW(ok, a, "self-check (original not run: would consume a handle slot)");
    }
    return self;   // retail returns `this` in RAX
}

// ============================================================================
// Achievement::GoalInstance::~GoalInstance()
// ??1GoalInstance@Achievement@@UEAA@XZ   retail 0x18009ef40, ordinal 601
//
// Two instructions and a tail jump. ORACLE: shadow self-check — running the
// original would release the handle-table slot twice.
// ============================================================================
void RtAch_GoalInstance_Dtor(GoalInstance* self) {
    RT_DIFF_BEGIN("Achievement::GoalInstance::~GoalInstance");

    *reinterpret_cast<void**>(self) = GoalInstanceVtable();

    if (rt_diff_on__) {
        char a[64];
        std::snprintf(a, sizeof(a), "vt=%p", *reinterpret_cast<void**>(self));
        RT_DIFF_SHADOW(*reinterpret_cast<void**>(self) == GoalInstanceVtable(), a,
                       "self-check (original not run: would free the handle slot twice)");
    }

    CallInternalDtor(self);   // retail: JMP 0x1800bfdb0
}

// ============================================================================
// int Achievement::GoalList::GetGoalCount() const
// ?GetGoalCount@GoalList@Achievement@@QEBAHXZ   retail 0x180001700, ordinal 2684
//
// One instruction. ORACLE: full.
// ============================================================================
int RtAch_GoalList_GetGoalCount(const GoalList* self) {
    RT_DIFF_BEGIN("Achievement::GoalList::GetGoalCount");
    const int ours = self->_goals._size;
    using Fn = int (*)(const GoalList*);
    RT_DIFF_SCALAR(ours, RT_ORIGINAL_ONCE(Fn, "?GetGoalCount@GoalList@Achievement@@QEBAHXZ")(self));
    return ours;
}

// ============================================================================
// InternalPtr<GoalInstance> Achievement::GoalList::GetGoal(int) const
// ?GetGoal@GoalList@Achievement@@QEBA?AV?$InternalPtr@VGoalInstance@Achievement@@@Object@@H@Z
//                                              retail 0x180001710, ordinal 2683
//
// HIDDEN RETURN SLOT: returns InternalPtr<T> BY VALUE, so the ABI is
// (RCX=this, RDX=retval, R8=index) — the trap
// reference_hidden_retslot_wild_write.md exists for. `index` is the THIRD
// argument, not the second. Retail does NOT bounds-check it; neither do we.
//
// ORACLE: full. Read-only on both sides, so the original can run into a private
// scratch slot.
// ============================================================================
void* RtAch_GoalList_GetGoal(const GoalList* self, void* retval, int index) {
    RT_DIFF_BEGIN("Achievement::GoalList::GetGoal");
    void* const ours = self->_goals._data[index].ptr;
    *static_cast<void**>(retval) = ours;

    if (rt_diff_on__) {
        using Fn = void* (*)(const GoalList*, void*, int);
        void* theirs = nullptr;
        RT_ORIGINAL_ONCE(Fn, "?GetGoal@GoalList@Achievement@@QEBA?AV?$InternalPtr@VGoalInstance@Achievement@@@Object@@H@Z")(
            self, &theirs, index);
        RT_DIFF_SCALAR(ours, theirs);
    }
    return retval;   // retail returns the slot in RAX
}

// ============================================================================
// ExternalPtr<Goal> Achievement::GoalInstance::GetGoal() const
// ?GetGoal@GoalInstance@Achievement@@QEBA?AV?$ExternalPtr@VGoal@Achievement@@@Object@@XZ
//                                              retail 0x180001fe0, ordinal 2682
//
// Same hidden-return-slot shape, no index. ORACLE: full.
// ============================================================================
void* RtAch_GoalInstance_GetGoal(const GoalInstance* self, void* retval) {
    RT_DIFF_BEGIN("Achievement::GoalInstance::GetGoal");
    void* const ours = self->_goal.ptr;
    *static_cast<void**>(retval) = ours;

    if (rt_diff_on__) {
        using Fn = void* (*)(const GoalInstance*, void*);
        void* theirs = nullptr;
        RT_ORIGINAL_ONCE(Fn, "?GetGoal@GoalInstance@Achievement@@QEBA?AV?$ExternalPtr@VGoal@Achievement@@@Object@@XZ")(
            self, &theirs);
        RT_DIFF_SCALAR(ours, theirs);
    }
    return retval;
}

// ============================================================================
// Object::Type Achievement::Goal::GetInstanceType() const
// ?GetInstanceType@Goal@Achievement@@UEBA?AVType@Object@@XZ
//                                              retail 0x18009eeb0, ordinal 2724
//
// HIDDEN RETURN SLOT EVEN THOUGH THE RESULT IS FOUR BYTES: `Object::Type` is a
// class (`?AV` in the mangling), and MSVC returns a class by value through the
// slot regardless of size. The disassembly settles it — `MOV dword ptr [RDX],EAX
// / MOV RAX,RDX`. Getting this one wrong is a wild write, not a wrong value.
//
// ORACLE: full — a pure read of a DLL global into a private scratch dword.
// ============================================================================
void* RtAch_Goal_GetInstanceType(const Goal* self, void* retval) {
    RT_DIFF_BEGIN("Achievement::Goal::GetInstanceType");
    (void)self;   // retail never reads `this`; the answer is a class constant
    const uint32_t ours = *GoalInstanceTypeInfo();
    *static_cast<uint32_t*>(retval) = ours;

    if (rt_diff_on__) {
        using Fn = void* (*)(const Goal*, void*);
        uint32_t theirs = 0;
        RT_ORIGINAL_ONCE(Fn, "?GetInstanceType@Goal@Achievement@@UEBA?AVType@Object@@XZ")(self, &theirs);
        RT_DIFF_SCALAR(ours, theirs);
    }
    return retval;
}

// ============================================================================
// System::String Achievement::Goal::GetText() const
// ?GetText@Goal@Achievement@@QEBA?AVString@System@@XZ  retail 0x180001ba0, ord 2958
//
// HIDDEN RETURN SLOT (String by value), and the body ADDREFS — so this is the
// WAVE4_LANE_C GetSavePath shape: the original really is run, and the extra
// reference its copy took is released again before returning. Both sides hand
// back the SAME block, so comparing the raw pointers is exact and strictly
// stronger than comparing contents.
// ============================================================================
void* RtAch_Goal_GetText(const Goal* self, void* retval) {
    RT_DIFF_BEGIN("Achievement::Goal::GetText");

    RtSystem::String* out = static_cast<RtSystem::String*>(retval);
    out->data = nullptr;
    char* const src = self->_text.data;
    if (src != nullptr) {
        out->data = src;
        ++StringRefCount(src);
    }

    if (rt_diff_on__) {
        using Fn = void* (*)(const Goal*, void*);
        RtSystem::String theirs{};
        RT_ORIGINAL_ONCE(Fn, "?GetText@Goal@Achievement@@QEBA?AVString@System@@XZ")(self, &theirs);
        char* t = theirs.data;
        // Release the oracle call's own reference FIRST, so the refcount is
        // back where our own body left it whichever way the compare goes.
        RtSystem::RtString_ReleaseInline(&theirs);
        RT_DIFF_SCALAR(static_cast<void*>(out->data), static_cast<void*>(t));
    }
    return retval;
}

// ============================================================================
// bool Achievement::GoalInstance::IsMet() const
// ?IsMet@GoalInstance@Achievement@@QEBA_NXZ   retail 0x18009eed0, ordinal 3141
// bool Achievement::GoalInstance::HasFailed() const
// ?HasFailed@GoalInstance@Achievement@@QEBA_NXZ  retail 0x18009eec0, ordinal 3046
//
// One bit each out of the +0x10 flag dword. ORACLE: full.
// ============================================================================
bool RtAch_GoalInstance_IsMet(const GoalInstance* self) {
    RT_DIFF_BEGIN("Achievement::GoalInstance::IsMet");
    const bool ours = (self->_flags & RtAchievement::GoalStateBits::kMet) != 0;
    using Fn = bool (*)(const GoalInstance*);
    RT_DIFF_SCALAR(ours, RT_ORIGINAL_ONCE(Fn, "?IsMet@GoalInstance@Achievement@@QEBA_NXZ")(self));
    return ours;
}

bool RtAch_GoalInstance_HasFailed(const GoalInstance* self) {
    RT_DIFF_BEGIN("Achievement::GoalInstance::HasFailed");
    const bool ours = ((self->_flags >> 1) & 1u) != 0;
    using Fn = bool (*)(const GoalInstance*);
    RT_DIFF_SCALAR(ours, RT_ORIGINAL_ONCE(Fn, "?HasFailed@GoalInstance@Achievement@@QEBA_NXZ")(self));
    return ours;
}

// ============================================================================
// bool Achievement::GoalList::AreGoalsMetExcept(InternalPtr<GoalInstance>) const
// ?AreGoalsMetExcept@GoalList@Achievement@@QEBA_NV?$InternalPtr@VGoalInstance@Achievement@@@Object@@@Z
//                                              retail 0x18009f070, ordinal 1609
//
// The parameter is a class passed BY VALUE, so MSVC passes the caller's copy BY
// ADDRESS in RDX — `MOV RDX,qword ptr [RDX]` at 0x18009f07c is retail loading
// the pointer OUT of that copy. Declaring it as a plain `GoalInstance*` would
// compile and be wrong at every call.
//
// TWO DETAILS THE DECOMPILER GETS WRONG, both taken from the disassembly:
//   * the count is loaded ONCE into R9 at entry (`MOVSXD R9,[RCX+8]`) and the
//     loop compares against R9 — Ghidra's C re-reads `*(int*)(this+8)` in the
//     condition, which would behave differently if a CheckGoalState side effect
//     resized the list.
//   * the element read is an UNGUARDED deref: `TEST byte ptr [RCX+0x10],0x1`
//     with RCX straight out of the array. A null entry faults, in retail too.
//     CLAUDE.md: do not "fix" that.
//
// ORACLE: full — read-only on both sides.
// ============================================================================
bool RtAch_GoalList_AreGoalsMetExcept(const GoalList* self, const void* exceptPtr) {
    RT_DIFF_BEGIN("Achievement::GoalList::AreGoalsMetExcept");

    bool ours = true;
    const int64_t count = self->_goals._size;      // loaded once, as retail does
    if (count > 0) {
        const GoalInstance* except =
            *static_cast<const GoalInstance* const*>(exceptPtr);
        const RtObject::InternalBasePtr* p = self->_goals._data;
        for (int64_t i = 0; i < count; ++i, ++p) {
            const GoalInstance* g = reinterpret_cast<const GoalInstance*>(p->ptr);
            if (g != except && (*reinterpret_cast<const uint8_t*>(
                    reinterpret_cast<const uint8_t*>(g) + 0x10) & 1) == 0) {
                ours = false;
                break;
            }
        }
    }

    using Fn = bool (*)(const GoalList*, const void*);
    RT_DIFF_SCALAR(ours, RT_ORIGINAL_ONCE(Fn,
        "?AreGoalsMetExcept@GoalList@Achievement@@QEBA_NV?$InternalPtr@VGoalInstance@Achievement@@@Object@@@Z")(
            self, exceptPtr));
    return ours;
}

// ============================================================================
// void Achievement::GoalInstance::Update(const System::Time&, const GoalList&)
// ?Update@GoalInstance@Achievement@@UEAAXAEBVTime@System@@AEBVGoalList@2@@Z
//                                              retail 0x18009f010, ordinal 4492
//
// Retail reads NEITHER argument. Both are there for the overrides: the subclass
// hierarchy is in the exe (TimeGoalInstance et al.), and this base body only
// polls the virtual CheckGoalState (GoalInstance vtable +0x30) and folds the
// answer into the flag dword. The "failed" bit LATCHES — once set, the entry
// guard makes every later Update a no-op.
//
// ORACLE: FULL, by save-restore. CheckGoalState is a `const` member (UEBA in
// its mangling) and every override in our tree is a query, so it is safe to
// call twice; the only state Update itself writes is the +0x10 dword, so the
// original can be run first, its answer captured, the dword restored, and ours
// run for real. Both calls hit the same virtual, so this really does compare
// two implementations rather than compare ours to itself.
// ============================================================================
namespace {
// The retail body, exactly. Split out so the oracle can run it after the
// original without duplicating it.
void GoalInstance_UpdateBody(GoalInstance* self) {
    if (((self->_flags >> 1) & 1u) != 0)
        return;

    void** vt = *reinterpret_cast<void***>(self);
    using CheckFn = int32_t (*)(GoalInstance*);
    const int32_t state = reinterpret_cast<CheckFn>(
        vt[RtAchievement::GoalInstanceVtbl::kCheckGoalState / 8])(self);

    if (state == 1) { self->_flags |= 1u;  return; }
    if (state == 0) { self->_flags &= 0xfffffffeu; return; }
    if (state == 2) { self->_flags |= 2u; }
}
}  // namespace

void RtAch_GoalInstance_Update(GoalInstance* self, const void* time, const void* list) {
    RT_DIFF_BEGIN("Achievement::GoalInstance::Update");

    if (rt_diff_on__) {
        const uint32_t before = self->_flags;
        using Fn = void (*)(GoalInstance*, const void*, const void*);
        RT_ORIGINAL_ONCE(Fn,
            "?Update@GoalInstance@Achievement@@UEAAXAEBVTime@System@@AEBVGoalList@2@@Z")(
                self, time, list);
        const uint32_t theirs = self->_flags;
        self->_flags = before;            // undo theirs; +0x10 is all Update writes
        GoalInstance_UpdateBody(self);
        RT_DIFF_SCALAR(self->_flags, theirs);
        return;
    }
    GoalInstance_UpdateBody(self);
}

// ============================================================================
// Registration
// ============================================================================
// `?Serialize@Goal@Achievement@@UEAA_NAEAVStream@IO@@@Z` (0x18009ee20) and
// `?Persist@GoalInstance@Achievement@@UEAAXAEAVStream@IO@@@Z` (0x18009ef70) are
// absent HERE but no longer absent from the build: Wave 9 lane P registers both
// from runtime/src/achievement/goal_serialize.cpp, under this same
// BANISHED_RT_GOAL switch. They live in their own file only so that two lanes
// never edit one file, not because they are still held back — the "DELIBERATELY
// ABSENT, see WAVE5_LANE_A.md §4" this line used to say is superseded.
//
// Both the exported NAME and the retail ADDRESS are registered for every port.
// Registering only the name would leave `ResolveAddr(Module::Runtime, ...)`
// callers on the DLL, which is the gap WAVE3_LANE_A.md found the hard way:
// `engine/funcs/engine_funcs.cpp` falls back to an address on a failed name
// resolve, and for `Persist` that fallback is the ONLY path that ever works
// (its name there is a phantom — see §5 of the wave doc).
//
// Every mangled name below is spelled as ONE string literal on ONE line:
// tools/runtime_census.py and tools/export_name_audit.py cannot see a name
// split across lines or hidden behind a named constant, and a port the census
// cannot see is a port nobody will ever check
// (reference_census_cannot_see_nonliteral_registrations.md).
int RtAchGoalRegister() {
    int n = 0;

    RegisterRuntimeExport("??0Goal@Achievement@@QEAA@XZ", (void*)&RtAch_Goal_Ctor); ++n;
    RegisterRuntimeAddr(0x18009ed50, (void*)&RtAch_Goal_Ctor);

    RegisterRuntimeExport("??1Goal@Achievement@@UEAA@XZ", (void*)&RtAch_Goal_Dtor); ++n;
    RegisterRuntimeAddr(0x18009edb0, (void*)&RtAch_Goal_Dtor);

    RegisterRuntimeExport("??0GoalInstance@Achievement@@QEAA@XZ", (void*)&RtAch_GoalInstance_Ctor); ++n;
    RegisterRuntimeAddr(0x18009ef00, (void*)&RtAch_GoalInstance_Ctor);

    RegisterRuntimeExport("??1GoalInstance@Achievement@@UEAA@XZ", (void*)&RtAch_GoalInstance_Dtor); ++n;
    RegisterRuntimeAddr(0x18009ef40, (void*)&RtAch_GoalInstance_Dtor);

    RegisterRuntimeExport("?GetGoalCount@GoalList@Achievement@@QEBAHXZ", (void*)&RtAch_GoalList_GetGoalCount); ++n;
    RegisterRuntimeAddr(0x180001700, (void*)&RtAch_GoalList_GetGoalCount);

    RegisterRuntimeExport("?GetGoal@GoalList@Achievement@@QEBA?AV?$InternalPtr@VGoalInstance@Achievement@@@Object@@H@Z", (void*)&RtAch_GoalList_GetGoal); ++n;
    RegisterRuntimeAddr(0x180001710, (void*)&RtAch_GoalList_GetGoal);

    RegisterRuntimeExport("?GetGoal@GoalInstance@Achievement@@QEBA?AV?$ExternalPtr@VGoal@Achievement@@@Object@@XZ", (void*)&RtAch_GoalInstance_GetGoal); ++n;
    RegisterRuntimeAddr(0x180001fe0, (void*)&RtAch_GoalInstance_GetGoal);

    RegisterRuntimeExport("?GetInstanceType@Goal@Achievement@@UEBA?AVType@Object@@XZ", (void*)&RtAch_Goal_GetInstanceType); ++n;
    RegisterRuntimeAddr(0x18009eeb0, (void*)&RtAch_Goal_GetInstanceType);

    RegisterRuntimeExport("?GetText@Goal@Achievement@@QEBA?AVString@System@@XZ", (void*)&RtAch_Goal_GetText); ++n;
    RegisterRuntimeAddr(0x180001ba0, (void*)&RtAch_Goal_GetText);

    RegisterRuntimeExport("?IsMet@GoalInstance@Achievement@@QEBA_NXZ", (void*)&RtAch_GoalInstance_IsMet); ++n;
    RegisterRuntimeAddr(0x18009eed0, (void*)&RtAch_GoalInstance_IsMet);

    RegisterRuntimeExport("?HasFailed@GoalInstance@Achievement@@QEBA_NXZ", (void*)&RtAch_GoalInstance_HasFailed); ++n;
    RegisterRuntimeAddr(0x18009eec0, (void*)&RtAch_GoalInstance_HasFailed);

    RegisterRuntimeExport("?AreGoalsMetExcept@GoalList@Achievement@@QEBA_NV?$InternalPtr@VGoalInstance@Achievement@@@Object@@@Z", (void*)&RtAch_GoalList_AreGoalsMetExcept); ++n;
    RegisterRuntimeAddr(0x18009f070, (void*)&RtAch_GoalList_AreGoalsMetExcept);

    RegisterRuntimeExport("?Update@GoalInstance@Achievement@@UEAAXAEBVTime@System@@AEBVGoalList@2@@Z", (void*)&RtAch_GoalInstance_Update); ++n;
    RegisterRuntimeAddr(0x18009f010, (void*)&RtAch_GoalInstance_Update);

    printf("[RtPort] Achievement::Goal: %d/15 exports registered "
           "(Goal::Serialize + GoalInstance::Persist held back -- see WAVE5_LANE_A.md)\n", n);
    return n;
}
