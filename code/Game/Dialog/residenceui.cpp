// engine/src/ui/residenceui.cpp
// Clean reimplementation of ResidenceUI game logic.
//
// ResidenceUI (0x40 bytes) — residence info panel.
// Extends DialogController (0x30 base).
// +0x30 = dynamic entry array (0x30-byte entries), +0x38 = entry count.
//
// Each entry (0x30 bytes):
//   +0x00 = imageMale element ptr
//   +0x08 = imageFemale element ptr
//   +0x10 = textName element ptr
//   +0x18 = textAge element ptr
//   +0x20 = textProfession element ptr
//   +0x28 = buttonGoto element ptr
//
// Decompiled from Application-x64.exe.

#include "Dialog/ui_panels.h"
#include "engine_funcs.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>

// ResidenceUI::Slot7 is the per-frame roster refresh (driven via the +0x40 W_OnUpdate
// hack in standalone). Its _DEBUG bring-up logs used to fflush an unbuffered log once
// per resident EVERY frame — the dominant cost that tanked the framerate once a second
// entity panel could stay pinned open. Gate them behind BANISHED_DIALOG_TRACE (cached
// once). Kept inside _DEBUG so release builds are unchanged.
#ifdef _DEBUG
static bool ResDbgTrace() {
    static const bool s_on = std::getenv("BANISHED_DIALOG_TRACE") != nullptr;
    return s_on;
}
#endif

// Range check for pointers a standalone citizen may leave uninitialized. A rostered
// CitizenComponent in standalone can have a garbage description cache (+0x288) or an
// unset name String; the raw derefs the faithful retail port does would fault (MinGW
// has no __try/__except to catch it). Same idea as CitUI_OkPtr in citizenui.cpp.
static inline bool RUI_OkPtr(uintptr_t p) {
    return p >= 0x10000 && p <= 0x00007FFFFFFFFFFFULL;
}

// ============================================================
// File-local helper: ResidenceUI_ArrayPush_Impl (FUN_140021020)
// ============================================================
// Resizes dynamic array to newCount entries (stride 0x30 bytes, no String fields).
// Entry layout: 6 × void* (imageMale, imageFemale, textName, textAge, textProfession, buttonGoto).
// arrayBase layout: [data* 8][count int32][capacity int32]
// Original: 0x140021020.
static void ResidenceUI_ArrayPush_Impl(void* arrayBase, int newCount, void* entry6) {
    uintptr_t* dataPtr  = (uintptr_t*)arrayBase;
    int* countPtr       = (int*)((uintptr_t)arrayBase + 8);
    int* capacityPtr    = (int*)((uintptr_t)arrayBase + 0xc);
    int oldCount        = *countPtr;

    if (newCount == oldCount) return;

    void* oldData = (void*)*dataPtr;
    *countPtr = newCount;

    if (*capacityPtr < newCount) {
        *capacityPtr = newCount;
        if (oldCount != 0 && newCount == oldCount + 1) {
            *capacityPtr = newCount * 3;
        }
        void* newData = fn_MemAlloc(g_pMemAllocSingleton, (int64_t)*capacityPtr * 0x30, 8);
        *dataPtr = (uintptr_t)newData;
    }

    int startCopy = (oldData == (void*)*dataPtr) ? oldCount : 0;
    for (int i = startCopy; i < newCount; i++) {
        uint64_t* dst = (uint64_t*)(*dataPtr + (uintptr_t)i * 0x30);
        if (dst != nullptr) {
            uint64_t* src = (i < oldCount)
                ? (uint64_t*)((uintptr_t)oldData + (uintptr_t)i * 0x30)
                : (uint64_t*)entry6;
            dst[0] = src[0]; dst[1] = src[1]; dst[2] = src[2];
            dst[3] = src[3]; dst[4] = src[4]; dst[5] = src[5];
        }
    }

    if (oldData != (void*)*dataPtr) {
        fn_MemFreeStatic(oldData);
    }
}

// Helper: find element by name in parent element, returns raw pointer
// Original pattern: FindElement → CheckPtr → deref
static void* FindChildElem(void* parentElem, const char* name) {
    uint8_t nameStr[8], retBuf[16];
    fn_String_Ctor_CStr(nameStr, name);
    fn_FindElement(parentElem, retBuf, nameStr);
    fn_InternalBasePtr_CheckPtr(retBuf);  // resolve IBP before deref
    void* result = *(void**)retBuf;
    fn_String_Dtor(nameStr);
    return result;
}

