// ============================================================================
// runtime/src/ui/spritesheet_vtable.cpp — WAVE 10 LANE UI4
//   the SpriteSheet virtual table's slot bodies
// ============================================================================
//
// WHY THIS FILE EXISTS, and what the census row it answers actually meant
// ---------------------------------------------------------------------------
// The wave-10 census lists `??_7SpriteSheet@UI@@6B@` as an UNPORTED UI export
// referenced from runtime/src/ui/sprite.cpp. That row is REAL but it is not
// what it looks like, and the difference is the whole of this lane's scoping:
//
//   * The vtable DATA is already minted. Wave 9 lane C2's generator emitted it
//     as `kVSlots45` in standalone/rt_data_mint_tables.inc (9 slots, size proof
//     "tail-not-a-pointer"), and standalone/rt_data_mint.cpp registers it under
//     BOTH doors — `RegisterRuntimeExport("??_7SpriteSheet@UI@@6B@", …)` and
//     `RegisterRuntimeAddr(0x180154590, …)`. The census cannot see either,
//     because both spell the symbol through a STRUCT FIELD rather than a
//     literal (reference_census_cannot_see_nonliteral_registrations.md), and
//     the mint is additionally gated behind `BANISHED_RT_MINT=1` until cutover.
//     So the row is a census artefact twice over, not unwritten data.
//
//   * What IS unwritten is the table's CONTENTS. A minted slot is filled from
//     the port registry at RtDataMintInit() time and is a NAMED TRAP until the
//     body behind it is ported. Before this file, 8 of SpriteSheet's 9 slots
//     were traps — only slot 0 (the deleting destructor, wave 9 lane K) was
//     filled. That is exactly the "8 predicted vtable walls" that
//     docs/runtime_port/WAVE10_PARTITION.md assigns to lane UI4, and it is the
//     lane's real work.
//
// This file closes 7 of those 8. `?Serialize@` is held back, and its blocker
// is named at the bottom of this banner rather than left implicit.
//
// ---------------------------------------------------------------------------
// THE ORACLE
// ---------------------------------------------------------------------------
// Every body here is a pure read or an in-place construct on storage the
// caller owns, so the REPLAY oracle applies without qualification: call ours,
// call the shipped export on the same inputs (a private scratch object for the
// two that write), compare. Nothing on these paths allocates, takes a lock, or
// touches state outside the object — which is what makes replaying safe and is
// why sprite.cpp's argument for scratch replicas does not need to be repeated.
//
// ---------------------------------------------------------------------------
// HELD BACK: ?Serialize@SpriteSheet@UI@@UEAA_NAEAVStream@IO@@@Z (0x18011d730)
// ---------------------------------------------------------------------------
// It is not held back for lack of a caller or lack of a decompile. The body is
// 60 instructions and reads clearly: it fetches the stream's Encoding through
// `stream+0x10`, picks `vt+0x18` or `vt+0x20` by `stream+0x08`, then drives
// FIVE calls through the Encoding vtable at +0xa0 (four of them) and +0x78,
// serialises the two Strings at +0x38/+0x40 and the SpriteInfo array at +0x20.
//
// The blocker is those vtable OFFSETS. reference_encoding_vtable_offsets_
// differ_between_builds.md records that IO::Encoding's slots sit at DIFFERENT
// offsets in the retail DLL than in the profile DLL (+0x78/+0x98/+0xa0 vs
// +0x90/+0xb0/+0xb8) — so the +0xa0 and +0x78 read off retail here cannot be
// named from the profile build's symbols, which is where every other Encoding
// slot in this tree got its name. Porting it means first pinning retail's
// Encoding vtable slot-by-slot, which is an IO-family job with its own
// evidence trail, not a SpriteSheet one.
//
// WHAT WOULD EXPIRE IT: a slot map for retail's `??_7Encoding@IO@@6B@` that
// names +0x78 and +0xa0. runtime/src/io/encoding.cpp is where that would land.
// ============================================================================

#include <cstddef>
#include <cstdint>
#include <cstring>

#include "../../funcs/rt_port.h"
#include "../../types/ui/SpriteSheet.h"

