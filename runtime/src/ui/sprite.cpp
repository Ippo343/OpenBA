// ============================================================================
// runtime/src/ui/sprite.cpp — Wave 7 lane F — Sprite + SpriteSheet +
//                              SpriteContext + StringTable + UI::String
// ============================================================================
//
// Ports 20 of the 23 exports this lane owns (PLAN.md §4.2 wave 7, §6). Every
// mangled name below was checked against the shipped export directory of
// C:\GOG Games\Banished\Runtime-x64.dll with a pefile-style reader BEFORE any
// body was written (reference_resolveexport_phantom_names.md); all 23 exist.
// The full write-up — the oracle argument per function, the SpriteInfo layout
// correction, and everything held back — is
// docs/runtime_port/WAVE7_LANE_F.md.
//
//   | Export (short form)                    | Retail      | Ported | Oracle  |
//   |-----------------------------------------|-------------|--------|---------|
//   | Sprite::Sprite()                         | 0x18011ba10 | yes    | replay  |
//   | Sprite::Sprite(sheet,name)                | 0x18011b9c0 | yes    | replay  |
//   | Sprite::Sprite(sheet,index)                | 0x18011ba30 | yes    | replay  |
//   | Sprite::IsValid()                        | 0x180030c40 | yes    | replay  |
//   | Sprite::Width()                          | 0x180030bf0 | yes    | replay  |
//   | Sprite::Height()                         | 0x180030c10 | yes    | replay  |
//   | SpriteSheet::SpriteSheet()                | 0x18011d600 | yes    | replay  |
//   | SpriteSheet::GetSpriteCount()             | 0x180003190 | yes    | replay  |
//   | SpriteSheet::GetSpriteIndexFromName()      | 0x1800031f0 | yes    | replay  |
//   | SpriteSheet::GetUVs()                     | 0x180003330 | yes    | replay  |
//   | SpriteSheet::SetSpriteInfo()               | 0x180003460 | yes    | scratch |
//   | SpriteSheet::AddSpriteInfo()                | 0x180003390 | yes    | scratch |
//   | SpriteContext::AddLine()                   | 0x18011cbe0 | NO     | —       |
//   | SpriteContext::SetScissor(Viewport const&)  | 0x18011d050 | NO     | —       |
//   | SpriteContext::SetScissor(int,int,int,int)  | 0x18011d010 | NO     | —       |
//   | StringTable::GetCount()                    | 0x180002610 | yes    | replay  |
//   | StringTable::GetText(int)                  | 0x180002550 | yes    | replay* |
//   | StringTable::GetText(String const&)         | 0x1800025b0 | yes    | replay* |
//   | UI::String::String()                       | 0x18011da40 | yes    | replay  |
//   | UI::String::String(table,name)              | 0x18011da60 | yes    | replay  |
//   | UI::String::String(table,index)             | 0x18011da20 | yes    | replay  |
//   | UI::String::IsValid()                      | 0x180035060 | yes    | replay  |
//   | UI::String::operator System::String()       | 0x180035020 | yes    | replay* |
//
// ---------------------------------------------------------------------------
// A CORRECTION THIS LANE MADE TO ITS OWN INHERITED HEADER
// ---------------------------------------------------------------------------
// runtime/types/ui/SpriteSheet.h's SpriteInfo layout was wrong before this
// lane touched it: width/height were guessed at +0x18/+0x1C and +0x20..+0x2C
// were guessed to be UV floats. The real layout (proven by AddSpriteInfo,
// SetSpriteInfo, GetUVs and Sprite::Width/Height all agreeing) is width
// +0x1C, height +0x20, and the four UVs at +0x24/+0x28/+0x2C/+0x30, with a
// previously-unnoticed always-zero int32 at +0x18. See the header for the
// full evidence; nothing in the tree read the old (wrong) fields, so this
// was a latent defect, not a live one.
//
// ---------------------------------------------------------------------------
// WHAT STAYS ON THE DLL, AND WHY (docs/runtime_port/WAVE7_LANE_F.md §2)
// ---------------------------------------------------------------------------
// AddLine and both SetScissor overloads all reach unexported, unreversed
// internal helpers (`Flush`, `FlushDeferredFonts`, `AddSpriteDraw`,
// `SpriteCapture::AddScissor`) that submit into the LIVE Video::Context /
// GPU vertex buffer this SpriteContext wraps — state that lives outside the
// SpriteContext object, so a scratch copy of the object does not isolate a
// replay the way it does for SetSpriteInfo/AddSpriteInfo. Running the DLL's
// body a second time for comparison would submit a second, real, visible
// draw or scissor change. See runtime/types/ui/SpriteContext.h for the
// partial layout this lane read while making that argument.
// ============================================================================

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>

#include "../../funcs/rt_port.h"

#include "../../types/object/External.h"
#include "../../types/ui/Sprite.h"
#include "../../types/ui/SpriteSheet.h"
#include "../../types/ui/StringTable.h"
#include "../../types/ui/String.h"
#include "../../types/system/String.h"
#include "../system/string.h"   // RtString_StrCompare, for GetIndexFromName
#include "../collection/array_internal.h" // ENDGAME B.3.2 step 2 — kSpec_Sprite38

