// ============================================================================
// runtime/src/object/ext_compilers.cpp — VTABLE BATCH 9
//   Object::ExtensionCompiler + Object::ExternalCompiler, whole classes.
// ============================================================================
//
// Two adjacent, byte-identical-shaped stub classes: each is `Object::Base`
// plus nothing (sizeof == 0x08, a bare vtable pointer). Both were read in
// Runtime-x64.dll (image base 0x180000000, read 2026-09-12); the profile
// build (Runtime-x64-profile.dll) carries the same two names with no extra
// strings worth citing beyond what retail's own mangled names already give.
//
// ??_7ExtensionCompiler@Object@@6B@ (retail 0x180155a88), 4 slots:
//   0  deleting dtor  0x18002c300  deleting_dtor.cpp row (sizeof 0x08)
//   1  TypeName       0x18002c250  here
//   2  Type           0x18002c230  here
//   3  purecall trap  0x18007ec28  minted, not this batch's job
// plus ??1ExtensionCompiler@Object@@UEAA@XZ (0x1800bcac0), the row's dtorVa.
//
// ??_7ExternalCompiler@Object@@6B@ (retail 0x180155aa8), 5 slots:
//   0  deleting dtor  0x18002c180  deleting_dtor.cpp row (sizeof 0x08)
//   1  TypeName       0x18002c0d0  here
//   2  Type           0x18002c0b0  here
//   3,4 purecall traps 0x18007ec28  minted, not this batch's job
// plus ??1ExternalCompiler@Object@@UEAA@XZ (0x1800bca90), the row's dtorVa.
//
// BOTH DTORS ARE TWO-INSTRUCTION VTABLE RESTAMPS, transcribed straight from
// the disassembly (no decompile ambiguity possible with two `MOV`s):
//
//     ??1ExtensionCompiler@Object@@UEAA@XZ  0x1800bcac0
//         *(void**)this = &??_7ExtensionCompiler@Object@@6B@;   (own table --
//             dead store, immediately overwritten, transcribed anyway: it is
//             two bytes of shipped behaviour, not an inference)
//         *(void**)this = &??_7Base@Object@@6B@;
//     ??1ExternalCompiler@Object@@UEAA@XZ   0x1800bca90   -- identical shape,
//         its own table then Base's.
//
// Both classes' `Type`/`TypeName` are the ordinary two-field
// `?_typeInfo@…@Object@@0VBaseTypeInfo@2@A` read this tree already uses
// throughout (object_typeinfo_accessors.cpp, groupdescription.cpp, ...): +0x00
// is the uint32 type id, +0x08 is the C-string name pointer. Names verified
// against standalone/rt_data_mint_tables.inc's own generated rows (lines 4357
// and 4365), which is the same table the mint fills these vtables from.
//
// The deleting-dtor slot 0 (0x18002c300 / 0x18002c180) is NOT here: both are
// the vector deleting destructor, one shared body parameterized by
// (~Class, sizeof), and that family lives entirely in
// runtime/src/object/deleting_dtor.cpp (rows appended this batch).
// ============================================================================

#include <cstdint>
#include <cstring>

#include "../../funcs/rt_port.h"

namespace RtObject {

namespace {

void* VtBase() { return RT_DATA_SLOT(void*, "??_7Base@Object@@6B@"); }

const uint8_t* ExtensionCompilerTypeInfo() {
    return RT_DATA_SLOT(const uint8_t*,
                        "?_typeInfo@ExtensionCompiler@Object@@0VBaseTypeInfo@2@A");
}
const uint8_t* ExternalCompilerTypeInfo() {
    return RT_DATA_SLOT(const uint8_t*,
                        "?_typeInfo@ExternalCompiler@Object@@0VBaseTypeInfo@2@A");
}

}  // namespace

// ---- ??1ExtensionCompiler@Object@@UEAA@XZ   0x1800bcac0 --------------------
extern "C" void RtObject_ExtensionCompiler_Dtor(void* selfv) {
    void** v = reinterpret_cast<void**>(selfv);
    *v = VtBase();      // 0x1800bcac0: own table stamped then immediately
                        // overwritten by Base's -- both stores transcribed.
}
extern "C" uint32_t RtObject_ExtensionCompiler_Type(const void*) {
    uint32_t v = 0; std::memcpy(&v, ExtensionCompilerTypeInfo(), sizeof v); return v;
}
extern "C" const char* RtObject_ExtensionCompiler_TypeName(const void*) {
    const char* v = nullptr; std::memcpy(&v, ExtensionCompilerTypeInfo() + 8, sizeof v); return v;
}

// ---- ??1ExternalCompiler@Object@@UEAA@XZ    0x1800bca90 --------------------
extern "C" void RtObject_ExternalCompiler_Dtor(void* selfv) {
    void** v = reinterpret_cast<void**>(selfv);
    *v = VtBase();      // 0x1800bca90: same shape as ExtensionCompiler above.
}
extern "C" uint32_t RtObject_ExternalCompiler_Type(const void*) {
    uint32_t v = 0; std::memcpy(&v, ExternalCompilerTypeInfo(), sizeof v); return v;
}
extern "C" const char* RtObject_ExternalCompiler_TypeName(const void*) {
    const char* v = nullptr; std::memcpy(&v, ExternalCompilerTypeInfo() + 8, sizeof v); return v;
}

}  // namespace RtObject

// ============================================================================
// Registration. Both doors for every row.
// ============================================================================
int RtObjectExtCompilersRegister() {
    using namespace RtObject;
    int n = 0;

    RegisterRuntimeExport("?TypeName@ExtensionCompiler@Object@@UEBAPEBDXZ",
                          (void*)&RtObject_ExtensionCompiler_TypeName); ++n;
    RegisterRuntimeAddr(0x18002c250ull, (void*)&RtObject_ExtensionCompiler_TypeName);

    RegisterRuntimeExport("?Type@ExtensionCompiler@Object@@UEBAIXZ",
                          (void*)&RtObject_ExtensionCompiler_Type); ++n;
    RegisterRuntimeAddr(0x18002c230ull, (void*)&RtObject_ExtensionCompiler_Type);

    RegisterRuntimeExport("??1ExtensionCompiler@Object@@UEAA@XZ",
                          (void*)&RtObject_ExtensionCompiler_Dtor); ++n;
    RegisterRuntimeAddr(0x1800bcac0ull, (void*)&RtObject_ExtensionCompiler_Dtor);

    RegisterRuntimeExport("?TypeName@ExternalCompiler@Object@@UEBAPEBDXZ",
                          (void*)&RtObject_ExternalCompiler_TypeName); ++n;
    RegisterRuntimeAddr(0x18002c0d0ull, (void*)&RtObject_ExternalCompiler_TypeName);

    RegisterRuntimeExport("?Type@ExternalCompiler@Object@@UEBAIXZ",
                          (void*)&RtObject_ExternalCompiler_Type); ++n;
    RegisterRuntimeAddr(0x18002c0b0ull, (void*)&RtObject_ExternalCompiler_Type);

    RegisterRuntimeExport("??1ExternalCompiler@Object@@UEAA@XZ",
                          (void*)&RtObject_ExternalCompiler_Dtor); ++n;
    RegisterRuntimeAddr(0x1800bca90ull, (void*)&RtObject_ExternalCompiler_Dtor);

    return n;
}
