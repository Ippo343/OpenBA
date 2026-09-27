// code/Game/Dialog/eventnotifydialog.cpp
// Clean reimplementation of EventNotifyDialog game logic.
//
// EventNotifyDialog (0xF0 bytes) — notification ribbon for game events.
// Extends DialogInstance (0xB8 base).
//
// Field layout:
//   +0xB8: (base field)
//   +0xC0: float smoothedYOffset (lerped ribbon position)
//   +0xC4: int32 lastEventIndex (last processed event log index)
//   +0xC8: void* notifyEntryArray (dynamic array data pointer)
//   +0xD0: int32 notifyEntryCount
//   +0xD4: int32 notifyEntryCapacity
//   +0xD8: void* activeEventArray (events being displayed, stride 0x30)
//   +0xE0: int32 activeEventCount
//   +0xE8: void* ribbonNotifyElement (ribbon container element)
//
// Decompiled from Application-x64.exe.

#include "ui/ImageElement.h"   // ENDGAME B.3.6 UI callers (K2)
#include "ui/Dialog.h"   // X::TypeId() (ENDGAME B.3.9)
#include "ui/Element.h"   // ENDGAME B.3.6 UI callers (K2)
#include "ui/DialogInstance.h"   // ENDGAME B.3.6 UI callers (K2)
#include "ui/UserInterface.h"   // ENDGAME B.3.6 UI callers (K2)
#include "ui/Sprite.h"   // ENDGAME B.3.6 UI callers (K2)
#include "ui/SpriteSheet.h"   // ENDGAME B.3.6 UI callers (K2)
#include "Dialog/EventNotifyDialog.h"
#include "Dialog/EventPopupDialog.h"
#include "Component/coll_bridge.h"
#include "engine_funcs.h"
#include "system/String.h"
#include "system/Filename.h"
#include "object/External.h"   // ENDGAME B.3.3 step 6A
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <new>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B
#include "audio/Audio.h"     // ENDGAME B.3.7 callers (K4)

// ============================================================
// EventNotifyDialog_CreatePopup — modal popup creation (Slot7 popup branch)
// ============================================================
// Faithful to the popup branch inside EventNotifyDialog::Slot7 (0x140067C30):
// when an event's popup field (entry+0x20) is >= 0, the ribbon raises a modal
// EventPopupDialog for it. The exe allocates a 0xC0-byte DialogInstance built
// against the ribbon's referenced dialog resource (EventNotify.rsc declares
// `External _references ["EventPopup.rsc"]`), stamps the EventPopupDialog vtable
// and the event-log index at +0xB8, then adds it to the UserInterface.
//
// In standalone we load Dialog/EventPopup.rsc by path (yielding the same
// Dialog-typed External the exe reaches via self+0x18 -> Dialog::GetReference)
// and drive the promoted EventPopupDialog standalone ctor. Returns the new
// dialog, or null if the resource/allocation is unavailable.
namespace eventnotify_sa { void EnsureGlobals(); }

static void* EventNotifyDialog_CreatePopup(EventNotifyDialog* self, int eventIndex) {
    uintptr_t s = (uintptr_t)self;
    void* ui = *(void**)(s + 0x10);
    if (!ui) return nullptr;

    // Load Dialog/EventPopup.rsc. The exe pulls this same resource from the ribbon's
    // own referenced-resource slot (self+0x18); loading by path is equivalent and
    // sidesteps the member-return Dialog::GetReference ABI.
    //
    // NOT cached across calls. It used to be, behind a one-shot s_triedRes flag --
    // the same shape as the Male.ndb/Female.ndb cache in citizen_names.cpp that
    // crashed the second new game of a session (2026-08-29). Quit-to-main-menu frees
    // the resource, and a one-shot flag means it is never reloaded, so the first
    // event popup of game two would build a dialog on a freed External. FindOrLoad is
    // a lookup-or-load; the repeat cost is a hash lookup, and popups are rare.
    void* s_popupRes = nullptr;
    {
        if (g_pExternalFactorySingleton &&
            *g_pExternalFactorySingleton) {
            System::Filename filename("Dialog/EventPopup.rsc");
            void* found = const_cast<::Object::External*>(
                reinterpret_cast<::Object::ExternalFactory*>(*g_pExternalFactorySingleton)   // B.3.3 bridge
                    ->FindOrLoad(filename));
            // Accept the loaded resource if it is Dialog-derived — the same guard the
            // exe applies before using it as the popup's external context.
            if (found && fn_IsDerivedFrom(found, UI::Dialog::TypeId()))
                s_popupRes = found;
        }
    }
    if (!s_popupRes) return nullptr;

    void* mem = fn_MemAlloc(g_pMemAllocSingleton, 0xC0, 8);
    if (!mem) return nullptr;
    void* popup = ::new (mem) EventPopupDialog(static_cast<UI::UserInterface*>(ui),
                                               static_cast<const UI::Dialog*>(s_popupRes),
                                               Object::InternalPtr<UI::DialogInstance>(),
                                               eventIndex);

    int64_t dialogPtr[1];
    new (dialogPtr) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>(popup));   // B.3.3 bridge
    reinterpret_cast<UI::UserInterface*>(ui)->AddDialog(reinterpret_cast<Object::InternalPtr<UI::DialogInstance>*>(dialogPtr));  // B.3.6 bridge
    return popup;
}

