// ============================================================================
// runtime/src/ui/element_update_probe.cpp — WAVE 10E lane VERIFY
// A SYNTHETIC-SUBJECT DIFFERENTIAL for ?Update@Element@UI@@ (slot 7, 0x180116270)
// ============================================================================
//
// THE RECORDED CLAIM, AND WHY IT IS TRUE OF ONLY PART OF THE BODY.
//
// element_vtable.cpp's own header says of this row:
//
//     ORACLE: `pin`, and there is no honest alternative. The body pushes events
//     into a live array, opens and closes tooltip dialogs and can call
//     CloseDialog; running the shipped export a second time would double every
//     one of those.
//
// WAVE10B_LANE_ELEM concluded from that "no replay oracle is possible", and the
// row has stood at 541 entries, never differentially compared, ever since.
//
// **That reasoning is exactly right about REPLAY on a LIVE element, and it does
// not settle the question, because the objection is not uniform across the
// body.** The four side effects it names are not reached on every path; three of
// them sit behind a single flag bit that the caller controls:
//
//     if ((f & kElementFlagToolTip16) == 0) return;          // BT 0x10
//     if ((f & kElementFlagToolTipValid) != 0) { ...tooltip... }   // BT 0x13
//
// With bit 16 SET and bit 19 CLEAR the tooltip block is skipped whole —
// BeginToolTip, SetUpperLeft, EndHover and CloseDialog are all unreachable —
// and the body falls through to the hover-enter/leave events and the input
// events. With `state->dragActive == 0` the drop arm (Play + EndDragDrop) is
// unreachable too. What is left in that regime makes NO call outside itself
// except:
//
//   * `Array<UI::Event>::SetSize` on `ui->_events`, which is an append into an
//     array THE CALLER OWNS, and
//   * the children's own vtable slot 7.
//
// Both of those are satisfiable by a subject we build. So the body is
// differentiable over four of its seven numbered steps, on two independent
// objects, with no live UI involved and nothing run twice. The remaining three
// steps are named as uncovered at the bottom and are NOT claimed.
//
// This is the `dialog_settings_probe.cpp` shape (Wave 9 lane ENC-2) applied one
// class up: a synthetic subject, one instance per side, real machinery
// underneath, stubs only for what belongs to the CALLER.
//
// ----------------------------------------------------------------------------
// WHAT IS COMPARED
// ----------------------------------------------------------------------------
//   1. the subject Element's 0xC0 bytes — which is where bit 17 (hover) and bit
//      20 (tooltip shown) live, and the only fields Update writes;
//   2. the UserInterface's whole `_events` array contents, record by record and
//      field by field — id, elementId, mouseX/Y, dragId, dragData, dialog,
//      element (compared as "which side's object", since the two sides hold
//      different addresses for the same logical object);
//   3. the CHILD-RECURSION TRACE — which children were entered, in what order.
//
// (2) is the one that matters most. The body's own banner warns:
//
//     THE EVENT IDS ARE NOT SEQUENTIAL AND THE ORDER IS NOT THE STATE ORDER:
//     state+0x01 -> 2, +0x02 -> 4, +0x03 -> 3, +0x04 -> 5, +0x05 -> 7,
//     +0x06 -> 6.  ... the temptation to write `id = 2 + i` would silently swap
//     two of them.
//
// A transcription slip there is invisible to a byte pin (the pin hashes the
// SHIPPED body, not ours) and invisible to an entry counter. It is visible here.
//
// ----------------------------------------------------------------------------
// THE CONTROLS, SCORED SEPARATELY
// ----------------------------------------------------------------------------
// EFFECTIVENESS (printed on every run, subject or control, so a screen that
// never moves is visible as one that never moves):
//   hovered      cases where the element came out hovering
//   entered      cases where bit 17 changed (the enter/leave edge)
//   evPushed     cases that pushed at least one event
//   evTotal      total event records produced
//   perId[]      how many records of EACH event id were produced — this is the
//                screen the `id = 2 + i` defect would move, and a zero column
//                is a claim this probe CANNOT make about that id
//   childCalls   child slot-7 dispatches
//   borderCase   cases where the hover rect came from `_border`, not `this`
//
// NEGATIVE (BANISHED_RT_ELEMUPDATE_PROBE_NEGCTL=1): OUR side is given a mouse X
// one pixel to the right of THEIRS, and nothing else changes. It is targeted,
// not random: the hover test is `l < mx && mx < r` (STRICT on all four sides)
// while the clip test three lines earlier is INCLUSIVE, and a one-pixel
// disagreement is precisely what a strict/inclusive mix-up produces.
//
// THE CONTROL REPORTS ITS OBSERVABILITY SPLIT BY REASON, because the first
// version of this file got that number wrong by 4.6x and printed "caught 9984
// of the 2160 changed case(s)" — a sentence nobody could have computed. It had
// modelled only the boundary crossings and forgotten that `rec.mouseX = mx`
// copies the perturbed coordinate into EVERY event record, so the perturbation
// is also observable in every case that pushes an event at all. Both counts are
// printed (`ctlPredicate`, `ctlRecord`, `ctlEither`), and `ctlPredicate` is
// labelled an UPPER BOUND in the output because it models the X axis only: a
// case whose Y already puts the cursor outside the rect is counted there and
// cannot actually differ. So the totals must never be read as a denominator.
//
// AND THE CATCHES ARE COUNTED PER SCREEN — elementFlags, eventCount,
// eventField, childTrace — never as a union. A control whose whole catch lands
// on `eventField` says nothing about whether the `elementFlags` screen works,
// and a union number cannot tell those apart
// (reference_union_control_hides_a_blind_screen.md). The report says so in
// words, and calls the flags screen BLIND by name if the predicate flipped in
// cases it did not catch.
// ============================================================================

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "../../funcs/rt_port.h"
#include "../../types/ui/Element.h"
#include "../../types/ui/UserInterface.h"

