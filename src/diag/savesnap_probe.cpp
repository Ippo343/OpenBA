// src/diag/savesnap_probe.cpp -- TEMPORARY PROBE (SAVESNAP investigation, not for merge).
//
// docs/extensions/handins/HITCH_SAVESNAP.md. Runs only in a stress run (g_pfProfOn), only at
// the first in-game save: after the real SaveRestore it walks SerializeAll again into flat
// streams (the raw-walk cost), counts the writes / Serializers / persisted objects, checks the
// flat image against the real save's decompressed image, and then serializes again after the
// next Updates to measure what changes from one Update to the next (per object, by type).
#include "savesnap_probe.h"

#include "Game/GameUpdateLoop.h"
#include "io/Stream.h"
#include "io/Serializer.h"
#include "object/Internal.h"
#include "object/TypeInfo.h"
#include "save/SaveLoad.h"
#include "err_line.h"
#include "load_time.h"
#include "stress_harness.h"

#include <windows.h>
#include <psapi.h>
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <map>
#include <new>
#include <string>
#include <unordered_map>
#include <vector>

extern long long g_probeSerializerCount;   // serializer_class.cpp
long long g_probeExtCalls = 0;           // the ExternalFactory lookups (external*.cpp)

namespace {

constexpr int kCap = 64 * 1024 * 1024;

struct ProbeStream : IO::MemoryStream {
    bool counting = false;
    bool nullWrites = false;
    uint64_t writes = 0, bytes = 0, seeks = 0;
    uint64_t bySize[10] = {};   // 1,2,4,8 and the rest
    uint32_t hiWater = 0;
    bool Write(int n, const void* s) override {
        if (counting) {
            ++writes; bytes += (uint64_t)n;
            const int k = n == 1 ? 0 : n == 2 ? 1 : n == 4 ? 2 : n == 8 ? 3 : n < 64 ? 4 : n < 4096 ? 5 : 6;
            ++bySize[k];
        }
        if (nullWrites) {
            _cursor += (uint32_t)n;
            if (_cursor > hiWater) hiWater = _cursor;
            return true;
        }
        const bool ok = MemoryStream::Write(n, s);
        if (_cursor > hiWater) hiWater = _cursor;
        return ok;
    }
    bool SetCurrentPos(int p) override {
        if (counting) ++seeks;
        return MemoryStream::SetCurrentPos(p);
    }
};

struct Mark { const char* name; uint32_t pos; double ms; };

uint8_t* g_buf = nullptr;   // one pre-touched buffer for every flat walk (no first-touch faults)
void Attach(ProbeStream& ps) {
    if (!g_buf) { g_buf = (uint8_t*)std::malloc(kCap); std::memset(g_buf, 0, kCap); }
    ps._data = g_buf; ps._size = kCap; ps._cursor = 0; ps._direction = 1; ps._ownsData = 0;
}

struct ObjRec {
    int index; uint32_t type; uint32_t start, end;
    uint64_t exclHash; uint32_t exclBytes;
    std::vector<std::pair<uint32_t, uint32_t>> kids;   // child ranges
};

struct Image {
    std::vector<uint8_t> bytes;
    std::vector<Mark> marks;
    std::unordered_map<int, ObjRec> objs;   // by factory index
    std::vector<int> order;
};

ProbeStream* g_active = nullptr;
std::vector<Mark>* g_marks = nullptr;
std::vector<ObjRec> g_stack;
Image* g_img = nullptr;
int g_savesSeen = 0;
int g_updatesAfter = -1;
Image g_base;
double g_baseMs = 0;

uint64_t Fnv(const uint8_t* p, size_t n, uint64_t h) {
    for (size_t i = 0; i < n; ++i) { h ^= p[i]; h *= 1099511628211ull; }
    return h;
}

void PersistHook(int phase, IO::Stream& stream, int index, uint32_t type) {
    if (&stream != g_active || !g_img) return;
    const uint32_t pos = stream.GetCurrentPos();
    if (phase == 0) {
        ObjRec r{}; r.index = index; r.type = type; r.start = pos;
        g_stack.push_back(std::move(r));
        return;
    }
    ObjRec r = std::move(g_stack.back());
    g_stack.pop_back();
    r.end = pos;
    if (!g_stack.empty()) g_stack.back().kids.push_back({r.start, r.end});
    g_img->objs[index] = std::move(r);
    g_img->order.push_back(index);
}

void FinishHashes(Image& img) {
    const uint8_t* b = img.bytes.data();
    for (auto& kv : img.objs) {
        ObjRec& r = kv.second;
        uint64_t h = 1469598103934665603ull;
        uint32_t at = r.start, excl = 0;
        for (auto& k : r.kids) {
            if (k.first > at) { h = Fnv(b + at, k.first - at, h); excl += k.first - at; }
            at = k.second;
        }
        if (r.end > at) { h = Fnv(b + at, r.end - at, h); excl += r.end - at; }
        r.exclHash = h; r.exclBytes = excl;
        r.kids.clear(); r.kids.shrink_to_fit();
    }
}

const char* TypeName(uint32_t typeId) {
    using TypeNode = Object::Factory::TypeSet::Node;
    Object::Factory* f = Object::Factory::_singleton;
    TypeNode* node = f ? f->_types._root : nullptr;
    while (node) {
        if (typeId < node->element.typeId) { node = node->left; continue; }
        if (node->element.typeId >= typeId) {
            const Object::BaseTypeInfo* info = node->element.record.info;
            return info ? info->GetName() : "?";
        }
        node = node->right;
    }
    return "?";
}

double Walk(GameUpdateLoop* gul, ProbeStream& ps, Image* img, std::vector<Mark>* marks) {
    Attach(ps);
    g_active = &ps; g_img = img; g_marks = marks; g_stack.clear();
    if (img) Object::InternalFactory::_probePersist = &PersistHook;
    const double t0 = LoadTime::NowMs();
    gul->SerializeAll(ps);
    const double ms = LoadTime::NowMs() - t0;
    Object::InternalFactory::_probePersist = nullptr;
    g_active = nullptr; g_img = nullptr; g_marks = nullptr;
    return ms;
}

void Capture(GameUpdateLoop* gul, Image& img, double* msOut) {
    alignas(16) uint8_t st[sizeof(ProbeStream)];
    ProbeStream* ps = ::new (st) ProbeStream();
    img = Image();
    *msOut = Walk(gul, *ps, &img, &img.marks);
    img.bytes.assign((uint8_t*)ps->_data, (uint8_t*)ps->_data + ps->hiWater);
    ps->~ProbeStream();
    FinishHashes(img);
}

void PrintSections(const char* tag, const Image& img) {
    for (size_t i = 0; i < img.marks.size(); ++i) {
        const uint32_t end = i + 1 < img.marks.size() ? img.marks[i + 1].pos : (uint32_t)img.bytes.size();
        const double ms = i + 1 < img.marks.size() ? img.marks[i + 1].ms - img.marks[i].ms : 0.0;
        ErrLine("[SaveSnap] %s section %-10s bytes=%u walk_ms=%.2f\n", tag, img.marks[i].name,
                end - img.marks[i].pos, ms);
    }
}

void Compare(const char* tag, const Image& a, const Image& b) {
    // sections
    for (size_t i = 0; i < a.marks.size() && i < b.marks.size(); ++i) {
        const uint32_t ae = i + 1 < a.marks.size() ? a.marks[i + 1].pos : (uint32_t)a.bytes.size();
        const uint32_t be = i + 1 < b.marks.size() ? b.marks[i + 1].pos : (uint32_t)b.bytes.size();
        const uint32_t an = ae - a.marks[i].pos, bn = be - b.marks[i].pos;
        uint32_t diff = 0;
        const uint32_t n = std::min(an, bn);
        for (uint32_t k = 0; k < n; ++k)
            if (a.bytes[a.marks[i].pos + k] != b.bytes[b.marks[i].pos + k]) ++diff;
        ErrLine("[SaveSnap] %s section %-10s sizeA=%u sizeB=%u sameSize=%d bytesDiffering(prefix)=%u\n",
                tag, a.marks[i].name, an, bn, an == bn ? 1 : 0, diff);
    }
    // objects
    struct T { int total = 0, changed = 0, gone = 0; uint64_t bytes = 0, changedBytes = 0; };
    std::map<uint32_t, T> byType;
    int total = 0, changed = 0, gone = 0, born = 0;
    uint64_t exclAll = 0, exclChanged = 0;
    for (int idx : a.order) {
        const ObjRec& ra = a.objs.at(idx);
        T& t = byType[ra.type];
        ++t.total; ++total; t.bytes += ra.exclBytes; exclAll += ra.exclBytes;
        auto it = b.objs.find(idx);
        if (it == b.objs.end() || it->second.type != ra.type) { ++t.gone; ++gone; continue; }
        if (it->second.exclHash != ra.exclHash) {
            ++t.changed; ++changed; t.changedBytes += ra.exclBytes; exclChanged += ra.exclBytes;
        }
    }
    for (int idx : b.order) if (!a.objs.count(idx)) ++born;
    ErrLine("[SaveSnap] %s objects=%d changed=%d (%.1f%%) gone=%d born=%d exclBytes=%llu changedExclBytes=%llu (%.1f%%)\n",
            tag, total, changed, total ? 100.0 * changed / total : 0.0, gone, born,
            (unsigned long long)exclAll, (unsigned long long)exclChanged,
            exclAll ? 100.0 * (double)exclChanged / (double)exclAll : 0.0);
    std::vector<std::pair<uint32_t, T>> v(byType.begin(), byType.end());
    std::sort(v.begin(), v.end(), [](auto& x, auto& y) { return x.second.total > y.second.total; });
    int shown = 0;
    for (auto& e : v) {
        if (shown++ >= 40) break;
        ErrLine("[SaveSnap] %s   type %-32s n=%6d changed=%6d (%5.1f%%) gone=%d bytes=%llu\n", tag,
                TypeName(e.first), e.second.total, e.second.changed,
                e.second.total ? 100.0 * e.second.changed / e.second.total : 0.0, e.second.gone,
                (unsigned long long)e.second.bytes);
    }
}

bool Decompress(Save::SavedState* s, std::vector<uint8_t>& out) {
    alignas(16) uint8_t st[sizeof(IO::CompressedMemoryStream)];
    IO::CompressedMemoryStream* cms = ::new (st) IO::CompressedMemoryStream();
    s->Retrieve(cms->GetMemoryStream());
    if (!cms->Open()) { cms->IO::CompressedMemoryStream::~CompressedMemoryStream(); return false; }
    const uint32_t size = cms->IO::CompressedStream::GetSize();
    out.resize(size);
    const bool ok = size == 0 || cms->IO::CompressedStream::Read((int)size, out.data());
    cms->IO::CompressedMemoryStream::Close();
    cms->IO::CompressedMemoryStream::~CompressedMemoryStream();
    // Retrieve hands the buffer to the stream without ownership? keep the state intact
    return ok;
}

}  // namespace

