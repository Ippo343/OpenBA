// The name<->address mirror's last rows (B3R W0-SM MIRROR, 2026-09-28).
//
// B3R W2-RT: the seven address rows this file registered copied bodies from
// name rows that RT-UI / RT-VG deleted in W1, so each registered NULL (the
// registry drops a null), and no code reads any of the seven addresses
// (0x180002470 0x1800031f0 0x180035020 0x1800d4bb0 0x1800fdb20 0x18011cbe0
// 0x18011d050). They are gone. This registers nothing and stays defined only
// because standalone/resolve_standalone.cpp's RuntimePortInit() still calls it:
// the coordinator removes that call and this file together.

void RtMirror16Register() {
}