// ----------------------------------------------------------------------------
// Module / ResolveAddr / ResolveExport, forward-declared rather than pulled in
// from standalone/resolve.h — same reasoning rt_port.h gives for declaring
// RegisterRuntimeExport locally: resolve.h drags in <windows.h>, which PLAN.md
// §4.1 keeps out of runtime/src/. These declarations must stay byte-identical
// to resolve.h's; the linker checks them against it. (Precedent:
// runtime/src/achievement/achievement_manager.cpp:69-71.)
// ----------------------------------------------------------------------------
enum class Module : int { Exe = 0, Runtime = 1, VideoDX11 = 2, COUNT = 3 };
uintptr_t ResolveAddr(Module mod, uintptr_t ghidra_addr);
void*     ResolveExport(const char* mangled_name);

namespace {

// ----------------------------------------------------------------------------
// SpriteSheet::GetSpriteIndexFromName's algorithm (retail 0x1800031f0),
// shared by the Sprite-by-name ctor. Retail's inner loop is a hand-rolled
// "compare lengths, then compare bytes up to the first mismatch and subtract"
// that is behaviourally strcmp for two null-terminated buffers of EQUAL
// length (which is the only case it ever reaches the byte loop in) — a plain
// strcmp() reproduces the same sign and the same zero/nonzero verdict, which
// is all GetSpriteIndexFromName's caller (an equality test against -1 or the
// returned index) can observe. Simplification noted rather than transcribing
// retail's manual byte walk instruction for instruction.
// ----------------------------------------------------------------------------
int SpriteIndexFromName(const SpriteInfo* data, int32_t count, const char* key) {
    const int32_t keyLen = key ? RtSystem::StringLength(key) : 0;
    for (int32_t i = 0; i < count; ++i) {
        const char* name = data[i].name;
        const int32_t nameLen = name ? RtSystem::StringLength(name) : 0;
        if (nameLen != keyLen) continue;
        if (name == nullptr || nameLen == 0) return i;   // covers keyLen==0 too (lengths already match)
        if (key != nullptr && std::strcmp(name, key) == 0) return i;
        // key == nullptr falls through to "no match, keep scanning": retail's
        // iVar6=1 path. Unreachable in practice (keyLen==0 forces nameLen==0,
        // handled above), kept for exact parity with retail's control flow.
    }
    return -1;
}

}  // namespace

// ============================================================================
// UI::Sprite — ENDGAME B.3.6 block D (RULE B6-10)
// ============================================================================
//
// The bodies are the class's real constructors and members now
// (runtime/types/ui/Sprite.h). Each registered DOOR keeps its name and
// placement-news / delegates onto them: the exe and the unconverted bodies
// construct Sprites INTO CALLER STORAGE through the ctor doors, so the doors
// stay until the caller batches convert those sites to C++ construction.

namespace UI {

// Sprite::Sprite(ExternalPtr<SpriteSheet>, String const& name) — retail
// 0x18011b9c0. `sheet` arrives as one raw pointer (ExternalPtr<T>, External.h);
// the name is read through its data pointer, the String's one field.
Sprite::Sprite(Object::ExternalPtr<SpriteSheet> sheet, const System::String& name)
    : spriteSheet(sheet.Get()) {
    if (spriteSheet != nullptr) {
        spriteIndex = static_cast<const SpriteSheet*>(spriteSheet)->GetSpriteIndexFromName(name);
    } else {
        spriteIndex = -1;
    }
}

// Sprite::Sprite(ExternalPtr<SpriteSheet>, int index) — retail 0x18011ba30.
// Two plain stores, no branch.
Sprite::Sprite(Object::ExternalPtr<SpriteSheet> sheet, int32_t index)
    : spriteSheet(sheet.Get()), spriteIndex(index) {}

// ?GetSpriteSheet@Sprite@UI@@ — retail 0x180030ca0 (`MOV RAX,[RCX]; MOV [RDX],RAX`).
Object::ExternalPtr<SpriteSheet> Sprite::GetSpriteSheet() const {
    return Object::ExternalPtr<SpriteSheet>(static_cast<SpriteSheet*>(spriteSheet));
}

// Sprite::Width() const — retail 0x180030bf0. NO upper-bound check on
// spriteIndex, exactly as retail has none (an out-of-range positive index
// reads whatever follows the array in memory) — CLAUDE.md: do not add a
// guard retail lacks.
int32_t Sprite::Width() const {
    if (spriteIndex < 0) return 0;
    const auto* sheet = static_cast<const SpriteSheet*>(spriteSheet);
    return sheet->m_sprites[spriteIndex].width;
}

// Sprite::Height() const — retail 0x180030c10. Same no-bound-check note.
int32_t Sprite::Height() const {
    if (spriteIndex < 0) return 0;
    const auto* sheet = static_cast<const SpriteSheet*>(spriteSheet);
    return sheet->m_sprites[spriteIndex].height;
}

}  // namespace UI

// ---- the doors --------------------------------------------------------------

// ??0Sprite@UI@@QEAA@XZ — retail 0x18011ba10. Writes +0x00 and +0x08 only.
void* RtUi_Sprite_DefaultCtor(void* retval) {
    new (retval) UI::Sprite();
    return retval;
}

// ??0Sprite@UI@@QEAA@V?$ExternalPtr@…@AEBVString@System@@@Z — retail 0x18011b9c0.
void* RtUi_Sprite_CtorByName(void* retval, uint64_t spriteSheetPtr, const void* spriteName) {
    new (retval) UI::Sprite(
        Object::ExternalPtr<UI::SpriteSheet>(reinterpret_cast<UI::SpriteSheet*>(spriteSheetPtr)),  // B.3.6 bridge
        *static_cast<const System::String*>(spriteName));
    return retval;
}

