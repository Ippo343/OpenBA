#include "Dialog/ui_dialogs.h"
#include "engine_funcs.h"
#include <cstdint>
#include <cstdio>

// ============================================================
// LoaderDialog::Slot4 (0x1400B2D90) — vtable +0x20, Setup
// ============================================================
// Finds "loadingImage" element and stores pointer at +0xB8.
//
// Original: 0x1400B2D90.

void LoaderDialog_Slot4(LoaderDialog* self) {
    uintptr_t s = (uintptr_t)self;

    uint8_t nameStr[8];
    fn_String_Ctor_CStr(nameStr, "loadingImage");
    uint8_t retBuf[16];
    fn_DialogInstance_FindElement(self, retBuf, nameStr);
    *(void**)(s + 0xB8) = *(void**)retBuf;
    fn_String_Dtor(nameStr);
}

// LoaderDialog::Slot6 (0x1400B2D00) — Draw
// Applies the current rotation angle to the loading spinner image element.
void LoaderDialog_Slot6(LoaderDialog* self) {
    uintptr_t s = (uintptr_t)self;

    // Mirrors original 0x1400b2d00 — CheckPtr the InternalPtr<ImageElement>
    // at +0xB8 before reading. Per CLAUDE.md, skipping this can give stale
    // or zero values when the pointer is a weak/lazy reference.
    if (*(void**)(s + 0xB8) != nullptr && fn_InternalBasePtr_CheckPtr) {
        fn_InternalBasePtr_CheckPtr(reinterpret_cast<void*>(s + 0xB8));
    }

    void* imageElem = *(void**)(s + 0xB8);
    if (imageElem) {
        float angle = *(float*)(s + 0xC0);
        fn_ImageElement_SetRotation(imageElem, angle);
    }
}

