// ============================================================================
// runtime/src/ui/dialog_settings_probe.cpp — the oracle for SaveSettings and
// OnClosed.  Wave 9 lane ENC-2.
// ============================================================================
//
// A harness, not a port: nothing here is registered.
//
// ----------------------------------------------------------------------------
// A SYNTHETIC SUBJECT, BECAUSE A LIVE ONE CANNOT BE REPLAYED
// ----------------------------------------------------------------------------
// SaveSettings writes into the DialogInstance's own settings buffer and
// dispatches into whatever controllers that dialog owns. Running THEIRS
// alongside OURS on a live dialog would write the record twice into the same
// buffer and invoke every controller's serializer twice. So the subject is
// built here: a DialogInstance-shaped object with a fake vtable, its own
// buffer, and a fake controller array, one instance per side.
//
// WHAT IS NOT FAKED is the machinery under test — the real ported MemoryStream
// constructor and destructor, the real Serializer constructor, the real
// IO::Encoding factories and the real BinaryWriter bodies all run. The stubs
// stand in only for the two things that belong to the CALLER rather than to
// SaveSettings: the dialog's own OnSerializeSettings and the controllers.
//
// The stubs are shared by both sides and are stateless with respect to which
// side is running: each is handed the stream it must write to, so it writes
// into whichever buffer its caller set up. A stub that behaved differently for
// ours and theirs would manufacture a divergence, and one that ignored its
// argument would hide one.
//
// ----------------------------------------------------------------------------
// WHAT THIS ISOLATES, AND WHAT IT DELIBERATELY DOES NOT RE-TEST
// ----------------------------------------------------------------------------
// Registering a function does NOT rewrite the pointer sitting in the shipped
// DLL's own .rdata vtable. So when OUR SaveSettings asks the stream's Encoding
// for a writer, it goes through the DLL's Encoding table to the DLL's
// CreateWriter — exactly as THEIRS does. Both sides therefore drive the SAME
// codec bodies, and any difference in the finished record is a difference in
// SaveSettings' own logic: the field ORDER, the framing NESTING, the loop
// decisions. The codec bodies are covered separately and on their own terms by
// binary_writer_probe.cpp. Keeping the two apart is the point; a probe that
// re-tested everything at once could not say which layer moved.
//
// ----------------------------------------------------------------------------
// WHAT IS COMPARED
// ----------------------------------------------------------------------------
//   * the finished settings BUFFER, byte for byte — the emission sequence
//   * the stream cursor (how many bytes the record consumed)
//   * the number of times each stub was invoked, per side, and IN WHICH ORDER
//     (a trace string), because a port that emitted identical bytes while
//     calling the controllers in the wrong order, or calling one twice, would
//     otherwise pass
//
// The call TRACE is the check that byte equality alone would not give: the
// per-controller records are framed identically, so two controllers whose
// payloads happened to match would produce equal bytes from the wrong order.
//
// ----------------------------------------------------------------------------
// CONTROLS
// ----------------------------------------------------------------------------
// EFFECTIVENESS — the corpus varies the controller COUNT (0, 1, 3), so the
// record length, the nesting depth and the serialized count all differ between
// cases. The zero-controller case matters on its own: it is the one that proves
// the outer record is still opened and closed when the loop body never runs.
//
// NEGATIVE — a known-good record is copied, one byte flipped, and the
// comparator must say no.
//
// **EVERY COUNTER ACCUMULATES BEFORE ANY VERDICT CHECK AND NONE IS GUARDED BY
// `!bad`.** Lane ENC's first run printed `emitted=0` because a control was only
// accumulated while the run was clean, so a failure in one place zeroed a
// counter that was about something else entirely. A control that stops counting
// when something else fails cannot fire when it is needed.
//
// ----------------------------------------------------------------------------
// HONEST LIMITS
// ----------------------------------------------------------------------------
//  1. **THE TWO LOOP-BOUND DECISIONS ARE NOT DISTINGUISHED.** SaveSettings
//     snapshots `_controllers._size` while OnClosed re-reads it every
//     iteration. Telling those apart needs a controller that MUTATES the array
//     from inside its own callback, which this probe does not do — it would be
//     testing a scenario no caller in the game produces, and a wrong guess
//     about what retail does when the array moves under it would manufacture a
//     green. Both behaviours are transcribed from the listings and are
//     UNVERIFIED BY THIS ORACLE. This is the largest single gap here.
//  2. ONE STREAM CLASS, one Encoding, one buffer size class.
//  3. THE `size < 4` PATH (no head magic) IS NOT EXERCISED — every case has a
//     real buffer. Nor is `_userData == nullptr`, beyond a single early-out case.
//  4. The DIALOG's own +0x48 and the controllers' +0x10/+0x50/+0x30 are STUBS.
//     This says nothing about the real implementations; it says SaveSettings
//     calls them correctly.
//  5. NOT PROOF THAT THE PORT RUNS — attendance is measured separately, see
//     WAVE9_LANE_ENC2.md.
// ============================================================================

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "../../funcs/rt_port.h"
#include "../../types/ui/DialogInstance.h"

