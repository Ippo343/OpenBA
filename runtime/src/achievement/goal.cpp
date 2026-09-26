// ============================================================================
// runtime/src/achievement/goal.cpp — Wave 5 lane A — Achievement::Goal /
//                                    GoalInstance / GoalList
// ============================================================================
// ENDGAME B.3.7 block B3: `Achievement::Goal`, `GoalInstance` and `GoalList`
// are real classes (runtime/types/achievement/Goal.h). Each ported body below
// is now a MEMBER; each registered door keeps its mangled name and retail
// address and is a one-line QUALIFIED, non-virtual delegation — qualified
// because a live Goal/GoalInstance wears the MINTED table or an EXE subclass's
// hand-built table, whose slot resolves back to the very door, so an
// unqualified virtual call inside a door would re-enter it (B34_BRIEFS §A.1).
//
// What is NOT a member, and why (B4b-2 / B7-13): the two CONSTRUCTOR doors.
// `Goal` and `GoalInstance` have exe subclasses with hand-built tables, and
// every construction in the tree goes through `??0Goal`/`??0GoalInstance`,
// which must stamp the MINTED tables. A C++ constructor would stamp GCC's.
// They stay free functions over the class's own fields.
//
// The DESTRUCTORS are members now, and real: `~Goal()` / `~GoalInstance()`
// are empty bodies whose compiler-emitted chain (vptr store, `~String` on
// `_text`, `~External` / `~Internal`) is exactly retail's
// re-stamp / release / tail-jump. The exe subclasses' slot-0 bodies call them
// as qualified member calls (code/Game/Goal/*.cpp).
//
// Retail's historical notes that still matter, kept from the Wave 5 banner:
//   * `??1Goal@Achievement@@QEAA@XZ`, `??1GoalInstance@…@QEAA@XZ`,
//     `?Persist@GoalInstance@…@QEAA…` and `?~Goal@…` do NOT exist in the
//     shipped export table — the destructors and Persist are VIRTUAL and
//     exported as UEAA. A QEAA spelling resolves null forever.
//   * Every body is transcribed from the disassembly. No null guards retail
//     lacks.
// ============================================================================

#include <cstdint>
#include <cstdio>
#include <cstring>

#include <atomic>
#include <new>          // placement new into the retslot doors

#include "../../funcs/rt_port.h"
#include "../../types/achievement/Goal.h"
#include "../../types/system/Value.h"

// Forward-declared rather than pulled in from standalone/resolve.h: that
// header drags in <windows.h>, and runtime/src/ stays platform-neutral
// (PLAN §4.1). Byte-identical to resolve.h's; the linker checks the pair.
enum class Module : int { Exe = 0, Runtime = 1, VideoDX11 = 2, COUNT = 3 };
uintptr_t ResolveAddr(Module mod, uintptr_t ghidra_addr);

namespace {

// The vtables the two constructor DOORS stamp. All EXPORTED data symbols,
// resolved by name so the door installs the MINTED table, never GCC's
// (B4b-2). Each is an RT_DATA_SLOT-shaped constant-initialised cache
// (`RtResolveCached`), which is the B6-5 thread shape: GoalInstance objects are
// constructed on the LOADER thread (GameUpdateLoop::Load -> GoalList::Initialize
// -> Factory::CreateObject), B37_RULES §5.
void* BaseVtable()         { return RT_EXPORT_ONCE(void*, "??_7Base@Object@@6B@"); }             // 0x180154788
void* ExternalVtable()     { return RT_EXPORT_ONCE(void*, "??_7External@Object@@6B@"); }         // 0x1801547a0
void* GoalVtable()         { return RT_EXPORT_ONCE(void*, "??_7Goal@Achievement@@6B@"); }        // 0x1801546b0
void* GoalInstanceVtable() { return RT_EXPORT_ONCE(void*, "??_7GoalInstance@Achievement@@6B@"); } // 0x180154678

// ??0Internal@Object@@QEAA@XZ  retail 0x1800bfd10 — allocates this object's
// handle-table slot. GoalInstance's constructor CALLs it (not inlined). Stays
// a by-address door: `Object::Internal` has no public real constructor either
// (B4b-2), and the address is registered by runtime/src/object/, so
// ResolveAddr lands on the port (registry first).
using PtrThisFn = void* (*)(void*);
std::atomic<void*> g_internalCtor{nullptr};
void CallInternalCtor(void* self) {
    void* p = g_internalCtor.load(std::memory_order_acquire);
    if (p == nullptr) {
        p = reinterpret_cast<void*>(ResolveAddr(Module::Runtime, 0x1800bfd10));
        if (p != nullptr) g_internalCtor.store(p, std::memory_order_release);
    }
    reinterpret_cast<PtrThisFn>(p)(self);
}

// `?_typeInfo@GoalInstance@Achievement@@0VBaseTypeInfo@Object@@A` @
// 0x1801f9940 — retail's Goal::GetInstanceType reads its first dword. Note the
// class asymmetry, which is not a typo: Goal::GetInstanceType returns
// GOALINSTANCE's type, because "the instance type" is what GoalList::Initialize
// asks the factory to create for this goal.
uint32_t GoalInstanceTypeId() {
    const uint32_t* p = RT_DATA_SLOT(const uint32_t*,
        "?_typeInfo@GoalInstance@Achievement@@0VBaseTypeInfo@Object@@A");
    return *p;
}

}  // namespace

