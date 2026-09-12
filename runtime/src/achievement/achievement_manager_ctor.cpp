// ============================================================================
// runtime/src/achievement/achievement_manager_ctor.cpp — WAVE 27 lane ACHIEVE
// Achievement::AchievementManager::AchievementManager()
// ============================================================================
//
//   ??0AchievementManager@Achievement@@QEAA@XZ               retail 0x18009eb00
//
// THIS FILE EXISTS TO RETIRE HALF OF A NAMED BLOCKER, and it is a separate file
// for exactly that reason: someone asking "did the expiry fire?" should find
// one file, not a hunk inside a 1200-line one.
//
// standalone/reg_mint_promoted.cpp:2714 tags
// `?_singleton@AchievementManager@Achievement@@0PEAV12@EA` UNPROMOTABLE with:
//
//     "an INVERTED-SHAPE row: only 1 in-DLL read against 5 writes, and the harm
//      is on the writer side. Two of the five writers,
//      ??0AchievementManager@Achievement@@ and a store inside
//      ??0DialogLoopT@Engine@@'s range, are UNPORTED, so retail constructs the
//      manager into retail's slot and a promoted slot would hold whatever our
//      ported Engine::Startup/Shutdown last wrote -- two owners, one object.
//      A LOW READ COUNT IS NOT A SAFE ROW."
//      [ADDRESSEE: lane MANIFEST of Wave 25;
//       expiry = ??0AchievementManager@Achievement@@ ported AND registered]
//
// This file ports it and registers it, by NAME and by ADDRESS. **That retires
// ONE of the two named writers and DOES NOT retire the tag.** The second,
// "a store inside ??0DialogLoopT@Engine@@'s range", is Wave 27 lane LOOPS's
// table (`??_7DialogLoopT@Engine@@6B@`, 10/12), not this lane's — this lane may
// not touch it (kickoff §3). The tag stays as written until LOOPS or a
// successor closes that half; what changes is that its expiry clause now names
// ONE outstanding writer instead of two. The correction is carried beside the
// tag itself, in reg_mint_promoted.cpp, because that is the surface the next
// person reaches (kickoff §4 #9) — a lane doc alone would not be read.
//
// ============================================================================
// THE BODY, READ FROM THE SHIPPED RETAIL BYTES
// ============================================================================
// Decoded with `pefile` + `objdump -D -b binary`, not from the MCP
// disassembler; see goal_list.cpp's banner for why. The program identity was
// checked with the Wave 27 discriminator — `disassemble_function(0x180002190)`
// returned the RETAIL two-instruction `MOV RAX,[0x1801fae10] ; RET`, not the
// profile build's ~40-instruction body — so nothing in this lane is
// profile-derived, and every decode was independently re-taken from the shipped
// file anyway.
//
//   0x18009eb00  48 89 4c 24 08     MOV [RSP+8],RCX        ; home, no semantics
//   0x18009eb05  48 83 ec 18        SUB RSP,0x18
//   0x18009eb09  48 c7 04 24 fe…    MOV [RSP],-2           ; MSVC EH state
//   0x18009eb11  33 c0              XOR EAX,EAX
//   0x18009eb13  48 89 01           MOV [RCX+0x00],RAX
//   0x18009eb16  48 89 41 08        MOV [RCX+0x08],RAX
//   0x18009eb1a  48 89 41 10        MOV [RCX+0x10],RAX
//   0x18009eb1e  48 89 41 18        MOV [RCX+0x18],RAX
//   0x18009eb22  48 89 41 20        MOV [RCX+0x20],RAX
//   0x18009eb26  48 89 41 28        MOV [RCX+0x28],RAX
//   0x18009eb2a  48 89 41 30        MOV [RCX+0x30],RAX
//   0x18009eb2e  48 89 41 38        MOV [RCX+0x38],RAX
//   0x18009eb32  89 41 40           MOV dword [RCX+0x40],EAX   *** DWORD ***
//   0x18009eb35  48 89 0d 34 db 15  MOV [0x1801fc670],RCX      ; _singleton = this
//   0x18009eb3c  48 8b c1           MOV RAX,RCX                ; return this
//
// EIGHT QWORDS AND ONE DWORD. +0x44 is NEVER WRITTEN — the last store is a
// 32-bit `MOV dword`, not a qword, so the four bytes at +0x44 keep whatever the
// allocation held. runtime/types/achievement/AchievementManager.h already
// declares that as `_pad44` with the comment "never read or written", and this
// body is the writer that proves it. A port that zeroed +0x44 "for tidiness"
// would be writing four bytes retail leaves alone — and the full-byte oracle
// below would catch it, which is why the oracle excludes nothing.
//
// THE SINGLETON STORE IS THE LAST EFFECT, and it is unconditional: there is no
// "already constructed" test. Constructing a second manager silently rebinds
// the singleton to it. That is retail and it is not guarded here.
//
// IT IS THE EXACT MIRROR OF THE DESTRUCTOR, which achievement_state.cpp already
// ports: the dtor's FIRST store nulls the same cell, before freeing the three
// arrays. Ctor writes it last, dtor clears it first — so the window in which
// the cell points at a partially-built or partially-torn-down object is empty
// in both directions. That symmetry is worth stating because it is the thing a
// reordering would break silently.
//
// ============================================================================
// ORACLE — FULL BYTE, WITH ONE REAL LIMIT NAMED
// ============================================================================
// The ctor's writes are confined to `this` and cover 0x00..0x43 of a 0x48-byte
// object, so running the DLL's copy into a poisoned scratch buffer proves every
// byte rather than an invariant — the same oracle `RtAch_Achievement_Ctor`
// carries, and for the same reason.
//
// EXCEPT FOR ONE SIDE EFFECT, WHICH IS WHY THIS IS NOT A COPY-PASTE OF THAT
// ONE: retail's ctor also stores `this` into the singleton cell. Run on scratch,
// it leaves the singleton pointing at a dead stack buffer. The cell is
// therefore SNAPSHOTTED before the oracle call and RESTORED immediately after.
//
// THE LIMIT, stated rather than hidden: the restore goes through
// `RT_DATA_SLOT`, which yields OUR minted cell once the row is promoted, while
// retail's ctor writes the DLL's cell by a hard-coded RVA it does not consult
// anyone about. While the row is UNPROMOTED — which is every run today, the row
// being tagged UNPROMOTABLE — those are the same cell and the restore is exact.
// The moment the row IS promoted they are two cells, and this oracle would
// leave the DLL's cell pointing at dead scratch.
//   ADDRESSEE: the lane that first promotes
//   `?_singleton@AchievementManager@Achievement@@0PEAV12@EA` must delete the
//   `rt_diff_on__` block below in the same commit. It is guarded by
//   BANISHED_RT_DIFF and so is inert on a default run, but "inert by default"
//   is not "safe once promoted".
//
// KILL SWITCH: BANISHED_RT_ACHMGR, with the rest of the class.
// ============================================================================

