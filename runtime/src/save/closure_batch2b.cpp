// ============================================================================
// runtime/src/save/closure_batch2b.cpp — cutover closure batch 2b
// ============================================================================
//
//   0x180032f70  Save_MetaData_CopyCtor         165 B  (unexported)
//   0x180032320  Save_SavedState_Delete          40 B  (unexported)
//
// The metadata record is the element type of the `Array<Save::MetaData>` that
// runtime/src/collection/array.cpp's `kSpec_MetaData58` (0x1800332d0) grows,
// and it is reached from IO::Persistence's UpdateList (0x1800c1ea0) /
// LoadMetaData (0x1800c1b30) / SaveMetaData (0x1800c1490).
// ============================================================================

#include <cstdint>
#include <new>          // placement new into the raw destination bytes

#include "../../funcs/rt_port.h"
#include "../../types/system/String.h"

enum class Module : int { Exe = 0, Runtime = 1, VideoDX11 = 2, COUNT = 3 };
uintptr_t ResolveAddr(Module mod, uintptr_t ghidra_addr);

namespace {

// `Array<Save::MetaDataEntry>::operator=` — retail 0x180033f30, the tail CALL at
// 0x180033048. Ported in this same commit
// (runtime/src/collection/array_destroy.cpp, J4_ENTRY(180033f30)); reached by
// ADDRESS because ResolveAddr is registry-first and the address is what
// tools/addr_door_audit.py can see.
void MetaEntryArrayAssign(void* dst, const void* src) {
    using Fn = void* (*)(void*, const void*);
    static Fn fn = nullptr;
    if (!fn) fn = reinterpret_cast<Fn>(ResolveAddr(Module::Runtime, 0x180033f30));
    fn(dst, src);
}

// The unexported `operator delete` COMDAT copy 0x180006a10 — the one
// runtime/src/video/context_lifecycle.cpp:702 already registers. Same body as
// ?MemFree@MemoryAllocator@System@@ and null-guarded inside.
void GuardedFree(void* p) {
    using Fn = void (*)(void*);
    static Fn fn = nullptr;
    if (!fn) fn = reinterpret_cast<Fn>(ResolveAddr(Module::Runtime, 0x180006a10));
    fn(p);
}

// `System::String`'s adopt-a-reference idiom, inlined by retail at both sites:
// zero the slot, then take the pointer and bump the refcount at `str-4`.
//
// ENDGAME B.3.1 step 2: that idiom IS ??0String@System@@QEAA@AEBV01@@Z
// (0x180006cb0) — zero, adopt, AddRef, in that order — so the copy constructor
// is spelled as itself here. `dst` is raw destination bytes inside a struct
// with no header of its own (see the banner), so it is placement-constructed;
// the two offsets stay raw because the field MEANINGS are still unknown.
inline void AdoptString(void* dst, const void* src) {
    new (dst) System::String(*static_cast<const System::String*>(src));
}

}  // namespace

// ============================================================================
// 0x180032f70  Save::MetaData's copy constructor — 0x58 bytes.
//
// Field by field, read out of the body:
//   +0x00  qword    verbatim
//   +0x08  qword    verbatim
//   +0x10  String*  zero, adopt, INC [str-4]
//   +0x18  qword    verbatim
//   +0x20  dword    verbatim
//   +0x24  dword    verbatim
//   +0x28  qword    verbatim
//   +0x30  qword    verbatim
//   +0x38  String*  zero, adopt, INC [str-4]
//   +0x40  dword    verbatim
//   +0x44  dword    verbatim
//   +0x48  qword    ZEROED  }  the nested Array<MetaDataEntry> header, zeroed
//   +0x50  qword    ZEROED  }  and THEN assigned through 0x180033f30 — the
//                              same zero-then-operator= order as everywhere
//                              else, because operator= destroys the
//                              destination's contents first.
//   +0x48..+0x57 is the whole 16-byte Array header; nothing else is written and
//   sizeof is 0x58, so there is no unwritten tail.
//
// NO STRUCT HEADER for this type, deliberately: the body is the only witness
// this batch has for the layout (the fields' MEANINGS are unknown — two of the
// nine are Strings and the rest are opaque), and a runtime/types/ header whose
// field names were guessed would be believed later. Raw offsets with the
// transcription above are honest about what is known.
// ============================================================================
extern "C" void* RtSave_MetaData_CopyCtor(void* self, const void* src) {
    uint8_t* d = static_cast<uint8_t*>(self);
    const uint8_t* s = static_cast<const uint8_t*>(src);

    *reinterpret_cast<uint64_t*>(d + 0x00) = *reinterpret_cast<const uint64_t*>(s + 0x00);
    *reinterpret_cast<uint64_t*>(d + 0x08) = *reinterpret_cast<const uint64_t*>(s + 0x08);
    AdoptString(d + 0x10, s + 0x10);
    *reinterpret_cast<uint64_t*>(d + 0x18) = *reinterpret_cast<const uint64_t*>(s + 0x18);
    *reinterpret_cast<uint32_t*>(d + 0x20) = *reinterpret_cast<const uint32_t*>(s + 0x20);
    *reinterpret_cast<uint32_t*>(d + 0x24) = *reinterpret_cast<const uint32_t*>(s + 0x24);
    *reinterpret_cast<uint64_t*>(d + 0x28) = *reinterpret_cast<const uint64_t*>(s + 0x28);
    *reinterpret_cast<uint64_t*>(d + 0x30) = *reinterpret_cast<const uint64_t*>(s + 0x30);
    AdoptString(d + 0x38, s + 0x38);
    *reinterpret_cast<uint32_t*>(d + 0x40) = *reinterpret_cast<const uint32_t*>(s + 0x40);
    *reinterpret_cast<uint32_t*>(d + 0x44) = *reinterpret_cast<const uint32_t*>(s + 0x44);
    *reinterpret_cast<uint64_t*>(d + 0x48) = 0;
    *reinterpret_cast<uint64_t*>(d + 0x50) = 0;
    MetaEntryArrayAssign(d + 0x48, s + 0x48);
    return self;
}

// ============================================================================
// 0x180032320  `delete Save::SavedState`
//
// Its one caller is `Save::SaveLoad::UpdateSave` (0x1800c2d0b), on
// `SaveLoad::_saveState` — runtime/types/save/SaveLoad.h already pins that
// member at +0x40 as a `SavedState*`, which is what identifies the type here.
// The call site tests the pointer itself (`TEST RCX,RCX / JZ`), so this body
// never sees null from its only caller.
//
//     if (this[1]) operator delete(this[1]);   // the heap buffer at +0x08
//     operator delete(this);
//     return this;
//
// Both frees go through 0x180006a10, which null-tests internally; the explicit
// test on +0x08 is RETAIL's, and it is kept as well. The object's own free is
// NOT guarded here for the same reason it is not there.
// ============================================================================
extern "C" void* RtSave_SavedState_Delete(void* self) {
    uint8_t* p = static_cast<uint8_t*>(self);
    if (void* buf = *reinterpret_cast<void**>(p + 8)) GuardedFree(buf);
    GuardedFree(self);
    return self;   // MOV RAX,<this> — retail returns the (now freed) pointer
}

int RtClosure2bSaveRegister() {
    int n = 0;
    RegisterRuntimeAddr(0x180032f70, (void*)&RtSave_MetaData_CopyCtor); ++n;
    RegisterRuntimeAddr(0x180032320, (void*)&RtSave_SavedState_Delete); ++n;
    return n;
}