// LoaderDialog::Slot7 (0x1400B2D50) — Update
// Accumulates rotation angle from elapsed time at 180 degrees/sec.
void LoaderDialog_Slot7(LoaderDialog* self, void* time) {
    uintptr_t s = (uintptr_t)self;
    float elapsed = fn_Time_AsSeconds(time);
    *(float*)(s + 0xC0) += elapsed * 180.0f;

#if 1   // A/B comparison dump: runs in BOTH dll and standalone modes so we
        // can compare the loaded Loading.rsc Dialog External byte-for-byte.
        // Fires once per unique LoaderDialog instance (s_seen dedup).
        // 2026-04-26: re-enabled to test "Runtime-global differs between modes"
        // hypothesis from project_standalone_splash_elements.md.
#  ifdef STANDALONE_EXE
        const char* AB_TAG = "A/B-STANDALONE";
#  else
        const char* AB_TAG = "A/B-DLL      ";
#  endif
    // Track up to 4 LoaderDialog instances; for each, find AnimatedImageElement
    // (vtbl = 0x180156000 = Runtime + 0x156000) during the first walk so we can
    // probe its +0xf8 accumulator on subsequent ticks. If +0xf8 grows tick over
    // tick, AnimatedImageElement::Update IS firing (and the spinner-static bug
    // is in Draw, not Update). If +0xf8 stays 0, the Element::Update walk never
    // reaches it.
    struct InstState {
        void* self;
        void* animatedImageElem;
        int callCount;
    };
    static InstState s_seen[4] = {{nullptr,nullptr,0},{nullptr,nullptr,0},{nullptr,nullptr,0},{nullptr,nullptr,0}};
    static int s_seenCount = 0;
    InstState* myState = nullptr;
    for (int i = 0; i < s_seenCount; ++i) {
        if (s_seen[i].self == self) { myState = &s_seen[i]; break; }
    }
    bool firstForThis = (myState == nullptr);
    if (firstForThis && s_seenCount < 4) {
        myState = &s_seen[s_seenCount++];
        myState->self = self;
        myState->animatedImageElem = nullptr;
        myState->callCount = 0;
    }
    if (myState) myState->callCount++;
    if (firstForThis && myState) {

        // Compute Runtime base so we can match vtable to 0x180156000 (AnimatedImageElement).
        HMODULE rtMod = GetModuleHandleA("Runtime-x64.dll");
        uintptr_t rtBase = reinterpret_cast<uintptr_t>(rtMod);
        uintptr_t targetAnimVtbl = rtBase + 0x156000;

        auto walk = [AB_TAG, targetAnimVtbl, myState](auto&& self_lambda, void* elem, int depth) -> void {
            if (!elem || depth > 6) return;
            void* vtbl = *(void**)elem;
            void** kidsArr = *(void***)((uintptr_t)elem + 0xa0);
            int    kidsCnt = *(int*) ((uintptr_t)elem + 0xa8);
            uint32_t flags = *(uint32_t*)((uintptr_t)elem + 0x34);
            void* parentField = *(void**)((uintptr_t)elem + 0x98);
            uint64_t f8 = *(uint64_t*)((uintptr_t)elem + 0xf8);
            const char* cls = "Element";
            if (reinterpret_cast<uintptr_t>(vtbl) == targetAnimVtbl) {
                cls = "AnimatedImageElement";
                if (!myState->animatedImageElem) myState->animatedImageElem = elem;
            }
            printf("[%s] ", AB_TAG);
            for (int i = 0; i < depth; ++i) printf("  ");
            printf("elem=%p vtbl=%p [%s] kids=%d (arr=%p) flags=0x%08x parent@+0x98=%p +0xf8=0x%016llx\n",
                   elem, vtbl, cls, kidsCnt, kidsArr, flags, parentField, (unsigned long long)f8);
            for (int i = 0; i < kidsCnt; ++i) {
                self_lambda(self_lambda, kidsArr[i], depth + 1);
            }
        };

        void* rootElem = *(void**)(s + 0x50);
        printf("[%s] === element tree dump (self=%p) ===\n", AB_TAG, (void*)self);
        walk(walk, rootElem, 0);
        printf("[%s] === end element tree ===\n", AB_TAG);

        // Dump dialog External + wrapped Dialog object so we can see whether
        // deserialization populated _rootElement and _elements.
        void* dialogExternal = *(void**)(s + 0x18);
        printf("[%s] DialogInstance+0x18 (Dialog*) = %p\n", AB_TAG, dialogExternal);
        if (dialogExternal) {
            void* rootDescIBP = (void*)((uintptr_t)dialogExternal + 0x38);
            void* rootDesc = *(void**)rootDescIBP;
            printf("[%s] Dialog+0x38 (rootDesc IBP at %p) -> %p\n",
                   AB_TAG, rootDescIBP, rootDesc);
            if (rootDesc) {
                void* descVtbl = *(void**)rootDesc;
                int elemCount = *(int*)((uintptr_t)rootDesc + 0x70);
                void* elemArr = *(void**)((uintptr_t)rootDesc + 0x68);
                printf("[%s] rootDesc vtbl=%p +0x68 (elems arr)=%p +0x70 (count)=%d\n",
                       AB_TAG, descVtbl, elemArr, elemCount);
                // Dump first 0x100 bytes of the description for context.
                for (int off = 0; off < 0x100; off += 16) {
                    uint64_t a = *(uint64_t*)((uintptr_t)rootDesc + off);
                    uint64_t b = *(uint64_t*)((uintptr_t)rootDesc + off + 8);
                    char ascii[17] = {0};
                    for (int i = 0; i < 16; ++i) {
                        unsigned char c = ((unsigned char*)((uintptr_t)rootDesc + off))[i];
                        ascii[i] = (c >= 32 && c < 127) ? c : '.';
                    }
                    printf("[%s]     desc+0x%02x: %016llx %016llx |%s|\n", AB_TAG, off,
                           (unsigned long long)a, (unsigned long long)b, ascii);
                }
            }
            // Dump 0x100 bytes of the dialog external + ascii.
            printf("[%s] === dialog External hex ===\n", AB_TAG);
            for (int off = 0; off < 0x100; off += 16) {
                uint64_t a = *(uint64_t*)((uintptr_t)dialogExternal + off);
                uint64_t b = *(uint64_t*)((uintptr_t)dialogExternal + off + 8);
                char ascii[17] = {0};
                for (int i = 0; i < 16; ++i) {
                    unsigned char c = ((unsigned char*)((uintptr_t)dialogExternal + off))[i];
                    ascii[i] = (c >= 32 && c < 127) ? c : '.';
                }
                printf("[%s]     ext+0x%02x: %016llx %016llx |%s|\n", AB_TAG, off,
                       (unsigned long long)a, (unsigned long long)b, ascii);
            }

            // Inspect the 'sibling' pointers ext+0x20 and ext+0x28 — these
            // might be other Description objects from the same .rsc.
            for (int idx = 0; idx < 2; ++idx) {
                int srcOff = idx == 0 ? 0x20 : 0x28;
                void* sib = *(void**)((uintptr_t)dialogExternal + srcOff);
                if (!sib) {
                    printf("[%s]     sibling@+0x%02x: null\n", AB_TAG, srcOff);
                    continue;
                }
                void* sibVtbl = *(void**)sib;
                int sibCnt = *(int*)((uintptr_t)sib + 0x70);
                void* sibArr = *(void**)((uintptr_t)sib + 0x68);
                printf("[%s]     sibling@+0x%02x: %p vtbl=%p +0x68(arr)=%p +0x70(cnt)=%d\n",
                       AB_TAG, srcOff, sib, sibVtbl, sibArr, sibCnt);
            }
        }
        fflush(stdout);
    } else if (myState && myState->callCount <= 30) {
        // Probe AnimatedImageElement state per-tick: +0xf8 (time accumulator),
        // +0xc0/+0xc4 (current sprite frame), +0xe0 (frame array ptr),
        // +0xe8 (frame count), +0xf0 (fps), +0x34 (flags incl. dirty bit 0x8000000).
        // From the Update decomp:
        //   accumulator += time->ticks
        //   frameIndex = (accumF8_seconds / (e8/f0)) * e8
        //   if (frameArr[frameIndex] != currentFrame): set dirty bit 0x8000000 up parent chain
        //   currentFrame = frameArr[frameIndex]
        void* anim = myState->animatedImageElem;
        if (!anim) return;
        uintptr_t a = (uintptr_t)anim;
        uint64_t f8     = *(uint64_t*)(a + 0xf8);
        uint32_t framePart1 = *(uint32_t*)(a + 0xc0);
        uint32_t framePart2 = *(uint32_t*)(a + 0xc4);
        uint32_t frameLow2  = *(uint32_t*)(a + 0xc8);
        void*    frameArr   = *(void**)  (a + 0xe0);
        uint32_t frameCount = *(uint32_t*)(a + 0xe8);
        uint32_t fps        = *(uint32_t*)(a + 0xf0);
        uint32_t flags      = *(uint32_t*)(a + 0x34);
        if (myState->callCount == 2 || myState->callCount % 5 == 0) {
            printf("[%s] call=%d anim=%p +0xc0=0x%08x +0xc4=0x%08x +0xc8=0x%08x +0xe0=%p +0xe8=%u +0xf0=%u +0xf8=0x%llx flags=0x%08x\n",
                   AB_TAG, myState->callCount, anim, framePart1, framePart2, frameLow2,
                   frameArr, frameCount, fps, (unsigned long long)f8, flags);
        }
    }
#endif
}