namespace Achievement {

// ============================================================================
// ??1Goal@Achievement@@UEAA@XZ   retail 0x18009edb0
//
//     [this] = ??_7Goal@Achievement@@6B@     ; re-stamp
//     _text.~String()                        ; the release idiom, INCLUDING
//                                            ; retail's asymmetry (String.h)
//     JMP ~External                          ; tail call
//
// All three are the compiler's: the vptr store on entry to a destructor, the
// member destructor of `_text` (a `System::String`), then the base destructor.
// The vptr stored is GCC's table for Goal, not the minted one — correct for a
// destructor (nothing dispatches through it before ~External re-stamps), and
// the one thing a hand-written store here could get wrong.
// ============================================================================
Goal::~Goal() {}

// ============================================================================
// Object::Type Goal::GetInstanceType() const — SLOT 9, RULE B7-4
// ?GetInstanceType@Goal@Achievement@@UEBA?AVType@Object@@XZ   retail 0x18009eeb0
//
//     MOV EAX,dword ptr [0x1801f9940]   ; GoalInstance::_typeInfo.typeId
//     MOV dword ptr [RDX],EAX           ; *ret
//     MOV RAX,RDX                       ; return ret
//     RET
//
// `this` is never read: the answer is a class constant. The hidden return slot
// is raw storage, so the value is placement-constructed into it.
// ============================================================================
Object::Type* Goal::GetInstanceType(Object::Type* ret) const {
    return new (ret) Object::Type(GoalInstanceTypeId());
}

// ============================================================================
// System::String Goal::GetText() const
// ?GetText@Goal@Achievement@@QEBA?AVString@System@@XZ   retail 0x180001ba0
// `*retval = 0; if (src) { *retval = src; ++src[-4]; }` — the String copy
// constructor (0x180006cb0).
// ============================================================================
System::String Goal::GetText() const {
    return _text;
}

// ============================================================================
// ??1GoalInstance@Achievement@@UEAA@XZ   retail 0x18009ef40
// `[this] = ??_7GoalInstance; JMP ~Internal` — both the compiler's. ~Internal
// IS the InternalFactory swap-remove (runtime/src/object/internal_class.cpp).
// ============================================================================
GoalInstance::~GoalInstance() {}

// ?IsMet@GoalInstance@Achievement@@QEBA_NXZ      retail 0x18009eed0 — bit 0.
bool GoalInstance::IsMet() const { return (_flags & kFlagMet) != 0; }

// ?HasFailed@GoalInstance@Achievement@@QEBA_NXZ  retail 0x18009eec0 — bit 1.
bool GoalInstance::HasFailed() const { return ((_flags >> 1) & 1u) != 0; }

// ============================================================================
// void GoalInstance::Update(const System::Time&, const GoalList&) — SLOT 5
// ?Update@GoalInstance@Achievement@@UEAAXAEBVTime@System@@AEBVGoalList@2@@Z
//                                              retail 0x18009f010
//
// Retail reads NEITHER argument (they are for the exe overrides). It polls the
// virtual CheckGoalState (slot 6) — an UNQUALIFIED call on purpose: retail
// dispatches (`CALL [RAX+0x30]`), and on an exe subclass that is the
// subclass's override — and folds the answer into the flags. The FAILED bit
// LATCHES: once set, the entry test makes every later Update a no-op.
// ============================================================================
void GoalInstance::Update(const System::Time& /*time*/, const GoalList& /*list*/) {
    if (((_flags >> 1) & 1u) != 0)
        return;

    const Goal::GoalState state = CheckGoalState();

    if (state == Goal::kMet)    { _flags |= kFlagMet;  return; }
    if (state == Goal::kNotMet) { _flags &= 0xfffffffeu; return; }
    if (state == Goal::kFailed) { _flags |= kFlagFailed; }
}

// ============================================================================
// GoalList::GoalList()  ??0GoalList@Achievement@@QEAA@XZ  retail 0x18009f0b0
// Two zero qwords — `Collection::Array`'s own default constructor.
// ============================================================================
GoalList::GoalList() {}

// ============================================================================
// GoalList::GoalList(const GoalList&)  ??0GoalList@…@QEAA@AEBV01@@Z  0x180001730
//     XOR EAX,EAX; MOV [RCX],RAX; MOV [RCX+8],RAX    ; the member's zero init
//     CALL 0x180003a30                               ; Array<InternalPtr>::operator=
// 0x180003a30 is the kSpec_PtrZero `operator=` (Assign8Z per element into a
// freshly allocated buffer) — `Collection::Array::Assign` driven by the row
// `_goals` names in its type. See Goal.h: the copy SHARES the instances.
// ============================================================================
GoalList::GoalList(const GoalList& other) : _goals() {
    _goals.Assign(other._goals);
}

// ??4GoalList@Achievement@@QEAAAEAV01@AEBV01@@Z  retail 0x180001760:
// `CALL 0x180003a30; MOV RAX,RBX; RET`.
GoalList& GoalList::operator=(const GoalList& other) {
    _goals.Assign(other._goals);
    return *this;
}

// ============================================================================
// bool GoalList::AreGoalsMetExcept(InternalPtr<GoalInstance>) const
// ?AreGoalsMetExcept@GoalList@Achievement@@QEBA_NV?$InternalPtr@VGoalInstance@Achievement@@@Object@@@Z
//                                              retail 0x18009f070
//
// The InternalPtr arrives by HIDDEN POINTER (`MOV RDX,[RDX]` at 0x18009f07c).
// Two details the decompiler gets wrong, both from the disassembly:
//   * the count is loaded ONCE (`MOVSXD R9,[RCX+8]`) — hoisted here too;
//   * the element read is UNGUARDED (`TEST byte [RCX+0x10],0x1` straight out
//     of the array): a null entry faults, in retail too. Not "fixed".
// ============================================================================
bool GoalList::AreGoalsMetExcept(const Object::InternalPtr<GoalInstance>* except) const {
    const GoalInstance* const skip = except->Get();
    const int64_t count = _goals._size;      // loaded once, as retail does
    for (int64_t i = 0; i < count; ++i) {
        const GoalInstance* g = _goals._data[i].Get();
        if (g != skip && (g->_flags & GoalInstance::kFlagMet) == 0)
            return false;
    }
    return true;
}

}  // namespace Achievement

