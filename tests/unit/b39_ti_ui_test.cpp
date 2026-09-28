// ============================================================================
// tests/unit/b39_ti_ui_test.cpp — ENDGAME B.3.9 TI-UI (B39_RULES D6, W3): the
// UI classes' type records are their own `DEFINE_TYPE` statics.
//
// The unit harness links the UI records the ABI tests define (ui_leaf_c1..c5,
// ui_resources, ui_dialog, ui_element: every one of the 68 UI classes, each
// with the literals runtime/src/ui passes — the minted rows are retired). For a
// sample of 18 classes, after running Factory::Startup's pass 1 over the chain
// (`typeId = TypeNameCRC(name)`, the only thing Startup writes into a record;
// the ids are put back afterwards so no other test sees a changed chain):
//   * X::TypeId() == ~CRC32 of the UTF-16LE name, computed HERE with a bitwise
//     CRC-32 (not through TypeNameCRC), and == the id TYPETABLE_414.txt
//     records for that name (the gate's baseline);
//   * TypeName() is the literal, the stored parent is the stripped literal;
//   * the record is in `_firstTypeInfo` exactly once, and no two chain
//     records share a name;
//   * Element / DialogInstance carry Object::Internal's in-place factory
//     (retail 0x1800070c0), every other sampled class its own.
// ============================================================================

#include "unit.h"

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "runtime/types/object/TypeInfo.h"
#include "runtime/types/object/Internal.h"
#include "runtime/types/ui/ButtonElement.h"
#include "runtime/types/ui/CheckElement.h"
#include "runtime/types/ui/ClipRegionElement.h"
#include "runtime/types/ui/ComboDialog.h"
#include "runtime/types/ui/Dialog.h"
#include "runtime/types/ui/DialogController.h"
#include "runtime/types/ui/DialogInstance.h"
#include "runtime/types/ui/DocumentTextDescription.h"
#include "runtime/types/ui/Element.h"
#include "runtime/types/ui/ElementDescription.h"
#include "runtime/types/ui/Font.h"
#include "runtime/types/ui/GroupElement.h"
#include "runtime/types/ui/ImageDescription.h"
#include "runtime/types/ui/ListElement.h"
#include "runtime/types/ui/SpinnerElement.h"
#include "runtime/types/ui/StringTable.h"
#include "runtime/types/ui/TextElement.h"
#include "runtime/types/ui/ToolTipDialog.h"

