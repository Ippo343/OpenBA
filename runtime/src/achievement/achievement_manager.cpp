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
// HELD BACK by Wave 5, deliberately, with the reasoning in the wave doc:
//   ?Persist@AchievementManager@Achievement@@QEAAXAEAVStream@IO@@@Z (0x18009e910)
//     -- it inlines an IO::Serializer construction, and Wave 4 lane A measured
//       that a Serializer ctor CONSUMES OR EMITS A 12-BYTE STREAM HEADER
//       (runtime/types/io/Serializer.h). So it has no runnable oracle of any
//       kind: a second construction desyncs the save.
//
// *** THAT HOLDBACK IS DISCHARGED. WAVE 23 LANE ACHIEVE, 2026-09-11. ***
// The blocker was real when written and stopped being true in Wave 9, when lane
// S built the save-stream emission oracle: it ran each side against its OWN
// INDEPENDENT SYNTHETIC SINK, so there is no shared stream to desync and each
// side constructs its own codec. Lane P discharged the IDENTICAL blocker, in
// the identical words, for Goal::Serialize and GoalInstance::Persist
// (runtime/src/achievement/goal_serialize.cpp). Persist is now ported in
// runtime/src/achievement/achievement_serialize.cpp, under a PREFIX oracle
// (stopAfter = 1) because its element loop mutates the global Object::Factory.
// The paragraph above is kept rather than deleted: it is the evidence for
// PLAN rule "re-check a stated blocker before acting on it", and this one
// survived fourteen waves because nobody re-read it.
//   ?_singleton@AchievementManager@Achievement@@0PEAV12@EA and the two
//   ?_typeInfo@… symbols are DATA exports; they cannot be registered before
//   cutover (same finding as Wave 4 lane D's FileSystem::_singleton).
//
// STATE. Only nine of these functions are pure reads; the other nine mutate
// the live singleton, a live instance, or the Windows registry.
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
#include <new>          // placement new into the retslot doors

#include "../../funcs/rt_port.h"
#include "../../types/achievement/AchievementManager.h"
#include "../../types/io/FileSystem.h"
#include "../../types/object/External.h"   // Object::ExternalPtr<T>, ExternalList
#include "../../types/object/Internal.h"   // Object::InternalPtr<T>

// ENDGAME B.3.7 block B3: every body below is a MEMBER of
// `Achievement::AchievementManager`, `AchievementInstance` or `Achievement`
// (runtime/types/achievement/AchievementManager.h); every registered door is a
// one-line QUALIFIED delegation. The two CONSTRUCTOR doors (`??0Achievement`,
// `??0AchievementInstance`) stay free functions: both classes have exe
// subclasses and every construction must stamp the MINTED table (B4b-2, B7-13).
//
// The callers inside this file that used to reach sibling members through
// ResolveExport name doors (`ReadAchievements`, `SetAchievementMet`,
// `CreateInstance`, `GoalList::Initialize`, `Base::IsDerivedFrom`,
// `~External`) and by address (`Array<ExternalPtr<Goal>>::operator=`
// 0x180003970) now call the members directly.

enum class Module : int { Exe = 0, Runtime = 1, VideoDX11 = 2, COUNT = 3 };
uintptr_t ResolveAddr(Module mod, uintptr_t ghidra_addr);

