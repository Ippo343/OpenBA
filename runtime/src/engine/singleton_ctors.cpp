// ============================================================================
// runtime/src/engine/singleton_ctors.cpp — Wave 9 lane X
//   The fourteen constructors that PUBLISH a `?_singleton@...` DATA export.
// ============================================================================
//
// WHY THESE FOURTEEN ARE ONE FILE AND NOT FOURTEEN LANES
// -----------------------------------------------------
// They share one defect, not one namespace. Lane C2 (WAVE9_LANE_C2.md section 4)
// enumerated the exported DATA symbols the mint hands out as ZEROED storage at
// cutover and asked, per symbol, "who writes it?". Twenty-three rows answered
// "a function nobody has ported", and fourteen of those are read on the boot
// path by a body of ours that runs before or during the first frame. For every
// one of the fourteen the writer is that class's own constructor.
//
// So the disposition, fixed by lane C2 and not re-litigated here, is: PORT THE
// WRITER, WITH THE SINGLETON WRITE AS A NAMED ACCEPTANCE CRITERION.
//
// *** A COMPLETE, CORRECT, BYTE-FAITHFUL PORT OF ONE OF THESE CONSTRUCTORS IS
// *** STILL WRONG IF IT OMITS THE SINGLETON STORE.
//
// That is not hypothetical. Lane E3 hit it: `MemoryAllocator::Construct` ends
// in `if (slot) *slot = self`, the resolve returned null, the store was skipped
// WITHOUT A WORD, `MemoryAllocator::Get()` returned null for the rest of the
// process, and the first `MemAlloc` faulted on `null + 0x640` several frames
// away from anything naming `_singleton`. Every accessor below therefore goes
// through `RT_DATA_SLOT` — the PORT-AWARE door — and every port is checked by
// the probe at the bottom of this file, which asserts `*slot == our object`
// after our body runs and reports it as its own comparison.
//
// EVIDENCE
// --------
// Every field store below is transcribed from `objdump -d -M intel` of the
// SHIPPED Runtime-x64.dll at the address named in each section banner. The
// stores are written through the `W8/W16/W32/W64/WP` helpers at byte offsets
// rather than through struct members, for three reasons that are all
// load-bearing here:
//
//   1. Several of these constructors use UNALIGNED qword stores that span two
//      named fields (`MOV qword ptr [RCX+0x4],1` in Video::Configuration writes
//      `interfaceIndex = 1` AND `adapterIndex = 0` in one instruction). A
//      member-by-member transcription silently invents an order and a width.
//   2. Several leave GAPS. Input::Device's three loops write bytes +0x00..+0x07
//      and +0x10..+0x17 of each 0x18-byte record and NEVER TOUCH +0x08..+0x0F.
//      Video::Device never writes +0x1c..+0x1f or +0x98 (which is why
//      Device.h's `maxAnisotropy` comment says the ctor does not write it).
//      Writing "the struct" would zero bytes retail leaves alone.
//   3. Where a runtime/types header already pins the field, a `static_assert`
//      below ties the byte offset to the header's `offsetof`, so the two cannot
//      drift apart. Where no header exists (Object::Factory) the constructor is
//      the ONLY evidence and this file says so instead of inventing a struct.
//
// UNWRITTEN BYTES AND THE ORACLE. Because retail leaves gaps, the probe fills
// both scratch subjects with 0xCD first: a byte neither side writes stays 0xCD
// in BOTH and compares equal, so a whole-object memcmp stays valid without an
// exclusion list. That is the inverse of the Wave 6 trap
// (reference_oracle_must_not_compare_unwritten_bytes.md) — there the fill was
// one-sided, here it is symmetric, and the gaps are named above so a later
// reader does not "fix" them.
//
// CALLS= ON A NORMAL RUN IS ZERO FOR ALL FOURTEEN, BY CONSTRUCTION. Nothing in
// our tree calls any of these constructors — the callers are `Engine::Engine`
// and `Engine::Startup` INSIDE the DLL, which we do not intercept. That is why
// this lane ships its own probe: without it all fourteen would report the
// classic clean-because-dead result
// (reference_uncalled_port_is_not_verified.md). The probe is the effectiveness
// control this lane is measured by, and it is the ONLY thing that executes
// these bodies before cutover.
//
// TWO OF THE FOURTEEN ARE REGISTERED BUT NOT PROBED — see the PERSISTENCE and
// AUDIO::DEVICE banners. Both constructors call a base at 0x1800ca4e0 that
// `_beginthreadex`s a SUSPENDED worker and creates three kernel events. Running
// the shipped body against a scratch buffer would start a thread bound to a
// fake object, and the two runs would disagree on the handle values by
// construction. They are ported, they publish their singleton, and their oracle
// is named as deferred rather than left implied.
// ============================================================================

#include <cstdio>
#include <cstring>
#include <cstdint>
#include <cstddef>
#include <new>

#include "../../funcs/rt_port.h"
#include "../../types/input/Device.h"   // ENDGAME B.3.9 ENG: the class static
#include "../../types/application/Application.h"   // ENDGAME B.3.9 ENG: the class static
#include "../../types/video/Device.h"             // ENDGAME B.3.9 VID: the class statics

#include "../../types/system/MemoryAllocator.h"
#include "../../types/io/Modification.h"
#include "../../types/io/FileSystem.h"
#include "../../types/io/Persistence.h"
#include "../../types/io/PersistenceWorker.h"   // ENDGAME B.3.9 IO (W2)
#include "../../types/object/External.h"
#include "../../types/object/Internal.h"
#include "../../types/video/Device.h"
#include "../../types/input/Input.h"
#include "../../types/audio/Audio.h"
#include "../../types/audio/Device.h"

// ----------------------------------------------------------------------------
// ResolveAddr, forward-declared rather than pulled in from standalone/resolve.h
// — same reasoning rt_port.h gives for RegisterRuntimeExport: resolve.h drags
// in <windows.h> and PLAN.md section 4.1 keeps runtime/src/ platform-neutral.
// This declaration must stay byte-identical to resolve.h's.
// ----------------------------------------------------------------------------
enum class Module : int { Exe = 0, Runtime = 1, VideoDX11 = 2, COUNT = 3 };
uintptr_t ResolveAddr(Module mod, uintptr_t ghidra_addr);

// The platform half (singleton_ctors_win.cpp).
void RtLaneXPlatformLockInit(void* cs);
void RtLaneXPlatformLockDelete(void* cs);
// Installs allocator.cpp's `g_plat` function-pointer seam if it is not already
// installed, and returns whether it took. See the long comment above the
// MemoryAllocator subject in the probe: `rtalloc::Construct` opens with
// `g_plat.lockInit(...)`, and on a normal run that table is all-null because the
// only two callers of RtSystemAllocatorInstallPlatform() are opt-in.
bool RtLaneXEnsureAllocatorPlatform();

