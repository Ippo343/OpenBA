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

#include "../../funcs/rt_port.h"

#include "../../types/system/MemoryAllocator.h"
#include "../../types/io/Modification.h"
#include "../../types/io/FileSystem.h"
#include "../../types/io/Persistence.h"
#include "../../types/object/External.h"
#include "../../types/object/Internal.h"
#include "../../types/video/Device.h"
#include "../../types/input/Input.h"
#include "../../types/audio/Audio.h"

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
// WHOLE POINT: a body that reached the slot by `ResolveExportRaw`, or by
// `ResolveAddr(Module::Runtime, 0x1801fc6b0)`, would keep writing the DLL's
// copy and leave the minted row zero forever — the exact failure this lane
// exists to close.
//
// Mangled names copied from standalone/rt_data_mint_tables.inc, which reads
// them out of the shipped export directory with pefile, so none of them can be
// a phantom name (reference_resolveexport_phantom_names.md).
// ---------------------------------------------------------------------------
void** SlotMemoryAllocator() { return RT_DATA_SLOT(void**, "?_singleton@MemoryAllocator@System@@0PEAV12@EA"); }
void** SlotModification()    { return RT_DATA_SLOT(void**, "?_singleton@Modification@IO@@2PEAV12@EA"); }
void** SlotFactory()         { return RT_DATA_SLOT(void**, "?_singleton@Factory@Object@@0PEAV12@EA"); }
void** SlotExternalFactory() { return RT_DATA_SLOT(void**, "?_singleton@ExternalFactory@Object@@0PEAV12@EA"); }
void** SlotInternalFactory() { return RT_DATA_SLOT(void**, "?_singleton@InternalFactory@Object@@0PEAV12@EA"); }
void** SlotFileSystem()      { return RT_DATA_SLOT(void**, "?_singleton@FileSystem@IO@@0PEAV12@EA"); }
void** SlotPersistence()     { return RT_DATA_SLOT(void**, "?_singleton@Persistence@IO@@0PEAV12@EA"); }
void** SlotVideoDevice()     { return RT_DATA_SLOT(void**, "?_singleton@Device@Video@@0PEAV12@EA"); }
void** SlotVideoConfig()     { return RT_DATA_SLOT(void**, "?_singleton@Configuration@Video@@0PEAV12@EA"); }
void** SlotApplication()     { return RT_DATA_SLOT(void**, "?_singleton@Application@1@0PEAV11@EA"); }
void** SlotInputDevice()     { return RT_DATA_SLOT(void**, "?_singleton@Device@Input@@0PEAV12@EA"); }
void** SlotInputConfig()     { return RT_DATA_SLOT(void**, "?_singleton@Configuration@Input@@0PEAV12@EA"); }
void** SlotAudioDevice()     { return RT_DATA_SLOT(void**, "?_singleton@Device@Audio@@0PEAV12@EA"); }
void** SlotAudioConfig()     { return RT_DATA_SLOT(void**, "?_singleton@Configuration@Audio@@0PEAV12@EA"); }

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
static_assert(offsetof(RtInput::Configuration, mouseDisplay) == 0x0A,
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
    using ThreadCtorFn = void* (*)(void*, uint32_t);
    static const void* baseVtable =
        reinterpret_cast<const void*>(ResolveAddr(Module::Runtime, 0x180155860));

    W64(self, 0x00, 0);
    W32(self, 0x08, 0);
    RT_EXPORT_ONCE(ThreadCtorFn, "??0Thread@System@@QEAA@I@Z")(
        static_cast<uint8_t*>(self) + 0x10, 0x10000);
    WP (self, 0x10, baseVtable);
    W64(self, 0x40, 0);
    W64(self, 0x48, 1);
    W32(self, 0x50, 0);
    W64(self, 0x60, 0);
    W64(self, 0x68, 0);
    W64(self, 0x70, 0);
    W64(self, 0x78, 0);
    W64(self, 0x80, 0);
    W64(self, 0x88, 0);
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
//   MOV word  [RCX+0x10],0x100        ; _stateLow = 0, _running = 1
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
// Audio::Stream is reached BY NAME, not by address: it is exported, it is not
// ported yet, and asking by name is what makes this call site follow the port
// when someone does port it. Today it resolves to the DLL, which is exactly the
// documented shape for an unported neighbour.
//
// *** ONE RESIDUAL CUTOVER DEPENDENCY, NAMED. *** The per-record vtable at
// 0x180157c60 is not an export and is not in lane C2's mint table, so it is
// reached by address and will be null at cutover. Same handover as
// Persistence's 0x180155860.
//
// NO TYPE HEADER for Audio::Device — Audio.h models Scene, Channel, EffectSlot
// and Configuration, not the device. The two array shapes above are pinned by
// this constructor and by nothing else, so they are transcribed as offsets and
// left unnamed rather than turned into a struct on one witness.
// ===========================================================================
void* Ctor_AudioDevice(void* self) {
    using StreamCtorFn = void* (*)(void*);
    static const void* slotVtable =
        reinterpret_cast<const void*>(ResolveAddr(Module::Runtime, 0x180157c60));

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
    for (int i = 0; i < 64; ++i) {
        const size_t base = 0xa98 + static_cast<size_t>(i) * 0x20;
        WP (self, base + 0x00, slotVtable);
        W64(self, base + 0x08, 0);
        W64(self, base + 0x18, 0);
    }
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
// It runs only when the diff harness is on (`if (!RtDiffEnabled()) return;`),
// which is both the correct semantics — it IS an oracle — and what keeps its
// `RT_ORIGINAL_ONCE` calls off a normal run's path for tools/rawdoor_audit.py.
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

#define LANEX_PROBE(LABEL, SIZE, SLOTFN, CTORNAME, OURFN)                      \
    do {                                                                       \
        RT_DIFF_BEGIN(LABEL);                                                  \
        using CtorFn = void* (*)(void*);                                       \
        void** slot__ = SLOTFN();                                              \
        void*  saved__ = slot__ ? *slot__ : nullptr;                           \
        std::memset(g_ours,   0xCD, (SIZE));                                   \
        std::memset(g_theirs, 0xCD, (SIZE));                                   \
        if (rt_diff_on__) {                                                    \
            RT_ORIGINAL_ONCE(CtorFn, CTORNAME)(g_theirs);                      \
        }                                                                      \
        OURFN(g_ours);                                                         \
        const bool published__ = (slot__ != nullptr &&                         \
                                  *slot__ == static_cast<void*>(g_ours));      \
        if (published__) ++g_singletonOk;                                      \
        RT_DIFF_SHADOW(published__, "_singleton == our object",                \
                       "the ctor must publish the slot");                      \
        RT_DIFF_BYTES(g_ours, g_theirs, (SIZE));                               \
        RT_DIFF_WORK(true);                                                    \
        ControlMustDetect((SIZE));                                             \
        if (slot__) *slot__ = saved__;                                         \
        ++g_probed;                                                            \
    } while (0)

}  // namespace

// ---------------------------------------------------------------------------
// RtLaneXSingletonCtorProbe — called at the end of RtSingletonCtorsPortInit().
// ---------------------------------------------------------------------------
void RtLaneXSingletonCtorProbe() {
    if (!RtDiffEnabled()) return;

    // ---- MemoryAllocator: compare AROUND the CRITICAL_SECTION at +0x640 ----
    // Lane E3's row, and the reason this lane exists in the shape it does.
    {
        RT_DIFF_BEGIN("Wave9X::System::MemoryAllocator::ctor");
        using CtorFn = void* (*)(void*);
        constexpr size_t kLockAt  = offsetof(rt::system::MemoryAllocator, lock);
        constexpr size_t kAfterAt = offsetof(rt::system::MemoryAllocator, pageSize);
        void** slot = SlotMemoryAllocator();
        void*  saved = slot ? *slot : nullptr;
        std::memset(g_ours,   0xCD, kSizeMemoryAllocator);
        std::memset(g_theirs, 0xCD, kSizeMemoryAllocator);
        if (rt_diff_on__) {
            RT_ORIGINAL_ONCE(CtorFn, "??0MemoryAllocator@System@@QEAA@XZ")(g_theirs);
        }
        Ctor_MemoryAllocator(g_ours);
        const bool published = (slot != nullptr && *slot == static_cast<void*>(g_ours));
        if (published) ++g_singletonOk;
        RT_DIFF_SHADOW(published, "_singleton == our object",
                       "the ctor must publish the slot");
        RT_DIFF_BYTES(g_ours, g_theirs, kLockAt);
        RT_DIFF_BYTES(g_ours + kAfterAt, g_theirs + kAfterAt,
                      kSizeMemoryAllocator - kAfterAt);
        RT_DIFF_WORK(true);
        ControlMustDetect(kLockAt);
        RtLaneXPlatformLockDelete(g_ours   + kLockAt);
        RtLaneXPlatformLockDelete(g_theirs + kLockAt);
        if (slot) *slot = saved;
        ++g_probed;
    }

    LANEX_PROBE("Wave9X::IO::Modification::ctor", kSizeModification,
                SlotModification, "??0Modification@IO@@QEAA@XZ", Ctor_Modification);
    LANEX_PROBE("Wave9X::Object::Factory::ctor", kSizeFactory,
                SlotFactory, "??0Factory@Object@@QEAA@XZ", Ctor_Factory);
    LANEX_PROBE("Wave9X::Object::ExternalFactory::ctor", kSizeExternalFactory,
                SlotExternalFactory, "??0ExternalFactory@Object@@QEAA@XZ",
                Ctor_ExternalFactory);
    LANEX_PROBE("Wave9X::IO::FileSystem::ctor", kSizeFileSystem,
                SlotFileSystem, "??0FileSystem@IO@@QEAA@XZ", Ctor_FileSystem);
    LANEX_PROBE("Wave9X::Video::Configuration::ctor", kSizeVideoConfig,
                SlotVideoConfig, "??0Configuration@Video@@QEAA@XZ",
                Ctor_VideoConfiguration);
    LANEX_PROBE("Wave9X::Input::Device::ctor", kSizeInputDevice,
                SlotInputDevice, "??0Device@Input@@QEAA@XZ", Ctor_InputDevice);
    LANEX_PROBE("Wave9X::Input::Configuration::ctor", kSizeInputConfig,
                SlotInputConfig, "??0Configuration@Input@@QEAA@XZ",
                Ctor_InputConfiguration);
    LANEX_PROBE("Wave9X::Audio::Configuration::ctor", kSizeAudioConfig,
                SlotAudioConfig, "??0Configuration@Audio@@QEAA@XZ",
                Ctor_AudioConfiguration);

    // ---- Application: a second argument (the HINSTANCE) -------------------
    {
        RT_DIFF_BEGIN("Wave9X::Application::Application::ctor");
        using CtorFn = void* (*)(void*, void*);
        void* const kArg = reinterpret_cast<void*>(static_cast<uintptr_t>(0x0badc0deull));
        void** slot = SlotApplication();
        void*  saved = slot ? *slot : nullptr;
        std::memset(g_ours,   0xCD, kSizeApplication);
        std::memset(g_theirs, 0xCD, kSizeApplication);
        if (rt_diff_on__) {
            RT_ORIGINAL_ONCE(CtorFn, "??0Application@0@QEAA@PEAX@Z")(g_theirs, kArg);
        }
        Ctor_Application(g_ours, kArg);
        const bool published = (slot != nullptr && *slot == static_cast<void*>(g_ours));
        if (published) ++g_singletonOk;
        RT_DIFF_SHADOW(published, "_singleton == our object",
                       "the ctor must publish the slot");
        RT_DIFF_BYTES(g_ours, g_theirs, kSizeApplication);
        RT_DIFF_WORK(true);
        ControlMustDetect(kSizeApplication);
        if (slot) *slot = saved;
        ++g_probed;
    }

    // ---- Video::Device: type 0 publishes, any other type MUST NOT ---------
    // The second call is this lane's per-class negative control. The retail
    // CMOVE only takes `this` when the DeviceType argument is 0, so a port that
    // published unconditionally would look correct on every check above and
    // would silently hand `?_singleton@Device@Video@@` to a secondary device.
    {
        RT_DIFF_BEGIN("Wave9X::Video::Device::ctor(type=0)");
        using CtorFn = void* (*)(void*, int32_t);
        void** slot = SlotVideoDevice();
        void*  saved = slot ? *slot : nullptr;
        std::memset(g_ours,   0xCD, kSizeVideoDevice);
        std::memset(g_theirs, 0xCD, kSizeVideoDevice);
        if (rt_diff_on__) {
            RT_ORIGINAL_ONCE(CtorFn, "??0Device@Video@@QEAA@W4DeviceType@01@@Z")(g_theirs, 0);
        }
        Ctor_VideoDevice(g_ours, 0);
        const bool published = (slot != nullptr && *slot == static_cast<void*>(g_ours));
        if (published) ++g_singletonOk;
        RT_DIFF_SHADOW(published, "_singleton == our object",
                       "DeviceType 0 must publish the slot");
        // The CRITICAL_SECTION at +0x60 holds a DebugInfo pointer and an owner
        // thread id that InitializeCriticalSection fills from live state, so
        // the two copies legitimately differ there. Compare the two ranges
        // AROUND it -- everything the constructor's own stores produced. The
        // +0x88/+0x8c pair immediately after the lock is Lock()'s bookkeeping,
        // which the ctor does not write either (Device.h says so), so the
        // second range starts at +0x90.
        RT_DIFF_BYTES(g_ours, g_theirs, offsetof(RtVideo::Device, lock));
        RT_DIFF_BYTES(g_ours + 0x90, g_theirs + 0x90, kSizeVideoDevice - 0x90);
        RT_DIFF_WORK(true);
        ControlMustDetect(offsetof(RtVideo::Device, lock));
        RtLaneXPlatformLockDelete(g_ours + offsetof(RtVideo::Device, lock));
        RtLaneXPlatformLockDelete(g_theirs + offsetof(RtVideo::Device, lock));
        if (slot) *slot = saved;
        ++g_probed;
    }
    {
        RT_DIFF_BEGIN("Wave9X::Video::Device::ctor(type=1, must NOT publish)");
        void** slot = SlotVideoDevice();
        void*  saved = slot ? *slot : nullptr;
        std::memset(g_ours, 0xCD, kSizeVideoDevice);
        Ctor_VideoDevice(g_ours, 1);
        const bool untouched = (slot == nullptr || *slot == saved);
        RT_DIFF_SHADOW(untouched, "_singleton unchanged",
                       "a non-zero DeviceType must leave the slot alone");
        RT_DIFF_WORK(true);
        RtLaneXPlatformLockDelete(g_ours + offsetof(RtVideo::Device, lock));
        if (slot) *slot = saved;
        ++g_probed;
    }

    // ---- InternalFactory: deterministic prefix + a self-consistent MT -----
    // The shipped constructor seeds from wall-clock seconds, so the 624-word
    // state legitimately differs between two calls. Compared here: the bytes
    // BEFORE the seed (+0x00..+0x23, which includes Random::index), plus a
    // check that the 623-word recurrence in OUR copy really follows from OUR
    // mt[0]. That is what is checkable; claiming more would be a lie.
    {
        RT_DIFF_BEGIN("Wave9X::Object::InternalFactory::ctor");
        using CtorFn = void* (*)(void*);
        void** slot = SlotInternalFactory();
        void*  saved = slot ? *slot : nullptr;
        std::memset(g_ours,   0xCD, kSizeInternalFactory);
        std::memset(g_theirs, 0xCD, kSizeInternalFactory);
        if (rt_diff_on__) {
            RT_ORIGINAL_ONCE(CtorFn, "??0InternalFactory@Object@@QEAA@XZ")(g_theirs);
        }
        Ctor_InternalFactory(g_ours);
        const bool published = (slot != nullptr && *slot == static_cast<void*>(g_ours));
        if (published) ++g_singletonOk;
        RT_DIFF_SHADOW(published, "_singleton == our object",
                       "the ctor must publish the slot");
        RT_DIFF_BYTES(g_ours, g_theirs, 0x24);   // everything up to mt[0]

        bool recurrenceOk = true;
        for (int i = 1; i < 624; ++i) {
            uint32_t prev = 0, cur = 0;
            std::memcpy(&prev, g_ours + 0x24 + (i - 1) * 4, 4);
            std::memcpy(&cur,  g_ours + 0x24 + i * 4,       4);
            const uint32_t want = (prev ^ (prev >> 30)) * 0x6c078965u
                                + static_cast<uint32_t>(i);
            if (cur != want) { recurrenceOk = false; break; }
        }
        RT_DIFF_SHADOW(recurrenceOk, "mt[1..623] follows from mt[0]",
                       "init_genrand recurrence (seed is wall-clock, not diffed)");
        RT_DIFF_WORK(true);
        ControlMustDetect(0x24);
        if (slot) *slot = saved;
        ++g_probed;
    }

    // EFFECTIVENESS CONTROL, printed rather than assumed. `subjects executed`
    // is the calls= this lane would otherwise not have at all, `published` is
    // the acceptance criterion met per subject, and the corruption control
    // proves the byte comparison can still FAIL — a memcmp over two identically
    // 0xCD-filled buffers is exactly the shape that passes when a size or a
    // pointer is wrong. `control caught` MUST equal `control run`.
    printf("[RtPort] Wave9 lane X probe: %d subjects executed, "
           "%d published their singleton, control caught %d of %d corruptions\n",
           g_probed, g_singletonOk, g_controlCaught, g_controlRun);
    printf("[RtPort] Wave9 lane X: IO::Persistence and Audio::Device are "
           "REGISTERED but NOT probed -- their base ctor starts a suspended "
           "worker thread (see their banners in singleton_ctors.cpp)\n");
}

// ---------------------------------------------------------------------------
// Registration.
//
// Registering redirects OUR call sites and nothing else, and our tree has NO
// call site for any of these fourteen names — the callers are Engine::Engine
// and Engine::Startup inside the DLL. So every registration here is inert on a
// live run and load-bearing only at cutover, which is precisely the property
// that makes it safe to land the two unprobed ones.
// ---------------------------------------------------------------------------
void RtSingletonCtorsPortInit() {
    if (!RtWaveEnabled("BANISHED_RT_SINGLETON_CTORS")) {
        printf("[RtPort] BANISHED_RT_SINGLETON_CTORS=0 -- lane X unregistered\n");
        return;
    }
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

    RtLaneXSingletonCtorProbe();
}

#undef LANEX_PROBE
