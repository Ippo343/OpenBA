// ============================================================================
// runtime/src/ui/ui_operator_delete.cpp — the 60 UI class `operator delete`s
// ============================================================================
//
// `??3<Class>@UI@@SAXPEAX@Z` — MSVC's CLASS-SCOPED `operator delete(void*)`,
// emitted once per UI class that has a virtual destructor. Sixty of them are
// reachable from our call surface (they are the scalar arm of every
// `??_E<Class>@UI@@` deleting destructor, and the failure arm of every
// `CreateNewObject`), and not one was ported.
//
// THEY ARE ONE FUNCTION, SIXTY TIMES. Not "similar" — measured. All sixty are
// 93 bytes; all sixty were read out of the shipped DLL and compared byte for
// byte against each other; every difference is inside a RIP-relative
// displacement, and when those five displacements are resolved to absolute
// addresses all sixty name the SAME five targets:
//
//     0x1801fc718  ?_singleton@MemoryAllocator@System@@0PEAV12@EA
//     0x18014b390  KERNEL32!EnterCriticalSection   (the IAT slot)
//     0x1800c6c00  MemoryAllocator::Free
//     0x18014b388  KERNEL32!LeaveCriticalSection   (the IAT slot)
//
//     ??3X(void* p):
//         if (!p) return;                        // TEST RCX,RCX ; JZ ret
//         cs = *_singleton + 0x640;
//         EnterCriticalSection(cs);
//         Free(*_singleton, p);
//         LeaveCriticalSection(cs);
//
// That is instruction for instruction `?MemFree@MemoryAllocator@System@@SAXPEAX@Z`
// (0x1800c7ad0), which was disassembled beside them for this file and differs
// only in where it sits. So the sixty registrations share ONE body, and that
// body is `rtcoll::MemFree` — the single allocator boundary
// runtime/src/collection/array.cpp owns, so that when the allocator port goes
// default-on there is still exactly one place to repoint. This is the same
// argument, and the same conclusion, as the ten `operator delete` copies
// enumerated in runtime/src/object/deleting_dtor.cpp's banner; two of the sixty
// (`??3GroupDescription@UI@@` 0x1800466f0 and `??3Dialog@UI@@` 0x1800384d0) are
// already NAMED in that file's prose as byte-identical, but neither was ever
// registered — the prose was the whole of the port. It is now a port.
//
// The null test is retail's own and it lives INSIDE 0x1800c7ad0 (`TEST RCX,RCX ;
// JZ ret` is its first instruction), so this file hands `rtcoll::MemFree` a
// possibly-null pointer exactly as retail's callers do. There is no added guard
// here and none is wanted.
//
// WHAT IS NOT HERE: `??2<Class>@UI@@SAPEAX_K@Z`, the matching class `operator
// new`. Only one of those is in this batch (`??2ComboDialog@UI@@`, 0x180074770)
// and it is ported in runtime/src/ui/combodialog.cpp with the rest of that
// class, beside the constructor that is its only caller.
// ============================================================================

#include <cstdint>

#include "../../funcs/rt_port.h"
#include "../../types/system/MemoryAllocator.h"


// ---------------------------------------------------------------------------
// The one body.
// ---------------------------------------------------------------------------
// B3R W0-A0: the allocator's static, called directly (it went through
// rtcoll::MemFree -> rtcoll::DoorFree(), which answers this same function).
extern "C" void RtUi_ClassOperatorDelete(void* p) {
    System::MemoryAllocator::MemFree(p);    // if (p) { Enter; Free; Leave; }
}

