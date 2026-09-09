#pragma once
// ============================================================================
// runtime/types/ui/UserInterface.h — binary layout for UI::UserInterface
// ============================================================================
//
// Wave 7 lane C of the Runtime-x64.dll port (docs/runtime_port/PLAN.md §4.2
// wave 7, §6 step 3). The per-function write-up, the oracle argument per
// function and everything this lane held back are in
// docs/runtime_port/WAVE7_LANE_C.md.
//
// PORTABILITY: no <windows.h>, per PLAN §4.1 decision 1.
//
// ---------------------------------------------------------------------------
// SIZE: 0x128 bytes, alignment 4. PROVEN, NOT INFERRED.
// ---------------------------------------------------------------------------
// Retail allocates it in exactly one place, and spells both numbers out:
//
//     UI::UserInterface::ShowDialog        (0x18011fd10, ord 4034)
//         System::MemoryAllocator::Allocate(_singleton, 0x128, 4)
//         UI::UserInterface::UserInterface(that)
//
// which is the nested modal UI ShowDialog builds for itself. Our own tree
// already independently reached the same pair — code/Game/Game/
// gameupdateloop_load.cpp:1065 does `AllocSub(0x128, 4)` before calling the
// ctor through `s_fn_UI_Ctor` — and the ctor's last store lands at +0x120..
// +0x127, one qword short of 0x128.
//
// ---------------------------------------------------------------------------
// THE CLASS IS NOT POLYMORPHIC. There is no vtable pointer at +0x00.
// ---------------------------------------------------------------------------
// This is the single most load-bearing fact in the lane, and it is read out of
// the shipped bytes rather than inferred from the absent `??_7UserInterface`
// export (WAVE7.md fact 2 asked for exactly that check). The ctor at
// 0x180120de0 begins
//
//     180120df1  XOR EAX,EAX
//     180120df3  MOV qword ptr [RCX],RAX        ; _dialogs._head = 0
//     180120df6  MOV qword ptr [RCX+0x8],RAX    ; _dialogs._tail = 0
//
// — its first store is the dialog list's head, not a vtable pointer, and every
// remaining store in the body is a zero or a literal constant. No method of
// the class is virtual: all 52 exported members are `QEAA`/`QEBA`/`CA`, none
// `UEAA`. So PLAN §4.3 risk 3 (vtable ownership) does not arise here, exactly
// as it did not for `Video::Context` in Wave 6 lane A.
//
// ---------------------------------------------------------------------------
// FIELD EVIDENCE
// ---------------------------------------------------------------------------
// A one-line exported accessor is the strongest kind of evidence, because it
// gives the offset AND the field's real name. Ten of the offsets below have
// one; the rest are named by the setter or the loop that owns them.
//
//   off    field                       proven by                       retail
//   0x000  _dialogs (LinkedList)       CleanRemovedDialogs unlink      0x18011f790
//   0x000  _dialogs._head              GetFirstDialog = *(this+0)      0x180031150
//   0x008  _dialogs._tail              GetFirstContainedDialog walks   0x18011fb00
//                                      backwards from *(this+8)
//   0x010  _dialogs._count             ++ / -- around every link edit   0x18011f790
//   0x018  _removedDialogs             ~UserInterface + CleanRemoved-   0x180120b90
//                                      Dialogs iterate it with count
//                                      at +0x20; CloseDialog appends
//   0x028  _addDialogs                 AddDialog pushes to +0x28        0x180120ee0
//   0x030  _addDialogs._size           GetAddDialogCount = *(this+0x30) 0x180031160
//   0x038  _toolTipOwner               BeginToolTip stores *param_2     0x180120860
//   0x040  _toolTipDialog              BeginToolTip stores the new
//                                      ToolTipDialog; CloseDialog and
//                                      Update clear +0x40/+0x38 as a pair
//   0x048  _events (Array, 0x30 each)  CleanRemovedDialogs compacts it  0x18011f790
//                                      with a 0x30 stride; Update and
//                                      CloseDialog push 0x30-byte records
//   0x058  _mouseDownX                 Update, on mouse-button-down     0x1801213c0
//   0x05c  _mouseDownY                 Update, on mouse-button-down     0x1801213c0
//   0x060  _dragDropDialog             Update / CloseDialog end-drag    0x1801213c0
//   0x068  _dragDropElement            Update destroys it (vslot 0)     0x1801213c0
//   0x070  _dragDropId                 DragDropId = *(this+0x70)        0x180031040
//   0x078  _dragDropData               DragDropData = *(this+0x78)      0x180031030
//   0x090  _audioScene                 SetAudioScene                    0x180031050
//   0x098  _sounds[7]                  SetSound writes +0x98 + i*8      0x1800310d0
//   0x0d0  _cursor                     SetCursor                        0x1801225f0
//   0x0d8  _cursorEnabled              EnableCursor                     0x180122580
//   0x0d9  _mouseCaptured              WasMouseCaptured = this[0xd9]    0x180031060
//   0x0dc  _uiScale                    SetUIScale = scale/100.0f        0x180031070
//   0x0e0  _fadeSprite                 SetFullscreenFadeSprite copies   0x18011f560
//                                      both qwords, +0xe0 and +0xe8
//   0x0f0  _fadeVelocity               SetFullscreenFade (+/-1.0f);     0x18011f500
//                                      IsFading is `+0xf0 != 0.0f`      0x180031010
//   0x0f4  _fadeAmount                 SetFullscreenFade reverse-branch 0x18011f500
//   0x0f8  _fadeColor                  SetFullscreenFade 2nd arg        0x18011f500
//   0x0fc  _clipRegion                 GetClipRegion = (this+0xfc)      0x180031000
//   0x110  _spriteCapture              EndSpriteCapture = 0;            0x18011f4a0
//                                      Draw passes it to SpriteContext  0x180121890
//   0x118  _captureViewport            Draw passes (Viewport*)(this+    0x180121890
//                                      0x118) to SpriteContext::Begin
//
// INDEPENDENT CROSS-CHECK from the exe side, written months before this lane
// existed: code/Game/Dialog/optionsdialog.cpp's `ApplyKeyMapToUI_Impl`
// (exe 0x140099260) writes UI +0xA8/+0xB0/+0xB8/+0xC0/+0xC8 as five sound
// effects and UI +0xDC as `settings[0] / 100.0f`. Those are sound slots 2..6
// of the `_sounds[7]` array at +0x98 and the `_uiScale` field — both agree
// with the Runtime-side accessors above, from a completely separate source.
//
// ---------------------------------------------------------------------------
// WHAT THE CTOR DOES *NOT* WRITE — 25 bytes, and the oracle depends on it
// ---------------------------------------------------------------------------
// `reference_oracle_must_not_compare_unwritten_bytes.md`: an oracle that
// memcmps bytes NEITHER side writes fails on padding noise and reads like a
// catastrophic port defect. The ctor leaves these ranges untouched:
//
//     0x014..0x017   _dialogs._pad14
//     0x074..0x077   padding after _dragDropId
//     0x089..0x08f   padding after _dragDropActive
//     0x0da..0x0db   padding after _mouseCaptured (the ctor's store at +0xd8
//                    is a WORD, so it covers 0xd8 and 0xd9 and stops)
//     0x0ec..0x0ef   _fadeSprite._pad0c
//     0x10c..0x10f   padding after _clipRegion
//
// The ctor's replay oracle therefore pre-fills BOTH scratch buffers with the
// SAME byte pattern before running either implementation, so those 25 bytes
// compare equal by construction rather than by luck.
// ============================================================================