enum class Module : int { Exe = 0, Runtime = 1, VideoDX11 = 2, COUNT = 3 };
uintptr_t ResolveAddr(Module module, uintptr_t ghidraAddr);

namespace RtUi {

using RtUI::DialogInstance;

extern "C" void RtUi_DialogInstance_SaveSettings(DialogInstance*);
extern "C" void RtUi_DialogInstance_OnClosed(DialogInstance*);

namespace {

using SaveFn = void (*)(DialogInstance*);

// ---- the synthetic controller ---------------------------------------------
// Layout is ours; only the vptr position matters to the code under test.
struct FakeController {
    void**   vptr;
    uint32_t id;
    int32_t  closedCalls;
    char*    trace;       // shared per-side trace buffer
    size_t*  traceLen;
};

using StreamWriteFn = bool (*)(void* stream, int32_t n, void* buf);

void Emit(char* trace, size_t* len, char tag, uint32_t id) {
    if (*len + 8 >= 256) return;
    *len += (size_t)std::snprintf(trace + *len, 256 - *len, "%c%u ", tag, id);
}

void StreamWrite4(void* stream, uint32_t v) {
    void** vt = *reinterpret_cast<void***>(stream);
    reinterpret_cast<StreamWriteFn>(vt[0x10 / 8])(stream, 4, &v);
}

uint32_t Stub_Ctrl_Type(void* self) {
    FakeController* c = static_cast<FakeController*>(self);
    Emit(c->trace, c->traceLen, 'T', c->id);
    return c->id;
}

void Stub_Ctrl_OnSerializeSettings(void* self, void* stream) {
    FakeController* c = static_cast<FakeController*>(self);
    Emit(c->trace, c->traceLen, 'S', c->id);
    StreamWrite4(stream, 0xC0000000u | c->id);
    StreamWrite4(stream, 0xC1000000u | (c->id * 3u));
}

void Stub_Ctrl_OnClosed(void* self) {
    FakeController* c = static_cast<FakeController*>(self);
    ++c->closedCalls;
    Emit(c->trace, c->traceLen, 'C', c->id);
}

// ---- the synthetic dialog ---------------------------------------------------
// A DialogInstance-shaped block. The port reads the vptr at +0, _controllers at
// +0x78, _userData at +0xa8 and _userFlags at +0xb0; everything else is zero.
struct Subject {
    alignas(16) uint8_t obj[0xb8];
    void*   ctrlVtbl[12];
    void*   dlgVtbl[16];
    FakeController ctrls[4];
    void*   ctrlPtrs[4];
    uint8_t buf[512];
    char    trace[256];
    size_t  traceLen;
};

void Stub_Dlg_OnSerializeSettings(void* self, void* stream) {
    // `obj` is Subject's FIRST member, so the object address the dialog was
    // called on IS the Subject address. That is how each side's stub finds its
    // own trace buffer without any global telling it which side it is.
    Subject* s = reinterpret_cast<Subject*>(self);
    Emit(s->trace, &s->traceLen, 'D', 0);
    StreamWrite4(stream, 0xD0000000u);
}

void BuildSubject(Subject* s, int nCtrl) {
    std::memset(s, 0, sizeof(*s));
    s->traceLen = 0;

    for (int i = 0; i < 12; ++i) s->ctrlVtbl[i] = nullptr;
    s->ctrlVtbl[0x10 / 8] = (void*)&Stub_Ctrl_Type;
    s->ctrlVtbl[0x30 / 8] = (void*)&Stub_Ctrl_OnClosed;
    s->ctrlVtbl[0x50 / 8] = (void*)&Stub_Ctrl_OnSerializeSettings;

    for (int i = 0; i < 16; ++i) s->dlgVtbl[i] = nullptr;
    s->dlgVtbl[0x48 / 8] = (void*)&Stub_Dlg_OnSerializeSettings;

    for (int i = 0; i < nCtrl; ++i) {
        s->ctrls[i].vptr        = reinterpret_cast<void**>(s->ctrlVtbl);
        s->ctrls[i].id          = (uint32_t)(0x100 + i);
        s->ctrls[i].closedCalls = 0;
        s->ctrls[i].trace       = s->trace;
        s->ctrls[i].traceLen    = &s->traceLen;
        s->ctrlPtrs[i]          = &s->ctrls[i];
    }

    // vptr, then the three fields SaveSettings reads.
    //
    // NOTE the local: `&s->dlgVtbl` is the address OF THE ARRAY, and memcpy'ing
    // 8 bytes from there copies dlgVtbl[0] (a null slot), NOT the array's
    // address. The first draft did exactly that and the port then read slot
    // +0x48 off a null vptr, faulting on address 0x48 -- a null handed to the
    // port BY THIS HARNESS, which is why the fix belongs here and not in a
    // guard inside SaveSettings.
    void* dlgVtblPtr = s->dlgVtbl;          // array-to-pointer decay
    std::memcpy(s->obj + 0x00, &dlgVtblPtr, sizeof(void*));
    void* dataPtr = s->ctrlPtrs;
    std::memcpy(s->obj + 0x78, &dataPtr, sizeof(void*));            // _controllers._data
    std::memcpy(s->obj + 0x80, &nCtrl,   sizeof(int32_t));          // _controllers._size
    std::memcpy(s->obj + 0x84, &nCtrl,   sizeof(int32_t));          // _capacity
    void* bufPtr = s->buf;
    std::memcpy(s->obj + 0xa8, &bufPtr,  sizeof(void*));            // _userData
    const int32_t bufSize = (int32_t)sizeof(s->buf);
    std::memcpy(s->obj + 0xb0, &bufSize, sizeof(int32_t));          // _userFlags
}

struct Res {
    int64_t cases = 0, runs = 0, bytesEmitted = 0, stubCalls = 0;
    int     distinct = 0, zeroCtrlCase = 0, closedSeen = 0;
    bool    negativeFired = false;
    const char* bad = nullptr;
    int      badOff = -1;
};

}  // namespace

void RtUi_DialogSettingsProbe() {
    if (!RtDiffEnabled()) return;

    RT_DIFF_BEGIN("UI::DialogInstance::SaveSettings/OnClosed [probe]");

    SaveFn theirSave  = reinterpret_cast<SaveFn>(
        ResolveExportRaw("?SaveSettings@DialogInstance@UI@@QEAAXXZ"));
    SaveFn theirClose = reinterpret_cast<SaveFn>(
        ResolveExportRaw("?OnClosed@DialogInstance@UI@@UEAAXXZ"));
    if (!theirSave || !theirClose) {
        std::printf("[DlgSettings] probe SKIPPED: no shipped bodies to diff "
                    "against (expected once the DLL is gone).\n");
        return;
    }

    Res r;
    static Subject sa, sb;            // static: 2 x ~1.5 KB is too much for a stack frame
    static const int kCounts[] = { 0, 1, 3 };
    uint8_t firstBuf[sizeof(sa.buf)];
    bool haveFirst = false;

    for (int c = 0; c < 3; ++c) {
        const int n = kCounts[c];
        ++r.cases;
        if (n == 0) ++r.zeroCtrlCase;

        // ---- SaveSettings ---------------------------------------------------
        BuildSubject(&sa, n);
        BuildSubject(&sb, n);
        RtUi_DialogInstance_SaveSettings(reinterpret_cast<DialogInstance*>(sa.obj));
        theirSave(reinterpret_cast<DialogInstance*>(sb.obj));
        ++r.runs;

        // controls, unconditionally
        r.stubCalls += (int64_t)sa.traceLen;
        for (int i = 0; i < (int)sizeof(sa.buf); ++i)
            if (sa.buf[i] != 0) { r.bytesEmitted += 1; }
        if (!haveFirst) { std::memcpy(firstBuf, sa.buf, sizeof(firstBuf)); haveFirst = true; }
        else if (std::memcmp(firstBuf, sa.buf, sizeof(firstBuf)) != 0) ++r.distinct;
        if (c == 0) {
            uint8_t bad[sizeof(sa.buf)];
            std::memcpy(bad, sa.buf, sizeof(bad));
            bad[0] ^= 0xFFu;
            r.negativeFired = (std::memcmp(bad, sb.buf, sizeof(bad)) != 0);
        }

        if (!r.bad && std::memcmp(sa.buf, sb.buf, sizeof(sa.buf)) != 0) {
            for (int i = 0; i < (int)sizeof(sa.buf); ++i)
                if (sa.buf[i] != sb.buf[i]) { r.badOff = i; break; }
            r.bad = "SaveSettings record bytes";
        }
        if (!r.bad && std::strcmp(sa.trace, sb.trace) != 0) r.bad = "SaveSettings call trace";

        // ---- OnClosed -------------------------------------------------------
        BuildSubject(&sa, n);
        BuildSubject(&sb, n);
        RtUi_DialogInstance_OnClosed(reinterpret_cast<DialogInstance*>(sa.obj));
        theirClose(reinterpret_cast<DialogInstance*>(sb.obj));
        ++r.runs;
        r.stubCalls += (int64_t)sa.traceLen;
        for (int i = 0; i < n; ++i) r.closedSeen += sa.ctrls[i].closedCalls;

        if (!r.bad && std::memcmp(sa.buf, sb.buf, sizeof(sa.buf)) != 0) {
            for (int i = 0; i < (int)sizeof(sa.buf); ++i)
                if (sa.buf[i] != sb.buf[i]) { r.badOff = i; break; }
            r.bad = "OnClosed record bytes";
        }
        if (!r.bad && std::strcmp(sa.trace, sb.trace) != 0) r.bad = "OnClosed call trace";
        for (int i = 0; i < n; ++i)
            if (!r.bad && sa.ctrls[i].closedCalls != sb.ctrls[i].closedCalls)
                r.bad = "OnClosed controller dispatch count";
    }

    // ---- the null-buffer early-out, its own case ----------------------------
    {
        BuildSubject(&sa, 2);
        BuildSubject(&sb, 2);
        void* nul = nullptr;
        std::memcpy(sa.obj + 0xa8, &nul, sizeof(nul));
        std::memcpy(sb.obj + 0xa8, &nul, sizeof(nul));
        RtUi_DialogInstance_SaveSettings(reinterpret_cast<DialogInstance*>(sa.obj));
        theirSave(reinterpret_cast<DialogInstance*>(sb.obj));
        ++r.cases; ++r.runs;
        if (!r.bad && (sa.traceLen != 0 || sb.traceLen != 0))
            r.bad = "null _userData did not early-out";
        if (!r.bad && std::memcmp(sa.buf, sb.buf, sizeof(sa.buf)) != 0)
            r.bad = "null _userData wrote bytes";
    }

    const bool covered = r.runs > 0 && r.bytesEmitted > 0 && r.stubCalls > 0 &&
                         r.distinct > 0 && r.zeroCtrlCase > 0 &&
                         r.closedSeen > 0 && r.negativeFired;
    const bool pass = (r.bad == nullptr) && covered;

    char da[256], db[256];
    std::snprintf(da, sizeof(da),
                  "cases=%lld runs=%lld nonzeroBytes=%lld stubCalls=%lld "
                  "distinct=%d zeroCtrl=%d onClosed=%d neg=%s",
                  (long long)r.cases, (long long)r.runs,
                  (long long)r.bytesEmitted, (long long)r.stubCalls,
                  r.distinct, r.zeroCtrlCase, r.closedSeen,
                  r.negativeFired ? "FIRED" : "DID-NOT-FIRE");
    if (r.bad && r.badOff >= 0)
        std::snprintf(db, sizeof(db), "DIVERGED on '%s' at buffer +0x%02x",
                      r.bad, r.badOff);
    else if (r.bad)
        std::snprintf(db, sizeof(db), "DIVERGED on '%s'", r.bad);
    else if (!covered)
        std::snprintf(db, sizeof(db), "no divergence, but a CONTROL READS ZERO");
    else
        std::snprintf(db, sizeof(db),
                      "identical record bytes and identical call traces");
    RT_DIFF_SHADOW(pass, da, db);
    RT_DIFF_WORK(r.runs > 0);

    std::printf("[DlgSettings] subject probe: %s -- %s\n", da, db);
}

}  // namespace RtUi