// Helper: find element by name, wrap as InternalBasePtr
// Original pattern: FindElement → CheckPtr → deref → IsDerivedFrom → IBP_Ctor
static uint64_t FindElemAsBasePtr(void* parentElem, const char* name) {
    void* elem = FindChildElem(parentElem, name);
    uint8_t bpBuf[8];
    fn_InternalBasePtr_Ctor(bpBuf, elem);
    return *(uint64_t*)bpBuf;
}

// ============================================================
// ResidenceUI::Slot4 (vtable +0x20) — Setup resident table
// ============================================================
// Finds "residenceTable" (or "residenceLongTable" fallback), iterates
// rows to find child elements (buttonGoto, textName, imageMale, imageFemale,
// textAge, textProfession), stores in dynamic array at +0x30.
// Sets element IDs on buttonGoto: 300 + rowIndex.
//
// Original: 0x1400B5850.

void ResidenceUI_Slot4(ResidenceUI* self) {
    uintptr_t s = (uintptr_t)self;
    void* dialogElem = *(void**)(s + 0x20);

    // Try "residenceTable" first, fall back to "residenceLongTable"
    void* tableRaw = FindChildElem(dialogElem, "residenceTable");
    void* tableElem;

    if (!tableRaw) {
        // Fallback: find "residenceLongTable"
        void* longTable = FindChildElem(dialogElem, "residenceLongTable");
        uint8_t basePtrBuf[8];
        fn_InternalBasePtr_Ctor(basePtrBuf, longTable);
        tableElem = *(void**)basePtrBuf;
    } else {
        uint8_t basePtrBuf[8];
        fn_InternalBasePtr_Ctor(basePtrBuf, tableRaw);
        tableElem = *(void**)basePtrBuf;
    }

    // Get row count and column count from table element
    int rowCount = *(int*)((uintptr_t)tableElem + 0xC0);
    int colCount = *(int*)((uintptr_t)tableElem + 0xC4);

#ifdef _DEBUG
    printf("[ResidenceUI::Slot4] tableElem=%p rowCount=%d colCount=%d\n",
           tableElem, rowCount, colCount);
    fflush(stdout);
#endif

    for (int i = 0; i < rowCount; i++) {
        // 0x30-byte entry: imageMale, imageFemale, textName, textAge, textProfession, buttonGoto
        uint64_t entry[6] = {0, 0, 0, 0, 0, 0};
        uint8_t childBuf[8];

        // Original uses flat table indices: row * colCount + column
        // Column layout: 0=buttonGoto, 1=textName, 2=imageMale/Female, 3=textAge, 4=textProfession
        int base = i * colCount;

        // Col 0: buttonGoto
        fn_InternalBasePtr_CheckPtr(&tableElem);
        fn_Element_Child(tableElem, childBuf, base + 0);
        fn_InternalBasePtr_CheckPtr(childBuf);
        entry[5] = FindElemAsBasePtr(*(void**)childBuf, "buttonGoto");
        fn_InternalBasePtr_CheckPtr(&entry[5]);
        if (entry[5]) fn_Element_SetElementId((void*)entry[5], 300 + i);

        // Col 1: textName
        fn_InternalBasePtr_CheckPtr(&tableElem);
        fn_Element_Child(tableElem, childBuf, base + 1);
        fn_InternalBasePtr_CheckPtr(childBuf);
        entry[2] = FindElemAsBasePtr(*(void**)childBuf, "textName");

        // Col 2: imageMale + imageFemale (container cell)
        fn_InternalBasePtr_CheckPtr(&tableElem);
        fn_Element_Child(tableElem, childBuf, base + 2);
        fn_InternalBasePtr_CheckPtr(childBuf);
        void* iconCell = *(void**)childBuf;
        entry[0] = FindElemAsBasePtr(iconCell, "imageMale");

        fn_InternalBasePtr_CheckPtr(&tableElem);
        fn_Element_Child(tableElem, childBuf, base + 2);
        fn_InternalBasePtr_CheckPtr(childBuf);
        entry[1] = FindElemAsBasePtr(*(void**)childBuf, "imageFemale");

        // Col 3: textAge
        fn_InternalBasePtr_CheckPtr(&tableElem);
        fn_Element_Child(tableElem, childBuf, base + 3);
        fn_InternalBasePtr_CheckPtr(childBuf);
        entry[3] = FindElemAsBasePtr(*(void**)childBuf, "textAge");

        // Col 4: textProfession
        fn_InternalBasePtr_CheckPtr(&tableElem);
        fn_Element_Child(tableElem, childBuf, base + 4);
        fn_InternalBasePtr_CheckPtr(childBuf);
        entry[4] = FindElemAsBasePtr(*(void**)childBuf, "textProfession");

#ifdef _DEBUG
        if (i < 3) {
            printf("[ResidenceUI::Slot4]   row[%d] base=%d: imgM=%llx imgF=%llx txtN=%llx txtA=%llx txtP=%llx btn=%llx\n",
                   i, base, entry[0], entry[1], entry[2], entry[3], entry[4], entry[5]);
            fflush(stdout);
        }
#endif

        // Push entry into dynamic array at +0x30
        // FUN_140021020: array resize/push with 0x30-byte elements
        ResidenceUI_ArrayPush_Impl((void*)(s + 0x30), *(int*)(s + 0x38) + 1, entry);
    }
}