void SaveSnapProbe_Mark(IO::Stream& stream, const char* name) {
    if (&stream != g_active || !g_marks) return;
    g_marks->push_back({name, stream.GetCurrentPos(), LoadTime::NowMs()});
}

#ifndef SAVESNAP_LIGHT
#define SAVESNAP_LIGHT 0
#endif
namespace {
Image g_u0; bool g_u0Pending = false; int g_drawsSinceU0 = 0;
bool g_inDraw = false; long long g_drawExtStart = 0, g_drawExt = 0, g_updExt = 0, g_updExtStart = 0;
int g_drawWindows = 0, g_updWindows = 0; bool g_countArmed = false;
}
namespace { std::vector<std::pair<char, double>> g_tl; double g_saveT0 = 0; int g_tlLeft = 0; }
void SaveSnapProbe_UpdateBegin(GameUpdateLoop* gul) {
    if (g_countArmed) {
        if (g_inDraw) { g_drawExt += g_probeExtCalls - g_drawExtStart; ++g_drawWindows; g_inDraw = false; }
        g_updExtStart = g_probeExtCalls;
    }
    if (g_u0Pending) {
        g_u0Pending = false;
        Image img; double ms = 0;
        Capture(gul, img, &ms);
        char tag[48]; std::snprintf(tag, sizeof(tag), "afterDraws(%d)", g_drawsSinceU0);
        ErrLine("[SaveSnap] %s: the image at the next Update's start vs the save Update's end\n", tag);
        Compare(tag, g_u0, img);
        g_u0 = Image();
        g_countArmed = true;
    }
}
void SaveSnapProbe_Event(char kind) {
    if (kind == 'D') {
        ++g_drawsSinceU0;
        if (g_countArmed) {
            if (g_inDraw) { g_drawExt += g_probeExtCalls - g_drawExtStart; ++g_drawWindows; }
            g_inDraw = true; g_drawExtStart = g_probeExtCalls;
            if (g_drawWindows >= 3000) {
                ErrLine("[SaveSnap] ExternalFactory Find/FindOrLoad/GetReferenceId calls: %lld in %d Draw windows "
                        "(Draw start to the next Draw or Update), %lld in %d Updates\n",
                        g_drawExt, g_drawWindows, g_updExt, g_updWindows);
                g_countArmed = false;
            }
        }
    }
    if (kind == 'U' && g_countArmed) { g_updExt += g_probeExtCalls - g_updExtStart; ++g_updWindows; }
    if (g_tlLeft <= 0) return;
    g_tl.push_back({kind, LoadTime::NowMs() - g_saveT0});
    if (--g_tlLeft == 0) {
        std::string line;
        char b[32];
        for (auto& e : g_tl) { std::snprintf(b, sizeof(b), " %c%.1f", e.first, e.second); line += b; }
        ErrLine("[SaveSnap] timeline after the save (ms from the save's end):%s\n", line.c_str());
    }
}

