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

// `if (src) { self->data = src; ++src[-4]; }` — the AddRef half of the copy
// ctor (0x180006cb0). Assumes self->data has already been zeroed/released.
void RtString_AdoptInline(String* self, char* src);

// ?StrCompare@String@System@@SAHPEBD0@Z  retail 0x1800c98a0
int RtString_StrCompare(const char* a, const char* b);

// ?GetLength@String@System@@QEBAHXZ  retail 0x1800063c0
int RtString_GetLength(const String* self);

// ---- ported entry points reused by Filename --------------------------------
void    RtSystem_String_Set(String* self, const char* src);                       // 0x1800c95d0
String* RtSystem_String_Assign(String* self, const String* rhs);                  // 0x1800c9810

// The two free operator+ overloads. Namespace-scope, so the hidden return slot
// is the FIRST argument (RCX=ret, RDX=lhs, R8=rhs).
// ??HSystem@@YA?AVString@0@AEBV10@0@Z     retail 0x1800c9440
// ??HSystem@@YA?AVString@0@AEBV10@PEBD@Z  retail 0x1800c92d0
String* RtSystem_String_ConcatCStr(String* ret, const String* lhs, const char* rhs);

// ??YString@System@@QEAA?AV01@PEBD@Z  retail 0x1800c9120 -- MEMBER operator+=,
// so `this` is RCX and the hidden return slot is RDX. The returned String is a
// COPY the caller normally discards; every caller must still release it, which
// is why the signature exposes it rather than hiding it.
// Declared here by Wave 9 lane EVM, which needs it for
// Video::Device::GetDLLForInterface. The body has been in string.cpp since
// Wave 2 and was simply never visible to another translation unit.
String* RtSystem_String_AppendAssignCStr(String* self, String* ret, const char* rhs);

}  // namespace RtSystem