#include <cstdint>
#include <cstddef>

#include "../collection/Collection.h"

namespace UI {

// ============================================================
// The element type of all three dialog containers.
//
// `Object::InternalPtr<UI::DialogInstance>` is ONE pointer, 8 bytes, whose
// single field IS the object pointer: every body in this class compares
// `*(void**)ptr` against a list element or an array slot directly
// (AddDialog 0x180120ee0, CloseDialog 0x180120f20, PausesGame 0x18011fa10,
// ~UserInterface 0x180120b90), and the LinkedList node stride is 0x18 with
// `prev` at +0x08, which pins sizeof(T) == 8 through
// `Node = align8(sizeof T) + 0x10` (Collection.h).
//
// It is spelled `void*` rather than `DialogInstance*` on purpose: lane B owns
// runtime/types/ui/DialogInstance.h, nothing in lane C dereferences a
// DialogInstance through a typed pointer, and an incomplete type declared
// twice in two lanes' headers is a needless way to make one lane's edit break
// another's build. The offsets lane C's port DOES read out of a
// DialogInstance (+0x20 flags, +0x50 root element) are spelled as raw
// offsets, with the evidence, at their use site in runtime/src/ui/
// userinterface.cpp.
// ============================================================
typedef void* DialogInstancePtr;

// ============================================================
// UI::UserInterface::ClipRegion — 4 ints, 0x10 bytes, at +0xfc.
//
// Named by its own exported accessor:
//   ?GetClipRegion@UserInterface@UI@@QEBAAEBUClipRegion@12@XZ  0x180031000
//   -> `return (ClipRegion*)(this + 0xfc);`
// and filled by Update (0x1801213c0) and Draw (0x180121890), both of which do
//   *(uint64_t*)(this+0xfc) = 0;            // left = top = 0
//   *(int*)(this+0x104) = scaledWidth - 1;  // right
//   *(int*)(this+0x108) = scaledHeight - 1; // bottom
// so the member order is left, top, right, bottom.
// ============================================================
struct ClipRegion {
    int32_t left;    // +0x00
    int32_t top;     // +0x04
    int32_t right;   // +0x08
    int32_t bottom;  // +0x0c
};
static_assert(sizeof(ClipRegion) == 0x10, "ClipRegion is 4 ints");

// ============================================================
// One queued UI event, 0x30 bytes. INTERIOR DELIBERATELY OPAQUE.
//
// The SIZE is proven — CleanRemovedDialogs (0x18011f790) compacts the +0x48
// array by copying element[i+1] down onto element[i] field by field with a
// 0x30 stride, and both push sites (Update 0x1801213c0, CloseDialog
// 0x180120f20) build a 0x30-byte stack record before calling the array grow
// helper at 0x180033690.
//
// The INTERIOR is not modelled, because nothing this lane ports reads a field
// of one, and inventing a layout we have not proven is how a shared struct
// corrupts silently (PLAN §4.3 risk 1). What the two push sites do show, for
// whoever lands `UpdateEvents`: +0x00 is the target dialog pointer, +0x10 is
// an event id (0xd = drag-drop end, 0xe = drag-drop cancel), +0x14 is -1,
// +0x20 copies _dragDropId and +0x28 copies _dragDropData.
// ============================================================
struct Event {
    uint8_t _opaque[0x30];
};
static_assert(sizeof(Event) == 0x30, "UI event record stride, from CleanRemovedDialogs");

// ============================================================
// UI::UserInterface — 0x128 bytes, no vtable.
// ============================================================
struct UserInterface {
    // --- the live dialog stack, front-to-back ---------------------------------
    // _head is the BACKmost dialog and _tail the FRONTmost: BringDialogToFront
    // (0x180120440) re-appends at _tail, GetFirstContainedDialog walks from
    // _tail backwards through `prev` so the topmost dialog wins a hit test,
    // and Draw/PausesGame walk from _head forwards.
    Collection::LinkedList<DialogInstancePtr> _dialogs;          // +0x000, 0x18

