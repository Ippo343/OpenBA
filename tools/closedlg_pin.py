#!/usr/bin/env python3
"""
tools/closedlg_pin.py -- Wave 10 lane CLOSEDLG

WHAT THIS IS, AND WHAT IT IS NOT.

It is a PRE-FLIGHT ON THE INSTRUMENT: it reads the shipped Runtime-x64.dll and
checks that the exact instruction encoding this lane's port claims at each cited
address is really there. Every offset in runtime/src/ui/userinterface_close.cpp
is written as a raw byte offset with the retail address that proves it; this
turns "the comment says 0x180121011 stores the drag dialog" into something a
machine rejects when it is false.

It is NOT an oracle and it is NOT a post-state comparison. It cannot tell you
that the port BEHAVES like retail -- it only tells you that the transcription
was read off the right bytes. A green board here with a red board at runtime
means the reading was right and the writing was wrong; a red board here means
the reading was wrong and every number downstream of it is suspect.

THE NEGATIVE CONTROL IS BUILT IN. `--control` corrupts one byte of each expected
pattern before comparing. Every pin must then fail. A pin that stays green under
--control is comparing something that cannot express a difference
(reference_diagnostic_field_cannot_express_the_answer.md).

    tools/ghidra_mcp/.venv/Scripts/python.exe tools/closedlg_pin.py
    tools/ghidra_mcp/.venv/Scripts/python.exe tools/closedlg_pin.py --control
"""
import sys

import pefile

DLL = r"C:\GOG Games\Banished\Runtime-x64.dll"
BASE = 0x180000000

# (retail VA, expected bytes, what the port claims about it)
PINS = [
    # --- the prologue the detour patches ------------------------------------
    (0x180120F20, "48 89 54 24 10",
     "MOV [RSP+0x10],RDX -- param_2 into the RDX home slot; the detour's first "
     "instruction, and NOT a MOV RAX,RSP (so Bug Class 3 cannot fire)"),
    (0x180120F25, "55 56 57 48 83 EC 60",
     "PUSH RBP/RSI/RDI + SUB RSP,0x60 -- ends the 12-byte patch on an exact "
     "instruction boundary at 0x180120f2c"),

    # --- block A: the child recursion ---------------------------------------
    (0x180120F46, "83 7B 48 00",
     "CMP dword [RBX+0x48],0 -- the child COUNT is the loop bound, and it is "
     "what the recursive call decrements (this is the arming predicate)"),
    (0x180120F67, "48 8B 4B 10",
     "MOV RCX,[RBX+0x10] -- the recursion's `this` is the CHILD's owning "
     "UserInterface, read out of the dialog, not `self`"),

    # --- block B: the dead compare with the live load ------------------------
    (0x180120F8A, "48 8B 41 30 48 3B C0",
     "MOV RAX,[RCX+0x30] ; CMP RAX,RAX -- the load is live, the compare is not"),

    # --- block E: the OnClosed virtual and the drag-cancel event -------------
    (0x180120FFB, "48 8B 01 FF 50 28",
     "MOV RAX,[RCX] ; CALL [RAX+0x28] -- vtable slot 5 = OnClosed"),
    (0x18012101B, "C7 44 24 38 0D 00 00 00",
     "the event record's +0x10 = 0xd (the drag-cancel id)"),
    (0x180121023, "C7 44 24 3C FF FF FF FF",
     "the event record's +0x14 = -1"),
    (0x180121047, "E8", "CALL Array<UI::Event>::SetSize (0x180033690, unexported)"),
    (0x18012104F, "E8", "CALL EndDragDrop (0x180120af0, ordinal 2308)"),

    # --- block F: the unlink, and the inlined MemFree ------------------------
    (0x180121088, "48 3B EA", "CMP RBP,RDX -- node == head?"),
    (0x180121095, "48 3B 69 40", "CMP RBP,[RCX+0x40] -- node == tail?"),
    (0x1801210CF, "FF 49 48", "DEC dword [RCX+0x48] -- the parent list COUNT"),
    (0x1801210FC, "E8", "CALL MemoryAllocator::Free (0x1800c6c00)"),

    # --- the two append sites pass DIFFERENT pointers ------------------------
    (0x18012115F, "4C 8B C3",
     "MOV R8,RBX -- the _dialogs path pushes the NODE"),
    (0x18012139F, "4D 8B C1",
     "MOV R8,R9 -- the _addDialogs path pushes the caller's InternalPtr"),

    # --- block H: Play(1) inlined -------------------------------------------
    (0x180121167, "48 8B 86 A0 00 00 00",
     "MOV RAX,[RSI+0xa0] -- _sounds[1] (the array starts at +0x98)"),
    (0x180121187, "0F 57 C0 F2 0F 5E 05",
     "XORPS XMM0,XMM0 ; DIVSD XMM0,[rip] -- 0.0/_secondsPerTick, NOT folded"),
    (0x1801211C6, "48 63 D5 48 6B D2 70",
     "MOVSXD RDX,EBP ; IMUL RDX,RDX,0x70 -- the channel stride, and a MOVSXD "
     "Ghidra prints as a zero extension"),
    (0x1801211E2, "8D 04 6D 00 00 00 00",
     "LEA EAX,[RBP*2] -- the handle's index half"),

    # --- block J: the compaction's dead store --------------------------------
    (0x180121346, "48 85 C9", "TEST RCX,RCX on base+i*8 -- dead, and kept"),
    (0x18012134B, "48 89 29 48 8B 41 08 48 89 01",
     "MOV [RCX],RBP ; MOV RAX,[RCX+8] ; MOV [RCX],RAX -- the dead zero-store "
     "followed by the real shift"),
    (0x18012135E, "FF 4E 30", "DEC dword [RSI+0x30] -- _addDialogs._size--"),
]


def main() -> int:
    control = "--control" in sys.argv
    pe = pefile.PE(DLL, fast_load=True)

    ok = bad = 0
    for va, pattern, why in PINS:
        want = bytes.fromhex(pattern.replace(" ", ""))
        if control:
            # Corrupt one byte. A pin that survives this is not comparing.
            want = bytes([want[0] ^ 0xFF]) + want[1:]
        got = pe.get_data(va - BASE, len(want))
        if got == want:
            ok += 1
            print(f"  OK   {va:#x}  {pattern:<32}  {why}")
        else:
            bad += 1
            print(f"  BAD  {va:#x}  want {want.hex(' ')}  got {got.hex(' ')}  {why}")

    print()
    if control:
        print(f"NEGATIVE CONTROL: {bad} of {len(PINS)} pins caught the corruption, "
              f"{ok} did NOT.")
        # Under the control EVERY pin must fail; any survivor is a dead pin.
        return 0 if ok == 0 else 1

    print(f"{ok} of {len(PINS)} pins match the shipped Runtime-x64.dll, {bad} do not.")
    return 0 if bad == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