// ============================================================
// ResidenceUI::Slot9 (vtable +0x48) — Button dispatch
// ============================================================
// Handles resident row button clicks. Each row has element ID = 300 + index.
// Clicking navigates camera to that resident citizen.
//
// Original: 0x1400B5820.

// ============================================================
// ResidenceUI::Slot7 (vtable +0x38) — Update resident display
// ============================================================
// Iterates storage component rooms to display resident info per row.
// For each occupied slot: show gender icons, set name/age/profession text.
// For unused rows beyond active count: hide and clear.
//
// Storage layout: +0x50 = room array ptr (0x10-byte entries: ptr, count),
//                 +0x58 = room count.
// Each resident ptr -> citizen entity with name at +0x28, gender at +0x38,
// age at +0x3c, profession at +0x1f0, description at +0x288, death at +0x208.
//
// Original: 0x1400B51F0.

void ResidenceUI_Slot7(ResidenceUI* self) {
    uintptr_t s = (uintptr_t)self;

    // Get entity from data source +0x28
    void* dataSource = *(void**)(s + 0x28);
    bool isEntity = fn_IsDerivedFrom(dataSource, *g_TypeId_ComponentEntity);
    void* entity = isEntity ? dataSource : nullptr;
    uint8_t entityBuf[8];
    fn_InternalBasePtr_Ctor(entityBuf, entity);
    uintptr_t entityPtr = *(uintptr_t*)entityBuf;

    // Find ResidenceComponent — original uses type ID at 0x1401c7cf0 (ResidenceComponent),
    // NOT StorageComponent (0x1401c7e78). Different component with room data at +0x50/+0x58.
    uintptr_t storageComp = 0;
    Entity_FindComponentByExeTypeId((void*)entityPtr, &storageComp, g_TypeId_ResidenceComponent);
    fn_InternalBasePtr_CheckPtr(&storageComp);

#ifdef _DEBUG
    if (ResDbgTrace())
    printf("[ResidenceUI::Slot7] self=%p entity=%p resComp=0x%llx\n",
           (void*)self, entity, (unsigned long long)storageComp);
#endif

    float ageMul = *g_pAgeMultiplier;  // 5.0f
    int totalResidents = 0;
    int rowIdx = 0;

    if (!storageComp) return;

    // Outer loop: iterate room groups (storageComp+0x58 = group count)
    int groupCount = *(int*)(storageComp + 0x58);
#ifdef _DEBUG
    if (ResDbgTrace())
    printf("[ResidenceUI::Slot7] groupCount=%d totalRowSlots=%d\n",
           groupCount, *(int*)(s + 0x38));
#endif

    if (groupCount > 0) {
        uintptr_t groupOffset = 0;

        for (int g = 0; g < groupCount; g++) {
            uintptr_t groupArrayBase = *(uintptr_t*)(storageComp + 0x50);
            int residentCount = *(int*)(groupArrayBase + 8 + groupOffset);

            if (residentCount > 0) {
                totalResidents += residentCount;
                int endRow = rowIdx + residentCount;
#ifdef _DEBUG
                if (ResDbgTrace())
                printf("[ResidenceUI::Slot7]   group[%d]: residentCount=%d rowIdx=%d\n",
                       g, residentCount, rowIdx);
#endif

                for (int r = 0; r < residentCount; r++) {
                    // [DECLARED DEVIATION — ROW-ARRAY BOUND] Retail indexes the row array at
                    // controller+0x30 by resident index with NO bound (0x1400b51f0: the populate
                    // walk is unbounded; only the blank-out loop afterwards is bounded by
                    // controller+0x38). Its invariant is that a household can never hold more
                    // residents than the dialog table has rows.
                    //
                    // [CORRECTION 2026-07-26] The earlier note here blamed StartTown's housing
                    // pass for "rostering a whole family into slot[0] regardless of the
                    // ResidenceDescription capacity". That is not a deviation: desc+0x20 is the
                    // number of HOUSEHOLD SLOTS, not an occupant cap. ResidenceComponent::Serialize
                    // resizes +0x50 to desc+0x20 and stores it in +0x58; every reader
                    // (sub_0e70, this slot) then walks +0x58 entries at stride 0x10, each entry
                    // being its own {data,count,cap} list of citizens. A wood house reads
                    // desc+0x20 == 1 at runtime, and retail's own home finder PREFERS
                    // `*(comp+0x58) == 1` and only falls back to `1 < *(comp+0x58)` — which would
                    // be inverted nonsense if the field were an occupant count. One family in
                    // slot[0] is exactly what retail's housing solver produces.
                    //
                    // So the guard is not covering for a start-town bug; it covers retail's own
                    // unbounded walk, which a household grown past the table's row count (by
                    // BIRTHS) would run off the end of, handing garbage to
                    // UI::Element::SetFlagsOnOff. It has never been observed to fire — and note
                    // the autostart soak never opens a residence panel, so "never fired" is not
                    // yet evidence either way about the ResidenceUI_Slot7+0x295 crash of
                    // 2026-07-25; the mass-death teardown remains an equally good explanation.
                    // Stop at the last real row instead of walking off the end, and say so.
                    const int rowCapacity = *(int*)(s + 0x38);
                    if (rowIdx >= rowCapacity) {
                        static int s_warned = 0;
                        if (s_warned < 4) { ++s_warned;
                            fprintf(stderr, "[ResidenceUI] GUARD: residence %p has %d resident(s) "
                                    "in group %d but the dialog table only has %d row(s) — "
                                    "truncating (SA over-filled house)\n",
                                    (void*)storageComp, residentCount, g, rowCapacity);
                            fflush(stderr);
                        }
                        break;
                    }
                    // Original CheckPtr's the component IBP every iteration
                    fn_InternalBasePtr_CheckPtr(&storageComp);

                    // Get citizen ptr from resident list + CheckPtr
                    uintptr_t residentListPtr = *(uintptr_t*)(*(uintptr_t*)(storageComp + 0x50) + groupOffset);
                    uintptr_t citizen = *(uintptr_t*)(residentListPtr + r * 8);
                    fn_InternalBasePtr_CheckPtr(&citizen);

#ifdef _DEBUG
                    if (ResDbgTrace()) {
                        uintptr_t rowBase = *(uintptr_t*)(s + 0x30) + rowIdx * 0x30;
                        void* imgM = *(void**)(rowBase + 0x00);
                        void* imgF = *(void**)(rowBase + 0x08);
                        void* txtN = *(void**)(rowBase + 0x10);
                        printf("[ResidenceUI::Slot7]     r[%d] citizen=%p imgM=%p imgF=%p txtN=%p\n",
                               r, (void*)citizen, imgM, imgF, txtN);
                        fflush(stdout);
                    }
#endif

                    // Get row entry: 0x30-byte entries at self+0x30
                    uintptr_t rowBase = *(uintptr_t*)(s + 0x30) + rowIdx * 0x30;

                    // Check gender: citizen+0x38 != 0 means male
                    fn_InternalBasePtr_CheckPtr(&citizen);
                    bool isMale = (*(int*)(citizen + 0x38) != 0);

                    // Show/hide gender icons — CheckPtr each element IBP in row array
                    fn_InternalBasePtr_CheckPtr((void*)(rowBase + 0x08));
                    void* imageFemale = *(void**)(rowBase + 0x08);
                    if (imageFemale) {
                        if (isMale) {
                            fn_Element_SetFlagsOnOff(imageFemale, 1, 0);
                        } else {
                            fn_Element_SetFlagsOnOff(imageFemale, 0, 1);
                        }
                    }
                    fn_InternalBasePtr_CheckPtr((void*)rowBase);
                    void* imageMale = *(void**)rowBase;
                    if (imageMale) fn_Element_SetFlagsOnOff(imageMale, (uint32_t)!isMale, (uint32_t)isMale);

                    // Set name text: citizen+0x28 is the name String
                    fn_InternalBasePtr_CheckPtr(&citizen);
                    fn_InternalBasePtr_CheckPtr((void*)(rowBase + 0x10));
                    void* textName = *(void**)(rowBase + 0x10);
                    // SA safety: the name String buffer (citizen+0x28) may be unset on a
                    // standalone citizen; SetParameter refcounts the source buffer and
                    // would fault on garbage. 0 (empty String) is fine.
                    uintptr_t nameBuf = *(uintptr_t*)(citizen + 0x28);
                    if (textName && (nameBuf == 0 || RUI_OkPtr(nameBuf)))
                        fn_TextElement_SetParameter(textName, 0, (void*)(citizen + 0x28));

                    // Set age text: (int)(age * 5.0f) formatted as "%i"
                    fn_InternalBasePtr_CheckPtr(&citizen);
                    uint8_t ageStr[8];
                    int ageYears = (int)(*(float*)(citizen + 0x3c) * ageMul);
                    fn_String_Format(ageStr, "%i", (uint64_t)(uint32_t)ageYears);
                    fn_InternalBasePtr_CheckPtr((void*)(rowBase + 0x18));
                    void* textAge = *(void**)(rowBase + 0x18);
                    if (textAge) fn_TextElement_SetParameter(textAge, 0, ageStr);
                    fn_String_Dtor(ageStr);

                    // Determine profession text. Faithful to retail 0x1400B51F0:
                    //   if (age < desc.childThreshold(+0x44) || job(+0x208) != 0):
                    //       job == 0 -> ProfessionList+0x40 (child/student label)
                    //       job != 0 -> ProfessionList+0x48 (employed label)
                    //   else (adult, no job) -> the citizen's own profession (+0x1f0)
                    // (The prior port had these branches scrambled and read +0x208 as a
                    //  "dead" flag; corrected here.) Every deref is range-guarded so a
                    // standalone citizen with a garbage description cache can't fault.
                    fn_InternalBasePtr_CheckPtr((void*)(rowBase + 0x20));
                    void* textProfession = *(void**)(rowBase + 0x20);

                    fn_InternalBasePtr_CheckPtr(&citizen);
                    float age = *(float*)(citizen + 0x3c);
                    uintptr_t jobEntity = *(uintptr_t*)(citizen + 0x208);

                    // Child-age threshold from the citizen description (+0x288 cache;
                    // fall back to the component's own description at +0x18 when the
                    // cache is garbage in standalone).
                    uintptr_t descPtr = *(uintptr_t*)(citizen + 0x288);
                    if (!RUI_OkPtr(descPtr)) descPtr = *(uintptr_t*)(citizen + 0x18);
                    float oldAgeThreshold = RUI_OkPtr(descPtr)
                                          ? *(float*)(descPtr + 0x44) : 1e30f;

                    // UI::String ctor needs 3 args: (retbuf, externalPtr at +0x20,
                    // stringName at +0x30). fn_UIString_Ctor passes all three; the
                    // Ghidra decompile drops R8 (the +0x30 name) on the call.
                    uint64_t uiStr[2];
                    bool builtProf = false;
                    if (age < oldAgeThreshold || jobEntity != 0) {
                        // Child (no job) -> ProfList+0x40; employed -> ProfList+0x48.
                        void* world = g_pGameWorldSingleton ? *g_pGameWorldSingleton : nullptr;
                        uintptr_t profList = world ? *(uintptr_t*)((uintptr_t)world + 0xB0) : 0;
                        if (RUI_OkPtr(profList)) {
                            uintptr_t entry = *(uintptr_t*)(profList + (jobEntity == 0 ? 0x40 : 0x48));
                            if (RUI_OkPtr(entry)) {
                                fn_UIString_Ctor(uiStr, *(uint64_t*)(entry + 0x20), (void*)(entry + 0x30));
                                builtProf = true;
                            }
                        }
                    } else {
                        // Adult laborer: the citizen's own profession object (+0x1f0).
                        uintptr_t profDesc = *(uintptr_t*)(citizen + 0x1f0);
                        if (RUI_OkPtr(profDesc)) {
                            fn_UIString_Ctor(uiStr, *(uint64_t*)(profDesc + 0x20), (void*)(profDesc + 0x30));
                            builtProf = true;
                        }
                    }
                    if (textProfession && builtProf) fn_TextElement_SetText(textProfession, uiStr);

                    // Activate buttonGoto: vtable[8](this)
                    fn_InternalBasePtr_CheckPtr((void*)(rowBase + 0x28));
                    void* buttonGoto = *(void**)(rowBase + 0x28);
                    if (buttonGoto) {
                        void** vtbl = *(void***)buttonGoto;
                        typedef void (*VCall_Activate_t)(void*);
                        ((VCall_Activate_t)vtbl[8])(buttonGoto);
                    }

                    rowIdx++;
                }
            }
            groupOffset += 0x10;
        }
    }

    // Hide unused rows beyond active resident count
    if (totalResidents < *(int*)(s + 0x38)) {
        int i = totalResidents;
        do {
            uintptr_t rowBase = *(uintptr_t*)(s + 0x30) + (uintptr_t)i * 0x30;

            // Hide both gender icons
            void* imageMale = *(void**)(rowBase + 0x00);
            if (imageMale) fn_Element_SetFlagsOnOff(imageMale, 1, 0);
            void* imageFemale = *(void**)(rowBase + 0x08);
            if (imageFemale) fn_Element_SetFlagsOnOff(imageFemale, 1, 0);

            // Clear name text
            uint8_t emptyStr1[8];
            fn_String_Ctor_CStr(emptyStr1, "");
            void* textName = *(void**)(rowBase + 0x10);
            if (textName) fn_TextElement_SetParameter(textName, 0, emptyStr1);
            fn_String_Dtor(emptyStr1);

            // Clear age text
            uint8_t emptyStr2[8];
            fn_String_Ctor_CStr(emptyStr2, "");
            void* textAge = *(void**)(rowBase + 0x18);
            if (textAge) fn_TextElement_SetParameter(textAge, 0, emptyStr2);
            fn_String_Dtor(emptyStr2);

            // Reset profession text: fn_UIString_Ctor(retbuf, externalPtr from +0xD8, empty string)
            void* textProfession = *(void**)(rowBase + 0x20);
            if (textProfession) {
                uint8_t emptyStr3[8];
                fn_String_Ctor_CStr(emptyStr3, "");
                uint64_t origUIStr[2];
                fn_UIString_Ctor(origUIStr,
                    *(uint64_t*)((uintptr_t)textProfession + 0xD8), emptyStr3);
                fn_TextElement_SetText(textProfession, origUIStr);
                fn_String_Dtor(emptyStr3);
            }

            // Deactivate buttonGoto: vtable[8](this, 0)
            void* buttonGoto = *(void**)(rowBase + 0x28);
            if (buttonGoto) {
                void** vtbl = *(void***)buttonGoto;
                typedef void (*VCall_Deactivate_t)(void*, int);
                ((VCall_Deactivate_t)vtbl[8])(buttonGoto, 0);
            }

            i++;
        } while (i < *(int*)(s + 0x38));
    }
}

