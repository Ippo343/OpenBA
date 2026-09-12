// ============================================================================
// runtime/src/ui/groupdescription.cpp — UI::GroupDescription, the whole class
// ============================================================================
//
// WHY THIS CLASS, AND WHY NOW. The 2026-09-12 cutover boot (make cutover, DLL
// absent) stopped here:
//
//   [RtMint] FATAL: call through a NOT-YET-PORTED minted vtable slot
//   [RtMint]   vtable : ??_7GroupDescription@UI@@6B@
//   [RtMint]   slot   : 2
//   [RtMint]   target : ?Type@GroupDescription@UI@@UEBAIXZ
//
// It is the first UI ElementDescription subclass the dialog load path
// dispatches on, and slot 2 is only where it happened to land first — the whole
// table was traps. So the unit ported here is the CLASS, not the slot.
//
// ??_7GroupDescription@UI@@6B@ (retail 0x180157190), ten slots:
//
//   0  deleting dtor       0x180046af0  runtime/src/object/deleting_dtor.cpp row
//   1  TypeName            0x180046660  here
//   2  Type                0x180046640  here
//   3  GetFileVersion      0x180117cf0  here  (returns 0x104)
//   4  External::Compile   0x1800066e0  already ported
//   5  Serialize           0x180117d00  here
//   6  Reconstruct         0x1800468e0  here
//   7  Destruct            0x180046a20  here
//   8  CreateInstance      0x180117dc0  here
//   9  Construct           0x180046980  here
//
// plus ??1GroupDescription@UI@@UEAA@XZ (0x180117c00) and
// ??3GroupDescription@UI@@SAXPEAX@Z (0x1800466f0), the class operator delete.
//
// NOT HERE, and deliberately: `?CreateNewObject@GroupDescription@UI@@`
// (0x180046750) and `?CreateInPlaceObject@` (0x180046840) are ALREADY ported
// and registered, table-driven, by runtime/src/ui/factory_ui1.cpp (the
// kGroupDescription row in factory_ui1_tables.inc, whose 19 stores are the same
// inlined constructor `Construct` below transcribes). Adding a second copy here
// would give the same two addresses two owners.
//
// Layout: runtime/types/ui/GroupDescription.h. `ElementDescription` + one
// `Array<ExternalPtr<ElementDescription>>` at +0x68; sizeof 0x78.
//
// ----------------------------------------------------------------------------
// THE CONSTRUCTOR IS INLINED EVERYWHERE AND THERE IS NO OUT-OF-LINE ??0.
// ----------------------------------------------------------------------------
// `Construct` (0x180046980), `Reconstruct` (0x1800468e0) and both factory steps
// each open-code the same 19 stores; the DLL exports no `??0GroupDescription`.
// `ConstructBody` below is that sequence written once, transcribed from
// 0x180046996..0x180046a06. Two details Ghidra's C loses:
//   * `+0x40` is written as ONE QWORD holding 3 (`MOV qword [RCX+0x40],3` at
//     0x1800469e0), so it covers `_elementType` AND `_flags` (+0x44). Writing
//     the two fields separately is byte-equivalent and is what is done here.
//   * `+0x38` and `+0x3c` are two SEPARATE DWORDS (0 and -1), because the
//     values differ. Not one qword.
// The three dead base-vtable stamps (Base, then External over it unread, then
// ElementDescription) are reproduced: they are four instructions in the shipped
// bytes and collapsing them would be a divergence for no gain, exactly as
// runtime/src/ui/dialog_ctor_dtor.cpp keeps Dialog's two.
// ============================================================================
#include <cstddef>
#include <cstdint>

#include "../../funcs/rt_port.h"
#include "../../types/ui/GroupDescription.h"
#include "../../types/ui/Element.h"

// ResolveAddr, forward-declared rather than pulled in from standalone/resolve.h
// — resolve.h drags in <windows.h> and PLAN.md §4.1 keeps it out of
// runtime/src/. This declaration must stay byte-identical to resolve.h's; the
// linker checks the two.
enum class Module : int { Exe = 0, Runtime = 1, VideoDX11 = 2, COUNT = 3 };
uintptr_t ResolveAddr(Module mod, uintptr_t ghidra_addr);