// ??0Sprite@UI@@QEAA@V?$ExternalPtr@…@H@Z — retail 0x18011ba30.
void* RtUi_Sprite_CtorByIndex(void* retval, void* spriteSheetPtr, int index) {
    new (retval) UI::Sprite(
        Object::ExternalPtr<UI::SpriteSheet>(static_cast<UI::SpriteSheet*>(spriteSheetPtr)),
        index);
    return retval;
}

// ?IsValid@Sprite@UI@@QEBA_NXZ — retail 0x180030c40.
bool RtUi_Sprite_IsValid(const void* spriteRaw) {
    return reinterpret_cast<const UI::Sprite*>(spriteRaw)->UI::Sprite::IsValid();  // B.3.6 bridge
}

// ?Width@Sprite@UI@@QEBAHXZ — retail 0x180030bf0.
int RtUi_Sprite_Width(const void* spriteRaw) {
    return reinterpret_cast<const UI::Sprite*>(spriteRaw)->UI::Sprite::Width();  // B.3.6 bridge
}

// ?Height@Sprite@UI@@QEBAHXZ — retail 0x180030c10.
int RtUi_Sprite_Height(const void* spriteRaw) {
    return reinterpret_cast<const UI::Sprite*>(spriteRaw)->UI::Sprite::Height();  // B.3.6 bridge
}

// ============================================================================
// UI::SpriteSheet — the non-virtual members (ENDGAME B.3.6 block D). The class
// is runtime/types/ui/SpriteSheet.h; its slots are in spritesheet_vtable.cpp.
// ============================================================================

// SpriteSheet::SpriteSheet() — retail 0x18011d600. THE CTOR DOOR, KEPT (B6-3:
// the exe's thumb_vtables.cpp constructs bare SpriteSheets through it; the
// class has no real ctor). Stamps Object::Base's vtable, then Object::External's,
// zeroes External's three fields, stamps SpriteSheet's own (EXPORTED,
// ??_7SpriteSheet@UI@@6B@ — PLAN §2.2 item D), then zeroes the rest of the
// object. The two intermediate vtable stores are dead by the time the ctor
// returns but are kept, matching the Achievement::Achievement precedent
// (runtime/src/achievement/achievement_manager.cpp): they ARE the body.
void* RtUi_SpriteSheet_Ctor(void* selfRaw) {
    auto* self = static_cast<uint8_t*>(selfRaw);
    auto qword = [](uint8_t* p, size_t off) -> void*& {
        return *reinterpret_cast<void**>(p + off);
    };
    static void* vtableBase     = ResolveExport("??_7Base@Object@@6B@");
    static void* vtableExternal = ResolveExport("??_7External@Object@@6B@");
    static void* vtableSheet    = ResolveExport("??_7SpriteSheet@UI@@6B@");

    qword(self, 0x00) = vtableBase;
    qword(self, 0x00) = vtableExternal;
    qword(self, 0x08) = nullptr;
    qword(self, 0x10) = nullptr;
    qword(self, 0x18) = nullptr;
    qword(self, 0x00) = vtableSheet;
    // ENDGAME B.3.8 block RB (RULE B8-1): the +0x20..+0x50 stores are ONE body
    // with the real constructor `UI::SpriteSheet()` (SpriteSheet.h, InitFields).
    UI::SpriteSheet::DoorBody::Init(reinterpret_cast<UI::SpriteSheet*>(selfRaw));   // B.3.8 bridge

    return selfRaw;
}