// System::MemoryAllocator's constructor body already exists: lane E ported it
// as `rtalloc::Construct` / `rtalloc::Destruct` in runtime/src/system/
// allocator.cpp, singleton store included. It was never REGISTERED, which is
// why lane C2's writer audit still ranks that row (c) "writer exists but is NOT
// ported". This lane registers it; the body stays where its owner put it, and
// this file does not duplicate it.
namespace rtalloc {
void Construct(rt::system::MemoryAllocator* self);
void Destruct(rt::system::MemoryAllocator* self);
}  // namespace rtalloc

namespace {

// ---------------------------------------------------------------------------
// Byte-offset store helpers. std::memcpy, not cast-and-assign: several of these
// stores are UNALIGNED in retail (Video::Configuration's `MOV qword ptr
// [RCX+0x4]`), and `*(uint64_t*)(p+4) = v` is undefined behaviour that GCC is
// entitled to compile into an aligned move.
// ---------------------------------------------------------------------------
inline void W8 (void* p, size_t o, uint8_t  v) { std::memcpy(static_cast<uint8_t*>(p) + o, &v, 1); }
inline void W16(void* p, size_t o, uint16_t v) { std::memcpy(static_cast<uint8_t*>(p) + o, &v, 2); }
inline void W32(void* p, size_t o, uint32_t v) { std::memcpy(static_cast<uint8_t*>(p) + o, &v, 4); }
inline void W64(void* p, size_t o, uint64_t v) { std::memcpy(static_cast<uint8_t*>(p) + o, &v, 8); }
inline void WP (void* p, size_t o, const void* v) { std::memcpy(static_cast<uint8_t*>(p) + o, &v, sizeof(void*)); }

// ---------------------------------------------------------------------------
// The fourteen singleton slots, each through RT_DATA_SLOT — the PORT-AWARE
// door. While the DLL is loaded these resolve to the DLL's own .data; at
// cutover they resolve to the storage lane C2's mint owns. THAT SWITCH IS THE
// WHOLE POINT: a body that reached the slot by `the raw DLL door`, or by
// `ResolveAddr(Module::Runtime, 0x1801fc6b0)`, would keep writing the DLL's
// copy and leave the minted row zero forever — the exact failure this lane
// exists to close.
//
// Mangled names copied from standalone/rt_data_mint_tables.inc, which reads
// them out of the shipped export directory with pefile, so none of them can be
// a phantom name (reference_resolveexport_phantom_names.md).
// ---------------------------------------------------------------------------
void** SlotMemoryAllocator() { return RT_DATA_SLOT(void**, "?_singleton@MemoryAllocator@System@@0PEAV12@EA"); }
void** SlotModification()    { return reinterpret_cast<void**>(&::IO::Modification::_singleton); }   // B.3.9 IO: class static
void** SlotFactory()         { return reinterpret_cast<void**>(&::Object::Factory::_singleton); }
void** SlotExternalFactory() { return reinterpret_cast<void**>(&::Object::ExternalFactory::_singleton); }
void** SlotInternalFactory() { return reinterpret_cast<void**>(&::Object::InternalFactory::_singleton); }
void** SlotFileSystem()      { return reinterpret_cast<void**>(&::IO::FileSystem::_singleton); }   // B.3.9 IO: class static
void** SlotPersistence()     { return reinterpret_cast<void**>(&::IO::Persistence::_singleton); }   // B.3.9 IO: class static
void** SlotVideoDevice()     { return reinterpret_cast<void**>(&::Video::Device::_singleton); }
void** SlotVideoConfig()     { return reinterpret_cast<void**>(&::Video::Configuration::_singleton); }
void** SlotApplication()     { return reinterpret_cast<void**>(&::Application::Application::_singleton); }
void** SlotInputDevice()     { return reinterpret_cast<void**>(&::Input::Device::_singleton); }
void** SlotInputConfig()     { return reinterpret_cast<void**>(&::Input::Configuration::_singleton); }
void** SlotAudioDevice()     { return reinterpret_cast<void**>(&::Audio::Device::_singleton); }
void** SlotAudioConfig()     { return reinterpret_cast<void**>(&::Audio::Configuration::_singleton); }

// The one store this whole lane is about. Written once, called fourteen times,
// so there is exactly one place to read to know how the criterion is met.
inline void Publish(void** slot, void* self) { if (slot) *slot = self; }

// ===========================================================================
// Subject sizes. Each is the end of the last byte the constructor touches. The
// probe uses them as its memcmp length: too SMALL under-tests, too LARGE
// compares bytes neither body wrote (safe here only because both scratches are
// pre-filled identically — see the banner).
// ===========================================================================
constexpr size_t kSizeMemoryAllocator = 0x670;   // MemoryAllocator.h sizeof static_assert
constexpr size_t kSizeModification    = 0x28;    // ctor's last store is qword +0x20
constexpr size_t kSizeFactory         = 0x10;    // ctor's last store is dword +0x08
constexpr size_t kSizeExternalFactory = 0x48;    // ctor's last store is byte  +0x42
constexpr size_t kSizeInternalFactory = 0x9e8;   // Internal.h sizeof static_assert
constexpr size_t kSizeFileSystem      = 0x50;    // ctor's last store is dword +0x48
constexpr size_t kSizePersistence     = 0x98;    // Persistence.h: _savePath at +0x90
constexpr size_t kSizeVideoDevice     = 0x108;   // Device.h sizeof static_assert
constexpr size_t kSizeVideoConfig     = 0x44;    // Device.h: last field +0x42, align 4
constexpr size_t kSizeApplication     = 0x30;    // ctor's last store is byte  +0x28
constexpr size_t kSizeInputDevice     = 0x1200;  // ctor's last store is dword +0x11fc
constexpr size_t kSizeInputConfig     = 0x10;    // ctor's last store is byte  +0x0a
constexpr size_t kSizeAudioDevice     = 0x1298;  // last record ends at 0xa98 + 64*0x20
constexpr size_t kSizeAudioConfig     = 0x10;    // Audio.h sizeof static_assert

// Offsets asserted against the headers that already pin them, so a header edit
// cannot silently move a field out from under a transcribed byte-offset store.
static_assert(offsetof(RtIO::Modification, mods) == 0x08,
              "Modification ctor 0x1800b04b0: MOV qword [RCX+0x8],RAX");
static_assert(offsetof(RtIO::FileSystem, existsInPackfileGate) == 0x18,
              "FileSystem ctor 0x1800ac7c0: MOV word [RCX+0x18],0x100 — low byte");
static_assert(offsetof(RtIO::FileSystem, platformExistsGate) == 0x19,
              "FileSystem ctor 0x1800ac7c0: ... high byte, so platformExistsGate starts at 1");
static_assert(offsetof(RtObject::ExternalFactory, _referenceModeEnabled) == 0x40,
              "ExternalFactory ctor 0x1800be7e0: MOV word [RCX+0x40],DX");
static_assert(offsetof(RtObject::ExternalFactory, _simpleLoadEnabled) == 0x42,
              "ExternalFactory ctor 0x1800be7e0: MOV byte [RCX+0x42],DL");
static_assert(offsetof(RtObject::InternalFactory, random) == 0x20,
              "InternalFactory ctor 0x1800c0030: the Random subobject it seeds");
static_assert(offsetof(RtVideo::Device, lock) == 0x60,
              "Video::Device ctor 0x1800f7160: ADD RCX,0x60 before InitializeCriticalSection");
static_assert(offsetof(RtVideo::Device, activeInterface) == 0xF8,
              "Video::Device ctor 0x1800f7160: MOV dword [RSI+0xf8],0xffffffff");
static_assert(offsetof(RtVideo::Configuration, refreshNumerator) == 0x38,
              "Video::Configuration ctor 0x1800f6f10: MOV dword [RCX+0x38],0x3c");
static_assert(offsetof(RtInput::Configuration, _mouseDisplay) == 0x0A,
              "Input::Configuration ctor 0x1800e0de0: MOV byte [RCX+0xa],0");
static_assert(RtInput::kKeyboardOffset == 0x6C0, "Input::Device ctor loop 2 base");
static_assert(RtInput::kMouseOffset == 0x1098, "Input::Device ctor loop 3 base");
static_assert(RtInput::kMouseClippingOffset == 0x11D0,
              "Input::Device ctor 0x1800e13b0: MOV byte [RCX+0x11d0],R8B");
static_assert(offsetof(RtAudio::Configuration, effectVolume) == 0x0C,
              "Audio::Configuration ctor 0x1800ddeb0: MOV dword [RCX+0xc],0x64");
static_assert(offsetof(RtIo::Persistence, savePath) == 0x90,
              "Persistence ctor 0x1800b8020: MOV qword [RDI+0x90],RSI");
// The two deferred subjects' sizes are recorded above but not used by the
// probe. Asserted rather than deleted, so the number a later lane needs when it
// arms them is here and is checked against the header that pins it.
static_assert(kSizePersistence >= offsetof(RtIo::Persistence, savePath) + 8,
              "Persistence subject must cover _savePath");
static_assert(kSizeAudioDevice == 0xa98 + 64 * 0x20,
              "Audio::Device subject must cover the whole of loop 2");

// ===========================================================================
// System::MemoryAllocator   ??0MemoryAllocator@System@@QEAA@XZ  0x1800c79a0
//                           ??1MemoryAllocator@System@@QEAA@XZ  0x1800c7960
// ---------------------------------------------------------------------------
// Body: lane E's `rtalloc::Construct` / `rtalloc::Destruct`. Only the MSVC
// return-`this` convention is added here (the retail ctor ends `MOV RAX,RDI`).
// Registering is the whole change, and it is what moves lane C2's row from (c)
// "writer exists but is NOT ported" to (b).
// ===========================================================================
void* Ctor_MemoryAllocator(void* self) {
    rtalloc::Construct(static_cast<rt::system::MemoryAllocator*>(self));
    return self;
}
void Dtor_MemoryAllocator(void* self) {
    // The dtor is a writer too: it stores nullptr into the same slot. Lane C2
    // lists it beside the ctor for that reason.
    rtalloc::Destruct(static_cast<rt::system::MemoryAllocator*>(self));
}

// ===========================================================================
// IO::Modification          ??0Modification@IO@@QEAA@XZ         0x1800b04b0
// ---------------------------------------------------------------------------
//   MOV word ptr [RCX],0x101          ; +0x00 = 1, +0x01 = 1  (two bools)
//   XOR EAX,EAX
//   MOV qword ptr [RCX+0x8],RAX       ; the Mod array pointer
//   MOV qword ptr [RCX+0x10],RAX      ; count (+0x10) and +0x14
//   MOV qword ptr [RCX+0x18],RAX
//   MOV qword ptr [RCX+0x20],RAX
//   MOV qword ptr [0x1801fc6b0],RCX   ; _singleton
//   MOV RAX,RCX
// Lane M found this row independently; loops.cpp:265 reads the slot from
// `RtEngine_Loader_Load`, which is registered at 0x1800dc050 and runs on the
// boot path.
// ===========================================================================
void* Ctor_Modification(void* self) {
    W16(self, 0x00, 0x0101);
    W64(self, 0x08, 0);
    W64(self, 0x10, 0);
    W64(self, 0x18, 0);
    W64(self, 0x20, 0);
    Publish(SlotModification(), self);
    return self;
}

// ===========================================================================
// Object::Factory           ??0Factory@Object@@QEAA@XZ          0x1800c09d0
// ---------------------------------------------------------------------------
//   XOR EAX,EAX
//   MOV qword ptr [RCX],RAX           ; +0x00
//   MOV dword ptr [RCX+0x8],EAX       ; +0x08   (+0x0c NOT written)
//   MOV qword ptr [0x1801fc6e8],RCX   ; _singleton
//   MOV RAX,RCX
//
// NO TYPE HEADER EXISTS FOR Object::Factory and this constructor is the only
// evidence about its layout, so none is invented here. The {qword, dword}
// shape is what a Collection::Array<T> prefix looks like, but "looks like" is
// not a pin and a struct asserting it would be a guess dressed as a layout.
// ===========================================================================
void* Ctor_Factory(void* self) {
    W64(self, 0x00, 0);
    W32(self, 0x08, 0);
    Publish(SlotFactory(), self);
    return self;
}

// ===========================================================================
// Object::ExternalFactory   ??0ExternalFactory@Object@@QEAA@XZ  0x1800be7e0
// ---------------------------------------------------------------------------
//   XOR EDX,EDX
//   MOV qword [RCX],RDX      / MOV dword [RCX+0x8],EDX     ; +0x0c NOT written
//   LEA RAX,[RCX+0x10] ; MOV qword [RAX],RDX / MOV dword [RAX+0x8],EDX
//   LEA RAX,[RCX+0x20] ; MOV qword [RAX],RDX / MOV dword [RAX+0x8],EDX
//   LEA RAX,[RCX+0x30] ; MOV qword [RAX],RDX / MOV qword [RAX+0x8],RDX
//   MOV word ptr [RCX+0x40],DX        ; _referenceModeEnabled, _loadEnabled
//   MOV byte ptr [RCX+0x42],DL        ; _simpleLoadEnabled
//   MOV qword ptr [0x1801fc6c8],RCX   ; _singleton
//
// The three `LEA` + store pairs are Collection::Array<T> default ctors inlined
// (`_data = 0; _size = 0;` with `_capacity` at +0x0c of each left alone) —
// which is why +0x0c, +0x1c and +0x2c stay unwritten. loops.cpp:257 reads this
// slot for `_loadEnabled`, the flag `Loader::Run` holds across a load.
// ===========================================================================
void* Ctor_ExternalFactory(void* self) {
    W64(self, 0x00, 0);
    W32(self, 0x08, 0);
    W64(self, 0x10, 0);
    W32(self, 0x18, 0);
    W64(self, 0x20, 0);
    W32(self, 0x28, 0);
    W64(self, 0x30, 0);
    W64(self, 0x38, 0);
    W16(self, 0x40, 0);
    W8 (self, 0x42, 0);
    Publish(SlotExternalFactory(), self);
    return self;
}

// ===========================================================================
// Object::InternalFactory   ??0InternalFactory@Object@@QEAA@XZ  0x1800c0030
// ---------------------------------------------------------------------------
//   XOR EAX,EAX
//   MOV qword [RCX],RAX ; [RCX+8],RAX ; [RCX+0x10],RAX ; [RCX+0x18],RAX
//   MOV dword [RCX+0x20],EAX                  ; Random::index = 0
//   LEA RCX,[RSP+0x48] ; CALL 0x18007fb9c     ; (uint)time(NULL)
//   LEA RDX,[RBX+0x24] ; MOV dword [RDX],EAX  ; Random::mt[0] = seed
//   ... 623 iterations of mt[i] = 0x6c078965*(mt[i-1]^(mt[i-1]>>30)) + i
//   MOV qword ptr [0x1801fc6d8],RBX           ; _singleton
//
// That tail IS `System::Random::Random()` (0x1800c7f20) inlined — index zeroed
// and the standard init_genrand recurrence over 624 words at +0x24, exactly
// ::Random's layout (Internal.h pins `random` at +0x20). Wave 2 already ported
// that constructor, so this body CALLS it through the port-aware door rather
// than transcribing the recurrence a second time; two copies of a PRNG seed
// that must agree is how a silent desync gets written.
//
// TIME-SEEDED, SO NOT BYTE-DIFFED. Two calls made microseconds apart can
// legitimately land in different seconds, and 0x6c078965 amplifies a
// one-count seed difference across all 624 words. The probe therefore compares
// only [0x00, 0x24) — the deterministic prefix and the Random index — and
// verifies the 623-word recurrence against our OWN mt[0]. Same rationale
// value_types.cpp already records for ??0Random@System@@QEAA@XZ itself.
// ===========================================================================
void* Ctor_InternalFactory(void* self) {
    W64(self, 0x00, 0);
    W64(self, 0x08, 0);
    W64(self, 0x10, 0);
    W64(self, 0x18, 0);
    using RandomCtorFn = void* (*)(void*);
    RT_EXPORT_ONCE(RandomCtorFn, "??0Random@System@@QEAA@XZ")(
        static_cast<uint8_t*>(self) + offsetof(RtObject::InternalFactory, random));
    Publish(SlotInternalFactory(), self);
    return self;
}

// ===========================================================================
// IO::FileSystem            ??0FileSystem@IO@@QEAA@XZ           0x1800ac7c0
// ---------------------------------------------------------------------------
//   XOR EDX,EDX
//   MOV qword [RCX],RDX                       ; sourceDirectory
//   LEA RAX,[RCX+0x8]  ; MOV qword [RAX],RDX  ; dataDirectory
//   LEA RAX,[RCX+0x10] ; MOV qword [RAX],RDX
//   MOV word ptr [RCX+0x18],0x100             ; existsInPackfileGate = 0,
//                                             ; platformExistsGate   = 1
//   MOV qword [RCX+0x20],RDX ; [RCX+0x28],RDX ; [RCX+0x30],RDX ; [RCX+0x38],RDX
//   LEA RAX,[RCX+0x40] ; MOV qword [RAX],RDX ; MOV dword [RAX+0x8],EDX
//   MOV qword ptr [0x1801fc6a8],RCX           ; _singleton
//
// The 0x100 word is the one non-zero initial value in the object and it is the
// reason a "memset the struct" port would be wrong: `platformExistsGate` starts
// ENABLED, and FileSystem::Exists reads it every lookup.
// ===========================================================================
void* Ctor_FileSystem(void* self) {
    W64(self, 0x00, 0);
    W64(self, 0x08, 0);
    W64(self, 0x10, 0);
    W16(self, 0x18, 0x0100);
    W64(self, 0x20, 0);
    W64(self, 0x28, 0);
    W64(self, 0x30, 0);
    W64(self, 0x38, 0);
    W64(self, 0x40, 0);
    W32(self, 0x48, 0);
    Publish(SlotFileSystem(), self);
    return self;
}

// ===========================================================================
// IO::Persistence           ??0Persistence@IO@@QEAA@XZ          0x1800b8020
// ---------------------------------------------------------------------------
//   MOV qword [RCX],RSI                       ; settingsMapRoot = 0
//   MOV dword [RCX+0x8],ESI
//   LEA RBX,[RCX+0x10] ; MOV EDX,0x10000 ; CALL 0x1800ca4e0
//                                             ; == ??0Thread@System@@QEAA@I@Z
//   LEA RAX,[0x180155860] ; MOV qword [RBX],RAX   ; override the base vtable
//   MOV qword [RBX+0x30],RSI                  ; = self+0x40
//   MOV qword [RBX+0x38],1                    ; = self+0x48
//   MOV dword [RBX+0x40],ESI                  ; = self+0x50
//   MOV qword [RBX+0x50..0x78],RSI            ; = self+0x60..+0x88
//   MOV qword [RDI+0x90],RSI                  ; _savePath
//   MOV qword ptr [0x1801fc6b8],RDI           ; _singleton
//
// THE BASE IS System::Thread AND IT IS ALREADY PORTED. 0x1800ca4e0 is
// `??0Thread@System@@QEAA@I@Z` (lane B3/P, runtime/src/system/thread_win.cpp),
// so IO::Persistence embeds a worker thread at +0x10 and the 0x10000 is its
// stack size. This body therefore reaches it BY NAME through RT_EXPORT_ONCE —
// the port-aware door — and not by address: tools/port_bypass_audit.py caught
// exactly that mistake in the first draft of this file, which is the same class
// of defect Wave 9 lane R found in graphics/scene.cpp (a port walking past a
// port). Reaching it by name also means the thread the object gets is OURS.
//
// *** ORACLE DEFERRED, AND HERE IS THE BLOCKER. *** That base constructor is
// not inert: it `_beginthreadex`s a SUSPENDED worker (thread proc 0x1800ca440,
// arg = the object), creates three events, and RESUMES it. So
//   (a) a scratch-buffer probe would start a real thread bound to a fake
//       object — twice, once for the shipped body and once for ours, and
//   (b) the four handle values differ between the two runs BY CONSTRUCTION, so
//       a byte compare of the +0x10..+0x40 region reports a divergence that
//       says nothing about the port.
// The port is registered anyway: it has no caller in our tree today (so
// registering is inert now) and at cutover it is the only thing that writes
// `?_singleton@Persistence@IO@@`, which input.cpp:591/619, audio.cpp:640/683
// and device.cpp:135 all read at startup.
//
// *** ONE RESIDUAL CUTOVER DEPENDENCY, NAMED RATHER THAN HIDDEN. *** The vtable
// at 0x180155860 (Persistence's own, overriding Thread's) is NOT an export —
// pefile finds no name for it — and it is NOT in lane C2's mint table. So it is
// reached by ADDRESS and at cutover `ResolveAddr(Module::Runtime, ...)` will
// hand back null and this field will be a null vptr. That is a row for lane C2
// (mint the vtable) or for whoever ports IO::Persistence's virtuals; it is not
// something this body can fix, and a silent `if (v)` here would only hide it.
// ===========================================================================
void* Ctor_Persistence(void* self) {
    W64(self, 0x00, 0);
    W32(self, 0x08, 0);
    // ENDGAME B.3.9 IO (W2, foreign site): the embedded worker is the REAL
    // class -- ??0Thread(0x10000), its vptr, the +0x40..+0x88 stores
    // (runtime/types/io/PersistenceWorker.h), the same steps in the same order.
    ::new (static_cast<uint8_t*>(self) + 0x10) IO::PersistenceWorker();
    W64(self, 0x90, 0);
    Publish(SlotPersistence(), self);
    return self;
}

// ===========================================================================
// Video::Device   ??0Device@Video@@QEAA@W4DeviceType@01@@Z      0x1800f7160
// ---------------------------------------------------------------------------
//   MOV dword [RCX],EDX                       ; type = the ctor argument
//   XOR EBP,EBP
//   MOV qword [RCX+0x4],RBP                   ; UNALIGNED: +0x04..+0x0b
//   MOV qword [RCX+0xc],RBP                   ; UNALIGNED: +0x0c..+0x13
//   MOV qword [RCX+0x14],RBP                  ; UNALIGNED: +0x14..+0x1b
//                                             ; +0x1c..+0x1f NEVER WRITTEN
//   MOV qword [RCX+0x20 .. +0x58],RBP         ; eight aligned qwords
//   ADD RCX,0x60 ; CALL [InitializeCriticalSection]
//   MOV EDX,0x1388 ; LEA RCX,[RSI+0x60] ; CALL [SetCriticalSectionSpinCount]
//   MOV qword [RSI+0xb0 .. +0xf0],RBP         ; the three plugin slots
//   MOV qword [RSI+0x90],RBP                  ; windowHandle
//   MOV word  [RSI+0x9c],BP                   ; settingsChanged, field9D
//   MOV dword [RSI+0xf8],0xffffffff           ; activeInterface = -1
//   MOV qword [RSI+0x100],RBP                 ; deviceData
//   MOV qword [RSI+0xa0],RBP ; [RSI+0xa8],RBP
//   MOV RAX,[0x1801fe7a8] ; TEST EDI,EDI ; CMOVE RAX,RSI ; MOV [0x1801fe7a8],RAX
//
// *** THE SINGLETON STORE IS CONDITIONAL AND THE CONDITION IS THE ARGUMENT. ***
// The CMOVE keeps the OLD value unless `type == 0`, so only a DeviceType-0
// device becomes `?_singleton@Device@Video@@`. Retail still executes an
// unconditional STORE of the cmov result, which is why this port writes the
// slot back rather than skipping the write — those differ if another thread
// wrote the slot in between, and faithful means faithful.
//
// +0x98 (`maxAnisotropy`) is NOT written by the constructor — Device.h already
// says so, and adapter enumeration fills it. +0x1c..+0x1f likewise.
// ===========================================================================
void* Ctor_VideoDevice(void* self, int32_t type) {
    W32(self, 0x00, static_cast<uint32_t>(type));
    W64(self, 0x04, 0);
    W64(self, 0x0c, 0);
    W64(self, 0x14, 0);
    for (size_t o = 0x20; o <= 0x58; o += 8) W64(self, o, 0);
    RtLaneXPlatformLockInit(static_cast<uint8_t*>(self) + offsetof(RtVideo::Device, lock));
    for (size_t o = 0xb0; o <= 0xf0; o += 8) W64(self, o, 0);
    W64(self, 0x90, 0);
    W16(self, 0x9c, 0);
    W32(self, 0xf8, 0xffffffffu);
    W64(self, 0x100, 0);
    W64(self, 0xa0, 0);
    W64(self, 0xa8, 0);

    void** slot = SlotVideoDevice();
    if (slot) {
        void* keep = *slot;
        *slot = (type == 0) ? self : keep;   // the CMOVE, then the store
    }
    return self;
}

// ===========================================================================
// Video::Configuration      ??0Configuration@Video@@QEAA@XZ     0x1800f6f10
// ---------------------------------------------------------------------------
//   MOV byte  [RCX],1                 ; settingsValid       (+0x01..+0x03 gap)
//   MOV qword [RCX+0x4],1             ; interfaceIndex=1, adapterIndex=0
//   MOV qword [RCX+0xc],-1            ; width=-1, height=-1
//   MOV qword [RCX+0x14],-1           ; windowX=-1, windowY=-1
//   MOV qword [RCX+0x1c],-1           ; windowWidth=-1, windowHeight=-1
//   MOV qword [RCX+0x24],1            ; multisample=1, textureFilter=0
//   MOV dword [RCX+0x2c],1            ; shadowScalar
//   MOV dword [RCX+0x30],2            ; shaderDetail
//   MOV dword [RCX+0x34],1            ; reflectionDetail
//   MOV dword [RCX+0x38],0x3c         ; refreshNumerator = 60
//   MOV dword [RCX+0x3c],1            ; refreshDenominator
//   MOV word  [RCX+0x40],0x101        ; fullscreen = 1, vsync = 1
//   MOV byte  [RCX+0x42],0            ; field42
//   MOV qword ptr [0x1801fe7a0],RCX   ; _singleton
//
// Six of the thirteen stores are UNALIGNED qwords spanning two named fields.
// Transcribing them as pairs of dword writes would produce the same bytes here,
// but it would lose the fact — and this is the class where a "cleanup" to
// member assignment is most tempting, because every field has a name.
// ===========================================================================
void* Ctor_VideoConfiguration(void* self) {
    W8 (self, 0x00, 1);
    W64(self, 0x04, 1);
    W64(self, 0x0c, 0xffffffffffffffffull);
    W64(self, 0x14, 0xffffffffffffffffull);
    W64(self, 0x1c, 0xffffffffffffffffull);
    W64(self, 0x24, 1);
    W32(self, 0x2c, 1);
    W32(self, 0x30, 2);
    W32(self, 0x34, 1);
    W32(self, 0x38, 0x3c);
    W32(self, 0x3c, 1);
    W16(self, 0x40, 0x0101);
    W8 (self, 0x42, 0);
    Publish(SlotVideoConfig(), self);
    return self;
}

// ===========================================================================
// Application::Application  ??0Application@0@QEAA@PEAX@Z        0x18009f590
// ---------------------------------------------------------------------------
//   LEA RAX,[RCX+0x18] ; XOR R8D,R8D
//   MOV qword [RAX],R8                ; +0x18
//   MOV dword [RAX+0x8],R8D           ; +0x20
//   MOV byte  [RCX+0x28],R8B          ; _quitRequested
//   MOV qword ptr [0x1801fc678],RCX   ; _singleton  <-- BEFORE the next three
//   MOV qword [RCX],R8                ; +0x00
//   MOV qword [RCX+0x8],RDX           ; +0x08 = the ctor's argument
//   MOV word  [RCX+0x10],0x100        ; _cursorEnabled = 0, _running = 1
//
// THE SINGLETON STORE IS NOT LAST. It sits between the +0x28 store and the
// +0x00 store, and the order is preserved here rather than tidied to the end,
// because a reader who assumes "publish last" would draw the wrong conclusion
// about what a concurrent reader can observe.
// ===========================================================================
void* Ctor_Application(void* self, void* hinstance) {
    W64(self, 0x18, 0);
    W32(self, 0x20, 0);
    W8 (self, 0x28, 0);
    Publish(SlotApplication(), self);
    W64(self, 0x00, 0);
    WP (self, 0x08, hinstance);
    W16(self, 0x10, 0x0100);
    return self;
}

// ===========================================================================
// Input::Device             ??0Device@Input@@QEAA@XZ            0x1800e13b0
// ---------------------------------------------------------------------------
// Three identical loops, one per input block, each writing TWO qwords of every
// 0x18-byte `State` record and skipping the middle one:
//
//   MOV EDX,0x47 ; LEA RAX,[RCX+0x6] ; XOR R8D,R8D
//   loop: DEC EDX ; MOV [RAX-0x6],R8 ; MOV [RAX+0xa],R8 ; LEA RAX,[RAX+0x18]
//         JNS loop
//
// `MOV EDX,0x47` with `DEC`/`JNS` runs 72 times (0x47 down through 0, then -1
// ends it) — 72 * 0x18 == 0x6C0, exactly Input.h's kKeyboardOffset, which is
// the independent check that 72 and not 71 is right. The other two loops are
// 0x68 -> 105 records from 0x6C0 and 0x0c -> 13 records from 0x1098, and both
// land exactly on the next block boundary.
//
// *** BYTES +0x08..+0x0F OF EVERY RECORD ARE NEVER WRITTEN. *** The ctor writes
// [rec+0x00..0x07] and [rec+0x10..0x17] and nothing else, across all 190
// records. That is not an optimisation this port may "complete": ResetState
// (0x1800e0ef0) zeroes the full record, the constructor does not, and a port
// that zeroed them would differ from retail on 1520 bytes.
//
//   MOV qword ptr [0x1801fe770],RCX   ; _singleton
//   MOV byte  [RCX+0x11d0],R8B        ; _mouseClipping   (+0x11d1..+0x11d7 gap)
//   MOV qword [RCX+0x11d8],R8 ; [RCX+0x11e0],R8 ; [RCX+0x11e8],R8
//   MOV byte  [RCX+0x11f0],1
//   MOV qword [RCX+0x11f4],R8         ; UNALIGNED
//   MOV dword [RCX+0x11fc],R8D
// ===========================================================================
void* Ctor_InputDevice(void* self) {
    for (int i = 0; i < 72; ++i) {                       // 4 joystick blocks
        const size_t base = static_cast<size_t>(i) * 0x18;
        W64(self, base + 0x00, 0);
        W64(self, base + 0x10, 0);
    }
    for (int i = 0; i < 105; ++i) {                      // keyboard
        const size_t base = RtInput::kKeyboardOffset + static_cast<size_t>(i) * 0x18;
        W64(self, base + 0x00, 0);
        W64(self, base + 0x10, 0);
    }
    for (int i = 0; i < 13; ++i) {                       // mouse
        const size_t base = RtInput::kMouseOffset + static_cast<size_t>(i) * 0x18;
        W64(self, base + 0x00, 0);
        W64(self, base + 0x10, 0);
    }
    Publish(SlotInputDevice(), self);
    W8 (self, 0x11d0, 0);
    W64(self, 0x11d8, 0);
    W64(self, 0x11e0, 0);
    W64(self, 0x11e8, 0);
    W8 (self, 0x11f0, 1);
    W64(self, 0x11f4, 0);
    W32(self, 0x11fc, 0);
    return self;
}

// ===========================================================================
// Input::Configuration      ??0Configuration@Input@@QEAA@XZ     0x1800e0de0
// ---------------------------------------------------------------------------
//   MOV qword [RCX],0                 ; keyMap
//   MOV word  [RCX+0x8],0             ; mouseClipping, legacy
//   MOV byte  [RCX+0xa],0             ; mouseDisplay
//   MOV qword ptr [0x1801fe768],RCX   ; _singleton
// ===========================================================================
void* Ctor_InputConfiguration(void* self) {
    W64(self, 0x00, 0);
    W16(self, 0x08, 0);
    W8 (self, 0x0a, 0);
    Publish(SlotInputConfig(), self);
    return self;
}

// ===========================================================================
// Audio::Configuration      ??0Configuration@Audio@@QEAA@XZ     0x1800ddeb0
// ---------------------------------------------------------------------------
//   MOV dword [RCX],0x64              ; uiVolume      = 100
//   MOV dword [RCX+0x4],0x32          ; musicVolume   = 50
//   MOV dword [RCX+0x8],0x64          ; ambientVolume = 100
//   MOV dword [RCX+0xc],0x64          ; effectVolume  = 100
//   MOV qword ptr [0x1801fe758],RCX   ; _singleton
//
// The four literals ARE the shipped defaults — music at half, everything else
// full. A zeroed row at cutover would be a silent MUTE, not a crash, which is
// the kind of tier-1 row that would have survived a first cutover boot.
// ===========================================================================
void* Ctor_AudioConfiguration(void* self) {
    W32(self, 0x00, 0x64);
    W32(self, 0x04, 0x32);
    W32(self, 0x08, 0x64);
    W32(self, 0x0c, 0x64);
    Publish(SlotAudioConfig(), self);
    return self;
}

// ===========================================================================
// Audio::Device             ??0Device@Audio@@QEAA@XZ            0x1800de3a0
// ---------------------------------------------------------------------------
//   MOV EDI,0x3f ; MOV ECX,EDI ; LEA RAX,[RBX+0x10] ; XOR ESI,ESI
//   loop1: MOV [RAX-0x10],ESI ; MOV [RAX-0x8],RSI ; MOV [RAX],RSI
//          MOV [RAX+0x8],ESI  ; MOV [RAX+0x10],RSI
//          LEA RAX,[RAX+0x28] ; DEC ECX ; JNS loop1
//        -> 64 records of stride 0x28 covering +0x000..+0x9ff; each record
//           writes +0x00 (dword), +0x08, +0x10, +0x18 (dword), +0x20 and
//           leaves +0x04..+0x07 and +0x1c..+0x1f alone.
//   LEA RCX,[RBX+0xa00] ; CALL 0x1800a0e30      ; ??0Stream@Audio@@QEAA@XZ
//   LEA RAX,[RBX+0xa98] ; LEA RCX,[0x180157c60]
//   loop2: MOV [RAX],RCX ; MOV [RAX+0x8],RSI ; MOV [RAX+0x18],RSI
//          LEA RAX,[RAX+0x20] ; DEC EDI ; JNS loop2
//        -> 64 records of stride 0x20 from +0xa98 to +0x1298, each stamped with
//           the same vtable and leaving +0x10..+0x17 for loop3.
//   MOV qword ptr [0x1801fe760],RBX             ; _singleton
//   MOV [RBX+0xa88],RSI ; MOV [RBX+0xa90],RSI
//   LEA RAX,[RBX+0xaa8] ; MOV RCX,RBX ; MOV EDX,0x40
//   loop3: MOV [RAX],RCX ; ADD RCX,0x28 ; LEA RAX,[RAX+0x20] ; DEC RDX ; JNE
//        -> each 0x20 record's +0x10 field points at the matching 0x28 record
//           from loop1, i.e. the two arrays are cross-linked 1:1.
//
// *** ORACLE DEFERRED — SAME BLOCKER AS PERSISTENCE. *** 0x1800a0e30 is
// `??0Stream@Audio@@QEAA@XZ`, a System::Thread SUBCLASS: it calls the same
// `??0Thread@System@@QEAA@I@Z` base that `_beginthreadex`s a suspended worker
// and creates three events, then stamps `??_7Stream@Audio@@6B@` (0x180154950)
// and initialises a second CRITICAL_SECTION. Running the shipped body twice
// against scratch buffers would start two threads on fake objects and the
// handle values would differ by construction. Registered anyway for the same
// reason as Persistence: no caller today, and it is the only writer of
// `?_singleton@Device@Audio@@`, which audio.cpp:880 reads.
//
// Audio::Stream is reached BY NAME, not by address: it is exported, and asking
// by name is what made this call site follow the port. It IS ported now
// (runtime/src/audio/audio_stream_win.cpp registers `??0Stream@Audio@@QEAA@XZ`;
// Audio::Stream is a real System::Thread subclass since ENDGAME B.3.7 block B4),
// and the ctor stays a door (B4b-2: no real ctor on a polymorphic class).
//
// *** THIS RESIDUAL CUTOVER DEPENDENCY IS DISCHARGED. *** The paragraph that
// stood here said: "The per-record vtable at 0x180157c60 is not an export and is
// not in lane C2's mint table, so it is reached by address and will be null at
// cutover." The second clause is NO LONGER TRUE and has not been for some time.
// Re-derived by Wave 24 lane SMALLBODY, and independently by lane CUTOVER:
//
//     grep -n '180157c60' standalone/rt_data_mint_tables.inc
//     608:  // (unexported, carried by address)  DLL 0x180157c60  7 slots
//     1542: { nullptr, 0x180157c60ull, 7, kVSlots2, kVTable2, kVTraps2, ... }
//
// It IS in the mint table, carried as an unexported address-keyed row with 7
// slots, which is exactly the shape an unexported per-record vtable needs. The
// `ResolveAddr(Module::Runtime, 0x180157c60)` door above therefore resolves to
// the minted row at cutover rather than to null.
//
// This correction is itself the Wave 23 §6 defect class — a discharge does not
// propagate, and nothing in this project detects a stale blocker. The comment
// was true when written and has been read as current ever since.
//
// NOT discharged, and deliberately still named: Persistence's 0x180155860,
// which this paragraph used to bracket with 0x180157c60. It was NOT re-checked
// by this lane and nothing here should be read as a verdict on it.
//
// The per-record vtable 0x180157c60 is `Audio::Device::PlatformChannel`'s (the
// IXAudio2VoiceCallback record, `GetPlatformChannel` 0x18000c720 =
// this+0xa98+i*0x20 -- named by block B4; B37_RULES 10).
//
// Audio::Device HAS a type header now (runtime/types/audio/Device.h, ENDGAME
// B.3.7 block B4): the two arrays are `Device::AudioChannel _channels[64]` at
// +0x000 and `Device::PlatformChannel _platformChannels[64]` at +0xa98, with
// the Stream by value at +0xa00. This constructor is still the DOOR (B7-12: no
// real Device ctor this step), so it keeps writing the offsets it transcribed.
// ===========================================================================
// ENDGAME B.3.9 ENG: loop 2's vptr + two zero stores are
// `Audio::Device::PlatformChannel`'s real constructor now -- `::new` into each
// record -- so every record wears the class's (COM-shaped) table, and the
// by-address read of the minted unnamed row (a function-local lazy cache, B9-10)
// is gone with that row.
void* Ctor_AudioDevice(void* self) {
    using StreamCtorFn = void* (*)(void*);
    Audio::Device* const dev = static_cast<Audio::Device*>(self);

    for (int i = 0; i < 64; ++i) {
        const size_t base = static_cast<size_t>(i) * 0x28;
        W32(self, base + 0x00, 0);
        W64(self, base + 0x08, 0);
        W64(self, base + 0x10, 0);
        W32(self, base + 0x18, 0);
        W64(self, base + 0x20, 0);
    }
    RT_EXPORT_ONCE(StreamCtorFn, "??0Stream@Audio@@QEAA@XZ")(
        static_cast<uint8_t*>(self) + 0xa00);
    for (int i = 0; i < 64; ++i)
        ::new (static_cast<void*>(&dev->_platformChannels[i])) Audio::Device::PlatformChannel();
    Publish(SlotAudioDevice(), self);
    W64(self, 0xa88, 0);
    W64(self, 0xa90, 0);
    for (int i = 0; i < 64; ++i) {
        WP(self, 0xaa8 + static_cast<size_t>(i) * 0x20,
           static_cast<uint8_t*>(self) + static_cast<size_t>(i) * 0x28);
    }
    return self;
}

// ===========================================================================
// THE PROBE — this lane's only executor, and its effectiveness control
// ===========================================================================
// Nothing in our tree calls any of these constructors, so `calls=` would be 0
// for all fourteen and every one would report clean-because-dead. The probe
// runs each ported body ONCE against the shipped one and reports:
//
//   * a byte comparison of the whole subject (both scratches pre-filled 0xCD,
//     so retail's unwritten gaps compare equal on both sides), and
//   * a SHADOW comparison whose verdict is `*slot == our object` — the named
//     acceptance criterion of this entire lane, checked per class rather than
//     assumed from the presence of a `Publish()` call in the source.
//
// It runs only when the diff harness is on (`if (!the diff oracle()) return;`),
// which is both the correct semantics — it IS an oracle — and what keeps its
// `the raw DLL door` calls off a normal run's path for tools/rawdoor_audit.py.
//
// THE SINGLETON SLOTS ARE SAVED AND RESTORED. Both constructors publish, so
// without the restore the probe would leave every live `_singleton` pointing at
// a static scratch buffer — it would BREAK the run it is measuring.
//
// CRITICAL_SECTIONs ARE DELETED ON BOTH COPIES. MemoryAllocator's and
// Video::Device's constructors each initialise one into the scratch bytes, and
// the next subject overwrites those bytes; both are released explicitly rather
// than leaked. They are also EXCLUDED from the byte comparison: an initialised
// CRITICAL_SECTION carries a DebugInfo pointer the kernel allocates per call,
// so the two copies legitimately differ there and comparing them would report a
// permanent divergence that says nothing about the port.
// ===========================================================================

alignas(16) uint8_t g_ours[0x1300];
alignas(16) uint8_t g_theirs[0x1300];

int g_probed        = 0;   // subjects the probe actually executed
int g_singletonOk   = 0;   // subjects where OUR body published the slot
int g_controlRun    = 0;   // negative control: corruptions injected
int g_controlCaught = 0;   // negative control: corruptions the comparison saw

// Negative control. A probe that cannot fail proves nothing, and a memcmp over
// a 0xCD-filled pair is exactly the shape that silently passes when a size or a
// pointer is wrong. After each subject the control flips one byte of our copy
// and asserts the SAME comparison the probe used reports a difference.
void ControlMustDetect(size_t n) {
    if (n == 0) return;
    ++g_controlRun;
    const uint8_t save = g_ours[n - 1];
    g_ours[n - 1] = static_cast<uint8_t>(save ^ 0xFFu);
    if (std::memcmp(g_ours, g_theirs, n) != 0) ++g_controlCaught;
    g_ours[n - 1] = save;
}

// ---------------------------------------------------------------------------
// THE SINGLETON WINDOW MUST BE ZERO INSTRUCTIONS WIDE.
//
// Both constructors publish, so between a ctor returning and our restore the
// process-wide `?_singleton@X@` points at a 0xCD scratch buffer. For twelve of
// the thirteen subjects that is merely wrong; for MemoryAllocator it means THE
// PROCESS ALLOCATOR IS A SCRATCH BUFFER, and any MemAlloc/MemFree in that
// window -- inside the diff oracle, inside a printf that touches the engine
// heap, inside a lazy resolve -- lands on an allocator with `ranges == nullptr`
// and frees a real pointer into a fake one.
//
// The first draft of this file held that window open across BOTH ctors and all
// of the reporting. It never fired (the run died earlier, on the seam below),
// which is exactly why it is closed now rather than after it does: read `*slot`
// on the instruction after the ctor returns, restore on the one after that, and
// report from the captured bool.
// ---------------------------------------------------------------------------
#define LANEX_PROBE(LABEL, SIZE, SLOTFN, CTORNAME, OURFN)                      \
    do {                                                                       \
                                                  \
        using CtorFn = void* (*)(void*);                                       \
        void** slot__ = SLOTFN();                                              \
        void*  saved__ = slot__ ? *slot__ : nullptr;                           \
        std::memset(g_ours,   0xCD, (SIZE));                                   \
        std::memset(g_theirs, 0xCD, (SIZE));                                   \
                                                                      \
        OURFN(g_ours);                                                         \
        const bool published__ = (slot__ != nullptr &&                         \
                                  *slot__ == static_cast<void*>(g_ours));      \
        if (slot__) *slot__ = saved__;                                         \
        if (published__) ++g_singletonOk;                                      \
                      \
                               \
                                                    \
        ControlMustDetect((SIZE));                                             \
        ++g_probed;                                                            \
    } while (0)

}  // namespace

