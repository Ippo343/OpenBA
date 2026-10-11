#!/usr/bin/env python3
"""gen_sdl_dik_tables.py [--check] -- generate src/platform/common/sdl_dik_tables.h

Phase D.4 SDL step I2. The SDL keyboard backend (src/platform/sdl/input_sdl.cpp) fills the
256-byte DirectInput scan-code buffer retail's poller unpacks, and answers the legacy
path's GetAsyncKeyState(vk), from SDL's keyboard state. It needs two tables, generated
here and never typed by hand:

  kSdlScancodeToDik[512]          SDL_Scancode -> DIK_* code (0 = no DirectInput key)
  kVirtualKeyToSdlScancode[256][2] Win32 VK -> up to two SDL_Scancodes (0 = none)

Sources (read, never retyped):
  third_party/SDL/include/SDL3/SDL_scancode.h          SDL_SCANCODE_* values
  third_party/SDL/src/events/scancodes_windows.h       SDL's own Windows scan-code table,
                                                       indexed exactly like DIK_* (low
                                                       byte, +0x80 for an E0 prefix)
  <MinGW>/include/dinput.h                             the DIK_* list
  <MinGW>/include/winuser.h                            the VK_* list
  src/engine/input/input_key_tables.h                  retail's two lifted tables

How a DIK code gets its SDL scan code: by NAME (DIK_* -> SDL_SCANCODE_*, the map below:
identical names pass through), else SDL's Windows table at that code. Every name-mapped
pair is cross-checked against SDL's Windows table; the pairs where they disagree are
listed in KNOWN_DISAGREEMENTS with the reason, and any other disagreement fails.

Asserted (the script exits 1 otherwise):
  * round trip: kSdlScancodeToDik[DikToSdl[d]] == d for every DIK code d with a scan code;
    no SDL scan code is claimed by two DIK codes;
  * every DIK code retail's kScanCodeToIndex maps to a live index (not the 0x68 alias)
    has an SDL scan code, so every index the DirectInput path can reach, SDL reaches;
  * the legacy table agrees with the scan-code path: for each retail index i, every SDL
    scan code of kIndexToVirtualKey[i] lands on index i through kSdlScancodeToDik and
    kScanCodeToIndex, except the indices listed in LEGACY_ONLY (retail's own mismatches
    between its two tables).

--check regenerates in memory and fails if the checked-in header differs.
"""
import os, re, sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, '..', '..'))
SDL_SCANCODE_H = os.path.join(ROOT, 'third_party', 'SDL', 'include', 'SDL3', 'SDL_scancode.h')
SDL_WIN_TABLE = os.path.join(ROOT, 'third_party', 'SDL', 'src', 'events', 'scancodes_windows.h')
MINGW_INCLUDE = os.environ.get('MINGW_INCLUDE', r'C:/msys64/ucrt64/include')
DINPUT_H = os.path.join(MINGW_INCLUDE, 'dinput.h')
WINUSER_H = os.path.join(MINGW_INCLUDE, 'winuser.h')
RETAIL_H = os.path.join(ROOT, 'src', 'engine', 'input', 'input_key_tables.h')
OUT_H = os.path.join(ROOT, 'src', 'platform', 'common', 'sdl_dik_tables.h')