    // Dialogs closed this frame. CloseDialog appends here; the next
    // CleanRemovedDialogs unlinks each one from _dialogs, drops any pending
    // event that names it, releases the object unless its flag bit 5 says a
    // nested ShowDialog UI owns it, and then clears the array.
    Collection::Array<DialogInstancePtr>      _removedDialogs;   // +0x018, 0x10

    // Dialogs added since the last Update. AddDialog appends here rather than
    // to _dialogs; Update drains it back-to-front into the list, so the LAST
    // dialog added in a frame ends up FRONTmost.
    Collection::Array<DialogInstancePtr>      _addDialogs;       // +0x028, 0x10

    // The element the tooltip belongs to, and the ToolTipDialog built for it.
    // Always cleared as a pair (BeginToolTip, CloseDialog, Update).
    DialogInstancePtr _toolTipOwner;                             // +0x038
    void*             _toolTipDialog;                            // +0x040

    Collection::Array<Event> _events;                            // +0x048, 0x10

    int32_t _mouseDownX;                                         // +0x058
    int32_t _mouseDownY;                                         // +0x05c

    DialogInstancePtr _dragDropDialog;                           // +0x060
    void*             _dragDropElement;                          // +0x068
    uint32_t          _dragDropId;                               // +0x070
    uint32_t          _pad74;                                    // +0x074 never written
    void*             _dragDropData;                             // +0x078
    // TWO READINGS, BOTH CORRECT (Wave 10E lane DRAG). The two sites that
    // CLEAR this -- userinterface_hover.cpp:291 and userinterface_frame.cpp:539
    // -- zero the whole qword, so `void*` is the right shape for them and the
    // declaration stays. But the WRITER, BeginDragDrop (0x18011f5f0), fills it
    // as a PAIR OF int32 GRAB OFFSETS: +0x080 = x - element->_layoutLeft and
    // +0x084 = y - element->_layoutTop, i.e. where inside the dragged element
    // the cursor took hold. The pair covers all eight bytes, so a qword clear
    // zeroes both halves and the two readings never disagree.
    void*             _dragDropUserData;                         // +0x080
    uint8_t           _dragDropActive;                           // +0x088
    uint8_t           _pad89[7];                                 // +0x089 never written