namespace {

using RtUI::GroupDescription;

// ---- the four exported vtables and the type-info static --------------------
// RT_DATA_SLOT caches a HIT only, so a lookup that runs before the mint has
// published the symbol is retried rather than frozen as null.
void* VtBase()               { return RT_DATA_SLOT(void*, "??_7Base@Object@@6B@"); }
void* VtExternal()           { return RT_DATA_SLOT(void*, "??_7External@Object@@6B@"); }
void* VtElementDescription() { return RT_DATA_SLOT(void*, "??_7ElementDescription@UI@@6B@"); }
void* VtGroupDescription()   { return RT_DATA_SLOT(void*, RtUI::kVtblGroupDescription); }
void* VtGroupElement()       { return RT_DATA_SLOT(void*, RtUI::kVtblGroupElement); }
const uint8_t* GroupDescriptionTypeInfo() {
    return RT_DATA_SLOT(const uint8_t*,
                        "?_typeInfo@GroupDescription@UI@@0VBaseTypeInfo@Object@@A");
}

template <typename Fn>
Fn RtAddr(uintptr_t ghidra) { return reinterpret_cast<Fn>(ResolveAddr(Module::Runtime, ghidra)); }

// ---- the IO::Serializer pair, the same shape every ported Serialize builds --
// [RSP+0x28] = the Stream*, [RSP+0x30] = the ReaderWriter the encoding factory
// returns, and `LEA RDX,[RSP+0x28]` is what every callee receives. Keeping the
// PAIR on the stack is load-bearing: a first draft of
// runtime/src/graphics/wave24_lane_gfxmat.cpp passed the address of a lone
// Stream* and every callee then read its reader from whatever slot happened to
// sit next to it.
struct Stream {
    uint8_t _head00[0x08];
    int32_t mode;              // +0x08  0 == reading
    int32_t _pad0c;
    void*   encodingFactory;   // +0x10
};
struct Serializer {
    Stream* stream;            // +0x00
    void*   readerWriter;      // +0x08
};
static_assert(offsetof(Stream, mode) == 0x08, "IO::Stream mode @ +0x08");
static_assert(offsetof(Stream, encodingFactory) == 0x10, "IO::Stream factory @ +0x10");
static_assert(offsetof(Serializer, readerWriter) == 0x08, "IO::Serializer rw @ +0x08");

// RETAIL vtable offsets. The profile build's Encoding/ReaderWriter tables
// disagree and must NOT be used to check these: its NAMES win, its OFFSETS do
// not (the documented retail-vs-profile trap; SerializeInt is +0xa0 retail,
// +0xb8 profile).
constexpr int kEncCreateReader = 0x18;   // 0x180117d42  CALL [RAX+0x18]
constexpr int kEncCreateWriter = 0x20;   // 0x180117d47  CALL [RAX+0x20]
constexpr int kRwScalarInt     = 0xa0;   // 0x180117d66  CALL [RAX+0xa0]
constexpr int kRwDeletingDtor  = 0x00;   // 0x180117da9  CALL [RAX]

void** VTableOf(void* o) { return *reinterpret_cast<void***>(o); }

}  // namespace