// ============================================================
// File-local helper: EventNotifyDialog_ArrayPush (FUN_140011750)
// ============================================================
// General-purpose array set-count/resize for 0x30-byte stride arrays with
// an embedded System::String at offset +0x20 within each element.
//
// arrayBase layout: [data* at +0][count int32 at +8][capacity int32 at +0xc]
// param_2 = new desired count (usually oldCount+1 for push)
// param_3 = pointer to new entry to copy into the last slot (when growing)
//
// Behavior:
//   - If new count == old count: no-op.
//   - If capacity < new count: reallocate (triple capacity for single-element grows).
//   - If realloc happened: copy existing elements [0..oldCount) from old buf + append param_3.
//     Dtor and free old buf.
//   - If no realloc: copy/move elements in place (copy param_3 into position newCount-1
//     for a grow, or dtor excess elements for a shrink).
//
// Original: FUN_140011750.
//
// B.3.2 NOT CONVERTED to Collection::Array<T>::SetSize: the 0x30-byte element
// carries a System::String at +0x20, so this instantiation has a real
// copy-construct AND a real destroy step (the shrink arm runs the String dtor).
// The trait-derived blob spec the Dialog bridge uses is byteCopy with
// destroy == nullptr and would leak the String on every shrink. This needs a
// hand-derived ArraySpec of its own, or the owning struct becoming a real class
// (§B.2); until then the transcription stays.
//
static void EventNotifyDialog_ArrayPush_Impl(void* arrayBasePtr, int newCount, void* newEntry) {
    uintptr_t* base = (uintptr_t*)arrayBasePtr;
    uintptr_t  oldData     = *base;
    int*       countPtr    = (int*)((uintptr_t)base + 8);
    int*       capacityPtr = (int*)((uintptr_t)base + 0xC);

    int oldCount = *countPtr;
    if (newCount == oldCount)
        return;

    *countPtr = newCount;

    uintptr_t newData = oldData;

    // Realloc if needed
    if (*capacityPtr < newCount) {
        *capacityPtr = newCount;
        // Triple-capacity heuristic when growing by exactly 1
        if (newCount == oldCount + 1 && oldCount != 0) {
            *capacityPtr = newCount * 3;
        }
        void* alloc = fn_MemAlloc(g_pMemAllocSingleton, (uintptr_t)*capacityPtr * 0x30, 8);
        *base = (uintptr_t)alloc;
        newData = (uintptr_t)alloc;
    }

    // Determine range to copy: if no realloc, copy from old position [0..min(oldCount,newCount))
    // If realloc, we must copy all preserved entries from the old buffer.
    int copyFrom = (oldData == newData) ? oldCount : 0;  // if same buf, start = oldCount (no copy of old)
    // Actually: if same buffer, copy range is [oldCount..newCount) from param_3 for grows
    // or just update count for shrinks. If different buffer: copy [0..min(oldCount,newCount)) then append.

    // Rebuilt from decompiler trace:
    // uVar3 = (pvVar6 == *param_1) ? uVar1 : 0  i.e. if same buf, startIdx = oldCount else 0
    int startIdx = (oldData == newData) ? oldCount : 0;

    if (startIdx < newCount) {
        uintptr_t srcBase = oldData + (uintptr_t)startIdx * 0x30;
        uintptr_t dstBase = newData + (uintptr_t)startIdx * 0x30;
        for (int i = startIdx; i < newCount; i++, srcBase += 0x30, dstBase += 0x30) {
            // Source: old entry if i < oldCount, otherwise param_3
            void* src = (i < oldCount) ? (void*)srcBase : newEntry;
            void* dst = (void*)dstBase;
            // Copy non-String fields
            *(uint64_t*)((uintptr_t)dst + 0x00) = *(uint64_t*)((uintptr_t)src + 0x00);
            *(uint32_t*)((uintptr_t)dst + 0x08) = *(uint32_t*)((uintptr_t)src + 0x08);
            *(uint64_t*)((uintptr_t)dst + 0x10) = *(uint64_t*)((uintptr_t)src + 0x10);
            *(uint64_t*)((uintptr_t)dst + 0x18) = *(uint64_t*)((uintptr_t)src + 0x18);
            // Copy String at +0x20 with copy ctor (dst is fresh/uninitialized memory,
            // so this is a placement-new construction, not an assignment).
            // B.3.1 bridge: field is a raw exe-struct SystemString, not owned here.
            new ((void*)((uintptr_t)dst + 0x20)) System::String(
                *reinterpret_cast<System::String*>((uintptr_t)src + 0x20));
            *(uint32_t*)((uintptr_t)dst + 0x28) = *(uint32_t*)((uintptr_t)src + 0x28);
        }
    }

    // If same buffer: dtor excess entries [newCount..oldCount) on shrink
    if (oldData == newData) {
        if (newCount < oldCount) {
            for (int i = newCount; i < oldCount; i++) {
                reinterpret_cast<System::String*>(oldData + (uintptr_t)i * 0x30 + 0x20)->~String();  // B.3.1 bridge
            }
        }
    } else {
        // Different buffer: dtor all entries in old buf and free it
        for (int i = 0; i < oldCount; i++) {
            reinterpret_cast<System::String*>(oldData + (uintptr_t)i * 0x30 + 0x20)->~String();  // B.3.1 bridge
        }
        fn_MemFreeStatic((void*)oldData);
    }
}

// ============================================================
// File-local helper: EventNotifyDialog_ArrayRemove (FUN_140011670)
// ============================================================
// Removes element at [param_2] from a dynamic array with 0x30-byte stride.
// The element has a System::String at offset +0x20 that must be dtored.
// Shifts all subsequent elements down by one slot.
// arrayBase layout: [data*, count, capacity] at [0], [8], [0xc]
//
// Original: 0x140011670.
//
static void EventNotifyDialog_ArrayRemove_Impl(void* arrayBasePtr, int removeIndex) {
    // arrayBasePtr points to layout: [data* 8][count int32][capacity int32]
    uintptr_t data = *(uintptr_t*)arrayBasePtr;
    int* countPtr  = (int*)((uintptr_t)arrayBasePtr + 8);
    int count      = *countPtr;

    if (removeIndex < count) {
        // Dtor the String at +0x20 within the removed element.
        // B.3.1 bridge: field is a raw exe-struct SystemString, not owned here.
        reinterpret_cast<System::String*>(data + (uintptr_t)removeIndex * 0x30 + 0x20)->~String();

        // Shift subsequent entries down (removeIndex+1 ... count-1 -> removeIndex ... count-2)
        for (int i = removeIndex + 1; i < count; i++) {
            uintptr_t dst = data + (uintptr_t)(i - 1) * 0x30;
            uintptr_t src = data + (uintptr_t)i * 0x30;
            // Copy non-String fields
            *(uint64_t*)(dst + 0x00) = *(uint64_t*)(src + 0x00);
            *(uint32_t*)(dst + 0x08) = *(uint32_t*)(src + 0x08);
            *(uint64_t*)(dst + 0x10) = *(uint64_t*)(src + 0x10);
            *(uint64_t*)(dst + 0x18) = *(uint64_t*)(src + 0x18);
            // Copy String field at +0x20 using String copy ctor (dst slot's old String
            // was already destroyed by the shift above or by the removal above it, so
            // this is a placement-new construction). B.3.1 bridge.
            new ((void*)(dst + 0x20)) System::String(
                *reinterpret_cast<System::String*>(src + 0x20));
            *(uint32_t*)(dst + 0x28) = *(uint32_t*)(src + 0x28);
        }
    }
    // Decrement count
    *countPtr = count - 1;
}

