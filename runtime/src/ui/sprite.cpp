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
// replay*  = replay into a scratch retval, THEN release the extra refcounted
//            reference the replay created (see ReleaseStringRef below), so
//            the net effect on shared state is zero even in diff mode.
// scratch  = byte-compare against an isolated scratch replica (§ below).
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
// TWO ORACLES
// ---------------------------------------------------------------------------
// (a) REPLAY, for every pure read and every ctor. None of these functions has
//     a side effect beyond writing to `this`/the return slot (the ctors that
//     call SpriteSheet::GetSpriteIndexFromName / StringTable::GetIndexFromName
//     only read the table, never mutate it), so calling both implementations
//     on the same inputs and comparing is always safe. Sprite/UIString ctors
//     compare only the first 0x0C bytes of the 0x10-byte object: `pad0C` is
//     never written by any ctor (Sprite.h / String.h), and a diff oracle must
//     not compare a byte neither side writes
//     (reference_oracle_must_not_compare_unwritten_bytes.md) or it reports a
//     divergence on whatever garbage was already in the caller's storage.
//     For the three String-returning functions (marked replay* above), the
//     replay's second call legitimately bumps a live String's refcount as a
//     side effect of "return an owned reference" — ReleaseStringRef undoes
//     exactly that bump afterward, so the net effect is zero, unlike a
//     one-way leak.
//
// (b) SCRATCH REPLICA, for SetSpriteInfo and AddSpriteInfo, which mutate a
//     SpriteInfo array that lives in a SEPARATE heap block from the
//     SpriteSheet object itself (a Collection::Array<SpriteInfo>, per
//     PLAN §4.3 risk 1) — snapshotting the 0x58-byte SpriteSheet object does
//     not snapshot the array's contents. Both build an isolated scratch copy
//     of the ONE array element that changes, run our body on the REAL
//     object, run retail's exported body on the scratch copy with the exact
//     same inputs, and RT_DIFF_BYTES the result. AddSpriteInfo also bumps a
//     shared String's refcount as part of "append a new owned reference";
//     the code below explicitly releases that one reference afterward for
//     the same zero-net-effect reason as (a). See the function for why the
//     scratch array is pre-sized to avoid touching any OTHER entry's
//     refcount during the replay.
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

#include "../../funcs/rt_port.h"
#include "../../types/ui/Sprite.h"
#include "../../types/ui/SpriteSheet.h"
#include "../../types/ui/StringTable.h"
#include "../../types/ui/String.h"
#include "../../types/system/String.h"
#include "../system/string.h"   // RtString_StrCompare, for GetIndexFromName

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
// Release one reference to a refcounted System::String buffer, reproducing
// the idiom documented in runtime/types/system/String.h ("RETAIL'S RELEASE
// IDIOM"): decrement the refcount; on reaching zero, the block starts 8
// bytes before `data` and is freed through the real MemoryAllocator.
// Used only to undo a reference OUR OWN diff replay created — every call site
// below is inside `if (rt_diff_on__)`.
// ----------------------------------------------------------------------------
void ReleaseStringRef(char* data) {
    if (!data) return;
    int32_t& rc = RtSystem::StringRefCount(data);
    if (--rc == 0) {
        using FreeFn = void (*)(void*);
        FreeFn memFree = RT_ORIGINAL_ONCE(FreeFn, "?MemFree@MemoryAllocator@System@@SAXPEAX@Z");
        memFree(data - 8);
    }
}

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
// UI::Sprite
// ============================================================================

// Sprite::Sprite() — retail 0x18011ba10. Writes only +0x00 (qword, null) and
// +0x08 (dword, -1); pad0C is never touched.
void* RtUi_Sprite_DefaultCtor(void* retval) {
    RT_DIFF_BEGIN("UI::Sprite::Sprite()");
    auto* self = static_cast<Sprite*>(retval);
    self->spriteSheet = nullptr;
    self->spriteIndex = -1;

    if (rt_diff_on__) {
        using Fn = void* (*)(void*);
        uint8_t scratch[sizeof(Sprite)];
        std::memset(scratch, 0xCD, sizeof(scratch));
        RT_ORIGINAL_ONCE(Fn, "??0Sprite@UI@@QEAA@XZ")(scratch);
        RT_DIFF_BYTES(self, scratch, kSpriteWrittenBytes);
    }
    return retval;
}

