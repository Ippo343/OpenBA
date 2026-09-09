// ============================================================================
// runtime/src/ui/spritecapture_probe.cpp — WAVE 10 LANE UI4 — the driven probe
// ============================================================================
//
// WHY A PROBE, AND WHY IT IS AN ORACLE RATHER THAN A SELF-CHECK
// ---------------------------------------------------------------------------
// `SpriteCapture::AddSprite` and `AddScissor` are reached from exactly one
// place in our tree — the two capture branches of
// `SpriteContext::AddSpriteDraw` and `SpriteContext::SetScissor` — and both
// branches are guarded by `_spriteCapture != nullptr`, which no harness cell
// makes true. Waiting for a caller therefore buys nothing: a longer run has
// the same shape and a bigger denominator, and the port would report
// `calls=0` forever (reference_uncalled_port_is_not_verified.md).
//
// This file does NOT fake the dependency. It builds the SAME subject the exe's
// dev tool builds — a real `UI::SpriteCapture`, zeroed exactly as
// `??0SpriteCapture@UI@@QEAA@XZ` leaves one — and drives our body and the
// SHIPPED body on two of them, side by side, over the same inputs. THEIRS RUNS
// FOR REAL. That is the distinction
// reference_shadow_marker_is_not_an_oracle.md turns on: an RT_DIFF_SHADOW site
// would take our own verdict and never consult the DLL, and a shared
// misunderstanding of retail would pass every time. Here the DLL computes an
// independent answer and the two are compared field by field.
//
// The shipped bodies are reached through `ResolveRuntimeRaw`, NOT `ResolveAddr`
// — this lane registers 0x1800d8b20 and 0x1800d8ef0 by address, so `ResolveAddr`
// would hand back OUR function and the oracle would compare ours to ours and
// agree unconditionally (reference_resolveaddr_consults_port_registry_first.md).
//
// ---------------------------------------------------------------------------
// WHAT IS COMPARED, AND THE THREE THINGS THAT ARE NOT
// ---------------------------------------------------------------------------
// Compared: every scalar of both capture objects that either body writes
// (`_sheets`/`_entries`/`_sprites` sizes and capacities, `_width`, `_height`),
// the sheets array element by element, the sprites array element by element
// over `kSpriteWrittenBytes`, and the entries array field by field.
//
// NOT compared, each for a reason that would otherwise fail 100% of cases and
// read as a catastrophic port defect:
//
//  1. The three `data` POINTERS. The two subjects own different heap blocks by
//     construction. The buffer CONTENTS are compared instead, which is the
//     fact we actually want.
//  2. `CaptureEntry::pad04`. Neither body writes it
//     (runtime/types/ui/SpriteCapture.h); comparing it reports whatever was on
//     the two stacks — reference_oracle_must_not_compare_unwritten_bytes.md.
//  3. `CaptureEntry::name`, on the cases that allocate a FRESH string. The two
//     runs allocate two different heap blocks, so the pointers differ by
//     construction and a pointer compare would fail every time while telling
//     us nothing. Those cases compare null-vs-non-null. The
//     `GetSpriteNameFromIndex` cases do NOT allocate — both runs return the
//     same sheet-owned pointer — so they compare the pointer itself, and that
//     is the one place the name is checked hard.
//
// ---------------------------------------------------------------------------
// THE EFFECTIVENESS CONTROL
// ---------------------------------------------------------------------------
// A green probe is worthless if its comparisons never look at anything
// (reference_probe_needs_an_effectiveness_control.md), so the probe reports how
// many ENTRIES and SPRITES it actually compared, not just how many cases ran.
//
// And `BANISHED_RT_SPRITECAP_NEGCTL=1` corrupts ONE field of the last entry our
// body appended — `color`, which is in the compared set — and the run must then
// report the same number of FAILED cases as it has AddSprite cases. A control
// that corrupted something nothing compares would indict a sound instrument
// (reference_control_must_pick_a_compared_subject.md), which is why the field
// is named here and is one the comparison provably reads.
// ============================================================================

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "../../funcs/rt_port.h"
#include "../../types/ui/Sprite.h"
#include "../../types/ui/SpriteCapture.h"
#include "../../types/ui/SpriteSheet.h"