// ============================================================
// ResidenceUI_GoToCitizen — navigate camera to resident citizen
// ============================================================
// Iterates storage component rooms to find the citizen at the given
// flat index, then centers the camera on that citizen entity.
//
// Storage layout: +0x50 = room array ptr (0x10-byte entries: ptr, count),
//                 +0x58 = room count.
// Each room entry: [citizenListPtr 8][count 4][pad 4].
// citizenListPtr -> array of citizen entity pointers (8 bytes each).
// Citizen entity +0x10 = InternalBasePtr self-reference.
//
// Original: 0x1400B56A0.

static void ResidenceUI_GoToCitizen(ResidenceUI* self, int citizenIndex) {
    uintptr_t s = (uintptr_t)self;

    // Get entity from data source +0x28
    void* dataSource = *(void**)(s + 0x28);
    bool isEntity = fn_IsDerivedFrom(dataSource, *g_TypeId_ComponentEntity);
    void* entity = isEntity ? dataSource : nullptr;
    uint8_t entityBuf[8];
    fn_InternalBasePtr_Ctor(entityBuf, entity);
    uintptr_t entityPtr = *(uintptr_t*)entityBuf;

    // Find StorageComponent on the residence entity
    uintptr_t storageComp = 0;
    fn_FindComponent_Storage((void*)entityPtr, &storageComp);

    // Iterate storage room groups to find citizen at flat index
    int flatIdx = 0;
    int groupCount = *(int*)(storageComp + 0x58);

    for (int g = 0; g < groupCount; g++) {
        uintptr_t groupArrayBase = *(uintptr_t*)(storageComp + 0x50);
        int residentCount = *(int*)(groupArrayBase + 8 + (uintptr_t)g * 0x10);

        for (int r = 0; r < residentCount; r++) {
            if (flatIdx == citizenIndex) {
                // Found the target citizen — re-read storageComp after CheckPtr
                fn_InternalBasePtr_CheckPtr(&storageComp);
                uintptr_t citizenPtr = *(uintptr_t*)(
                    *(uintptr_t*)(*(uintptr_t*)(storageComp + 0x50) + (uintptr_t)g * 0x10)
                    + (uintptr_t)r * 8);

                fn_InternalBasePtr_CheckPtr(&citizenPtr);

                // Get citizen's InternalBasePtr self-reference at +0x10
                uint64_t citizenBP = *(uint64_t*)(citizenPtr + 0x10);
                // Get parent dialog ptr from self+0x18
                uint64_t parentDialog = *(uint64_t*)(s + 0x18);

                // Center camera on this citizen via toolController (gameWorld+0x120)
                void* toolController = *(void**)((uintptr_t)*g_pGameWorldSingleton + 0x120);
                Toolbar_CenterOnEntity_Impl(toolController, &parentDialog, &citizenBP);
                return;
            }
            flatIdx++;
        }
    }
}

