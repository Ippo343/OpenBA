#pragma once
// The hearts / stars icon group (0x20 bytes) that CitizenUI (+0xA0 / +0xC0), StatusBarUI
// (+0x108 / +0x128) and TownHallUI (+0xB0 / +0xD0) embed: the "whole" and the "half" icon
// of every child row of the group element, each a Collection::Array<InternalPtr<
// ImageElement>> (held as UI::Element*). The owners' dtors free +0x10 before +0x00
// (TownHallUI 0x1400811cc, StatusBarUI 0x140014130, CitizenUI 0x140065990): the members'
// ~Array in reverse declaration order.
//
// Its two bodies (Dialog/townhall_helpers.cpp), one each in retail:
//   0x14006abf0 Init   -- RCX = the group, RDX = &the group element's InternalPtr
//   0x14006aed0 Update -- the whole / half visibility from value / max

#include <cstddef>
#include "collection/Collection.h"   // Collection::Array

namespace UI { class Element; }

struct RatingGroup {
    Collection::Array<UI::Element*> whole;   // +0x00
    Collection::Array<UI::Element*> half;    // +0x10
};
static_assert(sizeof(RatingGroup) == 0x20, "rating group 0x20 (0x1400811c0)");
static_assert(offsetof(RatingGroup, whole) == 0x0, "RatingGroup::whole");
static_assert(offsetof(RatingGroup, half) == 0x10, "RatingGroup::half @ +0x10 (0x1400811cc)");

void TownHall_GroupRatingInit(RatingGroup* group, UI::Element* const* groupElem);  // 0x14006abf0
void TownHall_GroupRatingUpdate(RatingGroup* group, float value, float maxValue);  // 0x14006aed0