enum class Module : int { Exe = 0, Runtime = 1, VideoDX11 = 2, COUNT = 3 };
uintptr_t ResolveAddr(Module mod, uintptr_t ghidra_addr);
// The RAW door — bypasses the port registry on purpose; see the banner.
uintptr_t ResolveRuntimeRaw(uintptr_t ghidra_addr);

// Our registered bodies, called directly rather than through the registry: the
// registry would be a second thing to get wrong and this probe is about the
// bodies. Same choice spritecontext_probe.cpp makes.
extern "C" void RtUi_SpriteCapture_AddScissor(void* self, int x, int y,
                                              int right, int bottom, bool fullScreen);
extern "C" void RtUi_SpriteCapture_AddSprite(void* self, const void* sprite,
                                             int x1, int y1, int x2, int y2,
                                             float rotation, uint32_t color);

// Lane UI4's other half — the SpriteSheet vtable slot bodies
// (runtime/src/ui/spritesheet_vtable.cpp). Driven at the end of the probe;
// see the block there for why they need a caller at all.
extern "C" uint32_t    RtUi_SpriteSheet_Type(const void*);
extern "C" uint32_t    RtUi_SpriteSheet_TypeId();
extern "C" const char* RtUi_SpriteSheet_TypeName(const void*);
extern "C" int         RtUi_SpriteSheet_GetFileVersion(const void*);
extern "C" int         RtUi_SpriteSheet_Compile(void*, const void*);
extern "C" void*       RtUi_SpriteSheet_Construct(void*);
extern "C" void*       RtUi_SpriteSheet_Reconstruct(void*);
extern "C" void        RtUi_SpriteSheet_Destruct(void*);