void ResidenceUI_Slot9(ResidenceUI* self, void* param2, int msg, int id) {
    (void)param2;
    uintptr_t s = (uintptr_t)self;

    if (msg != 0x12) return;

    int count = *(int*)(s + 0x38);
    if (id >= 300 && id < 300 + count) {
        ResidenceUI_GoToCitizen(self, id - 300);
    }
}

// ============================================================
// STANDALONE promotion — real ResidenceUI controller vtable
// ============================================================
// A house's info panel lists its residents (name / gender / age / profession, one
// row per occupant) via a ResidenceUI controller bound to the panel's residenceTable.
// In standalone this type was a StubNop (all slots no-op, see ui_dialog_stub_factories
// .cpp — that stub is REMOVED there to avoid a duplicate REGISTER_TYPE), so Slot4
// (build the row table) and Slot7 (fill rows from the ResidenceComponent roster at
// +0x50/+0x58) never dispatched and the list stayed empty even though founding rosters
// the residents. This installs a real per-class vtable, following the StorageUI
// promotion pattern.
//
// Vtable offsets (verified from the exe ResidenceUI vtable @ 0x14015f6d0):
//   +0x00 (idx0) ScalarDtor (0x140020fc0)  +0x10 (idx2) GetTypeId (0x140020f40)
//   +0x20 (idx4) Slot4  (0x1400b5850) — build+cache the per-row element array (+0x30/+0x38)
//   +0x38 (idx7) Slot7  (0x1400b51f0) — populate rows from the ResidenceComponent roster
//   +0x40 (idx8)        (0x14003a96c)  — base DialogController::OnUpdate (per-frame). We
//               drive the refresh here: build once (until +0x38 row count is set), then
//               re-populate every frame so the roster stays live as residents change.
//   +0x48 (idx9) Slot9  (0x1400b5820) — resident-row click dispatch (id 300+index)
#include "Object/factory_registration.h"
#include <cstddef>
#include <cstdlib>

