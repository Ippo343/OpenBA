// ============================================================================
// tests/unit/ext_load_notice_test.cpp -- which load notice an `ext:` pseudo-mod gets
// ============================================================================
// src/ext/load_notice.{h,cpp} (block EXTNOTICE, docs/extensions/handins/
// EXTNOTICE.md): the reason picked for each case DoLoad can meet, its dialog
// name, and None exactly where Ext::PseudoModLoadStatus says 0. The `unit_*`
// extensions exist only under BANISHED_UNIT_TEST (unit_a class A, unit_b
// class B v1, unit_c class C v3).
// ============================================================================
#include "unit.h"
#include "ext/load_notice.h"
#include "ext/registry.h"

#include <cstring>
#include <initializer_list>

namespace {

Ext::State WithOn(std::initializer_list<const char*> on) {
    Ext::State s = Ext::Resolve(nullptr, nullptr, nullptr, nullptr);
    for (const char* id : on) Ext::FindEntry(id)->slot(s) = true;
    return s;
}

}  // namespace

UNIT_TEST(ext_load_notice_reason) {
    using N = Ext::PseudoModNotice;
    Ext::Install(WithOn({"unit_b"}), Ext::Profile::PerEntry, false);
    CHECK(Ext::PseudoModNoticeFor("ext:unit_b", 1) == N::None);          // present, requested
    CHECK(Ext::PseudoModNoticeFor("ext:unit_b", 2) == N::NewerVersion);  // a newer format wrote it
    CHECK(Ext::PseudoModNoticeFor("ext:unit_c", 3) == N::TurnedOff);     // present, not requested
    CHECK(Ext::PseudoModNoticeFor("ext:unit_c", 4) == N::NewerVersion);  // newer wins over off
    CHECK(Ext::PseudoModNoticeFor("ext:nope", 1) == N::NotInBuild);      // unknown id
    CHECK(Ext::PseudoModNoticeFor("ext:unit_a", 1) == N::NotInBuild);    // class A is never in a save
    CHECK(Ext::PseudoModNoticeFor("Mods/x.pkm", 1) == N::NotInBuild);
    CHECK(Ext::PseudoModNoticeFor(nullptr, 1) == N::NotInBuild);
    Ext::Install(WithOn({}), Ext::Profile::PerEntry, false);
}

UNIT_TEST(ext_load_notice_matches_load_status) {
    const char* paths[] = {"ext:unit_a", "ext:unit_b", "ext:unit_c", "ext:nope", "Mods/x.pkm"};
    for (int on = 0; on < 4; ++on) {
        Ext::State s = WithOn({});
        if (on & 1) Ext::FindEntry("unit_b")->slot(s) = true;
        if (on & 2) Ext::FindEntry("unit_c")->slot(s) = true;
        Ext::Install(s, Ext::Profile::PerEntry, false);
        for (const char* p : paths)
            for (int32_t v = 0; v <= 4; ++v) {
                const bool none = Ext::PseudoModNoticeFor(p, v) == Ext::PseudoModNotice::None;
                CHECK_EQ(none, Ext::PseudoModLoadStatus(p, v) == 0);
            }
    }
    Ext::Install(WithOn({}), Ext::Profile::PerEntry, false);
}

UNIT_TEST(ext_load_notice_dialog_names) {
    using N = Ext::PseudoModNotice;
    CHECK(Ext::PseudoModNoticeDialog(N::None) == nullptr);
    CHECK(std::strcmp(Ext::PseudoModNoticeDialog(N::NotInBuild),
                      "Dialog/Extensions.rsc:extensionNotInBuild") == 0);
    CHECK(std::strcmp(Ext::PseudoModNoticeDialog(N::NewerVersion),
                      "Dialog/Extensions.rsc:extensionNewer") == 0);
    CHECK(std::strcmp(Ext::PseudoModNoticeDialog(N::TurnedOff),
                      "Dialog/Extensions.rsc:extensionTurnedOff") == 0);
}
