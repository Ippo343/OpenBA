#pragma once
// ============================================================================
// runtime/src/system/string.h — the System::String primitives filename.cpp
// shares with string.cpp.
// ============================================================================
// These are the inlined-everywhere idioms of the class (AddRef / Release /
// SetLength) plus the two ported entry points Filename reuses by pointer cast.
// Everything here is defined in string.cpp.
//
// ENDGAME B.3.1 step 1: THESE ARE ALL SHIMS NOW. The bodies moved onto
// `System::String` (runtime/types/system/String.h); every function declared
// below kept its exact signature — hidden return slot included — and is one
// line over the member on the same eight bytes, so no caller had to move.
// They die in step 2, each when its last caller is converted.
// ============================================================================

#include "../../types/system/String.h"

namespace RtSystem {

// The one registered System::MemoryAllocator, resolved once (NOT
// ResolveExport). Strings cross the DLL boundary, so both sides must use the
// same heap. Init happens lazily on first use.
void* RtMemAlloc(uint64_t size, int align);
void  RtMemFree(void* p);

// ?SetLength@String@System@@AEAAXH@Z  retail 0x1800c8220
void RtString_SetLength(String* self, int len);

// The release idiom inlined at ~200 sites in the DLL; the body of
// ?Release@String@System@@AEAAXXZ (0x1800c9870) and of ~String (0x180006c80).
void RtString_ReleaseInline(String* self);

// ?StrCompare@String@System@@SAHPEBD0@Z  retail 0x1800c98a0
int RtString_StrCompare(const char* a, const char* b);

// ?GetLength@String@System@@QEBAHXZ  retail 0x1800063c0
int RtString_GetLength(const String* self);

}  // namespace RtSystem
