// ============================================================================
// tests/unit/perfx_table_test.cpp -- PERFXTBL: the per-entity PerfX side table
// (src/app/perfx_table.h) gives every model the batch count the old per-call hash
// probe (BatchEntry) gave it, and every position key its frame's first verdict.
// ============================================================================
// Two worlds see the same script of calls. The OLD one is perf_experimental.cpp's
// former AnimBatchDecide count path: BatchHash::Entry on every call. The NEW one is
// Side::SlotFor + Side::Batch over a second BatchHash (the overflow home), with the
// walk index as the hint, and Side::Rebuild whenever the scripted snapshot changes.
// After every call the counts must be equal. The scripts exercise what the game can
// do between and inside iterations: entities joining and leaving the update tree
// (the model loses and regains its slot mid-frame), every index shifting, an
// unsorted snapshot, a failed rebuild, a model ticked outside the walk (hint -1 or a
// wrong hint), two models on one entity, and a new sim frame.
// ============================================================================
#include "unit.h"
#include "perfx_table.h"

#include <algorithm>
#include <cstdint>
#include <functional>
#include <memory>
#include <random>
#include <vector>

namespace {

using Hash = PerfXTbl::BatchHash<4096>;

struct Ent { int id; };
struct Model { int id; Ent* ent; };

struct Rig {
    std::unique_ptr<Hash> oldTbl{new Hash()};
    std::unique_ptr<Hash> newTbl{new Hash()};
    PerfXTbl::Side<Hash>  side{*newTbl};
    std::vector<Ent*>     snap;
    unsigned              serial = 1;
    int                   mismatches = 0;
    int                   calls = 0;

    Rig() {
        std::memset(oldTbl->tbl, 0, sizeof(oldTbl->tbl));
        std::memset(newTbl->tbl, 0, sizeof(newTbl->tbl));
    }
    void Sort() { std::sort(snap.begin(), snap.end(), std::less<const Ent*>()); }
    void Rebuild(bool sorted = true) { side.Rebuild(snap.data(), snap.size(), sorted, serial); }
    int64_t IndexOf(const Ent* e) const {
        for (size_t i = 0; i < snap.size(); ++i) if (snap[i] == e) return (int64_t)i;
        return -1;
    }
    // One AnimBatchDecide count step: skip=true means the model skipped (count + 1).
    void Call(Model& m, int64_t hint, bool skip) {
        ++calls;
        PerfXTbl::BatchSlot* o = oldTbl->Entry(&m, serial);
        PerfXTbl::Slot* s = side.SlotFor(m.ent, hint);
        unsigned* n = side.Batch(s, &m, serial);
        const bool ok = (o == nullptr) == (n == nullptr) && (!o || o->skipped == *n);
        if (!ok) ++mismatches;
        if (skip) { if (o) ++o->skipped; if (n) ++*n; }
    }
    unsigned NewCount(Model& m) {
        unsigned* n = side.Batch(side.SlotFor(m.ent, -1), &m, serial);
        return n ? *n : 0xffffffffu;
    }
};

}  // namespace

UNIT_TEST(perfx_table_hash_entry_is_the_old_probe) {
    std::unique_ptr<Hash> h(new Hash());
    std::memset(h->tbl, 0, sizeof(h->tbl));
    int a = 0, b = 0;
    PerfXTbl::BatchSlot* ea = h->Entry(&a, 1);
    CHECK(ea != nullptr);
    CHECK_EQ(ea->skipped, 0u);
    ea->skipped = 5;
    CHECK(h->Entry(&a, 1) == ea);
    CHECK(h->Find(&a, 1) == ea);
    CHECK(h->Find(&b, 1) == nullptr);
    CHECK(h->Find(&a, 2) == nullptr);
    PerfXTbl::BatchSlot* ea2 = h->Entry(&a, 2);   // new frame: the stale entry is reclaimed at 0
    CHECK(ea2 == ea);
    CHECK_EQ(ea2->skipped, 0u);
}

UNIT_TEST(perfx_table_find_follows_probe_chains) {
    std::unique_ptr<Hash> h(new Hash());
    std::memset(h->tbl, 0, sizeof(h->tbl));
    // Many keys: collisions build chains; Find must find every live key, and claim none.
    std::vector<int> keys(3000);
    for (size_t i = 0; i < keys.size(); ++i) {
        PerfXTbl::BatchSlot* e = h->Entry(&keys[i], 7);
        CHECK(e != nullptr);
        if (e) e->skipped = (unsigned)i;
    }
    for (size_t i = 0; i < keys.size(); ++i) {
        PerfXTbl::BatchSlot* f = h->Find(&keys[i], 7);
        CHECK(f != nullptr && f->skipped == (unsigned)i);
    }
    int other = 0;
    CHECK(h->Find(&other, 7) == nullptr);
}