void SaveSnapProbe_AfterSave(GameUpdateLoop* gul, Save::SavedState* saved, double realMs) {
    if (!g_pfProfOn || g_savesSeen++ != 0) return;
    if (SAVESNAP_LIGHT) {
        ErrLine("[SaveSnap] light: real SaveRestore %.2f ms\n", realMs);
        g_saveT0 = LoadTime::NowMs(); g_tlLeft = 60; g_tl.clear();
        return;
    }

    PROCESS_MEMORY_COUNTERS_EX pmc{}; pmc.cb = sizeof(pmc);
    GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS*)&pmc, sizeof(pmc));
    const int objCount = Object::InternalFactory::Get()->GetObjects()._size;
    ErrLine("[SaveSnap] real SaveRestore %.2f ms; process private=%.1f MB ws=%.1f MB; factory objects=%d\n",
            realMs, pmc.PrivateUsage / 1048576.0, pmc.WorkingSetSize / 1048576.0, objCount);

    // 1. counted walk with the object log
    alignas(16) uint8_t st[sizeof(ProbeStream)];
    {
        ProbeStream* ps = ::new (st) ProbeStream();
        ps->counting = true;
        const long long s0 = g_probeSerializerCount;
        g_base = Image();
        const double ms = Walk(gul, *ps, &g_base, &g_base.marks);
        g_baseMs = ms;
        g_base.bytes.assign((uint8_t*)ps->_data, (uint8_t*)ps->_data + ps->hiWater);
        ErrLine("[SaveSnap] counted walk %.2f ms: writes=%llu bytes=%llu seeks=%llu serializers=%lld "
                "persisted=%zu size=%u | n1=%llu n2=%llu n4=%llu n8=%llu n<64=%llu n<4k=%llu big=%llu\n",
                ms, (unsigned long long)ps->writes, (unsigned long long)ps->bytes,
                (unsigned long long)ps->seeks, g_probeSerializerCount - s0, g_base.order.size(),
                ps->hiWater, (unsigned long long)ps->bySize[0], (unsigned long long)ps->bySize[1],
                (unsigned long long)ps->bySize[2], (unsigned long long)ps->bySize[3],
                (unsigned long long)ps->bySize[4], (unsigned long long)ps->bySize[5],
                (unsigned long long)ps->bySize[6]);
        const uint64_t writes = ps->writes;
        ps->~ProbeStream();
        FinishHashes(g_base);
        PrintSections("base", g_base);

        // 2. the real save's image
        std::vector<uint8_t> real;
        const bool ok = Decompress(saved, real);
        size_t firstDiff = SIZE_MAX;
        if (ok) for (size_t i = 0; i < std::min(real.size(), g_base.bytes.size()); ++i)
            if (real[i] != g_base.bytes[i]) { firstDiff = i; break; }
        ErrLine("[SaveSnap] real image ok=%d size=%zu flat size=%zu identical=%d firstDiff=%lld\n",
                ok ? 1 : 0, real.size(), g_base.bytes.size(),
                (ok && real.size() == g_base.bytes.size() && firstDiff == SIZE_MAX) ? 1 : 0,
                firstDiff == SIZE_MAX ? -1LL : (long long)firstDiff);

        // 3. plain flat walks (no counting, no object hook), and null-write walks
        double best = 1e9, bestNull = 1e9;
        bool same = true;
        for (int r = 0; r < 3; ++r) {
            ProbeStream* p2 = ::new (st) ProbeStream();
            const double t = Walk(gul, *p2, nullptr, nullptr);
            best = std::min(best, t);
            if (p2->hiWater != g_base.bytes.size() ||
                std::memcmp(p2->_data, g_base.bytes.data(), p2->hiWater) != 0) same = false;
            p2->~ProbeStream();
        }
        for (int r = 0; r < 3; ++r) {
            ProbeStream* p3 = ::new (st) ProbeStream();
            p3->nullWrites = true;
            bestNull = std::min(bestNull, Walk(gul, *p3, nullptr, nullptr));
            p3->~ProbeStream();
        }
        ErrLine("[SaveSnap] flat walk best of 3 %.2f ms (replays identical=%d); null-write walk best of 3 %.2f ms\n",
                best, same ? 1 : 0, bestNull);

        // 4. the per-field chain: `writes` Serialize(int&) calls into the null stream and into the
        //    pre-touched flat stream, and the same count of 4-byte appends with no call chain
        {
            double chainNull = 0, chainFlat = 0, raw = 0;
            for (int mode = 0; mode < 2; ++mode) {
                ProbeStream* p4 = ::new (st) ProbeStream();
                Attach(*p4);
                p4->nullWrites = (mode == 0);
                const double t0 = LoadTime::NowMs();
                {
                    IO::Serializer ser(*p4, 0);
                    int v = 0;
                    for (uint64_t i = 0; i < writes; ++i) {
                        if (p4->_cursor > (uint32_t)kCap - 64) p4->_cursor = 16;
                        v = (int)i;
                        ser.Serialize(v, nullptr);
                    }
                }
                (mode == 0 ? chainNull : chainFlat) = LoadTime::NowMs() - t0;
                p4->~ProbeStream();
            }
            volatile uint32_t sink = 0;
            const double t1 = LoadTime::NowMs();
            uint32_t at = 0;
            for (uint64_t i = 0; i < writes; ++i) {
                uint32_t v = (uint32_t)i;
                std::memcpy(g_buf + at, &v, 4);
                at += 3; if (at >= (uint32_t)kCap - 8) at = 0;
            }
            sink = g_buf[at / 2];
            raw = LoadTime::NowMs() - t1;
            (void)sink;
            ErrLine("[SaveSnap] per-field chain x%llu: Serialize(int&) into null stream %.2f ms (%.2f ns), "
                    "into flat stream %.2f ms (%.2f ns); raw 4-byte appends %.2f ms (%.2f ns)\n",
                    (unsigned long long)writes, chainNull, chainNull * 1e6 / (double)writes,
                    chainFlat, chainFlat * 1e6 / (double)writes, raw, raw * 1e6 / (double)writes);
        }
    }
    g_updatesAfter = 0;
}

void SaveSnapProbe_UpdateEnd(GameUpdateLoop* gul) {
    if (g_updatesAfter < 0) return;
    ++g_updatesAfter;   // 1 = the save's own Update
    const int after = g_updatesAfter - 1;
    if (after == 0 || after == 1 || after == 2 || after == 10) {
        Image img; double ms = 0;
        Capture(gul, img, &ms);
        char tag[32]; std::snprintf(tag, sizeof(tag), "u+%d", after);
        ErrLine("[SaveSnap] %s image %zu bytes, walk %.2f ms\n", tag, img.bytes.size(), ms);
        Compare(tag, g_base, img);
        if (after == 0) { g_u0 = img; g_u0Pending = true; g_drawsSinceU0 = 0; }
        if (after == 10) { g_updatesAfter = -1; g_base = Image(); }
    }
}
