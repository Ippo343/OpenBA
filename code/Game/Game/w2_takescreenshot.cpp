// code/Game/Game/w2_takescreenshot.cpp
//
// Game::GameUpdateLoop::TakeScreenshot  —  port of exe 0x140095da0.
//
// Registered at 0x140095da0 by standalone/reg_w2_screenshot.cpp, which is what
// makes `fn_QuickSave` in code/Game/Game/gameupdateloop.cpp non-null. That
// pointer is misnamed: 0x140095da0 is NOT a quicksave handler, it is the
// screenshot grab that GameUpdateLoop::Draw runs when either the quicksave
// (key 0x21) or the quickload (0x22) flag is set. Retail's lifecycle is
// Update sets +0xC0/+0xC1, Draw tests them, this function clears both.
//
// ---------------------------------------------------------------------------
// Retail body (Ghidra 0x140095da0, disassembly cross-read instruction by
// instruction — the decompiler's arg order for Create is misleading):
//
//   RSI  = *(void**)Video::Device::_singleton        ; slot 0x140151a80
//   RBP  = (*(void**)Engine::MainLoop::_singleton)[0x10]   ; slot 0x140151440
//                                                   ; = the Video::Context*
//   this = MemAlloc(0x88, 8)                        ; STATIC 2-arg MemAlloc
//   if (this) { FrameCopyImageBuffer::ctor(this); *(void**)this = &vt_14015e370; }
//   else this = 0;                                  ; then dereferenced anyway
//   (*(vt+0x48))(this, 0, 4, 5, 0x100, RSI[4], RSI[8], 1,1,1,1, nullptr);
//   (*(vt+0x50))(this, RBP, 1);
//   base = Filename(IO::FileSystem::GetSaveDirectory(fs)
//                   + "\\Screenshots\\Screenshot");
//   Filename path;  int n = 0;
//   do { path = base + String::Format("%i.%s", n++, "bmp"); }
//   while (fs->Exists(path));
//   px = MemAlloc((int64)(RSI[8] * RSI[4]) << 2, 4);
//   this->CopyFromTexture(px, RSI[4], RSI[8]);
//   ImageBuffer::SaveImageToFile(path, RSI[4], RSI[8], px);   ; STATIC
//   MemFree(px);
//   (*(vt+0x00))(this, 1);                          ; ~FrameCopyImageBuffer + MemFree
//   *(uint16_t*)(self + 0xC0) = 0;                  ; clears BOTH flags
//
// Device+4 = screen width, Device+8 = screen height (same pair read by
// toolbardialog.cpp:1822 for its centering math).
//
// ---------------------------------------------------------------------------
// Two deliberate, documented departures from a literal transcription:
//
// 1. The vptr overwrite (`*(void**)this = &PTR_FUN_14015e370`) is NOT
//    replicated. That exe table is 12 slots; slot 0 is exe code (0x140019d50,
//    verified: `~FrameCopyImageBuffer(this); if (flag&1) MemFree(this);`) and
//    slots 1-11 are import thunks straight back to the Runtime's own virtuals
//    — i.e. it is a copy of the Runtime's vtable with slot 0 relinked. So the
//    ctor-installed Runtime vptr already dispatches every virtual identically,
//    and leaving it alone is strictly safer: anything inside Create/Generate
//    that dispatches on its own vptr keeps working, which a partially
//    reconstructed exe table could not guarantee.
//    Consequently Create/Generate are called through their exported names
//    (they ARE the functions the exe's +0x48/+0x50 thunks point at — verified
//    with tools/dispatch_slot_map.py on 0x1401520b0 / 0x1401520b8), and the
//    teardown does exactly what slot 0 does: dtor, then MemFree.
//
// 2. The retail body derefs `this` unguarded after a failed MemAlloc, and
//    calls seven Runtime methods through loader-filled .rdata slots that
//    cannot be null in retail. Ours resolves them at runtime and could get a
//    null, so there is one up-front resolve gate. That is the CLAUDE.md
//    exception (our build genuinely differs from retail), not a guard over a
//    faithful crash — and the gate still performs the +0xC0 clear, because
//    leaving the flags set re-enters this path every frame forever.
// ---------------------------------------------------------------------------