#include <cstdint>
#include <cstring>

#include "../../funcs/rt_port.h"
#include "../../types/achievement/AchievementManager.h"

using RtAchievement::AchievementManager;

extern "C" void* RtAch_AchievementManager_Ctor(void*);

namespace {

// `?_singleton@AchievementManager@Achievement@@0PEAV12@EA` — retail 0x1801fc670.
// Read through RT_DATA_SLOT, exactly as achievement_state.cpp's
// `ManagerSingletonCell()` does for the destructor, so ctor and dtor always
// write the SAME cell whichever way the row is promoted. Declared here rather
// than shared: that one lives in another file's anonymous namespace, and a
// header shared between them would be a new coupling for one line.
AchievementManager** ManagerSingletonCell() {
    return RT_DATA_SLOT(AchievementManager**,
        "?_singleton@AchievementManager@Achievement@@0PEAV12@EA");
}

typedef void* (*CtorFn)(void*);

}  // namespace

// ============================================================================
// AchievementManager* Achievement::AchievementManager::AchievementManager()
// ??0AchievementManager@Achievement@@QEAA@XZ               retail 0x18009eb00
//
// NON-virtual (QEAA), returns `this` in RAX as every MSVC constructor does.
// ============================================================================
extern "C" void* RtAch_AchievementManager_Ctor(void* selfRaw) {
    RT_DIFF_BEGIN("Achievement::AchievementManager::ctor");

    uint8_t* self = static_cast<uint8_t*>(selfRaw);
    auto qword = [](uint8_t* p, size_t off) -> void*& {
        return *reinterpret_cast<void**>(p + off);
    };

    qword(self, 0x00) = nullptr;   // _achievements
    qword(self, 0x08) = nullptr;   // _completed._data
    qword(self, 0x10) = nullptr;   // _completed._size / _capacity
    qword(self, 0x18) = nullptr;   // _instances._data
    qword(self, 0x20) = nullptr;   // _instances._size / _capacity
    qword(self, 0x28) = nullptr;   // _eventHandler
    qword(self, 0x30) = nullptr;   // _updateTimes._data
    qword(self, 0x38) = nullptr;   // _updateTimes._size / _capacity
    // A DWORD, not a qword. +0x44 stays untouched — see the banner.
    *reinterpret_cast<int32_t*>(self + 0x40) = 0;   // _lastAchievementUpdate

    // Last, and unconditional.
    *ManagerSingletonCell() = reinterpret_cast<AchievementManager*>(self);

    if (rt_diff_on__) {
        // Retail's copy stores `this` into the singleton cell; run on scratch
        // that would leave the cell pointing at a dead stack buffer. Snapshot
        // and restore around it. See the ADDRESSEE note in the banner for the
        // one condition under which this restore stops being exact.
        AchievementManager** cell  = ManagerSingletonCell();
        AchievementManager*  saved = *cell;

        alignas(16) uint8_t scratch[sizeof(AchievementManager)];
        // Poisoned, so a byte retail writes and we do not shows up as a
        // divergence instead of accidentally matching a zero — and so the
        // never-written +0x44 shows up as 0xCDCDCDCD on BOTH sides, which is
        // the positive evidence that neither writes it.
        std::memset(scratch, 0xCD, sizeof(scratch));
        RT_ORIGINAL_ONCE(CtorFn, "??0AchievementManager@Achievement@@QEAA@XZ")(scratch);

        *cell = saved;

        RT_DIFF_BYTES(self, scratch, sizeof(AchievementManager));
    }
    return self;
}

// ---- Registration ----------------------------------------------------------
// BY NAME **AND** BY ADDRESS, and both halves are load-bearing here rather than
// belt-and-braces:
//   * the NAME is the expiry clause's literal text ("ported AND registered")
//     and is what tools/runtime_census.py counts;
//   * the ADDRESS is what makes a by-address call site land on this body
//     (reference_resolveaddr_consults_port_registry_first.md).
//
// Gated by BANISHED_RT_ACHMGR with achievement_manager.cpp,
// achievement_state.cpp, achievement_serialize.cpp and achievement_vtable.cpp —
// class atomicity (PLAN §4.1 decision 5). It matters more than usual for this
// one: the destructor that nulls this singleton is already on that switch, and
// a build where the dtor is ours and the ctor is the DLL's would have two
// different opinions about which cell holds the manager.
int RtAchManagerCtorRegister() {
    int n = 0;
    RegisterRuntimeExport("??0AchievementManager@Achievement@@QEAA@XZ",
                          (void*)&RtAch_AchievementManager_Ctor); ++n;
    RegisterRuntimeAddr(0x18009eb00ull, (void*)&RtAch_AchievementManager_Ctor);
    return n;
}
