"""How many copies of the 748-byte ExternalPtr<T>::Serialize body ship in the DLL?

Builds a mask from the reference body in which every byte belonging to a rel32
or a RIP-relative displacement is a wildcard (those are the fields that MUST
differ between two copies at different addresses), then scans .text for matches.
"""
import pefile, re
from capstone import Cs, CS_ARCH_X86, CS_MODE_64

DLL = r"C:\GOG Games\Banished\Runtime-x64.dll"
BASE = 0x180000000
REF = 0x1800042a0
pe = pefile.PE(DLL)
exp = {}
for e in pe.DIRECTORY_ENTRY_EXPORT.symbols:
    if e.address:
        exp[BASE + e.address] = (e.name.decode() if e.name else "ord_%d" % e.ordinal)
md = Cs(CS_ARCH_X86, CS_MODE_64); md.detail = True

blob = pe.get_data(REF - BASE, 3000)
ins_list = []
for ins in md.disasm(blob, REF):
    ins_list.append(ins)
    if ins.mnemonic == "ret":
        break
n = sum(i.size for i in ins_list)
body = blob[:n]
mask = bytearray(b"\x01" * n)   # 1 = must match, 0 = wildcard
off = 0
for ins in ins_list:
    for op in ins.operands:
        if op.type == 3 and op.mem.base == 41:      # RIP-relative
            d = ins.disp_offset
            for k in range(ins.disp_size):
                mask[off + d + k] = 0
        elif op.type == 2 and (ins.mnemonic in ("call", "jmp") or ins.mnemonic.startswith("j")):
            d = ins.imm_offset
            for k in range(ins.imm_size):
                mask[off + d + k] = 0
    off += ins.size
print("body %d bytes, %d wildcard bytes" % (n, mask.count(0)))

pat = b"".join((re.escape(bytes([body[i]])) if mask[i] else b".") for i in range(n))
for sec in pe.sections:
    if not sec.Name.startswith(b".text"):
        continue
    data = sec.get_data()
    va = BASE + sec.VirtualAddress
    for m in re.finditer(pat, data, re.DOTALL):
        addr = va + m.start()
        # which typeInfo does this copy read?
        sub = pe.get_data(addr - BASE, n)
        tis = []
        for ins in md.disasm(sub, addr):
            if ins.mnemonic == "mov" and ins.op_str.startswith("edx, dword ptr [rip"):
                for op in ins.operands:
                    if op.type == 3 and op.mem.base == 41:
                        t = ins.address + ins.size + op.mem.disp
                        tis.append("0x%x %s" % (t, exp.get(t, "?")))
        print("  0x%x  %s  %s" % (addr, exp.get(addr, ""), " | ".join(sorted(set(tis)))))