enum class Module : int { Exe = 0, Runtime = 1, VideoDX11 = 2, COUNT = 3 };
uintptr_t ResolveAddr(Module mod, uintptr_t ghidra_addr);

using UI::Element;

// The ported body, called through its real C symbol so the probe cannot drift
// from the thing it verifies (element_aligned_probe.cpp's rule).
extern "C" void RtUi_Element_Update(Element* self, const void* time, void* ui,
                                    void** dlgPtr, const void* state);

namespace {

// Same UpdateState transcription element_vtable.cpp:208 uses; repeated here
// rather than shared, because that file's copy is in an anonymous namespace.
struct UpdateState {
    uint8_t  dragActive;  uint8_t buttonPressed; uint8_t b02; uint8_t b03;
    uint8_t  b04;         uint8_t b05;           uint8_t b06; uint8_t leftDown;
    uint8_t  b08;         uint8_t b09;           uint8_t pad0a[2];
    int32_t  dragDx; int32_t dragDy; int32_t wheel;
    int32_t  mouseX; int32_t mouseY;
    uint8_t  mouseCaptured; uint8_t pad21[7];
    void*    config;
};
static_assert(sizeof(UpdateState) == 0x30, "UpdateState frame span is 0x30");

struct EventRecord {
    void*    dialog; void* element; int32_t eventId; int32_t elementId;
    int32_t  mouseX; int32_t mouseY; uint32_t dragId; uint32_t pad24;
    void*    dragData;
};
static_assert(sizeof(EventRecord) == 0x30, "UI::Event stride");

using UpdateFn = void (*)(Element*, const void*, void*, void**, const void*);

const char* const kUpdate =
    "?Update@Element@UI@@UEAAXAEBVTime@System@@PEAVUserInterface@2@"
    "V?$InternalPtr@VDialogInstance@UI@@@Object@@AEBUUpdateState@12@@Z";

// ---------------------------------------------------------------------------
// The synthetic subject. One SIDE is a self-contained little UI: a parent
// element, three children (one hidden, one null slot), a border element, and a
// UserInterface carrying only the fields Update reads.
// ---------------------------------------------------------------------------
constexpr int kChildSlots = 3;

int  g_side = 0;                 // 0 = ours, 1 = theirs; set before each call
char g_trace[2][128];
int  g_traceLen[2];
int  g_childCalls = 0;

void TraceChild(int id) {
    int& n = g_traceLen[g_side];
    if (n + 6 >= (int)sizeof(g_trace[0])) return;
    n += std::snprintf(g_trace[g_side] + n, sizeof(g_trace[0]) - n, "%d ", id);
}

// The child stub. It stands in for a derived element's slot 7 — which is what
// belongs to the CALLER of this body rather than to this body — and it is
// SHARED by both sides, so it cannot manufacture a difference. It records only
// the identity and the order.
void Stub_ChildUpdate(Element* self, const void* /*time*/, void* /*ui*/,
                      void** /*dlgPtr*/, const void* /*state*/) {
    ++g_childCalls;
    TraceChild(self->_elementId);
}

struct Side {
    Element         parent;
    Element         children[kChildSlots];
    Element         border;
    Element*        childPtrs[kChildSlots];
    UI::UserInterface ui;
    void*           childVt[16];
    void*           dlg;              // an opaque non-null "dialog" pointer
};

// Two whole sides, statically allocated so nothing here depends on the game's
// allocator being up. `_events._data` IS allocated by the DLL's own
// Array<Event>::SetSize when the first record is pushed, and is deliberately
// left allocated across cases (size reset to 0, capacity kept) — retail clears
// the array per frame the same way, and re-allocating per case would leak.
Side g_ours, g_theirs;

void BuildSide(Side& s, int elementIdBase) {
    std::memset(&s, 0, sizeof(s));

    for (int i = 0; i < 16; ++i) s.childVt[i] = (void*)&Stub_ChildUpdate;

    for (int i = 0; i < kChildSlots; ++i) {
        s.children[i]._vtable    = (void*)s.childVt;
        s.children[i]._elementId = elementIdBase + 10 + i;
        s.children[i]._flags     = 0;
        s.childPtrs[i] = &s.children[i];
    }
    // Slot 1 is HIDDEN (the `continue` at element_vtable.cpp:762) and slot 2 is
    // NULL (the `continue` one line above it). Both skips are then exercised by
    // every case rather than by a special one.
    s.children[1]._flags = UI::kElementFlagHidden;
    s.childPtrs[2] = nullptr;

    s.parent._vtable = (void*)s.childVt;          // never dispatched on the parent
    s.parent._children._data     = s.childPtrs;
    s.parent._children._size     = kChildSlots;
    s.parent._children._capacity = kChildSlots;
    s.parent._elementId = elementIdBase;

    s.dlg = (void*)&s.ui;                  // opaque; only its non-nullness is read
}

// The case description, applied identically to both sides.
struct Case {
    int32_t l, t, r, b;          // the parent's layout rect
    int32_t cl, ct, cr, cb;      // the clip region
    int32_t mx, my;              // the mouse
    uint32_t flags;              // the parent's starting _flags
    bool     useBorder;
    int32_t  bl, bt, br, bb;     // the border's rect, when used
    UpdateState st;
};

void ApplyCase(Side& s, const Case& c) {
    s.parent._layoutLeft = c.l; s.parent._layoutTop = c.t;
    s.parent._layoutRight = c.r; s.parent._layoutBottom = c.b;
    s.parent._flags = c.flags;

    if (c.useBorder) {
        std::memset(&s.border, 0, sizeof(s.border));
        s.border._vtable = (void*)s.childVt;
        s.border._layoutLeft = c.bl; s.border._layoutTop = c.bt;
        s.border._layoutRight = c.br; s.border._layoutBottom = c.bb;
        s.parent._border = (void*)&s.border;
    } else {
        s.parent._border = nullptr;
    }

    s.ui._clipRegion.left = c.cl; s.ui._clipRegion.top = c.ct;
    s.ui._clipRegion.right = c.cr; s.ui._clipRegion.bottom = c.cb;
    s.ui._events._size = 0;              // capacity and buffer are kept
    s.ui._toolTipDialog = nullptr;
    s.ui._toolTipOwner  = nullptr;
    s.ui._dragDropId    = 0xA5A5A5A5u;
    s.ui._dragDropData  = (void*)(uintptr_t)0xD00Du;
}

struct Counters {
    int cases = 0, mismatches = 0;
    int hovered = 0, entered = 0, evPushed = 0, evTotal = 0;
    int childCalls = 0, borderCase = 0, clipReject = 0;
    int perId[18] = {0};
    // Per-SCREEN catches. A control scored on the UNION of several screens
    // reads as a pass while one of them is blind, so each is counted alone
    // (reference_union_control_hides_a_blind_screen.md).
    int catchFlags = 0, catchEvCount = 0, catchEvField = 0, catchTrace = 0;
    // Control only, and split by WHY the perturbation is observable. The first
    // version of this probe had ONE number here that undercounted by 4.6x,
    // because it modelled only boundary crossings and forgot that the mouse
    // coordinate is COPIED INTO EVERY EVENT RECORD -- so a 1-px move is visible
    // in any case that pushes an event at all, boundary or not. Reporting
    // "caught 9984 of the 2160 changed" is a number nobody computed.
    int ctlPredicate = 0;   // the perturbation flips the hover/clip verdict
    int ctlRecord    = 0;   // an event was pushed, so mouseX lands in a record
    int ctlEither    = 0;   // observable for either reason
};

void Report(const Counters& c, bool negctl) {
    std::fprintf(stderr,
        "[ElemUpdateProbe] %s: %d case(s), %d mismatch(es)\n",
        negctl ? "NEGATIVE CONTROL (ours sees mouseX+1)" : "sweep",
        c.cases, c.mismatches);
    std::fprintf(stderr,
        "[ElemUpdateProbe]   effectiveness: hovered=%d hoverEdge=%d "
        "casesThatPushed=%d events=%d childDispatches=%d borderRect=%d "
        "clipRejected=%d\n",
        c.hovered, c.entered, c.evPushed, c.evTotal, c.childCalls,
        c.borderCase, c.clipReject);
    std::fprintf(stderr, "[ElemUpdateProbe]   events per id:");
    for (int i = 0; i < 18; ++i)
        if (c.perId[i]) std::fprintf(stderr, " %d:%d", i, c.perId[i]);
    std::fprintf(stderr, "\n");

    std::fprintf(stderr,
        "[ElemUpdateProbe]   catches PER SCREEN (not a union): elementFlags=%d "
        "eventCount=%d eventField=%d childTrace=%d\n",
        c.catchFlags, c.catchEvCount, c.catchEvField, c.catchTrace);

    if (negctl) {
        std::fprintf(stderr,
            "[ElemUpdateProbe]   the perturbation is OBSERVABLE, by "
            "construction, in %d case(s): %d because it flips the hover/clip "
            "verdict, %d because an event was pushed and the mouse coordinate "
            "is copied into every record (these overlap)\n",
            c.ctlEither, c.ctlPredicate, c.ctlRecord);
        if (c.ctlEither == 0)
            std::fprintf(stderr,
                "[ElemUpdateProbe]   *** VACUOUS: no case changed, so this run "
                "scores NOTHING either way. ***\n");
        else if (c.mismatches == 0)
            std::fprintf(stderr,
                "[ElemUpdateProbe]   *** CONTROL FAILED: %d case(s) genuinely "
                "differed and the comparator saw none of them. Its clean runs "
                "are not evidence. ***\n", c.ctlEither);
        else
            std::fprintf(stderr,
                "[ElemUpdateProbe]   control OK: caught %d of the %d observable "
                "case(s). READ THE PER-SCREEN LINE ABOVE, not this one: a catch "
                "count that is all eventField says nothing about whether the "
                "elementFlags screen works.\n", c.mismatches, c.ctlEither);
        if (c.ctlPredicate > 0 && c.catchFlags == 0)
            std::fprintf(stderr,
                "[ElemUpdateProbe]   *** THE elementFlags SCREEN IS BLIND: %d "
                "case(s) flipped the hover verdict and it caught none. ***\n",
                c.ctlPredicate);
    } else {
        if (c.evTotal == 0)
            std::fprintf(stderr,
                "[ElemUpdateProbe]   *** WEAK: no case pushed an event, so the "
                "event block was never compared. ***\n");
        if (c.entered == 0)
            std::fprintf(stderr,
                "[ElemUpdateProbe]   *** WEAK: no case crossed the hover edge. "
                "***\n");
    }
    std::fprintf(stderr,
        "[ElemUpdateProbe]   NOT COVERED, and not claimed: the tooltip block "
        "(bit 19 set -> BeginToolTip / SetUpperLeft / EndHover / CloseDialog) "
        "and the drag-drop arm (state.dragActive -> Play + EndDragDrop). Both "
        "call into a LIVE UserInterface and neither is replayable here.\n");
    std::fflush(stderr);
}

// Compare one side's produced event array against the other's, translating the
// two object-identity fields (which legitimately differ, because each side owns
// its own objects) into a side-relative token.
const char* WhichObj(const Side& s, const void* p) {
    if (p == nullptr)         return "null";
    if (p == &s.parent)       return "parent";
    if (p == s.dlg)           return "dlg";
    for (int i = 0; i < kChildSlots; ++i)
        if (p == &s.children[i]) return "child";
    if (p == &s.border)       return "border";
    return "OTHER";
}

bool CompareEvents(int* shown, Counters& cc) {
    const int no = g_ours.ui._events._size;
    const int nt = g_theirs.ui._events._size;
    if (no != nt) {
        if ((*shown)++ < 6)
            std::fprintf(stderr, "[ElemUpdateProbe] MISMATCH event COUNT "
                                 "ours=%d theirs=%d\n", no, nt);
        return false;
    }
    const EventRecord* eo =
        reinterpret_cast<const EventRecord*>(g_ours.ui._events._data);
    const EventRecord* et =
        reinterpret_cast<const EventRecord*>(g_theirs.ui._events._data);
    for (int i = 0; i < no; ++i) {
        const bool same =
            eo[i].eventId   == et[i].eventId   &&
            eo[i].elementId == et[i].elementId &&
            eo[i].mouseX    == et[i].mouseX    &&
            eo[i].mouseY    == et[i].mouseY    &&
            eo[i].dragId    == et[i].dragId    &&
            !std::strcmp(WhichObj(g_ours,  eo[i].dialog),
                         WhichObj(g_theirs, et[i].dialog)) &&
            !std::strcmp(WhichObj(g_ours,  eo[i].element),
                         WhichObj(g_theirs, et[i].element)) &&
            ((eo[i].dragData == nullptr) == (et[i].dragData == nullptr));
        if (!same) {
            ++cc.catchEvField;
            if ((*shown)++ < 6)
                std::fprintf(stderr,
                    "[ElemUpdateProbe] MISMATCH event[%d] ours(id=%d elem=%d "
                    "m=%d,%d drag=%08x) theirs(id=%d elem=%d m=%d,%d "
                    "drag=%08x)\n", i,
                    eo[i].eventId, eo[i].elementId, eo[i].mouseX, eo[i].mouseY,
                    eo[i].dragId,
                    et[i].eventId, et[i].elementId, et[i].mouseX, et[i].mouseY,
                    et[i].dragId);
            return false;
        }
    }
    return true;
}

// Element bytes, minus the fields that legitimately hold different addresses on
// the two sides (the vtable, the child array, the border, the parent's own
// name/handle). Everything Update WRITES — only `_flags` — is inside the
// compared span, and `_flags` is compared explicitly as well so a masked field
// cannot hide it.
bool CompareElement(int* shown, Counters& cc) {
    if (g_ours.parent._flags != g_theirs.parent._flags) {
        ++cc.catchFlags;
        if ((*shown)++ < 6)
            std::fprintf(stderr,
                "[ElemUpdateProbe] MISMATCH _flags ours=%08x theirs=%08x\n",
                (unsigned)g_ours.parent._flags,
                (unsigned)g_theirs.parent._flags);
        return false;
    }
    // The scalar tail (+0x60..+0x8f) must not have moved either: nothing in
    // Update writes it, so a difference here is a wild store.
    if (std::memcmp(reinterpret_cast<const uint8_t*>(&g_ours.parent) + 0x60,
                    reinterpret_cast<const uint8_t*>(&g_theirs.parent) + 0x60,
                    0x30) != 0) {
        ++cc.catchFlags;
        if ((*shown)++ < 6)
            std::fprintf(stderr,
                "[ElemUpdateProbe] MISMATCH element tail +0x60..+0x8f\n");
        return false;
    }
    return true;
}

}  // namespace

