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
// `static` that Wave 5 introduced deliberately: the raw DLL door/ResolveExport is
// GetProcAddress, an export-table binary search under the loader lock, and two
// uncached ones in a per-frame body cost Wave 2 five points of sim time while
// being invisible to both profilers
// (reference_rtoriginal_is_getprocaddress.md).
// ============================================================================

#include <atomic>
#include <cstdint>
#include <cstdlib>

#include "../../funcs/rt_port.h"
#include "../../types/save/SaveLoad.h"
#include "../../types/io/Stream.h"
#include "../../types/io/Persistence.h"   // SaveBlock — SetupLoadFn's return
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

// ?SetupLoad@SavedState@Save@@ — SELF FIRST, HIDDEN RETURN SLOT SECOND.
//
// It lives here, and not in whichever file happens to call it, because it now
// has THREE consumers: Wave 13's UpdateList / UpdateLoad / UpdateLoadFromFile
// call it as a dependency, and Wave 14's port of it in savedstate_setupload.cpp
// calls the shipped body through it as an oracle. Wave 13 wrote it correctly in
// an anonymous namespace inside saveload_update.cpp, where nothing else could
// reach it; a second spelling elsewhere is how the two later drift, and this
// particular shape is one where drift is a WILD WRITE rather than a compile
// error (reference_hidden_retslot_wild_write.md).
//
// The order is the MSVC member-function convention — `this` in RCX, the hidden
// return pointer in RDX — which is the OPPOSITE of the GCC/Itanium sret order
// we would get by writing the natural `SaveBlock SetupLoad(SavedState*)`. All
// three retail call sites say so register for register; 0x1800c25cc is the
// clearest: `LEA RDX,[RSP+0x28]` (slot) then `MOV RCX,RAX` (self).
using SetupLoadFn        = RtIo::SaveBlock* (*)(SavedState* self, RtIo::SaveBlock* ret);

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

// WAVE 15 LANE ORDER. These four were `static T** p = RtDataExport<T*>(...)`,
// a frozen static that keeps the FIRST answer including a non-null one. Three
// of the four names are Wave 15 rows and the fourth, `SaveLoad`, is PROMOTED to
// our own storage on every default run — so ResolveExport's answer for it
// genuinely changes across RtDataMintInit() and a first call during startup
// would have kept the DLL's `.data` slot forever while every other reader moved
// to the minted one. Two copies of one live mutable pointer is a SPLIT BRAIN,
// not a stale cache.
//
// `RT_DATA_SLOT` is the right door and it also keeps Wave 5's cost argument
// intact (a hit is still cached, so steady state is one load, not a
// GetProcAddress — reference_rtoriginal_is_getprocaddress.md). What made the
// conversion worth doing is NOT the macro itself: RT_DATA_SLOT froze a hit
// exactly as hard as these statics did until lane ORDER changed
// RtResolveCached to refuse to freeze a pre-init answer
// (rt_resolve_cache.cpp §6). Converting the shape is what puts these four
// under that rule, and under the [RtSlotAudit] instrument that can see them.
using SaveLoadSlot = SaveLoad**;
inline SaveLoad** SaveLoadSingletonSlot() {
    return reinterpret_cast<SaveLoadSlot>(&::Save::SaveLoad::_singleton);
}
inline void* FileSystemSingleton() {
    void** p = RT_DATA_SLOT(void**, "?_singleton@FileSystem@IO@@0PEAV12@EA");
    return p ? *p : nullptr;
}
inline uint8_t* ModificationSingleton() {
    uint8_t** p = RT_DATA_SLOT(uint8_t**, "?_singleton@Modification@IO@@2PEAV12@EA");
    return p ? *p : nullptr;
}
inline uint8_t* PersistenceSingleton() {
    uint8_t** p = RT_DATA_SLOT(uint8_t**, "?_singleton@Persistence@IO@@0PEAV12@EA");
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

// ENDGAME B.3.7 block B2. A by-value `System::String` a MEMBER returns
// (GetAutoSaveName / GenerateSaveName), landed in a `const char*` slot the
// unconverted body then manages by hand. Retail constructs the String straight
// into the caller's slot (the member's hidden retslot IS `&slot`), so the
// refcount is born at 1 and nothing is added or dropped on the way: this steals
// the prvalue's pointer and leaves the temporary null, whose destructor is then
// a no-op. The same refcount trace as retail's, with no AddRef/Release pair.
inline void AdoptReturned(const char** slot, System::String&& s) {
    *slot = s._data;
    s._data = nullptr;
}

// ---- RULE B6-5 / B7-7: namespace-scope atomic resolve slots ------------------
// A member of these classes can run on the LOADER thread (the ctor, via
// GameUpdateLoop's slot-6 Load) and on the main thread. A function-local
// `static Fn fn = Resolve…()` or a `static Fn fn; if (!fn) fn = …` first-call
// cache is exactly what B6-5 forbids in a member body. These are
// `element_common.h`'s `AtomicExport`/`AtomicAddr` shape after the B36_RULES
// 14.4 fix: cache only what `RtResolveCached` itself would cache (a pre-init hit
// is not frozen), and a by-address hit only once it is non-null.
inline void* AtomicExport(std::atomic<void*>& slot, const char* mangled) {
    void* p = slot.load(std::memory_order_acquire);
    if (p == nullptr) {
        void* scratch = nullptr;
        p = RtResolveCached(mangled, &scratch);
        if (scratch != nullptr) slot.store(scratch, std::memory_order_release);
    }
    return p;
}
// A template on the module enum so this header need not include
// standalone/resolve.h (and <windows.h>): each file already declares
// `enum class Module` / `ResolveAddr` itself, and the call below is found by
// argument-dependent lookup when the caller passes `Module::Runtime`.
template <class ModuleT>
inline void* AtomicAddr(std::atomic<void*>& slot, ModuleT module, uintptr_t ghidra) {
    void* p = slot.load(std::memory_order_acquire);
    if (p == nullptr) {
        p = reinterpret_cast<void*>(ResolveAddr(module, ghidra));
        if (p != nullptr) slot.store(p, std::memory_order_release);
    }
    return p;
}

// ---- the two unexported Array destructors saveload.cpp transcribes ----------
// Retail CALLs both by address from other Save bodies (UpdateList state 0 and
// ~SaveLoad call 0x1800329c0; ~SaveInfo calls 0x1800331f0). They stay
// registered at those addresses (saveload.cpp); the callers inside this
// directory call the transcriptions directly rather than through a door.
//   0x1800329c0  Collection::Array<SaveInfo>::~Array  (stride 0x58)
//   0x1800331f0  Collection::Array<ModInfo>::~Array   (stride 0x20)
void ArraySaveInfoDtor(Save::SaveLoad::SaveInfoArray* arr);
void ArrayModInfoDtor(Collection::Array<Save::SaveLoad::ModInfo>* arr);

}  // namespace RtSaveDetail
