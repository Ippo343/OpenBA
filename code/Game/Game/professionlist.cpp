// code/Game/Game/professionlist.cpp
// Clean reimplementation of ProfessionList game logic.
//
// ProfessionList (0x50 bytes) -- container for profession references.
// Extends Object::External (0x20 base).
// Vtable at 0x14015ef50.
//
// ENDGAME B.3.8 block LOOP: a REAL class (code/Game/Game/ProfessionList.h, field
// layout and retail slot map there) with a compiler-generated vtable. Gone: the
// hand-built 9-slot `g_PL_Vtable`, its stubs, the lazy vtable resolve in the old
// Init, the lazily-resolved ExternalPtr door and the C entry points.
//
// Contains a dynamic array at +0x20/+0x28 and four ExternalPtr<Profession>
// references at +0x30, +0x38, +0x40, +0x48.
//
// Decompiled from Application-x64.exe.

#include "Game/ProfessionList.h"
#include "Game/Profession.h"   // caller batch CB6: Profession::TypeId(), its fields
#include "object/TypeInfo.h"
#include "collection/Collection.h"
#include "object/External.h"   // ENDGAME B.3.3 step 6A — must precede the
                                // `namespace Object { ... }` forward decl below
                                // (NAMESPACE HAZARD: spell ::Object::X everywhere)
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch B
#include "object/Internal.h" // Object::detail::InternalMemAlloc / InternalMemFree
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <new>
#include "Game/exe_bodies.h"

// FUN_140018020(ExternalPtr<Profession>*, Serializer*) -- typed ExternalPtr
// serialize. The address is registered to this body (address_registry.cpp); it
// is called directly (the old lazily-resolved pointer, a non-atomic first-call
// cache on the loader thread, B6-5, resolved to it).
extern "C" void Reimpl_ExternalPtr_Serialize_StringTable(void* refField, void* serializer);

// ============================================================
// ProfessionList::ProfessionList
// ============================================================
// Mirrors the inline ctor from ProfessionList_Factory (0x14001e5c0).
// External() and the 0x14015ef50 stamp are the compiler's; then zero all fields.
ProfessionList::ProfessionList() {
    // +0x20 professions: Collection::Array() is the three zero stores.
    field_0x30 = nullptr;
    field_0x38 = nullptr;
    childProfession = nullptr;
    studentProfession = nullptr;
}

// ============================================================
// ProfessionList::~ProfessionList / DeletingDtor (vtable +0x00)
// ============================================================
// Original: 0x14001e760
// Frees the dynamic array at +0x20, zeros +0x20 and +0x28; External::~External
// is the compiler's chain. Conditionally frees self.
//
// ENDGAME B.3.2 step 2: the array free is `Collection::Array<void*>::~Array`
// (re-decompiled 2026-09-13: MemFree(+0x20); +0x20 = 0; one qword over +0x28 —
// ArrayDtorImpl exactly; Profession* is trivially destructible so there is no
// element step). Trait-derived row: byteCopy, no init, no destroy, align 8 —
// the same row Startup's append below uses.
// The array is the `professions` member: the compiler runs its ~Array.
ProfessionList::~ProfessionList() {}

// ============================================================
// ProfessionList::Reconstruct (vtable +0x30, Slot6) -- Reset to defaults
// ============================================================
// Original: 0x14001e690. Destroys (slot 0, flags 0) then re-constructs in place.
Object::External* ProfessionList::Reconstruct() {
    this->~ProfessionList();
    return ::new (static_cast<void*>(this)) ProfessionList();
}

// [7] 0x140001640 — the shared body `MOV RAX,[RCX]; XOR EDX,EDX; JMP [RAX]`:
// slot 0 with flags 0 (destroy, no free).
void ProfessionList::Destruct() { this->~ProfessionList(); }

// ============================================================
// ProfessionList::Construct (vtable +0x40)
// ============================================================
// Original: 0x14001e630 — placement-new of the class on `this`.
Object::External* ProfessionList::Construct() {
    return ::new (static_cast<void*>(this)) ProfessionList();
}

// ============================================================
// ProfessionList::Serialize (vtable +0x28, Slot5) -- Serialize refs
// ============================================================
// Original: 0x1400a8e30
// Creates a Serializer(stream, version=0), then serializes four
// ExternalPtr<Profession> refs at +0x30, +0x38, +0x40, +0x48
// using FUN_140018020 (typed ExternalPtr serialize). Returns AL=1.
bool ProfessionList::Serialize(IO::Stream& stream) {
    IO::Serializer serializer(stream, 0);

    // Serialize four ExternalPtr<Profession> references
    Reimpl_ExternalPtr_Serialize_StringTable(&field_0x30, &serializer);         // +0x30
    Reimpl_ExternalPtr_Serialize_StringTable(&field_0x38, &serializer);         // +0x38
    Reimpl_ExternalPtr_Serialize_StringTable(&childProfession, &serializer);    // +0x40
    Reimpl_ExternalPtr_Serialize_StringTable(&studentProfession, &serializer);  // +0x48
    return true;
}

