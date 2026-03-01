// engine/reimpl/modification.cpp
// Reimplementation of IO::Modification methods from Runtime-x64.dll.
//
// The mod management system handles .pkm packages: enable/disable, ordering,
// discovery, and filesystem mounting.
//
// Methods fully reimplemented:
//   - Find          (0x1800aea10) — linear search mods by filename (case-insensitive)
//   - Enable        (0x1800aeab0) — toggle a mod's enabled state
//   - DisableAll    (0x1800b05b0) — disable all mods
//   - Swap          (0x1800aeb20) — swap two mod entries by index
//   - SaveList      (0x1800b05f0) — persist mod state to Persistence
//
// Methods hooked (passthrough wrapper with logging):
//   - EnableSavingSettings (0x1800aead0) — enable persistence, calls RefreshList
//   - RefreshList          (0x1800af5f0) — scan filesystem for .pkm packages
//   - SetupFileSystem      (0x1800aebb0) — mount mod packages into FileSystem

#include "Modification.h"
#include "hooks.h"
#include "resolve.h"
#include "engine_funcs.h"
#include <cstdio>
#include <cstdint>
#include <cstring>

// ============================================================
// Hook state
// ============================================================

static HookInfo g_hookFind;
static HookInfo g_hookEnable;
static HookInfo g_hookDisableAll;
static HookInfo g_hookSwap;
static HookInfo g_hookEnableSavingSettings;
static HookInfo g_hookSaveList;
static HookInfo g_hookRefreshList;
static HookInfo g_hookSetupFileSystem;

// ============================================================
// Find (0x1800aea10)
// Linear search through the mod array by filename (case-insensitive).
// Compares string lengths first as a fast-path reject, then _stricmp.
//
// Parameters: RCX = this, RDX = const Filename& (8-byte String ptr)
// Returns: index of matching mod, or -1 if not found
// ============================================================

static int Reimpl_Find(const Modification* self, const void* filename) {
    g_lastHook = "Modification::Find";
    // Get target filename's string data (System::Filename is same layout as String)
    const char* targetData = *reinterpret_cast<const char* const*>(filename);
    int targetLen = targetData ? *reinterpret_cast<const int32_t*>(targetData - 8) : 0;

#ifdef _DEBUG
    static int findCount = 0;
    if (findCount < 10) {
        printf("[Modification::Find] searching for \"%s\" (len=%d) in %d mods\n",
               targetData ? targetData : "(null)", targetLen, self->m_count);
        findCount++;
    }
#endif

    for (int i = 0; i < self->m_count; i++) {
        // m_filename is at offset +0x58 in each 0x80-byte Mod entry
        const char* modPath = reinterpret_cast<const char*>(self->m_mods[i].m_filename);
        int modLen = modPath ? *reinterpret_cast<const int32_t*>(modPath - 8) : 0;

        // Fast reject: skip if lengths differ
        if (modLen == targetLen && _stricmp(modPath, targetData) == 0) {
#ifdef _DEBUG
            if (findCount <= 10)
                printf("[Modification::Find] found at index %d\n", i);
#endif
            return i;
        }
    }

    return -1;
}

// ============================================================
// Enable (0x1800aeab0)
// Sets a mod's enabled flag and marks the modification list as dirty.
//
// Parameters: RCX = this, EDX = index, R8B = enabled (bool)
// ============================================================

static void Reimpl_Enable(Modification* self, int index, bool enabled) {
    g_lastHook = "Modification::Enable";
#ifdef _DEBUG
    printf("[Modification::Enable] index=%d enabled=%d (count=%d)\n",
           index, enabled, self->m_count);
#endif

    self->m_mods[index].m_enabled = enabled ? 1 : 0;
    self->m_dirty = 1;
}

// ============================================================
// DisableAll (0x1800b05b0)
// Clears the enabled flag on every mod and marks dirty.
//
// Note: trampoline passthrough is broken (relative jle in prologue)
// but we never call it — full reimplementation.
//
// Parameters: RCX = this
// ============================================================

static void Reimpl_DisableAll(Modification* self) {
    g_lastHook = "Modification::DisableAll";
#ifdef _DEBUG
    printf("[Modification::DisableAll] disabling %d mods\n", self->m_count);
#endif

    for (int i = 0; i < self->m_count; i++) {
        self->m_mods[i].m_enabled = 0;
    }
    self->m_dirty = 1;
}

