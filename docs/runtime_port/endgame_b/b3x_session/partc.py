"""partc.py -- the size of the NORMAL-VIRTUAL CUTOVER (INVENTORY_B Part C, re-measured on the B.3.9 close tree),
plus the list the cutover really depends on: every place that still depends on a vtable SLOT INDEX.

Run from the repo root:  python docs/runtime_port/endgame_b/b3x_session/partc.py [--list PATTERN_NAME]
Code lines only (comments stripped by codegrep.strip, strings kept); build*/ and obj/ excluded.
Counts are per top-level area: code/Game/<Sub>, runtime/src/<ns>, runtime/types/<ns>, standalone, engine, dx11, tests.

WHY SLOT INDICES: under GCC's Itanium ABI a `virtual ~T()` occupies TWO slots (complete + deleting), MSVC's one
scalar/vector deleting dtor occupies ONE. Replacing slot 0's `DeletingDtor(unsigned)` with `virtual ~T()` shifts
every later virtual by one slot. Anything that reads a vtable BY INDEX (raw `vt[N]`, a byte offset into the table,
a PMF-order test, a hand table indexed by retail's slot number, the dx11 seam's 84-slot table) silently calls the
wrong function after the cutover. That list, not the declaration count, is the size of the step.
"""
import os, re, sys, collections
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from codegrep import strip

ROOTS = ('code', 'runtime', 'standalone', 'engine', 'dx11', 'tests')
EXTS = ('.cpp', '.h', '.inc', '.hpp')

def area(rel):
    p = rel.split('/')
    if p[0] == 'code' and len(p) > 2: return 'code/Game/' + p[2]
    if p[0] == 'runtime' and len(p) > 2: return 'runtime/' + p[1] + '/' + (p[2] if len(p) > 3 else '')
    return p[0]

SRC = {}
for r in ROOTS:
    for dp, dn, fs in os.walk(r):
        dn[:] = [x for x in dn if not (x.startswith('build') or x == 'obj')]
        for f in fs:
            if f.endswith(EXTS):
                p = os.path.join(dp, f); rel = p.replace('\\', '/')
                SRC[rel] = strip(open(p, encoding='utf-8', errors='replace').read()).split('\n')

