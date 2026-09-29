#!/usr/bin/env python3
"""E_floor_and_tests.py -- B.4 block 0, inventory E: the legitimate-cast floor and the tests.

Re-run from the repo root with no arguments:
    python docs/runtime_port/endgame_b/b4_session/scripts/E_floor_and_tests.py
writes docs/runtime_port/endgame_b/b4_session/data/E_floor_and_tests.tsv.
    --dump PATH   also write every cast occurrence with its category (one per line) to PATH
    --summary     print per-category / per-directory sums to stdout

Scope: PROD = code/ runtime/ standalone/ dx11/ engine/, plus tests/unit as its own row.
Comments are stripped with b3r_session/codegrep.py's strip() (strings kept).

Spellings counted, one per OCCURRENCE (not per line, unlike codegrep --count):
    reinterpret_cast<...>          C-style pointer cast `(T*)x`, `(uintptr_t)x`
    static_cast<void*|uintptr_t|intptr_t ...>   const_cast<...>
Every occurrence gets ONE category. Categories in FLOOR stay (a genuine reinterpretation,
kept because of WHAT the cast does, never because of the directory it sits in); DEFER stays
through B.4 and falls in NEXT_SESSION section 4.1 (the dx11 seam); everything else falls in B.4.
Tests get their own categories: SUBJECT stays (the test is about the bytes), the rest falls
when the product function becomes a member.
"""
import os, re, sys, importlib.util
from collections import Counter, defaultdict

ROOT = os.getcwd()
_spec = importlib.util.spec_from_file_location(
    'cg', os.path.join(ROOT, 'docs/runtime_port/endgame_b/b3r_session/codegrep.py'))
cg = importlib.util.module_from_spec(_spec); _spec.loader.exec_module(cg)

PROD = ['code', 'runtime', 'standalone', 'dx11', 'engine']
TESTS = ['tests/unit']
EXCL = ('/standalone/build', '/tests/unit/build', '/.claude/worktrees', '/build/', '/build-')
EXTS = ('.cpp', '.h', '.hpp', '.inc', '.c')
OUT = 'docs/runtime_port/endgame_b/b4_session/data/E_floor_and_tests.tsv'

# ---------------------------------------------------------------- spellings
RI = re.compile(r'\breinterpret_cast\s*<')
SV = re.compile(r'\bstatic_cast\s*<\s*(?:const\s+)?(?:void|u?intptr_t|UINT_PTR|ULONG_PTR|DWORD_PTR)\b(?!\s*\()[^<>]*>')
CC = re.compile(r'\bconst_cast\s*<')
CS = re.compile(r'(?<![\w>\]])\(\s*((?:const\s+|volatile\s+|struct\s+|unsigned\s+)*[A-Za-z_][\w:]*'
                r'(?:\s*<[^()]*?>)?(?:\s+const)?\s*\*+\s*(?:const\s*)?)\)\s*(?=[\w(&*"])')
# integer C-style casts: only the pointer-sized typedefs (a `(size_t)n` / `(uint64_t)x` is an
# arithmetic conversion far more often than a pointer's value; F's compiler pass sees them exactly)
CSI = re.compile(r'(?<![\w>\]])\(\s*((?:u?intptr_t|DWORD_PTR|ULONG_PTR|LONG_PTR|UINT_PTR|INT_PTR))\s*\)\s*(?=[\w(&*])')
NOTCAST_BEFORE = re.compile(r'(\b(sizeof|alignof|decltype|typeid|return|operator)\s*$)')
KEYWORD_T = re.compile(r'^(return|else|case|delete|new|throw|co_return)\b')


def template_arg(text, i):
    """text[i] is '<' -- return (arg, index after '>')."""
    depth = 0; j = i
    while j < len(text):
        c = text[j]
        if c == '<': depth += 1
        elif c == '>':
            depth -= 1
            if depth == 0: return text[i + 1:j], j + 1
        elif c in ';{}': break
        j += 1
    return text[i + 1:j], j


def paren_operand(text, i):
    """skip ws; text[i] should be '(' -- return (operand, index after ')')."""
    while i < len(text) and text[i] in ' \t\n': i += 1
    if i >= len(text) or text[i] != '(': return '', i
    depth = 0; j = i
    while j < len(text):
        c = text[j]
        if c == '(': depth += 1
        elif c == ')':
            depth -= 1
            if depth == 0: return text[i + 1:j], j + 1
        j += 1
    return text[i + 1:], len(text)


def cstyle_operand(text, i):
    m = re.match(r'\s*(&?\s*[\w:.\->\[\]]+(?:\([^()]*\))?|\((?:[^()]|\([^()]*\))*\))', text[i:])
    return (m.group(1), i + m.end()) if m else ('', i)