// ============================================================
// EventNotifyDialog::Slot4 (vtable +0x20) — Startup
// ============================================================
// Finds "ribbonNotify" -> stores at +0xE8.
// Finds "notifyContent" container element.
// For each child of notifyContent:
//   - Gets child element (group) -> entry[0]
//   - Finds "notifyImage" sub-element -> entry[1]
//   - Finds "notifyButton" sub-element -> entry[2]
//   - Sets button element ID to 1500 + index
//   - Pushes 0x18-byte entry into array at +0xC8
// Finally copies event log counter to +0xC4.
//
// Original: 0x1400686d0.

// ============================================================
// EventNotifyDialog::Slot5 (vtable +0x28) — Shutdown
// ============================================================
// 3-byte no-op: XOR EAX,EAX; RET. Returns 0.
// Original: 0x1400686c0.

void EventNotifyDialog::OnClosed() {}

void EventNotifyDialog::OnOpened() {
    eventnotify_sa::EnsureGlobals();
    uintptr_t s = (uintptr_t)this;

    // Find "ribbonNotify" -> store raw InternalPtr value at +0xE8
    {
        System::String nameStr("ribbonNotify");
        uint8_t retBuf[16];
        ::new (retBuf) Object::InternalPtr<UI::Element>(reinterpret_cast<UI::DialogInstance*>(this)->FindElement(nameStr));  // B.3.6 bridge
        *(uint64_t*)(s + 0xE8) = *(uint64_t*)retBuf;
    }

    // Find "notifyContent" element
    void* notifyContent = nullptr;
    {
        System::String nameStr("notifyContent");
        uint8_t retBuf[16];
        ::new (retBuf) Object::InternalPtr<UI::Element>(reinterpret_cast<UI::DialogInstance*>(this)->FindElement(nameStr));  // B.3.6 bridge
        notifyContent = *(void**)retBuf;
    }

    // Iterate children of notifyContent
    int childCount = reinterpret_cast<const UI::Element*>(notifyContent)->ChildCount();  // B.3.6 bridge

    for (int i = 0; i < childCount; i++) {
        // Initialize entry: {group, image, button}
        uint64_t entry[3] = {0, 0, 0};

        // Get child[i] -> entry[0] (group element)
        {
            uint8_t retBuf[16];
            ::new (retBuf) Object::InternalPtr<UI::Element>(reinterpret_cast<UI::Element*>(notifyContent)->Child(i));  // B.3.6 bridge
            entry[0] = *(uint64_t*)retBuf;
        }

        // Find "notifyImage" in child[i] -> entry[1]
        {
            System::String nameStr("notifyImage");

            // Get child[i] again for the FindElement base
            uint8_t childBuf[16];
            ::new (childBuf) Object::InternalPtr<UI::Element>(reinterpret_cast<UI::Element*>(notifyContent)->Child(i));  // B.3.6 bridge
            void* child = *(void**)childBuf;

            uint8_t retBuf[16];
            ::new (retBuf) Object::InternalPtr<UI::Element>(reinterpret_cast<UI::Element*>(child)->FindElement(nameStr));  // B.3.6 bridge
            void* found = *(void**)retBuf;

            entry[1] = (uint64_t)found;
        }

        // Find "notifyButton" in child[i] -> entry[2]
        {
            System::String nameStr("notifyButton");

            uint8_t childBuf[16];
            ::new (childBuf) Object::InternalPtr<UI::Element>(reinterpret_cast<UI::Element*>(notifyContent)->Child(i));  // B.3.6 bridge
            void* child = *(void**)childBuf;

            uint8_t retBuf[16];
            ::new (retBuf) Object::InternalPtr<UI::Element>(reinterpret_cast<UI::Element*>(child)->FindElement(nameStr));  // B.3.6 bridge
            void* found = *(void**)retBuf;

            entry[2] = (uint64_t)found;
        }

        // Set button element ID to 1500 + i (0x5DC + i)
        reinterpret_cast<UI::Element*>((void*)entry[2])->SetElementId(i + 0x5DC);  // B.3.6 bridge

        // Push entry into dynamic array at +0xC8 (0x18-byte stride, no Strings).
        // This is an INLINE push — NOT EventNotifyDialog_ArrayPush_Impl which is
        // for the 0x30-stride +0xD8 array with String at +0x20 (that one is left
        // as a hand-rolled loop: it destroys a String on shrink and copy-constructs
        // one on grow, which GameColl::Bytes's byte-blob semantics cannot reproduce).
        // B.3.2 bridge: EventNotifyDialog+0xC8 is Collection::Array<{entry[0],
        // entry[1], entry[2]}> (stride 0x18, count +0xD0, capacity +0xD4) inside a
        // struct §B.2 has not converted, so it is reached at its offset.
        {
            auto& rows = GameColl::ArrayAt<GameColl::Bytes<0x18, 8>>(s, 0xC8);
            rows.SetSize(rows.GetSize() + 1,
                         GameColl::Bytes<0x18, 8>(entry[0], entry[1], entry[2]));
        }
    }

    // Copy event log counter: this+0xC4 = *(*(world+0x138) + 0x20)
    void* world = *g_pGameWorldSingleton;
    uintptr_t eventLog = *(uintptr_t*)((uintptr_t)world + 0x138);
    *(uint32_t*)(s + 0xC4) = *(uint32_t*)(eventLog + 0x20);
}

// ============================================================
// EventNotifyDialog::Slot7 (vtable +0x38) — Event processing
// ============================================================
// Decrements timers for active notifications, removes expired ones.
// Scans event log for new events since last processed index (+0xC4).
// Maps event types to categories, checks filter flags.
// Creates notification entries with icon, color, text, timer.
// Plays sound effect if new events were processed.
// Trims excess entries if beyond display capacity.
//
// Active event entry (0x30 bytes at +0xD8 array):
//   +0x00: Time timer (8 bytes, countdown)
//   +0x08: int32 eventIndex
//   +0x0C: (padding, 4 bytes)
//   +0x10: UI::Sprite (16 bytes, icon sprite)
//   +0x20: System::String (8 bytes, display text)
//   +0x28: uint32 color (ARGB)
//
// Original: 0x140067C30.