namespace {

using ::Sprite;
using UI::CaptureEntry;
using UI::SpriteCapture;

typedef void (*AddScissorFn)(void*, int, int, int, int, bool);
typedef void (*AddSpriteFn) (void*, const void*, int, int, int, int, float, uint32_t);

AddScissorFn TheirAddScissor() {
    static AddScissorFn f = nullptr;
    if (!f) f = reinterpret_cast<AddScissorFn>(ResolveRuntimeRaw(0x1800d8ef0));
    return f;
}
AddSpriteFn TheirAddSprite() {
    static AddSpriteFn f = nullptr;
    if (!f) f = reinterpret_cast<AddSpriteFn>(ResolveRuntimeRaw(0x1800d8b20));
    return f;
}
void MemFreeRt(void* p) {
    using Fn = void (*)(void*);
    static Fn f = nullptr;
    if (!f) f = reinterpret_cast<Fn>(ResolveAddr(Module::Runtime, 0x1800c7ad0));
    if (f) f(p);
}

// ---- the two subjects -------------------------------------------------------
// A SpriteCapture as `??0SpriteCapture@UI@@QEAA@XZ` leaves it: vtable stamped
// (never read by either body, so a null is honest here) and everything from
// +0x08 to +0x57 zero. Both arrays therefore start EMPTY and grow through the
// real allocator, which is deliberate — it is the only way this probe covers
// the reallocation and 3x-growth branches of the inlined sheets append.
SpriteCapture g_ours;
SpriteCapture g_theirs;

void ResetSubjects() {
    std::memset(&g_ours,   0, sizeof(g_ours));
    std::memset(&g_theirs, 0, sizeof(g_theirs));
}

void FreeSubjects() {
    MemFreeRt(g_ours._sheets.data);   MemFreeRt(g_theirs._sheets.data);
    MemFreeRt(g_ours._entries.data);  MemFreeRt(g_theirs._entries.data);
    MemFreeRt(g_ours._sprites.data);  MemFreeRt(g_theirs._sprites.data);
    ResetSubjects();
}

// ---- a real, hand-built SpriteSheet for the named-sprite case ---------------
// GetSpriteNameFromIndex (0x1800032d0) is three loads: it reads
// `sheet->m_sprites[index].name`, bumps its refcount if non-null and returns
// it. No bounds check, nothing else touched. So a sheet with one SpriteInfo is
// a REAL subject for that call, not a stand-in for one.
//
// The name is a refcounted System::String laid out by hand — [int32 length]
// [int32 refcount][chars] with the stored pointer at `chars`. Its refcount
// starts at 1 and the whole AddSprite path is net-zero on it (+1 from
// GetSpriteNameFromIndex, +1 into the entry, -1 for the temporary, -1 for the
// entry), so it never reaches 0 and is never handed to MemFree. The probe
// asserts that afterwards rather than assuming it.
struct RcString {
    int32_t length;
    int32_t refcount;
    char    chars[16];
};
RcString g_name = { 6, 1, "sprite" };
char* NameData() { return g_name.chars; }

SpriteInfo g_sheetSprites[2];
SpriteSheet g_sheet;

void BuildSheet() {
    std::memset(&g_sheet, 0, sizeof(g_sheet));
    std::memset(g_sheetSprites, 0, sizeof(g_sheetSprites));
    g_sheetSprites[0].name = NameData();
    g_sheetSprites[1].name = nullptr;      // the "sheet has the index but no name" arm
    g_sheet.m_sprites       = g_sheetSprites;
    g_sheet.m_spriteCount   = 2;
    g_sheet.m_spriteCapacity= 2;
}

// A second sheet pointer, never dereferenced: AddSprite only ever COMPARES and
// COPIES `_sheets` elements, so a distinct non-null value is all the dedup
// search needs and is all this is.
uint8_t g_otherSheet[16] = {};

// ---- comparison -------------------------------------------------------------
int g_entriesCompared = 0;
int g_spritesCompared = 0;

bool ArrayHeadersAgree(const UI::CaptureArray& a, const UI::CaptureArray& b,
                       const char* which, const char*& what) {
    if (a.size != b.size)         { what = which; return false; }
    if (a.capacity != b.capacity) { what = which; return false; }
    return true;
}

// `nameIsFresh` says the case allocates its own string, so the two pointers
// differ by construction and must not be compared — see the banner's item 3.
bool SubjectsAgree(bool nameIsFresh, const char*& what) {
    if (!ArrayHeadersAgree(g_ours._sheets,  g_theirs._sheets,  "_sheets header",  what))  return false;
    if (!ArrayHeadersAgree(g_ours._entries, g_theirs._entries, "_entries header", what))  return false;
    if (!ArrayHeadersAgree(g_ours._sprites, g_theirs._sprites, "_sprites header", what))  return false;
    if (g_ours._width  != g_theirs._width)  { what = "_width";  return false; }
    if (g_ours._height != g_theirs._height) { what = "_height"; return false; }

    void* const* sa = static_cast<void* const*>(g_ours._sheets.data);
    void* const* sb = static_cast<void* const*>(g_theirs._sheets.data);
    for (int i = 0; i < g_ours._sheets.size; ++i)
        if (sa[i] != sb[i]) { what = "_sheets[i]"; return false; }

    const Sprite* pa = static_cast<const Sprite*>(g_ours._sprites.data);
    const Sprite* pb = static_cast<const Sprite*>(g_theirs._sprites.data);
    for (int i = 0; i < g_ours._sprites.size; ++i, ++g_spritesCompared)
        if (std::memcmp(&pa[i], &pb[i], kSpriteWrittenBytes) != 0) {
            what = "_sprites[i]"; return false;
        }

    const CaptureEntry* ea = static_cast<const CaptureEntry*>(g_ours._entries.data);
    const CaptureEntry* eb = static_cast<const CaptureEntry*>(g_theirs._entries.data);
    for (int i = 0; i < g_ours._entries.size; ++i, ++g_entriesCompared) {
        if (ea[i].sheetIndex != eb[i].sheetIndex) { what = "entry.sheetIndex"; return false; }
        if (ea[i].x1 != eb[i].x1)                 { what = "entry.x1";         return false; }
        if (ea[i].y1 != eb[i].y1)                 { what = "entry.y1";         return false; }
        if (ea[i].x2 != eb[i].x2)                 { what = "entry.x2";         return false; }
        if (ea[i].y2 != eb[i].y2)                 { what = "entry.y2";         return false; }
        if (std::memcmp(&ea[i].rotation, &eb[i].rotation, 4) != 0) {
            what = "entry.rotation"; return false;                    // bit equality, not ==
        }
        if (ea[i].color != eb[i].color)           { what = "entry.color";      return false; }
        if (nameIsFresh) {
            if ((ea[i].name != nullptr) != (eb[i].name != nullptr)) {
                what = "entry.name null-ness"; return false;
            }
        } else if (ea[i].name != eb[i].name) {
            what = "entry.name"; return false;
        }
    }
    return true;
}

int g_cases = 0, g_failed = 0;

void Check(const char* label, bool nameIsFresh) {
    ++g_cases;
    const char* what = "";
    if (!SubjectsAgree(nameIsFresh, what)) {
        ++g_failed;
        printf("[SpriteCapProbe] DIVERGE %-34s field=%s\n", label, what);
    }
}

bool g_negctl = false;

// The negative control corrupts a field the comparison provably reads.
void MaybeCorrupt() {
    if (!g_negctl) return;
    if (g_ours._entries.size <= 0) return;
    auto* e = static_cast<CaptureEntry*>(g_ours._entries.data);
    e[g_ours._entries.size - 1].color ^= 0x00000001u;
}

}  // namespace