def occurrences(src):
    """yield (pos, spelling, T, operand, end)"""
    for m in RI.finditer(src):
        t, j = template_arg(src, m.end() - 1)
        o, k = paren_operand(src, j)
        yield m.start(), 'reinterpret', t, o, k
    for m in SV.finditer(src):
        t, j = template_arg(src, src.index('<', m.start()))
        o, k = paren_operand(src, j)
        yield m.start(), 'static_void', t, o, k
    for m in CC.finditer(src):
        t, j = template_arg(src, m.end() - 1)
        o, k = paren_operand(src, j)
        yield m.start(), 'const_cast', t, o, k
    for rx in (CS, CSI):
        for m in rx.finditer(src):
            ls = src.rfind('\n', 0, m.start()) + 1
            before = src[ls:m.start()]
            if NOTCAST_BEFORE.search(before): continue
            t = m.group(1).strip()
            if KEYWORD_T.match(t) or t.endswith('::*'): continue   # `bool (K::*)(...)`: a type
            o, k = cstyle_operand(src, m.end())
            if rx is CSI and re.match(r'^\s*-?\s*(0x[0-9a-fA-F]+|\d+)[uUlL]*\s*$', o):
                continue   # `(uintptr_t)0x38`: an integer literal, arithmetic
            yield m.start(), 'cstyle', t, o, k


# ---------------------------------------------------------------- categories
FLOOR = {'alloc_header', 'string_header', 'byte_io', 'hash', 'value_pun', 'com_iid', 'win32',
         'gpu_mapped', 'container_storage', 'placement_new', 'print_addr', 'ptr_int',
         'vtable_diag', 'crash_diag', 'pe_image', 'crt', 'sentinel_addr', 'alloc_result'}
DEFER = {'dx11_seam'}
SUBJECT = {'t_vtable_probe', 't_layout_probe', 't_sentinel', 't_fixture', 't_identity',
           't_placement_new', 't_print'}