// ---------------------------------------------------------------------------
// Registration.
//
// Registering redirects OUR call sites and nothing else, and our tree has NO
// call site for any of these fourteen names — the callers are Engine::Engine
// and Engine::Startup inside the DLL. So every registration here is inert on a
// live run and load-bearing only at cutover, which is precisely the property
// that makes it safe to land the two unprobed ones.
//
// ⚠ CORRECTED 2026-09-11 (Wave 22 lane ALLOC). **The paragraph above is no
// longer true of `??0MemoryAllocator@System@@`, and it was the load-bearing
// argument for this whole block.** We have since ported `Engine::Engine`, and
// `runtime/src/engine/engine_lifecycle.cpp:390` is
//
//     RT_EXPORT_ONCE(CtorFn, "??0MemoryAllocator@System@@QEAA@XZ")(&self->_allocator);
//
// — a call site in OUR tree, reaching `Ctor_MemoryAllocator` -> `rtalloc::
// Construct`, on EVERY default run, because the wave kill switch is a kill switch
// (default ON). That registration is therefore NOT inert on a live run and is
// not merely load-bearing at cutover.
//
// It is still SAFE, but for a different reason than the one stated above, and
// the difference matters to anyone reasoning about the allocator gate:
// `RtEngineLifecycleRegister()` (engine_lifecycle.cpp:1074) refuses to register
// ANYTHING unless `RtLaneXEnsureAllocatorPlatform()` has filled `g_plat` first,
// precisely so this path cannot be an EXEC-of-0 on a null `lockInit`. The body
// is byte-faithful and publishes into the DLL's own `_singleton` slot, exactly
// as retail's ctor does.
//
// The consequence worth writing down: "the allocator port is off unless
// RT_ALLOC is set" is a claim about the REGISTRATION DOOR for the five
// MemoryAllocator statics, not about every line of `rtalloc::`. Two of its
// functions run unarmed on every boot. Corrected in place rather than deleted —
// the conclusion (safe to land) still holds; its justification did not.
// Evidence and the full crossing list: docs/runtime_port/WAVE22_LANE_ALLOC.md §2.2.
// ---------------------------------------------------------------------------
void RtSingletonCtorsPortInit() {
    int n = 0;
    RegisterRuntimeExport("??0MemoryAllocator@System@@QEAA@XZ",       (void*)&Ctor_MemoryAllocator);     ++n;
    RegisterRuntimeExport("??1MemoryAllocator@System@@QEAA@XZ",       (void*)&Dtor_MemoryAllocator);     ++n;
    RegisterRuntimeExport("??0Modification@IO@@QEAA@XZ",              (void*)&Ctor_Modification);        ++n;
    RegisterRuntimeExport("??0Factory@Object@@QEAA@XZ",               (void*)&Ctor_Factory);             ++n;
    RegisterRuntimeExport("??0ExternalFactory@Object@@QEAA@XZ",       (void*)&Ctor_ExternalFactory);     ++n;
    RegisterRuntimeExport("??0InternalFactory@Object@@QEAA@XZ",       (void*)&Ctor_InternalFactory);     ++n;
    RegisterRuntimeExport("??0FileSystem@IO@@QEAA@XZ",                (void*)&Ctor_FileSystem);          ++n;
    RegisterRuntimeExport("??0Persistence@IO@@QEAA@XZ",               (void*)&Ctor_Persistence);         ++n;
    RegisterRuntimeExport("??0Device@Video@@QEAA@W4DeviceType@01@@Z", (void*)&Ctor_VideoDevice);         ++n;
    RegisterRuntimeExport("??0Configuration@Video@@QEAA@XZ",          (void*)&Ctor_VideoConfiguration);  ++n;
    RegisterRuntimeExport("??0Application@0@QEAA@PEAX@Z",             (void*)&Ctor_Application);         ++n;
    RegisterRuntimeExport("??0Device@Input@@QEAA@XZ",                 (void*)&Ctor_InputDevice);         ++n;
    RegisterRuntimeExport("??0Configuration@Input@@QEAA@XZ",          (void*)&Ctor_InputConfiguration);  ++n;
    RegisterRuntimeExport("??0Device@Audio@@QEAA@XZ",                 (void*)&Ctor_AudioDevice);         ++n;
    RegisterRuntimeExport("??0Configuration@Audio@@QEAA@XZ",          (void*)&Ctor_AudioConfiguration);  ++n;
    printf("[RtPort] Wave9 lane X (singleton ctors): %d exports registered\n", n);

    // THE PROBE IS NOT CALLED FROM HERE. It runs from RuntimePortInit() after
    // RtDataMintInit(); the split is deliberate and the reason is on the probe.
}

#undef LANEX_PROBE