// ---------------------------------------------------------------------------
// WHY THIS DOES NOT RUN AT REGISTRATION TIME
// ---------------------------------------------------------------------------
// It did, for one run, and it crashed: ACCESS_VIOLATION writing 0x648 inside
// `Runtime-x64.dll+0xC7A8A` (System::MemoryAllocator::MemAlloc, 0x1800c7a50),
// with RCX = 0x640 — i.e. `EnterCriticalSection(singleton + 0x640)` on a NULL
// singleton. `System::MemoryAllocator`'s singleton at 0x1801fc718 is not
// created at DLL load; `Engine::Startup` creates it, long after
// RuntimePortInit(). spritecontext_probe.cpp dodged this by pre-sizing its
// arrays so it never allocated — a dodge NOT available here, because every
// AddSprite path either calls System::String::Set or open-codes a
// MemAlloc(9,4), and AddScissor always allocates a "".
//
// So the probe is ARMED at registration and FIRES from the first live call to
// SpriteSheet::GetUVs, which is per-frame in-game (the status-icon geometry
// builder) and is therefore guaranteed to be past Engine::Startup. Same shape
// as runtime/src/graphics/scene_grid.cpp, which fires its probe from a live
// body for the same class of reason.
//
// A run that never reaches the trigger says DEFERRED rather than going silent
// — a probe that prints nothing is indistinguishable from a probe that passed
// (reference_truncated_tool_output_manufactures_calls_zero.md).
namespace { bool g_armed = false; bool g_ran = false; }

void RtUiSpriteCaptureProbeArm() {
    if (!RtDiffEnabled()) return;
    if (!RtWaveEnabled("BANISHED_RT_SPRITECAP_PROBE")) return;
    g_armed = true;
    printf("[SpriteCapProbe] ARMED -- fires on the first live SpriteSheet::GetUVs\n");
}

void RtUiSpriteCaptureProbeReport() {
    if (g_armed && !g_ran)
        printf("[SpriteCapProbe] DEFERRED and NEVER FIRED -- no live "
               "SpriteSheet::GetUVs in this run; the two bodies are UNVERIFIED\n");
}

