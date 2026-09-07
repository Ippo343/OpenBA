// ============================================================================
// runtime/src/achievement/achievement_manager.cpp — Wave 5, lane B
// Achievement::Achievement / AchievementInstance / AchievementManager
// ============================================================================
//
// Ports 18 of the 19 CODE exports our tree calls on these three classes
// (docs/runtime_port/WAVE5_LANE_B.md has the per-function table). Retail
// addresses are in Runtime-x64.dll, image base 0x180000000; profile addresses
// are in Runtime-x64-profile.dll, the ToolKit build, which is the source of
// NAMES only (PLAN.md §6 step 1). Both were read in Ghidra on 2026-09-06.
//
// HELD BACK, deliberately, with the reasoning in the wave doc:
//   ?Persist@AchievementManager@Achievement@@QEAAXAEAVStream@IO@@@Z (0x18009e910)
//     — it inlines an IO::Serializer construction, and Wave 4 lane A measured
//       that a Serializer ctor CONSUMES OR EMITS A 12-BYTE STREAM HEADER
//       (runtime/types/io/Serializer.h). So it has no runnable oracle of any
//       kind: a second construction desyncs the save. It is also the one
//       function here whose failure mode is a silent save/load desync that
//       surfaces on an innocent later record — the exact family this project
//       has lost the most time to (reference_slot3_wrapper_must_resolve_first.md).
//       Unported, every caller keeps the DLL's version, unchanged.
//   ?_singleton@AchievementManager@Achievement@@0PEAV12@EA and the two
//   ?_typeInfo@… symbols are DATA exports; they cannot be registered before
//   cutover (same finding as Wave 4 lane D's FileSystem::_singleton).
//
// STATE, and what that does to the oracle. Only nine of these functions are
// pure reads; the other nine mutate the live singleton, a live instance, or
// the Windows registry. Running the DLL's copy of a mutator as a differential
// oracle would perform the mutation TWICE (rt_diff.h's third bullet), so those
// use RT_DIFF_SHADOW: ours does the real work, and the macro's verdict is an
// INVARIANT WE CHECK AFTERWARDS, in several cases by asking the DLL to read
// the state back. Each site says exactly what its verdict proves and what it
// does not. Read them as "this held", never as "ours == theirs".
//
// NO ADDED NULL GUARDS. Retail derefs `_achievements` in GetAchievementCount,
// `_completed._data` in Update and `_instances._data` in Reset without
// checking any of them, and those faults are load-bearing: they are how a
// manager that never got SetAchievements announces itself
// (reference_achievementmanager_has_no_list_until_setachievements.md, whose
// "faults on a READ of 0x28" is re-derived and confirmed in the header). The
// ports below fault in exactly the same places, now at an address inside
// reimpl-x64.exe rather than inside Runtime-x64.dll.
// ============================================================================

#include <cstdint>
#include <cstdio>
#include <cstring>

#include "../../funcs/rt_port.h"
#include "../../types/achievement/AchievementManager.h"
#include "../../types/io/FileSystem.h"
#include "../system/string.h"   // RtString_ReleaseInline (Wave 2, lane String)

using RtAchievement::Achievement;
using RtAchievement::AchievementInstance;
using RtAchievement::AchievementManager;
using RtAchievement::GoalInstanceView;
using RtAchievement::GoalListView;
using RtSystem::String;
using RtSystem::StringRefCount;

// ----------------------------------------------------------------------------
// ResolveAddr / ResolveExport, forward-declared rather than pulled in from
// standalone/resolve.h — same reasoning rt_port.h gives for declaring
// RegisterRuntimeExport locally: resolve.h drags in <windows.h>, and PLAN.md
// §4.1 requires runtime/src/ to stay platform-neutral. These declarations must
// stay byte-identical to resolve.h's; the linker checks them against it.
// ----------------------------------------------------------------------------
enum class Module : int { Exe = 0, Runtime = 1, VideoDX11 = 2, COUNT = 3 };
uintptr_t ResolveAddr(Module mod, uintptr_t ghidra_addr);
void*     ResolveExport(const char* mangled_name);