namespace UI {

// SpriteSheet::GetSpriteCount() const — retail 0x180003190.
int32_t SpriteSheet::GetSpriteCount() const { return m_spriteCount; }

// SpriteSheet::GetSpriteIndexFromName(String const&) const — retail 0x1800031f0.
int32_t SpriteSheet::GetSpriteIndexFromName(const System::String& name) const {
    return SpriteIndexFromName(m_sprites, m_spriteCount, name._data);
}

// SpriteSheet::GetSpriteNameFromIndex(int) const — retail 0x1800032d0.
//
// WAVE 10 LANE UI4. Reached by SpriteCapture::AddSprite. Eighteen instructions,
// and NO bounds check of any kind: the index is scaled straight into
// `m_sprites` by `IMUL RAX,RAX,0x38`. Same faithfulness call as GetUVs below,
// and CLAUDE.md's rule — do not add a guard retail lacks.
//
// RETSLOT in retail: the door keeps (this, hidden-return-slot, index) and
// placement-news this member's prvalue into the slot.
System::String SpriteSheet::GetSpriteNameFromIndex(int32_t index) const {
    System::String out;                        // 0x1800032f4, null before the load
    char* name = m_sprites[index].name;
    if (name) {
        RtSystem::StringRefCount(name) += 1;   // 0x180003302
        out._data = name;
    }
    return out;
}

// SpriteSheet::GetUVs(int, float&, float&, float&, float&) const — retail
// 0x180003330. NO upper-bound check (only `index < 0` is guarded), matching
// retail exactly — CLAUDE.md: do not add a guard retail lacks.
void SpriteSheet::GetUVs(int32_t index, float& u0, float& v0, float& u1, float& v1) const {
    float ou0 = 0.0f, ov0 = 0.0f, ou1 = 0.0f, ov1 = 0.0f;
    if (index >= 0) {
        const SpriteInfo& e = m_sprites[index];
        ou0 = e.u0; ov0 = e.v0; ou1 = e.u1; ov1 = e.v1;
    }
    u0 = ou0; v0 = ov0; u1 = ou1; v1 = ov1;
}

// SpriteSheet::SetSpriteInfo(int,int,int,float,float,float,float) — retail
// 0x180003460. Mutates ONE existing SpriteInfo entry in place; no allocation,
// no refcounted field touched. No bound check on `index`, matching retail.
void SpriteSheet::SetSpriteInfo(int32_t index, int32_t width, int32_t height,
                                float u0, float v0, float u1, float v1) {
    SpriteInfo& e = m_sprites[index];
    e.width = width; e.height = height;
    e.u0 = u0; e.v0 = v0; e.u1 = u1; e.v1 = v1;
}

// SpriteSheet::AddSpriteInfo(String const&,String const&,int,int,int,int) —
// retail 0x180003390. This does not need retail's own pre-bump/post-release
// dance around the new element's refcounts (the growth helper's own
// copy-construct step supplies the ONE bump the caller-visible contract needs;
// the dance nets to the same +1 either way — verified by hand against the
// decompile of both 0x180003390 and its internal helper 0x180004e30).
void SpriteSheet::AddSpriteInfo(const System::String& name, const System::String& group,
                                int32_t x, int32_t y, int32_t width, int32_t height) {
    const int32_t oldCount = m_spriteCount;

    SpriteInfo newEntry{};
    newEntry.name        = name._data;
    newEntry.group       = group._data;
    newEntry.x           = x;
    newEntry.y           = y;
    newEntry._reserved18 = 0;
    newEntry.width       = width;
    newEntry.height      = height;
    newEntry.u0 = newEntry.v0 = newEntry.u1 = newEntry.v1 = 0.0f;
    newEntry._pad34      = 0;

    // The array mechanics (grow-if-needed, copy-construct with refcount bump,
    // free the old buffer) are Collection::Array<SpriteInfo>'s append-by-one
    // body. `m_sprites` + `m_spriteCount` + `m_spriteCapacity` (+0x20/+0x28/
    // +0x2C) are the {data,size,capacity} triple `Collection::Array<SpriteInfo>`
    // would use, so the call bridges at the use site. `kSpec_Sprite38`
    // (0x180004e30) is NON-TRIVIAL — a String refcount bump in its element
    // assign — so it stays the hand-derived `ExplicitSpec`.
    rtcoll::AsArray<&rtcoll::kSpec_Sprite38>(&m_sprites)
        ->SetSizeFill(oldCount + 1, &newEntry);
}

}  // namespace UI

// ---- the doors ---------------------------------------------------------------
int RtUi_SpriteSheet_GetSpriteCount(const void* selfRaw) {
    return reinterpret_cast<const UI::SpriteSheet*>(selfRaw)               // B.3.6 bridge
        ->UI::SpriteSheet::GetSpriteCount();
}

int RtUi_SpriteSheet_GetSpriteIndexFromName(const void* selfRaw, const void* nameStrPtr) {
    return reinterpret_cast<const UI::SpriteSheet*>(selfRaw)               // B.3.6 bridge
        ->UI::SpriteSheet::GetSpriteIndexFromName(
            *static_cast<const System::String*>(nameStrPtr));
}

// RETSLOT: `(this, ret, index)`, the slot handed back in RAX. Getting that arity
// wrong is reference_hidden_retslot_wild_write.md's failure mode.
void* RtUi_SpriteSheet_GetSpriteNameFromIndex(const void* selfRaw, void* retval, int index) {
    new (retval) System::String(
        reinterpret_cast<const UI::SpriteSheet*>(selfRaw)                  // B.3.6 bridge
            ->UI::SpriteSheet::GetSpriteNameFromIndex(index));
    return retval;                             // retail returns the slot in RAX
}

void RtUi_SpriteSheet_GetUVs(const void* selfRaw, int index, float* u0, float* v0, float* u1, float* v1) {
    reinterpret_cast<const UI::SpriteSheet*>(selfRaw)                      // B.3.6 bridge
        ->UI::SpriteSheet::GetUVs(index, *u0, *v0, *u1, *v1);
}

void RtUi_SpriteSheet_SetSpriteInfo(void* selfRaw, int index, int width, int height,
                                     float u0, float v0, float u1, float v1) {
    reinterpret_cast<UI::SpriteSheet*>(selfRaw)                            // B.3.6 bridge
        ->UI::SpriteSheet::SetSpriteInfo(index, width, height, u0, v0, u1, v1);
}

void RtUi_SpriteSheet_AddSpriteInfo(void* selfRaw, const void* nameStrPtr, const void* groupStrPtr,
                                     int x, int y, int width, int height) {
    reinterpret_cast<UI::SpriteSheet*>(selfRaw)                            // B.3.6 bridge
        ->UI::SpriteSheet::AddSpriteInfo(*static_cast<const System::String*>(nameStrPtr),
                                         *static_cast<const System::String*>(groupStrPtr),
                                         x, y, width, height);
}
// ============================================================================
// UI::StringTable — the non-virtual members (ENDGAME B.3.6 block D). The class
// is runtime/types/ui/StringTable.h; its slots are in stringtable_factory.cpp.
// ============================================================================