BYTE_T = re.compile(r'^\s*(const\s+)?(u?int8_t|char|unsigned\s+char|signed\s+char|BYTE|std::byte|void)\s*(const\s*)?\*\s*(const\s*)?$')
SCALAR_T = re.compile(r'^\s*(const\s+)?(u?int(8|16|32|64)_t|float|double|int|unsigned(\s+int)?|long\s+long|unsigned\s+long\s+long|bool|wchar_t|char16_t)\s*(const\s*)?[*&]\s*$')
INT_T = re.compile(r'^\s*(const\s+)?(u?intptr_t|uint64_t|int64_t|size_t|unsigned\s+long\s+long|long\s+long|DWORD_PTR|ULONG_PTR|UINT_PTR|INT_PTR|LONG_PTR|uint32_t)\s*$')
VOIDP_T = re.compile(r'^\s*(const\s+)?void\s*(const\s*)?\*\s*$')
OFFSET = re.compile(r'[+-]\s*(0x[0-9a-fA-F]+|\d+|k[A-Z]\w*|off\w*|\w+\s*\*\s*(0x[0-9a-fA-F]+|\d+|sizeof|stride))\b|\bAt\s*\(|\[\s*-')
PRINT = re.compile(r'\b(v?f?printf|snprintf|sprintf|CrashLogf|TRACE\w*|Log\w*f|OutputDebugString\w*|fputs|RtDbg\w*|Dbg\w*|PrintModOff|Note)\s*\(|%p')
WIN32_T = re.compile(r'\b(LPARAM|WPARAM|LRESULT|FARPROC|PROC|LPBYTE|BYTE|EXCEPTION_POINTERS|PEXCEPTION_POINTERS|CONTEXT|PCONTEXT|CWPSTRUCT|WINDOWPOS|MSG|IMAGE_\w+|LPCRITICAL_SECTION|CRITICAL_SECTION|LPTHREAD_START_ROUTINE|PVOID|LPVOID|LPCVOID|PAPCFUNC|WNDPROC|DLGPROC|HOOKPROC|PTOP_LEVEL_EXCEPTION_FILTER|PVECTORED_EXCEPTION_HANDLER|LPCWSTR|LPWSTR|LPCSTR|LPSTR|SRWLOCK|PSRWLOCK|CONDITION_VARIABLE|MINIDUMP\w*|SYMBOL_INFO\w*|pRENDERDOC_\w+|RENDERDOC_\w+)\b')
WIN32_CTX = re.compile(r'\b(GetProcAddress|VirtualAlloc|VirtualQuery|VirtualProtect|CreateThread|_beginthreadex|SetWindowLongPtr\w*|GetWindowLongPtr\w*|SendMessage\w*|PostMessage\w*|EnumWindows|RegQueryValueEx\w*|RegSetValueEx\w*|RegGetValue\w*|lParam|wParam|MultiByteToWideChar|WideCharToMultiByte|QueueUserAPC|SetUnhandledExceptionFilter|AddVectoredExceptionHandler|GetModuleHandle\w*|LoadLibrary\w*|InitializeCriticalSection\w*|EnterCriticalSection|LeaveCriticalSection|DeleteCriticalSection|SetCriticalSectionSpinCount|TryEnterCriticalSection)\b')
COM_CTX = re.compile(r'\b(QueryInterface|GetBuffer|GetParent|GetAdapter|CreateDXGIFactory\w*|kIID_\w+|IID_\w+|__uuidof|IID_PPV_ARGS|EnumAdapters\w*|EnumOutputs)\b')
IO_CTX = re.compile(r'\b(memcpy|memmove|memcmp|memset|fwrite|fread|WriteFile|ReadFile|Read|Write|ReadBytes|WriteBytes|ReadRaw|WriteRaw|Serialize\w*Array|SerializeBytes|SerializeRaw|Compress\w*|Decompress\w*|Inflate\w*|Deflate\w*|lz\w+|LZ\w+|zlib\w*)\s*\(')
HASH_CTX = re.compile(r'\b(\w*[Cc]rc\w*|\w*[Hh]ash\w*|[Ff]nv\w*|[Mm]urmur\w*)\b')
GPU_CTX = re.compile(r'\bpData\b|\bMap\s*\(|\bmapped\b|\bRowPitch\b')
FNPTR_T = re.compile(r'\(\s*(__\w+\s*)?\*\s*\)|\bFn\b|Fn$|(?<!int8)(?<!int16)(?<!int32)(?<!int64)(?<!intptr)(?<!size)(?<!char16)(?<!char32)(?<!wchar)(?<!ptrdiff)_t$|Proc$|Callback$|Func$|FnPtr|Thunk|Handler$|\bSlotFn|MessageCallback')
VT_T = re.compile(r'void\s*(const\s*)?\*\s*(const\s*)?\*\s*(const\s*)?\*|u?intptr_t\s*(const\s*)?\*\s*(const\s*)?\*|uint8_t\s*(const\s*)?\*\s*(const\s*)?\*\s*(const\s*)?\*|\bvptr\b|vtable|Vtbl|VTable')
POD_T = re.compile(r'\bRt[A-Z]\w*::|\brt\w*::|\bPod\w*|\bPOD\b|Shadow|Bridge')
COMIFACE_T = re.compile(r'\b(ID3D1\w+|IDXGI\w+|IUnknown|ID3DBlob|ID3D10Blob)\b')
HANDLE_T = re.compile(r'^\s*(HANDLE|HKEY|HWND|HMODULE|HINSTANCE|HDC|HMONITOR|HCURSOR|HICON|HBRUSH)\s*\*?\s*$')
CONTAINER_T = re.compile(r'^\s*(const\s+)?(T|U|K|V|Elem|Element|E|Key|Value|Node\w*|Entry\w*|Item|ElemT|ValueT|KeyT|T\s*\*)\s*(const\s*)?[*&]\s*(const\s*)?$')
SENTINEL_O = re.compile(r'^\s*(static_cast\s*<\s*u?intptr_t\s*>\s*)?\(?\s*(0x[0-9a-fA-F]+|\d+)[uUlL]*\s*\)?\s*$|^\s*\(u?intptr_t\)\s*(0x[0-9a-fA-F]+|\d+)[uUlL]*\s*$')
PLACEMENT_BEFORE = re.compile(r'\bnew\s*\(\s*$')
POST_OFFSET = re.compile(r'^\s*\)?\s*(\+\s*(0x[0-9a-fA-F]+|\d+|k[A-Z]\w*|off\w*|\w+\s*\*\s*(0x|\d|sizeof|stride))|\[\s*(0x[0-9a-fA-F]+|[1-9]\d*|k[A-Z]\w*)\s*\])')
STORAGE_LOCAL = re.compile(r'(?i)(buf\w*|storage|bytes|^&?ser\d*$|^&?serializer\d*$|emptySerializer|emptySprite|emptyStr|^&?tmp\w*$|vec$|^frustum$|^&?(ws|w|sysStrBuf|ibp)$)')