// ENDGAME B.3.5 Video/Graphics caller batch: Video::ImageBuffer is a real class
// and `SaveImageToFile` is its class static. ImageBuffer.h pulls no Volume.h, so
// it is safe after resolve.h's <windows.h> here.
#include "resolve.h"
#include "video/ImageBuffer.h"
#include "engine_funcs.h"
#include "io/FileSystem.h"      // ENDGAME B.3.4a closing 4/4
#include "system/Filename.h"
#include <new>

#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace {

// --- Runtime primitives, all named PE imports of the retail exe -------------
// System::MemoryAllocator::MemAlloc / MemFree are the STATIC overloads here
// (?MemAlloc@MemoryAllocator@System@@SAPEAX_KH@Z), not the singleton-taking
// Allocate that engine_funcs.h exposes as fn_MemAlloc. The call site passes
// two registers (ECX=size, EDX=alignment), so it must be the static pair.
typedef void* (*MemAllocS_t)(uint64_t size, int alignment);          // 0x140152b60
typedef void  (*MemFreeS_t)(void* ptr);                              // 0x140152b68

typedef void* (*FCIB_Ctor_t)(void* self);                            // 0x140151f40
typedef void  (*FCIB_Dtor_t)(void* self);                            // 0x140151f48

// ?Create@FrameCopyImageBuffer@Graphics@@UEAA_N ...                 // 0x1401520b0
// (ImageType, Usage, Format, uint mip, int w, int h, FilterMode,
//  AddressMode, AddressMode, AddressMode, Collection::Array<...>*)
typedef bool (*FCIB_Create_t)(void* self, int imageType, int usage, int format,
                              unsigned int mip, int width, int height,
                              int filterMode, int addr0, int addr1, int addr2,
                              void* initialData);
// ?Generate@FrameCopyImageBuffer@Graphics@@UEBAX AEAVContext@Video@@ H @Z
typedef void (*FCIB_Generate_t)(const void* self, void* videoContext, int flag); // 0x1401520b8

// ENDGAME B.3.4a closing 4/4: the GetSaveDirectory and Exists door typedefs,
// their file-statics, their resolver lines, their two `s_ok` conjuncts and
// their two diagnostic `%p`s are gone TOGETHER with the calls they served --
// both are `IO::FileSystem` members now. The dispatch slots 0x1401520c0 /
// 0x1401520d0 stay bound in the address registry (B34_RULES 6.5(g)).

// ??HSystem@@YA?AVString@0@AEBV10@PEBD@Z  — FREE operator+(String, char const*)
// RCX = return buffer, RDX = lhs, R8 = cstr.                        // 0x140151a08
typedef void* (*StrPlusCStr_t)(void* retStr, const void* lhs, const char* rhs);
// ??HSystem@@YA?AVString@0@AEBV10@0@Z     — FREE operator+(String, String)
typedef void* (*StrPlusStr_t)(void* retStr, const void* lhs, const void* rhs); // 0x140151a10
// ?Format@String@System@@SA?AV12@PEBDZZ   — STATIC varargs, RCX = return buffer
typedef void* (*StrFormat_t)(void* retStr, const char* fmt, ...);    // 0x140151560
typedef void  (*StrDtor_t)(void* self);                              // 0x140152af0

typedef void* (*FN_CtorDefault_t)(void* self);                       // 0x140151d40
typedef void* (*FN_CtorString_t)(void* self, const void* str);       // 0x140151d68
typedef void* (*FN_AssignString_t)(void* self, const void* str);     // 0x1401520c8
typedef void  (*FN_Dtor_t)(void* self);                              // 0x1401529e8

// ?SaveImageToFile@ImageBuffer@Video@@SA_NAEBVFilename@System@@HHPEAI@Z — STATIC
typedef bool (*SaveImageToFile_t)(const void* filename, int width, int height,
                                  unsigned int* pixels);             // 0x1401520e0

