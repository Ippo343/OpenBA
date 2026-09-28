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
#include "Component/coll_bridge.h"
#include "engine_funcs.h"
#include "Game/GameUpdateLoop.h"   // B3R STORAGE: GameUpdateLoop::_singleton
#include "system/String.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B
#include "runtime/types/ui/Element.h"  // ENDGAME B.3.6 UI callers (K3)
#include "runtime/types/ui/String.h"  // ENDGAME B.3.6 UI callers (K3)
#include "runtime/types/ui/StringTable.h"  // ENDGAME B.3.6 UI callers (K3)
#include "object/External.h"  // ENDGAME B.3.6 UI callers (K3)
#include <new>  // ENDGAME B.3.6 UI callers (K3)
#include "runtime/types/ui/TextElement.h"  // ENDGAME B.3.6 UI callers round 2 (K3)
#include "Object/factory_registration.h"
#include <cstddef>
#include "Component/ResidenceComponent.h"   // X::TypeId() (B.3.8 callers)
#include "Entity/ComponentEntity.h"   // X::TypeId() (B.3.8 callers)
#include "system/MemoryAllocator.h"   // B3R: the allocator doors
#include "Game/exe_constants.h"   // B3R: kRdata_*

// ResidenceUI::Slot7 is the per-frame roster refresh (driven via the +0x40 W_OnUpdate
// hack in standalone). Its _DEBUG bring-up logs used to fflush an unbuffered log once
// per resident EVERY frame — the dominant cost that tanked the framerate once a second
// entity panel could stay pinned open. Gate them behind BANISHED_DIALOG_TRACE (cached
// once). Kept inside _DEBUG so release builds are unchanged.
#ifdef _DEBUG
#endif

// Range check for pointers a standalone citizen may leave uninitialized. A rostered
// CitizenComponent in standalone can have a garbage description cache (+0x288) or an
// unset name String; the raw derefs the faithful retail port does would fault (MinGW
// has no __try/__except to catch it). Same idea as CitUI_OkPtr in citizenui.cpp.
static inline bool RUI_OkPtr(uintptr_t p) {
    return p >= 0x10000 && p <= 0x00007FFFFFFFFFFFULL;
}

// Helper: find element by name in parent element, returns raw pointer
// Original pattern: FindElement → CheckPtr → deref
static void* FindChildElem(void* parentElem, const char* name) {
    System::String nameStr(name);
    uint8_t retBuf[16];
    ::new (retBuf) Object::InternalPtr<UI::Element>(reinterpret_cast<UI::Element*>(parentElem)->FindElement(nameStr));  // B.3.6 bridge
    void* result = *(void**)retBuf;
    return result;
}

// Helper: find element by name, wrap as InternalBasePtr
// Original pattern: FindElement → CheckPtr → deref → IsDerivedFrom → IBP_Ctor
static uint64_t FindElemAsBasePtr(void* parentElem, const char* name) {
    void* elem = FindChildElem(parentElem, name);
    uint8_t bpBuf[8];
    new (bpBuf) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>(elem));   // B.3.3 bridge
    return *(uint64_t*)bpBuf;
}