// ============================================================================
// The inlined constructor, transcribed once.
// ============================================================================
namespace {
void ConstructBody(GroupDescription* self) {
    uint8_t* p = reinterpret_cast<uint8_t*>(self);
    void**   v = reinterpret_cast<void**>(p);

    *v = VtBase();                                              // 180046996
    *v = VtExternal();                                          // 1800469a0
    // `LEA RAX,[RCX+8]` then `MOV [RAX],RDX` -- retail writes +0x08 through an
    // intermediate pointer (the SEH frame's visible temporary) and +0x10/+0x18
    // directly. Byte-identical; the temporary costs nothing to drop.
    *reinterpret_cast<uint64_t*>(p + 0x08) = 0;                 // 1800469b5
    *reinterpret_cast<uint64_t*>(p + 0x10) = 0;                 // 1800469b8
    *reinterpret_cast<uint64_t*>(p + 0x18) = 0;                 // 1800469bc
    *v = VtElementDescription();                                // 1800469c0
    *reinterpret_cast<uint64_t*>(p + 0x20) = 0;                 // 1800469ca
    *reinterpret_cast<uint64_t*>(p + 0x28) = 0;                 // 1800469ce
    *reinterpret_cast<uint64_t*>(p + 0x30) = 0;                 // 1800469d2
    *reinterpret_cast<int32_t*>(p + 0x38)  = 0;                 // 1800469d6  DWORD
    *reinterpret_cast<int32_t*>(p + 0x3c)  = -1;                // 1800469d9  DWORD
    *reinterpret_cast<uint64_t*>(p + 0x40) = 3;                 // 1800469e0  ONE QWORD
    *reinterpret_cast<uint64_t*>(p + 0x48) = 0;                 // 1800469e8
    *reinterpret_cast<uint64_t*>(p + 0x50) = 0;                 // 1800469ec
    *reinterpret_cast<uint64_t*>(p + 0x58) = 0;                 // 1800469f0
    *reinterpret_cast<uint64_t*>(p + 0x60) = 0;                 // 1800469f4
    *v = VtGroupDescription();                                  // 1800469f8
    *reinterpret_cast<uint64_t*>(p + 0x68) = 0;                 // 180046a02
    *reinterpret_cast<uint64_t*>(p + 0x70) = 0;                 // 180046a06  _size+_capacity
}
}  // namespace

// ============================================================================
// The trivial slots
// ============================================================================
// `Type` returns the FIRST DWORD of the BaseTypeInfo static and `TypeName` the
// pointer at +0x08 — retail reads them as two separate globals
// (`?_typeInfo@GroupDescription@UI@@` and the label Ghidra prints as
// `PTR_s_GroupDescription_1801faba8`, which is that static + 8), which is the
// same two fields runtime/types/object/TypeInfo.h pins.
extern "C" uint32_t RtUi_GroupDescription_Type(const void*) {
    const uint8_t* ti = GroupDescriptionTypeInfo();
    uint32_t v = 0; __builtin_memcpy(&v, ti, sizeof v); return v;
}
extern "C" const char* RtUi_GroupDescription_TypeName(const void*) {
    const uint8_t* ti = GroupDescriptionTypeInfo();
    const char* v = nullptr; __builtin_memcpy(&v, ti + 8, sizeof v); return v;
}
// 0x180117cf0: `MOV EAX,0x104 ; RET`. 0x104 == 260, the dialog resource file
// version this class understands; it is NOT ElementDescription's (that one
// returns a different constant from its own body).
extern "C" int32_t RtUi_GroupDescription_GetFileVersion(const void*) { return 0x104; }

// ============================================================================
// Construct / Reconstruct / Destruct
// ============================================================================
// `Construct` is the ctor with a null test; `Reconstruct` is `slot0(this, 0)`
// — the DELETING DESTRUCTOR with flag 0, i.e. destroy WITHOUT freeing — and
// then the ctor again, with NO null test (retail derefs `this` to reach the
// vtable before it could test it, so a null here faults in retail too and a
// guard would skip the behaviour rather than restore it).
extern "C" void* RtUi_GroupDescription_Construct(void* selfv) {
    if (selfv == nullptr) return nullptr;                      // 180046991
    ConstructBody(static_cast<GroupDescription*>(selfv));
    return selfv;
}

namespace {
using Slot0Fn = void* (*)(void* self, unsigned flag);
void CallSlot0(void* self, unsigned flag) {
    reinterpret_cast<Slot0Fn>(VTableOf(self)[0])(self, flag);
}
}  // namespace

extern "C" void* RtUi_GroupDescription_Reconstruct(void* selfv) {
    CallSlot0(selfv, 0);                                       // 1800468f7
    ConstructBody(static_cast<GroupDescription*>(selfv));
    return selfv;
}
// 0x180046a20 is a TAIL JUMP into slot 0 with flag 0 (`JMP [RAX]`), which is
// why Ghidra prints the "could not recover jumptable" warning. Behaviourally a
// call plus return.
extern "C" void RtUi_GroupDescription_Destruct(void* selfv) {
    CallSlot0(selfv, 0);
}

