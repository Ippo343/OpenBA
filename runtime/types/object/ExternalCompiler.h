#pragma once
// ============================================================================
// runtime/types/object/ExternalCompiler.h — ENDGAME B.3.9 OBJ (B39_RULES D6 W3)
//
// `Object::ExtensionCompiler` and `Object::ExternalCompiler`: the resource
// compiler plug-in interfaces. Retail builds NEITHER (both factory pairs are
// `XOR EAX,EAX; RET`); they exist in this tree because each has a reflection
// record in the type table -- `?_typeInfo@ExtensionCompiler@Object@@` @
// 0x1801f9ba8 and `?_typeInfo@ExternalCompiler@Object@@` @ 0x1801f9b70, name
// = the class, parent "Base", no extension -- and the fold (D6) retires the
// minted records in favour of each class's own DEFINE_TYPE
// (runtime/src/object/external_compilers.cpp).
//
// Read out of Runtime-x64.dll (their minted vtables were retired by the
// B.3.9 pilot, e2626f00, which also recorded the slot lists):
//   ??_7ExtensionCompiler@Object@@6B@ 0x180155a88, 4 slots:
//     [0] 0x18002c300 vector deleting dtor (elem 8, ~ExtensionCompiler)
//     [1] 0x18002c250 TypeName  [2] 0x18002c230 Type  [3] _purecall
//   ??_7ExternalCompiler@Object@@6B@  0x180155aa8, 5 slots:
//     [0] 0x18002c180 vector deleting dtor (elem 8, ~ExternalCompiler)
//     [1] 0x18002c0d0 TypeName  [2] 0x18002c0b0 Type  [3] [4] _purecall
//   ??0ExtensionCompiler 0x1800bca10 / ??0ExternalCompiler 0x1800bca50:
//     Base's vptr then the class's (the compiler's); no data.
//   ??1ExtensionCompiler 0x1800bcac0 / ??1ExternalCompiler 0x1800bca90:
//     restamp the class's vptr then Base's (the compiler's chain); no body.
//
// The pure slots have NO retail name (no override exists in the DLL to read
// one off), so they are named by position. No slot-0 override: nothing ever
// constructs or destroys one, and Base's DeletingDtor runs the same (empty)
// chain.
// ============================================================================

#include "TypeInfo.h"

namespace Object {

class ExtensionCompiler : public Base {
    DECLARE_TYPE(ExtensionCompiler)

public:
    virtual void Slot3() = 0;          // [3] _purecall, name not recoverable

protected:
    ExtensionCompiler() = default;     // 0x1800bca10
    ~ExtensionCompiler() = default;    // 0x1800bcac0
};
static_assert(sizeof(ExtensionCompiler) == 8, "ExtensionCompiler is one vtable pointer");

class ExternalCompiler : public Base {
    DECLARE_TYPE(ExternalCompiler)

public:
    virtual void Slot3() = 0;          // [3] _purecall, name not recoverable
    virtual void Slot4() = 0;          // [4] _purecall, name not recoverable

protected:
    ExternalCompiler() = default;      // 0x1800bca50
    ~ExternalCompiler() = default;     // 0x1800bca90
};
static_assert(sizeof(ExternalCompiler) == 8, "ExternalCompiler is one vtable pointer");

}  // namespace Object