namespace {

// ============================================================================
// Dependencies this lane calls rather than reimplements.
//
// TWO DIFFERENT RESOLUTION RULES ARE USED BELOW, ON PURPOSE.
//
//  * ResolveAddr(Module::Runtime, …) — "always the DLL". Wave 4 lane C's rule
//    (runtime/src/io/persistence.cpp): a port should be correct standing alone
//    against the shipped DLL, not conditionally on another lane being enabled.
//    Used for the three Collection::Array<T>::SetSize instantiations, which
//    are not exported at all and belong to no lane.
//
//  * ResolveExport by name -- "whoever owns it now". Used for the FOUR
//    dependencies that touch state somebody else owns: Object::Internal's
//    constructor (the handle table — Wave 3), Base::IsDerivedFrom (Wave 3's
//    port is registered and default-on), and the two AchievementManager
//    members this lane does not port (ReadAchievements, SetAchievementMet).
//    Going around the registry for the Internal ctor would make Wave 3's
//    opt-in shadow replica of the handle table MISS an object and report a
//    false divergence; the registry is the single owner and must stay so.
// ============================================================================

// ---- Collection::Array<T>::SetSize, three instantiations -------------------
// One body per element width, because the engine's Array is a template and
// MSVC emits a separate instantiation per T (PLAN.md §4.3 risk 1's trap). All
// three allocate through System::MemoryAllocator::_singleton, which is the
// same one heap our exe uses, so calling the DLL's copy is not a heap split.
//
//   0x180008b90  Array<bool>::SetSize     — 1-byte elements, MemAlloc(n, 1)
//   0x180008ff0  Array<Time>::SetSize     — 8-byte, MemAlloc(n*8, 8), zero-fills new tail
//   0x180008d60  Array<InternalPtr>::SetSize — 8-byte, same, with the InternalPtr
//                                             clear-then-copy on the moved tail
//
// A NOTE FOR WHOEVER PORTS Collection::Array ITSELF: 0x180008b90 (the bool
// one) does NOT initialise elements past the old size, while its two 8-byte
// siblings do. That asymmetry is real in the shipped bytes, not a decompiler
// artefact, and AchievementManager::SetAchievements relies on nothing about
// it because ReadAchievements immediately writes every slot.
using ArraySetSizeFn = void (*)(void* array, int newSize);

ArraySetSizeFn ArraySetSizeBool() {
    static ArraySetSizeFn fn = reinterpret_cast<ArraySetSizeFn>(
        ResolveAddr(Module::Runtime, 0x180008b90));
    return fn;
}
ArraySetSizeFn ArraySetSizeTime() {
    static ArraySetSizeFn fn = reinterpret_cast<ArraySetSizeFn>(
        ResolveAddr(Module::Runtime, 0x180008ff0));
    return fn;
}
ArraySetSizeFn ArraySetSizeInternalPtr() {
    static ArraySetSizeFn fn = reinterpret_cast<ArraySetSizeFn>(
        ResolveAddr(Module::Runtime, 0x180008d60));
    return fn;
}

// ---- Object::Internal::Internal(this) — retail 0x1800bfd10 ------------------
// Enters the object into the InternalFactory handle table. Registry-resolved,
// see the rule note above.
using InternalCtorFn = void* (*)(void* self);
InternalCtorFn InternalCtor() {
    static InternalCtorFn fn = reinterpret_cast<InternalCtorFn>(
        ResolveExport("??0Internal@Object@@QEAA@XZ"));
    return fn;
}

// ---- Object::Base::IsDerivedFrom(this, typeId) — retail 0x1800c0390 --------
// Takes a TYPE ID, not a BaseTypeInfo&. GetAchievement's own bytes settle it:
//     MOV EDX,dword ptr [0x1801f9898]      ; the FIRST DWORD of _typeInfo
//     CALL 0x1800c0390
// Ported by Wave 3 lane A (runtime/src/object/type_system.cpp) and default-on.
using IsDerivedFromFn = bool (*)(const void* self, uint32_t typeId);
IsDerivedFromFn IsDerivedFrom() {
    static IsDerivedFromFn fn = reinterpret_cast<IsDerivedFromFn>(
        ResolveExport("?IsDerivedFrom@Base@Object@@QEBA_NI@Z"));
    return fn;
}

// ---- The two AchievementManager members this lane does not port ------------
// ?ReadAchievements@… (0x18009eb90) re-reads every "Ach<i>" key out of
// IO::Persistence into _completed; ?SetAchievementMet@… (0x18009eca0) sets one
// flag, writes it back through Persistence::Set and calls WriteSettings, which
// touches the player's real HKCU registry. Both are outside the 19 exports
// this lane was given; neither is reimplemented here.
using ReadAchievementsFn  = void (*)(void* self);
using SetAchievementMetFn = bool (*)(void* self, int index);

ReadAchievementsFn ReadAchievements() {
    static ReadAchievementsFn fn = reinterpret_cast<ReadAchievementsFn>(
        ResolveExport("?ReadAchievements@AchievementManager@Achievement@@QEAAXXZ"));
    return fn;
}
SetAchievementMetFn SetAchievementMet() {
    static SetAchievementMetFn fn = reinterpret_cast<SetAchievementMetFn>(
        ResolveExport("?SetAchievementMet@AchievementManager@Achievement@@QEAA_NH@Z"));
    return fn;
}

// ---- Achievement::CreateInstance(this, retval) — retail 0x18009df80 ---------
// Returns InternalPtr<AchievementInstance> BY VALUE, so the hidden return slot
// is argument 2 (RCX=this, RDX=retval). Allocates 0x28, constructs the
// instance inline (it does NOT call the exported ctor), points it at `this`
// and initialises its GoalList.
using CreateInstanceFn = void* (*)(const void* self, void* retval);
CreateInstanceFn CreateInstance() {
    static CreateInstanceFn fn = reinterpret_cast<CreateInstanceFn>(
        ResolveExport("?CreateInstance@Achievement@1@QEBA?AV?$InternalPtr@"
                      "VAchievementInstance@Achievement@@@Object@@XZ"));
    return fn;
}

// ---- GoalList::Initialize(this, Array<ExternalPtr<Goal>> byValue) ----------
// retail 0x18009f0e0. Lane A's class; called, not reimplemented. The Array is
// a BY-VALUE parameter, which in the MS x64 ABI means the caller builds the
// temporary and passes its address and THE CALLEE destroys it — which is why
// SetAchievement below never frees the temp it builds.
using GoalListInitializeFn = void (*)(void* self, void* arrayByValue);
GoalListInitializeFn GoalListInitialize() {
    static GoalListInitializeFn fn = reinterpret_cast<GoalListInitializeFn>(
        ResolveExport("?Initialize@GoalList@Achievement@@QEAAXV?$Array@V?$ExternalPtr@"
                      "VGoal@Achievement@@@Object@@@Collection@@@Z"));
    return fn;
}

// ---- Collection::Array<ExternalPtr<Goal>>::operator= — retail 0x180003970 --
// Not exported; the copy-assign SetAchievement uses to build that temporary.
using ArrayAssignFn = void* (*)(void* dst, const void* src);
ArrayAssignFn ArrayAssignGoalPtrs() {
    static ArrayAssignFn fn = reinterpret_cast<ArrayAssignFn>(
        ResolveAddr(Module::Runtime, 0x180003970));
    return fn;
}

// ---- DLL data symbols ------------------------------------------------------
// Addresses of VARIABLES, resolved lazily: RtAchievementPortInit() runs before
// anything has constructed these, so nothing may be read at registration time.

// ?_singleton@AchievementManager@Achievement@@0PEAV12@EA  (0x1801fc670)
AchievementManager** ManagerSingletonCell() {
    static AchievementManager** cell = reinterpret_cast<AchievementManager**>(
        ResolveExport("?_singleton@AchievementManager@Achievement@@0PEAV12@EA"));
    return cell;
}

// ?_typeInfo@Achievement@1@0VBaseTypeInfo@Object@@A  (0x1801f9898). The FIRST
// DWORD of a BaseTypeInfo is the type id — the same reading
// code/Game/Game/gameupdateloop_load.cpp:57 already uses, and the one
// GetAchievement's `MOV EDX,dword ptr [0x1801f9898]` proves.
uint32_t AchievementTypeId() {
    static const uint32_t* p = reinterpret_cast<const uint32_t*>(
        ResolveExport("?_typeInfo@Achievement@1@0VBaseTypeInfo@Object@@A"));
    return p ? *p : 0u;
}

// ?_singleton@FileSystem@IO@@0PEAV12@EA  (0x1801fc6a8)
RtIO::FileSystem** FileSystemSingletonCell() {
    static RtIO::FileSystem** cell = reinterpret_cast<RtIO::FileSystem**>(
        ResolveExport("?_singleton@FileSystem@IO@@0PEAV12@EA"));
    return cell;
}

// AchievementManager::Update's gate, inlined in retail as
// `CMP dword [RAX+0x38],0 / JG done`. The profile build (0x180028cd0) spells
// it `if (!IO::FileSystem::AreUserPackFilesEnabled(_singleton))`, so the field
// is the user-pack-file (mod) COUNT and the export is
// ?AreUserPackFilesEnabled@FileSystem@IO@@QEBA_NXZ (0x1800acfc0).
//
// Read as a raw offset rather than through a named member because
// runtime/types/io/FileSystem.h (Wave 4 lane D) still has +0x38 inside
// `_unknown2C`. WAVE5_LANE_B.md asks for that field to be named there.
bool UserPackFilesEnabled(const RtIO::FileSystem* fs) {
    return *reinterpret_cast<const int32_t*>(
               reinterpret_cast<const uint8_t*>(fs) +
               RtAchievement::kFileSystemUserPackFileCountOffset) > 0;
}

}  // namespace

