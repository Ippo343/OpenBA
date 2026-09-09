"""N-way 'is it ONE body?' check, position-resolved.

Generalises tools/extptr_serialize_diff.py to an arbitrary set of addresses.
At every byte difference the operand is resolved back to the ABSOLUTE address
it encodes; same absolute target => the difference is POSITION, not behaviour.
"""
import sys, pefile
from capstone import Cs, CS_ARCH_X86, CS_MODE_64

DLL = r"C:\GOG Games\Banished\Runtime-x64.dll"
BASE = 0x180000000
pe = pefile.PE(DLL)
exp = {}
for e in pe.DIRECTORY_ENTRY_EXPORT.symbols:
    if e.address:
        exp[BASE + e.address] = (e.name.decode() if e.name else "ord_%d" % e.ordinal)

md = Cs(CS_ARCH_X86, CS_MODE_64); md.detail = True


def decode(addr):
    blob = pe.get_data(addr - BASE, 3000)
    out = []
    for ins in md.disasm(blob, addr):
        out.append(ins)
        if ins.mnemonic == "ret":
            break
    return out


def targets(ins):
    """Absolute addresses this instruction encodes (rel32 / RIP-relative)."""
    t = []
    for op in ins.operands:
        if op.type == 3 and op.mem.base == 41:   # X86_OP_MEM, RIP
            t.append(ins.address + ins.size + op.mem.disp)
        elif op.type == 2 and ins.mnemonic in ("call", "jmp") or \
             (op.type == 2 and ins.mnemonic.startswith("j")):
            t.append(op.imm)
    return t


ref = int(sys.argv[1], 16)
A = decode(ref)
print("reference 0x%x: %d instructions, %d bytes\n" % (ref, len(A), sum(i.size for i in A)))

for arg in sys.argv[2:]:
    b = int(arg, 16)
    B = decode(b)
    print("=== 0x%x: %d instructions, %d bytes" % (b, len(B), sum(i.size for i in B)))
    if len(A) != len(B):
        print("    INSTRUCTION COUNT DIFFERS -- not one body.\n")
        continue
    same = pos = real = 0
    for x, y in zip(A, B):
        if x.bytes == y.bytes:
            same += 1
            continue
        if x.mnemonic != y.mnemonic or x.size != y.size:
            real += 1
            print("    REAL @%d  %s %s  |  %s %s" % (
                A.index(x), x.mnemonic, x.op_str, y.mnemonic, y.op_str))
            continue
        tx, ty = targets(x), targets(y)
        # Same absolute target(s) => the difference is only the encoded distance.
        if tx and ty and len(tx) == len(ty):
            dx = [t - ref for t in tx]  # unused; kept explicit below
            if tx == ty:
                pos += 1
                continue
            # different absolute targets: real, unless both are self-relative
            # jumps to the SAME offset within their own body.
            if all((a - ref) == (c - b) for a, c in zip(tx, ty)):
                pos += 1
                continue
            real += 1
            print("    REAL @%-4d %-6s %-38s -> %s\n              %-6s %-38s -> %s" % (
                A.index(x), x.mnemonic, x.op_str,
                ", ".join("0x%x %s" % (t, exp.get(t, "")) for t in tx),
                y.mnemonic, y.op_str,
                ", ".join("0x%x %s" % (t, exp.get(t, "")) for t in ty)))
        else:
            real += 1
            print("    REAL @%d  %s %s | %s %s (unresolvable)" % (
                A.index(x), x.mnemonic, x.op_str, y.mnemonic, y.op_str))
    print("    identical=%d  position-only=%d  REAL=%d\n" % (same, pos, real))
