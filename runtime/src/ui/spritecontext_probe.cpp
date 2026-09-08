// ============================================================================
// runtime/src/ui/spritecontext_probe.cpp — WAVE 9 LANE UIF — the driven probe
// ============================================================================
//
// WHY THIS EXISTS, and why a longer run was not the answer.
//
// The Wave 9 diff run on merged tip `03eecaa` reached a real world, 195,573
// calls did work across 99 declaring sites, 0 divergences — and NONE of this
// lane's five bodies declared a site at all. `calls=0`, five times.
//
// The cause is coverage, not routing. The only caller of this cluster in our
// tree is `Frustum_Draw` (code/Game/Component/frustumoutlineelement.cpp:245-255,
// the minimap's camera-frustum outline, live in vtable slot 6), and that harness
// cell never opened the minimap. Registration order was checked and is correct
// — `RuntimePortInit()` is step 2c in standalone/main.cpp, before
// `AddressRegistryInit()` at step 3 — so the exe dispatch slot at 0x140151908
// really does hold our `AddLine`.
//
// A LONGER RUN CANNOT FIX THIS, and that is the whole argument for a probe.
// The branches our live-run oracles cover are the RARE ones: `AddLine`'s is the
// invalid-sprite early-out, `SetScissor`'s is the unchanged-rect exit. Neither
// gets more frequent with more frames. A 400-frame run and a 6000-frame run
// have the same SHAPE and a bigger denominator, so more gameplay buys a larger
// `calls=` and no more coverage.
//
// So this file feeds each body its inputs directly, on objects it owns
// completely, and compares against the shipped body side by side.
//
// ---------------------------------------------------------------------------
// WHAT IT COVERS — and the honest hole, stated first
// ---------------------------------------------------------------------------
// Every branch that CALLS `Flush` (0x18011ba40) is out of reach here and is
// reported as SKIPPED rather than quietly omitted. `Flush` dereferences the
// Video device global at 0x1801fe7a8 and makes a vtable call through it before
// it looks at anything we control, so driving it from registration time — where
// this probe runs — would fault on a device that does not exist yet. That
// removes:
//
//   * `SetScissor`'s changed-rect branch (both overloads),
//   * `FlushDeferred`'s drain path,
//   * `AddLine`'s sheet-REBIND path (the one where `_currentSheet` is already
//     non-null and differs).
//
// Those three are verifiable only by a real caller — i.e. a harness cell that
// opens the minimap. This probe does not replace that; it makes the rest
// verifiable now.
//
// What it DOES cover, and this is the part that matters most:
//
//   * `AddSpriteDraw` — BOTH branches, over adversarial inputs. This is the
//     only body in the lane containing float arithmetic that can be subtly
//     wrong (four CVTDQ2PS / MULSS / COMISS comparisons against `_scale`), and
//     it is now driven directly instead of waiting for a dialog to open.
//   * `AddLine` — the invalid-sprite early-out AND the queueing path, by
//     seeding `_currentSheet` null or equal so no `Flush` is reached.
//   * Both `SetScissor` overloads — the unchanged-rect exit, which is also the
//     end-to-end proof of the HIDDEN RETURN SLOT: the shipped body is driven
//     through the same six-argument typedef the port declares, so a wrong arity
//     writes through a sign-extended `int` and the comparison fails.
//   * `FlushDeferred` — the `_deferring` clear no-op.
//
// ---------------------------------------------------------------------------
// TWO BYTES THIS PROBE MUST NOT COMPARE
// ---------------------------------------------------------------------------
// `reference_oracle_must_not_compare_unwritten_bytes.md`. A `SpriteDraw` is
// 0x28 bytes and retail's `AddLine` builds it as an uninitialised local, filling
// every field except **+0x25..+0x27**, which keep whatever was on its stack.
// Our port writes `SpriteDraw rec{}` and zeroes them — a deliberate, documented
// divergence, because reading indeterminate bytes is UB the compiler may
// exploit and nothing anywhere reads those three.
//
// So a byte-for-byte record comparison would fail on EVERY queued record and
// read as a catastrophic port defect. `RecordsEqual` compares +0x00..+0x24 and
// stops. That exclusion is the single most likely thing in this file to be
// mistaken for a cover-up, which is why it is named here and at its use site.
//
// ---------------------------------------------------------------------------
// THE PROBE'S CALLS ARE COUNTED BY THE LIVE SITES TOO
// ---------------------------------------------------------------------------
// Our bodies contain `RT_DIFF_BEGIN`, so every call this probe makes increments
// the same `calls=` counter a gameplay call would. The probe therefore PRINTS
// its own contribution per body, so a reader can subtract it and recover the
// live count. Without that line a future `AddLine calls=37` would be
// indistinguishable from 37 real minimap draws.
// ============================================================================

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "../../funcs/rt_port.h"
#include "../../types/ui/Sprite.h"
#include "../../types/ui/SpriteContext.h"