// ############################################################################
// #  PURE READS — full differential oracle
// ############################################################################

// ============================================================================
// static AchievementManager* AchievementManager::Get()
// retail 0x1800079d0 — `return _singleton;` (profile address not looked up;
// 0x180029a40 in the profile build is the COPY CONSTRUCTOR, not this)
// Full oracle: reads one DLL global, mutates nothing.
// ============================================================================
AchievementManager* RtAch_AchievementManager_Get() {
    RT_DIFF_BEGIN("Achievement::AchievementManager::Get");

    AchievementManager** cell = ManagerSingletonCell();
    AchievementManager* ours = cell ? *cell : nullptr;

    if (rt_diff_on__) {
        using GetFn = AchievementManager* (*)();
        RT_DIFF_SCALAR(static_cast<const void*>(ours),
                       static_cast<const void*>(
                           RT_ORIGINAL_ONCE(GetFn,
                               "?Get@AchievementManager@Achievement@@SAPEAV12@XZ")()));
    }
    return ours;
}

// ============================================================================
// int AchievementManager::GetAchievementCount() const
// retail 0x1800079b0 — `return *(int*)(*(void**)this + 0x28);`
//
// That is `this->_achievements->_count`, and the deref of `_achievements` is
// UNGUARDED. Faithfully unguarded here too: a manager that has not had
// SetAchievements called faults on a read of 0x28, which is precisely the
// recorded symptom and the only signal that the list is missing.
// ============================================================================
int RtAch_AchievementManager_GetAchievementCount(const AchievementManager* self) {
    RT_DIFF_BEGIN("Achievement::AchievementManager::GetAchievementCount");

    const int ours = self->achievements->_count;

    if (rt_diff_on__) {
        using CountFn = int (*)(const void*);
        RT_DIFF_SCALAR(ours,
                       RT_ORIGINAL_ONCE(CountFn,
                           "?GetAchievementCount@AchievementManager@Achievement@@QEBAHXZ")(self));
    }
    return ours;
}

// ============================================================================
// bool AchievementManager::IsAchievementAwarded(int index) const
// retail 0x1800079c0 — `return *(byte*)(index + *(void**)(this + 8));`
// One byte out of _completed._data. No bounds check in retail; none here.
// ============================================================================
bool RtAch_AchievementManager_IsAchievementAwarded(const AchievementManager* self, int index) {
    RT_DIFF_BEGIN("Achievement::AchievementManager::IsAchievementAwarded");

    const bool ours = self->completed._data[index] != 0;

    if (rt_diff_on__) {
        using AwardedFn = bool (*)(const void*, int);
        RT_DIFF_SCALAR(ours,
                       RT_ORIGINAL_ONCE(AwardedFn,
                           "?IsAchievementAwarded@AchievementManager@Achievement@@QEBA_NH@Z")(
                               self, index));
    }
    return ours;
}

// ============================================================================
// ExternalPtr<Achievement> AchievementManager::GetAchievement(int index) const
// retail 0x18009e8a0
//
// HIDDEN RETURN SLOT: `?AV?$ExternalPtr@…` is a class returned BY VALUE, so
// the ABI is RCX=this, RDX=retval, R8D=index — three arguments, not two. Our
// tree already has this right (engine_funcs.h:1533's
// fn_AchievementManager_GetAchievement_t). An `fn_*` typedef missing that slot
// would be a wild write that still returns the right answer
// (reference_hidden_retslot_wild_write.md).
//
// ExternalPtr is a bare pointer with no refcount (runtime/types/object/External.h),
// so the store is a plain qword and running the oracle into a scratch slot has
// no ownership consequences at all.
// ============================================================================
void* RtAch_AchievementManager_GetAchievement(const AchievementManager* self,
                                              void* retval, int index) {
    RT_DIFF_BEGIN("Achievement::AchievementManager::GetAchievement");

    void* obj = self->achievements->_resources[index];
    void* ours = IsDerivedFrom()(obj, AchievementTypeId()) ? obj : nullptr;
    *static_cast<void**>(retval) = ours;

    if (rt_diff_on__) {
        using GetAchFn = void* (*)(const void*, void*, int);
        void* theirs = reinterpret_cast<void*>(~uintptr_t(0));
        RT_ORIGINAL_ONCE(GetAchFn,
            "?GetAchievement@AchievementManager@Achievement@@QEBA?AV?$ExternalPtr@"
            "VAchievement@1@@Object@@H@Z")(self, &theirs, index);
        RT_DIFF_SCALAR(static_cast<const void*>(ours), static_cast<const void*>(theirs));
    }
    return retval;
}