namespace {

// ---- ??0Internal@Object@@QEAA@XZ — retail 0x1800bfd10 ----------------------
// Enters the object into the InternalFactory handle table. A name door onto
// Object's ported constructor DOOR (Object::Internal has no public real
// constructor either — B4b-2). RT_EXPORT_ONCE is the RtResolveCached slot
// (B6-5: AchievementInstances are constructed on the LOADER thread).
using InternalCtorFn = void* (*)(void* self);
InternalCtorFn InternalCtor() {
    return RT_EXPORT_ONCE(InternalCtorFn, "??0Internal@Object@@QEAA@XZ");
}

// ---- The vtables the two constructor doors stamp ---------------------------
// EXPORTED data symbols, resolved by name so the door installs the MINTED
// table. Cross-check addresses: 0x180154788 / 0x1801547a0 / 0x180154740 /
// 0x180154700 (the ctors' own LEA operands).
void* VtableObjectBase()          { return RT_EXPORT_ONCE(void*, "??_7Base@Object@@6B@"); }
void* VtableObjectExternal()      { return RT_EXPORT_ONCE(void*, "??_7External@Object@@6B@"); }
void* VtableAchievement()         { return RT_EXPORT_ONCE(void*, "??_7Achievement@0@6B@"); }
void* VtableAchievementInstance() { return RT_EXPORT_ONCE(void*, "??_7AchievementInstance@Achievement@@6B@"); }

// ?_singleton@AchievementManager@Achievement@@0PEAV12@EA  (0x1801fc670) — stays a
// data slot until the class-static conversion at the end of B.3 (carry-over 5).
Achievement::AchievementManager** ManagerSingletonCell() {
    return RT_DATA_SLOT(Achievement::AchievementManager**,
        "?_singleton@AchievementManager@Achievement@@0PEAV12@EA");
}

// ?_typeInfo@Achievement@1@0VBaseTypeInfo@Object@@A  (0x1801f9898). The FIRST
// DWORD is the type id — GetAchievement's own `MOV EDX,dword ptr [0x1801f9898]`.
uint32_t AchievementTypeId() {
    const uint32_t* p = RT_DATA_SLOT(const uint32_t*,
        "?_typeInfo@Achievement@1@0VBaseTypeInfo@Object@@A");
    return *p;
}

// ?_singleton@FileSystem@IO@@0PEAV12@EA  (0x1801fc6a8)
RtIO::FileSystem** FileSystemSingletonCell() {
    return RT_DATA_SLOT(RtIO::FileSystem**, "?_singleton@FileSystem@IO@@0PEAV12@EA");
}

// AchievementManager::Update's gate, inlined in retail as `CMP dword
// [RAX+0x38],0 / JG done` — `IO::FileSystem::AreUserPackFilesEnabled`
// (0x1800acfc0): the user (mod) pack-file COUNT is positive.
bool UserPackFilesEnabled(const RtIO::FileSystem* fs) {
    return fs->userPackFilenameCount > 0;
}

}  // namespace