namespace UI {

// StringTable::GetCount() const — retail 0x180002610: `return *(int*)(this+0x28)`.
int32_t StringTable::GetCount() const { return _size; }

// StringTable::GetText(int) const — retail 0x180002550. Bounds-checked both
// ways (unlike Sprite/SpriteSheet's UV accessors); returns an empty String on
// out-of-range, a NEW OWNED reference on a hit.
// *** WAVE 10 LANE UIORACLE — A REAL PORT DEFECT, FIXED HERE. THE LOADS WERE
// HOISTED ABOVE THE GUARDS. ***
//
// The first draft read `data` (this+0x20) and `count` (this+0x28) at the top and
// then tested `index >= 0 && index < count`. Retail does NOT: its very first
// instruction pair after the prologue is
//
//     180002569  MOV dword ptr [RSP],R9D
//     18000256c  TEST R8D,R8D
//     18000256e  JNS  0x180002573          ; index >= 0 ?
//     180002570  MOV  qword ptr [RDX],R9   ; else *ret = 0
//     180002573  ...                       ; RET -- `this` NEVER DEREFERENCED
//
// so on a NEGATIVE index retail returns an empty String **without touching
// `this` at all**, and only then does it read this+0x28 and this+0x20.
//
// That is not a hypothetical difference. A `UI::String` that names no text is
// `{table = nullptr, id = -1}`, and `UI::String::operator System::String`
// (0x180035020) forwards it here verbatim (`MOV R8D,[RCX+8]; MOV RCX,[RCX]`).
// Retail's guard order makes that harmless; the hoisted loads made it an
// ACCESS_VIOLATION reading 0x20. (As a member this means `this` may be null
// on that path; the body never compares or dereferences it there.)
//
// AND THIS IS NOT THE "don't fix a faithful crash with a null check" case
// (CLAUDE.md). No guard is being ADDED: retail's guards are in retail's ORDER.
System::String StringTable::GetText(int32_t index) const {
    System::String out;                                          // data == null
    if (index >= 0) {                                            // 0x18000256c
        if (index < _size) {                                     // 0x180002573
            char* text = _data[index].text;                      // 0x180002585
            if (text) {
                RtSystem::StringRefCount(text) += 1;
                out._data = text;
            }
        }
    }
    return out;
}

// ?GetIndexFromName@StringTable@UI@@QEBAHAEBVString@System@@@Z  retail 0x180002470
//
// A linear scan of the table's TextEntry array (stride 0x10, data at +0x20,
// count at +0x28 — StringTable.h), comparing `entry.name` against `key`. Two
// details from the listing that a paraphrase loses:
//
//  * retail pre-checks the STORED LENGTHS (`name[-8]` vs `key[-8]`, with a
//    null string counting as length 0) and skips the strcmp entirely when they
//    differ — 0x18000249f..0x1800024b6. Same shortcut String::operator== uses.
//  * once the lengths agree, a table entry whose name is null OR whose length
//    is zero MATCHES IMMEDIATELY (0x1800024b8..0x1800024c2), without looking at
//    the key at all. Since the lengths already agree, the key is empty too, so
//    the answer is right — but it means an empty key finds the first empty
//    slot rather than failing, and that is retail's behaviour, not a shortcut
//    introduced here.
//
// Returns -1 when nothing matches.
int32_t StringTable::GetIndexFromName(const System::String& keyString) const {
    const char* key = keyString._data;
    const int keyLen = (key != nullptr) ? RtSystem::StringLength(key) : 0;

    int result = -1;
    for (int i = 0; i < _size; ++i) {
        const char* name = _data[i].name;
        const int nameLen = (name != nullptr) ? RtSystem::StringLength(name) : 0;
        if (nameLen != keyLen) continue;
        if (name == nullptr || nameLen == 0) { result = i; break; }
        if (key == nullptr) continue;                 // retail: ECX = key+1, non-zero
        if (RtSystem::RtString_StrCompare(name, key) == 0) { result = i; break; }
    }
    return result;
}

// StringTable::GetText(String const& key) const — retail 0x1800025b0. A
// two-line forwarder in retail: `GetText(this, retval, GetIndexFromName(key))`.
System::String StringTable::GetText(const System::String& name) const {
    return GetText(GetIndexFromName(name));
}

}  // namespace UI

// ---- the doors ---------------------------------------------------------------
// GetText x2 are hidden-retslot: `(this, ret, …)`, the member's prvalue is
// placement-new'd into the caller's slot. THE DOOR DECIDES THE OVERLOAD.

int RtUi_StringTable_GetCount(const void* selfRaw) {
    return reinterpret_cast<const UI::StringTable*>(selfRaw)->UI::StringTable::GetCount();  // B.3.6 bridge
}

void* RtUi_StringTable_GetText_Index(const void* selfRaw, void* retval, int index) {
    new (retval) System::String(
        reinterpret_cast<const UI::StringTable*>(selfRaw)->UI::StringTable::GetText(index));  // B.3.6 bridge
    return retval;
}

int RtUi_StringTable_GetIndexFromName(const void* selfRaw, const void* keyRaw) {
    return reinterpret_cast<const UI::StringTable*>(selfRaw)                    // B.3.6 bridge
        ->UI::StringTable::GetIndexFromName(*static_cast<const System::String*>(keyRaw));
}

void* RtUi_StringTable_GetText_Key(const void* selfRaw, void* retval, const void* keyStrPtr) {
    new (retval) System::String(
        reinterpret_cast<const UI::StringTable*>(selfRaw)                       // B.3.6 bridge
            ->UI::StringTable::GetText(*static_cast<const System::String*>(keyStrPtr)));
    return retval;
}
// ============================================================================
// UI::String — ENDGAME B.3.6 block D (RULE B6-10); retail's `UI::String`,
// the tree's `UIString`. Same shape as UI::Sprite above.
// ============================================================================