// ============================================================
// Swap (0x1800aeb20) — FULL REIMPLEMENTATION
// Swaps two mod entries by index using copy-construct/assign/destruct.
// Uses resolved Mod lifecycle functions for proper refcount handling.
//
// Parameters: RCX = this, EDX = index1, R8D = index2
// ============================================================

static void Reimpl_Swap(Modification* self, int idx1, int idx2) {
    g_lastHook = "Modification::Swap";
#ifdef _DEBUG
    printf("[Modification::Swap] idx1=%d idx2=%d (count=%d)\n",
           idx1, idx2, self->m_count);
#endif

    // Stack-local temp Mod (0x80 bytes, zero-initialized for safety)
    uint8_t temp[0x80] = {};

    // temp = mods[idx1]  (copy construct — increments refcounts on strings/arrays)
    fn_ModEntry_CopyCtor(temp, &self->m_mods[idx1]);

    // mods[idx1] = mods[idx2]  (assign — handles refcount transfer)
    fn_ModEntry_Assign(&self->m_mods[idx1], &self->m_mods[idx2]);

    // mods[idx2] = temp  (assign — handles refcount transfer)
    fn_ModEntry_Assign(&self->m_mods[idx2], reinterpret_cast<Mod*>(temp));

    // Destroy temp (decrements refcounts, frees arrays if last ref)
    fn_ModEntry_Dtor(temp);

    self->m_dirty = 1;
}

// ============================================================
// EnableSavingSettings (0x1800aead0) — PASSTHROUGH WRAPPER
// Conditionally enables/disables persistence saving.
// If enabling (and not already enabled): calls RefreshList, sets dirty.
// If disabling (and not already disabled): copies mods to saved, sets dirty.
//
// Parameters: RCX = this, DL = enable (bool)
// ============================================================

typedef void (*Modification_EnableSavingSettings_t)(Modification* self, bool enable);

static void Reimpl_EnableSavingSettings(Modification* self, bool enable) {
    g_lastHook = "Modification::EnableSavingSettings";
#ifdef _DEBUG
    printf("[Modification::EnableSavingSettings] self=%p count=%d enable=%d\n",
           (void*)self, self->m_count, (int)enable);
#endif

    // Call original implementation via trampoline (must forward both params)
    ((Modification_EnableSavingSettings_t)g_hookEnableSavingSettings.trampoline)(self, enable);

#ifdef _DEBUG
    printf("[Modification::EnableSavingSettings] done, savingEnabled=%d savedCount=%d\n",
           self->m_savingEnabled, self->m_savedCount);
#endif
}

// ============================================================
// SaveList (0x1800b05f0) — FULL REIMPLEMENTATION
// Saves the mod list state (enabled flags, sort order) to Persistence.
// If saving is not enabled (m_savingEnabled == 0), copies m_mods to
// m_savedMods as a backup. Otherwise persists each mod's enabled
// state and sort index as "ModEnabled_<name>" / "ModIndex_<name>".
//
// Parameters: RCX = this
// ============================================================

static void Reimpl_SaveList(Modification* self) {
    g_lastHook = "Modification::SaveList";
#ifdef _DEBUG
    printf("[Modification::SaveList] self=%p count=%d dirty=%d savingEnabled=%d\n",
           (void*)self, self->m_count, self->m_dirty, self->m_savingEnabled);
#endif

    if (!self->m_savingEnabled) {
        // Copy m_mods array to m_savedMods (deep copy with refcount management)
        // Both arrays share the same contiguous layout: [Mod*, count, capacity]
        fn_ModArrayCopy(&self->m_savedMods, &self->m_mods);
    } else {
        void* persistence = *g_pPersistenceSingleton;

        for (int i = 0; i < self->m_count; i++) {
            Mod* mod = &self->m_mods[i];

            // Set sort index to current position
            mod->m_sortIndex = i;

            // Get filename without path or extension (e.g., "MyMod" from "data/mods/MyMod.pkm")
            // Member non-trivial return: RCX=self(&m_filename), RDX=retval, R8B=true
            void* pathFilename = nullptr;
            fn_Filename_GetPathFilename(&mod->m_filename, &pathFilename, true);

            // The raw C string data for use as key suffix
            const char* nameStr = (const char*)pathFilename;

            // Persist "ModEnabled_<name>" = true/false
            fn_Persistence_SetBool(persistence, "ModEnabled_", nameStr, (bool)mod->m_enabled);

            // Persist "ModIndex_<name>" = sort index
            fn_Persistence_SetInt(persistence, "ModIndex_", nameStr, mod->m_sortIndex);

            // Destroy temp filename string
            fn_String_Dtor(&pathFilename);

#ifdef _DEBUG
            if (i < 5) {
                printf("[Modification::SaveList]   [%d] \"%s\" enabled=%d\n",
                       i, nameStr ? nameStr : "(null)", mod->m_enabled);
            }
#endif
        }
    }

#ifdef _DEBUG
    printf("[Modification::SaveList] done\n");
#endif
}