// ============================================================================
// InternalPtr<AchievementInstance> AchievementManager::GetAchievementInstance(int) const
// retail 0x18009e8f0 / profile 0x1800293a0
//
// Hidden return slot again (RCX=this, RDX=retval, R8D=index). The body is one
// move: `*retval = _instances._data[index]`, with NO refcount and NO handle
// validation — an InternalPtr copy here is a raw pointer copy. Retail has no
// bounds check; the profile build asserts `index >= 0 && index < _size`
// against +0x20, which is how the field at +0x20 got named.
// ============================================================================
void* RtAch_AchievementManager_GetAchievementInstance(const AchievementManager* self,
                                                      void* retval, int index) {
    RT_DIFF_BEGIN("Achievement::AchievementManager::GetAchievementInstance");

    AchievementInstance* ours = self->instances._data[index];
    *static_cast<AchievementInstance**>(retval) = ours;

    if (rt_diff_on__) {
        using GetInstFn = void* (*)(const void*, void*, int);
        void* theirs = reinterpret_cast<void*>(~uintptr_t(0));
        RT_ORIGINAL_ONCE(GetInstFn,
            "?GetAchievementInstance@AchievementManager@Achievement@@QEBA?AV?$InternalPtr@"
            "VAchievementInstance@Achievement@@@Object@@H@Z")(self, &theirs, index);
        RT_DIFF_SCALAR(static_cast<const void*>(ours), static_cast<const void*>(theirs));
    }
    return retval;
}

// ============================================================================
// const GoalList& AchievementInstance::GetGoalList() const
// retail 0x180001960 — `return this + 0x10;` (a reference, not a copy)
// ============================================================================
const GoalListView* RtAch_AchievementInstance_GetGoalList(const AchievementInstance* self) {
    RT_DIFF_BEGIN("Achievement::AchievementInstance::GetGoalList");

    const GoalListView* ours = &self->goalList;

    if (rt_diff_on__) {
        using GetGoalListFn = const void* (*)(const void*);
        RT_DIFF_SCALAR(static_cast<const void*>(ours),
                       RT_ORIGINAL_ONCE(GetGoalListFn,
                           "?GetGoalList@AchievementInstance@Achievement@@QEBAAEBVGoalList@2@XZ")(
                               self));
    }
    return ours;
}

// ============================================================================
// bool AchievementInstance::IsMet() const — retail 0x18009e2f0
//
// An empty goal list is NOT met (retail's explicit `if (count == 0) return
// false` before the loop). Otherwise every goal must have bit 0 set and bit 1
// clear. Retail tests the two bits in that order with two separate branches;
// combining them changes nothing observable, and the loop below keeps the
// order anyway.
// ============================================================================
bool RtAch_AchievementInstance_IsMet(const AchievementInstance* self) {
    RT_DIFF_BEGIN("Achievement::AchievementInstance::IsMet");

    const int count = self->goalList.goals._size;
    bool ours = true;
    if (count == 0) {
        ours = false;
    } else {
        GoalInstanceView* const* goals = self->goalList.goals._data;
        for (int i = 0; i < count; ++i) {
            const uint32_t flags = goals[i]->flags;
            if ((flags & RtAchievement::goalflags::kMet) == 0)    { ours = false; break; }
            if ((flags & RtAchievement::goalflags::kFailed) != 0) { ours = false; break; }
        }
    }

    if (rt_diff_on__) {
        using IsMetFn = bool (*)(const void*);
        RT_DIFF_SCALAR(ours,
                       RT_ORIGINAL_ONCE(IsMetFn,
                           "?IsMet@AchievementInstance@Achievement@@QEBA_NXZ")(self));
    }
    return ours;
}

// ============================================================================
// bool AchievementInstance::HasFailed() const — retail 0x18009e2b0
// True if ANY goal has bit 1 set. An empty list has not failed.
// ============================================================================
bool RtAch_AchievementInstance_HasFailed(const AchievementInstance* self) {
    RT_DIFF_BEGIN("Achievement::AchievementInstance::HasFailed");

    bool ours = false;
    const int count = self->goalList.goals._size;
    GoalInstanceView* const* goals = self->goalList.goals._data;
    for (int i = 0; i < count; ++i) {
        if ((goals[i]->flags & RtAchievement::goalflags::kFailed) != 0) { ours = true; break; }
    }

    if (rt_diff_on__) {
        using HasFailedFn = bool (*)(const void*);
        RT_DIFF_SCALAR(ours,
                       RT_ORIGINAL_ONCE(HasFailedFn,
                           "?HasFailed@AchievementInstance@Achievement@@QEBA_NXZ")(self));
    }
    return ours;
}

// ============================================================================
// String Achievement::GetTitle() / GetDescription() / GetSpriteName() const
// retail 0x1800013e0 / 0x180001420 / 0x1800013a0
//
// HIDDEN RETURN SLOT (`?AVString@System@@XZ` — String returned by value), so
// RCX=this, RDX=retval. All three are the identical three-instruction body on
// a different field:
//     *retval = 0; if (field) { *retval = field; ++field[-4]; }
// i.e. an ADDREF. That makes them impure in exactly one way: running the DLL's
// copy as an oracle bumps the refcount a second time, so the oracle RELEASES
// its own temporary afterwards — the shape Wave 4 lane C used for
// Persistence::GetSavePath. The comparison is on the pointer, which is what
// the function actually computes; the refcount is verified by the release not
// destroying a live string.
// ============================================================================
namespace {
void* AchievementStringGetter(RtDiffSite* site, bool diffOn,
                              const Achievement* self, void* retval,
                              char* field, const char* mangled) {
    String* ret = static_cast<String*>(retval);
    ret->data = nullptr;
    if (field) {
        ret->data = field;
        ++StringRefCount(field);
    }

    if (diffOn) {
        using GetStringFn = void* (*)(const void*, void*);
        String theirs{};
        reinterpret_cast<GetStringFn>(ResolveExportRaw(mangled))(self, &theirs);
        const bool equal = (ret->data == theirs.data);
        char a[80], b[80];
        std::snprintf(a, sizeof(a), "\"%s\"", ret->data ? ret->data : "(null)");
        std::snprintf(b, sizeof(b), "\"%s\"", theirs.data ? theirs.data : "(null)");
        RtDiffReport(site, equal, a, b);
        RtSystem::RtString_ReleaseInline(&theirs);   // undo the oracle's own AddRef
    }
    return retval;
}
}  // namespace

void* RtAch_Achievement_GetTitle(const Achievement* self, void* retval) {
    RT_DIFF_BEGIN("Achievement::Achievement::GetTitle");
    return AchievementStringGetter(&rt_diff_site__, rt_diff_on__, self, retval,
                                   self->title.data,
                                   "?GetTitle@Achievement@1@QEBA?AVString@System@@XZ");
}