#ifdef STANDALONE_EXE

// ============================================================
// LoaderDialog standalone scaffolding
// ============================================================
// Provides a 10-slot vtable + factory + REGISTER_TYPE so that
// SplashDLT::CreateDialog can produce a real LoaderDialog instance
// for both Dialog/ShiningRock.rsc and Dialog/Loading.rsc instead of
// returning a null IBP (which left the splash chain blank).
//
// Vtable layout (exe @ 0x14015f300, 10 slots):
//   +0x00 Slot0 Dtor       — sets vtable, ~DialogInstance, conditional free
//   +0x08 Slot1 TypeName   — exe returns DAT_1401c5bf0 (BaseTypeInfo.name)
//   +0x10 Slot2 TypeId     — exe returns DAT_1401c5be8 (BaseTypeInfo.typeId)
//   +0x18 Slot3 Persist    — thunk to Runtime DialogInstance::Persist
//   +0x20 Slot4 Setup      — REIMPL'd above (LoaderDialog_Slot4)
//   +0x28 Slot5 OnClosed   — thunk to Runtime DialogInstance::OnClosed
//   +0x30 Slot6 Draw       — REIMPL'd above (LoaderDialog_Slot6)
//   +0x38 Slot7 Update     — REIMPL'd above (LoaderDialog_Slot7)
//   +0x40 Slot8 OnElementEvent      — thunk to Runtime DialogInstance::OnElementEvent
//   +0x48 Slot9 OnSerializeSettings — thunk to Runtime DialogInstance::OnSerializeSettings
//
// Layout differs from MainMenuDialog:
//   - 10 slots (LoaderDialog) vs 20 (MainMenuDialog has DialogController interface)
//   - DialogLoaderLoopT places UI at +0x60 and m_dialogExternal at +0x50
//     (vs DialogLoopT's +0x40 / +0x30) — see ctor in MainMenuDialog cf. here.

