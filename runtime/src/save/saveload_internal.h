#pragma once
// ============================================================================
// runtime/src/save/saveload_internal.h — helpers shared by the Save lane files
// ============================================================================
//
// WHY THIS FILE EXISTS. Wave 5 lane C put every one of these in an anonymous
// namespace inside `saveload.cpp`, which was correct while that was the only
// file in the directory. Wave 13 adds `saveload_update.cpp` (the five
// UpdateXxx state machines plus OnVideoRestart), and those bodies need the
// SAME `ZeroTime`, the SAME `ReleaseStringSlot` and the SAME singleton
// accessors.
//
// Copying them into the second file was the obvious move and is the wrong one.
// `ReleaseStringSlot` is a transcription of a refcount idiom whose two dead
// stores look like mistakes (see its comment); a second copy is a second place
// for someone to "fix" it, and the two copies would then disagree about the
// heap. One definition, included twice.
//
// Everything here is `inline`, and every accessor keeps the function-local
// `static` that Wave 5 introduced deliberately: RtOriginal/ResolveExport is
// GetProcAddress, an export-table binary search under the loader lock, and two
// uncached ones in a per-frame body cost Wave 2 five points of sim time while
// being invisible to both profilers
// (reference_rtoriginal_is_getprocaddress.md).
// ============================================================================

#include <cstdint>
#include <cstdlib>

#include "../../funcs/rt_port.h"
#include "../../types/save/SaveLoad.h"
#include "../../types/io/Stream.h"
#include "../system/string.h"
#include "../system/timer.h"

namespace RtSaveDetail {

using RtSave::SaveLoad;
using RtSave::SavedState;
using RtSave::SaveInfo;
using RtSave::ModInfo;
using RtIo::MemoryStream;
using RtSystem::String;

// ---- function pointer shapes ------------------------------------------------
using MemAllocFn         = void*   (*)(uint64_t size, int align);
using MemFreeFn          = void    (*)(void* p);
using GetDataDirectoryFn = String*  (*)(const void* fs, String* ret);
using ConcatFn           = String*  (*)(String* ret, const String* lhs, const String* rhs);
using AdjustDelimsFn     = void     (*)(String* self);
using MemStreamOpenFn    = bool     (*)(MemoryStream* self, int size);
using SaveLoadVoidFn     = void     (*)(SaveLoad* self);

// MemAlloc/MemFree go through the PORT registry, not the raw door: they are
// not oracles here, they are the allocation this lane performs, and a raw-door
// call is a call through NULL the moment the DLL is absent (WAVE 9 LANE V2).
inline void* MemAlloc(uint64_t size, int align) {
    return RT_EXPORT_ONCE(MemAllocFn, "?MemAlloc@MemoryAllocator@System@@SAPEAX_KH@Z")(size, align);
}
inline void MemFree(void* p) {
    RT_EXPORT_ONCE(MemFreeFn, "?MemFree@MemoryAllocator@System@@SAXPEAX@Z")(p);
}

// ---- DLL data symbols -------------------------------------------------------
// Every mangling below was verified present in the shipped export table with
// pefile before it was written down. Note Modification's `2PEAV12@EA` (a PUBLIC
// static member) against the others' `0PEAV12@EA` (private): spelling either
// one the other way produces a name the DLL does not contain, which resolves to
// NULL in silence (reference_resolveexport_phantom_names.md).
template <typename T>
inline T* RtDataExport(const char* mangled) {
    return reinterpret_cast<T*>(ResolveExport(mangled));
}

inline SaveLoad** SaveLoadSingletonSlot() {
    static SaveLoad** p = RtDataExport<SaveLoad*>("?_singleton@SaveLoad@Save@@0PEAV12@EA");
    return p;
}
inline void* FileSystemSingleton() {
    static void** p = RtDataExport<void*>("?_singleton@FileSystem@IO@@0PEAV12@EA");
    return p ? *p : nullptr;
}
inline uint8_t* ModificationSingleton() {
    static uint8_t** p = RtDataExport<uint8_t*>("?_singleton@Modification@IO@@2PEAV12@EA");
    return p ? *p : nullptr;
}
inline uint8_t* PersistenceSingleton() {
    static uint8_t** p = RtDataExport<uint8_t*>("?_singleton@Persistence@IO@@0PEAV12@EA");
    return p ? *p : nullptr;
}

inline double SecondsPerTick() { return RtSystem::TimerSecondsPerTick(); }

// `System::Time(0.0f)` as the two ctors inline it:
//     XORPS XMM0,XMM0 ; DIVSD XMM0,[Timer::_secondsPerTick] ; CVTTSD2SI RAX,XMM0
// Written out rather than folded to 0 because that is what the instructions
// say. Confirmed byte-for-byte at 0x1800c2e12..0x1800c2e2e (UpdateDelete) and
// at the identical tail of all four other UpdateXxx bodies.
inline int64_t ZeroTime() {
    const double q = 0.0 / SecondsPerTick();
    return static_cast<int64_t>(q);
}

// ---- the System::String refcount idiom, transcribed -------------------------
// Retail inlines this at ~200 sites. The shape is NOT the obvious one: when the
// refcount reaches zero it rewinds the member itself by 8 and frees THAT, and
// when it does not reach zero it stores 0 into the member. Both branches are
// dead stores at every call site that overwrites the member immediately
// afterwards — but they are what the bytes do, and this project has already
// lost a day to a String destructor applied to something that was not a String
// (reference_load_wild_write_high_dword_7fc.md, Wave 4 lane H).
inline void ReleaseStringSlot(const char** slot) {
    const char* p = *slot;
    if (p == nullptr) return;
    *reinterpret_cast<int*>(const_cast<char*>(p) - 4) -= 1;
    if (*reinterpret_cast<int*>(const_cast<char*>(p) - 4) == 0) {
        *slot = p - 8;
        MemFree(const_cast<char*>(*slot));
    } else {
        *slot = nullptr;
    }
}

// The ACQUIRE half, which Wave 5 never needed because none of its bodies
// re-pointed a String member. Four of Wave 13's do. Transcribed from
// UpdateDelete 0x1800c2dce..0x1800c2ddc: store first, then increment, and the
// whole sequence is skipped when the incoming pointer already equals the slot.
inline void RetainInto(const char** slot, const char* src) {
    *slot = src;
    if (src != nullptr) *reinterpret_cast<int*>(const_cast<char*>(src) - 4) += 1;
}

}  // namespace RtSaveDetail