void* RtAch_Achievement_GetDescription(const Achievement* self, void* retval) {
    RT_DIFF_BEGIN("Achievement::Achievement::GetDescription");
    return AchievementStringGetter(&rt_diff_site__, rt_diff_on__, self, retval,
                                   self->description.data,
                                   "?GetDescription@Achievement@1@QEBA?AVString@System@@XZ");
}

void* RtAch_Achievement_GetSpriteName(const Achievement* self, void* retval) {
    RT_DIFF_BEGIN("Achievement::Achievement::GetSpriteName");
    return AchievementStringGetter(&rt_diff_site__, rt_diff_on__, self, retval,
                                   self->spriteName.data,
                                   "?GetSpriteName@Achievement@1@QEBA?AVString@System@@XZ");
}

// ############################################################################
// #  MUTATORS — shadow verdicts, never a second call into the DLL
// ############################################################################

// ============================================================================
// AchievementInstance::AchievementInstance() — retail 0x18009e3b0
//
// Constructs the Object::Internal base (which ENTERS THE HANDLE TABLE — a real
// side effect on shared state, and the reason this is not diffed by running
// theirs too: a second construction would take a second handle), stamps the
// class's own vtable over Internal's, and zeroes the GoalList and the
// achievement pointer. MSVC constructors return `this` in RAX; our tree's
// fn_AchievementInstance_Ctor_t already declares that return
// (gameupdateloop_load.cpp:56).
//
// The vtable address is NOT cloned from a live object and NOT hardcoded: it is
// the exported data symbol ??_7AchievementInstance@Achievement@@6B@ (ordinal
// 1383), whose value equals the `LEA RAX,[0x180154700]` the ctor's own bytes
// carry. PLAN.md §2.2 D warns that an exported vtable makes the ADDRESS
// available but that stamping it is a per-class argument; here the argument is
// trivial, because retail's own constructor stamps that exact symbol and
// nothing else ever writes the slot.
//
// SHADOW VERDICT: the vtable we stamped equals the DLL's exported symbol, the
// GoalList and achievement pointer are zero, and the base constructor left a
// non-zero generation. That proves the object is shaped like retail's; it does
// NOT prove the handle-table entry is right — Wave 3's opt-in
// BANISHED_RT_INTERNAL_SHADOW oracle is what covers that, and this port goes
// through the registry precisely so that oracle still sees this object.
// ============================================================================
AchievementInstance* RtAch_AchievementInstance_Ctor(AchievementInstance* self) {
    RT_DIFF_BEGIN("Achievement::AchievementInstance::ctor");

    InternalCtor()(self);

    static void* const vtable = ResolveExport("??_7AchievementInstance@Achievement@@6B@");
    self->base.vptr = vtable;
    self->goalList.goals._data     = nullptr;
    self->goalList.goals._size     = 0;
    self->goalList.goals._capacity = 0;
    self->achievement              = nullptr;

    if (rt_diff_on__) {
        const bool equal = (self->base.vptr == vtable) && (vtable != nullptr) &&
                           (self->goalList.goals._data == nullptr) &&
                           (self->achievement == nullptr) &&
                           (self->base.generation != 0);
        char a[96], b[96];
        std::snprintf(a, sizeof(a), "vtbl=%p gen=%u goals=%p ach=%p",
                      self->base.vptr, self->base.generation,
                      static_cast<void*>(self->goalList.goals._data),
                      static_cast<void*>(self->achievement));
        std::snprintf(b, sizeof(b), "exported ??_7AchievementInstance=%p, zeroed tail", vtable);
        RT_DIFF_SHADOW(equal, a, b);
    }
    return self;
}

// ============================================================================
// void AchievementInstance::SetAchievement(ExternalPtr<Achievement>)
// retail 0x18009e440
//
// The ExternalPtr argument is one pointer passed in RDX by value. Retail's
// exact order, from the bytes:
//     MOV [RCX+0x20],RDX            ; store FIRST, before anything can fault
//     <zero a 16-byte stack temp>
//     LEA RDX,[achievement+0x48]; CALL 0x180003970   ; temp = achievement->_goals
//     LEA RCX,[this+0x10]; CALL 0x18009f0e0          ; GoalList::Initialize(temp)
// and no destructor call on the temp afterwards, because Initialize takes the
// Array BY VALUE and the MS ABI makes the CALLEE destroy a by-value class
// parameter. Passing a null achievement faults at `achievement + 0x48`,
// exactly as retail does.
//
// SHADOW VERDICT: the stored pointer is the one we were handed, and the
// GoalList now holds exactly as many goal instances as the achievement has
// goals. That is the invariant the whole achievement system rests on (a goal
// count of 0 is what makes IsMet return false forever), and it is checked
// without a second Initialize.
// ============================================================================
void RtAch_AchievementInstance_SetAchievement(AchievementInstance* self, Achievement* achievement) {
    RT_DIFF_BEGIN("Achievement::AchievementInstance::SetAchievement");

    self->achievement = achievement;

    Collection::Array<void*> temp{nullptr, 0, 0};
    ArrayAssignGoalPtrs()(&temp, &achievement->goals);
    GoalListInitialize()(&self->goalList, &temp);

    if (rt_diff_on__) {
        const bool equal = (self->achievement == achievement) &&
                           (self->goalList.goals._size == achievement->goals._size);
        char a[96], b[96];
        std::snprintf(a, sizeof(a), "ach=%p goals=%d",
                      static_cast<void*>(self->achievement), self->goalList.goals._size);
        std::snprintf(b, sizeof(b), "expected ach=%p goals=%d",
                      static_cast<void*>(achievement), achievement->goals._size);
        RT_DIFF_SHADOW(equal, a, b);
    }
}