namespace {

// ---- the exported DATA these bodies read -----------------------------------
// `?_typeInfo@SpriteSheet@UI@@0VBaseTypeInfo@Object@@A` is spelled `0V…A`, the
// PRIVATE static form — and this DLL exports its private members, so a name
// door is the correct door and no address door is needed
// (reference_aeaa_is_private_not_unexported.md). Read as a byte pointer at
// +0x00 / +0x08, the same idiom runtime/src/io/package_file.cpp uses.
const uint8_t* SheetTypeInfo() {
    return RT_DATA_SLOT(const uint8_t*,
                        "?_typeInfo@SpriteSheet@UI@@0VBaseTypeInfo@Object@@A");
}
void* BaseVtable()     { return RT_DATA_SLOT(void*, "??_7Base@Object@@6B@");     }  // 0x180154788
void* ExternalVtable() { return RT_DATA_SLOT(void*, "??_7External@Object@@6B@"); }  // 0x1801547a0
void* SheetVtable()    { return RT_DATA_SLOT(void*, "??_7SpriteSheet@UI@@6B@");  }  // 0x180154590

typedef uint32_t    (*U32ThisFn)(const void*);
typedef const char* (*CStrThisFn)(const void*);
typedef int         (*IntThisFn)(const void*);
typedef void*       (*PtrThisFn)(void*);

// The in-place construct, byte for byte as 0x180003100 spells it — and NOT a
// call to RtUi_SpriteSheet_Ctor, even though the two are the same instruction
// sequence. They are two separate inlined copies in the DLL; keeping them two
// here means a future correction to one cannot silently change the other's
// meaning, and the oracle checks each against its own export.
//
// The two dead intermediate vtable stores are kept for the reason
// sprite.cpp's constructor already states: they ARE the body, and a debugger
// break between them would observe the intermediate value.
void ConstructInPlace(uint8_t* self) {
    auto qword = [](uint8_t* p, size_t off) -> void*& {
        return *reinterpret_cast<void**>(p + off);
    };
    qword(self, 0x00) = BaseVtable();
    qword(self, 0x00) = ExternalVtable();
    qword(self, 0x08) = nullptr;
    qword(self, 0x10) = nullptr;
    qword(self, 0x18) = nullptr;
    qword(self, 0x00) = SheetVtable();
    qword(self, 0x20) = nullptr;
    qword(self, 0x28) = nullptr;   // m_spriteCount + m_spriteCapacity, one store
    qword(self, 0x30) = nullptr;
    qword(self, 0x38) = nullptr;
    qword(self, 0x40) = nullptr;
    qword(self, 0x48) = nullptr;   // m_field48 + m_field4C
    self[0x50] = 0;                // one byte; +0x51..+0x57 are never stored
}

}  // namespace

// ---- slot 2  ?Type@SpriteSheet@UI@@UEBAIXZ  0x180002e30 --------------------
//   MOV EAX,dword ptr [0x1801fadd0] / RET     — RCX is never read.
extern "C" uint32_t RtUi_SpriteSheet_Type(const void* self) {
    RT_DIFF_BEGIN("UI::SpriteSheet::Type");
    (void)self;
    uint32_t ours = 0;
    std::memcpy(&ours, SheetTypeInfo(), sizeof ours);
    RT_DIFF_SCALAR(ours, RT_ORIGINAL_ONCE(U32ThisFn, "?Type@SpriteSheet@UI@@UEBAIXZ")(self));
    RT_DIFF_WORK(true);
    return ours;
}

// The STATIC spelling of the same two instructions, at its own address
// (0x180002e40). Registered separately because it is a separate export with a
// separate call surface; sharing the body is what the DLL does too.
extern "C" uint32_t RtUi_SpriteSheet_TypeId() {
    RT_DIFF_BEGIN("UI::SpriteSheet::TypeId");
    uint32_t ours = 0;
    std::memcpy(&ours, SheetTypeInfo(), sizeof ours);
    using Fn = uint32_t (*)();
    RT_DIFF_SCALAR(ours, RT_ORIGINAL_ONCE(Fn, "?TypeId@SpriteSheet@UI@@SAIXZ")());
    RT_DIFF_WORK(true);
    return ours;
}

// ---- slot 1  ?TypeName@SpriteSheet@UI@@UEBAPEBDXZ  0x180002e50 -------------
//   MOV RAX,qword ptr [0x1801fadd8] / RET     — typeInfo + 8, RCX unread.
extern "C" const char* RtUi_SpriteSheet_TypeName(const void* self) {
    RT_DIFF_BEGIN("UI::SpriteSheet::TypeName");
    (void)self;
    const char* ours = nullptr;
    std::memcpy(&ours, SheetTypeInfo() + 8, sizeof ours);
    RT_DIFF_SCALAR(static_cast<const void*>(ours),
                   static_cast<const void*>(
                       RT_ORIGINAL_ONCE(CStrThisFn,
                           "?TypeName@SpriteSheet@UI@@UEBAPEBDXZ")(self)));
    RT_DIFF_WORK(true);
    return ours;
}

