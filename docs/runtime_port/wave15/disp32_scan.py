"""Lane DOOR's reimplementation of the kickoff's method-1 crude disp32 scan,
used as the SECOND route (alongside get_xrefs_to) for exe_text_sites in
docs/runtime_port/wave15/door_table.md.

For a given target VA, scans every byte offset in Application-x64.exe's .text
section, reads it as a little-endian int32 disp32, and checks whether it
produces `target` as a RIP-relative reference for any trailing-immediate
length 0-6 (`target = field_va + 4 + trailing + disp32`). This is intentionally
crude and instruction-boundary-blind -- see door_table.md's "Door table"
section for the false-positive analysis (9 extra hits across 7 rows, all
diagnosed with get_assembly_context: 8 land at "No instruction at address",
1 lands on an unrelated MOV-immediate whose literal operand coincidentally
satisfies the arithmetic). Kept only as the second-route cross-check the lane
doc asked for -- get_xrefs_to is the reliable column.

Usage: python disp32_scan.py 0x140151610
"""
import pefile, struct, sys

EXE = r"C:\GOG Games\Banished\Application-x64.exe"
pe = pefile.PE(EXE)
image_base = pe.OPTIONAL_HEADER.ImageBase

text = None
for s in pe.sections:
    if s.Name.rstrip(b'\x00') == b'.text':
        text = s
        break
assert text is not None
data = text.get_data()
va0 = image_base + text.VirtualAddress

def scan(target):
    hits = []
    for pos in range(0, len(data) - 4):
        disp = struct.unpack_from('<i', data, pos)[0]
        field_va = va0 + pos
        for trailing in range(0, 7):
            cand = field_va + 4 + trailing + disp
            if cand == target:
                hits.append((field_va, trailing))
    return hits

if __name__ == '__main__':
    target = int(sys.argv[1], 16)
    hits = scan(target)
    for va, trailing in sorted(hits):
        print(hex(va), 'trailing=', trailing)
    print('count=', len(hits))