namespace Achievement {

// ############################################################################
// #  AchievementManager
// ############################################################################

// static AchievementManager* Get()  retail 0x1800079d0 — `return _singleton;`
AchievementManager* AchievementManager::Get() {
    return *ManagerSingletonCell();
}

// int GetAchievementCount() const  retail 0x1800079b0 — `_achievements->_count`,
// UNGUARDED: a manager with no SetAchievements faults on a read of 0x28, the
// recorded symptom and the only signal that the list is missing.
int AchievementManager::GetAchievementCount() const {
    return _achievements->_count;
}

// bool IsAchievementAwarded(int) const  retail 0x1800079c0 — one byte of
// `_completed`, no bounds check.
bool AchievementManager::IsAchievementAwarded(int index) const {
    return _completed._data[index];
}

// ExternalPtr<Achievement> GetAchievement(int) const  retail 0x18009e8a0
// (door: RCX=this, RDX=retval, R8D=index). `_resources[index]` if it derives
// from Achievement, else null — Base::IsDerivedFrom (0x1800c0390).
Object::ExternalPtr<Achievement> AchievementManager::GetAchievement(int index) const {
    Object::External* obj = _achievements->_resources[index];
    return Object::ExternalPtr<Achievement>(
        obj->IsDerivedFrom(AchievementTypeId()) ? static_cast<Achievement*>(obj) : nullptr);
}

// InternalPtr<AchievementInstance> GetAchievementInstance(int) const
// retail 0x18009e8f0 — one move, no bounds check, no handle validation.
Object::InternalPtr<AchievementInstance> AchievementManager::GetAchievementInstance(int index) const {
    return _instances._data[index];
}

// void SetEventHandler(void (*)(int))  retail 0x18009e6b0 — one store to +0x28.
void AchievementManager::SetEventHandler(EventHandler handler) {
    _eventHandler = handler;
}

// ============================================================================
// void SetAchievements(ExternalPtr<ExternalList>)  retail 0x18009e860
//     _achievements = list;                         ; `MOV [RCX],RDX` — BARE (B7-5)
//     _updateTimes.SetSize(list->_count);           ; 0x180008ff0 kSpec_U64Zero
//     _completed.SetSize(_achievements->_count);    ; 0x180008b90 kSpec_Bool, RE-READ
//     ReadAchievements();
// It does NOT size `_instances` — that is Reset's job on a new game and
// Persist's on a load.
// ============================================================================
void AchievementManager::SetAchievements(Object::ExternalPtr<Object::ExternalList> list) {
    _achievements = list.Get();
    _updateTimes.SetSize(_achievements->_count);
    _completed.SetSize(_achievements->_count);
    ReadAchievements();
}

// ============================================================================
// void Reset()  retail 0x18009e5c0 — THE INSTANCE BUILDER.
// Sizes `_instances` to the achievement count (0x180008d60, kSpec_PtrZero) and
// fills every slot with Achievement::CreateInstance. The per-element type test
// is retail's INLINED Base::IsDerivedFrom — the member here. A missing
// registration still faults rather than yielding null (retail derefs the
// found node unconditionally, and so does the port).
// ============================================================================
void AchievementManager::Reset() {
    _instances.SetSize(_achievements->_count);

    const uint32_t achievementTypeId = AchievementTypeId();
    for (int i = 0; i < _instances._size; ++i) {
        Object::External* obj = _achievements->_resources[i];
        Achievement* achievement =
            obj->IsDerivedFrom(achievementTypeId) ? static_cast<Achievement*>(obj) : nullptr;
        _instances._data[i] = achievement->CreateInstance();
    }
}

// ============================================================================
// void Update(const System::Time&)  retail 0x18009e6c0 / profile 0x180028cd0
//
// The rotating updater. Once per tick:
//   0. If any user pack file (mod) is mounted, do nothing at all.
//   1. Accumulate the delta into _updateTimes[i] for every not-yet-completed
//      achievement; hard-zero the slot of every completed one.
//   2. Walk the cursor past completed achievements; if it skips all, return.
//   3. Update the ONE instance at the cursor with its accumulated time, zero
//      that slot, and if the instance is now met, award it and fire the handler.
//   4. Advance the cursor, wrapping (on a RE-READ `_instances._size`).
// Retail INLINES AchievementInstance::Update and ::IsMet; the members are
// called. The unguarded `_completed._data[_lastAchievementUpdate]` read is
// retail's and faults when SetAchievements never ran.
// ============================================================================
void AchievementManager::Update(const System::Time& time) {
    if (UserPackFilesEnabled(*FileSystemSingletonCell())) return;

    const int count = _instances._size;
    for (int i = 0; i < count; ++i) {
        if (!_completed._data[i]) _updateTimes._data[i]._ticks += time._ticks;
        else                      _updateTimes._data[i]._ticks = 0;
    }

    int skipped = 0;
    while (_completed._data[_lastAchievementUpdate]) {
        if (skipped >= count) break;
        ++_lastAchievementUpdate;
        ++skipped;
        if (_lastAchievementUpdate >= count) _lastAchievementUpdate = 0;
    }
    if (skipped == count) return;

    int index = _lastAchievementUpdate;
    _instances._data[index]->Update(_updateTimes._data[index]);
    _updateTimes._data[index]._ticks = 0;

    index = _lastAchievementUpdate;
    AchievementInstance* inst = _instances._data[index].Get();
    if (inst->IsMet()) {
        if (!_completed._data[index] && SetAchievementMet(index)) {
            if (_eventHandler) _eventHandler(_lastAchievementUpdate);
        }
    }

    ++_lastAchievementUpdate;
    if (_lastAchievementUpdate >= _instances._size) _lastAchievementUpdate = 0;
}

// ############################################################################
// #  AchievementInstance
// ############################################################################

// ??1AchievementInstance@Achievement@@UEAA@XZ  0x18009e400 — EMPTY: the
// compiler emits the re-stamp, `~GoalList()` on `_goalList` and `~Internal()`.
AchievementInstance::~AchievementInstance() {}

// ============================================================================
// bool IsMet() const  retail 0x18009e2f0
// An EMPTY goal list is NOT met (retail's explicit `count == 0` test).
// Otherwise every goal must have bit 0 set and bit 1 clear — tested in that
// order, as retail's two branches do. Elements dereferenced UNGUARDED.
// ============================================================================
bool AchievementInstance::IsMet() const {
    const int count = _goalList._goals._size;
    if (count == 0) return false;
    for (int i = 0; i < count; ++i) {
        const uint32_t flags = _goalList._goals._data[i]->_flags;
        if ((flags & GoalInstance::kFlagMet) == 0)    return false;
        if ((flags & GoalInstance::kFlagFailed) != 0) return false;
    }
    return true;
}

// bool HasFailed() const  retail 0x18009e2b0 — true if ANY goal has bit 1.
bool AchievementInstance::HasFailed() const {
    const int count = _goalList._goals._size;
    for (int i = 0; i < count; ++i) {
        if ((_goalList._goals._data[i]->_flags & GoalInstance::kFlagFailed) != 0)
            return true;
    }
    return false;
}

// ============================================================================
// void Update(const System::Time&)  retail 0x18009e340
// Forwards the tick to every goal instance through GoalInstance SLOT 5 —
// `(this, const Time&, const GoalList&)`, passing THIS INSTANCE'S OWN list
// (`LEA R8,[RDI+0x10]`). A VIRTUAL call, on purpose: retail dispatches, and
// the instance is an exe subclass (TimeGoalInstance, FinishTutorialInstance).
// ============================================================================
void AchievementInstance::Update(const System::Time& time) {
    const int count = _goalList._goals._size;
    for (int i = 0; i < count; ++i)
        _goalList._goals._data[i]->Update(time, _goalList);
}

// ============================================================================
// void SetAchievement(ExternalPtr<Achievement>)  retail 0x18009e440
//     MOV [RCX+0x20],RDX            ; store FIRST (the argument is BARE, B7-5)
//     temp = achievement->_goals    ; 0x180003970, a deep copy into a fresh buffer
//     _goalList.Initialize(temp)    ; by value — the CALLEE tears `temp` down
// A null achievement faults at `achievement + 0x48`, as retail does.
// ============================================================================
void AchievementInstance::SetAchievement(Object::ExternalPtr<Achievement> achievement) {
    _achievement = achievement;

    GoalArray temp;
    temp.Assign(achievement->_goals);
    _goalList.Initialize(&temp);
}

// ############################################################################
// #  Achievement (the External resource)
// ############################################################################

// ??1Achievement@0@UEAA@XZ  0x18009e180 — EMPTY: see the header. The
// compiler's member destruction order (`_goals`, `_spriteName`,
// `_description`, `_title`) is retail's, then ~External().
Achievement::~Achievement() {}

// int GetFileVersion() const  retail 0x18009e170 — `return 3;` Scenario's
// override ORs 0x600 onto it (Scenario_Slot3, exe 0x1400a9460).
int32_t Achievement::GetFileVersion() const { return 3; }

// String GetTitle() / GetDescription() / GetSpriteName() const
// retail 0x1800013e0 / 0x180001420 / 0x1800013a0 — the same three-instruction
// body on a different field: the String copy constructor (an ADDREF).
System::String Achievement::GetTitle() const       { return _title; }
System::String Achievement::GetDescription() const { return _description; }
System::String Achievement::GetSpriteName() const  { return _spriteName; }

}  // namespace Achievement