    void* _audioScene;                                           // +0x090 Audio::Scene*
    // ExternalPtr<Audio::SoundEffect>, one per UI sound id. Play(id) reads
    // _sounds[id] and queues it on _audioScene; Update calls Play(0) whenever
    // it drained a non-empty _addDialogs, which is the "dialog opened" sound.
    void* _sounds[7];                                            // +0x098..+0x0c8

    void*   _cursor;                                             // +0x0d0 ExternalPtr<UI::Cursor>
    uint8_t _cursorEnabled;                                      // +0x0d8
    uint8_t _mouseCaptured;                                      // +0x0d9
    uint8_t _pad0da[2];                                          // +0x0da never written
    float   _uiScale;                                            // +0x0dc init 1.0f

    // UI::Sprite, 0x10 bytes: {ExternalPtr<SpriteSheet>, int32 index, pad}.
    // Spelled out rather than pulled from runtime/types/ui/Sprite.h because
    // lane F owns that header and this lane only ever copies the 16 bytes
    // wholesale. The ctor's `index = -1` is the engine's empty-sprite value
    // (reference_unresolved_lookup_leaves_a_valid_looking_zero.md: index 0 is
    // a REAL sprite, -1 is the empty one).
    void*   _fadeSpriteSheet;                                    // +0x0e0
    int32_t _fadeSpriteIndex;                                    // +0x0e8 init -1
    int32_t _pad0ec;                                             // +0x0ec never written

    float    _fadeVelocity;                                      // +0x0f0 0 = not fading
    float    _fadeAmount;                                        // +0x0f4 0..1
    uint32_t _fadeColor;                                         // +0x0f8 init 0xffffffff

    ClipRegion _clipRegion;                                      // +0x0fc
    uint32_t   _pad10c;                                          // +0x10c never written