#include "Object/factory_registration.h"
#include "address_registry.h"
#include "resolve.h"

namespace {

void* GetLoaderDialogVtable();  // forward decl

// Slot 0 — Dtor (~LoaderDialog scalar deleting). Mirrors exe 0x140020c70:
// ~DialogInstance + (flags & 1) ? MemFree.
void* StandaloneStub_Dtor(void* self, uint32_t flags) {
    if (!self) return self;
    *(void**)self = GetLoaderDialogVtable();
    if (fn_DialogInstance_Dtor) fn_DialogInstance_Dtor(self);
    if ((flags & 1) && fn_MemFreeStatic) fn_MemFreeStatic(self);
    return self;
}

// Slot 1 — TypeName. Exe returns DAT_1401c5bf0 (a const char*). Standalone
// returns the registered name. Not called on the splash boot path.
const char* StandaloneStub_Slot1_TypeName(void* /*self*/) { return "LoaderDialog"; }

// Slot 2 — TypeId. Exe returns DAT_1401c5be8 (uint32_t set by Factory::Startup).
uint32_t StandaloneStub_Slot2_TypeId(void* /*self*/) {
    static uint32_t cached = 0;
    if (cached == 0) cached = ::Object::GetTypeIdByName("LoaderDialog");
    return cached;
}

// Generic logging stub for the 4 runtime-thunk slots (3, 5, 8, 9). Logs once
// per slot so we know if any get hit during the splash. None should fire
// during the 2.5s splash chain (no input events, no save, no close).
template <int SlotIdx>
void StandaloneStub_RuntimeThunk(void* self, ...) {
    static bool warned = false;
    if (!warned) {
        warned = true;
        printf("[LoaderDialog::Slot%d_Stub] self=%p (runtime thunk not wired)\n",
               SlotIdx, self);
    }
}

void* g_LoaderDialog_Vtable_Standalone[10] = {
    reinterpret_cast<void*>(&StandaloneStub_Dtor),                  // +0x00 Dtor
    reinterpret_cast<void*>(&StandaloneStub_Slot1_TypeName),        // +0x08 TypeName
    reinterpret_cast<void*>(&StandaloneStub_Slot2_TypeId),          // +0x10 TypeId
    reinterpret_cast<void*>(&StandaloneStub_RuntimeThunk<3>),       // +0x18 Persist
    reinterpret_cast<void*>(&LoaderDialog_Slot4),                   // +0x20 Setup
    reinterpret_cast<void*>(&StandaloneStub_RuntimeThunk<5>),       // +0x28 OnClosed
    reinterpret_cast<void*>(&LoaderDialog_Slot6),                   // +0x30 Draw
    reinterpret_cast<void*>(&LoaderDialog_Slot7),                   // +0x38 Update
    reinterpret_cast<void*>(&StandaloneStub_RuntimeThunk<8>),       // +0x40 OnElementEvent
    reinterpret_cast<void*>(&StandaloneStub_RuntimeThunk<9>),       // +0x48 OnSerializeSettings
};

void* GetLoaderDialogVtable() {
    return reinterpret_cast<void*>(g_LoaderDialog_Vtable_Standalone);
}

// In-place construct LoaderDialog from a SplashDLT loop. Mirrors exe ctor
// LoaderDialog_Ctor3 @ 0x140099070:
//   1. (caller allocs 200 bytes already)
//   2. Build a null parent IBP.
//   3. Call DialogInstance::DialogInstance(this, ui_at_loop+0x60,
//      rsc_at_loop+0x50, parentIBP, 0, 0, 0).
//   4. Patch vtable.
//   5. Zero +0xB8 (loadingImage element ptr) and +0xC0 (rotation float).
//
// Loop layout is DialogLoaderLoopT: m_dialogExternal at +0x50, embedded UI
// at +0x60. (Different from DialogLoopT's +0x30/+0x40.)
void* LoaderDialog_Ctor_FromLoop(void* mem, void* loop) {
    if (!mem) return nullptr;

    // Zero the 0xC8-byte object so any field read through the vtable sees
    // zero rather than uninit'd heap garbage.
    for (size_t off = 0; off < 0xC8; off += 8) {
        *reinterpret_cast<uint64_t*>(reinterpret_cast<uintptr_t>(mem) + off) = 0;
    }

    uintptr_t lp = reinterpret_cast<uintptr_t>(loop);
    void* rsc = loop ? *reinterpret_cast<void**>(lp + 0x50) : nullptr;

    if (loop && rsc && fn_DialogInstance_Ctor && fn_InternalBasePtr_Ctor) {
        void* ui = reinterpret_cast<void*>(lp + 0x60);
        uint8_t parentIBP[8] = {0};
        fn_InternalBasePtr_Ctor(parentIBP, nullptr);
        fn_DialogInstance_Ctor(mem, ui, rsc, parentIBP, nullptr, nullptr, 0);
    } else {
        printf("[LoaderDialog_FromLoop] bare-bones init (loop=%p rsc=%p) — "
               "DialogInstance ctor skipped\n", loop, rsc);
    }

    *reinterpret_cast<void**>(mem) = GetLoaderDialogVtable();
    // +0xB8 (loadingImage*) and +0xC0 (rotation float) already zeroed above.
    return mem;
}

void* LoaderDialog_Factory_Bare(void* /*loop*/) {
    if (!fn_MemAlloc || !g_pMemAllocSingleton) return nullptr;
    void* p = fn_MemAlloc(g_pMemAllocSingleton, 0xC8, 8);
    if (!p) return nullptr;
    return LoaderDialog_Ctor_FromLoop(p, nullptr);
}

void* LoaderDialog_CreateInPlace_Bare(void* mem) {
    return LoaderDialog_Ctor_FromLoop(mem, nullptr);
}

}  // namespace