// ############################################################################
// The CONSTRUCTOR doors — NOT members (B4b-2, B7-13).
// ############################################################################

// ============================================================================
// ??0AchievementInstance@Achievement@@QEAA@XZ  retail 0x18009e3b0
// CALL ??0Internal; stamp the MINTED 0x180154700; zero the GoalList (+0x10,
// two qwords) and +0x20. Returns `this` (fn_AchievementInstance_Ctor_t).
// ============================================================================
Achievement::AchievementInstance* RtAch_AchievementInstance_Ctor(Achievement::AchievementInstance* self) {
    InternalCtor()(self);
    *reinterpret_cast<void**>(self) = VtableAchievementInstance();   // B.3.7 bridge — the minted vptr
    self->_goalList._goals._data     = nullptr;
    self->_goalList._goals._size     = 0;
    self->_goalList._goals._capacity = 0;
    self->_achievement._ptr          = nullptr;
    return self;
}

// ============================================================================
// ??0Achievement@0@QEAA@XZ  retail 0x18009e240 — thirteen stores, nothing else:
//     [this] = ??_7Base; [this] = ??_7External;       (dead: inlined bases)
//     [+0x08] = [+0x10] = [+0x18] = 0
//     [this] = ??_7Achievement@0@6B@
//     [+0x20 .. +0x50] = 0
// Reached BY ADDRESS from code/Game/Game/scenario.cpp, and by Construct /
// Reconstruct (achievement_vtable.cpp). Over raw storage: nothing here is a
// constructed C++ object yet, so every field is written as its bytes.
// ============================================================================
void* RtAch_Achievement_Ctor(void* selfRaw) {
    uint8_t* self = static_cast<uint8_t*>(selfRaw);
    auto qword = [](uint8_t* p, size_t off) -> void*& {
        return *reinterpret_cast<void**>(p + off);
    };

    qword(self, 0x00) = VtableObjectBase();
    qword(self, 0x00) = VtableObjectExternal();
    qword(self, 0x08) = nullptr;
    qword(self, 0x10) = nullptr;
    qword(self, 0x18) = nullptr;
    qword(self, 0x00) = VtableAchievement();
    qword(self, 0x20) = nullptr;
    qword(self, 0x28) = nullptr;
    qword(self, 0x30) = nullptr;
    qword(self, 0x38) = nullptr;
    qword(self, 0x40) = nullptr;
    qword(self, 0x48) = nullptr;
    qword(self, 0x50) = nullptr;

    return self;
}