namespace residenceui_sa {

constexpr int kSlots = 48;
static void* g_vt[kSlots];

// Standalone-local fallbacks for exe-.rdata globals that resolve NULL in standalone.
static uint32_t s_resTid = 0;       // ResidenceComponent type-id
static uint32_t s_ceTid  = 0;       // ComponentEntity type-id
static float    s_ageMul = 1.0f;    // matches citizenui.cpp's SA age-multiplier fallback

void StubNop(void*, ...) {}

uint32_t GetTypeId(void*) {
    static uint32_t c = 0;
    if (!c) c = ::Object::GetTypeIdByName("ResidenceUI");
    return c;
}

void* Dtor(void* self, uint32_t flags) {
    if (!self) return self;
    *reinterpret_cast<void**>(self) = g_vt;
    // Free the cached per-row element array at +0x30 (mirrors the exe ScalarDtor).
    uintptr_t s = reinterpret_cast<uintptr_t>(self);
    void* rows = *reinterpret_cast<void**>(s + 0x30);
    if (rows && fn_MemFreeStatic) fn_MemFreeStatic(rows);
    *reinterpret_cast<uint64_t*>(s + 0x30) = 0;
    *reinterpret_cast<uint64_t*>(s + 0x38) = 0;
    if ((flags & 1) && fn_MemFreeStatic) fn_MemFreeStatic(self);
    return self;
}

void EnsureGlobals() {
    if (!g_TypeId_ComponentEntity) {
        s_ceTid = ::Object::ResolveTypeIdByNameLazy("ComponentEntity");
        if (s_ceTid) g_TypeId_ComponentEntity = &s_ceTid;
    }
    if (!g_TypeId_ResidenceComponent) {
        s_resTid = ::Object::ResolveTypeIdByNameLazy("ResidenceComponent");
        if (s_resTid) g_TypeId_ResidenceComponent = &s_resTid;
    }
    // g_pAgeMultiplier (0x140172198) is zero-fill in standalone; point it at the same
    // 1.0 fallback citizenui.cpp uses so resident ages match the citizen dialog.
    if (!g_pAgeMultiplier) g_pAgeMultiplier = &s_ageMul;
}

bool Ready() {
    return g_TypeId_ComponentEntity && *g_TypeId_ComponentEntity
        && g_TypeId_ResidenceComponent && *g_TypeId_ResidenceComponent
        && g_pAgeMultiplier
        && g_pGameWorldSingleton && *g_pGameWorldSingleton;
}

// idx4 (+0x20): build the per-row element array. Runs once (Slot4 appends rows, so it
// must not run twice — the OnUpdate self-heal below guards on +0x38 == 0).
void W_Slot4(void* self) {
    EnsureGlobals();
    if (!self || !Ready()) return;
    ResidenceUI_Slot4(reinterpret_cast<ResidenceUI*>(self));
}

// SA-guard: the dialog's data-source entity ref (+0x28) is nulled by entity
// destruction (verified live 2026-07-20: house demolished via the upgrade path
// with its panel open → IsDerivedFrom(null) crashed in Runtime). Bail when the
// entity is gone — the panel just stops refreshing.
static bool DataSourceAlive(void* self) {
    void* e = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(self) + 0x28);
    if (!e) return false;
    // Marked-for-removal (entity+0x40 bit 0x400): the entity survives until the
    // EntityList drain, but its components/roster may already be torn down —
    // reading them crashed live 2026-07-20 (garbage into IsDerivedFrom).
    if ((*reinterpret_cast<uint32_t*>(reinterpret_cast<uintptr_t>(e) + 0x40) >> 10) & 1)
        return false;
    return true;
}

