#pragma once
// ============================================================================
// runtime/types/ui/ComboElement.h — UI::ComboElement, binary-compatible layout
// ============================================================================
// Wave 7 lane D. Three PARALLEL Collection::Array<T> (runtime/types/collection/
// Collection.h), grown in lockstep by AddItem (held back — see
// WAVE7_LANE_D.md §3), plus a current-item index and a dialog handle used only
// by the held-back SetCurrentItem:
//
//   +0xE8  _strings   Array<System::String>  data+0xE8 / size+0xF0 / cap+0xF4
//   +0xF8  _sprites   Array<UI::Sprite>      data+0xF8 / size+0x100 / cap+0x104
//   +0x108 _userData  Array<void*>           data+0x108 / size+0x110 / cap+0x114
//   +0x118 _currentItem int32   ?GetCurrentItem@ComboElement@UI@@QEBAHXZ (0x180075160)
//   +0x120 _openDialog  void*   read/cleared only by the held-back SetCurrentItem
//
// Proof: `?AddItem@` (0x750b0) calls three growth helpers, each
// `FUN_xxx(&array, *(int*)(&array+0x08) + 1, &newElement)` — i.e. the SAME
// `Collection::Array<T>::SetSize`-shaped call the Wave 2 Collection.h write-up
// documents, once per array — and `?GetItemCount@` (0x75180) reads
// `*(int*)(this+0xf0)`, `?GetItemData@` (0x75170) reads
// `*(void**)(*(long*)(this+0x108) + index*8)`, and `?RemoveAllItems@`
// (0x75120) tears down all three plus resets `*(int*)(this+0x118) = -1`.
//
// `Collection::Array<T>` is T-INDEPENDENT in size (Collection.h: "sizeof(Array<T>)
// is T-independent", always 16 bytes), so `_sprites` and `_strings` are typed
// `Array<void*>` below purely to get the right CONTAINER shape — this lane
// never indexes into either buffer's element bytes, only into `_userData`'s
// (GetItemData), where `void*` is the real, proven element type.
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "Element.h"
#include "../collection/Collection.h"

namespace UI {

struct ComboElement {
    Element base;                              // +0x00 .. +0xBF
    uint8_t _pad_c0_e8[0x28];                  // +0xC0 .. +0xE7 — not reversed by this lane.
    Collection::Array<void*> _strings;          // +0xE8  element = System::String (see note above)
    Collection::Array<void*> _sprites;          // +0xF8  element = UI::Sprite (see note above)
    Collection::Array<void*> _userData;         // +0x108 element = void*
    int32_t _currentItem;                       // +0x118
    uint32_t _pad11c;                           // +0x11C — not reversed by this lane.
    void*   _openDialog;                        // +0x120 — used only by held-back SetCurrentItem.
};

static_assert(offsetof(ComboElement, base)        == 0x00,  "ComboElement::base @ +0x00");
static_assert(offsetof(ComboElement, _strings)    == 0xE8,  "ComboElement::_strings @ +0xE8");
static_assert(offsetof(ComboElement, _sprites)    == 0xF8,  "ComboElement::_sprites @ +0xF8");
static_assert(offsetof(ComboElement, _userData)   == 0x108, "ComboElement::_userData @ +0x108");
static_assert(offsetof(ComboElement, _currentItem)== 0x118, "ComboElement::_currentItem @ +0x118");
static_assert(offsetof(ComboElement, _openDialog) == 0x120, "ComboElement::_openDialog @ +0x120");

} // namespace UI