void EventNotifyDialog::OnUpdate(const System::Time& time) {
    eventnotify_sa::EnsureGlobals();
    uintptr_t s = (uintptr_t)this;

    uintptr_t eventLogPtr = *(uintptr_t*)((uintptr_t)(*g_pGameWorldSingleton) + 0x138);
    int64_t* tickParam = (int64_t*)&time;   // System::Time is one int64 tick count

    // Phase 1: Decrement timers on active notifications, remove expired ones
    int activeCount = *(int*)(s + 0xE0);
    if (activeCount > 0) {
        int idx = 0;
        int64_t offset = 0;
        do {
            // Decrement timer
            int64_t* timerPtr = (int64_t*)(offset + *(uintptr_t*)(s + 0xD8));
            *timerPtr -= *tickParam;

            // If timer expired (< 1), remove this entry
            if (*(int64_t*)(offset + *(uintptr_t*)(s + 0xD8)) < 1) {
                EventNotifyDialog_ArrayRemove_Impl((void*)(s + 0xD8), idx);
                idx--;
                offset -= 0x30;
            }

            idx++;
            offset += 0x30;
        } while (idx < *(int*)(s + 0xE0));
    }

    // Phase 2: Scan event log for new events
    float timerDuration = *g_pNotifyTimerDuration;  // 30.0f
    int lastEventIdx = *(int*)(s + 0xC4);
    int totalEvents = *(int*)(eventLogPtr + 0x20) - 1;

    if (lastEventIdx < totalEvents) {
        bool hasNewEvents = false;
        int nextIdx = lastEventIdx + 1;
        int scanIdx = nextIdx;

        if (scanIdx <= totalEvents) {
            int64_t entryOffset = (int64_t)scanIdx * 0x28;
            int remaining = totalEvents - scanIdx + 1;

            do {
                uintptr_t eventArray = *(uintptr_t*)(eventLogPtr + 0x18);
                int severity = *(int*)(entryOffset + 0x14 + eventArray);

                if (severity > 0) {
                    // Map event type to category
                    int eventType = *(int*)(entryOffset + 0x10 + eventArray);
                    int category;
                    switch (eventType) {
                        case 0: case 1: case 3: case 4:
                        case 5: case 6: case 7:
                            category = 1; break;
                        case 2:
                            category = 0; break;
                        case 0xB: case 0xC: case 0x1E: case 0x1F:
                            category = 2; break;
                        case 0xD: case 0xE:
                            category = 4; break;
                        case 0x11: case 0x12: case 0x13: case 0x14:
                        case 0x15: case 0x16: case 0x17: case 0x18:
                            category = 3; break;
                        default:
                            category = 5; break;
                    }

                    // Check category filter flag
                    if (*(char*)(category + 0x10 + eventLogPtr) != 0 &&
                        *(int*)(entryOffset + 0x1C + eventArray) >= 0) {

                        // Check if popup needed (+0x20 field >= 0 means show popup).
                        // Raise the modal EventPopupDialog for this event-log index.
                        if (*(int*)(entryOffset + 0x20 + eventArray) >= 0) {
                            EventNotifyDialog_CreatePopup(this, scanIdx);
                        }

                        // Build notification entry (0x30 bytes)
                        uint8_t notifyEntry[0x30];
                        memset(notifyEntry, 0, 0x30);

                        // +0x08 = event index (int32)
                        *(int*)(notifyEntry + 0x08) = scanIdx;

                        // +0x00 = timer (Time constructed from float seconds)
                        fn_Time_Ctor_Float(notifyEntry, timerDuration);

                        // +0x10 = sprite icon (from "EventIcon%i" format)
                        int iconIndex = *(int*)(entryOffset + 0x1C + eventArray);
                        System::String iconNameStr = System::String::Format("EventIcon%i", (uint64_t)(uint32_t)iconIndex);

                        // Get sprite from first notification slot's image element
                        uintptr_t slotArray = *(uintptr_t*)(s + 0xC8);
                        void* imageElem = *(void**)(slotArray + 8);  // first entry's image
                        uint8_t spriteBuf[16];
                        ::new (spriteBuf) UI::Sprite(reinterpret_cast<const UI::ImageElement*>(imageElem)->GetSprite());  // B.3.6 bridge

                        // Create sprite with icon name (copies sprite sheet, sets index)
                        uint8_t newSprite[32];
                        ::new (newSprite) UI::Sprite(Object::ExternalPtr<UI::SpriteSheet>(reinterpret_cast<UI::SpriteSheet*>(*(uint64_t*)spriteBuf)), iconNameStr);  // B.3.6 bridge
                        *(uint64_t*)(notifyEntry + 0x10) = *(uint64_t*)newSprite;
                        *(uint64_t*)(notifyEntry + 0x18) = *(uint64_t*)(newSprite + 8);

                        // +0x28 = color based on severity
                        int sev = *(int*)(entryOffset + 0x14 + eventArray);
                        uint32_t color;
                        if (sev == 1)      color = 0xFFFFFF00;  // yellow
                        else if (sev == 2) color = 0xFF8080FF;  // pink
                        else if (sev == 3) color = 0xFFFF6060;  // red
                        else if (sev == 4) color = 0xFFEA7517;  // orange
                        else               color = 0xFFFFFFFF;  // white
                        *(uint32_t*)(notifyEntry + 0x28) = color;

                        // +0x20 = text string (strip category prefix before ':')
                        // Event text is at eventArray + entryOffset + 0x08.
                        // B.3.1 bridge: raw exe-struct SystemString field, not owned here.
                        uintptr_t eventTextAddr = entryOffset + eventArray + 0x08;
                        System::String& eventTextStr = *reinterpret_cast<System::String*>(eventTextAddr);

                        // Get string data pointer
                        uintptr_t strData = *(uintptr_t*)&eventTextStr;
                        int strLen = 0;
                        if (strData != 0) {
                            strLen = *(int*)(strData - 8);
                        }

                        // Find ':' separator
                        int colonPos = 0;
                        for (int c = 0; c < strLen; c++) {
                            if (*(char*)(strData + c) == ':') {
                                colonPos = c + 1;  // skip the colon
                                break;
                            }
                            colonPos = c + 1;
                        }

                        // notifyEntry was memset to 0 above, so +0x20 is already a valid
                        // default-constructed System::String; assign into it directly.
                        // B.3.1 bridge: notifyEntry mirrors a raw exe struct layout.
                        System::String& entryStr = *reinterpret_cast<System::String*>(notifyEntry + 0x20);

                        // If colon found, take substring after it; otherwise use whole string
                        if (colonPos == strLen) {
                            entryStr = eventTextStr;
                        } else {
                            entryStr = eventTextStr.SubString(colonPos + 1, strLen - 1);
                        }

                        // Push entry to active array at +0xD8
                        int newCount = *(int*)(s + 0xE0) + 1;
                        EventNotifyDialog_ArrayPush_Impl((void*)(s + 0xD8), newCount, notifyEntry);

                        hasNewEvents = true;

                        // Clean up the string in our local entry (array push copied it)
                        entryStr.~String();
                    }
                }

                scanIdx++;
                entryOffset += 0x28;
                remaining--;
            } while (remaining != 0);
        }

        // Trim excess entries if beyond display capacity (+0xD0)
        if (*(int*)(s + 0xD0) < *(int*)(s + 0xE0)) {
            do {
                EventNotifyDialog_ArrayRemove_Impl((void*)(s + 0xD8), 0);
            } while (*(int*)(s + 0xD0) < *(int*)(s + 0xE0));
        }

        // Play sound effect if new events were found
        if (hasNewEvents) {
            uint8_t zeroTime[8];
            fn_Time_Ctor_Float(zeroTime, 0.0f);
            void* audioScene = *(void**)((uintptr_t)(*g_pGameWorldSingleton) + 0x78);
            uint64_t soundEffect = *(uint64_t*)(*(uintptr_t*)g_pKeyMapSingleton + 0x98);
            reinterpret_cast<Audio::Scene*>(audioScene)->PlayEffect(
                reinterpret_cast<Audio::SoundEffect*>(soundEffect), static_cast<Audio::Scene::SoundType>(1),
                *reinterpret_cast<const System::Time*>(zeroTime));   // B.3.7 bridge
        }

        totalEvents = *(int*)(eventLogPtr + 0x20) - 1;
    }

    // Update last processed event index
    *(int*)(s + 0xC4) = totalEvents;
}