// Sprite::Sprite(ExternalPtr<SpriteSheet>, String const& name) — retail
// 0x18011b9c0. `sheet` arrives as a raw pointer value (ExternalPtr<T> is one
// pointer, PLAN §2.2 / External.h); `name` is a System::String const& (its
// data pointer is the first and only field, per String.h).
void* RtUi_Sprite_CtorByName(void* retval, uint64_t spriteSheetPtr, const void* spriteName) {
    RT_DIFF_BEGIN("UI::Sprite::Sprite(sheet,name)");
    // WAVE 10 LANE UI4's probe trigger — the EARLIEST live site in this file,
    // measured: it fires while the front-end dialogs are built, so it works in
    // a menu-only cell as well as in-game. (The first attempt hung the trigger
    // on GetUVs, which is per-frame IN GAME only, and a BANISHED_AUTOQUIT run
    // never gets there — AUTOQUIT drives menu -> Quit INSTEAD of menu -> New
    // Game, so it overrides AUTOSTART. GetUVs keeps its trigger too; whichever
    // fires first wins.) One bool test, and only when the diff harness is on.
    if (rt_diff_on__) { extern void RtUiSpriteCaptureProbe(); RtUiSpriteCaptureProbe(); }
    auto* self = static_cast<Sprite*>(retval);
    auto* sheet = reinterpret_cast<SpriteSheet*>(spriteSheetPtr);
    self->spriteSheet = sheet;
    if (sheet != nullptr) {
        const char* key = *reinterpret_cast<char* const*>(spriteName);
        self->spriteIndex = SpriteIndexFromName(sheet->m_sprites, sheet->m_spriteCount, key);
    } else {
        self->spriteIndex = -1;
    }

    if (rt_diff_on__) {
        using Fn = void* (*)(void*, uint64_t, const void*);
        uint8_t scratch[sizeof(Sprite)];
        std::memset(scratch, 0xCD, sizeof(scratch));
        RT_ORIGINAL_ONCE(Fn, "??0Sprite@UI@@QEAA@V?$ExternalPtr@VSpriteSheet@UI@@@Object@@AEBVString@System@@@Z")(
            scratch, spriteSheetPtr, spriteName);
        RT_DIFF_BYTES(self, scratch, kSpriteWrittenBytes);
    }
    return retval;
}

// Sprite::Sprite(ExternalPtr<SpriteSheet>, int index) — retail 0x18011ba30.
// Two plain stores, no branch.
void* RtUi_Sprite_CtorByIndex(void* retval, void* spriteSheetPtr, int index) {
    RT_DIFF_BEGIN("UI::Sprite::Sprite(sheet,index)");
    auto* self = static_cast<Sprite*>(retval);
    self->spriteSheet = spriteSheetPtr;
    self->spriteIndex = index;

    if (rt_diff_on__) {
        using Fn = void* (*)(void*, void*, int);
        uint8_t scratch[sizeof(Sprite)];
        std::memset(scratch, 0xCD, sizeof(scratch));
        RT_ORIGINAL_ONCE(Fn, "??0Sprite@UI@@QEAA@V?$ExternalPtr@VSpriteSheet@UI@@@Object@@H@Z")(
            scratch, spriteSheetPtr, index);
        RT_DIFF_BYTES(self, scratch, kSpriteWrittenBytes);
    }
    return retval;
}

// Sprite::IsValid() const — retail 0x180030c40. Order matches retail exactly
// (index checked first, then sheet) even though it is commutative, per
// CLAUDE.md's byte-faithful-first rule.
bool RtUi_Sprite_IsValid(const void* spriteRaw) {
    RT_DIFF_BEGIN("UI::Sprite::IsValid()");
    const auto* self = static_cast<const Sprite*>(spriteRaw);
    const bool ours = (self->spriteIndex >= 0) && (self->spriteSheet != nullptr);

    if (rt_diff_on__) {
        using Fn = bool (*)(const void*);
        RT_DIFF_SCALAR(ours, RT_ORIGINAL_ONCE(Fn, "?IsValid@Sprite@UI@@QEBA_NXZ")(spriteRaw));
    }
    return ours;
}

// Sprite::Width() const — retail 0x180030bf0. NO upper-bound check on
// spriteIndex, exactly as retail has none (an out-of-range positive index
// reads whatever follows the array in memory) — CLAUDE.md: do not add a
// guard retail lacks.
int RtUi_Sprite_Width(const void* spriteRaw) {
    RT_DIFF_BEGIN("UI::Sprite::Width()");
    const auto* self = static_cast<const Sprite*>(spriteRaw);
    int ours = 0;
    if (self->spriteIndex >= 0) {
        const auto* sheet = static_cast<const SpriteSheet*>(self->spriteSheet);
        ours = sheet->m_sprites[self->spriteIndex].width;
    }

    if (rt_diff_on__) {
        using Fn = int (*)(const void*);
        RT_DIFF_SCALAR(ours, RT_ORIGINAL_ONCE(Fn, "?Width@Sprite@UI@@QEBAHXZ")(spriteRaw));
    }
    return ours;
}

// Sprite::Height() const — retail 0x180030c10. Same no-bound-check note.
int RtUi_Sprite_Height(const void* spriteRaw) {
    RT_DIFF_BEGIN("UI::Sprite::Height()");
    const auto* self = static_cast<const Sprite*>(spriteRaw);
    int ours = 0;
    if (self->spriteIndex >= 0) {
        const auto* sheet = static_cast<const SpriteSheet*>(self->spriteSheet);
        ours = sheet->m_sprites[self->spriteIndex].height;
    }

    if (rt_diff_on__) {
        using Fn = int (*)(const void*);
        RT_DIFF_SCALAR(ours, RT_ORIGINAL_ONCE(Fn, "?Height@Sprite@UI@@QEBAHXZ")(spriteRaw));
    }
    return ours;
}