// idx7 (+0x38): populate the rows from the residence roster.
void W_Slot7(void* self) {
    EnsureGlobals();
    if (!self || !Ready() || !DataSourceAlive(self)) return;
    if (std::getenv("BANISHED_DIALOG_TRACE")) {
        static int n = 0;
        if (n++ < 5) {
            uintptr_t s = reinterpret_cast<uintptr_t>(self);
            fprintf(stderr, "[ResidenceUI] Slot7 self=%p rowSlots(+0x38)=%d\n",
                    self, *reinterpret_cast<int*>(s + 0x38));
            fflush(stderr);
        }
    }
    ResidenceUI_Slot7(reinterpret_cast<ResidenceUI*>(self));
}

// idx8 (+0x40): per-frame. Build the table once (row count still 0), then keep the
// roster display live by re-populating every frame.
void W_OnUpdate(void* self, void* /*time*/) {
    EnsureGlobals();
    if (!self || !Ready() || !DataSourceAlive(self)) return;
    uintptr_t s = reinterpret_cast<uintptr_t>(self);
    if (*reinterpret_cast<int*>(s + 0x38) == 0) {
        ResidenceUI_Slot4(reinterpret_cast<ResidenceUI*>(self));
    }
    ResidenceUI_Slot7(reinterpret_cast<ResidenceUI*>(self));
}

