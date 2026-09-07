#pragma once
// UI::StringTable -- binary-compatible struct definition (PARTIAL).
// Runtime-x64.dll class. Full sizeof NOT proven this wave -- see note below.
//
// Decompiled from (Wave 7 lane F, 2026-09-07, Runtime-x64.dll, image base
// 0x180000000):
//   GetCount:          0x180002610 (?GetCount@StringTable@UI@@QEBAHXZ)
//   GetText(int):      0x180002550 (?GetText@StringTable@UI@@QEBA?AVString@System@@H@Z)
//   GetText(String&):  0x1800025b0 (?GetText@StringTable@UI@@QEBA?AVString@System@@AEBV34@@Z)
//   GetIndexFromName:  0x180002470 (?GetIndexFromName@StringTable@UI@@QEBAHAEBVString@System@@@Z)
//                       -- NOT in Wave 7 lane F's scope (not one of the 23
//                       functions this lane owns), but its body is read here
//                       because GetText(String&) and UI::String's by-name
//                       ctor both call it, and it pins the entry layout a
//                       second, independent way. This lane calls the DLL's
//                       OWN GetIndexFromName via RtOriginal<>() rather than
//                       porting it -- see sprite.cpp.
//
// VTABLE: `??_7StringTable@UI@@6B@` IS EXPORTED (retail 0x180154630). Not
// stamped by anything in this lane (StringTable's ctor is out of scope), but
// recorded here in case a later wave needs it.
//
// THE ONLY TWO FIELDS THIS LANE PROVES
// -------------------------------------
// GetCount is a one-line `return *(int*)(this+0x28)`. GetText(int) and
// GetIndexFromName both walk a stride-0x10 array at `*(void**)(this+0x20)`
// bounded by that same `+0x28` count -- the same {data, size, capacity}
// shape as Collection::Array<T> (runtime/types/collection/Collection.h),
// with T = TextEntry (0x10 bytes). Capacity (+0x2C, by the Array convention)
// is never read by any function in this lane's scope and is NOT asserted
// below.
//
// This header intentionally does NOT declare a `StringTable` struct with a
// `sizeof` -- nothing in Wave 7 lane F constructs or destroys one (the ctor
// isn't in scope), so there is no evidence for what comes after +0x28, and
// inventing a layout no read site needs is exactly the PLAN §4.3 risk 1
// trap ("getting a shared layout wrong silently corrupts every class that
// embeds it"). If a later wave needs the whole object, extend this file and
// add the static_asserts then.
//
// TextEntry (0x10 bytes per element), read by GetText(int) (0x180002550):
//     entry = data + index * 0x10
//     name  = *(char**)(entry + 0x00)   -- read by GetIndexFromName, not GetText
//     text  = *(char**)(entry + 0x08)   -- refcounted System::String data ptr;
//                                          GetText bumps its refcount before
//                                          returning it (StringLength/-Refcount
//                                          idiom, runtime/types/system/String.h)
//
// GetText(int) bounds-checks `index < 0` and `index >= count` and returns an
// empty (null-data) System::String on either; only the middle branch reads
// the array and bumps a refcount. GetText(String const&) is a two-line
// forwarder: `GetText(this, retval, GetIndexFromName(this, key))`.

#include <cstdint>
#include <cstddef>

struct StringTableTextEntry {
    char* name;   // +0x00  System::String data ptr (refcounted key, NULL = empty)
    char* text;   // +0x08  System::String data ptr (refcounted value, NULL = empty)
};
static_assert(sizeof(StringTableTextEntry) == 0x10, "StringTableTextEntry must be 0x10 bytes");
static_assert(offsetof(StringTableTextEntry, name) == 0x00, "name at +0x00");
static_assert(offsetof(StringTableTextEntry, text) == 0x08, "text at +0x08");

// Offsets into UI::StringTable this lane proves and uses. Not a struct --
// see the note above for why no full StringTable type is declared.
namespace StringTableOffsets {
    inline constexpr size_t kData  = 0x20;  // StringTableTextEntry* (Collection::Array._data)
    inline constexpr size_t kCount = 0x28;  // int32_t                (Collection::Array._size)
}