// ---- slot 3  ?GetFileVersion@SpriteSheet@UI@@UEBAHXZ  0x18011d710 ----------
//   MOV EAX,6 / RET. The sheet resource's on-disk format version.
extern "C" int RtUi_SpriteSheet_GetFileVersion(const void* self) {
    RT_DIFF_BEGIN("UI::SpriteSheet::GetFileVersion");
    (void)self;
    const int ours = 6;
    RT_DIFF_SCALAR(ours, RT_ORIGINAL_ONCE(IntThisFn,
        "?GetFileVersion@SpriteSheet@UI@@UEBAHXZ")(self));
    RT_DIFF_WORK(true);
    return ours;
}

// ---- slot 4  ?Compile@SpriteSheet@UI@@UEAA…  0x18011d720 -------------------
//   MOV EAX,1 / RET. It IGNORES its Filename argument entirely — the return is
//   an Object::External::CompileResult and 1 is the constant answer. Not a
//   stub of ours: those two instructions are the whole shipped body.
extern "C" int RtUi_SpriteSheet_Compile(void* self, const void* filename) {
    RT_DIFF_BEGIN("UI::SpriteSheet::Compile");
    (void)self; (void)filename;
    const int ours = 1;
    using Fn = int (*)(void*, const void*);
    RT_DIFF_SCALAR(ours, RT_ORIGINAL_ONCE(Fn,
        "?Compile@SpriteSheet@UI@@UEAA?AW4CompileResult@External@Object@@AEBVFilename@System@@@Z")
        (self, filename));
    RT_DIFF_WORK(true);
    return ours;
}

// ---- slot 7  ?Destruct@SpriteSheet@UI@@UEAAXXZ  0x180003180 ----------------
//   MOV RAX,[RCX] / XOR EDX,EDX / JMP qword ptr [RAX]
// A tail jump into SLOT 0 of the object's own vtable — the vector deleting
// destructor — with flag 0, i.e. "destroy, do not free". Reading the vtable
// from the OBJECT and not from a resolved constant is load-bearing: a derived
// SpriteSheet would dispatch to its own slot 0, and hard-coding ours would
// break that silently.
//
// NOT REPLAYED. Its effect is to run a destructor on the live object, and
// calling the shipped body a second time would run it twice — a double free of
// whatever the sheet owns. This is the one body in the file where the replay
// argument fails, and it is stated rather than quietly skipped.
extern "C" void RtUi_SpriteSheet_Destruct(void* self) {
    RT_DIFF_BEGIN("UI::SpriteSheet::Destruct");
    RT_DIFF_WORK(true);
    void** const vt = *reinterpret_cast<void***>(self);
    reinterpret_cast<void (*)(void*, uint32_t)>(vt[0])(self, 0);
}

// ---- slot 8  ?Construct@SpriteSheet@UI@@UEAAPEAVExternal@Object@@XZ  0x180003100
//   if (this) { <the constructor, inlined>; return this; } else return nullptr;
// The null test is retail's own (0x180003114), not a guard added here.
extern "C" void* RtUi_SpriteSheet_Construct(void* self) {
    RT_DIFF_BEGIN("UI::SpriteSheet::Construct");
    if (!self) {
        if (rt_diff_on__) {
            void* theirs = RT_ORIGINAL_ONCE(PtrThisFn,
                "?Construct@SpriteSheet@UI@@UEAAPEAVExternal@Object@@XZ")(nullptr);
            RT_DIFF_SCALAR(static_cast<void*>(nullptr), theirs);
        }
        RT_DIFF_WORK(false);          // the null arm does no work, and says so
        return nullptr;
    }
    ConstructInPlace(static_cast<uint8_t*>(self));

    if (rt_diff_on__) {
        // A PRIVATE scratch object, so the shipped body never writes the live
        // one. Poisoned first, and compared over kSpriteSheetWrittenBytes
        // (0x51) rather than sizeof — +0x51..+0x57 is never stored by either
        // side and comparing it reports allocator residue
        // (reference_oracle_must_not_compare_unwritten_bytes.md; this exact
        // window cost the Wave 7 run a 100% false divergence).
        alignas(16) uint8_t theirs[sizeof(SpriteSheet)];
        std::memset(theirs, 0xCD, sizeof theirs);
        void* const ret = RT_ORIGINAL_ONCE(PtrThisFn,
            "?Construct@SpriteSheet@UI@@UEAAPEAVExternal@Object@@XZ")(theirs);
        RT_DIFF_BYTES(self, theirs, kSpriteSheetWrittenBytes);
        RT_DIFF_SHADOW(ret == static_cast<void*>(theirs),
                       "returns this", "returns this");
        RT_DIFF_WORK(true);
    }
    return self;
}