namespace {

struct OpDelRow {
    uintptr_t   addr;
    const char* mangled;
};

// Sixty rows, sorted by class name. Each `addr` was checked against the export
// directory of the shipped Runtime-x64.dll (the name is the authority; the
// address is what a by-address call site uses).
const OpDelRow kRows[] = {
    { 0x180036760ull, "??3AnimatedImageDescription@UI@@SAXPEAX@Z" },
    { 0x180036b40ull, "??3AnimatedImageElement@UI@@SAXPEAX@Z" },
    { 0x180040a70ull, "??3BorderDescription@UI@@SAXPEAX@Z" },
    { 0x180041140ull, "??3BorderElement@UI@@SAXPEAX@Z" },
    { 0x180047ec0ull, "??3ButtonDescription@UI@@SAXPEAX@Z" },
    { 0x1800482f0ull, "??3ButtonElement@UI@@SAXPEAX@Z" },
    { 0x1800374e0ull, "??3CharacterSet@UI@@SAXPEAX@Z" },
    { 0x180048710ull, "??3CheckDescription@UI@@SAXPEAX@Z" },
    { 0x180048b10ull, "??3CheckElement@UI@@SAXPEAX@Z" },
    { 0x180037a20ull, "??3ClipRegionDescription@UI@@SAXPEAX@Z" },
    { 0x180038000ull, "??3ClipRegionElement@UI@@SAXPEAX@Z" },
    { 0x180074b70ull, "??3ComboDescription@UI@@SAXPEAX@Z" },
    { 0x1800747f0ull, "??3ComboDialog@UI@@SAXPEAX@Z" },
    { 0x180074fc0ull, "??3ComboElement@UI@@SAXPEAX@Z" },
    { 0x180039960ull, "??3ContainerDescription@UI@@SAXPEAX@Z" },
    { 0x180039d50ull, "??3ContainerElement@UI@@SAXPEAX@Z" },
    { 0x18003b010ull, "??3Cursor@UI@@SAXPEAX@Z" },
    { 0x1800384d0ull, "??3Dialog@UI@@SAXPEAX@Z" },
    { 0x18003b990ull, "??3DocumentTextDescription@UI@@SAXPEAX@Z" },
    { 0x18003bee0ull, "??3DocumentTextElement@UI@@SAXPEAX@Z" },
    { 0x18003f760ull, "??3EditDescription@UI@@SAXPEAX@Z" },
    { 0x18003fb70ull, "??3EditElement@UI@@SAXPEAX@Z" },
    { 0x180031a50ull, "??3Font@UI@@SAXPEAX@Z" },
    { 0x180040120ull, "??3GraphDescription@UI@@SAXPEAX@Z" },
    { 0x1800404f0ull, "??3GraphElement@UI@@SAXPEAX@Z" },
    { 0x1800466f0ull, "??3GroupDescription@UI@@SAXPEAX@Z" },
    { 0x180046c40ull, "??3GroupElement@UI@@SAXPEAX@Z" },
    { 0x180035d60ull, "??3ImageDescription@UI@@SAXPEAX@Z" },
    { 0x180036340ull, "??3ImageElement@UI@@SAXPEAX@Z" },
    { 0x180042440ull, "??3LabelDescription@UI@@SAXPEAX@Z" },
    { 0x180042880ull, "??3LabelElement@UI@@SAXPEAX@Z" },
    { 0x180042dd0ull, "??3LayoutDescription@UI@@SAXPEAX@Z" },
    { 0x180043320ull, "??3LayoutElement@UI@@SAXPEAX@Z" },
    { 0x180044c30ull, "??3ListDescription@UI@@SAXPEAX@Z" },
    { 0x180045020ull, "??3ListElement@UI@@SAXPEAX@Z" },
    { 0x18004a2d0ull, "??3NumberDescription@UI@@SAXPEAX@Z" },
    { 0x180045f20ull, "??3NumberEditDescription@UI@@SAXPEAX@Z" },
    { 0x1800462e0ull, "??3NumberEditElement@UI@@SAXPEAX@Z" },
    { 0x18004a8b0ull, "??3NumberElement@UI@@SAXPEAX@Z" },
    { 0x180046f70ull, "??3PageDescription@UI@@SAXPEAX@Z" },
    { 0x180047300ull, "??3PageElement@UI@@SAXPEAX@Z" },
    { 0x180077800ull, "??3ProgressDescription@UI@@SAXPEAX@Z" },
    { 0x180077dd0ull, "??3ProgressElement@UI@@SAXPEAX@Z" },
    { 0x180047700ull, "??3RadioDescription@UI@@SAXPEAX@Z" },
    { 0x180047a90ull, "??3RadioElement@UI@@SAXPEAX@Z" },
    { 0x1800441c0ull, "??3RibbonDescription@UI@@SAXPEAX@Z" },
    { 0x180044770ull, "??3RibbonElement@UI@@SAXPEAX@Z" },
    { 0x18003a1b0ull, "??3ScrollDescription@UI@@SAXPEAX@Z" },
    { 0x18003a8e0ull, "??3ScrollElement@UI@@SAXPEAX@Z" },
    { 0x1800497e0ull, "??3SliderDescription@UI@@SAXPEAX@Z" },
    { 0x180049db0ull, "??3SliderElement@UI@@SAXPEAX@Z" },
    { 0x18004adf0ull, "??3SpinnerDescription@UI@@SAXPEAX@Z" },
    { 0x18004b1a0ull, "??3SpinnerElement@UI@@SAXPEAX@Z" },
    { 0x18003c8a0ull, "??3SpriteCapture@UI@@SAXPEAX@Z" },
    { 0x18004c280ull, "??3TabDescription@UI@@SAXPEAX@Z" },
    { 0x18004c790ull, "??3TabElement@UI@@SAXPEAX@Z" },
    { 0x180078f60ull, "??3TableDescription@UI@@SAXPEAX@Z" },
    { 0x180079360ull, "??3TableElement@UI@@SAXPEAX@Z" },
    { 0x180041b60ull, "??3TextDescription@UI@@SAXPEAX@Z" },
    { 0x180041f80ull, "??3TextElement@UI@@SAXPEAX@Z" },
};

constexpr int kRowCount = static_cast<int>(sizeof(kRows) / sizeof(kRows[0]));
static_assert(kRowCount == 60, "sixty class operator deletes, one per UI class");

}  // namespace

int RtUiClassOperatorDeleteRegister() {
    int n = 0;
    for (const OpDelRow& r : kRows) {
        RegisterRuntimeExport(r.mangled, (void*)&RtUi_ClassOperatorDelete);
        RegisterRuntimeAddr(r.addr, (void*)&RtUi_ClassOperatorDelete);
        ++n;
    }
    return n;
}
