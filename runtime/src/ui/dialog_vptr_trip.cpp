// ============================================================================
// runtime/src/ui/dialog_vptr_trip.cpp — WAVE 10E LANE FINAL
// ============================================================================
// See dialog_vptr_trip.h for what this measures and why lane DLG8's counter
// could not measure it.
//
// THE FORWARDING THUNKS
// ---------------------------------------------------------------------------
// Nine 2-instruction entries, one per slot, each loading its slot index into
// R10D and jumping to one shared trampoline. R10 is volatile and is not an
// argument register in the Microsoft x64 ABI, so loading it cannot disturb the
// call it is about to forward.
//
// The trampoline saves the four integer argument registers, calls a C recorder
// with the slot index, gets the REAL slot body back from it in RAX, restores
// the arguments and TAIL-JUMPS. The return address is untouched, so the real
// body returns straight to the original caller and the thunk is invisible in
// everything except the counter.
//
// XMM0-3 are NOT saved, deliberately: none of `UI::Dialog`'s nine virtuals
// takes a floating-point argument (slot 0 `(this,int)`, 1/2/3 `(this)`,
// 4 `(this,...)`, 5 `(this,Stream&)`, 6/7/8 `(this)`), and saving registers a
// probe does not need is how a probe grows a bug of its own. If a future
// Dialog slot ever takes a float this file must save them — that is stated
// here because a silent omission is the failure mode.
//
// The thunks carry NO UNWIND INFO. An exception thrown through one would not
// unwind. None of the nine bodies throws (they are constant loads, in-place
// constructs and a Serialize whose error path returns false), and the
// instrument is off in every ordinary run.
//
// STACK ALIGNMENT: on entry RSP % 16 == 8 (the call pushed the return
// address). `sub 0x48` makes RSP % 16 == 0, so the `call` inside leaves the
// recorder's own entry at RSP % 16 == 8, which is what the ABI requires. The
// 32 bytes at [RSP,RSP+0x20) are the shadow store the recorder is entitled to.
// ============================================================================

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "dialog_vptr_trip.h"

#include "../../funcs/rt_port.h"

namespace {

constexpr int kSlots = 9;

const char* const kSlotName[kSlots] = {
    "slot0_vectorDeletingDtor", "slot1_TypeName", "slot2_Type",
    "slot3_GetFileVersion",     "slot4_Compile",  "slot5_Serialize",
    "slot6_Reconstruct",        "slot7_Destruct", "slot8_Construct",
};

// The DLL's own `??_7Dialog@UI@@6B@`, read RAW. Raw is correct and is not a
// bypass: the tripwire must forward to whatever the stamp WOULD have been, and
// the row is unpromoted, so the stamp is the DLL's table. If the row is ever
// promoted this must become ResolveExport — flagged at the read, not here.
void* const* RealTable() {
    static void* const* t = nullptr;
    if (!t) t = reinterpret_cast<void* const*>(
                 ResolveExportRaw("??_7Dialog@UI@@6B@"));
    return t;
}

int Mode() {
    static int m = -1;
    if (m < 0) {
        const char* e = getenv("BANISHED_DLG_VPTR_TRIP");
        m = (e && *e && std::strcmp(e, "0") != 0) ? std::atoi(e) : 0;
        if (m < 0) m = 0;
    }
    return m;
}

unsigned long long g_hits[kSlots] = {0};
unsigned long long g_stamped     = 0;   // objects that got the tripwire table
unsigned long long g_controlFired = 0;  // mode-2 dispatches WE made

// The tripwire table itself. Filled on first use from the thunk addresses.
void* g_tripTable[kSlots] = {nullptr};

struct TripReport {
    ~TripReport() {
        if (Mode() == 0) return;
        unsigned long long total = 0;
        for (int i = 0; i < kSlots; ++i) total += g_hits[i];
        // `totalDispatch` INCLUDES the mode-2 control, so the number that
        // answers the row's question is `totalDispatch - control`. Both are
        // printed; the derived one is printed too, so nobody has to subtract.
        printf("[DlgTrip] mode=%d stamped=%llu control=%llu totalDispatch=%llu "
                    "foreignDispatch=%llu\n",
                    Mode(), g_stamped, g_controlFired, total, total - g_controlFired);
        if (total == 0) {
            printf("[DlgTrip] NO SLOT OF ??_7Dialog@UI@@6B@ WAS DISPATCHED "
                        "in this run (all nine counted).\n");
        } else {
            for (int i = 0; i < kSlots; ++i)
                if (g_hits[i])
                    printf("[DlgTrip]   %-26s = %llu\n", kSlotName[i], g_hits[i]);
        }
        // The instrument's own effectiveness statement, printed next to the
        // reading rather than left to a lane doc: in mode 2 the control MUST
        // appear in slot3, and if it does not, the zero above means nothing.
        if (Mode() >= 2) {
            printf("[DlgTrip] POSITIVE CONTROL: fired=%llu observed-in-slot3=%llu "
                        "-- %s\n",
                        g_controlFired, g_hits[3],
                        (g_controlFired > 0 && g_hits[3] >= g_controlFired)
                            ? "CAUGHT (the instrument discriminates)"
                            : "NOT CAUGHT (this run's zero proves nothing)");
        }
    }
};
TripReport g_report;

} // namespace