// ============================================================================
// UI::SpriteSheet
// ============================================================================

// SpriteSheet::SpriteSheet() — retail 0x18011d600. Stamps Object::Base's
// vtable, then Object::External's, zeroes External's three fields, stamps
// SpriteSheet's own (EXPORTED, ??_7SpriteSheet@UI@@6B@ — PLAN §2.2 item D),
// then zeroes the rest of the object. The two intermediate vtable stores are
// dead by the time the ctor returns but are kept, matching the
// Achievement::Achievement precedent (runtime/src/achievement/
// achievement_manager.cpp): they ARE the body, and a debugger break between
// them would observe the intermediate value.
void* RtUi_SpriteSheet_Ctor(void* selfRaw) {
    RT_DIFF_BEGIN("UI::SpriteSheet::SpriteSheet()");
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
    qword(self, 0x20) = nullptr;
    qword(self, 0x28) = nullptr;   // m_spriteCount + m_spriteCapacity (one qword store)
    qword(self, 0x30) = nullptr;
    qword(self, 0x38) = nullptr;
    qword(self, 0x40) = nullptr;
    qword(self, 0x48) = nullptr;   // m_field48 + m_field4C
    self[0x50] = 0;                // single byte; +0x51..+0x57 are compiler padding, not retail stores

    if (rt_diff_on__) {
        using CtorFn = void* (*)(void*);
        alignas(16) uint8_t scratch[sizeof(SpriteSheet)];
        std::memset(scratch, 0xCD, sizeof(scratch));
        RT_ORIGINAL_ONCE(CtorFn, "??0SpriteSheet@UI@@QEAA@XZ")(scratch);
        // kSpriteSheetWrittenBytes (0x51), NOT sizeof(SpriteSheet) (0x58): the
        // ctor never touches +0x51..+0x57 (SpriteSheet.h), and comparing
        // those 7 bytes reports a divergence on whatever the allocator left
        // there -- unrelated between the real object and this scratch buffer.
        // Wave 7 integration run, 2026-09-07: 100% divergence at +0x51 until
        // this fix (see docs/runtime_port/WAVE7_LANE_F.md §9).
        RT_DIFF_BYTES(self, scratch, kSpriteSheetWrittenBytes);
    }
    return selfRaw;
}

// SpriteSheet::GetSpriteCount() const — retail 0x180003190.
int RtUi_SpriteSheet_GetSpriteCount(const void* selfRaw) {
    RT_DIFF_BEGIN("UI::SpriteSheet::GetSpriteCount()");
    const auto* self = static_cast<const SpriteSheet*>(selfRaw);
    const int ours = self->m_spriteCount;

    if (rt_diff_on__) {
        using Fn = int (*)(const void*);
        RT_DIFF_SCALAR(ours, RT_ORIGINAL_ONCE(Fn, "?GetSpriteCount@SpriteSheet@UI@@QEBAHXZ")(selfRaw));
    }
    return ours;
}

// SpriteSheet::GetSpriteIndexFromName(String const&) const — retail 0x1800031f0.
int RtUi_SpriteSheet_GetSpriteIndexFromName(const void* selfRaw, const void* nameStrPtr) {
    RT_DIFF_BEGIN("UI::SpriteSheet::GetSpriteIndexFromName()");
    const auto* self = static_cast<const SpriteSheet*>(selfRaw);
    const char* key = *reinterpret_cast<char* const*>(nameStrPtr);
    const int ours = SpriteIndexFromName(self->m_sprites, self->m_spriteCount, key);

    if (rt_diff_on__) {
        using Fn = int (*)(const void*, const void*);
        RT_DIFF_SCALAR(ours, RT_ORIGINAL_ONCE(
            Fn, "?GetSpriteIndexFromName@SpriteSheet@UI@@QEBAHAEBVString@System@@@Z")(selfRaw, nameStrPtr));
    }
    return ours;
}

