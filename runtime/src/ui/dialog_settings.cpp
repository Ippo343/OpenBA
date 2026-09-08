// ============================================================================
// runtime/src/ui/dialog_settings.cpp — the DialogInstance settings round-trip's
// write half.  Wave 9 lane ENC-2, bodies 5 and 6.
// ============================================================================
//
//   0x180114240  ?SaveSettings@DialogInstance@UI@@QEAAXXZ       (EXPORTED)
//   0x180115330  ?OnClosed@DialogInstance@UI@@UEAAXXZ           (EXPORTED)
//
// This closes the row three lanes declined (Z, UIC, UID). The chain that had to
// land first was: IO::Encoding's two factories and the two codec constructors
// (lane ENC), then BinaryWriter's two Serialize slots, its back-patching
// destructor and its deleting destructor (lane ENC-2, binary_writer.cpp). Only
// then does SaveSettings reach real bodies rather than minted traps.
//
// ----------------------------------------------------------------------------
// WHAT +0xa8 AND +0xb0 ACTUALLY ARE — a correction to DialogInstance.h
// ----------------------------------------------------------------------------
// runtime/types/ui/DialogInstance.h names them `_userData` (void*) and
// `_userFlags` (int), from the constructor's trailing `PEAX` and `H`
// parameters. The offsets are right and the provenance is right, but the ROLE
// is not what those names suggest: SaveSettings uses the pair as a
// **settings buffer and its byte length** —
//
//     if (_userData == nullptr) return;          // nothing to save into
//     ms.data = _userData;  ms.size = _userFlags;
//
// and `?LoadSettings@DialogInstance@UI@@` (0x180114450) is the exact mirror:
// same null guard on +0xa8, same MemoryStream, same two fields, but
// `direction = 0` and it READS the 0xDEADBEEF magic back out. So the pair is
// one caller-supplied blob plus its size, round-tripped by Save/Load.
//
// The header's field NAMES are left alone here — renaming them would ripple
// through every DialogInstance consumer for a documentation gain — but the role
// is recorded in both places. `_userFlags` being a BYTE LENGTH is the fact a
// future reader most needs; treating it as a bitfield would corrupt the buffer.
//
// This also independently re-confirms lane ENC's direction mapping (0 = read ->
// CreateReader, non-zero = write -> CreateWriter) from a second, unrelated call
// site.
//
// ----------------------------------------------------------------------------
// SaveSettings — transcribed from 0x180114240..0x18011443a
// ----------------------------------------------------------------------------
// The shape, with everything retail inlines named:
//
//   1. bail out if there is no buffer
//   2. MemoryStream ctor (a real CALL to 0x1800ae710), then the stream is
//      pointed AT THE CALLER'S BUFFER by hand: _data/_size/_cursor/_direction
//      are stored directly, NOT through Open() — nothing is allocated and the
//      buffer is not owned, which is why the destructor does not free it
//   3. if the buffer is at least 4 bytes, 0xDEADBEEF is stored into its first
//      dword **directly, not through the stream**, and the cursor is stepped
//      past it. LoadSettings reads that dword back, so it is the format's
//      head magic
//   4. an IO::Serializer is constructed on the stack (retail INLINES
//      ??0Serializer@IO@@QEAA@AEAVStream@1@H@Z; this calls the ported one,
//      which is the same code and keeps the factory-slot choice in one place)
//   5. the dialog's own virtual +0x48 (OnSerializeSettings) gets the STREAM
//   6. the controller count is serialized as an int
//   7. per controller: its type id as a uint, then A SECOND Serializer is built
//      and immediately destroyed around the controller's own +0x50
//   8. both ReaderWriters are released through vtable slot 0 with flags = 1
//   9. the MemoryStream is destroyed (retail inlines ~MemoryStream; this calls
//      the ported one, which is the same code)
//
// FOUR DETAILS THE DECOMPILED C WOULD HAVE FLATTENED:
//
//  * **The loop bound is SNAPSHOT, the array base is NOT.** `_controllers._size`
//    is copied to a stack slot once and every iteration compares against that
//    copy, so a controller added during serialization is not visited. But
//    `_controllers._data` is RE-READ from the object twice per iteration, so a
//    reallocation mid-loop is tolerated. Those are different decisions about
//    the same container and both are reproduced.
//  * **The second Serializer is created and destroyed around a call that never
//    receives it.** The controller's +0x50 is handed the STREAM, not the
//    ReaderWriter. Its only observable effect is the 12-byte header its
//    constructor emits and the trailer plus back-patch its destructor emits —
//    i.e. each controller's settings are WRAPPED IN THEIR OWN FRAMED RECORD.
//    That is the whole reason the framing exists, and it is invisible unless
//    the constructor and destructor are read together.
//  * **The branch selecting CreateReader vs CreateWriter is dead here**, and
//    deliberately kept. `direction` is set to 1 four instructions earlier and
//    the register the branch tests is a copy of it, so the writer arm always
//    wins. Retail still emits the test; collapsing it would be a divergence for
//    no gain, and the same inlined ctor appears in LoadSettings where the other
//    arm is the live one.
//  * **`_userFlags` is read as an int and used as an unsigned size.** A negative
//    value would make `size >= 4` false and leave `_size` huge; retail does not
//    guard it and neither does this. Callers supply a real length.
//
// ----------------------------------------------------------------------------
// OnClosed — transcribed from 0x180115330..0x18011538b
// ----------------------------------------------------------------------------
//     SaveSettings(this);
//     for (i = 0; i < _controllers._size; ++i)
//         _controllers._data[i]->vtable[+0x30]();
//
// Same container idiom as SaveSettings but NOT the same decisions: here the
// bound is re-read from the object every iteration (`CMP EBX,[RSI+0x80]`), so
// this loop DOES see a controller added during dispatch. Two loops over one
// container, three instructions apart in the binary, disagreeing about that —
// which is exactly the kind of thing a "cleaned up" port silently unifies.
//
// ----------------------------------------------------------------------------
// TWO CONTROLLER VTABLE SLOTS, NOW EXPORT-CONFIRMED
// ----------------------------------------------------------------------------
// runtime/src/ui/dialog.cpp recorded slot +0x30 as "a fifth DialogController
// vtable slot not yet in the enum ... a slot pinned by ONE unported caller is
// not yet evidence of a name." That caution is now dischargeable: the table at
// ??_7DialogController@UI@@6B@ (0x1801565f0) was read byte-wise and both slots
// resolve to exported names, so they are named by the EXPORT TABLE rather than
// by a caller:
//
//     +0x30  0x1800cd290  ?OnClosed@DialogController@UI@@UEAAXXZ
//     +0x50  0x1800cd250  ?OnSerializeSettings@DialogController@UI@@UEAAXAEAVStream@IO@@@Z
//
// (The same read also shows +0x10 is `?Type@DialogController@UI@@UEBAIXZ`; the
// enum's existing `kControllerVtblGetTypeId` describes it correctly, and the
// name is left as is.)
//
// ----------------------------------------------------------------------------
// NOT NEW CUTOVER SURFACE — the arithmetic that makes this a six-body lane
// ----------------------------------------------------------------------------
// SaveSettings dispatches `self->vtable[+0x48]` and the controllers' +0x10 and
// +0x50; OnClosed dispatches +0x30. **None of those is surface this port adds.**
// Those vtables are minted whether or not anything here is ported, so the
// bodies behind them are already whatever they already are. What porting
// SaveSettings would have added, and what binary_writer.cpp removed first, is
// the BinaryWriter tail — which is reached only because SaveSettings builds a
// writer. Distinguishing the two is what stopped this row from being either a
// two-body underestimate or an open-ended refusal.
//
// ----------------------------------------------------------------------------
// ORACLE: dialog_settings_probe.cpp. Read its HONEST LIMITS.
// ============================================================================