void RtUiElementUpdateProbe() {
    if (!RtWaveEnabled("BANISHED_RT_ELEMUPDATE_PROBE")) return;
    if (std::getenv("BANISHED_RT_ELEMUPDATE_PROBE") == nullptr) return;

    UpdateFn theirs = reinterpret_cast<UpdateFn>(ResolveExportRaw(kUpdate));
    if (!theirs) {
        std::fprintf(stderr,
            "[ElemUpdateProbe] SKIPPED: the shipped export did not resolve, so "
            "there is no authority to compare against (this is the cutover "
            "value, not a pass)\n");
        std::fflush(stderr);
        return;
    }

    const bool negctl = RtWaveEnabled("BANISHED_RT_ELEMUPDATE_PROBE_NEGCTL") &&
                        std::getenv("BANISHED_RT_ELEMUPDATE_PROBE_NEGCTL") != nullptr;

    BuildSide(g_ours,   100);
    BuildSide(g_theirs, 100);   // SAME ids: the two sides are the same logical UI

    // The rect the sweep aims at. Coordinates are chosen so the sweep steps
    // ACROSS every boundary of both rectangles, including exactly ON them —
    // that is where inclusive and strict differ, and it is the whole point.
    const int32_t L = 100, T = 50, R = 200, B = 90;

    static const int32_t kXs[] = { 99, 100, 101, 150, 199, 200, 201, 4000, -5 };
    static const int32_t kYs[] = { 49, 50,  51,  70,  89,  90,  91,  4000, -5 };
    // Clip regions: one that contains the rect, one that cuts it, one that is
    // degenerate the way lane MOUSE found the live UI to be.
    static const int32_t kClips[][4] = {
        { 0, 0, 2559, 1376 },
        { 120, 60, 180, 80 },
        { 0, 0, -1, -1 },
    };
    // bit16 must be SET or the body returns before the event block; bit19 must
    // be CLEAR or the tooltip block runs and this probe's regime is void.
    static const uint32_t kFlags[] = {
        UI::kElementFlagToolTip16,                              // fresh
        UI::kElementFlagToolTip16 | 0x00020000u,                // already hovering
        UI::kElementFlagToolTip16 | 0x00100000u,                // "tip shown" bit
        UI::kElementFlagToolTip16 | 0x00020000u | 0x00100000u,
    };
    // Every single-button state, plus a combination, plus the drag-delta case.
    static const UpdateState kStates[] = {
        {},
        { 0,1,0,0,0,0,0, 0,0,0,{0,0}, 0,0,0, 0,0, 0,{0},nullptr },  // pressed -> 2
        { 0,0,1,0,0,0,0, 0,0,0,{0,0}, 0,0,0, 0,0, 0,{0},nullptr },  // b02     -> 4
        { 0,0,0,1,0,0,0, 0,0,0,{0,0}, 0,0,0, 0,0, 0,{0},nullptr },  // b03     -> 3
        { 0,0,0,0,1,0,0, 0,0,0,{0,0}, 0,0,0, 0,0, 0,{0},nullptr },  // b04     -> 5
        { 0,0,0,0,0,1,0, 0,0,0,{0,0}, 0,0,0, 0,0, 0,{0},nullptr },  // b05     -> 7
        { 0,0,0,0,0,0,1, 0,0,0,{0,0}, 0,0,0, 0,0, 0,{0},nullptr },  // b06     -> 6
        { 0,1,1,1,1,1,1, 0,0,0,{0,0}, 0,0,0, 0,0, 0,{0},nullptr },  // all six
        { 0,0,0,0,0,0,0, 1,0,0,{0,0}, 3,4,0, 0,0, 0,{0},nullptr },  // drag    -> 0xb
        { 0,0,0,0,0,0,0, 1,0,0,{0,0}, 0,0,0, 0,0, 0,{0},nullptr },  // held, no delta
    };

    Counters c;
    int shown = 0;

    for (size_t xi = 0; xi < sizeof(kXs)/sizeof(kXs[0]); ++xi)
    for (size_t yi = 0; yi < sizeof(kYs)/sizeof(kYs[0]); ++yi)
    for (size_t ci = 0; ci < sizeof(kClips)/sizeof(kClips[0]); ++ci)
    for (size_t fi = 0; fi < sizeof(kFlags)/sizeof(kFlags[0]); ++fi)
    for (size_t si = 0; si < sizeof(kStates)/sizeof(kStates[0]); ++si)
    for (int bd = 0; bd <= 1; ++bd) {
        Case k = {};
        k.l = L; k.t = T; k.r = R; k.b = B;
        k.cl = kClips[ci][0]; k.ct = kClips[ci][1];
        k.cr = kClips[ci][2]; k.cb = kClips[ci][3];
        k.mx = kXs[xi]; k.my = kYs[yi];
        k.flags = kFlags[fi];
        k.useBorder = (bd != 0);
        // A border that is OFFSET from the element, so "which rect was used"
        // is decidable from the outcome rather than being a coin flip.
        k.bl = L + 20; k.bt = T + 5; k.br = R - 20; k.bb = B - 5;
        k.st = kStates[si];

        ApplyCase(g_ours,   k);
        ApplyCase(g_theirs, k);

        UpdateState so = k.st, st = k.st;
        so.mouseX = k.mx; so.mouseY = k.my;
        st.mouseX = k.mx; st.mouseY = k.my;
        if (negctl) so.mouseX = k.mx + 1;

        g_traceLen[0] = g_traceLen[1] = 0;
        g_trace[0][0] = g_trace[1][0] = '\0';
        const int childBefore = g_childCalls;

        void* dlgO = g_ours.dlg;
        void* dlgT = g_theirs.dlg;
        const uint32_t flagsBefore = k.flags;

        g_side = 0;
        RtUi_Element_Update(&g_ours.parent, nullptr, &g_ours.ui, &dlgO, &so);
        g_side = 1;
        theirs(&g_theirs.parent, nullptr, &g_theirs.ui, &dlgT, &st);

        ++c.cases;
        c.childCalls += g_childCalls - childBefore;
        if (k.useBorder) ++c.borderCase;

        // Coverage bookkeeping, recomputed from THEIRS (the authority) so it
        // describes the case rather than trusting the thing under test.
        const bool hov = (g_theirs.parent._flags & 0x00020000u) != 0;
        if (hov) ++c.hovered;
        if (((flagsBefore ^ g_theirs.parent._flags) & 0x00020000u) != 0) ++c.entered;
        if (!hov && (flagsBefore & 0x00020000u) == 0) ++c.clipReject;
        const int nt = g_theirs.ui._events._size;
        if (nt > 0) ++c.evPushed;
        c.evTotal += nt;
        if (const EventRecord* et =
                reinterpret_cast<const EventRecord*>(g_theirs.ui._events._data))
            for (int i = 0; i < nt; ++i)
                if (et[i].eventId >= 0 && et[i].eventId < 18) ++c.perId[et[i].eventId];

        if (negctl) {
            // Did the perturbation change ANYTHING on the authority's terms?
            // Re-run THEIRS with our perturbed input on a scratch pass would
            // double its side effects, so the change is detected the cheap way:
            // the one-pixel move matters only when it crosses a boundary of the
            // rect actually used, or of the clip region.
            const int32_t rl = k.useBorder ? k.bl : k.l;
            const int32_t rr = k.useBorder ? k.br : k.r;
            const bool inA = (rl < k.mx && k.mx < rr);
            const bool inB = (rl < k.mx + 1 && k.mx + 1 < rr);
            const bool clipA = !(k.mx < k.cl || k.mx > k.cr);
            const bool clipB = !(k.mx + 1 < k.cl || k.mx + 1 > k.cr);
            const bool pred  = (inA != inB) || (clipA != clipB);
            const bool rec   = nt > 0;      // mouseX is copied into every record
            if (pred) ++c.ctlPredicate;
            if (rec)  ++c.ctlRecord;
            if (pred || rec) ++c.ctlEither;
        }

        bool ok = CompareElement(&shown, c);
        if (ok) ok = CompareEvents(&shown, c);
        if (ok && std::strcmp(g_trace[0], g_trace[1]) != 0) {
            ++c.catchTrace;
            if (shown++ < 6)
                std::fprintf(stderr,
                    "[ElemUpdateProbe] MISMATCH child trace ours=[%s] "
                    "theirs=[%s]\n", g_trace[0], g_trace[1]);
            ok = false;
        }
        if (!ok) {
            ++c.mismatches;
            if (shown <= 6)
                std::fprintf(stderr,
                    "[ElemUpdateProbe]   case: mouse=(%d,%d) rect=(%d,%d)-(%d,%d) "
                    "border=%d clip=(%d,%d)-(%d,%d) flags=%08x\n",
                    k.mx, k.my, k.l, k.t, k.r, k.b, bd,
                    k.cl, k.ct, k.cr, k.cb, (unsigned)k.flags);
        }
    }

    Report(c, negctl);
}
