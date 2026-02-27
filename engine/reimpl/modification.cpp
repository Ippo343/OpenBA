// engine/reimpl/modification.cpp
// Reimplementation of IO::Modification methods from Runtime-x64.dll.
//
// The mod management system handles .pkm packages: enable/disable, ordering,
// discovery, and filesystem mounting. This file reimplements the simpler
// methods that don't depend on Persistence, FileSystem, or PackageFile.
//
// Methods reimplemented:
//   - Find          (0x1800aea10) — linear search mods by filename (case-insensitive)
//   - Enable        (0x1800aeab0) — toggle a mod's enabled state
//   - DisableAll    (0x1800b05b0) — disable all mods
//
// Methods NOT reimplemented (complex dependencies):
//   - Swap              — needs internal Mod copy/destruct helpers
//   - EnableSavingSettings — needs RefreshList + array copy with refcounting
//   - RefreshList        — needs Persistence, FileSystem, PackageFile
//   - SaveList           — needs Persistence
//   - SetupFileSystem    — needs FileSystem, PackageFile

#include "Modification.h"
#include "hooks.h"
#include "resolve.h"
#include <cstdio>
#include <cstdint>
#include <cstring>

// ============================================================
// Hook state
// ============================================================

static HookInfo g_hookFind;
static HookInfo g_hookEnable;
static HookInfo g_hookDisableAll;

// ============================================================
// Find (0x1800aea10)
// Linear search through the mod array by filename (case-insensitive).
// Compares string lengths first as a fast-path reject, then _stricmp.
//
// Parameters: RCX = this, RDX = const Filename& (8-byte String ptr)
// Returns: index of matching mod, or -1 if not found
// ============================================================

static int Reimpl_Find(const Modification* self, const void* filename) {
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
// Original is only 20 bytes (inline, no frame):
//   mov rax, [rcx+8]       ; m_mods pointer
//   movsxd rdx, edx        ; sign-extend index
//   mov byte ptr [rcx], 1  ; m_dirty = true
//   shl rdx, 7             ; index * 0x80
//   mov [rdx+rax+0x50], r8b ; m_mods[i].m_enabled = enabled
//
// Parameters: RCX = this, EDX = index, R8B = enabled (bool)
// ============================================================

static void Reimpl_Enable(Modification* self, int index, bool enabled) {
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
// Original loops through m_mods array clearing m_enabled at +0x50.
// Note: trampoline passthrough is broken (relative jle in prologue)
// but we never call it — full reimplementation.
//
// Parameters: RCX = this
// ============================================================

static void Reimpl_DisableAll(Modification* self) {
#ifdef _DEBUG
    printf("[Modification::DisableAll] disabling %d mods\n", self->m_count);
#endif

    for (int i = 0; i < self->m_count; i++) {
        self->m_mods[i].m_enabled = 0;
    }
    self->m_dirty = 1;
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