namespace residenceui_sa {

// ENDGAME B.3.9 CG2: TypeId_ComponentEntity / TypeId_ResidenceComponent no longer
// need healing here — ComponentEntity::TypeId() / ResidenceComponent::TypeId() (the
// class statics) always answer, so the s_ceTid/s_resTid lazy-fill retires.
// B3R: the age multiplier is the .rdata constant kRdata_140172198 (5.0f); the
// never-taken 1.0f fallback over g_pAgeMultiplier (EnsureGlobals) is gone.
bool Ready() {
    return GameUpdateLoop::_singleton != nullptr;
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



}  // namespace residenceui_sa

// ============================================================
// ResidenceUI::Slot4 (vtable +0x20) — Setup resident table
// ============================================================
// Finds "residenceTable" (or "residenceLongTable" fallback), iterates
// rows to find child elements (buttonGoto, textName, imageMale, imageFemale,
// textAge, textProfession), stores in dynamic array at +0x30.
// Sets element IDs on buttonGoto: 300 + rowIndex.
//
// Original: 0x1400B5850.

// [4] 0x1400b5850
void ResidenceUI::OnCreated() {
    if (!residenceui_sa::Ready()) return;
    uintptr_t s = (uintptr_t)this;
    void* dialogElem = *(void**)(s + 0x20);

    // Try "residenceTable" first, fall back to "residenceLongTable"
    void* tableRaw = FindChildElem(dialogElem, "residenceTable");
    void* tableElem;

    if (!tableRaw) {
        // Fallback: find "residenceLongTable"
        void* longTable = FindChildElem(dialogElem, "residenceLongTable");
        uint8_t basePtrBuf[8];
        new (basePtrBuf) ::Object::InternalBasePtr(
            reinterpret_cast<RtObject::Internal*>(longTable));   // B.3.3 bridge
        tableElem = *(void**)basePtrBuf;
    } else {
        uint8_t basePtrBuf[8];
        new (basePtrBuf) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>(tableRaw));   // B.3.3 bridge
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
        ::new (childBuf) Object::InternalPtr<UI::Element>(reinterpret_cast<UI::Element*>(tableElem)->Child(base + 0));  // B.3.6 bridge
        entry[5] = FindElemAsBasePtr(*(void**)childBuf, "buttonGoto");
        if (entry[5]) reinterpret_cast<UI::Element*>(entry[5])->SetElementId(300 + i);  // B.3.6 bridge

        // Col 1: textName
        ::new (childBuf) Object::InternalPtr<UI::Element>(reinterpret_cast<UI::Element*>(tableElem)->Child(base + 1));  // B.3.6 bridge
        entry[2] = FindElemAsBasePtr(*(void**)childBuf, "textName");

        // Col 2: imageMale + imageFemale (container cell)
        ::new (childBuf) Object::InternalPtr<UI::Element>(reinterpret_cast<UI::Element*>(tableElem)->Child(base + 2));  // B.3.6 bridge
        void* iconCell = *(void**)childBuf;
        entry[0] = FindElemAsBasePtr(iconCell, "imageMale");

        ::new (childBuf) Object::InternalPtr<UI::Element>(reinterpret_cast<UI::Element*>(tableElem)->Child(base + 2));  // B.3.6 bridge
        entry[1] = FindElemAsBasePtr(*(void**)childBuf, "imageFemale");

        // Col 3: textAge
        ::new (childBuf) Object::InternalPtr<UI::Element>(reinterpret_cast<UI::Element*>(tableElem)->Child(base + 3));  // B.3.6 bridge
        entry[3] = FindElemAsBasePtr(*(void**)childBuf, "textAge");

        // Col 4: textProfession
        ::new (childBuf) Object::InternalPtr<UI::Element>(reinterpret_cast<UI::Element*>(tableElem)->Child(base + 4));  // B.3.6 bridge
        entry[4] = FindElemAsBasePtr(*(void**)childBuf, "textProfession");

#ifdef _DEBUG
        if (i < 3) {
            printf("[ResidenceUI::Slot4]   row[%d] base=%d: imgM=%llx imgF=%llx txtN=%llx txtA=%llx txtP=%llx btn=%llx\n",
                   i, base, entry[0], entry[1], entry[2], entry[3], entry[4], entry[5]);
            fflush(stdout);
        }
#endif

        // Push entry into dynamic array at +0x30.
        // B.3.2 bridge: ResidenceUI+0x30 is Collection::Array<{imageMale,
        // imageFemale, textName, textAge, textProfession, buttonGoto}>
        // (stride 0x30, count +0x38, capacity +0x3C) inside a struct §B.2 has
        // not converted, so it is reached at its offset. Was FUN_140021020's
        // own inlined SetSize (ResidenceUI_ArrayPush_Impl).
        auto& rows = GameColl::ArrayAt<GameColl::Bytes<0x30, 8>>(s, 0x30);
        rows.SetSize(rows.GetSize() + 1,
                     GameColl::Bytes<0x30, 8>(entry[0], entry[1], entry[2],
                                    entry[3], entry[4], entry[5]));
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

// [7] 0x1400b51f0
void ResidenceUI::OnUpdateLayout() {
    if (!residenceui_sa::Ready() || !residenceui_sa::DataSourceAlive(this)) return;
    uintptr_t s = (uintptr_t)this;

    // Get entity from data source +0x28
    void* dataSource = *(void**)(s + 0x28);
    bool isEntity = ::Object::IsDerivedFromNullSafe(dataSource, ComponentEntity::TypeId());
    void* entity = isEntity ? dataSource : nullptr;
    uint8_t entityBuf[8];
    new (entityBuf) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>(entity));   // B.3.3 bridge
    uintptr_t entityPtr = *(uintptr_t*)entityBuf;

    // Find ResidenceComponent — original uses type ID at 0x1401c7cf0 (ResidenceComponent),
    // NOT StorageComponent (0x1401c7e78). Different component with room data at +0x50/+0x58.
    uintptr_t storageComp = 0;
    uint32_t residenceComponentTypeId = ResidenceComponent::TypeId();
    Entity_FindComponentByExeTypeId((void*)entityPtr, &storageComp, &residenceComponentTypeId);

#ifdef _DEBUG
#endif

    float ageMul = kRdata_140172198;  // 5.0f
    int totalResidents = 0;
    int rowIdx = 0;

    if (!storageComp) return;

    // Outer loop: iterate room groups (storageComp+0x58 = group count)
    int groupCount = *(int*)(storageComp + 0x58);
#ifdef _DEBUG
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

                    // Get citizen ptr from resident list + CheckPtr
                    uintptr_t residentListPtr = *(uintptr_t*)(*(uintptr_t*)(storageComp + 0x50) + groupOffset);
                    uintptr_t citizen = *(uintptr_t*)(residentListPtr + r * 8);

#ifdef _DEBUG
#endif

                    // Get row entry: 0x30-byte entries at self+0x30
                    uintptr_t rowBase = *(uintptr_t*)(s + 0x30) + rowIdx * 0x30;

                    // Check gender: citizen+0x38 != 0 means male
                    bool isMale = (*(int*)(citizen + 0x38) != 0);

                    // Show/hide gender icons — CheckPtr each element IBP in row array
                    void* imageFemale = *(void**)(rowBase + 0x08);
                    if (imageFemale) {
                        if (isMale) {
                            reinterpret_cast<UI::Element*>(imageFemale)->SetFlagsOnOff(1, 0);  // B.3.6 bridge
                        } else {
                            reinterpret_cast<UI::Element*>(imageFemale)->SetFlagsOnOff(0, 1);  // B.3.6 bridge
                        }
                    }
                    void* imageMale = *(void**)rowBase;
                    if (imageMale) reinterpret_cast<UI::Element*>(imageMale)->SetFlagsOnOff((uint32_t)!isMale, (uint32_t)isMale);  // B.3.6 bridge

                    // Set name text: citizen+0x28 is the name String
                    void* textName = *(void**)(rowBase + 0x10);
                    // SA safety: the name String buffer (citizen+0x28) may be unset on a
                    // standalone citizen; SetParameter refcounts the source buffer and
                    // would fault on garbage. 0 (empty String) is fine.
                    uintptr_t nameBuf = *(uintptr_t*)(citizen + 0x28);
                    if (textName && (nameBuf == 0 || RUI_OkPtr(nameBuf)))
                        reinterpret_cast<UI::TextElement*>(textName)->SetParameter(0, *reinterpret_cast<const System::String*>(citizen + 0x28));  // B.3.6 bridge

                    // Set age text: (int)(age * 5.0f) formatted as "%i"
                    int ageYears = (int)(*(float*)(citizen + 0x3c) * ageMul);
                    System::String ageStr = System::String::Format("%i", (uint64_t)(uint32_t)ageYears);
                    void* textAge = *(void**)(rowBase + 0x18);
                    if (textAge) reinterpret_cast<UI::TextElement*>(textAge)->SetParameter(0, ageStr);  // B.3.6 bridge

                    // Determine profession text. Faithful to retail 0x1400B51F0:
                    //   if (age < desc.childThreshold(+0x44) || job(+0x208) != 0):
                    //       job == 0 -> ProfessionList+0x40 (child/student label)
                    //       job != 0 -> ProfessionList+0x48 (employed label)
                    //   else (adult, no job) -> the citizen's own profession (+0x1f0)
                    // (The prior port had these branches scrambled and read +0x208 as a
                    //  "dead" flag; corrected here.) Every deref is range-guarded so a
                    // standalone citizen with a garbage description cache can't fault.
                    void* textProfession = *(void**)(rowBase + 0x20);

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
                        void* world = GameUpdateLoop::_singleton;
                        uintptr_t profList = world ? *(uintptr_t*)((uintptr_t)world + 0xB0) : 0;
                        if (RUI_OkPtr(profList)) {
                            uintptr_t entry = *(uintptr_t*)(profList + (jobEntity == 0 ? 0x40 : 0x48));
                            if (RUI_OkPtr(entry)) {
                                ::new (uiStr) UI::String(Object::ExternalPtr<UI::StringTable>(reinterpret_cast<UI::StringTable*>(*(uint64_t*)(entry + 0x20))), *reinterpret_cast<const System::String*>(entry + 0x30));  // B.3.6 bridge
                                builtProf = true;
                            }
                        }
                    } else {
                        // Adult laborer: the citizen's own profession object (+0x1f0).
                        uintptr_t profDesc = *(uintptr_t*)(citizen + 0x1f0);
                        if (RUI_OkPtr(profDesc)) {
                            ::new (uiStr) UI::String(Object::ExternalPtr<UI::StringTable>(reinterpret_cast<UI::StringTable*>(*(uint64_t*)(profDesc + 0x20))), *reinterpret_cast<const System::String*>(profDesc + 0x30));  // B.3.6 bridge
                            builtProf = true;
                        }
                    }
                    if (textProfession && builtProf) reinterpret_cast<UI::TextElement*>(textProfession)->SetText(reinterpret_cast<const UI::String*>(uiStr));  // B.3.6 bridge

