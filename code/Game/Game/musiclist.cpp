// code/Game/Game/musiclist.cpp
// Clean reimplementation of the MusicList resource type.
//
// MusicList (0x30 bytes) — the music-track playlist resource (Game\MusicTracks.rsc).
// Extends Object::External (0x20 base). Vtable @ 0x14015ee40.
// Registered (NullFactory stub) at exe 0x14014cc00 (MenuScene_StaticInit).
//
// It is loaded by External::Load when the in-game resource preload
// (UpdateLoop::LoadResources) reads Game\MusicTracks.rsc out of the resource
// tree. In standalone it had only a NullFactory stub, so ExternalFactory::
// FindOrLoad got NULL back and crashed in IsDerivedFrom(NULL) (Runtime+0xC039A).
// This file provides the real factory + faithful Serialize, mirroring the
// Toolbar / Scenario / NameDatabase factory-cascade pattern.
//
// Layout (from factory sub_c260, dtor 0x14001c360, Serialize 0x1400a3e90):
//   +0x00 vtable (0x14015ee40)
//   +0x08..+0x1F  Object::External base
//   +0x20 entries.data   (void*, dynamic array of 0x18-byte track entries)
//   +0x28 entries.count  (int)
// (The array header is the standard {data,count,cap} shape; the resize helper
//  at exe 0x14001c3c0 manages it. cap lives at +0x2c.)
//
// Each track entry (stride 0x18):
//   +0x00 ExternalPtr (8 bytes, ref to the track's audio resource)
//   +0x08 float   +0x0c float   +0x10 int   +0x14 int
//
// Serialize (vtable +0x28, exe 0x1400a3e90): serialize count int; if loading
// (serializer version word == 0) resize the array; then per element serialize
// float@+0x08, float@+0x0c, int@+0x10, int@+0x14, ExternalPtr@+0x00 (THIS ORDER —
// serialize order is load-bearing). The +0x00 field is serialized by exe
// FUN_1400019a0 = the templated Object::ExternalPtr::Serialize (NOT String::Serialize)
// — see the per-element note below. GetFileVersion (vtable +0x18, exe 0x1400a3f80)
// = 1 (verified: `mov eax,1; ret`).
//
// ENDGAME B.3.8 block LOOP: a REAL class (code/Game/Game/MusicList.h, field
// layout and retail slot map there) with a compiler-generated vtable. Gone: the
// hand-built 9-slot `g_MusicList_Vtable` (three null slots: TypeName, Compile,
// Destruct), the lazily-resolved resize pointer (a non-atomic first-call cache,
// B6-5) and the C entry points.

#include "Game/MusicList.h"
#include "object/External.h"
#include "object/TypeInfo.h"
#include "Game/exe_bodies.h"
#include "system/MemoryAllocator.h"   // System::MemoryAllocator::MemAlloc / MemFree
#include "resolve.h"
#include "collection/Collection.h"
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch B
#include "object/Internal.h" // Object::detail::InternalMemAlloc / InternalMemFree
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <new>

static constexpr size_t    kEntryStride = 0x18;

