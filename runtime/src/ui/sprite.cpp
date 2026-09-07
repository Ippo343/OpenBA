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

// SpriteSheet::GetUVs(int, float&, float&, float&, float&) const — retail
// 0x180003330. NO upper-bound check (only `index < 0` is guarded), matching
// retail exactly — CLAUDE.md: do not add a guard retail lacks.
void RtUi_SpriteSheet_GetUVs(const void* selfRaw, int index, float* u0, float* v0, float* u1, float* v1) {
    RT_DIFF_BEGIN("UI::SpriteSheet::GetUVs()");
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
    // refcount bump, free the old buffer) to retail's own unexported helper —
    // Collection::Array<SpriteInfo>'s append-by-one body, byte-identical to
    // the growth policy documented in runtime/types/collection/Collection.h.
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
void* RtUi_StringTable_GetText_Index(const void* selfRaw, void* retval, int index) {
    RT_DIFF_BEGIN("UI::StringTable::GetText(int)");
    const auto* base  = static_cast<const uint8_t*>(selfRaw);
    const auto* data  = *reinterpret_cast<StringTableTextEntry* const*>(base + StringTableOffsets::kData);
    const int   count = *reinterpret_cast<const int32_t*>(base + StringTableOffsets::kCount);

    auto* ret = static_cast<RtSystem::String*>(retval);
    ret->data = nullptr;
    if (index >= 0 && index < count) {
        char* text = data[index].text;
        if (text) {
            RtSystem::StringRefCount(text) += 1;
            ret->data = text;
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

// StringTable::GetText(String const& key) const — retail 0x1800025b0. A
// two-line forwarder in retail: `GetText(this, retval, GetIndexFromName(key))`.
// GetIndexFromName (0x180002470) is NOT in this lane's scope (not one of the
// 23 functions Wave 7 lane F owns); it is a pure read with no side effects,
// so calling the DLL's own copy here — rather than porting a function this
// lane was not asked to own — costs nothing and stays correct if a later
// wave ports it differently.
void* RtUi_StringTable_GetText_Key(const void* selfRaw, void* retval, const void* keyStrPtr) {
    RT_DIFF_BEGIN("UI::StringTable::GetText(String const&)");
    using IdxFn = int (*)(const void*, const void*);
    const int index = RT_ORIGINAL_ONCE(
        IdxFn, "?GetIndexFromName@StringTable@UI@@QEBAHAEBVString@System@@@Z")(selfRaw, keyStrPtr);
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
        using IdxFn = int (*)(const void*, const void*);
        self->index = RT_ORIGINAL_ONCE(
            IdxFn, "?GetIndexFromName@StringTable@UI@@QEBAHAEBVString@System@@@Z")(
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

// ============================================================================
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
    RegisterRuntimeExport("?SetSpriteInfo@SpriteSheet@UI@@QEAAXHHHMMMM@Z", (void*)&RtUi_SpriteSheet_SetSpriteInfo); ++n;
    RegisterRuntimeExport("?AddSpriteInfo@SpriteSheet@UI@@QEAAXAEBVString@System@@0HHHH@Z",
                          (void*)&RtUi_SpriteSheet_AddSpriteInfo); ++n;

    // SpriteContext::AddLine / SetScissor x2 -- HELD BACK, see file header §.

    RegisterRuntimeExport("?GetCount@StringTable@UI@@QEBAHXZ", (void*)&RtUi_StringTable_GetCount); ++n;
    RegisterRuntimeExport("?GetText@StringTable@UI@@QEBA?AVString@System@@H@Z",
                          (void*)&RtUi_StringTable_GetText_Index); ++n;
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