void RtUiSpriteCaptureProbe() {
    if (!g_armed || g_ran) return;
    g_ran = true;
    if (!TheirAddScissor() || !TheirAddSprite()) {
        printf("[SpriteCapProbe] SKIPPED -- shipped bodies did not resolve\n");
        return;
    }
    // Opt-IN, so it cannot read as "on by default" the way RtWaveEnabled does.
    { const char* e = std::getenv("BANISHED_RT_SPRITECAP_NEGCTL");
      g_negctl = (e && e[0] && std::strcmp(e, "0") != 0); }

    BuildSheet();
    const int32_t rcBefore = g_name.refcount;

    // ---- AddScissor -----------------------------------------------------
    // Both `fullScreen` arms, and a rect sweep wide enough that a sign or an
    // argument-order mistake cannot survive: negative, zero, and large values
    // in every position.
    static const int kRects[][4] = {
        {   0,   0,   0,   0 }, {   1,   2,   3,   4 }, { -5,  -6,  -7,  -8 },
        { 800, 600,   0,   0 }, {   0,   0, 800, 600 }, { -1,   0,   1,   0 },
        { 0x7fffffff, 0, -0x7fffffff, 0 },
    };
    for (int f = 0; f < 2; ++f) {
        for (const auto& r : kRects) {
            ResetSubjects();
            RtUi_SpriteCapture_AddScissor(&g_ours,   r[0], r[1], r[2], r[3], f != 0);
            TheirAddScissor()            (&g_theirs, r[0], r[1], r[2], r[3], f != 0);
            Check(f ? "AddScissor(fullScreen)" : "AddScissor", /*nameIsFresh=*/true);
            FreeSubjects();
        }
    }

    // ---- AddSprite, invalid index (the open-coded "" path) --------------
    // Also the FIRST-APPEND branch of the inlined sheets grow: an empty array,
    // so `oldSize == 0` and the 3x growth heuristic must NOT fire.
    {
        Sprite s; s.spriteSheet = g_otherSheet; s.spriteIndex = -1; s.pad0C = 0;
        ResetSubjects();
        RtUi_SpriteCapture_AddSprite(&g_ours,   &s, 10, 20, 30, 40, 1.5f, 0xAABBCCDDu);
        MaybeCorrupt();
        TheirAddSprite()            (&g_theirs, &s, 10, 20, 30, 40, 1.5f, 0xAABBCCDDu);
        Check("AddSprite(index=-1, new sheet)", /*nameIsFresh=*/true);
        FreeSubjects();
    }

    // ---- AddSprite, repeated sheet (the dedup HIT branch) ---------------
    // Three appends of the same sheet then one of a different sheet: covers
    // "found at index 0", "found at a later index", and the grow-again arm
    // where `oldSize != 0` DOES trigger the 3x capacity heuristic.
    {
        Sprite a; a.spriteSheet = g_otherSheet; a.spriteIndex = -1; a.pad0C = 0;
        Sprite b; b.spriteSheet = &g_sheet;     b.spriteIndex = -1; b.pad0C = 0;
        ResetSubjects();
        for (int i = 0; i < 3; ++i) {
            RtUi_SpriteCapture_AddSprite(&g_ours,   &a, i, i + 1, i + 2, i + 3, 0.0f, 0x11u * i);
            TheirAddSprite()            (&g_theirs, &a, i, i + 1, i + 2, i + 3, 0.0f, 0x11u * i);
        }
        RtUi_SpriteCapture_AddSprite(&g_ours,   &b, 7, 8, 9, 10, -2.25f, 0xFFFFFFFFu);
        MaybeCorrupt();
        TheirAddSprite()            (&g_theirs, &b, 7, 8, 9, 10, -2.25f, 0xFFFFFFFFu);
        Check("AddSprite(dedup + second sheet)", /*nameIsFresh=*/true);
        FreeSubjects();
    }

    // ---- AddSprite, valid index (the GetSpriteNameFromIndex path) -------
    // index 0 has a name, index 1 does not — the two arms of the callee's own
    // `if (name)`. Neither allocates, so the stored pointer is comparable and
    // IS compared (nameIsFresh=false), which is the only hard check this probe
    // makes on `CaptureEntry::name`.
    for (int idx = 0; idx < 2; ++idx) {
        Sprite s; s.spriteSheet = &g_sheet; s.spriteIndex = idx; s.pad0C = 0;
        ResetSubjects();
        // The ledger is read on BOTH sides of BOTH calls, so a non-zero net is
        // attributed to an implementation instead of being reported as one
        // unexplained number at the end.
        const int32_t rc0 = g_name.refcount;
        RtUi_SpriteCapture_AddSprite(&g_ours,   &s, -3, -4, 5, 6, 0.75f, 0x01020304u);
        const int32_t rc1 = g_name.refcount;
        MaybeCorrupt();
        TheirAddSprite()            (&g_theirs, &s, -3, -4, 5, 6, 0.75f, 0x01020304u);
        const int32_t rc2 = g_name.refcount;
        if (rc1 != rc0 || rc2 != rc1)
            printf("[SpriteCapProbe] LEDGER idx=%d  before=%d afterOURS=%d afterTHEIRS=%d\n",
                   idx, rc0, rc1, rc2);
        // idx 1's name is null on both sides, so the pointer compare is exact
        // there too.
        Check(idx == 0 ? "AddSprite(named sprite)" : "AddSprite(unnamed sprite)",
              /*nameIsFresh=*/false);
        FreeSubjects();
    }

    // ---- the SpriteSheet vtable slots -----------------------------------
    // The other half of lane UI4. All seven were ported and registered and
    // then reported `calls=0` in the first live cell: nothing in our tree
    // calls a SpriteSheet virtual before cutover, because the minted vtable
    // that would route to them is gated behind BANISHED_RT_MINT. A port with
    // zero callers reports clean and means nothing
    // (reference_uncalled_port_is_not_verified.md), so they are driven here.
    //
    // Five carry their own replay oracle and fire it when called; this just
    // supplies the caller. Destruct and Reconstruct cannot be replayed (the
    // destructor half runs on the live object and cannot run twice), so they
    // are EXERCISED, not compared, and the distinction is printed.
    {
        alignas(16) uint8_t scratch[sizeof(SpriteSheet)];
        std::memset(scratch, 0xCD, sizeof scratch);

        // The four constant//reflection readers, and the null arm of Construct.
        (void)RtUi_SpriteSheet_Type(scratch);
        (void)RtUi_SpriteSheet_TypeId();
        (void)RtUi_SpriteSheet_TypeName(scratch);
        (void)RtUi_SpriteSheet_GetFileVersion(scratch);
        (void)RtUi_SpriteSheet_Compile(scratch, nullptr);
        (void)RtUi_SpriteSheet_Construct(nullptr);

        // Construct on poisoned storage — its built-in oracle byte-compares
        // against the shipped body on a second scratch, over
        // kSpriteSheetWrittenBytes.
        void* c = RtUi_SpriteSheet_Construct(scratch);

        // Destruct and Reconstruct are safe on a JUST-CONSTRUCTED sheet: every
        // owning field is null, so the deleting destructor in slot 0 frees
        // nothing. Reconstruct's result is then compared against a freshly
        // constructed reference — not an oracle against the DLL, but it does
        // check that the destroy-then-construct pair lands where Construct
        // alone lands, which is the property its callers rely on.
        alignas(16) uint8_t reference[sizeof(SpriteSheet)];
        std::memcpy(reference, scratch, sizeof reference);
        void* r = RtUi_SpriteSheet_Reconstruct(scratch);
        const bool reconstructOk =
            r == static_cast<void*>(scratch) &&
            std::memcmp(scratch, reference, kSpriteSheetWrittenBytes) == 0;
        RtUi_SpriteSheet_Destruct(scratch);

        printf("[SpriteCapProbe] SpriteSheet vtable: 5 slots driven WITH their "
               "replay oracle (Type/TypeId/TypeName/GetFileVersion/Compile/"
               "Construct), Reconstruct+Destruct EXERCISED not compared; "
               "Construct returned %s, Reconstruct %s\n",
               c == static_cast<void*>(scratch) ? "this" : "WRONG",
               reconstructOk ? "== Construct" : "DIVERGED from Construct");
    }

    // ---- the refcount ledger -------------------------------------------
    // EXPECTED: +2 — one per implementation. Exactly one case stores the
    // shared name (idx=0), and the stored array element legitimately OWNS a
    // reference, because the entry array's element copier copy-constructs the
    // String (0x18004beac). `FreeSubjects()` releases the raw buffer without
    // running element destructors, so that owned reference is a deliberate,
    // accounted probe leak of one per side.
    //
    // This number is the reason the type header says what it says. The first
    // draft of this lane predicted ZERO here, on the theory that SetSize
    // copied bitwise and retail therefore left a dangling pointer. The ledger
    // said +2 and the disassembly of the copier agreed with the ledger. Keep
    // the assertion EXACT rather than loosening it to "nonzero is fine" — an
    // exact expectation is what made the wrong theory visible.
    const int32_t kExpectedLeak = 2;
    if (g_name.refcount != rcBefore + kExpectedLeak)
        printf("[SpriteCapProbe] REFCOUNT LEDGER MISMATCH: %d -> %d (expected %d)\n",
               rcBefore, g_name.refcount, rcBefore + kExpectedLeak);
    else
        printf("[SpriteCapProbe] refcount ledger OK: %d -> %d "
               "(+1 owned by each side's stored entry, as expected)\n",
               rcBefore, g_name.refcount);

    printf("[SpriteCapProbe] cases=%d failed=%d  entries compared=%d sprites compared=%d%s\n",
           g_cases, g_failed, g_entriesCompared, g_spritesCompared,
           g_negctl ? "  [NEGATIVE CONTROL ARMED -- 4 failures expected]" : "");
}