VT = r'(?:vt|vtbl|vtable|vtab|vptr|table|VT|Vtable|vt\w*)'
PATTERNS = collections.OrderedDict([
    ('DeletingDtor decl/def', re.compile(r'\bDeletingDtor\s*\(\s*(?:unsigned|uint32_t|int|uint8_t)\b')),
    ('DeletingDtor(0) self-call', re.compile(r'DeletingDtor\s*\(\s*0\s*\)')),
    ('->DeletingDtor(1)', re.compile(r'DeletingDtor\s*\(\s*1\s*\)')),
    ('Object::Destroy(', re.compile(r'\b(?:Object::)?Destroy\s*\(\s*[\w>.\-*&()]+\s*\)\s*;')),
    ('CallSlot0(', re.compile(r'\bCallSlot0\s*\(')),
    ('explicit ~T() call', re.compile(r'(?:->|\.)\s*(?:\w+::)*~\w+\s*\(')),
    # --- slot-index dependencies ---
    ('raw vt[0] dispatch', re.compile(r'(?:\b' + VT + r'\s*\[\s*0\s*\]|\)\s*\[\s*0\s*\])\s*\)?\s*\(')),
    ('raw vt[N>=1] index', re.compile(r'(?:\b' + VT + r'\s*\[\s*(?:[1-9]\d*|0x[0-9a-fA-F]+|k\w+)\s*\]|\*\*\*\s*>?\s*\([^)]*\)\s*\)?\s*\[\s*[1-9]\d*\s*\])')),
    ('vtable byte offset (+0x..)', re.compile(r'\b' + VT + r'\s*\+\s*0x[0-9a-fA-F]+\b|\*\(\s*\w+\s*\*\*\s*\)\s*\(\s*\*\s*\(\s*\w+\s*\*\*\s*\)')),
    ('slot helpers (VSlot/SlotOf/VCall*/CallVSlot/Slot<N>)', re.compile(r'\b(?:VSlot\w*|SlotOf|VCall\w*|CallVSlot|VtSlot\w*|SlotFn\w*)\s*[<(]')),
    ('PMF-order test (PmfSlot)', re.compile(r'\bPmfSlot\w*\s*[<(]')),
    ('dx11 seam slot index', re.compile(r'\bkSlot\w*\b|\bSeam\w*Slot\w*\b')),
    # --- shapes the B3X block-0 inventories found partc missing (INVENTORY_B1 6.2, R-IO / R-UI hand-ins) ---
    ('B3X table[k / 8]', re.compile(r'\b\w*(?:vt|vtbl|vtable|Vtable|VTable)\w*\s*\[[^\]]*/\s*(?:8|sizeof\s*\(\s*void\s*\*\s*\))\s*\]')),
    ('B3X named slot constant', re.compile(r'\b(?:rw_vtbl|programdata_vtbl|devicedata_vtbl|pdvt|ddvt)::k\w+|\bk(?:Rw|CtrlVtbl|DlgVtbl|Vt[A-Z]|StreamVtbl|FileStreamVtbl|InternalVtbl|StreamSlot)\w*|\bEL::kVtbl\w+|kExternalSerializeSlotBytes')),
    ('B3X byte offset on a cast table', re.compile(r'reinterpret_cast\s*<\s*(?:const\s+)?(?:uint8_t|char|unsigned char)\s*\*\s*>\s*\(\s*[\w>.\-]*(?:vt|vtable|vtbl|vptr)\w*\s*\)\s*\+|reinterpret_cast\s*<\s*(?:const\s+)?(?:uint8_t|char|unsigned char)\s*\*\s*>\s*\(\s*\*\s*reinterpret_cast\s*<\s*(?:const\s+)?void\s*\*\s*(?:const\s*)?\*\s*>|\*reinterpret_cast<\w+\*>\(\s*\*reinterpret_cast<(?:const\s+)?uint8_t\s*\*\*>\([^)]*\)\s*\+\s*0x[0-9a-fA-F]+')),
    ('B3X helper-returned table indexed', re.compile(r'\b(?:VTable\w*|VtblOf|Vtbl|RwVTable|VtOf)\s*\([^;]*?\)\s*\[')),
    ('B3X slot helper names', re.compile(r'\b(?:VSlot|RwSlot|SourceSlot|ReadVtableSlot|CompanionSlot|ComSlot|Slot\d*|SlotFn\w*|RenderStateSlot\d*|SerializeBufferSlot|DeletingDtorOf|GetTypeIdVirtual|SerializeVirtual|CallGetTypeId|EncCall\w+|ComCall|DestroyReaderWriter|DeleteRw|SerScalar|RwScalar)\s*[<(]')),
    ('B3X cast vptr indexed', re.compile(r'\(\s*\*\s*(?:static_cast|reinterpret_cast)\s*<\s*(?:const\s+)?\w+\s*\*\s*(?:const\s*)?\*\s*(?:const\s*)?\*?\s*>\s*\([^)]*\)\s*\)\s*\[|\(\s*\*\s*reinterpret_cast\s*<\s*\w+\s*\*\s*>\s*\(\s*\*\s*reinterpret_cast\s*<\s*(?:const\s+)?void\s*\*\s*(?:const\s*)?\*\s*>\s*\(')),
    # --- the four shapes b3x_session/W0_AUDIT.md section 6 found missing (B3R owed 6) ---
    ('B3R static_cast byte offset on the vptr', re.compile(r'(?:static_cast|reinterpret_cast)\s*<\s*(?:const\s+)?(?:uint8_t|char)\s*\*\s*>\s*\(\s*\*\s*reinterpret_cast\s*<\s*(?:const\s+)?void\s*\*\s*(?:const\s*)?\*\s*>\s*\([^)]*\)\s*\)\s*\+')),
    ('B3R POD field _vftable', re.compile(r'\b_vftable\b')),
    ('B3R typed fn-pointer table indexed', re.compile(r'\(\s*\*\s*reinterpret_cast\s*<\s*[\w:]+\s*\*\s*(?:const\s*)?\*\s*>\s*\([^)]*\)\s*\)\s*\[')),
    ('B3R typed slot-0 read, no index', re.compile(r'\*\s*reinterpret_cast\s*<\s*\w+\s*\*\s*>\s*\(\s*\*\s*reinterpret_cast\s*<\s*void\s*\*\*\s*>\s*\(\w+\)\s*\)')),
])

counts = {k: collections.Counter() for k in PATTERNS}
hits = {k: [] for k in PATTERNS}
for rel, lines in SRC.items():
    for i, l in enumerate(lines, 1):
        for k, pat in PATTERNS.items():
            if pat.search(l) and not (k.startswith('B3X') and 'static_assert' in l):
                if k == 'dx11 seam slot index' and not rel.startswith('dx11/'): continue
                counts[k][area(rel)] += 1
                hits[k].append(f'{rel}:{i}: {l.strip()[:150]}')

if '--list' in sys.argv:
    want = sys.argv[sys.argv.index('--list') + 1]
    for k in PATTERNS:
        if want.lower() in k.lower():
            print('\n'.join(hits[k]))
    sys.exit(0)
for k in PATTERNS:
    c = counts[k]
    top = ', '.join(f'{a} {n}' for a, n in c.most_common(12))
    print(f'{k}: {sum(c.values())}  [{top}{", ..." if len(c) > 12 else ""}]')