enum class Module : int { Exe = 0, Runtime = 1, VideoDX11 = 2, COUNT = 3 };
uintptr_t ResolveRuntimeRaw(uintptr_t ghidra_addr);

// Our registered bodies, called directly rather than through the registry:
// the registry would be a second thing to get wrong, and this probe is about
// the bodies.
void  RtUi_SpriteContext_AddLine(void* self, const void* sprite,
                                 int x1, int y1, int x2, int y2, uint32_t color);
void  RtUi_SpriteContext_AddSpriteDraw(void* self, const void* draw);
void  RtUi_SpriteContext_FlushDeferred(void* self);
void* RtUi_SpriteContext_SetScissor_Viewport(void* self, void* ret, const void* viewport);
void* RtUi_SpriteContext_SetScissor_Ints(void* self, void* ret, int x, int y, int w, int h);

namespace {

using UI::ClipRect;
using UI::SpriteContext;
using UI::SpriteDraw;

typedef void  (*AddLineFn)(void*, const void*, int, int, int, int, uint32_t);
typedef void  (*AddSpriteDrawFn)(void*, const void*);
typedef void  (*FlushDeferredFn)(void*);
typedef void* (*SetScissorVpFn)(void*, void*, const void*);
typedef void* (*SetScissorIntsFn)(void*, void*, int, int, int, int);

// ---- the two synthetic subjects --------------------------------------------
// Sized past +0x88, the proven lower bound (SpriteContext.h). Each gets its own
// draw buffer with capacity far above anything a case appends, so the append
// path never reallocates and the probe never touches the allocator — a probe
// that can fault in the allocator is a probe nobody will leave enabled.
constexpr int kCap = 64;

struct Subject {
    alignas(16) uint8_t     obj[0x100];
    alignas(16) SpriteDraw  buf[kCap];
    SpriteContext* ctx() { return reinterpret_cast<SpriteContext*>(obj); }
};

Subject g_ours;
Subject g_theirs;

// One shared clip region, so `_clipRegion` compares equal by construction
// rather than by luck. Deliberately NOT the whole screen: a clip that rejects
// nothing would make the capture-branch bounds test unreachable.
const ClipRect kClip = { 0, 0, 800, 600 };

// A sprite sheet pointer that is never dereferenced by anything on the covered
// branches — it is only ever compared and copied.
uint8_t g_fakeSheet[16] = {};

struct Seed {
    float   scale;
    void*   currentSheet;
    int     sx, sy, sw, sh;
    uint8_t deferring;
};

void Reset(Subject& s, const Seed& seed) {
    // 0xCD everywhere first, so any field a body forgets to write shows up as
    // poison in BOTH subjects rather than as an accidental zero match.
    std::memset(s.obj, 0xCD, sizeof(s.obj));
    std::memset(s.buf, 0xCD, sizeof(s.buf));
    SpriteContext* c = s.ctx();
    c->_scale         = seed.scale;
    c->_currentSheet  = seed.currentSheet;
    c->_draws._data     = s.buf;
    c->_draws._size     = 0;
    c->_draws._capacity = kCap;
    c->_scissorX = seed.sx;
    c->_scissorY = seed.sy;
    c->_scissorW = seed.sw;
    c->_scissorH = seed.sh;
    c->_context   = nullptr;   // never read on a covered branch
    c->_deferring = seed.deferring;
    c->_deferred._data     = nullptr;
    c->_deferred._size     = 0;
    c->_deferred._capacity = 0;
    c->_clipRegion    = &kClip;
    c->_spriteCapture = nullptr;   // keeps every capture branch out of reach
}

// +0x25..+0x27 excluded — see the header. This is the ONE exclusion in the
// file and it is the documented AddLine divergence, not a convenience.
bool RecordsEqual(const SpriteDraw& a, const SpriteDraw& b) {
    return std::memcmp(&a, &b, 0x25) == 0;
}

// Compare everything the covered branches can touch. `_draws._data` is
// deliberately NOT compared: the two subjects own different buffers by
// construction, so comparing the pointers would fail 100% of cases while
// telling us nothing. The buffer CONTENTS are compared instead, which is the
// fact we actually want.
bool SubjectsAgree(const char*& what) {
    const SpriteContext* a = g_ours.ctx();
    const SpriteContext* b = g_theirs.ctx();
    if (a->_scale        != b->_scale)        { what = "_scale";        return false; }
    if (a->_currentSheet != b->_currentSheet) { what = "_currentSheet"; return false; }
    if (a->_scissorX     != b->_scissorX)     { what = "_scissorX";     return false; }
    if (a->_scissorY     != b->_scissorY)     { what = "_scissorY";     return false; }
    if (a->_scissorW     != b->_scissorW)     { what = "_scissorW";     return false; }
    if (a->_scissorH     != b->_scissorH)     { what = "_scissorH";     return false; }
    if (a->_deferring    != b->_deferring)    { what = "_deferring";    return false; }
    if (a->_draws._size  != b->_draws._size)  { what = "_draws._size";  return false; }
    if (a->_deferred._size != b->_deferred._size) { what = "_deferred._size"; return false; }
    for (int i = 0; i < a->_draws._size && i < kCap; ++i) {
        if (!RecordsEqual(g_ours.buf[i], g_theirs.buf[i])) {
            what = "a queued SpriteDraw record"; return false;
        }
    }
    what = nullptr;
    return true;
}

// ---- the Flush tripwire -----------------------------------------------------
// A `SetScissor` case reaches `Flush` if and only if the scaled-down rect
// differs from the subject's stored one — the same predicate both bodies use.
// It can therefore be evaluated WITHOUT calling anything, and any case that
// would take the changed branch is skipped instead of crashing the boot.
//
// This guard exists because the first version of this probe did crash exactly
// that way (see the retslot negative control below). One crash per lesson is
// enough; a table that grows a case later should not be able to repeat it.
bool WouldReachFlush(const SpriteContext* c, int x, int y, int w, int h) {
    const float s = c->_scale;
    const int nx = static_cast<int>(static_cast<float>(x) / s);
    const int ny = static_cast<int>(static_cast<float>(y) / s);
    const int nw = static_cast<int>(static_cast<float>(w) / s);
    const int nh = static_cast<int>(static_cast<float>(h) / s);
    return !(nx == c->_scissorX && ny == c->_scissorY &&
             nw == c->_scissorW && nh == c->_scissorH);
}

struct Counters {
    int cases      = 0;
    int appended   = 0;   // effectiveness: cases that queued a record
    int culled     = 0;   // effectiveness: cases that rejected one
    int earlyOut   = 0;
    int mismatches = 0;
    int addLine = 0, addDraw = 0, scissorVp = 0, scissorInt = 0, flushDef = 0;
    int skippedFlush = 0;
};

Counters g_n;

void Note(bool ok, const char* site, int caseId, const char* what) {
    if (ok) return;
    ++g_n.mismatches;
    printf("[SpriteCtxProbe] *** DIVERGENCE *** %s case %d: %s differs\n",
           site, caseId, what ? what : "(unknown field)");
}

}  // namespace