// ============================================================================
// ??1GroupDescription@UI@@UEAA@XZ   0x180117c00
// ============================================================================
// Re-stamp own vtable, destroy the child array, re-stamp the BASE vtable, then
// release `ElementDescription::_textName` at +0x60 and TAIL-JUMP to
// `??1External@Object@@` (0x1800bc950 — `JMP`, not `CALL`, at 0x180117c66).
//
// THE STRING RELEASE KEEPS RETAIL'S ASYMMETRY, which is the same one
// dialog_ctor_dtor.cpp documents for `??1Dialog@UI@@`: when the refcount
// reaches zero retail rewrites THE FIELD to `data - 8` and frees THAT (so the
// field is left holding a freed pointer); when it does not reach zero it NULLS
// the field. Normalising the two arms would be a divergence.
extern "C" void RtUi_GroupDescription_Dtor(void* selfv) {
    uint8_t* p = reinterpret_cast<uint8_t*>(selfv);
    void**   v = reinterpret_cast<void**>(p);

    *v = VtGroupDescription();                                 // 180117c1d

    using ArrayDtorFn = void* (*)(void*);
    RtAddr<ArrayDtorFn>(0x180039440ull)(p + 0x68);             // 180117c24  Array::~Array (J1)

    *v = VtElementDescription();                               // 180117c31

    char*& s = *reinterpret_cast<char**>(p + 0x60);
    if (s != nullptr) {                                        // 180117c38
        --*reinterpret_cast<int32_t*>(s - 4);                  // 180117c3d
        if (*reinterpret_cast<int32_t*>(s - 4) == 0) {         // 180117c40
            s = s - 8;                                         // 180117c46  the FIELD moves
            using MemFreeFn = void (*)(void*);
            RT_EXPORT_ONCE(MemFreeFn, "?MemFree@MemoryAllocator@System@@SAXPEAX@Z")(s);
        } else {
            s = nullptr;                                       // 180117c56
        }
    }

    using ExtDtorFn = void (*)(void*);
    RT_EXPORT_ONCE(ExtDtorFn, "??1External@Object@@UEAA@XZ")(p);   // 180117c66 JMP
}

// ============================================================================
// ?Serialize@GroupDescription@UI@@UEAA_NAEAVStream@IO@@@Z   0x180117d00
// ============================================================================
//   ElementDescription::Serialize(this, stream)            0x180116b00, ported
//   pair = { stream, nullptr }
//   rw   = stream->mode == 0 ? factory->vt[0x18](factory,&pair,0)
//                            : factory->vt[0x20](factory,&pair,0)
//   n    = _children._size
//   rw->vt[0xa0](rw, &n, 0)
//   if (pair.stream->mode == 0) SetSize(&_children, n)      0x1800492d0, ported
//   ElementDescriptionPtrArray_Serialize(&pair, n, _children._data)
//                                                          0x1800493d0, ported
//   if (rw) rw->vt[0](rw, 1)
//   return true
//
// THE MODE IS RE-READ FROM THE PAIR'S COPY, not from the argument: the
// disassembly is `MOV RAX,[RSP+0x28] ; CMP dword [RAX+8],0` at
// 0x180117d6c..0x180117d71, one deref deeper than the first test at 0x180117d3c
// which reads `[RBX+8]` directly. The two are the same Stream today; the
// re-read is transcribed rather than folded because folding it would assert
// they must be.
extern "C" bool RtUi_GroupDescription_Serialize(void* selfv, void* streamv) {
    uint8_t* p      = reinterpret_cast<uint8_t*>(selfv);
    Stream*  stream = static_cast<Stream*>(streamv);

    using BaseSerFn = void (*)(void*, void*);
    RtAddr<BaseSerFn>(0x180116b00ull)(p, stream);              // 180117d1a

    Serializer ser = { stream, nullptr };                      // [RSP+0x28], [RSP+0x30]

    void* factory = stream->encodingFactory;
    void** fvt    = VTableOf(factory);
    const int which = (stream->mode == 0) ? kEncCreateReader : kEncCreateWriter;
    using MakeRwFn = void* (*)(void* factory, Serializer* ser, int zero);
    void* rw = reinterpret_cast<MakeRwFn>(fvt[which / 8])(factory, &ser, 0);
    ser.readerWriter = rw;                                     // 180117d4a

    int32_t n = *reinterpret_cast<int32_t*>(p + 0x70);         // 180117d4f
    using ScalarFn = void (*)(void* rw, void* value, const char* name);
    reinterpret_cast<ScalarFn>(VTableOf(rw)[kRwScalarInt / 8])(rw, &n, nullptr);

    if (ser.stream->mode == 0) {                               // 180117d6c (re-read)
        using SetSizeFn = void (*)(void* array, int n);
        RtAddr<SetSizeFn>(0x1800492d0ull)(p + 0x68, n);        // 180117d7f
    }

    using ElemSerFn = void (*)(Serializer* ser, int count, void* data);
    RtAddr<ElemSerFn>(0x1800493d0ull)(&ser, n, *reinterpret_cast<void**>(p + 0x68));

    if (rw != nullptr) {                                       // 180117d9c
        using DeleteFn = void (*)(void* rw, int flag);
        reinterpret_cast<DeleteFn>(VTableOf(rw)[kRwDeletingDtor / 8])(rw, 1);
    }
    return true;                                               // MOV AL,1
}

