#pragma once
// ============================================================================
// runtime/src/system/widestring.h — System::WideString, Wave 9 lane FS
// ============================================================================
// WideString is System::String over wchar_t: ONE pointer, the SAME allocation
// header (int32 length in CHARACTERS at data-8, int32 refcount at data-4,
// characters at data+0), and the SAME release idiom including its asymmetry.
// Pinned by ?SetLength@WideString@System@@AEAAXH@Z (0x1800ca9b0):
//     MemAlloc(8 + (len+1)*2, 4); data = block+8; data[-8]=len; data[-4]=1
//
// Modelled as its own struct rather than a typedef of String so a caller
// cannot pass one where the other belongs; the two are cast into each other
// deliberately and only inside widestring.cpp.
// ============================================================================

#include <cstddef>
#include "../../types/system/String.h"

namespace RtSystem {

struct WideString {
    wchar_t* data;   // +0x00 — block + 8, or null for "no buffer"
};
static_assert(sizeof(WideString) == 8, "System::WideString must be 8 bytes");
static_assert(sizeof(wchar_t) == 2,
              "WideString is UTF-16: this port assumes a 2-byte wchar_t, which "
              "is what both MSVC and MinGW give on Windows");

// ?SetLength@WideString@System@@AEAAXH@Z          0x1800ca9b0  (PRIVATE: AEAA)
void        RtSystem_WideString_SetLength(WideString* self, int len);
// ?Set@WideString@System@@QEAAXPEB_W@Z            0x1800caf40
void        RtSystem_WideString_Set(WideString* self, const wchar_t* src);
// ?AsUTF8@WideString@System@@QEBA?AVString@2@XZ   0x1800cacd0
String*     RtSystem_WideString_AsUTF8(const WideString* self, String* ret);
// ??0WideString@System@@QEAA@AEBVString@1@@Z      0x1800cab50
WideString* RtSystem_WideString_CtorFromString(WideString* self, const String* src);
// ??0WideString@System@@QEAA@PEB_W@Z              0x18000a6e0
WideString* RtSystem_WideString_CtorFromWide(WideString* self, const wchar_t* src);
// ??1WideString@System@@QEAA@XZ                   0x18000a730
void        RtSystem_WideString_Dtor(WideString* self);
// ?StrLength@WideString@System@@SAHPEB_W@Z        0x1800cab20
int         RtSystem_WideString_StrLength(const wchar_t* s);

}  // namespace RtSystem