// ============================================================================
void RtUiSpriteContextProbe() {
    if (!RtDiffEnabled()) return;
    if (!RtWaveEnabled("BANISHED_RT_SPRITECTX_PROBE")) return;

    AddSpriteDrawFn  theirAddDraw  = reinterpret_cast<AddSpriteDrawFn>(ResolveRuntimeRaw(0x18011c890));
    AddLineFn        theirAddLine  = reinterpret_cast<AddLineFn>(ResolveRuntimeRaw(0x18011cbe0));
    FlushDeferredFn  theirFlushDef = reinterpret_cast<FlushDeferredFn>(ResolveRuntimeRaw(0x18011c6a0));
    SetScissorVpFn   theirScisVp   = reinterpret_cast<SetScissorVpFn>(ResolveRuntimeRaw(0x18011d050));
    SetScissorIntsFn theirScisInt  = reinterpret_cast<SetScissorIntsFn>(ResolveRuntimeRaw(0x18011d010));

    if (!theirAddDraw || !theirAddLine || !theirFlushDef || !theirScisVp || !theirScisInt) {
        printf("[SpriteCtxProbe] *** BROKEN -- its silence means NOTHING *** "
               "a raw resolve returned null (addDraw=%p addLine=%p flushDef=%p "
               "scisVp=%p scisInt=%p). With Runtime-x64.dll absent this is EXPECTED "
               "and the probe simply cannot run; with it present, something is wrong.\n",
               (void*)theirAddDraw, (void*)theirAddLine, (void*)theirFlushDef,
               (void*)theirScisVp, (void*)theirScisInt);
        return;
    }

    const Seed base = { 1.0f, nullptr, 10, 20, 300, 200, 0 };

    // ---- 1. AddSpriteDraw, both branches ------------------------------------
    // The scissor is (10,20,300,200) at scale 1, so the accept window in record
    // coordinates is x2 >= 10, x1 <= 310, y2 >= 20, y1 <= 220. Cases straddle
    // every one of those four edges, including exactly-on-the-edge, because the
    // four retail compares are strict `>` (JA) and an off-by-one in either
    // direction is precisely what a `>=` would produce.
    struct DrawCase { int x1, y1, x2, y2; float rot; float scale; const char* note; };
    static const DrawCase drawCases[] = {
        {  50,  50, 100, 100, 0.0f, 1.0f, "well inside" },
        {  10,  20, 310, 220, 0.0f, 1.0f, "exactly the scissor rect" },
        {   0,   0,  10,  20, 0.0f, 1.0f, "touching the near edge" },
        {   0,   0,   9,  19, 0.0f, 1.0f, "one short of the near edge" },
        { 310, 220, 400, 400, 0.0f, 1.0f, "touching the far edge" },
        { 311, 221, 400, 400, 0.0f, 1.0f, "one past the far edge" },
        {-100,-100,-050,-050, 0.0f, 1.0f, "entirely negative" },
        {  50,  50, 100, 100, 0.5f, 1.0f, "rotated: cull is skipped entirely" },
        {  50,  50, 100, 100, 0.0f, 2.0f, "scale 2 shrinks the accept window" },
        {  50,  50, 100, 100, 0.0f, 0.5f, "scale 0.5 widens it" },
        {  20,  40, 620, 440, 0.0f, 0.5f, "scale 0.5, straddling the scaled edge" },
        {   0,   0,   0,   0, 0.0f, 1.0f, "degenerate zero rect" },
    };

    for (size_t i = 0; i < sizeof(drawCases) / sizeof(drawCases[0]); ++i) {
        const DrawCase& c = drawCases[i];
        Seed s = base;
        s.scale = c.scale;
        Reset(g_ours, s);
        Reset(g_theirs, s);

        SpriteDraw rec{};
        rec.sheet = g_fakeSheet;
        rec.spriteIndex = 3;
        rec.x1 = c.x1; rec.y1 = c.y1; rec.x2 = c.x2; rec.y2 = c.y2;
        rec.rotation = c.rot;
        rec.color = 0x80ff00ffu;
        rec.isLine = 1;

        RtUi_SpriteContext_AddSpriteDraw(g_ours.ctx(), &rec);
        theirAddDraw(g_theirs.ctx(), &rec);
        ++g_n.addDraw; ++g_n.cases;

        if (g_ours.ctx()->_draws._size > 0) ++g_n.appended; else ++g_n.culled;
        const char* what = nullptr;
        Note(SubjectsAgree(what), "AddSpriteDraw", (int)i, what);
    }

    // ---- 2. AddLine ----------------------------------------------------------
    // `_currentSheet` is seeded null or equal to the sprite's sheet, so the
    // rebind never reaches Flush. The invalid-sprite cases are the branch the
    // LIVE oracle covers; the queueing cases are the branch it cannot.
    struct LineCase { void* sheet; int index; void* current; const char* note; };
    static const LineCase lineCases[] = {
        { nullptr,      3, nullptr,      "null sheet -> early out" },
        { g_fakeSheet, -1, nullptr,      "index -1 -> early out" },
        { g_fakeSheet, -7, nullptr,      "index very negative -> early out (JL is signed)" },
        { nullptr,     -1, nullptr,      "both invalid -> early out" },
        { g_fakeSheet,  0, nullptr,      "index 0 is a REAL sprite; binds and queues" },
        { g_fakeSheet,  5, nullptr,      "binds from null cursor and queues" },
        { g_fakeSheet,  5, g_fakeSheet,  "cursor already equal; queues without rebinding" },
    };

    for (size_t i = 0; i < sizeof(lineCases) / sizeof(lineCases[0]); ++i) {
        const LineCase& c = lineCases[i];
        Seed s = base;
        s.currentSheet = c.current;
        Reset(g_ours, s);
        Reset(g_theirs, s);

        Sprite sp;
        sp.spriteSheet = c.sheet;
        sp.spriteIndex = c.index;
        sp.pad0C = 0;

        RtUi_SpriteContext_AddLine(g_ours.ctx(),   &sp, 40, 50, 90, 95, 0xff00ff00u);
        theirAddLine            (g_theirs.ctx(), &sp, 40, 50, 90, 95, 0xff00ff00u);
        ++g_n.addLine; ++g_n.cases;

        if (g_ours.ctx()->_draws._size > 0) ++g_n.appended; else ++g_n.earlyOut;
        const char* what = nullptr;
        Note(SubjectsAgree(what), "AddLine", (int)i, what);
    }

    // ---- 3. Both SetScissor overloads, unchanged-rect exit -------------------
    // This is the HIDDEN RETURN SLOT proof. The shipped body is driven through
    // the same typedef the port declares; a missing slot parameter would put
    // `x` in RDX and write 16 bytes through a sign-extended int, and the
    // comparison below would fail rather than the process failing later.
    //
    // Both return slots are poisoned first, so a body that writes nothing is
    // caught as loudly as one that writes the wrong thing.
    struct ScisCase { float scale; int x, y, w, h; const char* note; };
    static const ScisCase scisCases[] = {
        { 1.0f,  10,  20, 300, 200, "identical rect at scale 1" },
        { 2.0f,  20,  40, 600, 400, "scale 2: divides back to the same rect" },
        { 0.5f,   5,  10, 150, 100, "scale 0.5: ditto, and the return is NOT scaled here" },
        { 4.0f,  43,  83, 1203, 803, "scale 4 with truncation on every component" },
    };

    for (size_t i = 0; i < sizeof(scisCases) / sizeof(scisCases[0]); ++i) {
        const ScisCase& c = scisCases[i];
        Seed s = base;
        s.scale = c.scale;
        Reset(g_ours, s);
        Reset(g_theirs, s);

        // The tripwire. Both subjects are seeded identically here, so one test
        // covers both; it is still written as an explicit skip rather than an
        // assumption, because the case table is the thing most likely to grow.
        if (WouldReachFlush(g_ours.ctx(), c.x, c.y, c.w, c.h)) {
            ++g_n.skippedFlush;
            printf("[SpriteCtxProbe] SKIPPED SetScissor case %d (%s): it would take "
                   "the changed-rect branch and call Flush, which needs a live Video "
                   "device this probe does not have.\n", (int)i, c.note);
            continue;
        }

        ClipRect inVp = { c.x, c.y, c.w, c.h };
        ClipRect ourRet, theirRet;
        std::memset(&ourRet,   0xCD, sizeof(ourRet));
        std::memset(&theirRet, 0xCD, sizeof(theirRet));

        void* ourRc   = RtUi_SpriteContext_SetScissor_Viewport(g_ours.ctx(),   &ourRet,   &inVp);
        void* theirRc = theirScisVp                          (g_theirs.ctx(), &theirRet, &inVp);
        ++g_n.scissorVp; ++g_n.cases;

        const char* what = nullptr;
        bool ok = SubjectsAgree(what);
        if (ok && std::memcmp(&ourRet, &theirRet, sizeof(ClipRect)) != 0) {
            ok = false; what = "the returned Viewport";
        }
        if (ok && (ourRc != &ourRet || theirRc != &theirRet)) {
            ok = false; what = "the returned POINTER (RAX must be the slot)";
        }
        Note(ok, "SetScissor(Viewport)", (int)i, what);

        // The four-int overload, same rect, same expectation.
        Reset(g_ours, s);
        Reset(g_theirs, s);
        std::memset(&ourRet,   0xCD, sizeof(ourRet));
        std::memset(&theirRet, 0xCD, sizeof(theirRet));
        ourRc   = RtUi_SpriteContext_SetScissor_Ints(g_ours.ctx(),   &ourRet,   c.x, c.y, c.w, c.h);
        theirRc = theirScisInt                      (g_theirs.ctx(), &theirRet, c.x, c.y, c.w, c.h);
        ++g_n.scissorInt; ++g_n.cases;

        what = nullptr;
        ok = SubjectsAgree(what);
        if (ok && std::memcmp(&ourRet, &theirRet, sizeof(ClipRect)) != 0) {
            ok = false; what = "the returned Viewport";
        }
        if (ok && (ourRc != &ourRet || theirRc != &theirRet)) {
            ok = false; what = "the returned POINTER (RAX must be the slot)";
        }
        Note(ok, "SetScissor(4 ints)", (int)i, what);
    }

    // ---- 4. FlushDeferred, the _deferring-clear no-op ------------------------
    {
        Seed s = base;
        s.deferring = 0;
        Reset(g_ours, s);
        Reset(g_theirs, s);
        RtUi_SpriteContext_FlushDeferred(g_ours.ctx());
        theirFlushDef                   (g_theirs.ctx());
        ++g_n.flushDef; ++g_n.cases; ++g_n.earlyOut;
        const char* what = nullptr;
        Note(SubjectsAgree(what), "FlushDeferred", 0, what);
    }

    // ---- 5. THE NEGATIVE CONTROL, and it must FIRE ---------------------------
    // Everything above can only come back green, so on its own it would stay
    // green if the comparison stopped executing — the shape
    // reference_probe_needs_an_effectiveness_control.md was written about.
    //
    // So: run one case where the two subjects are deliberately given DIFFERENT
    // scissor rects, which must produce different cull decisions, and REQUIRE
    // `SubjectsAgree` to say no. If it says yes, the comparator is not looking
    // and every green line above is worthless.
    bool negativeFired = false;
    {
        Seed a = base;
        Seed b = base;
        b.sw = 1; b.sh = 1;            // theirs accepts almost nothing
        Reset(g_ours,   a);
        Reset(g_theirs, b);
        SpriteDraw rec{};
        rec.sheet = g_fakeSheet;
        rec.spriteIndex = 2;
        rec.x1 = 50; rec.y1 = 50; rec.x2 = 100; rec.y2 = 100;
        rec.rotation = 0.0f;
        rec.color = 0x11223344u;
        rec.isLine = 0;
        RtUi_SpriteContext_AddSpriteDraw(g_ours.ctx(),   &rec);
        theirAddDraw                    (g_theirs.ctx(), &rec);
        ++g_n.addDraw;
        const char* what = nullptr;
        negativeFired = !SubjectsAgree(what);
    }

    // A second negative control, aimed at the retslot specifically: if the
    // comparison of the returned Viewport were dead code, nothing above would
    // notice. Give the two bodies different scales so their returns MUST differ.
    // *** THIS CONTROL CRASHED THE FIRST RUN, AND THE CRASH BELONGS IN THE
    // RECORD. *** Its first version gave the two subjects different STORED
    // rects (ours `_scissorX` 10, theirs 999) against ONE shared input, so the
    // shipped body saw a CHANGED rect, took the changed branch and called
    // `Flush` — which reads the Video device global at 0x1801fe7a8 and then
    // dereferences `[RAX+0x100]`. At registration time that global is null, so
    // the process died at Runtime-x64.dll+0x11BA66 reading address 0x100, from
    // return address 0x18011D0F9 — `SetScissor`'s `CALL 0x18011ba40`.
    //
    // That is EXACTLY the hole this file's header documents, and the lane then
    // walked into it with its own control. The fix is not to guard `Flush`
    // (nothing is wrong with it) but to keep BOTH subjects on the unchanged
    // branch while still forcing their RETURNS to differ: give each subject a
    // different stored rect AND its own matching input, so each compares equal
    // to itself and returns its own raw ints.
    bool negativeRetFired = false;
    {
        Seed a = base; a.sx = 10; a.sy = 20; a.sw = 300; a.sh = 200;
        Seed b = base; b.sx = 11; b.sy = 21; b.sw = 301; b.sh = 201;
        Reset(g_ours,   a);
        Reset(g_theirs, b);
        ClipRect inA = { a.sx, a.sy, a.sw, a.sh };
        ClipRect inB = { b.sx, b.sy, b.sw, b.sh };
        ClipRect r1, r2;
        std::memset(&r1, 0xCD, sizeof(r1));
        std::memset(&r2, 0xCD, sizeof(r2));
        RtUi_SpriteContext_SetScissor_Viewport(g_ours.ctx(),   &r1, &inA);
        theirScisVp                           (g_theirs.ctx(), &r2, &inB);
        ++g_n.scissorVp;
        negativeRetFired = (std::memcmp(&r1, &r2, sizeof(ClipRect)) != 0);
    }

    // ---- the verdict ---------------------------------------------------------
    const bool bothBranches = (g_n.appended > 0 && g_n.culled > 0 && g_n.earlyOut > 0);
    const bool alive = (g_n.mismatches == 0) && negativeFired && negativeRetFired && bothBranches;

    if (alive) {
        printf("[SpriteCtxProbe] ALIVE -- %d cases, 0 divergences vs the shipped bodies. "
               "Effectiveness: appended=%d culled=%d earlyOut=%d (all three branches "
               "exercised). Both negative controls FIRED (cull=%d retslot=%d). "
               "Flush-tripwire skips=%d.\n",
               g_n.cases, g_n.appended, g_n.culled, g_n.earlyOut,
               (int)negativeFired, (int)negativeRetFired, g_n.skippedFlush);
    } else {
        printf("[SpriteCtxProbe] *** BROKEN -- its silence means NOTHING *** "
               "cases=%d divergences=%d appended=%d culled=%d earlyOut=%d "
               "negCull=%d negRetslot=%d\n",
               g_n.cases, g_n.mismatches, g_n.appended, g_n.culled, g_n.earlyOut,
               (int)negativeFired, (int)negativeRetFired);
    }

    // What the probe drove DIRECTLY. These are NOT the numbers to subtract from
    // a `[RtDiff] calls=` line, and saying so is a correction to this probe's
    // own first version, which told the reader to subtract them.
    //
    // Two things break that arithmetic, both measured rather than reasoned:
    //   * `calls=` counts REPORTING calls, not entries. `AddSpriteDraw` is
    //     entered 16 times in a probe-only run and reports 4, because only the
    //     cull branch has a comparison to make.
    //   * the bodies call each other. `AddLine` reaches `AddSpriteDraw`, and
    //     `SetScissor(4 ints)` forwards to `SetScissor(Viewport)`, so a direct
    //     count under-reports both callees.
    //
    // The subtraction baseline is therefore the whole `[RtDiff]` block from a
    // PROBE-ONLY run (`BANISHED_AUTOQUIT=1`, no gameplay). Measured on
    // 2026-09-08, and it reconciles exactly with the case table:
    //
    //   AddSpriteDraw            calls=4  worked=12   (16 entries: 12 cases +
    //                                                  1 control + 3 via AddLine;
    //                                                  4 culled, 12 appended)
    //   AddLine                  calls=4  worked=3    (7 cases: 4 invalid, 3 queued)
    //   SetScissor(Viewport)     calls=9  worked=0    (4 cases + 1 control +
    //                                                  4 forwarded from the ints)
    //   SetScissor(int,int,int,int) calls=4 worked=0
    //   FlushDeferred            calls=1  worked=0
    //
    // Anything above those in a gameplay run is a real caller.
    printf("[SpriteCtxProbe] driven directly by the probe: AddLine=%d AddSpriteDraw=%d "
           "SetScissor(Viewport)=%d SetScissor(4int)=%d FlushDeferred=%d. "
           "NOTE these are ENTRIES, not [RtDiff] calls= -- calls= counts only "
           "REPORTING entries and the bodies call each other. The subtraction "
           "baseline is the whole [RtDiff] block of a probe-only run: "
           "AddSpriteDraw 4/12, AddLine 4/3, SetScissorVp 9/0, SetScissor4 4/0, "
           "FlushDeferred 1/0 (calls/worked).\n",
           g_n.addLine, g_n.addDraw, g_n.scissorVp, g_n.scissorInt, g_n.flushDef);

    // The hole, restated where the numbers are, not only in the file header.
    printf("[SpriteCtxProbe] NOT COVERED (needs a live Video device, so only a real "
           "caller can reach it): SetScissor's changed-rect branch, FlushDeferred's "
           "drain path, and AddLine's sheet-REBIND path -- all three call Flush "
           "(0x18011ba40), which derefs the device global at 0x1801fe7a8 before it "
           "reads anything we own. FlushDeferred reporting calls=0 in a gameplay run "
           "is EXPECTED, not dead: _deferring is retail's deferred-font path and "
           "nothing on the boot path sets it.\n");
}