// ============================================================================
// ?CreateInstance@GroupDescription@UI@@UEBA?AV?$InternalPtr@VElement@UI@@@Object@@PEBVDialog@2@@Z
// retail 0x180117dc0
// ============================================================================
// *** THE HIDDEN RETURN SLOT. *** This returns `Object::InternalPtr<UI::Element>`
// BY VALUE, so the Microsoft x64 ABI inserts a slot pointer as the FIRST
// argument after `this`: RCX = this, RDX = the return slot, R8 = the Dialog*.
// Ghidra prints it as `CreateInstance(GroupDescription*, Dialog*)` and DROPS
// the third register entirely — the exact shape of the hidden-retslot trap this
// project has paid for before. The disassembly is unambiguous: `MOV R15,R8`
// (the Dialog, used only as CreateElement's `this`), `MOV R14,RDX` (written
// through at 0x180117ed1), `MOV RSI,RCX`.
//
// ONE STACK SLOT DOES TWO JOBS, and it must stay one variable here. Retail's
// [RSP+0x50] holds the new GroupElement when it is handed to
// `ElementDescription::CreateInstance` (which applies the description to it),
// and is then REUSED as `Dialog::CreateElement`'s return slot inside the loop.
// Splitting it into two locals would be tidier and would stop being retail.
//
// The parent-chain walk at the end of each iteration is retail's: set
// `kElementFlagNeedsSizeCalc` (0x08000000) on the group and on every ancestor
// through `Element::_parent` (+0x98), until the chain ends.
extern "C" void** RtUi_GroupDescription_CreateInstance(const void* selfv,
                                                       void** ret,
                                                       const void* dialog) {
    const uint8_t* self = reinterpret_cast<const uint8_t*>(selfv);

    using MemAllocFn = void* (*)(uint64_t size, int align);
    // The locked wrapper, which is the Enter/Allocate/Leave triple retail
    // inlines at 0x180117deb..0x180117e25 (reference: the identical
    // identification in runtime/src/io/encoding.cpp and dialog_factory.cpp).
    void* raw = RtAddr<MemAllocFn>(0x1800c7a50ull)(0xc0, 8);

    uint8_t* elem = nullptr;
    if (raw != nullptr) {                                      // 180117e32
        using ElementCtorFn = void* (*)(void*);
        RtAddr<ElementCtorFn>(0x180115e50ull)(raw);            // 180117e3a ??0Element
        *reinterpret_cast<void**>(raw) = VtGroupElement();     // 180117e47
        elem = static_cast<uint8_t*>(raw);
    }

    void* slot = elem;                                         // 180117e4f [RSP+0x50]

    using EdCreateInstanceFn = void (*)(const void* self, void** elemPtr);
    RtAddr<EdCreateInstanceFn>(0x180115ad0ull)(self, &slot);   // 180117e5c

    const int32_t count = *reinterpret_cast<const int32_t*>(self + 0x70);
    for (int32_t i = 0; i < count; ++i) {                      // 180117e61 / 180117ec9
        void* const* descArray = *reinterpret_cast<void* const* const*>(self + 0x68);
        void*  desc      = descArray[i];                       // 180117e77

        using CreateElementFn = void** (*)(const void* dlg, void** ret, void* desc);
        void** made = RtAddr<CreateElementFn>(0x180112ed0ull)(dialog, &slot, desc);

        uint8_t* child = *reinterpret_cast<uint8_t**>(made);   // 180117e88
        if (child != nullptr)
            *reinterpret_cast<void**>(child + 0x98) = elem;    // 180117e90  Element::_parent

        // Array<Element*>::SetSize(&elem->_children, size + 1, made) — the
        // three-argument (fill) form, ported as array.cpp's kSpec_Fill8Z.
        using PushFn = void (*)(void* array, int n, const void* fill);
        RtAddr<PushFn>(0x180076900ull)(elem + 0xa0,
                                       *reinterpret_cast<int32_t*>(elem + 0xa8) + 1,
                                       made);                  // 180117ea9

        for (uint8_t* n = elem; n != nullptr;                   // 180117eb1..180117ec5
             n = *reinterpret_cast<uint8_t**>(n + 0x98)) {
            *reinterpret_cast<uint32_t*>(n + 0x34) |= UI::kElementFlagNeedsSizeCalc;
        }
    }

    *ret = elem;                                               // 180117ed1
    return ret;                                                // MOV RAX,R14
}