// SpriteSheet::GetSpriteNameFromIndex(int) const — retail 0x1800032d0.
//
// WAVE 10 LANE UI4. Not part of Wave 7's SpriteSheet set; it is here because
// this lane's port of SpriteCapture::AddSprite CALLS it, and porting AddSprite
// therefore turned a symbol nothing in the tree referenced into a NEW unported
// census row. That growth is the documented shape of
// reference_porting_can_grow_the_cutover_surface.md — the answer is to close
// the callee, not to route around it.
//
// Eighteen instructions, and NO bounds check of any kind: the index is scaled
// straight into `m_sprites` by `IMUL RAX,RAX,0x38`. Same faithfulness call as
// GetUVs below, and CLAUDE.md's rule — do not add a guard retail lacks.
//
// The return is a System::String BY VALUE, so the ABI is
// (this, hidden-return-slot, index) with the slot handed back in RAX. Getting
// that arity wrong is reference_hidden_retslot_wild_write.md's failure mode.
void* RtUi_SpriteSheet_GetSpriteNameFromIndex(const void* selfRaw, void* retval, int index) {
    RT_DIFF_BEGIN("UI::SpriteSheet::GetSpriteNameFromIndex()");
    const auto* self = static_cast<const SpriteSheet*>(selfRaw);

    auto* ret = static_cast<RtSystem::String*>(retval);
    ret->data = nullptr;                       // 0x1800032f4, before the load
    char* name = self->m_sprites[index].name;
    if (name) {
        ret->data = name;
        RtSystem::StringRefCount(name) += 1;   // 0x180003302
    }

    if (rt_diff_on__) {
        // Replay into a private slot. The second call legitimately bumps the
        // same live String again — "return an owned reference" — and
        // ReleaseStringRef undoes exactly that bump, so the net effect is
        // zero, unlike a one-way leak. Identical to the three replay* rows in
        // this file's banner.
        using Fn = void* (*)(const void*, void*, int);
        RtSystem::String scratch{};
        RT_ORIGINAL_ONCE(Fn,
            "?GetSpriteNameFromIndex@SpriteSheet@UI@@QEBA?AVString@System@@H@Z")(
                selfRaw, &scratch, index);
        RT_DIFF_BYTES(&ret->data, &scratch.data, sizeof(char*));
        ReleaseStringRef(scratch.data);
    }
    return retval;                             // retail returns the slot in RAX
}

// SpriteSheet::GetUVs(int, float&, float&, float&, float&) const — retail
// 0x180003330. NO upper-bound check (only `index < 0` is guarded), matching
// retail exactly — CLAUDE.md: do not add a guard retail lacks.
void RtUi_SpriteSheet_GetUVs(const void* selfRaw, int index, float* u0, float* v0, float* u1, float* v1) {
    RT_DIFF_BEGIN("UI::SpriteSheet::GetUVs()");
    // WAVE 10 LANE UI4's probe trigger. This is a live per-frame call site
    // (the status-icon geometry builder), which is what makes it a point past
    // Engine::Startup — the MemoryAllocator singleton the SpriteCapture probe
    // needs does not exist at registration time. Costs one bool test, and only
    // when the diff harness is on; see runtime/src/ui/spritecapture_probe.cpp.
    if (rt_diff_on__) { extern void RtUiSpriteCaptureProbe(); RtUiSpriteCaptureProbe(); }
    const auto* self = static_cast<const SpriteSheet*>(selfRaw);
    float ou0 = 0.0f, ov0 = 0.0f, ou1 = 0.0f, ov1 = 0.0f;
    if (index >= 0) {
        const SpriteInfo& e = self->m_sprites[index];
        ou0 = e.u0; ov0 = e.v0; ou1 = e.u1; ov1 = e.v1;
    }
    *u0 = ou0; *v0 = ov0; *u1 = ou1; *v1 = ov1;

    if (rt_diff_on__) {
        using Fn = void (*)(const void*, int, float*, float*, float*, float*);
        float tu0 = 0, tv0 = 0, tu1 = 0, tv1 = 0;
        RT_ORIGINAL_ONCE(Fn, "?GetUVs@SpriteSheet@UI@@QEBAXHAEAM000@Z")(selfRaw, index, &tu0, &tv0, &tu1, &tv1);
        const float ours[4]   = { ou0, ov0, ou1, ov1 };
        const float theirs[4] = { tu0, tv0, tu1, tv1 };
        RT_DIFF_BYTES(ours, theirs, sizeof(ours));
    }
}

// SpriteSheet::SetSpriteInfo(int,int,int,float,float,float,float) — retail
// 0x180003460. Mutates ONE existing SpriteInfo entry in place; no allocation,
// no refcounted field touched, so the whole 0x38-byte entry is a clean
// scratch-replica byte oracle. No bound check on `index`, matching retail.
void RtUi_SpriteSheet_SetSpriteInfo(void* selfRaw, int index, int width, int height,
                                     float u0, float v0, float u1, float v1) {
    RT_DIFF_BEGIN("UI::SpriteSheet::SetSpriteInfo()");
    auto* self = static_cast<SpriteSheet*>(selfRaw);
    SpriteInfo& e = self->m_sprites[index];

    // Scratch replica of the PRE-call entry, snapshotted before we mutate the
    // real one, so retail's body can be replayed against it afterward. Only
    // needed inside diff mode -- costs nothing in a normal run.
    SpriteInfo before;
    if (rt_diff_on__) before = e;

    e.width = width; e.height = height;
    e.u0 = u0; e.v0 = v0; e.u1 = u1; e.v1 = v1;

    if (rt_diff_on__) {
        // Address the scratch entry through a fake 0x28-byte header whose
        // +0x20 is the ONLY field SetSpriteInfo reads
        // (`this + index*0x38` off `*(this+0x20)`) -- built so that
        // `fakeSheet + index*0x38` off its own +0x20 lands back on
        // `&before`, exactly reproducing the real object's addressing
        // without needing a vtable, filename or count (SetSpriteInfo reads
        // none of those).
        uint8_t fakeSheet[0x28] = {};
        *reinterpret_cast<SpriteInfo**>(fakeSheet + 0x20) =
            reinterpret_cast<SpriteInfo*>(reinterpret_cast<uint8_t*>(&before) - static_cast<size_t>(index) * sizeof(SpriteInfo));

        using Fn = void (*)(void*, int, int, int, float, float, float, float);
        RT_ORIGINAL_ONCE(Fn, "?SetSpriteInfo@SpriteSheet@UI@@QEAAXHHHMMMM@Z")(
            fakeSheet, index, width, height, u0, v0, u1, v1);
        RT_DIFF_BYTES(&e, &before, sizeof(SpriteInfo));
    }
}