// ============================================================
// EventNotifyDialog::Update (vtable +0x30) — Ribbon position and slot sync
// ============================================================
// Each frame:
//   1. Compute target Y for the ribbon: (parentRoot.y - selfRoot.y) + 44
//      — positions the ribbon just below the parent dialog (toolbar).
//   2. Lerp the smoothed Y toward target: smoothedY = smoothedY*0.8 + targetY*0.2
//   3. Apply smoothedY as top padding on the self root element.
//   4. Show the ribbonNotify container element.
//   5. For each active notification slot (0..activeEventCount-1):
//      - Copy the event's icon sprite to the slot's imageElem.
//      - Get child[4] of buttonElem; if it is an ImageElement, copy sprite to it too.
//      - Check event log entry popup field at eventEntry+0x18:
//          if < 0 (no popup): hide button, show image.
//          if >= 0 (popup active): show button, hide image.
//      - Obtain button's controller via vtable[0x58/8=11], call vtable[0x50/8=10](color).
//      - Also call vtable[0x50] on each of the 4 button children with the same color.
//      - Copy the event's text string into the group element's string at +0x58.
//   6. For empty slots (activeEventCount..capacity-1):
//      - Reset button controller color and direct button element color to 0xFFFFFFFF.
//      - Clear the group element's string at +0x58.
//      - Hide image element (turn off visibility flag 1).
//      - Show button element (turn on visibility flag 1).
//
// Key offsets on EventNotifyDialog:
//   +0xB8: parent dialog InternalBasePtr (dereference to get DialogInstance*)
//   +0xC0: float smoothedYOffset
//   +0xC8: notifyEntryArray (stride 0x18: {void* group, void* image, void* button})
//   +0xD0: int32 notifyEntryCapacity (max slots)
//   +0xD8: activeEventArray (stride 0x30, see Slot7 for layout)
//   +0xE0: int32 activeEventCount
//   +0xE8: void* ribbonNotifyElement (the ribbon container)
//
// Active event entry layout (0x30 bytes):
//   +0x00: Time timer (8 bytes)
//   +0x08: int32 event_index (into game event log)
//   +0x0C: (padding)
//   +0x10: Sprite (16 bytes) — icon sprite
//   +0x20: System::String — display text
//   +0x28: uint32 color (ARGB, set from severity in Slot7)
//
// Event log entry layout (0x28 bytes, at gameWorld+0x138 data):
//   +0x18: int32 popup_ref (-1 if no popup, >=0 if popup dialog active)
//
// Original: 0x1400680c0
void EventNotifyDialog::OnUpdateLayout() {
    eventnotify_sa::EnsureGlobals();
    uintptr_t s = (uintptr_t)this;

    // -------------------------------------------------------
    // Step 1-3: Compute target Y and lerp smoothed position
    // -------------------------------------------------------

    // Parent dialog is stored as InternalBasePtr at this+0xB8.
    // CheckPtr validates the InternalBasePtr, then dereference to get the DialogInstance*.
    // DialogInstance::GetRoot() is equivalent to reading *(void**)(dlg + 0x50).
    void* parentDlg  = *(void**)(s + 0xB8);
    if (!parentDlg) return;
    void* parentRoot = *(void**)((uintptr_t)parentDlg + 0x50);
    if (!parentRoot) return;
    int   parentY    = *(int*)((uintptr_t)parentRoot + 0x60);

    // GetRoot on this:
    void* selfRoot = *(void**)(s + 0x50);
    if (!selfRoot) return;
    int   selfY    = *(int*)((uintptr_t)selfRoot + 0x60);

    int targetY = parentY - selfY + 44;  // 0x2c = 44

    // fVar16 = final target based on active count
    float fVar16 = 0.0f;
    int activeCount = *(int*)(s + 0xE0);
    if (activeCount == 1) {
        fVar16 = (float)targetY;
    } else if (activeCount > 1) {
        // Additional entries add 40.0f spacing (DAT_1401721dc = 0x42200000 = 40.0f)
        fVar16 = (float)(activeCount - 1) * 40.0f + (float)targetY;
    }

    // Lerp: smoothedY = smoothedY * 0.8 + fVar16 * 0.2
    // DAT_140172128 = 0.8f, DAT_1401720f8 = 0.2f
    float smoothedY = *(float*)(s + 0xC0);
    smoothedY = smoothedY * 0.8f + fVar16 * 0.2f;
    *(float*)(s + 0xC0) = smoothedY;

    // Apply as top padding on this root: floor(smoothedY + 0.5f)
    // DAT_140172114 = 0.5f (rounding)
    reinterpret_cast<UI::Element*>(selfRoot)->SetPadding(0, (int)(smoothedY + 0.5f), 0, 2);  // B.3.6 bridge

    // -------------------------------------------------------
    // Step 4: Show ribbonNotify element
    // -------------------------------------------------------
    void* gameWorld = *g_pGameWorldSingleton;
    if (!gameWorld) return;
    uintptr_t eventLogPtr = *(uintptr_t*)((uintptr_t)gameWorld + 0x138);
    if (!eventLogPtr) return;  // event log not initialized yet (e.g. during loading)
    uintptr_t eventArray  = *(uintptr_t*)(eventLogPtr + 0x18);
    if (!eventArray) return;

    void* ribbonElem = *(void**)(s + 0xE8);
    if (ribbonElem) reinterpret_cast<UI::Element*>(ribbonElem)->SetFlagsOnOff(0, 1);  // B.3.6 bridge; clear flag 1 = show

    // -------------------------------------------------------
    // Steps 5 & 6: Sync notification slots
    // -------------------------------------------------------
    uintptr_t notifyEntryBase = *(uintptr_t*)(s + 0xC8);  // array of 0x18-byte entries
    uintptr_t activeBase      = *(uintptr_t*)(s + 0xD8);  // array of 0x30-byte entries
    int capacity              = *(int*)(s + 0xD0);
    if (!notifyEntryBase || !activeBase) return;

    // --- Active slots ---
    for (int i = 0; i < activeCount; i++) {
        uintptr_t activeEntry = activeBase + (uintptr_t)i * 0x30;
        uintptr_t slotEntry   = notifyEntryBase + (uintptr_t)i * 0x18;

        void* groupElem  = *(void**)(slotEntry + 0x00);
        void* imageElem  = *(void**)(slotEntry + 0x08);
        void* buttonElem = *(void**)(slotEntry + 0x10);
        if (!buttonElem || !groupElem || !imageElem) continue;

        // Event index stored at active_entry+0x08
        int eventIndex = *(int*)(activeEntry + 0x08);

        // Sprite stored at active_entry+0x10 (16 bytes)
        uint8_t sprite[16];
        *(uint64_t*)(sprite + 0)  = *(uint64_t*)(activeEntry + 0x10);
        *(uint64_t*)(sprite + 8)  = *(uint64_t*)(activeEntry + 0x18);

        // Set icon sprite on image element
        reinterpret_cast<UI::ImageElement*>(imageElem)->SetSprite(reinterpret_cast<const UI::Sprite*>(sprite));  // B.3.6 bridge

        // Get child[4] of button element — may be an ImageElement for embedded icon
        uint8_t childBuf[16];
        void* child4 = (::new (childBuf) Object::InternalPtr<UI::Element>(
            reinterpret_cast<UI::Element*>(buttonElem)->Child(4)))->Get();  // B.3.6 bridge

        // Check if child4 is an ImageElement; if so, copy sprite to it as well
        {
            bool isImage = child4 && fn_IsDerivedFrom(child4, UI::ImageElement::TypeId());
            void* imgChild = isImage ? child4 : nullptr;
            // Construct InternalBasePtr for imgChild on stack
            uint8_t imgChildPtr[16];
            new (imgChildPtr) ::Object::InternalBasePtr(
                reinterpret_cast<RtObject::Internal*>(imgChild));   // B.3.3 bridge
            // Copy same sprite from active_entry+0x10
            uint8_t sprite2[16];
            *(uint64_t*)(sprite2 + 0) = *(uint64_t*)(activeEntry + 0x10);
            *(uint64_t*)(sprite2 + 8) = *(uint64_t*)(activeEntry + 0x18);
            // SetSprite on imgChild (only if non-null after CheckPtr)
            void* imgElemResolved = *(void**)imgChildPtr;
            if (imgElemResolved) {
                reinterpret_cast<UI::ImageElement*>(imgElemResolved)->SetSprite(reinterpret_cast<const UI::Sprite*>(sprite2));  // B.3.6 bridge
            }
        }

        // Determine popup state: check event log entry at eventIndex, popup field at +0x18
        int popupField = *(int*)(eventArray + (uintptr_t)eventIndex * 0x28 + 0x18);
        bool noPopup   = (popupField < 0);

        if (noPopup) {
            // No popup: hide button, show image
            reinterpret_cast<UI::Element*>(buttonElem)->SetFlagsOnOff(1, 0);  // B.3.6 bridge; set flag 1 = hide
            reinterpret_cast<UI::Element*>(imageElem)->SetFlagsOnOff(0, 1);  // B.3.6 bridge; clear flag 1 = show
        } else {
            // Popup active: show button, hide image
            reinterpret_cast<UI::Element*>(buttonElem)->SetFlagsOnOff(0, 1);  // B.3.6 bridge; clear flag 1 = show
            reinterpret_cast<UI::Element*>(imageElem)->SetFlagsOnOff(1, 0);  // B.3.6 bridge; set flag 1 = hide
        }

        // Color from active_entry+0x28
        uint32_t color = *(uint32_t*)(activeEntry + 0x28);

        // Set color on button's controller: buttonElem->vtable[11](retBuf) → controller,
        // then controller->vtable[10](color).
        // vtable[11] = vtable offset 0x58 = GetController / GetPressColor sub-object.
        // vtable[10] = vtable offset 0x50 = SetColor(uint32_t).
        {
            // (slot 11 of every UI::Element table is GetBorder, RETSLOT per B6-2)
            uint8_t ctrlBuf[16];
            void* ctrl = reinterpret_cast<UI::Element*>(buttonElem)->GetBorder(  // B.3.6 bridge
                reinterpret_cast<Object::InternalPtr<UI::Element>*>(ctrlBuf));
            void* ctrlResolved = *(void**)ctrl;
            if (ctrlResolved) {
                reinterpret_cast<UI::Element*>(ctrlResolved)->SetColor(color);  // B.3.6 bridge
            }
        }

        // Also set color on each of the 4 children of the button element.
        // Each child's SetColor is at vtable offset 0x50.
        for (int c = 0; c < 4; c++) {
            uint8_t kidBuf[16];
            void* kid = (::new (kidBuf) Object::InternalPtr<UI::Element>(
                reinterpret_cast<UI::Element*>(buttonElem)->Child(c)))->Get();  // B.3.6 bridge
            void* kidResolved = *(void**)&kid;
            if (!kidResolved) continue;
            reinterpret_cast<UI::Element*>(kidResolved)->SetColor(color);  // B.3.6 bridge
        }

        // Copy event text string to the group element's embedded string at +0x58.
        // activeEntry+0x20 = System::String (display text).
        {
            void* groupResolved = groupElem;
            // B.3.1 bridge: raw exe-struct SystemString fields, not owned here.
            System::String& groupStr = *reinterpret_cast<System::String*>((uintptr_t)groupResolved + 0x58);
            System::String& srcStr   = *reinterpret_cast<System::String*>(activeEntry + 0x20);
            groupStr = srcStr;
        }
    }

    // --- Empty slots (activeCount..capacity-1): reset color, clear text, show empty image ---
    for (int i = activeCount; i < capacity; i++) {
        uintptr_t slotEntry = notifyEntryBase + (uintptr_t)i * 0x18;

        void* groupElem  = *(void**)(slotEntry + 0x00);
        void* imageElem  = *(void**)(slotEntry + 0x08);
        void* buttonElem = *(void**)(slotEntry + 0x10);
        if (!buttonElem || !groupElem || !imageElem) continue;

        // Reset button controller color to 0xFFFFFFFF (white = no tint)
        {
            // (slot 11 of every UI::Element table is GetBorder, RETSLOT per B6-2)
            uint8_t ctrlBuf[16];
            void* ctrl = reinterpret_cast<UI::Element*>(buttonElem)->GetBorder(  // B.3.6 bridge
                reinterpret_cast<Object::InternalPtr<UI::Element>*>(ctrlBuf));
            void* ctrlResolved = *(void**)ctrl;
            if (ctrlResolved) {
                reinterpret_cast<UI::Element*>(ctrlResolved)->SetColor(0xFFFFFFFFu);  // B.3.6 bridge
            }
        }

        // Also reset color on button element itself (direct vtable[10] call)
        {
            reinterpret_cast<UI::Element*>(buttonElem)->SetColor(0xFFFFFFFFu);  // B.3.6 bridge
        }

        // Clear group element's embedded string at +0x58 (assign null = empty string)
        {
            void* groupResolved = groupElem;
            // B.3.1 bridge: raw exe-struct SystemString field, not owned here.
            System::String& groupStr = *reinterpret_cast<System::String*>((uintptr_t)groupResolved + 0x58);
            groupStr = System::String();
        }

        // Show image element (clear flag 1), hide button element (set flag 1).
        // Empty image slots display nothing (no sprite set); button hidden to prevent clicks.
        reinterpret_cast<UI::Element*>(imageElem)->SetFlagsOnOff(0, 1);  // B.3.6 bridge; clear flag = show
        reinterpret_cast<UI::Element*>(buttonElem)->SetFlagsOnOff(1, 0);  // B.3.6 bridge; set flag = hide
    }
}