MemAllocS_t           s_MemAlloc          = nullptr;
MemFreeS_t            s_MemFree           = nullptr;
FCIB_Ctor_t           s_FCIB_Ctor         = nullptr;
FCIB_Dtor_t           s_FCIB_Dtor         = nullptr;
FCIB_Create_t         s_FCIB_Create       = nullptr;
FCIB_Generate_t       s_FCIB_Generate     = nullptr;
StrPlusCStr_t         s_StrPlusCStr       = nullptr;
StrPlusStr_t          s_StrPlusStr        = nullptr;
StrFormat_t           s_StrFormat         = nullptr;
StrDtor_t             s_StrDtor           = nullptr;
FN_CtorDefault_t      s_Filename_Ctor     = nullptr;
FN_CtorString_t       s_Filename_CtorStr  = nullptr;
FN_AssignString_t     s_Filename_Assign   = nullptr;
FN_Dtor_t             s_Filename_Dtor     = nullptr;
SaveImageToFile_t     s_SaveImageToFile   = nullptr;

void** s_pFileSystemSingleton = nullptr;   // ?_singleton@FileSystem@IO@@0PEAV12@EA

bool s_resolved = false;
bool s_ok       = false;

bool ResolveFuncs() {
    if (s_resolved) return s_ok;
    s_resolved = true;

    s_MemAlloc         = (MemAllocS_t)          ResolveExport("?MemAlloc@MemoryAllocator@System@@SAPEAX_KH@Z");
    s_MemFree          = (MemFreeS_t)           ResolveExport("?MemFree@MemoryAllocator@System@@SAXPEAX@Z");
    s_FCIB_Ctor        = (FCIB_Ctor_t)          ResolveExport("??0FrameCopyImageBuffer@Graphics@@QEAA@XZ");
    s_FCIB_Dtor        = (FCIB_Dtor_t)          ResolveExport("??1FrameCopyImageBuffer@Graphics@@UEAA@XZ");
    s_FCIB_Create      = (FCIB_Create_t)        ResolveExport(
        "?Create@FrameCopyImageBuffer@Graphics@@UEAA_NW4ImageType@ImageBuffer@Video@@W4Usage@45@"
        "W4Format@45@IHHW4FilterMode@45@W4AddressMode@45@44PEAV?$Array@V?$Array@E@Collection@@@Collection@@@Z");
    s_FCIB_Generate    = (FCIB_Generate_t)      ResolveExport(
        "?Generate@FrameCopyImageBuffer@Graphics@@UEBAXAEAVContext@Video@@H@Z");
    s_StrPlusCStr      = (StrPlusCStr_t)        ResolveExport("??HSystem@@YA?AVString@0@AEBV10@PEBD@Z");
    s_StrPlusStr       = (StrPlusStr_t)         ResolveExport("??HSystem@@YA?AVString@0@AEBV10@0@Z");
    s_StrFormat        = (StrFormat_t)          ResolveExport("?Format@String@System@@SA?AV12@PEBDZZ");
    s_StrDtor          = (StrDtor_t)            ResolveExport("??1String@System@@QEAA@XZ");
    s_Filename_Ctor    = (FN_CtorDefault_t)     ResolveExport("??0Filename@System@@QEAA@XZ");
    s_Filename_CtorStr = (FN_CtorString_t)      ResolveExport("??0Filename@System@@QEAA@AEBVString@1@@Z");
    s_Filename_Assign  = (FN_AssignString_t)    ResolveExport("??4Filename@System@@QEAAAEAV01@AEBVString@1@@Z");
    s_Filename_Dtor    = (FN_Dtor_t)            ResolveExport("??1Filename@System@@QEAA@XZ");
    s_SaveImageToFile  = (SaveImageToFile_t)    ResolveExport("?SaveImageToFile@ImageBuffer@Video@@SA_NAEBVFilename@System@@HHPEAI@Z");

    s_pFileSystemSingleton = (void**)ResolveExport("?_singleton@FileSystem@IO@@0PEAV12@EA");

    s_ok = s_MemAlloc && s_MemFree && s_FCIB_Ctor && s_FCIB_Dtor && s_FCIB_Create &&
           s_FCIB_Generate && s_StrPlusCStr &&
           s_StrPlusStr && s_StrFormat && s_StrDtor && s_Filename_Ctor &&
           s_Filename_CtorStr && s_Filename_Assign && s_Filename_Dtor &&
           s_SaveImageToFile && s_pFileSystemSingleton &&
           // from engine_funcs.h — both are exref-backed and could be null:
           // B.3.5: the CopyFromTexture readiness term went with its door (the
           // global itself stays, and the printf below still reports it).
           g_pVideoDeviceSingleton && g_pEngineSingleton;

    if (!s_ok) {
        printf("[TakeScreenshot] primitives unresolved — screenshot disabled\n"
                    "  alloc=%p free=%p ctor=%p dtor=%p create=%p gen=%p\n"
                    "  s+c=%p s+s=%p fmt=%p ~str=%p fn()=%p fn(s)=%p fn==%p ~fn=%p save=%p fs=%p\n"
                    "  copyFromTex=%p device=%p mainloop=%p\n",
                    (void*)s_MemAlloc, (void*)s_MemFree, (void*)s_FCIB_Ctor, (void*)s_FCIB_Dtor,
                    (void*)s_FCIB_Create, (void*)s_FCIB_Generate,
                    (void*)s_StrPlusCStr, (void*)s_StrPlusStr, (void*)s_StrFormat,
                    (void*)s_StrDtor, (void*)s_Filename_Ctor, (void*)s_Filename_CtorStr,
                    (void*)s_Filename_Assign, (void*)s_Filename_Dtor, (void*)s_SaveImageToFile,
                    (void*)s_pFileSystemSingleton, (void*)fn_ImageBuffer_CopyFromTexture,
                    (void*)g_pVideoDeviceSingleton, (void*)g_pEngineSingleton);
        std::fflush(stdout);
    }
    return s_ok;
}