# Files whose EVERY cast was read and found genuine, with the reason. A directory is never
# enough: each entry was read.
FILE_FLOOR = {
    'runtime/types/object/VSlot.h': ('vtable_diag', 'CompilerSlotOf / SlotAddressOf: the one sanctioned look into a compiler vtable (diagnostics, harness patches); dispatch never uses it'),
    'runtime/src/system/allocator.cpp': ('alloc_header', 'System::MemoryAllocator: boundary tags, free-list links stored in freed blocks, range page arithmetic, pointer hash -- the allocator IS raw memory'),
    'runtime/src/system/allocator_win.cpp': ('win32', 'the allocator keeps its CRITICAL_SECTION as opaque bytes (the type header cannot include <windows.h>); five Win32 lock calls'),
    'runtime/src/crt/msvc_stricmp.cpp': ('crt', 'CRT port: _stricmp compares unsigned bytes, as MSVC\'s does'),
    'runtime/src/crt/msvc_libm.cpp': ('crt', 'CRT port: libm float/double bit access'),
    'runtime/src/crt/msvc_qsort.cpp': ('crt', 'CRT port: qsort swaps untyped elements byte-wise'),
    'standalone/hw_watch.cpp': ('crash_diag', 'hardware watchpoints: DR0-DR3 addresses as integers, EXCEPTION_POINTERS/CONTEXT from the VEH, the watched word read at a raw address'),
    'standalone/sampler.cpp': ('pe_image', 'sampling profiler: walks the PE export directory of a loaded module and the suspended thread\'s stack (CONTEXT.Rsp)'),
}

# Per-file: a base variable whose "+ 0x.." reads are a GENUINE raw walk (allocator blocks),
# versus bases that are a class read by offset (those still fall).
ALLOC_WALK_FILES = {'standalone/redzone_alloc.cpp', 'standalone/alloc_stats.cpp',
                    'runtime/src/system/allocator.cpp'}
ALLOC_OBJ_BASES = re.compile(r'^\s*\(?\s*(alloc|rng|rg|rg2|ranges|range|self)\b|_singleton')
CRASH_CTX = re.compile(r'\b(ctx|c)\s*\.\s*(Rip|Rsp|Rbp|Rcx|Rdx|Rax|Dr\d)\b|\bctx->\w+|\bep->|\bExceptionRecord\b|\bIsBadReadPtr\b|\bmbi\.\w+|\bCONTEXT\b|\bRtlCaptureStackBackTrace\b|\bRtlVirtualUnwind\b|\bRtlLookupFunctionEntry\b|\bbt\s*\[')
CRASH_FILES = {'standalone/main.cpp', 'standalone/redzone_alloc.cpp', 'standalone/hw_watch.cpp',
               'standalone/sampler.cpp', 'standalone/boot_state_watchdog.cpp',
               'standalone/stall_debug.cpp'}

# standalone/ split (question 2).
STANDALONE_KIND = {
    # boot glue: the exe's own start-up, window, options, the plugin handshake
    'main.cpp': 'boot_glue', 'runtime_init.cpp': 'boot_glue', 'video_interface.cpp': 'boot_glue',
    'boot_window.cpp': 'boot_glue', 'frame_limiter.cpp': 'boot_glue', 'options.cpp': 'boot_glue',
    'engine_loader_gate.cpp': 'boot_glue', 'res_load_lock.cpp': 'boot_glue',
    # game / engine logic that retail compiled, living in standalone/
    'menudialogloopt_stubs.cpp': 'game_logic', 'splash_loop_stubs.cpp': 'game_logic',
    'loader_loop_stubs.cpp': 'game_logic', 'alloc_cutover_arm.cpp': 'boot_glue',
    'chunked_scene_loader.cpp': 'game_logic',
}


_DECL = {}


def file_decl_type(name):
    """the declared type of a local / member NAME in the file being scanned (float, double,
    FILETIME), or '' -- a cheap substitute for the compiler, good enough for a pun's operand."""
    m = re.search(r'\b(float|double|FILETIME)\b[^;(){}=]*?\b' + re.escape(name) + r'\b\s*(\[|=|;|,|\{)',
                  _DECL.get('src', ''))
    return m.group(1) if m else ''


def is_raw_array(name):
    """NAME is declared in the file as a raw byte / word array (`uint64_t c0[2]`,
    `alignas(8) uint8_t buf[0x40]`): storage standing in for an object."""
    if not re.match(r'^\w+$', name): return False
    return bool(re.search(r'\b(u?int(8|16|32|64)_t|char|unsigned\s+char|std::byte|void\s*\*)\s+(\w+\s*\[[^\]]*\]\s*(=\s*\{[^}]*\})?\s*,\s*)*'
                          + re.escape(name) + r'\s*\[', _DECL.get('src', '')))