// ============================================================
// ENDGAME B.3.2 step 2 — the track-entry Array row
// ============================================================
// Retail's resize FUN_14001c3c0 (re-decompiled 2026-09-13) is `rtcoll::SetSizeCore`
// in its two-argument form over the 0x18-byte track entry:
//   assign = three qword copies (a pure byte copy of the whole stride);
//   init   = qword 0 @+0x00, dword 0 @+0x08, qword 0x3f800000 @+0x0c (i.e. float
//            1.0f @+0x0c AND int 0 @+0x10), dword 11 @+0x14 — every byte written;
//   destroy = none: the realloc path frees the old block without an element
//            step and there is no truncation step (the +0x00 ExternalPtr is a
//            bare pointer here, not refcounted).
// Used by the destructor below. The resize fallback further down is NOT
// converted to it yet — see its banner.
namespace {
void AssignMusicTrackEntry(uint8_t* d, const uint8_t* s) {
    *reinterpret_cast<uint64_t*>(d + 0x00) = *reinterpret_cast<const uint64_t*>(s + 0x00);
    *reinterpret_cast<uint64_t*>(d + 0x08) = *reinterpret_cast<const uint64_t*>(s + 0x08);
    *reinterpret_cast<uint64_t*>(d + 0x10) = *reinterpret_cast<const uint64_t*>(s + 0x10);
}
void InitMusicTrackEntry(uint8_t* d) {
    *reinterpret_cast<uint64_t*>(d + 0x00) = 0;              // ExternalPtr
    *reinterpret_cast<uint32_t*>(d + 0x08) = 0;              // float 0.0f
    *reinterpret_cast<uint64_t*>(d + 0x0c) = 0x3f800000ull;  // float 1.0f, int 0
    *reinterpret_cast<uint32_t*>(d + 0x14) = 11;
}
// stays explicit: `init` is a patterned fill (1.0f at +0x0c, 11 at +0x14), not
// the zero or as-found tail any element type's `T()` would write.
const rtcoll::ArraySpec kSpec_MusicTrackEntry = {       // exe 0x14001c3c0 (resize),
    "Array<MusicList::Track>", 0x18, 8,                 //     0x14001c360 (~Array)
    &AssignMusicTrackEntry, &InitMusicTrackEntry, nullptr, true,
    /*assign unwritten*/ 0, 0,
    /*init   unwritten*/ 0, 0,
    /*seed zero       */ 0, 0 };
using MusicTrackArray = Collection::Array<uint8_t, Collection::DefaultAllocator,
                                          Collection::ExplicitSpec<&kSpec_MusicTrackEntry>>;
}  // namespace

// Exe-internal array resize helper for the 0x18-byte track entries
// (0x14001c3c0). The exe-internal addr is not registered, so the faithful local
// resize below, with the same default-init, is what runs.
namespace { std::atomic<bool> s_resizeWarned{false}; }