// ============================================================
// EventNotifyDialog::OnEvent (0x140068680) — Slot8
// ============================================================
// Handles notification button clicks (element IDs 1500..1500+count-1).
// eventType 0x12 = button click, eventData in [0x5DC, 0x5DC+activeEventCount)
// Calls FUN_1400685a0 to navigate to the event location on the map.
//
// Original: 0x140068680.

// ============================================================
// EventNotifyDialog::NavigateToEvent (FUN_1400685a0)
// ============================================================
// Navigates the camera to the map location of the active event at [index].
//
// Steps:
//  1. Get the event log from gameWorld+0x138. Get the event list data at +0x18.
//     Each entry in this list is 0x28 bytes; field +0x18 = tile index (int).
//  2. Look up the event log entry index from the activeEventArray:
//     activeEventArray[index*0x30 + 0x8] = the event log entry index.
//  3. Look up the tile index in the event log entry list.
//  4. Convert tile index to world XY (centered at -0.5f per tile).
//  5. Call fn_MapGetTerrainPos (terrain pick) to get the 3D camera target.
//  6. Write result to cameraTarget (gameWorld+0xe0) at offsets +0x1c/+0x20/+0x24.
//
// Constant: DAT_140172114 = 0.5f (tile center offset).
// Original: 0x1400685a0.
static void EventNotifyDialog_NavigateToEvent(EventNotifyDialog* self, int index) {
    uintptr_t game = (uintptr_t)*g_pGameWorldSingleton;

    // Step 1+2: get event log entry index from active event array
    uintptr_t activeEvents = *(uintptr_t*)((uintptr_t)self + 0xD8);
    int eventLogEntryIdx = *(int*)(activeEvents + 8 + (uintptr_t)index * 0x30);

    // Step 3: look up tile index in event log entry list
    // eventLog = *(gameWorld+0x138); entryList = *(eventLog+0x18); entry = entryList + idx*0x28
    uintptr_t eventLog   = *(uintptr_t*)(game + 0x138);
    uintptr_t entryList  = *(uintptr_t*)(eventLog + 0x18);
    int tileIdx = *(int*)(entryList + 0x18 + (uintptr_t)eventLogEntryIdx * 0x28);

    if (tileIdx < 0) return;  // no valid tile position

    // Step 4: convert tile index to world XY
    // mapGrid = *(gameWorld+0xf8); gridSize = *mapGrid
    int* mapGrid = *(int**)(game + 0xF8);
    int gridSize = *mapGrid;
    float halfSize = (float)(gridSize >> 1) - 0.5f;  // = (gridSize/2) - 0.5f (DAT_140172114)
    float inXY[3] = {
        (float)(tileIdx % gridSize) - halfSize,  // worldX
        (float)(tileIdx / gridSize) - halfSize,  // worldY
        0.0f
    };

    // Step 5: terrain pick — get actual 3D world position
    float outPos[3] = {};
    if (fn_MapGetTerrainPos) {
        fn_MapGetTerrainPos(mapGrid, outPos, inXY, '\0', nullptr);
    } else {
        // Fallback: use 2D position directly (no height)
        outPos[0] = inXY[0];
        outPos[1] = inXY[1];
        outPos[2] = 0.0f;
    }

    // Step 6: write to camera target at gameWorld+0xe0 (+0x1c/+0x20/+0x24)
    uintptr_t camTarget = *(uintptr_t*)(game + 0xE0);
    *(float*)(camTarget + 0x1C) = outPos[0];
    *(float*)(camTarget + 0x20) = outPos[1];
    *(float*)(camTarget + 0x24) = outPos[2];
}