# DIK_* name -> SDL_SCANCODE_* name where the two spell the same key differently.
DIK_TO_SDL_NAME = {
    'BACK': 'BACKSPACE', 'LBRACKET': 'LEFTBRACKET', 'RBRACKET': 'RIGHTBRACKET',
    'LCONTROL': 'LCTRL', 'RCONTROL': 'RCTRL', 'LMENU': 'LALT', 'RMENU': 'RALT',
    'MULTIPLY': 'KP_MULTIPLY', 'CAPITAL': 'CAPSLOCK', 'NUMLOCK': 'NUMLOCKCLEAR',
    'SCROLL': 'SCROLLLOCK', 'SUBTRACT': 'KP_MINUS', 'ADD': 'KP_PLUS', 'DECIMAL': 'KP_PERIOD',
    'DIVIDE': 'KP_DIVIDE', 'NUMPADENTER': 'KP_ENTER', 'NUMPADEQUALS': 'KP_EQUALS',
    'OEM_102': 'NONUSBACKSLASH', 'SYSRQ': 'PRINTSCREEN', 'PRIOR': 'PAGEUP', 'NEXT': 'PAGEDOWN',
    'LWIN': 'LGUI', 'RWIN': 'RGUI', 'APPS': 'APPLICATION',
    'KANA': 'INTERNATIONAL2', 'ABNT_C1': 'INTERNATIONAL1', 'CONVERT': 'INTERNATIONAL4',
    'NOCONVERT': 'INTERNATIONAL5', 'YEN': 'INTERNATIONAL3', 'ABNT_C2': 'KP_COMMA',
    'NEXTTRACK': 'MEDIA_NEXT_TRACK', 'PLAYPAUSE': 'MEDIA_PLAY_PAUSE', 'MEDIASTOP': 'MEDIA_STOP',
    'WEBHOME': 'AC_HOME', 'WEBSEARCH': 'AC_SEARCH', 'WEBFAVORITES': 'AC_BOOKMARKS',
    'WEBREFRESH': 'AC_REFRESH', 'WEBSTOP': 'AC_STOP', 'WEBFORWARD': 'AC_FORWARD',
    'WEBBACK': 'AC_BACK', 'MEDIASELECT': 'MEDIA_SELECT',
    **{f'NUMPAD{d}': f'KP_{d}' for d in range(10)},
}
# DIK codes that get NO SDL scan code, and why.
DIK_UNMAPPED = {
    'NUMPADCOMMA': 'NEC PC-98 key; SDL reports the keypad comma as 0x7E (DIK_ABNT_C2)',
}
# (DIK code, the name-mapped SDL name, SDL's Windows table name at that code): the name
# wins; each pair is a documented difference between DirectInput's and SDL's tables.
KNOWN_DISAGREEMENTS = {
    (0xC5, 'PAUSE', 'NUMLOCKCLEAR'):
        'DirectInput reports Pause as 0xC5; SDL reads E0 45 as NumLock and synthesises '
        'Pause as E0 46 (SDL_windowsevents.c)',
}
# Win32 VK_* name -> the SDL scan codes of the keys that set it (GetAsyncKeyState of a
# generic modifier is true for either side). Letters and digits are added below. The
# positions are the US layout's: see the generated header's banner.
VK_TO_SDL = {
    'ESCAPE': ['ESCAPE'], 'BACK': ['BACKSPACE'], 'TAB': ['TAB'], 'RETURN': ['RETURN', 'KP_ENTER'],
    'SHIFT': ['LSHIFT', 'RSHIFT'], 'CONTROL': ['LCTRL', 'RCTRL'], 'MENU': ['LALT', 'RALT'],
    'PAUSE': ['PAUSE'], 'CAPITAL': ['CAPSLOCK'], 'SPACE': ['SPACE'], 'PRIOR': ['PAGEUP'],
    'NEXT': ['PAGEDOWN'], 'END': ['END'], 'HOME': ['HOME'], 'LEFT': ['LEFT'], 'UP': ['UP'],
    'RIGHT': ['RIGHT'], 'DOWN': ['DOWN'], 'EXECUTE': ['EXECUTE'], 'SNAPSHOT': ['PRINTSCREEN'],
    'INSERT': ['INSERT'], 'DELETE': ['DELETE'], 'LWIN': ['LGUI'], 'RWIN': ['RGUI'],
    'APPS': ['APPLICATION'], 'MULTIPLY': ['KP_MULTIPLY'], 'ADD': ['KP_PLUS'],
    'SUBTRACT': ['KP_MINUS'], 'DECIMAL': ['KP_PERIOD'], 'DIVIDE': ['KP_DIVIDE'],
    'NUMLOCK': ['NUMLOCKCLEAR'], 'SCROLL': ['SCROLLLOCK'],
    'LSHIFT': ['LSHIFT'], 'RSHIFT': ['RSHIFT'], 'LCONTROL': ['LCTRL'], 'RCONTROL': ['RCTRL'],
    'LMENU': ['LALT'], 'RMENU': ['RALT'],
    'OEM_1': ['SEMICOLON'], 'OEM_PLUS': ['EQUALS'], 'OEM_COMMA': ['COMMA'], 'OEM_MINUS': ['MINUS'],
    'OEM_PERIOD': ['PERIOD'], 'OEM_2': ['SLASH'], 'OEM_3': ['GRAVE'], 'OEM_4': ['LEFTBRACKET'],
    'OEM_5': ['BACKSLASH'], 'OEM_6': ['RIGHTBRACKET'], 'OEM_7': ['APOSTROPHE'],
    'OEM_102': ['NONUSBACKSLASH'],
    **{f'F{n}': [f'F{n}'] for n in range(1, 25)},
    **{f'NUMPAD{d}': [f'KP_{d}'] for d in range(10)},
}
# Retail indices whose virtual key and scan code disagree in retail's own two tables
# (kIndexToVirtualKey vs kScanCodeToIndex); the legacy path keeps the virtual key.
LEGACY_ONLY = {
    0x3D: 'VK_SHIFT (either shift): the DirectInput path has LSHIFT 0x3E and RSHIFT 0x42',
    0x43: 'VK_CONTROL (either ctrl): the DirectInput path has LCONTROL 0x45 and RCONTROL 0x49',
    0x44: 'VK_MENU (either alt): the DirectInput path has LMENU 0x46 and RMENU 0x48',
    0x3C: 'VK_RETURN is also the keypad Enter, which the DirectInput path sends to 0x5D',
    0x5D: 'VK_EXECUTE: the DirectInput path puts the keypad Enter here',
    0x68: 'VK_OEM_8: no US key; the DirectInput path aliases unmapped codes here',
}