// SpriteSheet::AddSpriteInfo(String const&,String const&,int,int,int,int) —
// retail 0x180003390. See the file header for the scratch-replica oracle
// design and for why this does not need retail's own pre-bump/post-release
// dance around the new element's refcounts (the growth helper's own
// copy-construct step supplies the ONE bump the caller-visible contract
// needs; the dance nets to the same +1 either way — verified by hand against
// the decompile of both 0x180003390 and its internal helper 0x180004e30).
void RtUi_SpriteSheet_AddSpriteInfo(void* selfRaw, const void* nameStrPtr, const void* groupStrPtr,
                                     int x, int y, int width, int height) {
    RT_DIFF_BEGIN("UI::SpriteSheet::AddSpriteInfo()");
    auto* self = static_cast<SpriteSheet*>(selfRaw);

    const SpriteInfo* oldData  = self->m_sprites;
    const int32_t     oldCount = self->m_spriteCount;

    SpriteInfo newEntry{};
    newEntry.name        = *reinterpret_cast<char* const*>(nameStrPtr);
    newEntry.group       = *reinterpret_cast<char* const*>(groupStrPtr);
    newEntry.x           = x;
    newEntry.y           = y;
    newEntry._reserved18 = 0;
    newEntry.width       = width;
    newEntry.height      = height;
    newEntry.u0 = newEntry.v0 = newEntry.u1 = newEntry.v1 = 0.0f;
    newEntry._pad34      = 0;

    // Delegate the array mechanics (grow-if-needed, copy-construct with
    // refcount bump, free the old buffer) to Collection::Array<SpriteInfo>'s
    // append-by-one body, which follows the growth policy documented in
    // runtime/types/collection/Collection.h.
    //
    // *** WAVE 9 LANE Q, 2026-09-07: this is no longer "retail's own unexported
    // helper". *** 0x180004e30 is a registered port (kSpec_Sprite38 in
    // runtime/src/collection/array.cpp), one of the three NON-TRIVIAL element
    // types in that lane. It still calls retail's per-element copy-assign at
    // 0x180004220 by address, so the ONE refcount bump the note above relies on
    // is unchanged -- but the container algebra around it is ours now, and it
    // can be disarmed on its own with BANISHED_RT_ARRAY_NONTRIVIAL=0.
    using GrowFn = void (*)(void* arrayDesc, int newCount, void* newElem);
    static GrowFn grow = reinterpret_cast<GrowFn>(ResolveAddr(Module::Runtime, 0x180004e30));
    grow(&self->m_sprites, oldCount + 1, &newEntry);

    if (rt_diff_on__) {
        using AllocFn = void* (*)(uint64_t, int);
        using FreeFn  = void  (*)(void*);
        AllocFn memAlloc = RT_ORIGINAL_ONCE(AllocFn, "?MemAlloc@MemoryAllocator@System@@SAPEAX_KH@Z");
        FreeFn  memFree  = RT_ORIGINAL_ONCE(FreeFn,  "?MemFree@MemoryAllocator@System@@SAXPEAX@Z");

        // Pre-size the scratch array's CAPACITY to oldCount+1 so retail's
        // AddSpriteInfo takes the no-realloc path and touches ONLY the new
        // slot — if it reallocated, its growth helper would copy-construct
        // (and refcount-bump) every OLD entry too, which this oracle would
        // then have to un-bump one by one for no benefit: the old entries
        // are not what changed and are not what this call is verifying.
        SpriteInfo* scratchData = static_cast<SpriteInfo*>(
            memAlloc(static_cast<uint64_t>(oldCount + 1) * sizeof(SpriteInfo), 8));
        if (oldCount > 0) std::memcpy(scratchData, oldData, static_cast<size_t>(oldCount) * sizeof(SpriteInfo));

        uint8_t fakeSheet[0x30] = {};
        *reinterpret_cast<SpriteInfo**>(fakeSheet + 0x20) = scratchData;
        *reinterpret_cast<int32_t*>(fakeSheet + 0x28)     = oldCount;
        *reinterpret_cast<int32_t*>(fakeSheet + 0x2c)     = oldCount + 1;

        using AddFn = void (*)(void*, const void*, const void*, int, int, int, int);
        RT_ORIGINAL_ONCE(AddFn, "?AddSpriteInfo@SpriteSheet@UI@@QEAAXAEBVString@System@@0HHHH@Z")(
            fakeSheet, nameStrPtr, groupStrPtr, x, y, width, height);

        SpriteInfo* theirsData = *reinterpret_cast<SpriteInfo**>(fakeSheet + 0x20);
        const SpriteInfo& oursAppended   = self->m_sprites[oldCount];
        const SpriteInfo& theirsAppended = theirsData[oldCount];
        // kSpriteInfoWrittenBytes (0x34), NOT sizeof(SpriteInfo) (0x38): the
        // shared copy-construct helper (0x180004220, both implementations go
        // through it) never touches +0x34..+0x37, and `oursAppended` /
        // `theirsAppended` live in two UNRELATED allocations (the real array
        // vs. this call's scratch replica), so their unwritten tails have no
        // reason to agree. Wave 7 integration run, 2026-09-07: 100%
        // divergence at +0x34 until this fix (WAVE7_LANE_F.md §9).
        RT_DIFF_BYTES(&oursAppended, &theirsAppended, kSpriteInfoWrittenBytes);

        // Undo the ONE reference retail's replay left on the shared strings
        // (the scratch array is discarded here) so the net effect of running
        // the oracle is zero, not a one-time leak.
        ReleaseStringRef(theirsAppended.name);
        ReleaseStringRef(theirsAppended.group);
        memFree(theirsData);
    }
}