// ---- slot 6  ?Reconstruct@SpriteSheet@UI@@UEAAPEAVExternal@Object@@XZ  0x180003080
//   Destruct-then-Construct on the SAME storage, and note the difference from
//   Construct: there is NO null test on this path (0x180003092 dereferences
//   RCX immediately). Retail relies on the caller never passing null, and
//   CLAUDE.md's rule says leave the deref faithful.
//
// NOT REPLAYED, for Destruct's reason: the destructor half runs on the live
// object and cannot be run twice.
extern "C" void* RtUi_SpriteSheet_Reconstruct(void* self) {
    RT_DIFF_BEGIN("UI::SpriteSheet::Reconstruct");
    RT_DIFF_WORK(true);
    void** const vt = *reinterpret_cast<void***>(self);
    reinterpret_cast<void (*)(void*, uint32_t)>(vt[0])(self, 0);
    ConstructInPlace(static_cast<uint8_t*>(self));
    return self;
}

// ============================================================================
// Registration — called from RtUiSpriteCaptureRegister(), under the same
// BANISHED_RT_UISPRITECAP switch, because the two halves of lane UI4 are
// verified by the same run and an A/B that could split them would be a third
// state nobody wants to reason about.
// ============================================================================
int RtUiSpriteSheetVtableRegister() {
    int n = 0;
    RegisterRuntimeExport("?Type@SpriteSheet@UI@@UEBAIXZ",
                          (void*)&RtUi_SpriteSheet_Type); ++n;
    RegisterRuntimeAddr(0x180002e30, (void*)&RtUi_SpriteSheet_Type);
    RegisterRuntimeExport("?TypeId@SpriteSheet@UI@@SAIXZ",
                          (void*)&RtUi_SpriteSheet_TypeId); ++n;
    RegisterRuntimeAddr(0x180002e40, (void*)&RtUi_SpriteSheet_TypeId);
    RegisterRuntimeExport("?TypeName@SpriteSheet@UI@@UEBAPEBDXZ",
                          (void*)&RtUi_SpriteSheet_TypeName); ++n;
    RegisterRuntimeAddr(0x180002e50, (void*)&RtUi_SpriteSheet_TypeName);
    RegisterRuntimeExport("?GetFileVersion@SpriteSheet@UI@@UEBAHXZ",
                          (void*)&RtUi_SpriteSheet_GetFileVersion); ++n;
    RegisterRuntimeAddr(0x18011d710, (void*)&RtUi_SpriteSheet_GetFileVersion);
    RegisterRuntimeExport(
        "?Compile@SpriteSheet@UI@@UEAA?AW4CompileResult@External@Object@@AEBVFilename@System@@@Z",
        (void*)&RtUi_SpriteSheet_Compile); ++n;
    RegisterRuntimeAddr(0x18011d720, (void*)&RtUi_SpriteSheet_Compile);
    RegisterRuntimeExport("?Destruct@SpriteSheet@UI@@UEAAXXZ",
                          (void*)&RtUi_SpriteSheet_Destruct); ++n;
    RegisterRuntimeAddr(0x180003180, (void*)&RtUi_SpriteSheet_Destruct);
    RegisterRuntimeExport("?Construct@SpriteSheet@UI@@UEAAPEAVExternal@Object@@XZ",
                          (void*)&RtUi_SpriteSheet_Construct); ++n;
    RegisterRuntimeAddr(0x180003100, (void*)&RtUi_SpriteSheet_Construct);
    RegisterRuntimeExport("?Reconstruct@SpriteSheet@UI@@UEAAPEAVExternal@Object@@XZ",
                          (void*)&RtUi_SpriteSheet_Reconstruct); ++n;
    RegisterRuntimeAddr(0x180003080, (void*)&RtUi_SpriteSheet_Reconstruct);
    // BOTH doors on every row, for the reason the mint itself gives: a minted
    // vtable slot is filled by NAME, but 30-odd sites in this tree reach
    // exported code by ADDRESS, and a name-only registration leaves those
    // walking past the port into the DLL.
    return n;
}