// ############################################################################
// The CONSTRUCTOR doors — NOT members (B4b-2, B7-13; see the banner).
// ############################################################################

// ??0Goal@Achievement@@QEAA@XZ   retail 0x18009ed50
//     [this] = ??_7Base; [this] = ??_7External;           (dead: MSVC's inlined
//     [+0x08] = [+0x10] = [+0x18] = 0;                     base constructors)
//     [this] = ??_7Goal; [+0x20] = [+0x28] = 0
// Six qword stores covering 0x00..0x2f exactly. The two dead stores are kept:
// a debugger or detour between them would make them observable.
Achievement::Goal* RtAch_Goal_Ctor(Achievement::Goal* self) {
    *reinterpret_cast<void**>(self) = BaseVtable();        // B.3.7 bridge — the minted vptr
    *reinterpret_cast<void**>(self) = ExternalVtable();    // B.3.7 bridge
    self->_path.data     = nullptr;                        // +0x08
    self->_refs._data    = nullptr;                        // +0x10
    self->_refs._size    = 0;                              // +0x18 — one qword store over
    self->_refs._capacity = 0;                             //         size and capacity
    *reinterpret_cast<void**>(self) = GoalVtable();        // B.3.7 bridge
    self->_stringTable._ptr = nullptr;                     // +0x20
    self->_text._data       = nullptr;                     // +0x28 — raw bytes, not constructed
    return self;   // retail returns `this` in RAX
}