// ============================================================================
// UI::StringTable
// ============================================================================

// StringTable::GetCount() const — retail 0x180002610.
int RtUi_StringTable_GetCount(const void* selfRaw) {
    RT_DIFF_BEGIN("UI::StringTable::GetCount()");
    const auto* base = static_cast<const uint8_t*>(selfRaw);
    const int ours = *reinterpret_cast<const int32_t*>(base + StringTableOffsets::kCount);

    if (rt_diff_on__) {
        using Fn = int (*)(const void*);
        RT_DIFF_SCALAR(ours, RT_ORIGINAL_ONCE(Fn, "?GetCount@StringTable@UI@@QEBAHXZ")(selfRaw));
    }
    return ours;
}

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
// ACCESS_VIOLATION reading 0x20.
//
// FOUND BY: porting TextElement::CacheText (Wave 10 lane UIORACLE) — the first
// caller in the tree to route a never-assigned UI::String through this port
// rather than through the DLL's own copy. A four-cell A/B on the trading-post
// panel route isolated it: default CRASHES, `BANISHED_RT_UIWIDGETSLAY=0`
// (CacheText back on the DLL) clean, `BANISHED_RT_UISPRITE=0` (this body back
// on the DLL) clean, `BANISHED_RT_UI=0` clean. Two independent switches, each
// removing exactly one half of the pair, each making it go away.
//
// AND THIS IS NOT THE "don't fix a faithful crash with a null check" case
// (CLAUDE.md). No guard is being ADDED: retail's guards are being put back in
// retail's ORDER. The upstream null is not a defect to hunt — it is the normal
// representation of "no text", and retail is built to tolerate it here.
void* RtUi_StringTable_GetText_Index(const void* selfRaw, void* retval, int index) {
    RT_DIFF_BEGIN("UI::StringTable::GetText(int)");
    const auto* base = static_cast<const uint8_t*>(selfRaw);

    auto* ret = static_cast<RtSystem::String*>(retval);
    ret->data = nullptr;
    if (index >= 0) {                                            // 0x18000256c
        const int count = *reinterpret_cast<const int32_t*>(     // 0x180002573
            base + StringTableOffsets::kCount);
        if (index < count) {
            const auto* data = *reinterpret_cast<StringTableTextEntry* const*>(
                base + StringTableOffsets::kData);               // 0x180002585
            char* text = data[index].text;
            if (text) {
                RtSystem::StringRefCount(text) += 1;
                ret->data = text;
            }
        }
    }

    if (rt_diff_on__) {
        using Fn = void* (*)(const void*, void*, int);
        RtSystem::String scratch{};
        RT_ORIGINAL_ONCE(Fn, "?GetText@StringTable@UI@@QEBA?AVString@System@@H@Z")(selfRaw, &scratch, index);
        RT_DIFF_BYTES(&ret->data, &scratch.data, sizeof(char*));
        ReleaseStringRef(scratch.data);
    }
    return retval;
}

