// code/Game/Game/game.cpp
// Clean reimpl of the exe-internal Game/KeyMap container ctor.
//
// Original: FUN_140001050 (Application-x64.exe).
// Decompiled body (paraphrased):
//   - zero param_1[0..0x14] (21 qwords)
//   - UI::Sprite::Sprite(param_1 + 0x15)   // 16 bytes at slot 0x15
//   - UI::Sprite::Sprite(param_1 + 0x17)   // 16 bytes at slot 0x17
//   - param_1[0x19] = param_1[0x1a] = param_1[0x1b] = 0
//   - g_pKeyMapSingleton = param_1
//
// Total struct size: 0x1c qwords = 0xe0 bytes. The slot index 0x15..0x18 holds
// two 16-byte UI::Sprite values.

#include "game.h"
#include "engine_funcs.h"
#include "Component/StorageComponent.h"   // ENDGAME B.3.9 TR
#include "Component/RawMaterialDescription.h"   // ENDGAME B.3.9 TR
#include "Component/SelectDescription.h"   // ENDGAME B.3.9 TR
#include "Component/ComponentDescription.h"   // ENDGAME B.3.9 TR
#include <cstdint>
#include <cstring>
#include <new>
#include "ui/Sprite.h"   // ENDGAME B.3.6 UI callers (K4)

extern "C" void* Game_Ctor(void* self) {
    // Zero the entire 0xe0-byte struct, including the regions where the
    // sprites will be initialized — Sprite default ctor is itself a 16-byte
    // zero-init in the engine, so this is safe.
    std::memset(self, 0, 0xe0);

    // Two embedded UI::Sprite default ctors at slot 0x15 and slot 0x17.
    auto base = reinterpret_cast<uint64_t*>(self);
    ::new (&base[0x15]) UI::Sprite();   // B.3.6: value-type ctor in place (B6-10)
    ::new (&base[0x17]) UI::Sprite();   // B.3.6: value-type ctor in place (B6-10)

    // Publish to global KeyMap singleton storage (DAT_1401c3de8).
    if (g_pKeyMapSingleton) {
        *g_pKeyMapSingleton = self;
    }
    return self;
}

// moved from engine_funcs.h (ENDGAME B.3.9 TR): reads X::TypeId() of the cell's class.
void Game_TriggerDisasterByType_Impl(int disasterType) {
    if (!g_pExternalFactorySingleton || !*g_pExternalFactorySingleton) return;
    if (!fn_IsDerivedFrom) return;

    void* factory = *g_pExternalFactorySingleton;
    void* gameWorld = *g_pGameWorldSingleton;
    if (!gameWorld) return;
    void* entityList = *(void**)((uintptr_t)gameWorld + 0xf0);

    // Iterate all ComponentDescription-type externals
    const ::Object::ExternalFactory* efactory =
        reinterpret_cast<const ::Object::ExternalFactory*>(factory);   // B.3.3 bridge
    void* ext = const_cast<void*>(static_cast<const void*>(
        efactory->FindNext(ComponentDescription::TypeId(), nullptr)));
    while (ext != nullptr) {
        uintptr_t e = (uintptr_t)ext;
        if (fn_IsDerivedFrom(ext, ComponentDescription::TypeId())) {
            // Find SelectDescription sub-component
            int compCount = *(int*)(e + 0x30);
            uintptr_t compArray = *(uintptr_t*)(e + 0x28);

            // First pass: check if entity HAS a SelectDescription
            bool hasSelect = false;
            for (int i = 0; i < compCount; i++) {
                void* comp = *(void**)(compArray + (uintptr_t)i * 8);
                typedef uint32_t (*GetTypeId_t)(void*);
                uint32_t typeId = ((GetTypeId_t)(*(uintptr_t*)(*(uintptr_t*)comp + 0x10)))(comp);
                if (typeId == SelectDescription::TypeId()) {
                    if (fn_IsDerivedFrom(comp, SelectDescription::TypeId()) && comp != nullptr) {
                        hasSelect = true;
                    }
                    break;
                }
            }

            if (hasSelect) {
                // Second pass: find the SelectDescription and check type
                void* selectDesc = nullptr;
                for (int i = 0; i < compCount; i++) {
                    void* comp = *(void**)(compArray + (uintptr_t)i * 8);
                    typedef uint32_t (*GetTypeId_t)(void*);
                    uint32_t typeId = ((GetTypeId_t)(*(uintptr_t*)(*(uintptr_t*)comp + 0x10)))(comp);
                    if (typeId == SelectDescription::TypeId()) {
                        if (fn_IsDerivedFrom(comp, SelectDescription::TypeId())) {
                            selectDesc = comp;
                        }
                        break;
                    }
                }
                // Check if SelectDescription.type (+0x40) matches requested disaster type
                if (selectDesc && *(int*)((uintptr_t)selectDesc + 0x40) == disasterType) {
                    Game_RemoveEntity_Impl(entityList, ext);
                }
            }
        }
        ext = const_cast<void*>(static_cast<const void*>(
            efactory->FindNext(ComponentDescription::TypeId(),
                               reinterpret_cast<const ::Object::External*>(ext))));
    }
}