// ============================================================================
// void AchievementInstance::Update(const Time&) — retail 0x18009e340
//
// Forwards the tick to every goal instance through vtable slot 5 (+0x28),
// which is ?Update@GoalInstance@Achievement@@UEAAXAEBVTime@System@@AEBVGoalList@2@@Z
// — (this, const Time&, const GoalList&) — passing THIS INSTANCE'S OWN GoalList
// as the third argument (retail: `LEA R8,[RDI+0x10]`). Our exe registers a port
// for that virtual (standalone/reg_goal.cpp:82), and it is reached through the
// object's vtable either way.
//
// SHADOW VERDICT: this one is weak and is labelled as such. Dispatching a tick
// into N goal instances has no result to compare and cannot be run twice (each
// goal instance accumulates state), so the verdict is only "the loop visited
// exactly _size goals and none of the pointers it dispatched through was
// null". That is an EXECUTION COUNTER plus a null check, not an equivalence
// proof — read the diff summary's calls= column for this site as "how often
// this ran", never as "how often ours agreed with theirs"
// (reference_uncalled_port_is_not_verified.md).
// ============================================================================
void RtAch_AchievementInstance_Update(AchievementInstance* self, const ::Time* time) {
    RT_DIFF_BEGIN("Achievement::AchievementInstance::Update");

    using GoalUpdateFn = void (*)(void* self, const ::Time* time, const void* goalList);
    constexpr int kGoalInstanceUpdateSlot = 0x28;

    // Retail derefs the goal pointer and its vtable unguarded; so does this.
    int visited = 0;
    const int count = self->goalList.goals._size;
    for (int i = 0; i < count; ++i) {
        GoalInstanceView* goal = self->goalList.goals._data[i];
        void** vtbl = *reinterpret_cast<void***>(goal);
        auto fn = reinterpret_cast<GoalUpdateFn>(
            *reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(vtbl) + kGoalInstanceUpdateSlot));
        fn(goal, time, &self->goalList);
        ++visited;
    }

    if (rt_diff_on__) {
        const bool equal = (visited == count);
        char a[64], b[64];
        std::snprintf(a, sizeof(a), "dispatched %d goal(s)", visited);
        std::snprintf(b, sizeof(b), "goalList._size = %d", count);
        RT_DIFF_SHADOW(equal, a, b);
    }
}

// ============================================================================
// void AchievementManager::SetEventHandler(void (*)(int)) — retail 0x18009e6b0
// One store to +0x28. Shadow verdict is the readback of the field.
// ============================================================================
void RtAch_AchievementManager_SetEventHandler(AchievementManager* self, void (*handler)(int)) {
    RT_DIFF_BEGIN("Achievement::AchievementManager::SetEventHandler");

    self->eventHandler = handler;

    if (rt_diff_on__) {
        const bool equal = (self->eventHandler == handler);
        char a[48], b[48];
        std::snprintf(a, sizeof(a), "+0x28 = %p", reinterpret_cast<void*>(self->eventHandler));
        std::snprintf(b, sizeof(b), "argument = %p", reinterpret_cast<void*>(handler));
        RT_DIFF_SHADOW(equal, a, b);
    }
}

// ============================================================================
// void AchievementManager::SetAchievements(ExternalPtr<ExternalList>)
// retail 0x18009e860
//
//     _achievements = list;
//     Array<Time>::SetSize(&_updateTimes, list->_count);
//     Array<bool>::SetSize(&_completed, _achievements->_count);   // re-read
//     ReadAchievements();
//
// The second SetSize re-reads the count THROUGH the field it just stored,
// rather than reusing the register — kept as written, since the two only agree
// because the store happened first and that ordering is the point.
//
// Note what this does NOT do: it does not size `_instances`. That is Reset's
// job on a new game and Persist's on a load, which is why a manager with a
// perfectly good achievement list can still have zero instances and award
// nothing (the finding recorded at gameupdateloop_load.cpp's step30b).
//
// SHADOW VERDICT: after our call, the DLL's own GetAchievementCount agrees
// with the list we stored, and both arrays are that long. `ReadAchievements`
// is the DLL's, so `_completed`'s CONTENTS are the DLL's work, not ours.
// ============================================================================
void RtAch_AchievementManager_SetAchievements(AchievementManager* self, void* list) {
    RT_DIFF_BEGIN("Achievement::AchievementManager::SetAchievements");

    self->achievements = static_cast<RtObject::ExternalList*>(list);
    ArraySetSizeTime()(&self->updateTimes, self->achievements->_count);
    ArraySetSizeBool()(&self->completed, self->achievements->_count);
    ReadAchievements()(self);

    if (rt_diff_on__) {
        using CountFn = int (*)(const void*);
        const int theirCount = RT_ORIGINAL_ONCE(CountFn,
            "?GetAchievementCount@AchievementManager@Achievement@@QEBAHXZ")(self);
        const bool equal = (theirCount == self->completed._size) &&
                           (theirCount == self->updateTimes._size);
        char a[96], b[96];
        std::snprintf(a, sizeof(a), "completed=%d updateTimes=%d",
                      self->completed._size, self->updateTimes._size);
        std::snprintf(b, sizeof(b), "DLL GetAchievementCount=%d", theirCount);
        RT_DIFF_SHADOW(equal, a, b);
    }
}

// ============================================================================
// void AchievementManager::Reset() — retail 0x18009e5c0
//
// THE INSTANCE BUILDER, not a "clear the awards" call: it sizes `_instances` to
// the achievement count and fills every slot with Achievement::CreateInstance.
// Nothing else in the engine ever writes that array on a new game.
//
// The per-element type test is an INLINED Object::Base::IsDerivedFrom in
// retail — GetTypeId through vtable slot 2, an early-out when it already
// equals Achievement's id, then a walk of Factory's type map. Called here
// through the export instead of transcribed, both because Wave 3 lane A owns
// that body and because re-transcribing a factory-map walk is how two copies
// drift apart. Retail derefs the found node unconditionally, and so does Wave
// 3's port, so a missing registration still faults rather than silently
// yielding null.
//
// SHADOW VERDICT: the array is the right length, no slot is null, and every
// instance points back at the achievement that built it. Reset cannot be run
// twice for a comparison — a second run allocates a whole second set of
// instances and leaks the first, since SetSize to the same size is a no-op
// that does not free the old objects.
// ============================================================================
void RtAch_AchievementManager_Reset(AchievementManager* self) {
    RT_DIFF_BEGIN("Achievement::AchievementManager::Reset");

    ArraySetSizeInternalPtr()(&self->instances, self->achievements->_count);

    const uint32_t achievementTypeId = AchievementTypeId();
    for (int i = 0; i < self->instances._size; ++i) {
        void* obj = self->achievements->_resources[i];
        Achievement* achievement =
            IsDerivedFrom()(obj, achievementTypeId) ? static_cast<Achievement*>(obj) : nullptr;

        void* created = nullptr;
        CreateInstance()(achievement, &created);
        self->instances._data[i] = static_cast<AchievementInstance*>(created);
    }

    if (rt_diff_on__) {
        bool equal = (self->instances._size == self->achievements->_count);
        int firstBad = -1;
        for (int i = 0; equal && i < self->instances._size; ++i) {
            AchievementInstance* inst = self->instances._data[i];
            if (!inst || static_cast<void*>(inst->achievement) !=
                             static_cast<void*>(self->achievements->_resources[i])) {
                equal = false;
                firstBad = i;
            }
        }
        char a[96], b[96];
        std::snprintf(a, sizeof(a), "instances=%d firstBad=%d", self->instances._size, firstBad);
        std::snprintf(b, sizeof(b), "achievements=%d, each instance->_achievement matching",
                      self->achievements->_count);
        RT_DIFF_SHADOW(equal, a, b);
    }
}