USERDATA_CTX = re.compile(r'\b(AddItem|InsertItem|SetItemData|SetUserData|UserData|PostMessage\w*|SendMessage\w*|PostThreadMessage\w*|SetWindowLongPtr\w*|CreateThread|_beginthreadex|QueueUserWorkItem)\b')


def classify_prod(rel, spelling, t, o, before, after, line):
    """return a category for one PROD occurrence."""
    t = ' '.join(t.split()); o_s = o.strip()
    ff = FILE_FLOOR.get(rel)
    if ff: return ff[0]
    # an offset inside the operand, or applied to the cast's result (`(u8*)(p) + 0x58`, `[kX]`)
    has_off = bool(OFFSET.search(o)) or bool(POST_OFFSET.match(after))
    # a literal address: a sentinel / tag value, not a read
    if re.match(r'^\(?\s*(0x0+|0)[uUlL]*\s*\)?$', o_s):
        return 'null_literal'   # Ghidra's `(int*)0x0`: falls to nullptr
    if re.match(r'^(static_cast\s*<\s*u?intptr_t\s*>\s*)?\(?\s*(0x[0-9a-fA-F]{1,4}|\d{1,4})[uUlL]*\s*\)?$', o_s) and \
            re.search(r'\*\s*$', before) and not re.match(r'^\s*(0x0*1|1)\s*$', o_s):
        return 'null_field_deref'   # `*(T*)0x50`: a faithful read through a null receiver
    if re.match(r'^(static_cast\s*<\s*u?intptr_t\s*>\s*)?\(?\s*-?\s*(0x[0-9a-fA-F]+|\d+)[uUlL]*\s*\)?$', o_s) or \
            (spelling == 'cstyle' and re.match(r'^\s*\(?\s*u?intptr_t\s*\)?\s*$', t) and re.match(r'^\s*-\s*1\b', after)):
        # an integer in a pointer-typed USER-DATA slot (a combo item's data, a message
        # parameter) is what that API is for; an integer handed to a C body's `void*`
        # parameter falls when the parameter gets its real type
        if USERDATA_CTX.search(line) or re.search(r'\bNote\s*\(|\bHit\s*\(', line): return 'sentinel_addr'
        return 'untyped_param'
    # the one cast from the allocator's untyped result
    if re.search(r'\b(MemAlloc|Allocate\w*|malloc|calloc|realloc|VirtualAlloc|HeapAlloc|_aligned_malloc|operator\s+new)\s*\(', o) and not has_off \
            and '*' in t:
        return 'alloc_result'
    # placement new: `new (static_cast<void*>(p)) T(...)`
    if VOIDP_T.match(t) and PLACEMENT_BEFORE.search(before):
        # a local byte buffer standing in for an object the body could declare falls;
        # construction into `this` (retail's Construct/Reconstruct slots), a return slot,
        # allocated or container memory is what placement new is for
        if has_off: return 'offset_read'
        if STORAGE_LOCAL.search(o_s) or re.match(r'^&\s*\w+$', o_s) or is_raw_array(o_s):
            return 'storage_local'
        return 'placement_new'
    # string / wide-string block header: refcount at data[-1], length at data[-2]
    if re.search(r'int32_t|\bint\b', t) and not OFFSET.search(o) and (
            re.match(r'^\s*\)?\s*\[\s*-\s*[12]\s*\]', after) or
            (re.match(r'^\s*-\s*1\s*[;)]', after) and re.search(r'data|name|\bp\b|\bs\b|str', o))):
        return 'string_header'
    if re.search(r'wchar_t|\bchar\b', t) and re.search(r'\b(block\s*\+\s*2|\w*data\s*\)?\s*-\s*8)\s*$', o):
        return 'string_header'
    # allocator raw walks (block tags); a read of the allocator OBJECT's fields still falls
    if rel in ALLOC_WALK_FILES and not ALLOC_OBJ_BASES.match(o) and '_singleton' not in o:
        if has_off or re.search(r'uint64_t|int64_t|uint32_t|uint8_t|uintptr_t', t):
            if not PRINT.search(line):
                return 'alloc_header'
    if rel in CRASH_FILES and (CRASH_CTX.search(line) or CRASH_CTX.search(o)) and not has_off:
        return 'crash_diag'
    if rel in CRASH_FILES and CRASH_CTX.search(line) and re.search(r'\b(sp|vt|target|q)\b', o):
        return 'crash_diag'
    # addresses printed by a diagnostic
    if (VOIDP_T.match(t) or INT_T.match(t)) and PRINT.search(_DECL["stmt"]) and not has_off:
        return 'print_addr'
    # COM out-param that the API types as void** (IID-taking calls)
    if re.search(r'void\s*\*\s*\*', t) and COM_CTX.search(line + after):
        return 'com_iid'
    # Win32 API parameter / result types
    if WIN32_T.search(t) and not has_off:
        return 'win32'
    if (WIN32_CTX.search(o) or (WIN32_CTX.search(line) and FNPTR_T.search(t))) and not has_off:
        return 'win32'
    # mapped GPU memory is bytes
    if GPU_CTX.search(o) and (BYTE_T.match(t) or 'uint8_t' in t):
        return 'gpu_mapped'
    # hashing / CRC over bytes or a pointer value
    if HASH_CTX.search(line) and (BYTE_T.match(t) or INT_T.match(t) or SCALAR_T.match(t)) and not has_off:
        return 'hash'
    # bytes to / from a stream, file, memcpy
    if BYTE_T.match(t) and IO_CTX.search(before) and not has_off:
        return 'byte_io'
    # a Win32 lock kept as opaque bytes in a class whose header cannot include <windows.h>
    if re.search(r'CRITICAL_SECTION', t) and not has_off:
        return 'win32'
    # a pointer used as an integer (alignment, range compare, module offset, pointer hash)
    if INT_T.match(t) and not has_off:
        if re.match(r'^\s*(>>|&|%|\^|<|>|==|!=|-\s*(reinterpret_cast|\(u?intptr_t\)|\w+\s*[;)]))', after) \
                or re.search(r'-\s*$', before) or re.search(r'(<|>|<=|>=)\s*$', before):
            return 'ptr_int'
    # scalar reinterpretation of an object's bytes: float bits, FILETIME->u64, int<->uint for
    # a Serialize overload, a Vector3 as float[3]
    if SCALAR_T.match(t) and re.match(r'^&\s*[\w.\->\[\]]+$', o_s) and not has_off:
        # only a float's bits read as an integer (or a FILETIME as a u64) is a genuine pun;
        # an int field serialized as unsigned, a Vector3 passed as float[3], a Ghidra local
        # of the wrong type all fall when the field / local is declared with its real type
        if re.search(r'Serialize\w*\s*\(|SerU32|SerI32', line): return 'serialize_signedness'
        if re.search(r'u?int(32|64)_t|unsigned|\bint\b', t) and not OFFSET.search(line):
            m = re.match(r'^&\s*(\w+)', o_s)
            if m and file_decl_type(m.group(1)) in ('float', 'double', 'FILETIME'):
                return 'value_pun'
        if 'float' in t and re.search(r'\.(min|max|rotation|translation|position|scale)\b|Vector|\[\w*\]', o_s):
            return 'struct_as_array'
        return 'type_confusion'
    # container templates: element storage from raw bytes
    if CONTAINER_T.match(t) and ('collection' in rel.lower() or 'Collection' in rel):
        return 'container_storage'
    if re.search(r'sizeof\s*\(\s*T\s*\)|\*\s*sizeof\s*\(', o) and ('collection' in rel.lower()):
        return 'container_storage'
    # the dx11 seam (NEXT_SESSION 4.1: companion types become subclasses of Video::*Data)
    if rel.startswith('dx11/') and (re.search(r'Video::\w*Data\b', t) or
                                    re.search(r'Dx11OriginalSlot|Dx11LiveVtable|s_ourVt|kVtables|Dx9P_|Dx9Placeholder|vtName', line + o) or
                                    rel == 'dx11/funcs/dx11_seam.h' or rel == 'dx11/funcs/dx11_seam.cpp'):
        return 'dx11_seam'
    if rel.startswith('standalone/video_interface') and re.search(r'Dx9Placeholder|Dx11_|SentinelModule', o):
        return 'dx11_seam'
    # ---- what falls
    if has_off: return 'offset_read'
    if COMIFACE_T.search(t): return 'untyped_com_field'
    if HANDLE_T.match(t): return 'untyped_handle_field'
    if VT_T.search(t) or re.search(r'vtable|vptr|\bvt\b', o): return 'vtable_read'
    if FNPTR_T.search(t) or re.search(r'&\w+::\w+|&\w+_\w+', o_s) and VOIDP_T.match(t): return 'fnptr'
    if POD_T.search(t) or POD_T.search(o): return 'pod_shadow'
    if re.search(r'System::(String|WideString|Filename)\s*[&*]|String\s*[&*]\s*$', t): return 'pod_shadow'
    if re.match(r'^(self|selfV|selfv|s|p|obj|o|ptr|ctx|host|owner|this|comp|c|e|d|w|raw|base|pv|v|ent|entity|inst|data|storage|mem|buf)$', o_s):
        return 'void_receiver'
    return 'other'