// ============================================================
// RefreshList (0x1800af5f0) — PASSTHROUGH WRAPPER
// Scans the filesystem for .pkm mod packages, rebuilds the mod list.
// Complex: ~3600 bytes, touches FileSystem, PackageFile, Persistence,
// BST operations, and string processing.
//
// Parameters: RCX = this
// ============================================================

typedef void (*Modification_RefreshList_t)(Modification* self);

static void Reimpl_RefreshList(Modification* self) {
    g_lastHook = "Modification::RefreshList";
#ifdef _DEBUG
    printf("[Modification::RefreshList] self=%p count=%d\n",
           (void*)self, self->m_count);
#endif

    // Call original implementation via trampoline
    ((Modification_RefreshList_t)g_hookRefreshList.trampoline)(self);

#ifdef _DEBUG
    printf("[Modification::RefreshList] done, count=%d\n", self->m_count);
    for (int i = 0; i < self->m_count && i < 5; i++) {
        const char* name = reinterpret_cast<const char*>(self->m_mods[i].m_name);
        const char* path = reinterpret_cast<const char*>(self->m_mods[i].m_filename);
        printf("[Modification::RefreshList]   [%d] \"%s\" enabled=%d path=\"%s\"\n",
               i, name ? name : "(null)", self->m_mods[i].m_enabled,
               path ? path : "(null)");
    }
    if (self->m_count > 5)
        printf("[Modification::RefreshList]   ... and %d more\n", self->m_count - 5);
#endif
}

// ============================================================
// SetupFileSystem (0x1800aebb0) — PASSTHROUGH WRAPPER
// Mounts enabled mod packages into the engine's FileSystem.
// Complex: ~2600 bytes, iterates mods, closes/loads PackageFiles,
// interacts with ExternalFactory.
//
// Parameters: RCX = this
// ============================================================

typedef void (*Modification_SetupFileSystem_t)(Modification* self);

static void Reimpl_SetupFileSystem(Modification* self) {
    g_lastHook = "Modification::SetupFileSystem";
#ifdef _DEBUG
    printf("[Modification::SetupFileSystem] self=%p count=%d\n",
           (void*)self, self->m_count);
    int enabledCount = 0;
    for (int i = 0; i < self->m_count; i++)
        if (self->m_mods[i].m_enabled) enabledCount++;
    printf("[Modification::SetupFileSystem] %d mods enabled\n", enabledCount);
#endif

    // Call original implementation via trampoline
    ((Modification_SetupFileSystem_t)g_hookSetupFileSystem.trampoline)(self);

#ifdef _DEBUG
    printf("[Modification::SetupFileSystem] done\n");
#endif
}

// ============================================================
// Install / Remove
// ============================================================

void InstallModificationFind() {
    uintptr_t target = ResolveAddr(Module::Runtime, ModificationAddr::Find);
    // Prologue: mov [rsp+8],rbx(5) + push rbp(1) + push rsi(1) + push rdi(1) + ... = 17 bytes safe
    InstallHook(target, (void*)Reimpl_Find, &g_hookFind, 17);
#ifdef _DEBUG
    printf("[Modification] Find hooked at %p (prologue=17)\n", (void*)target);
#endif
}
void RemoveModificationFind() { RemoveHook(&g_hookFind); }