namespace UI {

// UI::String::String(ExternalPtr<StringTable>, String const& name) — retail
// 0x18011da60. StringTable::GetIndexFromName is a member (above).
String::String(Object::ExternalPtr<StringTable> t, const System::String& name)
    : table(t.Get()) {
    if (table != nullptr) {
        index = static_cast<const StringTable*>(table)->GetIndexFromName(name);
    } else {
        index = -1;
    }
}

// UI::String::String(ExternalPtr<StringTable>, int index) — retail
// 0x18011da20. Two plain stores.
String::String(Object::ExternalPtr<StringTable> t, int32_t i)
    : table(t.Get()), index(i) {}

// ?GetStringTable@String@UI@@ — retail 0x180035080 (`MOV RAX,[RCX]; MOV [RDX],RAX`).
Object::ExternalPtr<StringTable> String::GetStringTable() const {
    return Object::ExternalPtr<StringTable>(static_cast<StringTable*>(table));
}

// UI::String::operator System::String() const — retail 0x180035020. A
// ONE-LINE forwarder: `return table->GetText(index)`, with `table` read
// UNGUARDED — StringTable::GetText(int) tests `index < 0` BEFORE it touches
// `this`, which is what makes a never-assigned `{null, -1}` harmless (the
// WAVE 10 LANE UIORACLE note on RtUi_StringTable_GetText_Index). No null check
// retail lacks (CLAUDE.md).
String::operator System::String() const {
    return static_cast<const StringTable*>(table)->GetText(index);
}

}  // namespace UI

// ---- the doors --------------------------------------------------------------

// ??0String@UI@@QEAA@XZ — retail 0x18011da40.
void* RtUi_UIString_DefaultCtor(void* retval) {
    new (retval) UI::String();
    return retval;
}

// ??0String@UI@@QEAA@V?$ExternalPtr@VStringTable@UI@@@Object@@AEBV0System@@@Z
// — retail 0x18011da60.
void* RtUi_UIString_CtorByName(void* retval, uint64_t stringTablePtr, const void* nameStrPtr) {
    new (retval) UI::String(
        Object::ExternalPtr<UI::StringTable>(reinterpret_cast<UI::StringTable*>(stringTablePtr)),  // B.3.6 bridge
        *static_cast<const System::String*>(nameStrPtr));
    return retval;
}

// ??0String@UI@@QEAA@V?$ExternalPtr@VStringTable@UI@@@Object@@H@Z — retail 0x18011da20.
void* RtUi_UIString_CtorByIndex(void* retval, uint64_t stringTablePtr, int index) {
    new (retval) UI::String(
        Object::ExternalPtr<UI::StringTable>(reinterpret_cast<UI::StringTable*>(stringTablePtr)),  // B.3.6 bridge
        index);
    return retval;
}

// ?IsValid@String@UI@@QEBA_NXZ — retail 0x180035060.
bool RtUi_UIString_IsValid(const void* selfRaw) {
    return reinterpret_cast<const UI::String*>(selfRaw)->UI::String::IsValid();  // B.3.6 bridge
}

// ??BString@UI@@QEBA?AV0System@@XZ — retail 0x180035020. RETSLOT: the door
// placement-news the member's prvalue into the caller's slot (B5-12 as amended).
void* RtUi_UIString_ToSystemString(const void* selfRaw, void* retval) {
    new (retval) System::String(
        reinterpret_cast<const UI::String*>(selfRaw)->UI::String::operator System::String());  // B.3.6 bridge
    return retval;
}

// WAVE 9 LANE UIF — runtime/src/ui/spritecontext.cpp.
extern int RtUiSpriteContextRegister();