// ############################################################################
// The registered doors — one-line QUALIFIED delegations. Retslot doors keep
// `(this, ret, …)` and placement-new the member's prvalue (B5-12).
// ############################################################################

Achievement::AchievementManager* RtAch_AchievementManager_Get() {
    return Achievement::AchievementManager::Get();
}

int RtAch_AchievementManager_GetAchievementCount(const Achievement::AchievementManager* self) {
    return self->Achievement::AchievementManager::GetAchievementCount();
}

bool RtAch_AchievementManager_IsAchievementAwarded(const Achievement::AchievementManager* self, int index) {
    return self->Achievement::AchievementManager::IsAchievementAwarded(index);
}

void* RtAch_AchievementManager_GetAchievement(const Achievement::AchievementManager* self,
                                              void* retval, int index) {
    new (retval) Object::ExternalPtr<Achievement::Achievement>(
        self->Achievement::AchievementManager::GetAchievement(index));
    return retval;
}

void* RtAch_AchievementManager_GetAchievementInstance(const Achievement::AchievementManager* self,
                                                      void* retval, int index) {
    new (retval) Object::InternalPtr<Achievement::AchievementInstance>(
        self->Achievement::AchievementManager::GetAchievementInstance(index));
    return retval;
}

void RtAch_AchievementManager_SetEventHandler(Achievement::AchievementManager* self, void (*handler)(int)) {
    self->Achievement::AchievementManager::SetEventHandler(handler);
}