TEST_C_BODY_CALL = re.compile(r'\b(Rt[A-Z]\w*|[A-Z][A-Za-z0-9]*_[A-Z]\w+)\s*\(\s*$')


def classify_test(rel, spelling, t, o, before, after, line):
    """SUBJECT: the cast is what the test checks (a vtable slot, a byte at an offset, a
    pointer's identity, a sentinel, a raw fixture) and stays. The rest falls: a door thunk
    re-entering a member through `self`, a void* handed to a C body, a POD shadow type."""
    t = ' '.join(t.split()); o_s = o.strip()
    if VOIDP_T.match(t) and PLACEMENT_BEFORE.search(before): return 't_placement_new'
    if SENTINEL_O.match(o_s) or re.match(r'^\s*-?\s*(0x[0-9a-fA-F]+|\d+)[uUlL]*\s*$', o_s): return 't_sentinel'
    if PRINT.search(_DECL["stmt"]) and (VOIDP_T.match(t) or INT_T.match(t)): return 't_print'
    # a door thunk: `reinterpret_cast<X*>(self)->X::Member(...)` -- falls with the door
    if re.match(r'^(self|selfV|selfv|s|thisV|obj)$', o_s) and re.match(r'^\s*\)?\s*->\s*[\w:]+::', after):
        return 't_door_thunk'
    if re.search(r'Minted\w*Door|\w+Door\b', o_s + line) and not VT_T.search(t) and '->' in after[:4]:
        return 't_door_thunk'
    # a hand-built retail-shaped function table (`(void*)&Unused, // +0x80`): it mirrors a
    # product body that still dispatches by retail byte slot, and falls with that body
    if (VOIDP_T.match(t) or FNPTR_T.search(t)) and re.match(r'^&\s*[\w:]+$', o_s) and \
            re.match(r'^\s*\)?\s*(,|\}|\)\s*\}|$)', after.split('\n')[0]) and not re.search(r'[\w>]\s*\([^()]*$', before):
        return 't_minted_table'
    if VT_T.search(t) or re.search(r'VtGroup|VTableOf|VptrOf|RetailEntry|RetailSlot|SlotAt|SlotAddressOf|vptr|vtable|\bvt\b|\bvt\[|\bvt\w*\[|minted|g_\w*[Tt]able|Slot\(|\*\s*reinterpret_cast<\s*(const\s+)?void\s*(const\s*)?\*\s*(const\s*)?\*\s*>', line):
        return 't_vtable_probe'
    if FNPTR_T.search(t) and re.search(r'\bvt|slot|Slot|table', o_s + line):
        return 't_vtable_probe'
    if INT_T.match(t) and re.search(r'-\s*(reinterpret_cast|\(u?intptr_t\))|offsetof|Offset|\boff\b', line + after):
        return 't_layout_probe'
    if OFFSET.search(o) or POST_OFFSET.match(after): return 't_layout_probe'
    if re.search(r'uint8_t|char', t) and re.search(r'&\w+|\bbuf\b|\bbytes\b|\braw\b', o_s): return 't_layout_probe'
    if (VOIDP_T.match(t) or INT_T.match(t)) and re.search(r'==|!=|CHECK|\braw\w*\s*=|g_last\w*', line):
        return 't_identity'
    if re.search(r'\b(buf\w*|g_\w*Bytes|\w*Bytes|\w*Storage|storage|mem|raw|bytes|block|cbuf|tbuf|nbuf)\b', o_s):
        return 't_fixture'
    # the product function still takes void*: the cast is the call's receiver / argument
    if TEST_C_BODY_CALL.search(before) or re.search(r'\b(Rt[A-Z]\w*|[A-Z][A-Za-z0-9]*_[A-Z]\w+)\s*\([^()]*$', before):
        return 't_void_receiver'
    if VOIDP_T.match(t) and spelling in ('static_void', 'cstyle'): return 't_void_receiver'
    if re.match(r'^(self|selfV|s|p|obj|ptr|o|this|owner|host)$', o_s): return 't_void_receiver'
    if FNPTR_T.search(t) or re.match(r'^&\s*[\w:]+$', o_s) and VOIDP_T.match(t): return 't_fnptr'
    if POD_T.search(t) or re.search(r'\brt::|\bRt[A-Z]', t): return 't_pod_shadow'
    return 't_other'