// System::String and System::Filename are both 8-byte handles (a pointer to
// refcounted character data). Confirmed two ways: engine_funcs.h:1939 documents
// "Filename, 8 bytes", and retail's four locals here sit at [RSP+0xa0], +0xa8,
// +0xb0, +0xb8 — exactly 8 apart.
struct alignas(8) StringHandle { void* p; };

}  // namespace

// ============================================================================
// GameUpdateLoop::TakeScreenshot(self) — exe 0x140095da0.
// Signature matches fn_QuickSave_t in gameupdateloop.cpp: void(void*).
// ============================================================================
extern "C" void GameUpdateLoop_TakeScreenshot(void* self) {

    if (!ResolveFuncs()) {
        // See departure (2) in the header comment: consume the request anyway,
        // or Draw re-enters this branch on every frame for the rest of the run.
        if (self) *reinterpret_cast<uint16_t*>(reinterpret_cast<uintptr_t>(self) + 0xC0) = 0;
        return;
    }

    void* device   = *g_pVideoDeviceSingleton;                      // Video::Device*
    void* mainLoop = *g_pEngineSingleton;                           // Engine::MainLoop*
    void* videoCtx = mainLoop ? *reinterpret_cast<void**>(
                         reinterpret_cast<uintptr_t>(mainLoop) + 0x10) : nullptr;
    void* fileSys  = *s_pFileSystemSingleton;

    const int width  = *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(device) + 4);
    const int height = *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(device) + 8);

    // --- Grab the back buffer into a FrameCopyImageBuffer ---------------------
    void* frameCopy = s_MemAlloc(0x88, 8);
    if (frameCopy) {
        s_FCIB_Ctor(frameCopy);
        // Retail additionally stores &PTR_FUN_14015e370 over the vptr here; see
        // departure (1). The ctor-installed Runtime vptr is kept instead.
    }

    // Retail's literal arg list: (imageType=0, usage=4, format=5, mip=0x100,
    // width, height, filterMode=1, addressMode 1,1,1, initialData=nullptr).
    s_FCIB_Create(frameCopy, 0, 4, 5, 0x100u, width, height, 1, 1, 1, 1, nullptr);
    s_FCIB_Generate(frameCopy, videoCtx, 1);

    // --- <saveDir>\Screenshots\Screenshot<N>.bmp, first N that does not exist --
    StringHandle saveDir{};   // Filename, from GetSaveDirectory
    StringHandle joined{};    // String temp
    StringHandle base{};      // Filename: saveDir + "\Screenshots\Screenshot"
    StringHandle path{};      // Filename: the candidate being tested

    // ENDGAME B.3.4a closing: onto IO::FileSystem::GetSaveDirectory, which
    // became a member in this same commit. `saveDir` stays a raw StringHandle
    // and is still released by `s_Filename_Dtor` below, because that is what
    // the door did: it filled the 8 raw bytes of a hidden return slot. The
    // member's by-value return fills the same slot through the same ABI, so
    // the construction/release pairing and its ORDER are unchanged.
    ::new (&saveDir) System::Filename(
        reinterpret_cast<const IO::FileSystem*>(fileSys)->GetSaveDirectory());   // B.3.4 bridge
    s_StrPlusCStr(&joined, &saveDir, "\\Screenshots\\Screenshot");   // exe 0x14015e520
    s_Filename_CtorStr(&base, &joined);
    s_StrDtor(&joined);
    s_Filename_Dtor(&saveDir);

    s_Filename_Ctor(&path);

    int index = 0;
    do {
        StringHandle suffix{};   // "<N>.bmp"
        StringHandle full{};
        // exe 0x14015e238 = "%i.%s", exe 0x14015e518 = "bmp" (62 6d 70 00) —
        // BMP, not PNG.
        s_StrFormat(&suffix, "%i.%s", index, "bmp");
        s_StrPlusStr(&full, &base, &suffix);
        s_Filename_Assign(&path, &full);
        ++index;
        s_StrDtor(&full);
        s_StrDtor(&suffix);
    } while (reinterpret_cast<const IO::FileSystem*>(fileSys)->Exists(
                 *reinterpret_cast<const RtSystem::Filename*>(&path)));   // B.3.4 bridge

    // --- Read the pixels back and write the file -----------------------------
    unsigned int* pixels = reinterpret_cast<unsigned int*>(
        s_MemAlloc(static_cast<uint64_t>(static_cast<int64_t>(height * width)) << 2, 4));
    // B.3.5 bridge — ?CopyFromTexture@ImageBuffer@Video@@QEAA_NPEAXHH@Z.
    // `frameCopy` is a FrameCopyImageBuffer, which derives from Video::ImageBuffer
    // at offset 0 (GeneratedImageBuffer.h's chain), so the base member applies.
    reinterpret_cast<Video::ImageBuffer*>(frameCopy)->CopyFromTexture(
        pixels, width, height);   // B.3.5 bridge
    // B.3.5 bridge — ?SaveImageToFile@ImageBuffer@Video@@SA_NAEBVFilename@System@@HHPEAI@Z
    // is STATIC (`SA_N`), so there is no receiver: the door's first argument was
    // the Filename, not a `this`. `s_SaveImageToFile` and its resolve STAY — the
    // `s_ok` aggregate and the [TakeScreenshot] printf still read the pointer.
    Video::ImageBuffer::SaveImageToFile(
        *reinterpret_cast<const System::Filename*>(&path),
        width, height, pixels);   // B.3.5 bridge
    s_MemFree(pixels);


    // --- Teardown: exactly what the exe vtable's slot 0 (0x140019d50) does ----
    s_FCIB_Dtor(frameCopy);
    s_MemFree(frameCopy);

    // The ONLY clearer of m_quickSavePressed / m_quickLoadPressed. Retail stores
    // a single 16-bit zero over both bytes at self+0xC0.
    *reinterpret_cast<uint16_t*>(reinterpret_cast<uintptr_t>(self) + 0xC0) = 0;

    s_Filename_Dtor(&path);
    s_Filename_Dtor(&base);
}