// ============================================================
// Local resize fallback (standalone) — mirrors exe FUN_14001c3c0
// ============================================================
// Standard {data,count,cap} header at arrayHeader (= &self[+0x20]). Grows the
// 0x18-byte entry array and default-inits NEW elements to:
//   String@+0x00 = null, float@+0x08 = 0, float@+0x0c = 1.0f, int@+0x14 = 11.
// (int@+0x10 is left zero — the exe inits the {+0x08=0,+0x0c=1.0f,+0x14=11}
//  triple plus the null String; +0x10 falls out as zero from the alloc clear.)
//
// ENDGAME B.3.2 step 2: NOT converted to MusicTrackArray::SetSize, on purpose.
// The algorithm after the two guards below matches SetSizeCore arm for arm
// (compared against the re-decompiled 0x14001c3c0: early-out, size-before-cap,
// treble on single append, byte-copy prefix on a move, full-element init on the
// tail, free old on a move). What does not match is the site's two front
// guards, which have an observable effect retail does not have: a negative
// newCount prints once and leaves the array unchanged (retail stores the
// negative size and returns), and a missing allocator returns instead of
// faulting. BRIEF.md lists "an extra guard with observable effect" as a STOP.
// The swap is one line once those guards are ruled on:
//   reinterpret_cast<MusicTrackArray*>(arrayHeader)->SetSize(newCount);
static void MusicList_ResizeEntries_Fallback(void* arrayHeader, int newCount) {
    if (!arrayHeader) return;
    if (newCount < 0) {
        if (!s_resizeWarned.exchange(true)) {
            printf("[MusicList::Resize] negative newCount=%d header=%p — stream "
                   "misaligned upstream; leaving array unchanged\n",
                   newCount, arrayHeader);
            fflush(stdout);
        }
        return;
    }
    if (!System::MemoryAllocator::_singleton) return;

    auto* hdr = reinterpret_cast<uint8_t*>(arrayHeader);
    void**   dataSlot  = reinterpret_cast<void**>(hdr + 0x00);
    int32_t* countSlot = reinterpret_cast<int32_t*>(hdr + 0x08);
    int32_t* capSlot   = reinterpret_cast<int32_t*>(hdr + 0x0C);

    int oldCount = *countSlot;
    if (newCount == oldCount) return;

    void* oldData = *dataSlot;
    *countSlot = newCount;

    if (*capSlot < newCount) {
        *capSlot = newCount;
        if (newCount == oldCount + 1 && oldCount != 0) {
            *capSlot = newCount * 3;
        }
        size_t bytes = static_cast<size_t>(*capSlot) * kEntryStride;
        *dataSlot = System::MemoryAllocator::MemAlloc(bytes, 8);
    }

    int copyStart = (oldData == *dataSlot) ? oldCount : 0;
    auto* dst = reinterpret_cast<uint8_t*>(*dataSlot);

    for (int slot = copyStart; slot < newCount; ++slot) {
        uint8_t* elem = dst + static_cast<size_t>(slot) * kEntryStride;
        if (!elem) continue;
        if (slot < oldCount) {
            const uint8_t* src = reinterpret_cast<const uint8_t*>(oldData)
                               + static_cast<size_t>(slot) * kEntryStride;
            for (size_t i = 0; i < kEntryStride; ++i) elem[i] = src[i];
        } else {
            // Default-init a fresh entry (matches exe FUN_14001c3c0).
            for (size_t i = 0; i < kEntryStride; ++i) elem[i] = 0;
            *reinterpret_cast<void**>(elem + 0x00)  = nullptr;   // String
            *reinterpret_cast<float*>(elem + 0x08)  = 0.0f;
            *reinterpret_cast<float*>(elem + 0x0c)  = 1.0f;
            *reinterpret_cast<int32_t*>(elem + 0x14) = 11;
        }
    }

    if (oldData != *dataSlot && oldData != nullptr) {
        System::MemoryAllocator::MemFree(oldData);
    }
}

// ============================================================
// MusicList::MusicList — factory sub_c260 body
// ============================================================
// External() and the 0x14015ee40 stamp are the compiler's; then +0x20 = 0,
// +0x28 = 0 (two qwords: data, count + capacity).
MusicList::MusicList() {
    _entries       = nullptr;
    _entryCount    = 0;
    _entryCapacity = 0;
}