// ============================================================================
// ============================================================================
// 0x1800034F0  UI::SpriteSheet::Set(int, int, String const&, String const&, bool)
//              ord 3819.  WAVE 23 LANE TAIL
// ============================================================================
//
// Five parameters, and the mangling `XHHAEBVString@System@@0_N` is what pins
// their order: int, int, String const&, String const& (the `0` is MSVC's
// back-reference meaning "same type as the previous argument"), bool.
//
// THE ARGUMENT MAPPING WAS READ OFF THE PROLOGUE, not assumed from the
// mangling, because two of the five arrive on the stack. Retail saves four
// registers into the shadow space, pushes R14 and subtracts 0x20, so the frame
// has moved 0x28 by the time the stack arguments are read:
//
//   RCX          -> this
//   EDX  -> R14D -> p1 (int)     stored to +0x48 at 0x1800035B1
//   R8D  -> EBP  -> p2 (int)     stored to +0x4C at 0x1800035A6
//   R9   -> RDI  -> p3 (String&) assigned to +0x38
//   [RSP+0x50]   -> p4 (String&) assigned to +0x40   (caller slot +0x28 = arg5)
//   [RSP+0x58]   -> p5 (bool)    stored to +0x50 at 0x1800035AE (caller +0x30)
//
// NOTE THE ORDER IS NOT SOURCE ORDER: the body assigns +0x40 from p4 FIRST
// (0x1800034FF..0x180003558) and +0x38 from p3 SECOND. Reproduced.
//
// THIS ALSO NAMES THREE FIELDS runtime/types/ui/SpriteSheet.h left as
// `m_field48` / `m_field4C` / `m_field50` "unknown". They are precisely this
// function's two ints and its bool. The header is NOT renamed here - a name
// needs more evidence than "the one setter that writes it", and this lane has
// no reader to corroborate against. Recorded in the lane report instead.
//
// ---------------------------------------------------------------------------
// THE STRING ASSIGN IDIOM, AND THE QUIRK IN IT
// ---------------------------------------------------------------------------
// `System::String` is a bare `char*` into a refcounted block: the refcount is
// the dword at [data-4] and the allocation base is [data-8], which is the same
// layout spritecapture.cpp's `StringRc` / `ReleaseString` helpers already use.
// Retail's member-assign, per string (0x180003519..0x180003558):
//
//   old = this->field;
//   if (old != *src) {                       ; pointer identity short-circuit
//       if (old) {
//           if (--*(int32_t*)(old - 4) == 0) {
//               this->field -= 8;            ; *** the field itself is adjusted
//               MemFree(this->field);        ; *** and THEN passed to MemFree
//           } else {
//               this->field = nullptr;       ; *** see below
//           }
//       }
//       this->field = *src;
//       if (this->field) ++*(int32_t*)(this->field - 4);
//   }
//
// TWO THINGS HERE ARE DELIBERATE AND WOULD LOOK LIKE TRANSCRIPTION ERRORS:
//
//  * THE `else this->field = nullptr;` BRANCH (0x180003542 / 0x180003580).
//    When the refcount is still non-zero retail NULLS the member before
//    overwriting it two instructions later. The store is dead in every
//    single-threaded reading of this function. It is reproduced because it is
//    what the bytes do, and because a reader diffing this against the
//    disassembly must not find a "simplification" they then have to re-derive.
//  * THE FREE GOES THROUGH THE MEMBER, NOT A LOCAL. Retail writes
//    `ADD qword ptr [this+0x40],-8` and then loads that same member back as
//    MemFree's argument. Same value as `MemFree(old - 8)` would be, but it
//    leaves the member transiently pointing at the allocation base, and this
//    body reproduces the sequence rather than the shortcut.
//
// The only callee is `?MemFree@MemoryAllocator@System@@SAXPEAX@Z`
// (0x1800C7AD0, confirmed by pefile against the export directory), which this
// file already reaches through its existing MemFree door. No other call.
// ---------------------------------------------------------------------------
namespace {

// Retail's member-assign, written once and used twice so the two call sites
// cannot drift apart. `slot` is the MEMBER, taken by reference on purpose -
// see "the free goes through the member" above.
inline void RtUi_SpriteSheetAssignString(char*& slot, char* const* src) {
    char* old = slot;
    if (old == *src) return;                 // pointer identity short-circuit
    if (old) {
        if (--*reinterpret_cast<int32_t*>(old - 4) == 0) {
            slot -= 8;                       // the member is adjusted...
            // ...and then freed THROUGH the member, via the port-registry
            // door so the ported MemoryAllocator answers: every String block
            // here is allocated and freed through the SAME door, and mixing
            // doors inside one refcounted block is the hazard.
            using FreeFn = void (*)(void*);
            FreeFn memFree = RT_EXPORT_ONCE(FreeFn,
                "?MemFree@MemoryAllocator@System@@SAXPEAX@Z");
            memFree(slot);
        } else {
            slot = nullptr;                  // dead store, faithful - see banner
        }
    }
    slot = *src;
    if (slot) ++*reinterpret_cast<int32_t*>(slot - 4);
}

}  // namespace

namespace UI {

void SpriteSheet::Set(int32_t p1, int32_t p2, const System::String& p3,
                      const System::String& p4, bool p5) {
    // p4 -> +0x40 FIRST, then p3 -> +0x38. Retail's order, not source order.
    RtUi_SpriteSheetAssignString(m_groupName,   &p4._data);
    RtUi_SpriteSheetAssignString(m_textureName, &p3._data);

    // The three scalars, in retail's store order: +0x4C, then +0x50, then +0x48.
    m_field4C = p2;
    m_field50 = static_cast<uint8_t>(p5);
    m_field48 = p1;
}

}  // namespace UI

