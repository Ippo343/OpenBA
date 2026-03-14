// spritesheet_debug.cpp — Debug wrappers for UI::SpriteSheet functions.
//
// Logs sprite name lookups through GetSpriteIndexFromName to diagnose
// missing/wrong sprite icons (e.g., BuildRequirementUI).
//
// The core insight from decompilation: GetSpriteIndexFromName (0x1800031f0)
// does a LINEAR SCAN comparing System::String contents case-sensitively.
// Returns -1 on not found. This wrapper logs every lookup so we can see
// exactly what names are being queried and whether they resolve.
//
// Usage: Call SpriteSheet_Debug_Init() after engine_funcs are resolved.
//        Then call the Debug_ wrappers instead of the raw function pointers.
//
// All logging is guarded by #ifdef _DEBUG so release builds have zero overhead.

#include "../../../engine/funcs/engine_funcs.h"
#include "../../../runtime/types/ui/SpriteSheet.h"
#include "../../../engine/types/SystemString.h"

#include <cstdio>

// ============================================================
// Debug wrapper: GetSpriteIndexFromName
// ============================================================
// Calls the real function and logs the lookup name + result.
// This is THE most important diagnostic — 47 xrefs call this
// function and any silent -1 return means a missing sprite icon.
//
// param sheet: SpriteSheet* (this pointer)
// param name:  System::String const& (8-byte struct with char* m_data)
// returns:     sprite index (0-based) or -1 if not found
int SpriteSheet_Debug_GetSpriteIndexFromName(void* sheet, void* name) {
    int result = -1;

    if (fn_SpriteSheet_GetSpriteIndexFromName) {
        result = fn_SpriteSheet_GetSpriteIndexFromName(sheet, name);
    }

#ifdef _DEBUG
    // Dereference System::String: the struct is just a char* pointer
    // name is passed as System::String const& — so name points to a SystemString
    const char* nameStr = nullptr;
    int nameLen = 0;
    if (name) {
        const SystemString* sysStr = reinterpret_cast<const SystemString*>(name);
        nameStr = StringGetData(sysStr);
        nameLen = StringGetLength(sysStr);
    }

    // Get sheet info for context
    int spriteCount = 0;
    const char* sheetFile = nullptr;
    if (sheet) {
        const SpriteSheet* ss = reinterpret_cast<const SpriteSheet*>(sheet);
        spriteCount = ss->m_spriteCount;
        sheetFile = ss->m_filename;  // System::Filename same layout as String
    }

    if (result == -1) {
        // NOT FOUND — this is the critical diagnostic case
        printf("[SpriteSheet] LOOKUP FAILED: name=\"%s\" (len=%d) sheet=%p "
               "(file=\"%s\", count=%d) -> NOT FOUND (-1)\n",
               nameStr ? nameStr : "(null)", nameLen,
               sheet,
               sheetFile ? sheetFile : "(null)", spriteCount);
    } else {
        // Found — log at lower verbosity (can comment out if too noisy)
        printf("[SpriteSheet] lookup: name=\"%s\" sheet=%p (count=%d) -> index=%d\n",
               nameStr ? nameStr : "(null)",
               sheet, spriteCount, result);
    }
#endif

    return result;
}

// ============================================================
// Debug wrapper: GetSpriteCount
// ============================================================
int SpriteSheet_Debug_GetSpriteCount(void* sheet) {
#ifdef _DEBUG
    if (!sheet) {
        printf("[SpriteSheet] GetSpriteCount called with NULL sheet!\n");
        return 0;
    }

    const SpriteSheet* ss = reinterpret_cast<const SpriteSheet*>(sheet);
    int count = ss->m_spriteCount;
    const char* sheetFile = ss->m_filename;

    printf("[SpriteSheet] GetSpriteCount: sheet=%p file=\"%s\" -> %d sprites\n",
           sheet, sheetFile ? sheetFile : "(null)", count);

    return count;
#else
    if (!sheet) return 0;
    return reinterpret_cast<const SpriteSheet*>(sheet)->m_spriteCount;
#endif
}

// ============================================================
// Debug: Dump all sprite names in a sheet
// ============================================================
// Call this to see the full contents of a sprite sheet.
// Useful when a lookup fails — shows what names ARE available.
#ifdef _DEBUG
void SpriteSheet_Debug_DumpAllSprites(void* sheet) {
    if (!sheet) {
        printf("[SpriteSheet] DumpAllSprites: NULL sheet\n");
        return;
    }

    const SpriteSheet* ss = reinterpret_cast<const SpriteSheet*>(sheet);
    int count = ss->m_spriteCount;

    printf("[SpriteSheet] === DUMP sheet=%p file=\"%s\" count=%d ===\n",
           sheet,
           ss->m_filename ? ss->m_filename : "(null)",
           count);

    if (!ss->m_sprites) {
        printf("[SpriteSheet]   (no sprite array allocated)\n");
        return;
    }

    for (int i = 0; i < count && i < 200; i++) {
        const SpriteInfo* info = &ss->m_sprites[i];
        const char* spriteName = info->name ? info->name : "(null)";
        const char* groupName = info->group ? info->group : "(null)";
        printf("[SpriteSheet]   [%3d] name=\"%s\" group=\"%s\" "
               "pos=(%d,%d) size=(%d,%d)\n",
               i, spriteName, groupName,
               info->x, info->y, info->width, info->height);
    }

    if (count > 200) {
        printf("[SpriteSheet]   ... (%d more entries truncated)\n", count - 200);
    }

    printf("[SpriteSheet] === END DUMP ===\n");
}
#endif

// ============================================================
// Init function — call after engine_funcs are resolved
// ============================================================
// Currently just validates that function pointers resolved.
// Future: could swap fn_SpriteSheet_GetSpriteIndexFromName to
// point to our debug wrapper for automatic interception.
void SpriteSheet_Debug_Init() {
#ifdef _DEBUG
    printf("[SpriteSheet] Debug init: GetSpriteIndexFromName=%p, Ctor=%p, "
           "AddSpriteInfo=%p, SetSpriteInfo=%p\n",
           (void*)fn_SpriteSheet_GetSpriteIndexFromName,
           (void*)fn_SpriteSheet_Ctor,
           (void*)fn_SpriteSheet_AddSpriteInfo,
           (void*)fn_SpriteSheet_SetSpriteInfo);

    if (!fn_SpriteSheet_GetSpriteIndexFromName) {
        printf("[SpriteSheet] WARNING: GetSpriteIndexFromName not resolved! "
               "All lookups will return -1.\n");
    }
#endif
}