void EventNotifyDialog::OnElementEvent(Object::InternalPtr<UI::Element>* /*element*/, int32_t eventType, int32_t eventData, int32_t /*c*/, int32_t /*d*/, int32_t /*e*/, void* /*userData*/) {
    if (eventType == 0x12) {
        int activeCount = *(int*)((uintptr_t)this + 0xE0);
        if (eventData > 0x5DB && eventData < activeCount + 0x5DC) {
            EventNotifyDialog_NavigateToEvent(this, eventData - 0x5DC);
        }
    }
}

// ============================================================
// STANDALONE EXE: real EventNotifyDialog vtable + ctor + registration
// ============================================================
// EventNotifyDialog (DialogInstance, 0xF0 bytes, exe vtable 0x14015bca0) — the
// notification ribbon near the toolbar. In standalone this was a dormant
// UI_DIALOG_STUB (all slots no-op) so the ribbon never processed or displayed
// events. Promote it to a real per-class vtable + a standalone ctor, following
// the FollowDialog (followdialog.cpp) DialogInstance promotion pattern.
//
// DialogInstance slot semantics (verified against FollowDialog + the exe vtable):
//   +0x00 idx0 ScalarDtor          -> free notify/active arrays + base dtor
//   +0x10 idx2 GetTypeId
//   +0x18 idx3 Persist             -> no-op (serialize thunk)
//   +0x20 idx4 Startup             -> Slot4 (find ribbon elements, build slot array)
//   +0x28 idx5 Shutdown            -> Slot5 (RET 0)
//   +0x30 idx6 OnUpdateLayout      -> Update (position ribbon, sync slot sprites)  [per-frame]
//   +0x38 idx7 OnUpdate            -> Slot7 (scan event log, manage notifications) [per-frame]
//   +0x40 idx8 OnCommand           -> OnEvent (ribbon button clicks -> navigate)
//   +0x48 idx9 OnSerializeSettings -> no-op (serialize thunk)

