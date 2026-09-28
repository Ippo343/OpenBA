// code/Game/Component/consumeproduce_recipe_dump.cpp
//
// One-shot dump of a ConsumeProduceDescription's recipe table.
//
// The tavern stalls at "no materials" and the useful question is not "why is the work
// stuck" but "which recipe is selected and what does that recipe actually eat". Reading
// it out of the live description answers both, and beats reading ToolKit/*.rsc — the
// .pkm mods override data0.pkg, so the authored source on disk is not necessarily what
// is running.
//
// Layout, read off ConsumeProduceUI::Slot4 (0x14005F3D0) and ConsumeProduce::Execute
// (0x1400e7840). The description holds a recipe array at +0x28 with the count at +0x30,
// stride 0x28 (5 qwords), and its UI string table at +0x20. Per recipe entry:
//
//   +0x00  qword  accepted-INPUT array   (stride 0x10, first qword = material description)
//   +0x08  int    input count
//   +0x10  qword  the PRODUCT's description  (Execute treats a stored good matching this
//                                             as "belongs here", same as an input)
//   +0x18  int    materialType -- index into the world material table at
//                                 (*(GameWorld+0xf0))[0x2e], where [t*4] is supply and
//                                 [t*4+1] demand (Execute's depletion test)
//   +0x20  ---    name key, resolved against the string table at desc+0x20
//
// A "material description" object (both the product at +0x10 and each input) carries a
// child list at +0x28 / count +0x30 holding a RawMaterialDescription, whose +0x5c is the
// category flag mask and whose +0x38 is its sprite name. Those flags are the same ones
// HappinessComponent::IsActive ANDs against the tavern's wantMask 0x20000 -- so this dump
// also says, directly, which recipe can ever satisfy the happiness goods arm.

#include "ui/UserInterface.h"
#include "object/TypeInfo.h"
#include "system/String.h"
#include <cstdint>
#include <cstdio>
#include <new>
#include "object/External.h"   // ENDGAME B.3.6 UI callers (K4)
#include "ui/String.h"         // ENDGAME B.3.6 UI callers (K4)
#include "ui/StringTable.h"    // ENDGAME B.3.6 UI callers (K4)
#include "Component/RawMaterialDescription.h"

namespace {

// Pull the RawMaterialDescription out of a material description's child list and report
// its category flags and sprite name. Guarded throughout -- this is a diagnostic, and a
// malformed entry must print "?" rather than take the process down.
// Read a RawMaterialDescription's category flags (+0x5c) and sprite name (+0x38).
bool ReadRawMat(void* c, const char** outName, unsigned* outFlags) {
    if (!c) return false;
    const uintptr_t rd = reinterpret_cast<uintptr_t>(c);
    *outFlags = *reinterpret_cast<unsigned*>(rd + 0x5c);
    const char* nm = reinterpret_cast<const System::String*>(rd + 0x38)->_data;   // B.3.1 bridge
    if (nm && nm[0]) *outName = nm;
    return true;
}

bool IsRawMat(void* c) {
    if (!c) return false;
    const uint32_t tid = reinterpret_cast<Object::Base*>(c)->Type();   // B.3.8 bridge (slot 2)
    if (tid != RawMaterialDescription::TypeId()) return false;
    return ::Object::IsDerivedFromNullSafe(c, RawMaterialDescription::TypeId());
}

// The recipe table mixes two shapes and the first pass only handled one, which is why
// every INPUT printed its flags but came back nameless: a recipe's product at +0x10 is
// a wrapper description whose CHILD list holds the RawMaterialDescription, while an
// entry in the input array is (or can be) the RawMaterialDescription itself. Try the
// object directly first, then fall back to its child list.
void DescribeMaterial(uintptr_t matDesc, const char** outName, unsigned* outFlags) {
    *outName = "?";
    *outFlags = 0;
    if (!matDesc) return;
    void* self = reinterpret_cast<void*>(matDesc);
    if (IsRawMat(self)) { ReadRawMat(self, outName, outFlags); return; }
    const int n = *reinterpret_cast<int*>(matDesc + 0x30);
    const uintptr_t arr = *reinterpret_cast<uintptr_t*>(matDesc + 0x28);
    if (!arr || n <= 0 || n > 4096) return;
    for (int j = 0; j < n; ++j) {
        void* c = *reinterpret_cast<void**>(arr + static_cast<uintptr_t>(j) * 8);
        if (!c) continue;
        const uint32_t tid = reinterpret_cast<Object::Base*>(c)->Type();   // B.3.8 bridge (slot 2)
        if (tid != RawMaterialDescription::TypeId()) continue;
        if (!::Object::IsDerivedFromNullSafe(c, RawMaterialDescription::TypeId())) continue;
        ReadRawMat(c, outName, outFlags);
        return;
    }
}

// The recipe LABEL -- the exact text the panel puts in the Product dropdown.
//
// This is the readable identity, and it is on the recipe ENTRY (+0x20, a key into the
// description's UI string table at desc+0x20), not on the material. Chasing the name
// through the material description gave "?" for every input: the RawMaterialDescription
// reached that way has an EMPTY sprite-name field, even though its +0x5c flags read
// fine. ConsumeProduceUI::Slot4 (0x14005F3D0) builds the combo text this way instead,
// so this reproduces that path exactly.
const char* RecipeLabel(uintptr_t stringTable, uintptr_t nameAddr, char* buf, int cap) {
    buf[0] = 0;
    if (!stringTable) return buf;
    uint8_t uiStr[16] = {};
    ::new (uiStr) UI::String(
        Object::ExternalPtr<UI::StringTable>(reinterpret_cast<UI::StringTable*>(stringTable)),  // B.3.6 bridge
        *reinterpret_cast<const System::String*>(nameAddr));
    System::String sys;
    ::new (&sys) System::String(
        reinterpret_cast<const UI::String*>(uiStr)->operator System::String());  // B.3.6 bridge
    const char* d = sys._data;
    if (d) { int i = 0; for (; i < cap - 1 && d[i]; ++i) buf[i] = d[i]; buf[i] = 0; }
    return buf;
}

}  // namespace