UNIT_TEST(perfx_table_walk_hits_and_frames) {
    Rig r;
    std::vector<Ent> ents(64);
    std::vector<Model> models(64);
    for (int i = 0; i < 64; ++i) { ents[i].id = i; models[i] = Model{i, &ents[i]}; r.snap.push_back(&ents[i]); }
    r.Sort(); r.Rebuild();
    for (int frame = 0; frame < 4; ++frame) {
        ++r.serial;
        for (int it = 0; it < 10; ++it)
            for (size_t i = 0; i < r.snap.size(); ++i) {
                Model& m = models[r.snap[i]->id];
                r.Call(m, (int64_t)i, it < 9 && (m.id % 3) != 0);
            }
        for (Model& m : models) CHECK_EQ(r.NewCount(m), (m.id % 3) != 0 ? 9u : 0u);
    }
    CHECK_EQ(r.mismatches, 0);
}

UNIT_TEST(perfx_table_entity_leaves_and_returns_mid_frame) {
    Rig r;
    std::vector<Ent> ents(16);
    std::vector<Model> models(16);
    for (int i = 0; i < 16; ++i) { ents[i].id = i; models[i] = Model{i, &ents[i]}; r.snap.push_back(&ents[i]); }
    r.Sort(); r.Rebuild();
    ++r.serial;
    auto pass = [&](bool skip) {
        for (size_t i = 0; i < r.snap.size(); ++i) r.Call(models[r.snap[i]->id], (int64_t)i, skip);
    };
    pass(true); pass(true);
    // entity 5 leaves the tree (its count goes to the hash), the rest shift
    r.snap.erase(std::find(r.snap.begin(), r.snap.end(), &ents[5]));
    r.Rebuild();
    pass(true);
    r.Call(models[5], -1, true);           // ticked outside the walk while out of the tree
    CHECK_EQ(r.NewCount(models[5]), 3u);
    // it comes back: the slot pulls the count from the hash
    r.snap.push_back(&ents[5]); r.Sort(); r.Rebuild();
    pass(true);
    pass(false);
    for (Model& m : models) CHECK_EQ(r.NewCount(m), 4u);
    CHECK_EQ(r.mismatches, 0);
}

UNIT_TEST(perfx_table_unsorted_and_failed_rebuild) {
    Rig r;
    std::vector<Ent> ents(12);
    std::vector<Model> models(12);
    for (int i = 0; i < 12; ++i) { ents[i].id = i; models[i] = Model{i, &ents[i]}; r.snap.push_back(&ents[i]); }
    r.Sort(); r.Rebuild();
    ++r.serial;
    for (size_t i = 0; i < r.snap.size(); ++i) r.Call(models[r.snap[i]->id], (int64_t)i, true);
    // unsorted: everything spills, the side is unusable (every call is the hash)
    std::reverse(r.snap.begin(), r.snap.end());
    r.Rebuild(false);
    CHECK(!r.side.Usable());
    for (size_t i = 0; i < r.snap.size(); ++i) r.Call(models[r.snap[i]->id], (int64_t)i, true);
    // sorted again: the slots pull the counts back
    r.Sort(); r.Rebuild();
    CHECK(r.side.Usable());
    for (size_t i = 0; i < r.snap.size(); ++i) r.Call(models[r.snap[i]->id], (int64_t)i, true);
    // a failed rebuild (corrupt tree: n = 0, unsorted)
    r.side.Rebuild(static_cast<Ent* const*>(nullptr), 0, false, r.serial);
    for (Model& m : models) r.Call(m, -1, true);
    r.Rebuild();
    for (size_t i = 0; i < r.snap.size(); ++i) r.Call(models[r.snap[i]->id], (int64_t)i, false);
    for (Model& m : models) CHECK_EQ(r.NewCount(m), 4u);
    CHECK_EQ(r.mismatches, 0);
}

UNIT_TEST(perfx_table_two_models_one_entity_and_wrong_hints) {
    Rig r;
    std::vector<Ent> ents(8);
    std::vector<Model> models;
    for (int i = 0; i < 8; ++i) { ents[i].id = i; r.snap.push_back(&ents[i]); }
    for (int i = 0; i < 8; ++i) models.push_back(Model{i, &ents[i]});
    for (int i = 0; i < 8; i += 2) models.push_back(Model{100 + i, &ents[i]});   // second models
    r.Sort(); r.Rebuild();
    ++r.serial;
    for (int it = 0; it < 6; ++it)
        for (Model& m : models) {
            int64_t hint = r.IndexOf(m.ent);
            if (m.id % 4 == 1) hint = (hint + 1) % (int64_t)r.snap.size();   // a wrong hint
            if (m.id % 5 == 2) hint = -1;
            r.Call(m, hint, it < 5);
        }
    for (Model& m : models) CHECK_EQ(r.NewCount(m), 5u);
    CHECK_EQ(r.mismatches, 0);
}