    void*   _spriteCapture;                                      // +0x110 UI::SpriteCapture*
    // Video::Viewport, 0x10 bytes; handed to SpriteContext::Begin by Draw.
    // Opaque here for the same reason as Event: nothing in this lane reads it.
    uint8_t _captureViewport[0x10];                              // +0x118
};

static_assert(sizeof(UserInterface) == 0x128,
              "sizeof(UI::UserInterface) is the literal 0x128 in ShowDialog's Allocate");

static_assert(offsetof(UserInterface, _dialogs)          == 0x000, "_dialogs at +0x000");
static_assert(offsetof(UserInterface, _removedDialogs)   == 0x018, "_removedDialogs at +0x018");
static_assert(offsetof(UserInterface, _addDialogs)       == 0x028, "_addDialogs at +0x028");
static_assert(offsetof(UserInterface, _toolTipOwner)     == 0x038, "_toolTipOwner at +0x038");
static_assert(offsetof(UserInterface, _toolTipDialog)    == 0x040, "_toolTipDialog at +0x040");
static_assert(offsetof(UserInterface, _events)           == 0x048, "_events at +0x048");
static_assert(offsetof(UserInterface, _mouseDownX)       == 0x058, "_mouseDownX at +0x058");
static_assert(offsetof(UserInterface, _mouseDownY)       == 0x05c, "_mouseDownY at +0x05c");
static_assert(offsetof(UserInterface, _dragDropDialog)   == 0x060, "_dragDropDialog at +0x060");
static_assert(offsetof(UserInterface, _dragDropElement)  == 0x068, "_dragDropElement at +0x068");
static_assert(offsetof(UserInterface, _dragDropId)       == 0x070, "_dragDropId at +0x070");
static_assert(offsetof(UserInterface, _dragDropData)     == 0x078, "_dragDropData at +0x078");
static_assert(offsetof(UserInterface, _dragDropUserData) == 0x080, "_dragDropUserData at +0x080");
static_assert(offsetof(UserInterface, _dragDropActive)   == 0x088, "_dragDropActive at +0x088");
static_assert(offsetof(UserInterface, _audioScene)       == 0x090, "_audioScene at +0x090");
static_assert(offsetof(UserInterface, _sounds)           == 0x098, "_sounds at +0x098");
static_assert(offsetof(UserInterface, _cursor)           == 0x0d0, "_cursor at +0x0d0");
static_assert(offsetof(UserInterface, _cursorEnabled)    == 0x0d8, "_cursorEnabled at +0x0d8");
static_assert(offsetof(UserInterface, _mouseCaptured)    == 0x0d9, "_mouseCaptured at +0x0d9");
static_assert(offsetof(UserInterface, _uiScale)          == 0x0dc, "_uiScale at +0x0dc");
static_assert(offsetof(UserInterface, _fadeSpriteSheet)  == 0x0e0, "_fadeSpriteSheet at +0x0e0");
static_assert(offsetof(UserInterface, _fadeSpriteIndex)  == 0x0e8, "_fadeSpriteIndex at +0x0e8");
static_assert(offsetof(UserInterface, _fadeVelocity)     == 0x0f0, "_fadeVelocity at +0x0f0");
static_assert(offsetof(UserInterface, _fadeAmount)       == 0x0f4, "_fadeAmount at +0x0f4");
static_assert(offsetof(UserInterface, _fadeColor)        == 0x0f8, "_fadeColor at +0x0f8");
static_assert(offsetof(UserInterface, _clipRegion)       == 0x0fc, "_clipRegion at +0x0fc");
static_assert(offsetof(UserInterface, _spriteCapture)    == 0x110, "_spriteCapture at +0x110");
static_assert(offsetof(UserInterface, _captureViewport)  == 0x118, "_captureViewport at +0x118");

// The three container members must line up with the Collection layouts Wave 2
// recovered; if either header ever drifts, this is where it is caught.
static_assert(sizeof(Collection::LinkedList<DialogInstancePtr>) == 0x18,
              "LinkedList<T> is head/tail/count/pad = 0x18");
static_assert(sizeof(Collection::Array<DialogInstancePtr>) == 0x10,
              "Array<T> is data/size/capacity = 0x10");

} // namespace UI