def parse_sdl_scancodes():
    vals = {}
    for m in re.finditer(r'SDL_SCANCODE_([A-Z0-9_]+)\s*=\s*(\d+)', open(SDL_SCANCODE_H).read()):
        vals.setdefault(m.group(1), int(m.group(2)))
    return vals


def parse_sdl_windows_table(sdl):
    body = open(SDL_WIN_TABLE).read()
    body = body[body.index('windows_scancode_table[] = {'):]
    body = body[:body.index('};')]
    table = {}
    for m in re.finditer(r'/\*0x([0-9a-f]+)\*/\s*SDL_SCANCODE_([A-Z0-9_]+)', body):
        code = int(m.group(1), 16)
        idx = (code & 0xFF) | (0x80 if code > 0xFF else 0)
        if m.group(2) != 'UNKNOWN':
            table[idx] = m.group(2)
    return table


def parse_defines(path, prefix):
    out = {}
    for m in re.finditer(r'#define\s+' + prefix + r'([A-Z0-9_]+)\s+(0x[0-9A-Fa-f]+)\b',
                         open(path).read()):
        out.setdefault(m.group(1), int(m.group(2), 16))
    return out


def parse_retail():
    txt = open(RETAIL_H).read()
    def arr(name):
        body = txt[txt.index(name):]
        body = body[body.index('{') + 1:body.index('};')]
        return [int(x, 16) for x in re.findall(r'0x[0-9a-fA-F]+', body)]
    return arr('kIndexToVirtualKey'), arr('kScanCodeToIndex')


def build():
    errors = []
    sdl = parse_sdl_scancodes()
    wintab = parse_sdl_windows_table(sdl)
    dik = parse_defines(DINPUT_H, 'DIK_')
    vk = parse_defines(WINUSER_H, 'VK_')
    for c in '0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ':
        vk[c] = ord(c)
        VK_TO_SDL[c] = [c]
    idx_to_vk, dik_to_idx = parse_retail()
    if len(idx_to_vk) != 0x69 or len(dik_to_idx) != 256:
        errors.append('retail tables: wrong sizes %d / %d' % (len(idx_to_vk), len(dik_to_idx)))

    # DIK code -> SDL scan code.
    dik_to_sdl = {}
    disagreements = set()
    for name, code in sorted(dik.items(), key=lambda kv: kv[1]):
        if name in DIK_UNMAPPED:
            continue
        sname = DIK_TO_SDL_NAME.get(name, name)
        if sname in sdl:
            tname = wintab.get(code)
            if tname is not None and tname != sname:
                disagreements.add((code, sname, tname))
            dik_to_sdl.setdefault(code, sdl[sname])
        elif code in wintab:
            dik_to_sdl.setdefault(code, sdl[wintab[code]])
    if disagreements != set(KNOWN_DISAGREEMENTS):
        errors.append('DIK vs SDL Windows table disagreements changed: %s' %
                      sorted(disagreements ^ set(KNOWN_DISAGREEMENTS)))

    sdl_to_dik = [0] * 512
    for code, s in sorted(dik_to_sdl.items()):
        if sdl_to_dik[s]:
            errors.append('SDL scan code %d claimed by DIK 0x%02X and 0x%02X' %
                          (s, sdl_to_dik[s], code))
        sdl_to_dik[s] = code
    for code, s in dik_to_sdl.items():          # round trip
        if sdl_to_dik[s] != code:
            errors.append('round trip DIK 0x%02X -> %d -> 0x%02X' % (code, s, sdl_to_dik[s]))
    for code in range(256):                     # every live retail index is reachable
        if dik_to_idx and dik_to_idx[code] != 0x68 and code not in dik_to_sdl:
            errors.append('DIK 0x%02X (retail index 0x%02X) has no SDL scan code' %
                          (code, dik_to_idx[code]))

    # VK -> SDL scan codes.
    vk_to_sdl = [[0, 0] for _ in range(256)]
    by_value = {}
    for name, v in vk.items():
        by_value.setdefault(v, name)
    for name, snames in VK_TO_SDL.items():
        if name not in vk:
            errors.append('VK_%s not in winuser.h' % name)
            continue
        for k, sname in enumerate(snames):
            vk_to_sdl[vk[name]][k] = sdl[sname]
    for i, v in enumerate(idx_to_vk):           # legacy agrees with the scan-code path
        codes = [s for s in vk_to_sdl[v] if s]
        if i in LEGACY_ONLY:
            # An exemption must be a real mismatch, or it would hide nothing and lie.
            if codes and all(dik_to_idx[sdl_to_dik[s]] == i for s in codes):
                errors.append('LEGACY_ONLY index 0x%02X agrees: remove the exemption' % i)
            continue
        if not codes:
            errors.append('index 0x%02X: VK 0x%02X has no SDL scan code' % (i, v))
        for s in codes:
            got = dik_to_idx[sdl_to_dik[s]]
            if got != i:
                errors.append('index 0x%02X: VK 0x%02X -> SDL %d -> DIK 0x%02X -> index 0x%02X' %
                              (i, v, s, sdl_to_dik[s], got))

    names = {v: k for k, v in sdl.items()}
    return errors, render(sdl_to_dik, vk_to_sdl, names, by_value, dik)