namespace {

// Standard reflected CRC-32 (poly 0xEDB88320, init/final ~0) over the name
// widened to UTF-16LE — Startup's id. Bitwise, independent of TypeNameCRC.
uint32_t IndependentId(const char* name) {
    uint32_t crc = 0xFFFFFFFFu;
    for (const char* p = name; *p; ++p) {
        const uint8_t bytes[2] = { static_cast<uint8_t>(*p), 0 };
        for (uint8_t b : bytes) {
            crc ^= b;
            for (int k = 0; k < 8; ++k)
                crc = (crc & 1u) ? (crc >> 1) ^ 0xEDB88320u : (crc >> 1);
        }
    }
    return ~crc;
}

struct Sample {
    const ::Object::BaseTypeInfo* rec;
    uint32_t (*typeId)();
    const char* name;          // the literal
    const char* parent;        // the stripped parent literal
    uint32_t tableId;          // TYPETABLE_414.txt
    bool internalInPlace;      // retail row: ?CreateInPlaceObject@Internal@Object@@
};

#define TI_UI_SAMPLE(Cls, Parent, Id, Inh) \
    { &UI::Cls::GetTypeInfo(), &UI::Cls::TypeId, #Cls, Parent, Id, Inh }

std::vector<Sample> Samples() {
    return {
        TI_UI_SAMPLE(ButtonElement,           "Element",            0x5310BDA4u, false),
        TI_UI_SAMPLE(CheckElement,            "ButtonElement",      0x1039FDD3u, false),
        TI_UI_SAMPLE(ClipRegionElement,       "Element",            0x352B9ADAu, false),
        TI_UI_SAMPLE(GroupElement,            "Element",            0x8CED93CDu, false),
        TI_UI_SAMPLE(ImageDescription,        "ElementDescription", 0xFDD24E9Au, false),
        TI_UI_SAMPLE(TextElement,             "Element",            0x1A20CEDEu, false),
        TI_UI_SAMPLE(ListElement,             "Element",            0x2475EC1Au, false),
        TI_UI_SAMPLE(SpinnerElement,          "Element",            0x26681EB1u, false),
        TI_UI_SAMPLE(DocumentTextDescription, "ElementDescription", 0xEC3779BBu, false),
        TI_UI_SAMPLE(Font,                    "External",           0xE3D8954Eu, false),
        TI_UI_SAMPLE(StringTable,             "External",           0xC03519E4u, false),
        TI_UI_SAMPLE(Dialog,                  "External",           0x4FCC106Bu, false),
        TI_UI_SAMPLE(DialogInstance,          "Internal",           0xBE12E2EBu, true),
        TI_UI_SAMPLE(Element,                 "Internal",           0x8D4A19A7u, true),
        TI_UI_SAMPLE(ElementDescription,      "External",           0x42F6A818u, false),
        TI_UI_SAMPLE(ToolTipDialog,           "DialogInstance",     0x175C6265u, false),
        TI_UI_SAMPLE(DialogController,        "Internal",           0xF2C192B8u, false),
        TI_UI_SAMPLE(ComboDialog,             "DialogInstance",     0x4C55FDDFu, false),
    };
}
#undef TI_UI_SAMPLE

// Startup's pass-1 write, over the whole chain; the old ids are returned so the
// caller can put them back.
std::vector<uint32_t> RunStartupPass1() {
    std::vector<uint32_t> old;
    for (::Object::BaseTypeInfo* ti = ::Object::BaseTypeInfo::_firstTypeInfo; ti; ti = ti->next) {
        old.push_back(ti->typeId);
        ti->typeId = ::Object::TypeNameCRC(ti->name);
    }
    return old;
}
void Restore(const std::vector<uint32_t>& old) {
    size_t i = 0;
    for (::Object::BaseTypeInfo* ti = ::Object::BaseTypeInfo::_firstTypeInfo; ti && i < old.size();
         ti = ti->next)
        ti->typeId = old[i++];
}

}  // namespace

UNIT_TEST(b39_ti_ui_type_ids_are_the_crc_of_the_literal) {
    const std::vector<uint32_t> old = RunStartupPass1();
    for (const Sample& s : Samples()) {
        CHECK_EQ(s.typeId(), IndependentId(s.name));
        CHECK_EQ(s.typeId(), s.tableId);
        CHECK_EQ(s.rec->typeId, s.tableId);
    }
    Restore(old);
}

UNIT_TEST(b39_ti_ui_names_and_parents_are_the_literals) {
    for (const Sample& s : Samples()) {
        CHECK_STREQ(s.rec->name, s.name);
        CHECK_STREQ(s.rec->parentName, s.parent);
        CHECK(s.rec->fileExtension == nullptr);
        CHECK(s.rec->factoryFn != nullptr);
        const void* internalInPlace =
            reinterpret_cast<const void*>(&::Object::Internal::CreateInPlaceObject);
        if (s.internalInPlace) CHECK(s.rec->inPlaceFactoryFn == internalInPlace);
        else                   CHECK(s.rec->inPlaceFactoryFn != internalInPlace);
    }
}

UNIT_TEST(b39_ti_ui_records_are_in_the_chain_once_and_names_are_unique) {
    std::vector<const ::Object::BaseTypeInfo*> chain;
    for (::Object::BaseTypeInfo* ti = ::Object::BaseTypeInfo::_firstTypeInfo; ti; ti = ti->next)
        chain.push_back(ti);
    for (const Sample& s : Samples()) {
        int seen = 0;
        for (const ::Object::BaseTypeInfo* ti : chain)
            if (ti == s.rec) ++seen;
        CHECK_EQ(seen, 1);
    }
    int dup = 0;
    for (size_t i = 0; i < chain.size(); ++i)
        for (size_t j = i + 1; j < chain.size(); ++j)
            if (std::strcmp(chain[i]->name, chain[j]->name) == 0) ++dup;
    CHECK_EQ(dup, 0);
}