// The recorder. Returns the real body so the trampoline can tail-jump to it.
extern "C" void* RtUiDlgTripRecord(int slot) {
    if (slot >= 0 && slot < kSlots) ++g_hits[slot];
    void* const* t = RealTable();
    if (!t || slot < 0 || slot >= kSlots) {
        std::fprintf(stderr, "[DlgTrip] FATAL: slot %d dispatched with no real "
                             "table to forward to.\n", slot);
        std::fflush(stderr);
        abort();
    }
    return t[slot];
}

extern "C" void RtUiDlgTrip0(); extern "C" void RtUiDlgTrip1();
extern "C" void RtUiDlgTrip2(); extern "C" void RtUiDlgTrip3();
extern "C" void RtUiDlgTrip4(); extern "C" void RtUiDlgTrip5();
extern "C" void RtUiDlgTrip6(); extern "C" void RtUiDlgTrip7();
extern "C" void RtUiDlgTrip8();

asm(
".text\n"
".p2align 4\n"
"RtUiDlgTripCommon:\n"
"    subq  $0x48, %rsp\n"
"    movq  %rcx, 0x20(%rsp)\n"
"    movq  %rdx, 0x28(%rsp)\n"
"    movq  %r8,  0x30(%rsp)\n"
"    movq  %r9,  0x38(%rsp)\n"
"    movl  %r10d, %ecx\n"
"    call  RtUiDlgTripRecord\n"
"    movq  %rax, 0x40(%rsp)\n"
"    movq  0x20(%rsp), %rcx\n"
"    movq  0x28(%rsp), %rdx\n"
"    movq  0x30(%rsp), %r8\n"
"    movq  0x38(%rsp), %r9\n"
"    movq  0x40(%rsp), %rax\n"
"    addq  $0x48, %rsp\n"
"    jmp   *%rax\n"
".globl RtUiDlgTrip0\n" "RtUiDlgTrip0:\n movl $0, %r10d\n jmp RtUiDlgTripCommon\n"
".globl RtUiDlgTrip1\n" "RtUiDlgTrip1:\n movl $1, %r10d\n jmp RtUiDlgTripCommon\n"
".globl RtUiDlgTrip2\n" "RtUiDlgTrip2:\n movl $2, %r10d\n jmp RtUiDlgTripCommon\n"
".globl RtUiDlgTrip3\n" "RtUiDlgTrip3:\n movl $3, %r10d\n jmp RtUiDlgTripCommon\n"
".globl RtUiDlgTrip4\n" "RtUiDlgTrip4:\n movl $4, %r10d\n jmp RtUiDlgTripCommon\n"
".globl RtUiDlgTrip5\n" "RtUiDlgTrip5:\n movl $5, %r10d\n jmp RtUiDlgTripCommon\n"
".globl RtUiDlgTrip6\n" "RtUiDlgTrip6:\n movl $6, %r10d\n jmp RtUiDlgTripCommon\n"
".globl RtUiDlgTrip7\n" "RtUiDlgTrip7:\n movl $7, %r10d\n jmp RtUiDlgTripCommon\n"
".globl RtUiDlgTrip8\n" "RtUiDlgTrip8:\n movl $8, %r10d\n jmp RtUiDlgTripCommon\n"
);

namespace RtUiDlgTrip {

void* TableOrReal(void* real) {
    if (Mode() == 0) return real;
    if (!RealTable()) return real;          // no oracle -> no instrument
    if (!g_tripTable[0]) {
        g_tripTable[0] = (void*)&RtUiDlgTrip0; g_tripTable[1] = (void*)&RtUiDlgTrip1;
        g_tripTable[2] = (void*)&RtUiDlgTrip2; g_tripTable[3] = (void*)&RtUiDlgTrip3;
        g_tripTable[4] = (void*)&RtUiDlgTrip4; g_tripTable[5] = (void*)&RtUiDlgTrip5;
        g_tripTable[6] = (void*)&RtUiDlgTrip6; g_tripTable[7] = (void*)&RtUiDlgTrip7;
        g_tripTable[8] = (void*)&RtUiDlgTrip8;
        printf("[DlgTrip] ARMED mode=%d real=%p trip=%p\n",
                    Mode(), (const void*)RealTable(), (void*)g_tripTable);
    }
    ++g_stamped;
    return (void*)g_tripTable;
}

void AfterStamp(void* obj) {
    if (Mode() < 2 || !obj) return;
    if (g_controlFired) return;             // once, on the first object only
    void* const* vt = *reinterpret_cast<void* const* const*>(obj);
    if (vt != g_tripTable) return;          // not a stamped object
    ++g_controlFired;
    reinterpret_cast<int (*)(const void*)>(vt[3])(obj);   // GetFileVersion
}

} // namespace RtUiDlgTrip