extern "C" void RtUi_SpriteSheet_Set(void* selfRaw, int p1, int p2,
                                     const void* p3Raw, const void* p4Raw,
                                     bool p5) {
    reinterpret_cast<UI::SpriteSheet*>(selfRaw)                            // B.3.6 bridge
        ->UI::SpriteSheet::Set(p1, p2, *static_cast<const System::String*>(p3Raw),
                               *static_cast<const System::String*>(p4Raw), p5);
}
int RtUiSpriteRegister() {
    int n = 0;

    RegisterRuntimeExport("??0Sprite@UI@@QEAA@XZ", (void*)&RtUi_Sprite_DefaultCtor); ++n;
    RegisterRuntimeExport("??0Sprite@UI@@QEAA@V?$ExternalPtr@VSpriteSheet@UI@@@Object@@AEBVString@System@@@Z",
                          (void*)&RtUi_Sprite_CtorByName); ++n;
    RegisterRuntimeExport("??0Sprite@UI@@QEAA@V?$ExternalPtr@VSpriteSheet@UI@@@Object@@H@Z",
                          (void*)&RtUi_Sprite_CtorByIndex); ++n;
    RegisterRuntimeExport("?IsValid@Sprite@UI@@QEBA_NXZ", (void*)&RtUi_Sprite_IsValid); ++n;
    RegisterRuntimeExport("?Width@Sprite@UI@@QEBAHXZ", (void*)&RtUi_Sprite_Width); ++n;
    RegisterRuntimeExport("?Height@Sprite@UI@@QEBAHXZ", (void*)&RtUi_Sprite_Height); ++n;

    RegisterRuntimeExport("??0SpriteSheet@UI@@QEAA@XZ", (void*)&RtUi_SpriteSheet_Ctor); ++n;
    RegisterRuntimeExport("?GetSpriteCount@SpriteSheet@UI@@QEBAHXZ", (void*)&RtUi_SpriteSheet_GetSpriteCount); ++n;
    RegisterRuntimeExport("?GetSpriteIndexFromName@SpriteSheet@UI@@QEBAHAEBVString@System@@@Z",
                          (void*)&RtUi_SpriteSheet_GetSpriteIndexFromName); ++n;
    RegisterRuntimeExport("?GetUVs@SpriteSheet@UI@@QEBAXHAEAM000@Z", (void*)&RtUi_SpriteSheet_GetUVs); ++n;
    // WAVE 23 LANE TAIL. Reached by name from runtime/src/save/saveload_draw.cpp.
    RegisterRuntimeExport("?Set@SpriteSheet@UI@@QEAAXHHAEBVString@System@@0_N@Z",
                          (void*)&RtUi_SpriteSheet_Set); ++n;
    RegisterRuntimeAddr(0x1800034f0, (void*)&RtUi_SpriteSheet_Set);
    // WAVE 10 LANE UI4 — the callee this wave's SpriteCapture::AddSprite opened.
    RegisterRuntimeExport("?GetSpriteNameFromIndex@SpriteSheet@UI@@QEBA?AVString@System@@H@Z",
                          (void*)&RtUi_SpriteSheet_GetSpriteNameFromIndex); ++n;
    RegisterRuntimeAddr(0x1800032d0, (void*)&RtUi_SpriteSheet_GetSpriteNameFromIndex);
    RegisterRuntimeExport("?SetSpriteInfo@SpriteSheet@UI@@QEAAXHHHMMMM@Z", (void*)&RtUi_SpriteSheet_SetSpriteInfo); ++n;
    RegisterRuntimeExport("?AddSpriteInfo@SpriteSheet@UI@@QEAAXAEBVString@System@@0HHHH@Z",
                          (void*)&RtUi_SpriteSheet_AddSpriteInfo); ++n;

    // SpriteContext::AddLine / SetScissor x2 -- NO LONGER HELD BACK. Ported by
    // WAVE 9 LANE UIF in runtime/src/ui/spritecontext.cpp, together with the
    // two unexported internals they call. Registered from here rather than
    // from a new slot in rt_ui_port.cpp because that file says, in its own
    // header, not to edit it to add a lane -- SpriteContext is lane F's row and
    // this keeps it there.
    //
    // The Wave 7 hold-back argument was about `Flush` (0x18011ba40), which
    // submits into the live GPU vertex buffer and is STILL held back. See
    // runtime/types/ui/SpriteContext.h for why that argument does not reach
    // the three exports, and docs/runtime_port/WAVE9_LANE_UIF2.md for the
    // cutover cost of the address door it leaves open.
    n += RtUiSpriteContextRegister();

    // WAVE 10 LANE UI4 -- UI::SpriteCapture::AddSprite / AddScissor, the two
    // rows the wave-9 census recorded UNREACHABLE. Registered from here for
    // the same reason SpriteContext is: rt_ui_port.cpp says not to edit it to
    // add a lane, and SpriteCapture is the far side of the same capture path.
    extern int RtUiSpriteCaptureRegister();
    n += RtUiSpriteCaptureRegister();

    RegisterRuntimeExport("?GetCount@StringTable@UI@@QEBAHXZ", (void*)&RtUi_StringTable_GetCount); ++n;
    RegisterRuntimeExport("?GetText@StringTable@UI@@QEBA?AVString@System@@H@Z",
                          (void*)&RtUi_StringTable_GetText_Index); ++n;
    // Wave 9 lane B3 -- a REAL DEPENDENCY row (WAVE9_LANE_V.md §1b).
    RegisterRuntimeExport("?GetIndexFromName@StringTable@UI@@QEBAHAEBVString@System@@@Z",
                          (void*)&RtUi_StringTable_GetIndexFromName); ++n;
    RegisterRuntimeExport("?GetText@StringTable@UI@@QEBA?AVString@System@@AEBV34@@Z",
                          (void*)&RtUi_StringTable_GetText_Key); ++n;

    RegisterRuntimeExport("??0String@UI@@QEAA@XZ", (void*)&RtUi_UIString_DefaultCtor); ++n;
    RegisterRuntimeExport("??0String@UI@@QEAA@V?$ExternalPtr@VStringTable@UI@@@Object@@AEBV0System@@@Z",
                          (void*)&RtUi_UIString_CtorByName); ++n;
    RegisterRuntimeExport("??0String@UI@@QEAA@V?$ExternalPtr@VStringTable@UI@@@Object@@H@Z",
                          (void*)&RtUi_UIString_CtorByIndex); ++n;
    RegisterRuntimeExport("?IsValid@String@UI@@QEBA_NXZ", (void*)&RtUi_UIString_IsValid); ++n;
    RegisterRuntimeExport("??BString@UI@@QEBA?AV0System@@XZ", (void*)&RtUi_UIString_ToSystemString); ++n;

    return n;
}