extern "C" void RegisterLoaderDialogVtable() {
    RegisterExeAddr(0x14015f300, reinterpret_cast<uintptr_t>(g_LoaderDialog_Vtable_Standalone));
}

// Public entry called from SplashDLT::CreateDialog stub — mirrors the
// MainMenuDialog_FromMenuDLT shape: alloc, ctor, wrap result in IBP.
extern "C" void* LoaderDialog_FromSplashDLT(void* loop, void* retInternalBasePtr) {
    if (!fn_MemAlloc || !g_pMemAllocSingleton) {
        if (retInternalBasePtr) {
            *reinterpret_cast<uint64_t*>(retInternalBasePtr) = 0;
            *(reinterpret_cast<uint64_t*>(retInternalBasePtr) + 1) = 0;
        }
        return retInternalBasePtr;
    }
    void* mem = fn_MemAlloc(g_pMemAllocSingleton, 0xC8, 8);
    void* dlg = LoaderDialog_Ctor_FromLoop(mem, loop);
    if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor(retInternalBasePtr, dlg);
    return retInternalBasePtr;
}

// REGISTER_TYPE trampolines (no-arg / single-arg signatures).
extern "C" void* LoaderDialog_StandaloneFactory(void) {
    return LoaderDialog_Factory_Bare(nullptr);
}
extern "C" void* LoaderDialog_StandaloneCreateInPlace(void* memory) {
    return LoaderDialog_CreateInPlace_Bare(memory);
}

REGISTER_TYPE(LoaderDialog, "UI::DialogInstance",
              &LoaderDialog_StandaloneFactory, &LoaderDialog_StandaloneCreateInPlace);

#endif  // STANDALONE_EXE