// Dump every recipe once per description. `selected` is the component's live
// consumeResourceId (+0x50) so the chosen row is marked in the output.
extern "C" void ConsumeProduce_DumpRecipes(void* cpDescV, int selected) {
    static void* s_done[8] = {};
    static int   s_n = 0;
    if (!cpDescV) return;
    for (int i = 0; i < s_n; ++i) if (s_done[i] == cpDescV) return;
    if (s_n < 8) s_done[s_n++] = cpDescV;

    const uintptr_t d = reinterpret_cast<uintptr_t>(cpDescV);
    const int count = *reinterpret_cast<int*>(d + 0x30);
    const uintptr_t base = *reinterpret_cast<uintptr_t*>(d + 0x28);
    fprintf(stderr, "[Recipes] desc=%p count=%d selected(+0x50)=%d\n",
            cpDescV, count, selected);
    if (!base || count <= 0 || count > 256) { fflush(stderr); return; }
    const uintptr_t stringTable = *reinterpret_cast<uintptr_t*>(d + 0x20);

    for (int i = 0; i < count; ++i) {
        const uintptr_t e = base + static_cast<uintptr_t>(i) * 0x28;
        const uintptr_t product = *reinterpret_cast<uintptr_t*>(e + 0x10);
        const int matType = *reinterpret_cast<int*>(e + 0x18);
        const uintptr_t inArr = *reinterpret_cast<uintptr_t*>(e + 0x00);
        const int inCount = *reinterpret_cast<int*>(e + 0x08);

        const char* pName; unsigned pFlags;
        DescribeMaterial(product, &pName, &pFlags);
        char label[128];
        RecipeLabel(stringTable, e + 0x20, label, sizeof(label));
        fprintf(stderr, "[Recipes]  [%d]%s label=\"%s\" product=%s flags=0x%x matType=%d inputs=%d\n",
                i, (i == selected ? " <== SELECTED" : ""), label, pName, pFlags,
                matType, inCount);

        if (!inArr || inCount <= 0 || inCount > 256) continue;
        for (int k = 0; k < inCount; ++k) {
            const uintptr_t in = *reinterpret_cast<uintptr_t*>(
                inArr + static_cast<uintptr_t>(k) * 0x10);
            const char* iName; unsigned iFlags;
            DescribeMaterial(in, &iName, &iFlags);
            fprintf(stderr, "[Recipes]        input[%d] = %s flags=0x%x ptr=%p\n",
                    k, iName, iFlags, reinterpret_cast<void*>(in));
        }
    }
    fflush(stderr);
}

// Public single-entry form of the label reader, for the town-stock cross-reference
// probe (standalone/tavern_stock_probe.cpp), which matches recipe inputs against
// stored goods by POINTER and only needs the text to name the row it prints.
extern "C" const char* ConsumeProduce_RecipeLabelAt(void* cpDescV, int index,
                                                    char* buf, int cap) {
    if (cap > 0) buf[0] = 0;
    if (!cpDescV || index < 0 || cap <= 0) return buf;
    const uintptr_t d = reinterpret_cast<uintptr_t>(cpDescV);
    const int count = *reinterpret_cast<int*>(d + 0x30);
    const uintptr_t base = *reinterpret_cast<uintptr_t*>(d + 0x28);
    if (!base || index >= count) return buf;
    const uintptr_t stringTable = *reinterpret_cast<uintptr_t*>(d + 0x20);
    return RecipeLabel(stringTable,
                       base + static_cast<uintptr_t>(index) * 0x28 + 0x20, buf, cap);
}