UNIT_TEST(perfx_table_random_scripts) {
    std::mt19937 rng(12345);
    for (int script = 0; script < 40; ++script) {
        Rig r;
        const int nEnt = 20 + (int)(rng() % 60);
        std::vector<Ent> ents((size_t)nEnt);
        std::vector<Model> models;
        for (int i = 0; i < nEnt; ++i) {
            ents[(size_t)i].id = i;
            models.push_back(Model{i, &ents[(size_t)i]});
            if (rng() % 8 == 0) models.push_back(Model{1000 + i, &ents[(size_t)i]});
        }
        std::vector<bool> inTree((size_t)nEnt, true);
        auto resnap = [&] {
            r.snap.clear();
            for (int i = 0; i < nEnt; ++i) if (inTree[(size_t)i]) r.snap.push_back(&ents[(size_t)i]);
            r.Sort();
            const bool sorted = rng() % 25 != 0;
            if (!sorted) std::shuffle(r.snap.begin(), r.snap.end(), rng);
            r.Rebuild(sorted);
        };
        resnap();
        for (int frame = 0; frame < 6; ++frame) {
            ++r.serial;
            const int iters = 1 + (int)(rng() % 10);
            for (int it = 0; it < iters; ++it) {
                if (rng() % 3 == 0) {   // entities join / leave between iterations
                    for (int k = 0; k < 3; ++k) { const size_t e = rng() % (size_t)nEnt; inTree[e] = !inTree[e]; }
                    resnap();
                }
                for (Model& m : models) {
                    if (!inTree[(size_t)m.ent->id] && rng() % 4 != 0) continue;
                    int64_t hint = r.IndexOf(m.ent);
                    if (rng() % 10 == 0) hint = -1;
                    if (rng() % 10 == 0) hint = (int64_t)(rng() % (r.snap.size() + 1));
                    r.Call(m, hint, rng() % 5 != 0);
                }
            }
        }
        CHECK_EQ(r.mismatches, 0);
        CHECK(r.calls > 0);
    }
}

UNIT_TEST(perfx_table_visibility_first_verdict_per_frame) {
    std::unique_ptr<Hash> h(new Hash());
    PerfXTbl::Side<Hash> side(*h);
    Ent a{0}, b{1};
    std::vector<Ent*> snap{&a, &b};
    std::sort(snap.begin(), snap.end(), std::less<const Ent*>());
    side.Rebuild(snap.data(), snap.size(), true, 1);
    float posA[3] = {0, 0, 0}, posB[3] = {1, 1, 1};
    int computes = 0;
    bool answer = false;
    auto compute = [&] { ++computes; return answer; };
    PerfXTbl::Slot* sa = side.SlotFor(&a, -1);
    CHECK(sa != nullptr);
    CHECK(!PerfXTbl::Side<Hash>::Visible(sa, posA, 1, compute));
    answer = true;   // a later compute would say otherwise: the slot keeps the first verdict
    for (int it = 0; it < 9; ++it) CHECK(!PerfXTbl::Side<Hash>::Visible(sa, posA, 1, compute));
    CHECK_EQ(computes, 1);
    CHECK(PerfXTbl::Side<Hash>::Visible(sa, posA, 2, compute));    // new frame: recomputed
    CHECK_EQ(computes, 2);
    CHECK(PerfXTbl::Side<Hash>::Visible(sa, posB, 2, compute));    // another key: recomputed
    CHECK_EQ(computes, 3);
    CHECK(PerfXTbl::Side<Hash>::Visible(nullptr, posA, 2, compute));   // no slot: always compute
    CHECK_EQ(computes, 4);
    // the verdict follows its entity through a rebuild that shifts it
    Ent c{2};
    snap.push_back(&c);
    std::sort(snap.begin(), snap.end(), std::less<const Ent*>());
    PerfXTbl::Side<Hash>::Visible(side.SlotFor(&a, -1), posA, 3, compute);
    const int before = computes;
    side.Rebuild(snap.data(), snap.size(), true, 1);
    answer = false;
    CHECK(PerfXTbl::Side<Hash>::Visible(side.SlotFor(&a, -1), posA, 3, compute));
    CHECK_EQ(computes, before);
}
