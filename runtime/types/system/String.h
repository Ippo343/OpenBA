#pragma once
// ============================================================================
// runtime/types/system/String.h — System::String binary layout + allocation
//                                 header (Wave 2, lane A)
// ============================================================================
//
// Every fact below was read out of Runtime-x64.dll in Ghidra (image base
// 0x180000000, decompiled 2026-09-06); the function that pins it is named.
//
// THE OBJECT
// ----------
// System::String is ONE POINTER. `?GetLength@String@System@@QEBAHXZ`
// (0x1800063c0) is the whole class in miniature:
//
//     if (*(int64*)this != 0) return *(int*)(*(int64*)this - 8);
//     return 0;
//
// so the object is `{ char* data; }` and the length lives in front of the
// character data, not in the object. Confirmed independently by the default
// ctor (0x180006760: `*(uint64*)this = 0` and nothing else) and by the copy
// ctor (0x180006cb0), which copies exactly one qword.
//
// THE ALLOCATION HEADER
// ---------------------
// `?SetLength@String@System@@AEAAXH@Z` (0x1800c8220) is the only allocator in
// the class and it pins the whole block:
//
//     block = MemoryAllocator::Allocate(_singleton, (len + 1) + 8, 4);
//     this->data = (int*)block + 2;      // data = block + 8
//     ((int*)block)[0] = len;            // data[-8] = length
//     ((int*)block)[1] = 1;              // data[-4] = refcount, born at 1
//
// So, with `p` = the allocator's user pointer:
//
//     p + 0 : int32 length      (character count, EXCLUDING the NUL)
//     p + 4 : int32 refcount
//     p + 8 : char data[length + 1]
//     String::data == p + 8
//
// This confirms the header comment in standalone/filename_runtime_hook.cpp
// ("refcount at buffer-4, user pointer at buffer-8") and adds the field it did
// not name: the dword at buffer-8 is the LENGTH, which is why a `dec`/`inc` on
// a freed String scribbles on the free-list link's high dword and not its low.
//
// SetLength does NOT write a terminator: every caller writes data[len] itself
// (Set via StrCopy, SubString explicitly, operator+ explicitly).
//
// RETAIL'S RELEASE IDIOM, AND WHY IT MUST BE COPIED EXACTLY
// --------------------------------------------------------
// The release sequence is inlined at ~200 sites in the DLL and is always:
//
//     if (data) {
//         if (--data[-4] == 0) { this->data = data - 8; MemFree(this->data); }
//         else                 { this->data = nullptr; }
//     }
//
// Note the asymmetry, which is NOT a decompiler artifact (see ~String
// 0x180006c80): on the FREEING branch the object is left holding
// `data - 8`, a dangling pointer, not null. On the sharing branch it is
// nulled. Ports must reproduce both, because ~String runs on stack temporaries
// whose bytes are then read by nothing — but Set() and operator= call the same
// sequence and then immediately overwrite `data`, and any port that "tidied"
// the freeing branch to null would still be correct there while diverging in
// the dtor. Reproduce it; do not tidy it.
//
// THE ALLOCATOR IS NOT OURS
// -------------------------
// Strings cross the DLL boundary constantly (the DLL makes them, we destroy
// them, and vice versa), so every allocation and free MUST go through
// Runtime-x64.dll's own System::MemoryAllocator, resolved with RtOriginal<>()
// — never ResolveExport, which would hand back a registered port. See
// runtime/src/system/string.cpp.
// ============================================================================

#include <cstdint>
#include <cstddef>

namespace RtSystem {

// ---- System::String --------------------------------------------------------
struct String {
    char* data;   // +0x00 — points at block+8; null == the empty string
};
static_assert(sizeof(String) == 8, "System::String must be 8 bytes");

// ---- the allocation header in front of `data` ------------------------------
// Laid out so that `reinterpret_cast<StringBlock*>(data - 8)` is exact.
struct StringBlock {
    int32_t length;    // -0x08 from data
    int32_t refCount;  // -0x04 from data
    char    chars[1];  // +0x00 = String::data
};
static_assert(offsetof(StringBlock, chars) == 8, "String chars @ block+8");

// Header accessors, spelled the way the DLL spells them.
inline int32_t& StringLength(char* data)   { return reinterpret_cast<int32_t*>(data)[-2]; }
inline int32_t& StringRefCount(char* data) { return reinterpret_cast<int32_t*>(data)[-1]; }
inline int32_t  StringLength(const char* data) {
    return reinterpret_cast<const int32_t*>(data)[-2];
}
inline int32_t  StringRefCount(const char* data) {
    return reinterpret_cast<const int32_t*>(data)[-1];
}

// The MemAlloc alignment argument SetLength passes (0x1800c8220).
constexpr int kStringAllocAlign = 4;

}  // namespace RtSystem
