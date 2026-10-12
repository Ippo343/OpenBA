# Fixes and extensions -- status (newest first)

## 2026-10-11 late -- EYECAM merged: the first extension

- `ext.eye_camera` (class A, default off) merged `a67c8b445`: `src/ext/eye_camera/` (eye_math pure + the
  game glue), three seams (Update step 6, Draw step 4 at the renderer's alpha + near 0.1, the zoom in
  camera_input.cpp), six unit tests; PlayerController's saved fields never written. Gate EYECAM at 0;
  stress faithful vs `/profile:all` identical (0.17 ms / iteration, 140 fps). `tools/stress_run.sh` takes
  `STRESS_PROFILE` (default faithful).
- **Owed**: the user's look (`handins/EYECAM.md` §7: 13 checks -- eye height 0.88 units, the follow
  engage / disengage, first person, hidden / removed citizen, faithful load of a save made while following).

The design is `DESIGN.md`; the classified deviation list is `INVENTORY.md`; every block's hand-in is
`handins/<BLOCK>.md`. The gate table for this phase is `docs/runtime_port/phase_d/GATE_TABLE.md`
(rows EXT1 / EXT2).

## 2026-10-11 night -- the phase opened; scaffold, three fix pilots, verification, inventory

- **Merged on main**: EXTSCAFFOLD (`src/ext/registry.*`, `Ext::Fix()` / `Ext::Ext()`, settings keys
  `fix.<id>` / `ext.<id>` / `profile=faithful|all`, the class lock, the pseudo-mod provider type, the
  Extensions dialog + button beside Mods in the front end and the pause menu, 11 unit tests),
  FIXPILOT (`fix.icon_slide` at slot 11 AND `SetFlag`, `fix.residence_goto`, `fix.tavern_counter`;
  default on), SAVETAIL (`handins/SAVETAIL.md`, five verdicts VERIFIED), EXTINVENTORY
  (`INVENTORY.md`: RESTORE 18, FIX 7, EXT-A 1, KEEP 7).
- **Gate EXT2 at 0 problems** (faithful profile forced by `/profile:faithful` on the exe's command
  line: `tools/build.sh run`, `resave_gate.sh`, `phase_d/gate.sh`): new game 5694 / 0, tutorial
  5773 / 12, loadgame 3436 / 0, typetable `965b527a`, resave 3/3 == golden (23,453,453 bytes), unit
  1013 / 0, castmetric 2, acceptance 0 off target, 0 new warnings. EXT1 (one problem, castmetric 6)
  was the dialog's three casts and the residence faithful arm's; fixed in `e2d8be2f4`.
- **Design corrections from SAVETAIL**: a class-C chunk lives at the tail of save block 1, not after
  the file (retail checks the exact file size); every B/C extension writes a manifest there; the
  pseudo-mod's format version goes in ModInfo+0x18 (+0x10 is the package CRC); both write seams emit
  zero bytes under the faithful profile.
- **Facts learned**: a loose text `.rsc` is NOT loaded by the shipped loader and `Tools-x64.exe` is
  not on this machine, so the Extensions dialog reuses `Dialog/Mod.rsc` with its texts replaced and
  draws its icon in code (EXTSCAFFOLD hand-in). The button's tooltip still reads the Mods text.
- **LOOKED at by the user (2026-10-11 night)**: the Extensions dialog's name / author / description
  rows and the enable / disable toggles work in the front end and in game; the row icon column is empty
  (the user will author programmer art later; the slot stays). The three fixes seen working: icon_slide,
  residence_goto, tavern_counter. Fixes are global and live, NOT recorded in the save (a disable + save +
  re-enable + load shows the fix enabled): confirmed as intended by the user.
- **Next**: the first extension, class A eye-level camera (seam in `INVENTORY.md` §2).