// ============================================================================
// Registration. Both doors for every row: the mangled name (which is what the
// minted BaseTypeInfo and the minted vtable ask for first) and the address
// (which is what a ported caller's `ResolveAddr` literal asks for).
// ============================================================================
int RtUiGroupDescriptionRegister() {
    int n = 0;
    RegisterRuntimeExport("?Type@GroupDescription@UI@@UEBAIXZ",
                          (void*)&RtUi_GroupDescription_Type);            ++n;
    RegisterRuntimeAddr(0x180046640ull, (void*)&RtUi_GroupDescription_Type);

    RegisterRuntimeExport("?TypeName@GroupDescription@UI@@UEBAPEBDXZ",
                          (void*)&RtUi_GroupDescription_TypeName);        ++n;
    RegisterRuntimeAddr(0x180046660ull, (void*)&RtUi_GroupDescription_TypeName);

    RegisterRuntimeExport("?GetFileVersion@GroupDescription@UI@@UEBAHXZ",
                          (void*)&RtUi_GroupDescription_GetFileVersion);  ++n;
    RegisterRuntimeAddr(0x180117cf0ull, (void*)&RtUi_GroupDescription_GetFileVersion);

    RegisterRuntimeExport("?Construct@GroupDescription@UI@@UEAAPEAVExternal@Object@@XZ",
                          (void*)&RtUi_GroupDescription_Construct);       ++n;
    RegisterRuntimeAddr(0x180046980ull, (void*)&RtUi_GroupDescription_Construct);

    RegisterRuntimeExport("?Reconstruct@GroupDescription@UI@@UEAAPEAVExternal@Object@@XZ",
                          (void*)&RtUi_GroupDescription_Reconstruct);     ++n;
    RegisterRuntimeAddr(0x1800468e0ull, (void*)&RtUi_GroupDescription_Reconstruct);

    RegisterRuntimeExport("?Destruct@GroupDescription@UI@@UEAAXXZ",
                          (void*)&RtUi_GroupDescription_Destruct);        ++n;
    RegisterRuntimeAddr(0x180046a20ull, (void*)&RtUi_GroupDescription_Destruct);

    RegisterRuntimeExport("??1GroupDescription@UI@@UEAA@XZ",
                          (void*)&RtUi_GroupDescription_Dtor);            ++n;
    RegisterRuntimeAddr(0x180117c00ull, (void*)&RtUi_GroupDescription_Dtor);

    RegisterRuntimeExport("?Serialize@GroupDescription@UI@@UEAA_NAEAVStream@IO@@@Z",
                          (void*)&RtUi_GroupDescription_Serialize);       ++n;
    RegisterRuntimeAddr(0x180117d00ull, (void*)&RtUi_GroupDescription_Serialize);

    RegisterRuntimeExport(
        "?CreateInstance@GroupDescription@UI@@UEBA?AV?$InternalPtr@VElement@UI@@@Object@@PEBVDialog@2@@Z",
        (void*)&RtUi_GroupDescription_CreateInstance);                    ++n;
    RegisterRuntimeAddr(0x180117dc0ull, (void*)&RtUi_GroupDescription_CreateInstance);

    return n;
}