def rel_of(p):
    return os.path.relpath(p, ROOT).replace('\\', '/')


def walk(dirs):
    for d in dirs:
        for dp, dn, fs in os.walk(os.path.join(ROOT, d)):
            for f in fs:
                p = os.path.join(dp, f).replace('\\', '/')
                if any(e in p for e in EXCL) or not p.endswith(EXTS): continue
                yield p


def scan(dirs, is_test):
    rows = []
    for p in sorted(walk(dirs)):
        rel = rel_of(p)
        # newline='' : a lone '\r' must not become a line (sed / grep line numbers)
        src = cg.strip(open(p, encoding='utf-8', errors='replace', newline='').read().replace('\r\n', '\n'))
        seen = set()
        _DECL['src'] = src
        for pos, spelling, t, o, end in occurrences(src):
            if pos in seen: continue
            seen.add(pos)
            ls = src.rfind('\n', 0, pos) + 1
            le = src.find('\n', end); le = len(src) if le < 0 else le
            line = src[ls:le]
            # the statement so far (a printf whose "%p" argument sits on a continuation line)
            p2 = src.rfind(";", max(0, ls - 400), ls); _DECL["stmt"] = src[(p2 + 1 if p2 >= 0 else ls):le]
            before = src[ls:pos]
            after = src[end:end + 160]
            cat = (classify_test if is_test else classify_prod)(rel, spelling, t, o, before, after, line)
            lineno = src.count('\n', 0, pos) + 1
            rows.append((rel, lineno, spelling, cat, ' '.join(t.split()), ' '.join(o.split())[:90]))
    return rows