// ============================================================
// MusicList::Serialize (vtable +0x28) — faithful port of 0x1400a3e90
// ============================================================
bool MusicList::Serialize(void* stream) {
    alignas(8) uint8_t ser[24];
    ::new (static_cast<void*>(ser)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge

    int count = _entryCount;
    reinterpret_cast<IO::Serializer*>(ser)->Serialize(count, nullptr);   // B.3.4 bridge (cast dropped: count is int)

    // Loading when the serializer's stream-mode field == 0. The original reads
    // *(int*)(local_20[0] + 8), i.e. DEREFERENCE the serializer's first qword
    // (the stream/state ptr) then +8 — NOT serializer+8. (Canonical idiom, see
    // namedatabase.cpp.) Reading ser+8 directly mis-evaluates the load flag, so
    // the resize is skipped, entries.data stays null, and the per-element read
    // below faults (dst = elem+0x08 = 0x8 when elem == null).
    // FUN_14001c3c0 is unregistered; the resize is the local mirror above
    // (called directly; the old lazily-resolved pointer always fell back to it).
    void* serState = *(void**)ser;
    if (serState && static_cast<int>(reinterpret_cast<IO::Serializer*>(ser)->GetDirection()) == 0) {   // B.3.4 bridge  (IO::Serializer::GetDirection, retail 0x180006210)
        MusicList_ResizeEntries_Fallback(&_entries, count);   // &entries header
    }

    // Per-element read. Guard on a non-null buffer: if the array wasn't
    // allocated (e.g. count==0, or a resize that couldn't allocate), don't
    // deref null. With the load flag fixed this holds whenever count>0.
    uint8_t* elem = reinterpret_cast<uint8_t*>(_entries);   // entries.data
    if (count > 0 && elem) {
        for (int i = 0; i < count; ++i) {
            // ORDER IS LOAD-BEARING: float@+0x08, float@+0x0c, int@+0x10,
            // int@+0x14, then String@+0x00.
            reinterpret_cast<IO::Serializer*>(ser)->Serialize(*reinterpret_cast<float*>(elem + 0x08), nullptr);   // B.3.4 bridge
            reinterpret_cast<IO::Serializer*>(ser)->Serialize(*reinterpret_cast<float*>(elem + 0x0c), nullptr);   // B.3.4 bridge
            reinterpret_cast<IO::Serializer*>(ser)->Serialize(*reinterpret_cast<int*>(elem + 0x10), nullptr);   // B.3.4 bridge
            reinterpret_cast<IO::Serializer*>(ser)->Serialize(*reinterpret_cast<int*>(elem + 0x14), nullptr);   // B.3.4 bridge
            // +0x00 is an Object::External REFERENCE (the track's audio resource),
            // NOT a String. The original element serializer (exe FUN_1400019a0) is
            // the templated ExternalPtr::Serialize (encoding/ref-mode aware: reads a
            // uint refId in reference mode, else a bool + path String) — NOT
            // System::String::Serialize. Reimpl_ExternalPtr_Serialize_StringTable(0x14000bf40) is
            // the byte-identical instantiation (same one UIDescription's element
            // ptrs use). Reading +0x00 as a bare String consumed the wrong byte
            // count and drifted the whole element loop past EOF.
            Reimpl_ExternalPtr_Serialize_StringTable(elem + 0x00, ser);
            elem += kEntryStride;
        }
    }

    reinterpret_cast<IO::Serializer*>(ser)->~Serializer();   // B.3.4 bridge
    return true;
}

// ============================================================
// MusicList::~MusicList / DeletingDtor (vtable +0x00, exe 0x14001c360)
// ============================================================
// ENDGAME B.3.2 step 2: the array free is `MusicTrackArray::~Array`
// (re-decompiled 2026-09-13: MemFree(+0x20); +0x20 = 0; one qword over +0x28 —
// ArrayDtorImpl exactly; the row above has no destroy step, matching retail's
// absence of an element loop). External::~External is the compiler's chain.
MusicList::~MusicList() {
    reinterpret_cast<MusicTrackArray*>(&_entries)->~Array();   // B.3.2 bridge: entries header
}

// ============================================================
// MusicList::Reconstruct (vtable +0x30, exe 0x14001c310) — dtor(0) then re-init
// ============================================================
Object::External* MusicList::Reconstruct() {
    this->~MusicList();
    return ::new (static_cast<void*>(this)) MusicList();
}

// [7] 0x140001640 — the shared body `MOV RAX,[RCX]; XOR EDX,EDX; JMP [RAX]`:
// slot 0 with flags 0 (destroy, no free).
void MusicList::Destruct() { this->~MusicList(); }

// ============================================================
// MusicList::Construct (vtable +0x40, exe 0x14001c2c0) — in-place init
// ============================================================
Object::External* MusicList::Construct() {
    return ::new (static_cast<void*>(this)) MusicList();
}

// ============================================================
// Reflection: create / createInPlace, the registration
// ============================================================
void* MusicList::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(MusicList), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(::new (p) MusicList());
}
void* MusicList::CreateInPlaceObject(void* memory) {
    if (!memory) return nullptr;
    return static_cast<Object::Base*>(::new (memory) MusicList());
}


DEFINE_EXE_TYPE(MusicList, "Object::External")