#include "Object/factory_registration.h"
#include "address_registry.h"
#include <cstddef>
#include <cstdio>
#include <cstdlib>

namespace eventnotify_sa {

// Slot7 dereferences g_pNotifyTimerDuration (raw exe .rdata -> NULL in standalone).
// Lazy-fill so the ribbon tick can't null-deref. The timer duration is a
// byte-faithful 30.0s (exe DAT_1401721cc).
// ENDGAME B.3.9: the TypeId_Dialog / TypeId_ImageElement heals that used to live
// here are dead -- UI::Dialog / UI::ImageElement have a ported TypeId() now (the old
// heal's "External" guess for TypeId_Dialog was wrong too: that cell is UI::Dialog's
// own _typeInfo, engine_funcs.cpp:828, and the heal never actually ran).
void EnsureGlobals() {
    static float s_notifyTimer = 30.0f;
    if (!g_pNotifyTimerDuration) g_pNotifyTimerDuration = &s_notifyTimer;
}

}  // namespace eventnotify_sa

// Retail EventNotifyDialog_CopyCtor (0x140068ab0): DialogInstance base ctor,
// then the ribbon fields. The parent is the toolbar dialog (the ribbon sits just
// below it — see OnUpdateLayout).
//   +0xB8 parent · +0xC0 smoothedY=0 · +0xC4 lastEventIdx=-1 · +0xC8..+0xE8 = 0
EventNotifyDialog::EventNotifyDialog(UI::UserInterface* ui, const UI::Dialog* dialog,
                                     Object::InternalPtr<UI::DialogInstance> parent)
    : UI::DialogInstance(ui, dialog, parent, nullptr, nullptr, 0) {
    dataB8 = *reinterpret_cast<const uint64_t*>(&parent);
    dataC0 = 0;
    dataC4 = -1;
    dataC8 = 0; dataD0 = 0;
    dataD8 = 0; dataE0 = 0;
    dataE8 = 0;
}

// The body of the scalar deleting dtor 0x140011550 before ~DialogInstance: free
// the +0xD8 active-event array (String at each entry +0x20), free the +0xC8
// slot array, clear both headers.
EventNotifyDialog::~EventNotifyDialog() {
    uintptr_t s = (uintptr_t)this;
    void* activeArr = *(void**)(s + 0xD8);
    int activeCnt = *(int*)(s + 0xE0);
    if (activeArr && activeCnt > 0) {
        uintptr_t e = (uintptr_t)activeArr;
        // B.3.1 bridge: raw exe-struct SystemString field, not owned here.
        for (int i = 0; i < activeCnt; i++, e += 0x30) reinterpret_cast<System::String*>(e + 0x20)->~String();
    }
    if (fn_MemFreeStatic) { fn_MemFreeStatic(activeArr); fn_MemFreeStatic(*(void**)(s + 0xC8)); }
    *(uint64_t*)(s + 0xC8) = 0; *(uint64_t*)(s + 0xD0) = 0;
    *(uint64_t*)(s + 0xD8) = 0; *(uint64_t*)(s + 0xE0) = 0;
}

// [0] 0x140011550: the body above, ~DialogInstance, MemFree on &1.
Object::Base* EventNotifyDialog::DeletingDtor(unsigned flags) {
    this->~EventNotifyDialog();
    if (flags & 1) Object::detail::InternalMemFree(this);
    return this;
}

// create 0x140011500: MemAlloc(0xf0, 8), then the ctor.
void* EventNotifyDialog::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(EventNotifyDialog), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(::new (p) EventNotifyDialog());
}
// createInPlace 0x140011540 (the tree's in-place build is kept, B8-13).
void* EventNotifyDialog::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(::new (storage) EventNotifyDialog());
}

DEFINE_EXE_VTABLE(EventNotifyDialog, "_ZTV17EventNotifyDialog")
DEFINE_EXE_TYPE(EventNotifyDialog, "UI::DialogInstance")

// The ctor DOOR, reached by address: reg_code_lane2.cpp registers it at
// 0x140068ab0 and GameUpdateLoop::Startup calls it to build the ribbon.
extern "C" void* EventNotifyDialog_Ctor_Standalone(void* self, void* ui, void* extRes, void* parentIBP) {
    if (!self) return nullptr;
    Object::InternalPtr<UI::DialogInstance> parent;
    if (parentIBP)
        parent = *static_cast<const Object::InternalPtr<UI::DialogInstance>*>(parentIBP);
    return ::new (self) EventNotifyDialog(static_cast<UI::UserInterface*>(ui),
                                          static_cast<const UI::Dialog*>(extRes), parent);
}