// The ExternalPtr argument is one pointer in RDX (B7-5).
void RtAch_AchievementManager_SetAchievements(Achievement::AchievementManager* self,
                                              Object::ExternalList* list) {
    self->Achievement::AchievementManager::SetAchievements(
        Object::ExternalPtr<Object::ExternalList>(list));
}

void RtAch_AchievementManager_Reset(Achievement::AchievementManager* self) {
    self->Achievement::AchievementManager::Reset();
}

void RtAch_AchievementManager_Update(Achievement::AchievementManager* self, const System::Time* time) {
    self->Achievement::AchievementManager::Update(*time);
}

const Achievement::GoalList* RtAch_AchievementInstance_GetGoalList(const Achievement::AchievementInstance* self) {
    return &self->Achievement::AchievementInstance::GetGoalList();
}

bool RtAch_AchievementInstance_IsMet(const Achievement::AchievementInstance* self) {
    return self->Achievement::AchievementInstance::IsMet();
}

bool RtAch_AchievementInstance_HasFailed(const Achievement::AchievementInstance* self) {
    return self->Achievement::AchievementInstance::HasFailed();
}

void RtAch_AchievementInstance_Update(Achievement::AchievementInstance* self, const System::Time* time) {
    self->Achievement::AchievementInstance::Update(*time);
}

// The ExternalPtr argument is one pointer in RDX (B7-5).
void RtAch_AchievementInstance_SetAchievement(Achievement::AchievementInstance* self,
                                              Achievement::Achievement* achievement) {
    self->Achievement::AchievementInstance::SetAchievement(
        Object::ExternalPtr<Achievement::Achievement>(achievement));
}

void* RtAch_Achievement_GetTitle(const Achievement::Achievement* self, void* retval) {
    new (retval) System::String(self->Achievement::Achievement::GetTitle());
    return retval;
}

void* RtAch_Achievement_GetDescription(const Achievement::Achievement* self, void* retval) {
    new (retval) System::String(self->Achievement::Achievement::GetDescription());
    return retval;
}

void* RtAch_Achievement_GetSpriteName(const Achievement::Achievement* self, void* retval) {
    new (retval) System::String(self->Achievement::Achievement::GetSpriteName());
    return retval;
}

void RtAch_Achievement_Dtor(Achievement::Achievement* self) {
    self->Achievement::Achievement::~Achievement();
}

int RtAch_Achievement_GetFileVersion(const Achievement::Achievement* self) {
    return self->Achievement::Achievement::GetFileVersion();
}

// ============================================================================
// Registration (unchanged). Every name is ONE string literal on ONE line —
// the census sees no other spelling. Address rows registered alongside every
// name (scenario.cpp reaches the Achievement lifecycle BY ADDRESS).
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

    // ---- Achievement lifecycle, reached from scenario.cpp BY ADDRESS ---------
    RegisterRuntimeExport("??0Achievement@0@QEAA@XZ", (void*)&RtAch_Achievement_Ctor); ++n;
    RegisterRuntimeAddr(0x18009e240, (void*)&RtAch_Achievement_Ctor);

    RegisterRuntimeExport("??1Achievement@0@UEAA@XZ", (void*)&RtAch_Achievement_Dtor); ++n;
    RegisterRuntimeAddr(0x18009e180, (void*)&RtAch_Achievement_Dtor);

    RegisterRuntimeExport("?GetFileVersion@Achievement@1@UEBAHXZ", (void*)&RtAch_Achievement_GetFileVersion); ++n;
    RegisterRuntimeAddr(0x18009e170, (void*)&RtAch_Achievement_GetFileVersion);

    printf("[RtPort]   Achievement::AchievementManager/Instance/Achievement: %d/23 exports "
           "registered here\n", n);
    return n;
}