void InstallModificationEnable() {
    uintptr_t target = ResolveAddr(Module::Runtime, ModificationAddr::Enable);
    // 20-byte inline function, no frame. Instruction boundary at 14 bytes
    // (mov rax,[rcx+8]=4 + movsxd rdx,edx=3 + mov [rcx],1=3 + shl rdx,7=4 = 14)
    InstallHook(target, (void*)Reimpl_Enable, &g_hookEnable, 14);
#ifdef _DEBUG
    printf("[Modification] Enable hooked at %p (prologue=14)\n", (void*)target);
#endif
}
void RemoveModificationEnable() { RemoveHook(&g_hookEnable); }

void InstallModificationDisableAll() {
    uintptr_t target = ResolveAddr(Module::Runtime, ModificationAddr::DisableAll);
    // No frame setup. jle at byte 5 makes trampoline broken, but we don't call it.
    // Instruction boundary at 16 bytes (xor=2 + cmp=3 + jle=2 + mov=3 + nop=6 = 16)
    InstallHook(target, (void*)Reimpl_DisableAll, &g_hookDisableAll, 16);
#ifdef _DEBUG
    printf("[Modification] DisableAll hooked at %p (prologue=16)\n", (void*)target);
#endif
}
void RemoveModificationDisableAll() { RemoveHook(&g_hookDisableAll); }

void InstallModificationSwap() {
    uintptr_t target = ResolveAddr(Module::Runtime, ModificationAddr::Swap);
    // Prologue=20: unusual layout, 9-byte MOV spans offset 11-19.
    // Trampoline is NOT usable (may contain RIP-relative), but we never call it.
    InstallHook(target, (void*)Reimpl_Swap, &g_hookSwap, 20);
#ifdef _DEBUG
    printf("[Modification] Swap hooked at %p (prologue=20)\n", (void*)target);
#endif
}
void RemoveModificationSwap() { RemoveHook(&g_hookSwap); }

void InstallModificationEnableSavingSettings() {
    uintptr_t target = ResolveAddr(Module::Runtime, ModificationAddr::EnableSavingSettings);
    // Prologue: standard 13 bytes (MOV+PUSH+SUB)
    InstallHook(target, (void*)Reimpl_EnableSavingSettings, &g_hookEnableSavingSettings, 13);
#ifdef _DEBUG
    printf("[Modification] EnableSavingSettings hooked at %p (prologue=13)\n", (void*)target);
#endif
}
void RemoveModificationEnableSavingSettings() { RemoveHook(&g_hookEnableSavingSettings); }

void InstallModificationSaveList() {
    uintptr_t target = ResolveAddr(Module::Runtime, ModificationAddr::SaveList);
    // Prologue: standard 13 bytes (MOV+PUSH+SUB)
    InstallHook(target, (void*)Reimpl_SaveList, &g_hookSaveList, 13);
#ifdef _DEBUG
    printf("[Modification] SaveList hooked at %p (prologue=13)\n", (void*)target);
#endif
}
void RemoveModificationSaveList() { RemoveHook(&g_hookSaveList); }

void InstallModificationRefreshList() {
    uintptr_t target = ResolveAddr(Module::Runtime, ModificationAddr::RefreshList);
    // Prologue: standard 13 bytes (MOV+PUSH+SUB)
    InstallHook(target, (void*)Reimpl_RefreshList, &g_hookRefreshList, 13);
#ifdef _DEBUG
    printf("[Modification] RefreshList hooked at %p (prologue=13)\n", (void*)target);
#endif
}
void RemoveModificationRefreshList() { RemoveHook(&g_hookRefreshList); }

void InstallModificationSetupFileSystem() {
    uintptr_t target = ResolveAddr(Module::Runtime, ModificationAddr::SetupFileSystem);
    // Prologue: standard 13 bytes (MOV+PUSH+SUB)
    InstallHook(target, (void*)Reimpl_SetupFileSystem, &g_hookSetupFileSystem, 13);
#ifdef _DEBUG
    printf("[Modification] SetupFileSystem hooked at %p (prologue=13)\n", (void*)target);
#endif
}
void RemoveModificationSetupFileSystem() { RemoveHook(&g_hookSetupFileSystem); }