// ?GetIndexFromName@StringTable@UI@@QEBAHAEBVString@System@@@Z  retail 0x180002470
//
// Wave 9 lane B3. WAVE7_LANE_F left this on the DLL as out of scope; it is one
// of WAVE9_LANE_V.md's REAL DEPENDENCY rows (this file, :581 and :629), so at
// cutover the two call sites below would be calls through null.
//
// A linear scan of the table's `StringTableTextEntry` array (stride 0x10, data
// at +0x20, count at +0x28 — StringTable.h), comparing `entry.name` against
// `key`. Two details from the listing that a paraphrase loses:
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
int RtUi_StringTable_GetIndexFromName(const void* selfRaw, const void* keyRaw) {
    RT_DIFF_BEGIN("UI::StringTable::GetIndexFromName");

    const auto* self = static_cast<const uint8_t*>(selfRaw);
    const auto* entries = *reinterpret_cast<StringTableTextEntry* const*>(
        self + StringTableOffsets::kData);
    const int count = *reinterpret_cast<const int32_t*>(self + StringTableOffsets::kCount);
    const char* key = *static_cast<const char* const*>(keyRaw);
    const int keyLen = (key != nullptr) ? RtSystem::StringLength(key) : 0;

    int result = -1;
    for (int i = 0; i < count; ++i) {
        const char* name = entries[i].name;
        const int nameLen = (name != nullptr) ? RtSystem::StringLength(name) : 0;
        if (nameLen != keyLen) continue;
        if (name == nullptr || nameLen == 0) { result = i; break; }
        if (key == nullptr) continue;                 // retail: ECX = key+1, non-zero
        if (RtSystem::RtString_StrCompare(name, key) == 0) { result = i; break; }
    }

    // Fully diffable: `this` and the key are both const and nothing here
    // allocates, so the shipped body can be run on the identical inputs.
    using Fn = int (*)(const void*, const void*);
    RT_DIFF_SCALAR(result, RtOriginal<Fn>(
        "?GetIndexFromName@StringTable@UI@@QEBAHAEBVString@System@@@Z")(selfRaw, keyRaw));
    return result;
}

// StringTable::GetText(String const& key) const — retail 0x1800025b0. A
// two-line forwarder in retail: `GetText(this, retval, GetIndexFromName(key))`.
void* RtUi_StringTable_GetText_Key(const void* selfRaw, void* retval, const void* keyStrPtr) {
    RT_DIFF_BEGIN("UI::StringTable::GetText(String const&)");
    // Direct call, not RT_ORIGINAL_ONCE: GetIndexFromName is ported above, in
    // this same translation unit.
    const int index = RtUi_StringTable_GetIndexFromName(selfRaw, keyStrPtr);
    RtUi_StringTable_GetText_Index(selfRaw, retval, index);

    if (rt_diff_on__) {
        using Fn = void* (*)(const void*, void*, const void*);
        RtSystem::String scratch{};
        RT_ORIGINAL_ONCE(Fn, "?GetText@StringTable@UI@@QEBA?AVString@System@@AEBV34@@Z")(
            selfRaw, &scratch, keyStrPtr);
        auto* ret = static_cast<RtSystem::String*>(retval);
        RT_DIFF_BYTES(&ret->data, &scratch.data, sizeof(char*));
        ReleaseStringRef(scratch.data);
    }
    return retval;
}

// ============================================================================
// UI::String
// ============================================================================

// UI::String::String() — retail 0x18011da40. Same shape as Sprite's default
// ctor (table=null, index=-1); pad0C never written.
void* RtUi_UIString_DefaultCtor(void* retval) {
    RT_DIFF_BEGIN("UI::String::String()");
    auto* self = static_cast<UIString*>(retval);
    self->table = nullptr;
    self->index = -1;

    if (rt_diff_on__) {
        using Fn = void* (*)(void*);
        uint8_t scratch[sizeof(UIString)];
        std::memset(scratch, 0xCD, sizeof(scratch));
        RT_ORIGINAL_ONCE(Fn, "??0String@UI@@QEAA@XZ")(scratch);
        RT_DIFF_BYTES(self, scratch, kUIStringWrittenBytes);
    }
    return retval;
}

// UI::String::String(ExternalPtr<StringTable>, String const& name) — retail
// 0x18011da60. StringTable::GetIndexFromName is out of this lane's scope
// (same reasoning as GetText(String const&) above): pure read, called
// through the DLL's own implementation.
void* RtUi_UIString_CtorByName(void* retval, uint64_t stringTablePtr, const void* nameStrPtr) {
    RT_DIFF_BEGIN("UI::String::String(table,name)");
    auto* self = static_cast<UIString*>(retval);
    self->table = reinterpret_cast<void*>(stringTablePtr);
    if (stringTablePtr != 0) {
        self->index = RtUi_StringTable_GetIndexFromName(
            reinterpret_cast<const void*>(stringTablePtr), nameStrPtr);
    } else {
        self->index = -1;
    }

    if (rt_diff_on__) {
        using Fn = void* (*)(void*, uint64_t, const void*);
        uint8_t scratch[sizeof(UIString)];
        std::memset(scratch, 0xCD, sizeof(scratch));
        RT_ORIGINAL_ONCE(Fn, "??0String@UI@@QEAA@V?$ExternalPtr@VStringTable@UI@@@Object@@AEBV0System@@@Z")(
            scratch, stringTablePtr, nameStrPtr);
        RT_DIFF_BYTES(self, scratch, kUIStringWrittenBytes);
    }
    return retval;
}