// ============================================================================
// void AchievementManager::Update(const Time&) — retail 0x18009e6c0,
//                                                profile 0x180028cd0
//
// The rotating updater. Once per tick:
//
//   0. If any user pack file (mod) is mounted, do nothing at all. This is the
//      whole mechanism behind BANISHED_ACH_IGNORE_MODS and behind
//      reference_mods_off_breaks_save_loading.md's "achievements need them
//      OUT": with a mod loaded, not one line below ever runs.
//   1. Accumulate the frame delta into _updateTimes[i] for every not-yet
//      completed achievement, and hard-zero the slot of every completed one.
//   2. Walk the cursor forward past completed achievements, counting how many
//      it skipped. If it skips all of them, every achievement is done — return.
//   3. Update the ONE instance at the cursor with its whole accumulated time,
//      zero that slot, and if the instance is now met, award it and fire the
//      event handler.
//   4. Advance the cursor, wrapping.
//
// So each achievement is evaluated once every _size ticks, with _size ticks'
// worth of accumulated time — which is why the times array exists at all.
//
// Retail INLINES AchievementInstance::Update and ::IsMet here; this port calls
// this file's own ports of them instead. Same work, and it keeps one copy of
// each body. Retail also peels the first iteration of the skip loop; the
// profile build's un-peeled `while` is transcribed below because the two are
// the same loop and the profile's reads correctly.
//
// The unguarded `_completed._data[_lastAchievementUpdate]` read in step 2 is
// retail's, and faults when SetAchievements has never run. Kept.
//
// SHADOW VERDICT: the cursor stayed inside [0, _size), and any achievement we
// awarded this tick reads back as awarded through the DLL's own
// IsAchievementAwarded. A tick that awards nothing checks only the cursor —
// honestly, most ticks prove very little here.
// ============================================================================
void RtAch_AchievementManager_Update(AchievementManager* self, const ::Time* time) {
    RT_DIFF_BEGIN("Achievement::AchievementManager::Update");

    RtIO::FileSystem** fsCell = FileSystemSingletonCell();
    if (UserPackFilesEnabled(*fsCell)) return;

    const int count = self->instances._size;
    for (int i = 0; i < count; ++i) {
        if (self->completed._data[i] == 0) self->updateTimes._data[i].ticks += time->ticks;
        else                               self->updateTimes._data[i].ticks = 0;
    }

    int skipped = 0;
    while (self->completed._data[self->lastAchievementUpdate] != 0) {
        if (skipped >= count) break;
        ++self->lastAchievementUpdate;
        ++skipped;
        if (self->lastAchievementUpdate >= count) self->lastAchievementUpdate = 0;
    }
    if (skipped == count) return;

    int index = self->lastAchievementUpdate;
    AchievementInstance* inst = self->instances._data[index];
    RtAch_AchievementInstance_Update(inst, &self->updateTimes._data[index]);
    self->updateTimes._data[index].ticks = 0;

    index = self->lastAchievementUpdate;
    inst  = self->instances._data[index];
    bool awarded = false;
    if (RtAch_AchievementInstance_IsMet(inst)) {
        if (self->completed._data[index] == 0 && SetAchievementMet()(self, index)) {
            awarded = true;
            if (self->eventHandler) self->eventHandler(self->lastAchievementUpdate);
        }
    }

    // Retail RE-READS _instances._size here rather than reusing the register it
    // loaded at the top (0x18009e836), so this does too — cheap, and it keeps
    // the wrap correct if anything a goal update reached ever resized the array.
    ++self->lastAchievementUpdate;
    if (self->lastAchievementUpdate >= self->instances._size) self->lastAchievementUpdate = 0;

    if (rt_diff_on__) {
        bool equal = (self->lastAchievementUpdate >= 0) &&
                     (self->lastAchievementUpdate < self->instances._size);
        if (awarded) {
            using AwardedFn = bool (*)(const void*, int);
            equal = equal && RT_ORIGINAL_ONCE(AwardedFn,
                "?IsAchievementAwarded@AchievementManager@Achievement@@QEBA_NH@Z")(self, index);
        }
        char a[96], b[96];
        std::snprintf(a, sizeof(a), "cursor=%d awarded=%d idx=%d",
                      self->lastAchievementUpdate, static_cast<int>(awarded), index);
        std::snprintf(b, sizeof(b), "count=%d, DLL IsAchievementAwarded readback", count);
        RT_DIFF_SHADOW(equal, a, b);
    }
}