void Game_SpawnResources_Impl(void* citizenListPtr, uint32_t resourceFlag, int count) {
    int64_t* param_1 = (int64_t*)citizenListPtr;

    // Dynamic array for matching entities
    // Ghidra uses local_48 = array ptr, local_40 = packed CONCAT44(capacity, count)
    void* matchArr = nullptr;
    int matchCount = 0;
    int matchCapacity = 0;

    // Phase 1: Build array of entity pointers whose NaturalResourceComponent flags match resourceFlag
    // Entity array is raw pointers: param_1[0x22] = array ptr, param_1[0x23] = count
    if ((int)param_1[0x23] > 0) {
        int64_t entityOffset = 0;
        int entityIdx = 0;
        do {
            int64_t entity = *(int64_t*)(entityOffset + param_1[0x22]);

            // Find NaturalResourceComponent on this entity (raw component array at +0x28/+0x30)
            void* natResComp = nullptr;
            int compIdx = 0;
            int64_t compOffset = 0;
            if (*(int*)(entity + 0x30) > 0) {
                do {
                    void* comp = *(void**)(*(int64_t*)(entity + 0x28) + compOffset * 8);
                    // vtable call: GetTypeId at vtable+0x10
                    typedef uint32_t (*GetTypeId_t)(void*);
                    uint32_t typeId = ((GetTypeId_t)(*(uintptr_t*)(*(uintptr_t*)comp + 0x10)))(comp);
                    if (typeId == RawMaterialDescription::TypeId()) {
                        void* foundComp = *(void**)(*(int64_t*)(entity + 0x28) + compOffset * 8);
                        bool isDerived = fn_IsDerivedFrom(foundComp, RawMaterialDescription::TypeId());
                        natResComp = nullptr;
                        if (isDerived) {
                            natResComp = foundComp;
                        }
                        goto phase1_found;
                    }
                    compIdx++;
                    compOffset++;
                } while (compIdx < *(int*)(entity + 0x30));
            }
            natResComp = nullptr;

        phase1_found:
            {
                // Check resource flags match (natResComp+0x5c & resourceFlag)
                // Original code does NOT null-check — relies on entities always having the component
                void* oldArr = matchArr;
                int oldCount = matchCount;
                if ((*(uint32_t*)((uintptr_t)natResComp + 0x5c) & resourceFlag) != 0) {
                    int newCount = matchCount + 1;
                    if (newCount != matchCount) {  // overflow guard (matches original JZ)
                        matchCount = newCount;
                        if (matchCapacity < newCount) {
                            // Growth: if first alloc, capacity = newCount; else capacity = newCount * 3
                            matchCapacity = newCount;
                            if (oldArr != nullptr) {
                                matchCapacity = newCount * 3;
                            }
                            matchArr = fn_MemAlloc(g_pMemAllocSingleton, (uint64_t)matchCapacity * 8, 8);
                        }

                        // Copy old entries + append new entity
                        // If matchArr == oldArr (no realloc needed), start from old count (just append)
                        // If matchArr != oldArr (reallocated), start from 0 (copy all + append)
                        uint32_t copyStart = 0;
                        if (oldArr == matchArr) {
                            copyStart = (uint32_t)oldCount;
                        }
                        int copyIdx = (int)copyStart;
                        if (copyIdx < newCount) {
                            int64_t* dstSlot = (int64_t*)((int64_t)matchArr + (int64_t)copyIdx * 8);
                            int64_t* srcSlot = (int64_t*)((int64_t)oldArr + (int64_t)copyIdx * 8);
                            do {
                                if (dstSlot != nullptr) {
                                    int64_t* source = &entity;
                                    if ((int)copyStart < oldCount) {
                                        source = srcSlot;
                                    }
                                    *dstSlot = *source;
                                }
                                copyStart++;
                                srcSlot++;
                                dstSlot++;
                            } while ((int)copyStart < newCount);
                        }

                        // Free old array if it was reallocated
                        if (oldArr != matchArr) {
                            fn_MemFreeStatic(oldArr);
                        }
                    }
                }
            }

            entityIdx++;
            entityOffset += 8;
        } while (entityIdx < (int)param_1[0x23]);

        // Phase 2: Round-robin distribute items across citizens' StorageComponents
        // Citizens are InternalBasePtr entries: param_1[0] = array ptr, param_1[1] = count
        if (matchCount > 0 && count > 0) {
            int spawned = 0;

            while (true) {
                bool anyAdded = false;
                int citizenIdx = 0;
                if ((int)param_1[1] < 1) break;

                int64_t citizenOffset = 0;
                do {
                    // Each citizen entry is an InternalBasePtr — load then CheckPtr
                    int64_t citizenIBP = *(int64_t*)(citizenOffset + *param_1);
                    int64_t citizen = citizenIBP;

                    // Find StorageComponent on citizen
                    // Citizen component array is IBP entries at citizen+0x30, count at citizen+0x38
                    void* storageComp = nullptr;
                    int cCompIdx = 0;
                    int64_t cCompOffset = 0;
                    if (*(int*)(citizen + 0x38) > 0) {
                        do {
                            // Each component entry is an InternalBasePtr — CheckPtr before reading
                            void* compIBP = (void*)(*(int64_t*)(citizen + 0x30) + cCompOffset * 8);

                            typedef uint32_t (*GetTypeId_t)(void*);
                            uint32_t typeId = ((GetTypeId_t)(*(uintptr_t*)(*(uintptr_t*)(*(int64_t*)compIBP) + 0x10)))(*(void**)compIBP);
                            if (typeId == StorageComponent::TypeId()) {
                                // Re-CheckPtr the IBP slot and read again
                                void* compIBP2 = (void*)(*(int64_t*)(citizen + 0x30) + cCompOffset * 8);
                                void* compVal = *(void**)compIBP2;

                                bool isDerived = fn_IsDerivedFrom(compVal, StorageComponent::TypeId());
                                storageComp = nullptr;
                                if (isDerived) {
                                    storageComp = compVal;
                                }
                                goto phase2_found;
                            }
                            cCompIdx++;
                            cCompOffset++;
                        } while (cCompIdx < *(int*)(citizen + 0x38));
                    }
                    storageComp = nullptr;

                phase2_found:
                    {
                        // Wrap found storage component in an InternalBasePtr
                        int64_t storageIBP = 0;
                        new (&storageIBP) ::Object::InternalBasePtr(
                            reinterpret_cast<RtObject::Internal*>(storageComp));   // B.3.3 bridge

                        // Try to add one item via round-robin
                        if (spawned < count) {
                            int64_t* matchSlot = (int64_t*)((int64_t)matchArr + (int64_t)(spawned % matchCount) * 8);

                            // CheckPtr the storage IBP before use

                            // Call original exe CanAdd function (0x14012cd80)
                            uint32_t canAdd = fn_StorageCanAdd((void*)storageIBP, (void*)*matchSlot, 1);
                            if ((char)canAdd != '\0') {
                                // CheckPtr again before AddItem

                                // Call original exe AddItem function (0x14012d000)
                                fn_StorageAddItem((void*)storageIBP, (void*)*matchSlot, 1);
                                spawned++;
                                anyAdded = true;
                            }
                        }
                    }

                    citizenIdx++;
                    citizenOffset += 8;
                } while (citizenIdx < (int)param_1[1]);

                if (!anyAdded || spawned >= count) break;
            }
        }
    }

    // Free match array
    fn_MemFreeStatic(matchArr);
}