                    // Enable buttonGoto: retail 0x1400b54f6 `MOV DL,0x1` before
                    // `CALL [RAX+0x40]` (the call used to leave RDX unset).
                    void* buttonGoto = *(void**)(rowBase + 0x28);
                    if (buttonGoto) {
                        reinterpret_cast<UI::Element*>(buttonGoto)->Enable(true);   // B.3.x bridge (slot 8)
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
            if (imageMale) reinterpret_cast<UI::Element*>(imageMale)->SetFlagsOnOff(1, 0);  // B.3.6 bridge
            void* imageFemale = *(void**)(rowBase + 0x08);
            if (imageFemale) reinterpret_cast<UI::Element*>(imageFemale)->SetFlagsOnOff(1, 0);  // B.3.6 bridge

            // Clear name text
            {
                System::String emptyStr1("");
                void* textName = *(void**)(rowBase + 0x10);
                if (textName) reinterpret_cast<UI::TextElement*>(textName)->SetParameter(0, emptyStr1);  // B.3.6 bridge
            }

            // Clear age text
            {
                System::String emptyStr2("");
                void* textAge = *(void**)(rowBase + 0x18);
                if (textAge) reinterpret_cast<UI::TextElement*>(textAge)->SetParameter(0, emptyStr2);  // B.3.6 bridge
            }

            // Reset profession text: fn_UIString_Ctor(retbuf, externalPtr from +0xD8, empty string)
            void* textProfession = *(void**)(rowBase + 0x20);
            if (textProfession) {
                System::String emptyStr3("");
                uint64_t origUIStr[2];
                ::new (origUIStr) UI::String(
                    Object::ExternalPtr<UI::StringTable>(reinterpret_cast<UI::StringTable*>(*(uint64_t*)((uintptr_t)textProfession + 0xD8))), emptyStr3);  // B.3.6 bridge
                reinterpret_cast<UI::TextElement*>(textProfession)->SetText(reinterpret_cast<const UI::String*>(origUIStr));  // B.3.6 bridge
            }

            // Disable buttonGoto (retail 0x1400b5664 `XOR EDX,EDX`).
            void* buttonGoto = *(void**)(rowBase + 0x28);
            if (buttonGoto) {
                reinterpret_cast<UI::Element*>(buttonGoto)->Enable(false);   // B.3.x bridge (slot 8)
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
    bool isEntity = ::Object::IsDerivedFromNullSafe(dataSource, ComponentEntity::TypeId());
    void* entity = isEntity ? dataSource : nullptr;
    uint8_t entityBuf[8];
    new (entityBuf) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>(entity));   // B.3.3 bridge
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
                uintptr_t citizenPtr = *(uintptr_t*)(
                    *(uintptr_t*)(*(uintptr_t*)(storageComp + 0x50) + (uintptr_t)g * 0x10)
                    + (uintptr_t)r * 8);

                // Get citizen's InternalBasePtr self-reference at +0x10
                uint64_t citizenBP = *(uint64_t*)(citizenPtr + 0x10);
                // Get parent dialog ptr from self+0x18
                uint64_t parentDialog = *(uint64_t*)(s + 0x18);

                // Center camera on this citizen via toolController (gameWorld+0x120)
                void* toolController = *(void**)((uintptr_t)GameUpdateLoop::_singleton + 0x120);
                Toolbar_CenterOnEntity_Impl(toolController, &parentDialog, &citizenBP);
                return;
            }
            flatIdx++;
        }
    }
}