def render(sdl_to_dik, vk_to_sdl, sdl_names, vk_names, dik):
    dik_names = {}
    for k, v in dik.items():
        dik_names.setdefault(v, k)
    o = []
    o.append('#pragma once')
    o.append('// ' + '=' * 76)
    o.append('// src/platform/common/sdl_dik_tables.h -- SDL scan codes <-> DirectInput / VK keys')
    o.append('// ' + '=' * 76)
    o.append('// GENERATED by tools/sdl_dik/gen_sdl_dik_tables.py from SDL_scancode.h,')
    o.append("// SDL's scancodes_windows.h, MinGW's dinput.h / winuser.h and retail's two lifted")
    o.append('// tables (src/engine/input/input_key_tables.h). Do not edit: change the script and')
    o.append('// re-run it (`--check` verifies this file). Phase D.4 SDL step I2.')
    o.append('//')
    o.append('// kSdlScancodeToDik: SDL_Scancode -> the DIK_* code DirectInput reports for that')
    o.append('//   physical key (0: none). Both are positional (layout-independent).')
    o.append('// kVirtualKeyToSdlScancode: Win32 VK -> the scan codes whose keys set it, for the')
    o.append("//   legacy GetAsyncKeyState path (0: none). Win32's VKs follow the active layout;")
    o.append("//   this table is the US layout's, so on another layout a letter / OEM key is the")
    o.append('//   key at the US position (recorded in the SDL-C hand-in).')
    o.append('// No OS header, no SDL header.')
    o.append('// ' + '=' * 76)
    o.append('')
    o.append('#include <cstdint>')
    o.append('')
    o.append('namespace Platform {')
    o.append('namespace Input {')
    o.append('')
    o.append('constexpr int kSdlScancodeCount = 512;   // SDL_SCANCODE_COUNT')
    o.append('')
    o.append('inline constexpr uint8_t kSdlScancodeToDik[kSdlScancodeCount] = {')
    for s in range(512):
        if sdl_to_dik[s]:
            o.append('    0x%02X,   // %3d SDL_SCANCODE_%s -> DIK_%s' %
                     (sdl_to_dik[s], s, sdl_names.get(s, '?'), dik_names.get(sdl_to_dik[s], '?')))
        else:
            o.append('    0x00,   // %3d%s' % (s, (' SDL_SCANCODE_' + sdl_names[s]) if s in sdl_names else ''))
    o.append('};')
    o.append('')
    o.append('inline constexpr uint16_t kVirtualKeyToSdlScancode[256][2] = {')
    for v in range(256):
        a, b = vk_to_sdl[v]
        nm = vk_names.get(v)
        if nm is None and (0x30 <= v <= 0x39 or 0x41 <= v <= 0x5A):
            nm = chr(v)
        cm = ''
        if a:
            cm = ' VK_%s -> %s' % (nm, ', '.join(sdl_names[x] for x in (a, b) if x))
        o.append('    {%3d, %3d},   // 0x%02X%s' % (a, b, v, cm))
    o.append('};')
    o.append('')
    o.append('}  // namespace Input')
    o.append('}  // namespace Platform')
    return '\n'.join(o) + '\n'


def main():
    errors, text = build()
    for e in errors:
        print('ERROR:', e)
    if errors:
        return 1
    if '--check' in sys.argv:
        cur = open(OUT_H, encoding='utf-8').read().replace('\r\n', '\n')
        if cur != text:
            print('ERROR: %s is out of date' % OUT_H)
            return 1
        print('OK: %s is current; all assertions hold' % OUT_H)
        return 0
    with open(OUT_H, 'w', encoding='utf-8', newline='\n') as f:
        f.write(text)
    print('wrote %s; all assertions hold' % OUT_H)
    return 0


if __name__ == '__main__':
    sys.exit(main())
