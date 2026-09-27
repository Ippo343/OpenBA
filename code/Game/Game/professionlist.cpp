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
#include "engine_funcs.h"
#include "resolve.h"
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
    arrayData = nullptr;
    arraySize = 0;
    profRef1 = nullptr;
    profRef2 = nullptr;
    profRef3 = nullptr;
    profRef4 = nullptr;
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
ProfessionList::~ProfessionList() {
    // B.3.2 bridge: the header is still spelled `arrayData` + a 64-bit
    // `arraySize` covering count and capacity.
    reinterpret_cast<Collection::Array<void*>*>(&arrayData)->~Array();
}

Object::Base* ProfessionList::DeletingDtor(unsigned flags) {
    this->~ProfessionList();
    if (flags & 1) Object::detail::InternalMemFree(this);
    return this;
}

// ============================================================
// ProfessionList::Reconstruct (vtable +0x30, Slot6) -- Reset to defaults
// ============================================================
// Original: 0x14001e690. Destroys (slot 0, flags 0) then re-constructs in place.
Object::External* ProfessionList::Reconstruct() {
    DeletingDtor(0);
    return ::new (static_cast<void*>(this)) ProfessionList();
}

// [7] 0x140001640 — the shared body `MOV RAX,[RCX]; XOR EDX,EDX; JMP [RAX]`:
// slot 0 with flags 0 (destroy, no free).
void ProfessionList::Destruct() { DeletingDtor(0); }

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
bool ProfessionList::Serialize(void* stream) {
    // Stack-allocated serializer (24 bytes)
    alignas(8) uint8_t serializer[24];
    ::new (static_cast<void*>(serializer)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge

    // Serialize four ExternalPtr<Profession> references
    Reimpl_ExternalPtr_Serialize_StringTable(&profRef1, serializer);  // +0x30
    Reimpl_ExternalPtr_Serialize_StringTable(&profRef2, serializer);  // +0x38
    Reimpl_ExternalPtr_Serialize_StringTable(&profRef3, serializer);  // +0x40
    Reimpl_ExternalPtr_Serialize_StringTable(&profRef4, serializer);  // +0x48

    reinterpret_cast<IO::Serializer*>(serializer)->~Serializer();   // B.3.4 bridge
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
extern "C" void ProfessionList_Startup_Impl(void* profList) {
    ProfessionList* const self = static_cast<ProfessionList*>(profList);   // B.3.8 bridge
    if (!self || !fn_IsDerivedFrom ||
        !g_pExternalFactorySingleton || !*g_pExternalFactorySingleton)
        return;

    // B.3.2 bridge: ProfessionList+0x20 is still a raw {data, count|cap} header.
    auto* profArray = reinterpret_cast<Collection::Array<void*>*>(&self->arrayData);

    // Profession type id: raw .rdata dword when the exe is mapped, else the class's.
    uint32_t profTid = 0;
    {
        uint32_t* p = reinterpret_cast<uint32_t*>(ResolveAddr(Module::Exe, 0x1401c5a98));
        if (p && *p) profTid = *p;
    }
    if (!profTid) profTid = Profession::TypeId();
    if (!profTid) return;

    void* factory = *g_pExternalFactorySingleton;
    // ENDGAME B.3.3 step 6A: member calls on the real Object::ExternalFactory.
    // The singleton is still read through the EXISTING door, so the guard
    // above still gates the same cell and the same registered body still runs.
    const ::Object::ExternalFactory* efactory =
        reinterpret_cast<const ::Object::ExternalFactory*>(factory);   // B.3.3 bridge
    const ::Object::External* found = efactory->FindNext(profTid, nullptr);
    for (; found; found = efactory->FindNext(profTid, found)) {
        void* it = const_cast<::Object::External*>(found);   // B.3.3 bridge
        if (!fn_IsDerivedFrom(it, profTid)) continue;
        if (static_cast<Profession*>(it)->isAvailable == 0) continue;  // +0x4e: not player-selectable

        // Growable push into +0x20 (buf) / +0x28 (count) / +0x2c (cap).
        profArray->SetSize(profArray->GetSize() + 1, it);
    }

    // Sort ascending by sort priority (Profession+0x48) — FUN_14001e7c0 semantics.
    int n = static_cast<int32_t>(self->arraySize);               // +0x28 count
    uintptr_t* arr = static_cast<uintptr_t*>(self->arrayData);  // +0x20
    if (n > 1 && arr) {
        for (int i = 1; i < n; i++) {
            uintptr_t key = arr[i];
            int keyPri = reinterpret_cast<const Profession*>(key)->field48;   // +0x48
            int j = i - 1;
            while (j >= 0 && reinterpret_cast<const Profession*>(arr[j])->field48 > keyPri) {
                arr[j + 1] = arr[j];
                j--;
            }
            arr[j + 1] = key;
        }
    }

#ifdef _DEBUG
    printf("[ProfessionList] Startup: %d professions in array (tid=0x%08x)\n",
           static_cast<int32_t>(self->arraySize), profTid);
#endif
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

DEFINE_EXE_VTABLE(ProfessionList, "_ZTV14ProfessionList")

extern "C" void RegisterProfessionListVtable() {
    // B38_RULES D5: nothing in the tree reads 0x14015ef50 back (swept by hex
    // and owner); this row is reader-less and serves the compiler table only.
    // The closing pass deletes it with the call.
    RegisterExeAddr(0x14015ef50, reinterpret_cast<uintptr_t>(ProfessionList::Vtable()));
    // ProfessionList::Startup (array populate + sort) — GameUpdateLoop::Startup
    // resolves exe 0x1400a90a0 (fn_ProfessionList_Startup) and calls it when the
    // profession list is live; unregistered it was NULL and the call was skipped,
    // leaving the profession array empty (TownHallUI blank-tab root cause).
    RegisterExeAddr(0x1400a90a0, reinterpret_cast<uintptr_t>(&ProfessionList_Startup_Impl));
}

DEFINE_EXE_TYPE(ProfessionList, "Object::External")
