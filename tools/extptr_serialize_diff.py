#!/usr/bin/env python3
"""Are 0x1800042a0 and 0x180076610 ONE body at two addresses?

Wave 9 lane UIW2 predicted they are ("215 instructions each with byte-identical
prologues ... almost certainly ONE body parameterised by a type id") and made
porting Serialize@ElementDescription conditional on the answer.

A RAW byte-diff cannot answer it: every rel32 and every RIP-relative
displacement encodes a distance FROM THE INSTRUCTION, so two identical bodies at
different addresses differ in every one of those fields. That is why this walks
the two byte strings together and, at each difference, resolves the field back
to the ABSOLUTE address it encodes. Same absolute target => the difference is
position, not behaviour.

Uses the capstone disassembler when available (exact instruction boundaries);
falls back to reporting that it could not decide rather than guessing.
"""
import sys
import pefile

DLL = r"C:\GOG Games\Banished\Runtime-x64.dll"
BASE = 0x180000000
A_ADDR = 0x1800042a0
B_ADDR = 0x180076610


def main():
    pe = pefile.PE(DLL)

    def rd(va, n):
        return pe.get_data(va - BASE, n)

    try:
        from capstone import Cs, CS_ARCH_X86, CS_MODE_64
    except ImportError:
        print("capstone not installed -- cannot decide. "
              "pip install capstone, or read both in Ghidra.")
        return 2

    md = Cs(CS_ARCH_X86, CS_MODE_64)
    md.detail = True

    def decode(addr, blob):
        out = []
        for ins in md.disasm(blob, addr):
            out.append(ins)
            if ins.mnemonic == "ret":
                break
        return out

    A = decode(A_ADDR, rd(A_ADDR, 2048))
    B = decode(B_ADDR, rd(B_ADDR, 2048))

    print("A 0x%x: %d instructions, %d bytes" % (A_ADDR, len(A), sum(i.size for i in A)))
    print("B 0x%x: %d instructions, %d bytes" % (B_ADDR, len(B), sum(i.size for i in B)))
    print()

    if len(A) != len(B):
        print("INSTRUCTION COUNT DIFFERS -- not one body.")
        return 1

    same_shape = 0
    pos_only = []      # differ only by an absolute-equal displacement
    real = []          # genuinely different

    for ia, ib in zip(A, B):
        if ia.bytes == ib.bytes:
            same_shape += 1
            continue
        if ia.mnemonic != ib.mnemonic or ia.size != ib.size:
            real.append((ia, ib, "different instruction"))
            continue

        # Same mnemonic and length, different bytes. Resolve any
        # PC-relative operand to its absolute target and compare those.
        ta = tb = None
        if ia.mnemonic in ("call", "jmp") or ia.mnemonic.startswith("j"):
            try:
                ta = ia.operands[0].imm
                tb = ib.operands[0].imm
            except Exception:
                ta = tb = None
        if ta is None:
            # RIP-relative memory operand.
            for oa, ob in zip(ia.operands, ib.operands):
                if oa.type == 3 and oa.mem.base == 41:      # X86_OP_MEM, RIP
                    ta = ia.address + ia.size + oa.mem.disp
                    tb = ib.address + ib.size + ob.mem.disp
                    break

        if ta is not None and ta == tb:
            pos_only.append((ia, ib, ta))
        else:
            real.append((ia, ib, "abs targets 0x%s vs 0x%s"
                         % (format(ta, 'x') if ta else '?',
                            format(tb, 'x') if tb else '?')))

    print("identical bytes            : %d" % same_shape)
    print("differ ONLY by position    : %d  (same absolute target)" % len(pos_only))
    print("genuinely different        : %d" % len(real))
    print()

    if real:
        print("THE REAL DIFFERENCES -- these are what a port must parameterise:")
        for ia, ib, why in real[:25]:
            print("  A 0x%x  %-8s %-40s" % (ia.address, ia.mnemonic, ia.op_str))
            print("  B 0x%x  %-8s %-40s   [%s]" % (ib.address, ib.mnemonic, ib.op_str, why))
            print()
    else:
        print("*** ZERO behavioural differences: these are ONE BODY at two "
              "addresses. Porting it once closes both. ***")
    return 0


if __name__ == "__main__":
    sys.exit(main())