// ============================================================================
// Registration.
//
// Every name is spelled as a SINGLE string literal on ONE line: the census
// (tools/runtime_census.py) only matches a RegisterRuntimeExport whose first
// argument is a plain one-line string literal, and it can see through neither
// a named constant nor a split literal, and both blind
// spots have already produced a silent under-count of live ports
// (reference_census_cannot_see_nonliteral_registrations.md). Every one of them
// was verified present in the shipped export directory with pefile BEFORE a
// body was written — a name the DLL does not contain resolves to NULL silently
// and the port then reports calls=0 forever
// (reference_resolveexport_phantom_names.md).
//
// Address fallbacks are registered alongside every name, because half our call
// sites resolve by ResolveAddr(Module::Runtime, …) with the export as a
// fallback, or the reverse (WAVE3_LANE_A.md §6).
// ============================================================================
int RtAchManagerRegister() {
    int n = 0;

    // ---- AchievementManager, pure reads ------------------------------------
    RegisterRuntimeExport("?Get@AchievementManager@Achievement@@SAPEAV12@XZ", (void*)&RtAch_AchievementManager_Get); ++n;
    RegisterRuntimeAddr(0x1800079d0, (void*)&RtAch_AchievementManager_Get);

    RegisterRuntimeExport("?GetAchievementCount@AchievementManager@Achievement@@QEBAHXZ", (void*)&RtAch_AchievementManager_GetAchievementCount); ++n;
    RegisterRuntimeAddr(0x1800079b0, (void*)&RtAch_AchievementManager_GetAchievementCount);

    RegisterRuntimeExport("?IsAchievementAwarded@AchievementManager@Achievement@@QEBA_NH@Z", (void*)&RtAch_AchievementManager_IsAchievementAwarded); ++n;
    RegisterRuntimeAddr(0x1800079c0, (void*)&RtAch_AchievementManager_IsAchievementAwarded);

    RegisterRuntimeExport("?GetAchievement@AchievementManager@Achievement@@QEBA?AV?$ExternalPtr@VAchievement@1@@Object@@H@Z", (void*)&RtAch_AchievementManager_GetAchievement); ++n;
    RegisterRuntimeAddr(0x18009e8a0, (void*)&RtAch_AchievementManager_GetAchievement);

    RegisterRuntimeExport("?GetAchievementInstance@AchievementManager@Achievement@@QEBA?AV?$InternalPtr@VAchievementInstance@Achievement@@@Object@@H@Z", (void*)&RtAch_AchievementManager_GetAchievementInstance); ++n;
    RegisterRuntimeAddr(0x18009e8f0, (void*)&RtAch_AchievementManager_GetAchievementInstance);

    // ---- AchievementManager, mutators --------------------------------------
    RegisterRuntimeExport("?SetEventHandler@AchievementManager@Achievement@@QEAAXP6AXH@Z@Z", (void*)&RtAch_AchievementManager_SetEventHandler); ++n;
    RegisterRuntimeAddr(0x18009e6b0, (void*)&RtAch_AchievementManager_SetEventHandler);

    RegisterRuntimeExport("?SetAchievements@AchievementManager@Achievement@@QEAAXV?$ExternalPtr@VExternalList@Object@@@Object@@@Z", (void*)&RtAch_AchievementManager_SetAchievements); ++n;
    RegisterRuntimeAddr(0x18009e860, (void*)&RtAch_AchievementManager_SetAchievements);

    RegisterRuntimeExport("?Reset@AchievementManager@Achievement@@QEAAXXZ", (void*)&RtAch_AchievementManager_Reset); ++n;
    RegisterRuntimeAddr(0x18009e5c0, (void*)&RtAch_AchievementManager_Reset);

    RegisterRuntimeExport("?Update@AchievementManager@Achievement@@QEAAXAEBVTime@System@@@Z", (void*)&RtAch_AchievementManager_Update); ++n;
    RegisterRuntimeAddr(0x18009e6c0, (void*)&RtAch_AchievementManager_Update);

    // ---- AchievementInstance ------------------------------------------------
    RegisterRuntimeExport("??0AchievementInstance@Achievement@@QEAA@XZ", (void*)&RtAch_AchievementInstance_Ctor); ++n;
    RegisterRuntimeAddr(0x18009e3b0, (void*)&RtAch_AchievementInstance_Ctor);

    RegisterRuntimeExport("?SetAchievement@AchievementInstance@Achievement@@QEAAXV?$ExternalPtr@VAchievement@1@@Object@@@Z", (void*)&RtAch_AchievementInstance_SetAchievement); ++n;
    RegisterRuntimeAddr(0x18009e440, (void*)&RtAch_AchievementInstance_SetAchievement);

    RegisterRuntimeExport("?Update@AchievementInstance@Achievement@@QEAAXAEBVTime@System@@@Z", (void*)&RtAch_AchievementInstance_Update); ++n;
    RegisterRuntimeAddr(0x18009e340, (void*)&RtAch_AchievementInstance_Update);

    RegisterRuntimeExport("?IsMet@AchievementInstance@Achievement@@QEBA_NXZ", (void*)&RtAch_AchievementInstance_IsMet); ++n;
    RegisterRuntimeAddr(0x18009e2f0, (void*)&RtAch_AchievementInstance_IsMet);

    RegisterRuntimeExport("?HasFailed@AchievementInstance@Achievement@@QEBA_NXZ", (void*)&RtAch_AchievementInstance_HasFailed); ++n;
    RegisterRuntimeAddr(0x18009e2b0, (void*)&RtAch_AchievementInstance_HasFailed);

    RegisterRuntimeExport("?GetGoalList@AchievementInstance@Achievement@@QEBAAEBVGoalList@2@XZ", (void*)&RtAch_AchievementInstance_GetGoalList); ++n;
    RegisterRuntimeAddr(0x180001960, (void*)&RtAch_AchievementInstance_GetGoalList);

    // ---- Achievement (the External resource) --------------------------------
    RegisterRuntimeExport("?GetTitle@Achievement@1@QEBA?AVString@System@@XZ", (void*)&RtAch_Achievement_GetTitle); ++n;
    RegisterRuntimeAddr(0x1800013e0, (void*)&RtAch_Achievement_GetTitle);

    RegisterRuntimeExport("?GetDescription@Achievement@1@QEBA?AVString@System@@XZ", (void*)&RtAch_Achievement_GetDescription); ++n;
    RegisterRuntimeAddr(0x180001420, (void*)&RtAch_Achievement_GetDescription);

    RegisterRuntimeExport("?GetSpriteName@Achievement@1@QEBA?AVString@System@@XZ", (void*)&RtAch_Achievement_GetSpriteName); ++n;
    RegisterRuntimeAddr(0x1800013a0, (void*)&RtAch_Achievement_GetSpriteName);

    printf("[RtPort]   Achievement::AchievementManager/Instance/Achievement: %d/19 exports "
           "registered (Persist held back -- no oracle exists for a Serializer "
           "construction, see WAVE5_LANE_B.md)\n", n);
    return n;
}