// ============================================================
// ProfessionList::Startup (0x1400a90a0) — build the profession ARRAY
// ============================================================
// Iterates every Profession external in the ExternalFactory (TypeInfo_Profession
// @ 0x1401c5a98), pushes each with the player-selectable flag (+0x4e) set into
// the dynamic array at +0x20/+0x28/+0x2c (grow ×3), then sorts ascending by the
// profession's sort priority (+0x48) — FUN_14001e7c0 is an introsort on that key;
// an insertion sort gives the same ordering (N ≈ a dozen professions).
//
// Called from GameUpdateLoop::Startup (gameupdateloop.cpp resolves exe 0x1400a90a0
// via fn_ProfessionList_Startup — registered for standalone below). Without it the
// array stays empty (count 0) and every profession-array consumer (TownHallUI
// General/Professions tabs, ProfessionUI) sees zero professions and shows blank.
//
// ENDGAME B.3.2 step 2: the push WAS a hand-written copy of `rtcoll::SetSizeCore`
// in its three-argument form. Retail 0x1400a90a0 (re-decompiled 2026-09-13)
// inlines it with the found Profession* as the fill: `n = count+1; if (n !=
// count) {...}` (the exact-equal early-out, always taken), `_size = n`, `if (cap
// < n) { cap = n; if (count != 0) cap = n*3; MemAlloc(cap << 3, 8) }`, copy the
// prefix on a move else start at count, `dst[i] = i < count ? old[i] : it`, skip
// a null computed address, `if (old != data) MemFree(old)`. The capacity
// spelling `(count != 0) ? n*3 : n` equals the reference `n == old+1 && old != 0
// ? n*3 : n` BECAUSE n is count+1 by construction — this loop is the only
// writer. So it is `Array<void*>::SetSize(count+1, it)` on the trait-derived row
// (byteCopy, no init, no destroy, align 8). The previous port's
// `if (!buf) { count restore; continue; }` was an out-of-memory-only guard
// retail does not have (SetSizeCore skips a null element address instead).
// Caller batch CB6: the list's and each Profession's fields are read by name;
// the by-name fallback for the Profession type id is Profession::TypeId().
// The tree keeps three tests retail 0x1400a90a0 lacks (known_port_issues 14d,
// GAME-b): the null ExternalFactory singleton and the 0 type id return early, and
// an external that is not a Profession is skipped where retail reads +0x4e of the
// null it substitutes.
void ProfessionList::Startup() {
    if (!::Object::ExternalFactory::_singleton)
        return;

    // 0x1401c5a98 is Profession's type-id cell (Table 1).
    uint32_t profTid = Profession::TypeId();
    if (!profTid) return;

    const ::Object::ExternalFactory* efactory = ::Object::ExternalFactory::_singleton;
    const ::Object::External* found = efactory->FindNext(profTid, nullptr);
    for (; found; found = efactory->FindNext(profTid, found)) {
        if (!found->IsDerivedFrom(profTid)) continue;
        // LEFT: FindNext returns a const External*; the array holds Profession*.
        Profession* prof = const_cast<Profession*>(static_cast<const Profession*>(found));
        if (prof->isAvailable == 0) continue;  // +0x4e: not player-selectable

        // Growable push into +0x20 (buf) / +0x28 (count) / +0x2c (cap).
        professions.SetSize(professions.GetSize() + 1, prof);
    }

    // Sort ascending by sort priority (Profession+0x48) — FUN_14001e7c0 semantics.
    int n = professions.GetSize();                               // +0x28 count
    Profession** arr = professions.GetData();                    // +0x20
    if (n > 1 && arr) {
        for (int i = 1; i < n; i++) {
            Profession* key = arr[i];
            int keyPri = key->field48;   // +0x48
            int j = i - 1;
            while (j >= 0 && arr[j]->field48 > keyPri) {
                arr[j + 1] = arr[j];
                j--;
            }
            arr[j + 1] = key;
        }
    }

#ifdef _DEBUG
    printf("[ProfessionList] Startup: %d professions in array (tid=0x%08x)\n",
           professions.GetSize(), profTid);
#endif
}

// B4 FORWARD: 1 outside caller (gameupdateloop.cpp, GAME-a).
// (Its one caller tests the pointer for null first; the old body's own test of it
// was unreachable and is gone.)
extern "C" void ProfessionList_Startup_Impl(ProfessionList* profList) {
    profList->Startup();
}

// ============================================================
// Reflection: create / createInPlace, the registration
// ============================================================
// ProfessionList (Object::External, 0x50) is loaded from Profession.rsc during the
// in-game world build (GameUpdateLoop::Load step 2).
void* ProfessionList::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(ProfessionList), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(::new (p) ProfessionList());
}
void* ProfessionList::CreateInPlaceObject(void* memory) {
    if (!memory) return nullptr;
    return static_cast<Object::Base*>(::new (memory) ProfessionList());
}

DEFINE_EXE_TYPE(ProfessionList, "Object::External")