// idx9 (+0x48): resident-row click. Dispatched (self, &elementPtr, msg, id, x, y).
void W_Slot9(void* self, void* p2, int msg, int id, int /*mx*/, int /*my*/) {
    if (!self || !Ready() || !DataSourceAlive(self)) return;
    ResidenceUI_Slot9(reinterpret_cast<ResidenceUI*>(self), p2, msg, id);
}

struct Init {
    Init() {
        for (int i = 0; i < kSlots; ++i) g_vt[i] = reinterpret_cast<void*>(&StubNop);
        g_vt[0] = reinterpret_cast<void*>(&Dtor);
        g_vt[2] = reinterpret_cast<void*>(&GetTypeId);
        g_vt[4] = reinterpret_cast<void*>(&W_Slot4);
        g_vt[7] = reinterpret_cast<void*>(&W_Slot7);
        g_vt[8] = reinterpret_cast<void*>(&W_OnUpdate);
        g_vt[9] = reinterpret_cast<void*>(&W_Slot9);
    }
} g_init;

void Zero(void* p) {
    for (size_t o = 0; o < static_cast<size_t>(0x40); o += 8)
        *reinterpret_cast<uint64_t*>(static_cast<char*>(p) + o) = 0;
}
void* Factory() {
    if (!fn_MemAlloc || !g_pMemAllocSingleton) return nullptr;
    void* p = fn_MemAlloc(g_pMemAllocSingleton, static_cast<uint64_t>(0x40), 8);
    if (!p) return nullptr;
    Zero(p);
    *reinterpret_cast<void**>(p) = g_vt;
    return p;
}
void* InPlace(void* m) {
    if (!m) return nullptr;
    Zero(m);
    *reinterpret_cast<void**>(m) = g_vt;
    return m;
}

}  // namespace residenceui_sa

REGISTER_TYPE(ResidenceUI, "UI::DialogController",
              &residenceui_sa::Factory, &residenceui_sa::InPlace);


// ============================================================