// [9] 0x1400b5820
void ResidenceUI::OnElementEvent(Object::InternalPtr<UI::Element>* element, int32_t msg, int32_t id,
                                 int32_t /*c*/, int32_t /*d*/, int32_t /*e*/, void* /*userData*/) {
    void* param2 = element;
    if (!residenceui_sa::Ready() || !residenceui_sa::DataSourceAlive(this)) return;
    (void)param2;
    uintptr_t s = (uintptr_t)this;

    if (msg != 0x12) return;

    int count = *(int*)(s + 0x38);
    if (id >= 300 && id < 300 + count) {
        ResidenceUI_GoToCitizen(this, id - 300);
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


// ~ResidenceUI: the tree's own teardown (its hand-built slot 0 ran this, then freed
// WITHOUT ~DialogController); moved here unchanged. The compiler now chains
// ~DialogController -> ~Internal after it.
ResidenceUI::~ResidenceUI() {
    void* self = this;
    // Free the cached per-row element array at +0x30 (mirrors the exe ScalarDtor).
    uintptr_t s = reinterpret_cast<uintptr_t>(self);
    void* rows = *reinterpret_cast<void**>(s + 0x30);
    if (rows) System::MemoryAllocator::MemFree(rows);
    *reinterpret_cast<uint64_t*>(s + 0x30) = 0;
    *reinterpret_cast<uint64_t*>(s + 0x38) = 0;
}



// [8] STANDALONE DEVIATION (retail inherits the base slot) — the tree's wiring, kept.
void ResidenceUI::OnUpdate(const System::Time& /*time*/) {
    if (!residenceui_sa::Ready() || !residenceui_sa::DataSourceAlive(this)) return;
    uintptr_t s = reinterpret_cast<uintptr_t>(this);
    if (*reinterpret_cast<int*>(s + 0x38) == 0) {
        ResidenceUI::OnCreated();
    }
    ResidenceUI::OnUpdateLayout();
}


// create 0x140020f60: MemAlloc(0x40, 8), then the ctor (DialogController ->
// Internal: AddObject, the compiler's stamps, the field initialisers).
void* ResidenceUI::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(ResidenceUI), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) ResidenceUI());
}
// Retail's createInPlace does not build a controller in place (B8-13); the
// tree's in-place entry did, and that is kept.
void* ResidenceUI::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) ResidenceUI());
}

DEFINE_EXE_TYPE(ResidenceUI, "UI::DialogController")

// ============================================================