// UI::String::String(ExternalPtr<StringTable>, int index) — retail
// 0x18011da20. Two plain stores.
void* RtUi_UIString_CtorByIndex(void* retval, uint64_t stringTablePtr, int index) {
    RT_DIFF_BEGIN("UI::String::String(table,index)");
    auto* self = static_cast<UIString*>(retval);
    self->table = reinterpret_cast<void*>(stringTablePtr);
    self->index = index;

    if (rt_diff_on__) {
        using Fn = void* (*)(void*, uint64_t, int);
        uint8_t scratch[sizeof(UIString)];
        std::memset(scratch, 0xCD, sizeof(scratch));
        RT_ORIGINAL_ONCE(Fn, "??0String@UI@@QEAA@V?$ExternalPtr@VStringTable@UI@@@Object@@H@Z")(
            scratch, stringTablePtr, index);
        RT_DIFF_BYTES(self, scratch, kUIStringWrittenBytes);
    }
    return retval;
}

// UI::String::IsValid() const — retail 0x180035060. Order is TABLE-first,
// INDEX-second (the opposite short-circuit order from Sprite::IsValid) —
// preserved even though it is commutative, per CLAUDE.md.
bool RtUi_UIString_IsValid(const void* selfRaw) {
    RT_DIFF_BEGIN("UI::String::IsValid()");
    const auto* self = static_cast<const UIString*>(selfRaw);
    const bool ours = (self->table != nullptr) && (self->index >= 0);

    if (rt_diff_on__) {
        using Fn = bool (*)(const void*);
        RT_DIFF_SCALAR(ours, RT_ORIGINAL_ONCE(Fn, "?IsValid@String@UI@@QEBA_NXZ")(selfRaw));
    }
    return ours;
}

// UI::String::operator System::String() const — retail 0x180035020. A
// ONE-LINE forwarder: `return table->GetText(index)`, with `table` read
// UNGUARDED — if table is null, retail itself faults dereferencing it, and
// so does RtUi_StringTable_GetText_Index below (it reads `*(table+0x20)`).
// No null check retail lacks (CLAUDE.md).
void* RtUi_UIString_ToSystemString(const void* selfRaw, void* retval) {
    RT_DIFF_BEGIN("UI::String::operator System::String()");
    const auto* self = static_cast<const UIString*>(selfRaw);
    RtUi_StringTable_GetText_Index(self->table, retval, self->index);

    if (rt_diff_on__) {
        using Fn = void* (*)(const void*, void*);
        RtSystem::String scratch{};
        RT_ORIGINAL_ONCE(Fn, "??BString@UI@@QEBA?AV0System@@XZ")(selfRaw, &scratch);
        auto* ret = static_cast<RtSystem::String*>(retval);
        RT_DIFF_BYTES(&ret->data, &scratch.data, sizeof(char*));
        ReleaseStringRef(scratch.data);
    }
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
            // ...and then freed THROUGH the member. The door is
            // RT_ORIGINAL_ONCE and not the port-aware RT_EXPORT_ONCE, to match
            // `ReleaseStringRef` above and the other String sites in this
            // file: every String block here is allocated and freed through the
            // SAME door, and mixing doors inside one refcounted block is the
            // hazard. rt_port.h's standing advice is the opposite
            // (RT_EXPORT_ONCE for a dependency) and that tension is real - it
            // is flagged to lane ALLOC in this lane's report rather than
            // resolved here, because the allocator is ALLOC's subject and
            // WAVE23_KICKOFF 1a puts it out of bounds for this lane.
            using FreeFn = void (*)(void*);
            FreeFn memFree = RT_ORIGINAL_ONCE(FreeFn,
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

extern "C" void RtUi_SpriteSheet_Set(void* selfRaw, int p1, int p2,
                                     const void* p3Raw, const void* p4Raw,
                                     bool p5) {
    RT_DIFF_BEGIN("UI::SpriteSheet::Set");
    RT_DIFF_WORK(true);          // no early-out at function scope

    auto* self = static_cast<SpriteSheet*>(selfRaw);

    // p4 -> +0x40 FIRST, then p3 -> +0x38. Retail's order, not source order.
    RtUi_SpriteSheetAssignString(self->m_groupName,
                                 static_cast<char* const*>(p4Raw));
    RtUi_SpriteSheetAssignString(self->m_textureName,
                                 static_cast<char* const*>(p3Raw));

    // The three scalars, in retail's store order: +0x4C, then +0x50, then +0x48.
    self->m_field4C = p2;
    self->m_field50 = static_cast<uint8_t>(p5);
    self->m_field48 = p1;

    RT_DIFF_SHADOW(true, "assigned both strings and the three scalars",
                   "no return value; the observable state is the object");
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
    // Its own kill switch is BANISHED_RT_UISPRITECAP.
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