// ??0GoalInstance@Achievement@@QEAA@XZ   retail 0x18009ef00
//     CALL ??0Internal; [this] = ??_7GoalInstance;
//     MOV dword [+0x10],0 (a DWORD — +0x14 is left alone); [+0x18] = 0
Achievement::GoalInstance* RtAch_GoalInstance_Ctor(Achievement::GoalInstance* self) {
    CallInternalCtor(self);
    *reinterpret_cast<void**>(self) = GoalInstanceVtable();   // B.3.7 bridge — the minted vptr
    self->_flags     = 0;
    self->_goal._ptr = nullptr;
    return self;
}

// ############################################################################
// The registered doors — one-line QUALIFIED delegations.
// ############################################################################

// ??1Goal@Achievement@@UEAA@XZ  0x18009edb0
void RtAch_Goal_Dtor(Achievement::Goal* self) {
    self->Achievement::Goal::~Goal();
}

// ??1GoalInstance@Achievement@@UEAA@XZ  0x18009ef40
void RtAch_GoalInstance_Dtor(Achievement::GoalInstance* self) {
    self->Achievement::GoalInstance::~GoalInstance();
}

// ?GetGoalCount@GoalList@Achievement@@QEBAHXZ  0x180001700
int RtAch_GoalList_GetGoalCount(const Achievement::GoalList* self) {
    return self->Achievement::GoalList::GetGoalCount();
}

// ?GetGoal@GoalList@…  0x180001710 — HIDDEN RETURN SLOT: (this, ret, index).
// The index is the THIRD argument (reference_hidden_retslot_wild_write.md).
void* RtAch_GoalList_GetGoal(const Achievement::GoalList* self, void* retval, int index) {
    new (retval) Object::InternalPtr<Achievement::GoalInstance>(
        self->Achievement::GoalList::GetGoal(index));
    return retval;
}

// ?GetGoal@GoalInstance@…  0x180001fe0 — (this, ret).
void* RtAch_GoalInstance_GetGoal(const Achievement::GoalInstance* self, void* retval) {
    new (retval) Object::ExternalPtr<Achievement::Goal>(
        self->Achievement::GoalInstance::GetGoal());
    return retval;
}

// ?GetInstanceType@Goal@…  0x18009eeb0 — (this, ret), the slot-9 shape.
Object::Type* RtAch_Goal_GetInstanceType(const Achievement::Goal* self, Object::Type* retval) {
    return self->Achievement::Goal::GetInstanceType(retval);
}

// ?GetText@Goal@…  0x180001ba0 — (this, ret). `retval` is raw storage.
void* RtAch_Goal_GetText(const Achievement::Goal* self, void* retval) {
    new (retval) System::String(self->Achievement::Goal::GetText());
    return retval;
}

// ?IsMet@GoalInstance@…  0x18009eed0
bool RtAch_GoalInstance_IsMet(const Achievement::GoalInstance* self) {
    return self->Achievement::GoalInstance::IsMet();
}

// ?HasFailed@GoalInstance@…  0x18009eec0
bool RtAch_GoalInstance_HasFailed(const Achievement::GoalInstance* self) {
    return self->Achievement::GoalInstance::HasFailed();
}

// ?AreGoalsMetExcept@GoalList@…  0x18009f070 — the InternalPtr by hidden pointer.
bool RtAch_GoalList_AreGoalsMetExcept(const Achievement::GoalList* self,
                                      const Object::InternalPtr<Achievement::GoalInstance>* except) {
    return self->Achievement::GoalList::AreGoalsMetExcept(except);
}

// ?Update@GoalInstance@…  0x18009f010
void RtAch_GoalInstance_Update(Achievement::GoalInstance* self, const System::Time* time,
                               const Achievement::GoalList* list) {
    self->Achievement::GoalInstance::Update(*time, *list);
}

// ============================================================================
// Registration — by NAME and by ADDRESS, unchanged. Every mangled name is ONE
// string literal on ONE line (tools/runtime_census.py cannot see any other
// spelling — reference_census_cannot_see_nonliteral_registrations.md).
// `Goal::Serialize` and `GoalInstance::Persist` register from
// goal_serialize.cpp; the GoalList copy constructor and `operator=` are real
// members but have no caller and no registration (an unregistered address is
// the honest state of a body nothing reaches).
// ============================================================================
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

    printf("[RtPort] Achievement::Goal: %d/15 exports registered here "
           "(+2 in goal_serialize.cpp: Goal::Serialize, GoalInstance::Persist)\n", n);
    return n;
}