#include <cstddef>
#include <cstdint>
#include <cstring>

#include "../../funcs/rt_port.h"
#include "../../types/ui/DialogInstance.h"
#include "../../types/io/Stream.h"
#include "../../types/io/Serializer.h"

enum class Module : int { Exe = 0, Runtime = 1, VideoDX11 = 2, COUNT = 3 };
uintptr_t ResolveAddr(Module module, uintptr_t ghidraAddr);

namespace RtUi {

using RtUI::DialogInstance;

namespace {

using MsCtorFn  = void* (*)(void*);
using MsDtorFn  = void  (*)(void*);
using SerCtorFn = void* (*)(void* self, void* stream, int version);

// Virtual-call shapes used below. All are RETAIL offsets.
using VoidVFn    = void     (*)(void* self);
using StreamVFn  = void     (*)(void* self, void* stream);
using U32VFn     = uint32_t (*)(void* self);
using SerVFn     = void     (*)(void* rw, void* value, const char* name);
using DelDtorVFn = void*    (*)(void* self, unsigned int flags);

inline void* VSlot(void* obj, int off) {
    return (*reinterpret_cast<void***>(obj))[off / 8];
}

// DialogInstance's own vtable slot that receives the settings stream.
constexpr int kDlgVtblOnSerializeSettings = 0x48;
// DialogController vtable, confirmed against ??_7DialogController@UI@@6B@.
constexpr int kCtrlVtblType                = 0x10;
constexpr int kCtrlVtblOnClosed            = 0x30;
constexpr int kCtrlVtblOnSerializeSettings = 0x50;
// IO::ReaderWriter vtable (runtime/types/io/Serializer.h, rw_vtbl).
constexpr int kRwDeletingDtor = 0x00;
constexpr int kRwScalarUInt   = 0x98;
constexpr int kRwScalarInt    = 0xA0;

}  // namespace

// ---------------------------------------------------------------------------
// ?SaveSettings@DialogInstance@UI@@QEAAXXZ   0x180114240
// ---------------------------------------------------------------------------
extern "C" void RtUi_DialogInstance_SaveSettings(DialogInstance* self) {
    if (self->_userData == nullptr) return;

    RtIo::MemoryStream ms;
    RT_EXPORT_ONCE(MsCtorFn, "??0MemoryStream@IO@@QEAA@XZ")(&ms);

    const int32_t size = self->_userFlags;      // a BYTE LENGTH — see the header
    void* const   buf  = self->_userData;
    ms.data           = buf;
    ms.size           = static_cast<uint32_t>(size);
    ms.cursor         = 0;
    ms.base.direction = 1;                      // WRITING

    if (size >= 4) {
        // Stored DIRECTLY into the buffer, not through the stream, then the
        // cursor is stepped past it. LoadSettings reads this dword back.
        *reinterpret_cast<uint32_t*>(buf) = 0xdeadbeefu;
        ms.cursor += 4;
    }

    // Retail inlines the Serializer ctor; this is the same code, ported.
    rt::io::Serializer ser;
    RT_EXPORT_ONCE(SerCtorFn, "??0Serializer@IO@@QEAA@AEAVStream@1@H@Z")(&ser, &ms, 0);
    void* rw = ser.readerWriter;

    // The dialog's own OnSerializeSettings gets the STREAM, not the serializer.
    reinterpret_cast<StreamVFn>(VSlot(self, kDlgVtblOnSerializeSettings))(self, &ms);

    // SNAPSHOT: the loop bound is taken once (see the header).
    int32_t count = self->_controllers._size;
    reinterpret_cast<SerVFn>(VSlot(rw, kRwScalarInt))(rw, &count, nullptr);

    for (int32_t i = 0; i < count; ++i) {
        // RE-READ of _data, twice per iteration, exactly as retail does.
        void* c = self->_controllers._data[i];
        uint32_t id = reinterpret_cast<U32VFn>(VSlot(c, kCtrlVtblType))(c);
        reinterpret_cast<SerVFn>(VSlot(rw, kRwScalarUInt))(rw, &id, nullptr);

        // A second, per-controller record: this Serializer exists only for the
        // header its ReaderWriter writes now and the trailer it writes below.
        rt::io::Serializer ser2;
        RT_EXPORT_ONCE(SerCtorFn, "??0Serializer@IO@@QEAA@AEAVStream@1@H@Z")(&ser2, &ms, 0);
        void* rw2 = ser2.readerWriter;

        void* c2 = self->_controllers._data[i];
        reinterpret_cast<StreamVFn>(VSlot(c2, kCtrlVtblOnSerializeSettings))(c2, &ms);

        if (rw2 != nullptr)
            reinterpret_cast<DelDtorVFn>(VSlot(rw2, kRwDeletingDtor))(rw2, 1u);
    }

    if (rw != nullptr)
        reinterpret_cast<DelDtorVFn>(VSlot(rw, kRwDeletingDtor))(rw, 1u);

    // Retail inlines ~MemoryStream; the ported body is the same code, and it
    // does NOT free _data because _ownsData is 0 — the buffer is the caller's.
    RT_EXPORT_ONCE(MsDtorFn, "??1MemoryStream@IO@@UEAA@XZ")(&ms);
}

// ---------------------------------------------------------------------------
// ?OnClosed@DialogInstance@UI@@UEAAXXZ   0x180115330
// ---------------------------------------------------------------------------
extern "C" void RtUi_DialogInstance_OnClosed(DialogInstance* self) {
    RtUi_DialogInstance_SaveSettings(self);

    // NOT a snapshot: retail re-reads the count from the object every iteration
    // here, unlike SaveSettings three instructions earlier. See the header.
    for (int32_t i = 0; i < self->_controllers._size; ++i) {
        void* c = self->_controllers._data[i];
        reinterpret_cast<VoidVFn>(VSlot(c, kCtrlVtblOnClosed))(c);
    }
}

}  // namespace RtUi

// ============================================================================
// Registration
// ============================================================================
int RtUiDialogSettingsRegister() {
    using namespace RtUi;
    int n = 0;

    RegisterRuntimeExport("?SaveSettings@DialogInstance@UI@@QEAAXXZ",
                          (void*)&RtUi_DialogInstance_SaveSettings); ++n;
    RegisterRuntimeAddr(0x180114240, (void*)&RtUi_DialogInstance_SaveSettings);

    RegisterRuntimeExport("?OnClosed@DialogInstance@UI@@UEAAXXZ",
                          (void*)&RtUi_DialogInstance_OnClosed); ++n;
    RegisterRuntimeAddr(0x180115330, (void*)&RtUi_DialogInstance_OnClosed);

    return n;
}