def area(rel):
    if rel.startswith('tests/'): return 'tests'
    parts = rel.split('/')
    if parts[0] == 'standalone':
        return 'standalone:' + STANDALONE_KIND.get(parts[1], 'instrument')
    return parts[0]


def main():
    prod = scan(PROD, False)
    tests = scan(TESTS, True)
    allrows = prod + tests
    byfile = defaultdict(list)
    for r in allrows: byfile[r[0]].append(r)
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, 'w', encoding='utf-8', newline='\n') as f:
        f.write('file\tcasts_total\tlegit_count\tcategory\treason\tarea\treinterpret\tcstyle\tstatic_void\tconst_cast\tdeferred_4_1\tfalls\tfalls_by_kind\n')
        for rel in sorted(byfile):
            rs = byfile[rel]
            sp = Counter(r[2] for r in rs)
            cats = Counter(r[3] for r in rs)
            is_test = rel.startswith('tests/')
            keep = SUBJECT if is_test else FLOOR
            legit = sum(n for c, n in cats.items() if c in keep)
            defer = sum(n for c, n in cats.items() if c in DEFER)
            falls = len(rs) - legit - defer
            legit_cats = Counter({c: n for c, n in cats.items() if c in keep})
            fall_cats = Counter({c: n for c, n in cats.items() if c not in keep and c not in DEFER})
            category = ','.join(c for c, _ in legit_cats.most_common()) or ('dx11_seam' if defer else 'none')
            if rel in FILE_FLOOR: reason = FILE_FLOOR[rel][1]
            elif legit_cats: reason = 'legit: ' + ', '.join(f'{c} {n}' for c, n in legit_cats.most_common())
            else: reason = 'no genuine reinterpretation found'
            fk = ' '.join(f'{c}:{n}' for c, n in fall_cats.most_common())
            f.write(f'{rel}\t{len(rs)}\t{legit}\t{category}\t{reason}\t{area(rel)}\t{sp["reinterpret"]}\t{sp["cstyle"]}\t'
                    f'{sp["static_void"]}\t{sp["const_cast"]}\t{defer}\t{falls}\t{fk}\n')
    if '--dump' in sys.argv:
        path = sys.argv[sys.argv.index('--dump') + 1]
        with open(path, 'w', encoding='utf-8', newline='\n') as f:
            for r in allrows: f.write('\t'.join(map(str, r)) + '\n')
    if '--summary' in sys.argv:
        tot = Counter(); byarea = defaultdict(Counter)
        for r in allrows:
            tot[r[3]] += 1; byarea[area(r[0])][r[3]] += 1
        for c, n in tot.most_common(): print(f'{c}\t{n}')
        print('---')
        for a in sorted(byarea):
            cs = byarea[a]; n = sum(cs.values())
            keep = SUBJECT if a == 'tests' else FLOOR
            print(f'{a}\t{n}\tlegit {sum(v for c, v in cs.items() if c in keep)}\tdefer {sum(v for c, v in cs.items() if c in DEFER)}')
    print(f'wrote {OUT}: {len(byfile)} files, {len(allrows)} occurrences')


if __name__ == '__main__':
    main()
