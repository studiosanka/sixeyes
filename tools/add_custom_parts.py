#!/usr/bin/env python3
"""Generate the SixEyes_custom KiCad library (symbols, footprints, 3D models).

Parts:
  AS5600_Module            - 7-pin symbol. Three footprints (variants A/B/C), one per
                             candidate STEP model, each = the two female sockets
                             (1x3 + 1x4) with the module STEP floating above them.
                             The STEP files are copied UNMODIFIED so they can be
                             compared in the 3D viewer.
  NEMA17_40                - mechanical-only symbol (no pins), outline footprint, motor
                             STEP floats above the board, rear face toward the chip.

Run:   python tools/add_custom_parts.py            (writes the library)
       python tools/add_custom_parts.py --register (also adds it to the project's
                                                    fp-lib-table / sym-lib-table;
                                                    close KiCad first)
Tune the numbers in CONFIG, re-run, reopen the footprint in KiCad.
Geometry was read from the STEP files; see the notes beside each value.
"""
import argparse
import math
import re
import shutil
import sys
from pathlib import Path

# ---------------------------------------------------------------- CONFIG ----
ROOT = Path(__file__).resolve().parent.parent
PCB_DIR = ROOT / "hardware_assets" / "pcb_project_files"
ASSETS = PCB_DIR / "v1.0" / "SixEyes_v1.2" / "Custom Assets"
PROJECT = PCB_DIR / "v1.0" / "SixEyes_v1.2"
LIB_NAME = "SixEyes_custom"
LIB_DIR = PCB_DIR / "custom_lib"
# Model paths are written relative to the project (${KIPRJMOD} = PROJECT).
MODEL_BASE = "${KIPRJMOD}/../../custom_lib/" + LIB_NAME + ".3dshapes/"

NEMA_STEP = ASSETS / "Nema 17 40x42x5mm.STEP"
STANDOFF_STEP = ASSETS / "M3x8_brass_standoff.step"     # made by tools/make_standoff_step.py
STANDOFF_LEN = 8.0             # mm
STANDOFF_AF = 4.7              # mm across flats
STANDOFF_HOLE = 3.2            # mm, M3 clearance through the board
STANDOFF_BELOW = False         # False: stands on top of the board and carries the motor;
                               # True: hangs below the board
# Motor mounted on the standoffs: F-F standoff + M3 screw from the PCB side (M3x12).
MOTOR_INDENT = 1.5             # mm, recess around the motor's tapped holes; the standoff
                               # enters it, so the motor's flat rear face is this much higher
MOTOR_FACE_Z = STANDOFF_LEN + MOTOR_INDENT     # rear face height above the board
MOTOR_THREAD = 4.0             # mm, tapped depth from the recess floor (measured)
SHAFT_TO_IC = 2.5              # mm, rear shaft/magnet end to the AS5600 chip (measured)

# KiCad's rotation fields are negated internally: dialog X=-90 stands a Y-up STEP
# upright (STEP +Y -> board +Z). If a model looks upside down, flip the sign.
NEMA_ROT = (90, 0, 0)          # opposite sign: STEP +Y (rear face) -> board -Z (down)
# Sign of the model Y offset (only variant C uses one). If C sits off-centre in the
# 3D viewer, set this to -1.
OFFSET_Y_SIGN = +1

SOCKET_H = 0.0                 # module is soldered straight to the board (no sockets); its
                               # male-header plastic rests on the PCB. (Sockets were 8.5 mm.)

# Three candidate AS5600 module models. Rotation/offset only; STEPs are not edited.
# pads = [(x, y_fp), ...] in footprint coordinates, pins 1..7 in order:
#   3-pin row (VCC OUT GND), then 4-pin row (DIR SCL SDA GPO), ascending along the row.
# Assumption (all variants): pin 1 is the same end of both rows. VERIFY on your module.
# All three agree that the 4-pin row is shifted 0.3-0.5 mm toward -Y vs the 3-pin row.
VARIANTS = {
    "A": dict(  # GrabCAD "AS5600 module.STEP": Y-up, 23.0 mm, no pins modelled, rows 13.0 apart
        step="AS5600 module.STEP", rot=(-90, 0, 0), off_xy=(0.0, 0.0),
        plastic=0.0, top=3.0,
        pads=[(-6.5, y) for y in (-2.58, -0.04, 2.50)] +
             [(6.5, y) for y in (-4.12, -1.58, 0.96, 3.50)],
        outline=(23.0, 23.0)),
    "B": dict(  # "AS5600 module 2.step": Z-up, 23.2 x 22.65, rows 13.5 apart. Modelled
                # 180 deg turned vs A (4-pin row on -X), so it is rotated 180 here.
        step="AS5600 module 2.step", rot=(0, 0, 180), off_xy=(0.0, 0.0),
        plastic=1.6,            # board is z -1.6..0 in the STEP, so lift by its thickness
        top=3.5,
        pads=[(-6.75, y) for y in (-2.34, 0.18, 2.73)] +
             [(6.75, y) for y in (-4.08, -1.55, 0.98, 3.52)],
        outline=(23.2, 22.65)),
    "C": dict(  # "AS5600 module 3.stp": Y-up assembly, 23 mm, male headers modelled
                # (plastic 2.29 mm under the board), rows 13.47 apart. Rows are native
                # along X, so it is turned 90 deg to match A/B, and re-centred by an
                # offset because the STEP origin is the board corner.
        step="AS5600 module 3.stp", rot=(-90, 0, 90), off_xy=(-11.515, 11.48),
        plastic=2.29, top=5.87,
        chip_top=3.07,          # top of the AS5600 chip above the module underside (the 4x4x2
                                # magnet block at 3.87..5.87 in this STEP is a model artifact)
        pads=[(-6.82, y) for y in (-2.875, -0.335, 2.205)] +
             [(6.65, y) for y in (-4.525, -1.985, 0.555, 3.095)],
        outline=(23.0, 23.0)),
}
ACTIVE_VARIANTS = ("C",)       # variants written to the library; the rest are archived to
                               # custom_lib/_archive/ (kept, not in the library)
DEFAULT_VARIANT = "C"          # footprint the AS5600 symbol points at
MOTOR_VARIANT = "C"            # motor rear-face height is computed from this variant

# Slotted socket holes (long axis = X, the row-to-row direction). A 0.64 mm socket leg in
# a 1.6 x 1.0 slot can sit +/-0.3 mm from nominal, so row spacing may be off by +/-0.6 mm
# (covers 13.0 .. 13.5 mm modules). The separate 1x3 / 1x4 strips are also soldered with
# the module seated as a jig.
SLOT_DRILL = (1.6, 1.0)        # (x, y) mm
SLOT_PAD = (2.4, 1.7)          # annular ring >= 0.3 mm each side of the slot

PIN_NAMES = ("VCC", "OUT", "GND", "DIR", "SCL", "SDA", "GPO")
PIN_TYPES = ("power_in", "passive", "power_in", "input", "input", "bidirectional", "passive")

NEMA_REAR_Y = 9.6              # STEP: rear face of the motor body (shaft is at -Y)
NEMA_BODY = 42.3               # square face
NEMA_HOLES = 31.0              # M3 pattern pitch
NEMA_MOUNT_HOLES = False       # True: add 4x 3.2 mm NPTH at the 31 mm pattern
# ---------------------------------------------------------------------------


def fp_name(key):
    return f"AS5600_Module_{key}_Solder_1x3_1x4"


def module_z(v):
    """Model Z offset: the module's underside sits on SOCKET_H (0 = soldered straight to
    the board), with the modelled header plastic (if any) between."""
    return SOCKET_H + v["plastic"]


def model_block(path, off, rot):
    return (f'  (model "{MODEL_BASE}{path}"\n'
            f'    (offset (xyz {off[0]:.3f} {OFFSET_Y_SIGN * off[1]:.3f} {off[2]:.3f}))\n'
            f'    (scale (xyz 1 1 1))\n'
            f'    (rotate (xyz {rot[0]} {rot[1]} {rot[2]}))\n  )\n')


def txt(kind, text, x, y, layer):
    return (f'  (fp_text {kind} "{text}" (at {x} {y}) (layer "{layer}")\n'
            f'    (effects (font (size 1 1) (thickness 0.15)))\n  )\n')


def line(x1, y1, x2, y2, layer, w):
    return (f'  (fp_line (start {x1:.3f} {y1:.3f}) (end {x2:.3f} {y2:.3f}) '
            f'(stroke (width {w}) (type solid)) (layer "{layer}"))\n')


def rect_lines(hx, hy, layer, w):
    c = [(-hx, -hy), (hx, -hy), (hx, hy), (-hx, hy)]
    return "".join(line(*c[i], *c[(i + 1) % 4], layer, w) for i in range(4))


def as5600_footprint(key):
    v = VARIANTS[key]
    name = fp_name(key)
    hx, hy = v["outline"][0] / 2, v["outline"][1] / 2
    s = (f'(footprint "{name}" (version 20221018) (generator "sixeyes_add_custom_parts")\n'
         f'  (layer "F.Cu")\n'
         f'  (descr "AS5600 module, model variant {key} ({v["step"]}), soldered straight to the board '
         f'(male pins in slotted holes, header plastic on the PCB). Module underside {module_z(v):.2f} mm above the board.")\n'
         f'  (attr through_hole)\n')
    s += txt("reference", "REF**", 0, -hy - 1.5, "F.SilkS")
    s += txt("value", name, 0, hy + 1.5, "F.Fab")
    s += rect_lines(hx, hy, "F.Fab", 0.1)
    s += rect_lines(hx + 0.25, hy + 0.25, "F.CrtYd", 0.05)
    xs = sorted({p[0] for p in v["pads"]})
    for x in xs:
        ys = [p[1] for p in v["pads"] if p[0] == x]
        s += line(x - 1.9, min(ys) - 1.5, x - 1.9, max(ys) + 1.5, "F.SilkS", 0.12)
    for i, (x, y) in enumerate(v["pads"], 1):
        shape = "rect" if i in (1, 4) else "oval"
        s += (f'  (pad "{i}" thru_hole {shape} (at {x:.3f} {y:.3f}) '
              f'(size {SLOT_PAD[0]} {SLOT_PAD[1]}) (drill oval {SLOT_DRILL[0]} {SLOT_DRILL[1]}) '
              f'(layers "*.Cu" "*.Mask"))\n')
    s += model_block(f"AS5600_module_{key}.step", (*v["off_xy"], module_z(v)), v["rot"])
    return name, s + ")\n"


def nema_footprint():
    name = "NEMA17_40mm_Rear_Overhead"
    rear_z = MOTOR_FACE_Z                 # standoff length + indentation
    z_off = rear_z + NEMA_REAR_Y          # model z = z_off - STEP_y
    h = NEMA_BODY / 2
    s = (f'(footprint "{name}" (version 20221018) (generator "sixeyes_add_custom_parts")\n'
         f'  (layer "F.Cu")\n'
         f'  (descr "NEMA17 40 mm body, mechanical only. Floats with its rear face '
         f'{rear_z:.1f} mm above the board ({STANDOFF_LEN:g} mm standoff + {MOTOR_INDENT:g} mm indentation); '
         f'origin on the shaft axis.")\n'
         f'  (attr exclude_from_pos_files exclude_from_bom)\n')
    s += txt("reference", "REF**", 0, -h - 1.5, "F.SilkS")
    s += txt("value", name, 0, h + 1.5, "F.Fab")
    s += rect_lines(h, h, "F.SilkS", 0.12)
    s += rect_lines(h + 0.3, h + 0.3, "F.CrtYd", 0.05)
    s += '  (fp_circle (center 0 0) (end 11 0) (stroke (width 0.1) (type solid)) (fill none) (layer "F.Fab"))\n'
    s += '  (fp_circle (center 0 0) (end 2.5 0) (stroke (width 0.1) (type solid)) (fill none) (layer "F.Fab"))\n'
    s += line(-3, 0, 3, 0, "F.SilkS", 0.12) + line(0, -3, 0, 3, "F.SilkS", 0.12)
    if NEMA_MOUNT_HOLES:
        for sx in (-1, 1):
            for sy in (-1, 1):
                s += (f'  (pad "" np_thru_hole circle (at {sx * NEMA_HOLES / 2} {sy * NEMA_HOLES / 2}) '
                      f'(size 3.2 3.2) (drill 3.2) (layers "*.Cu" "*.Mask"))\n')
    s += model_block("NEMA17_40mm.step", (0, 0, z_off), NEMA_ROT)
    return name, s + ")\n"


def standoff_footprint():
    name = "Standoff_M3_8mm_Hex"
    ac = STANDOFF_AF / 0.8660254037844386 / 2          # circumradius
    z_off = -STANDOFF_LEN if STANDOFF_BELOW else 0.0
    side = "below" if STANDOFF_BELOW else "above"
    s = (f'(footprint "{name}" (version 20221018) (generator "sixeyes_add_custom_parts")\n'
         f'  (layer "F.Cu")\n'
         f'  (descr "M3 x {STANDOFF_LEN:g} mm brass hex standoff, female-female. '
         f'{STANDOFF_HOLE} mm clearance hole; body {side} the board.")\n'
         f'  (attr exclude_from_pos_files)\n')
    s += txt("reference", "REF**", 0, -ac - 1.5, "F.SilkS")
    s += txt("value", name, 0, ac + 1.5, "F.Fab")
    s += (f'  (pad "" np_thru_hole circle (at 0 0) (size {STANDOFF_HOLE} {STANDOFF_HOLE}) '
          f'(drill {STANDOFF_HOLE}) (layers "*.Cu" "*.Mask"))\n')
    # corners on +/-X, as in the STEP (cadquery polygon starts on +X)
    pts = [(ac * math.cos(math.radians(60 * i)), ac * math.sin(math.radians(60 * i)))
           for i in range(6)]
    for i in range(6):
        s += line(*pts[i], *pts[(i + 1) % 6], "F.Fab", 0.1)
    s += (f'  (fp_circle (center 0 0) (end {ac + 0.25:.3f} 0) (stroke (width 0.05) (type solid)) '
          f'(fill none) (layer "F.CrtYd"))\n')
    s += (f'  (fp_circle (center 0 0) (end {STANDOFF_HOLE / 2 + 0.35:.3f} 0) '
          f'(stroke (width 0.12) (type solid)) (fill none) (layer "F.SilkS"))\n')
    s += model_block("M3x8_brass_standoff.step", (0, 0, z_off), (0, 0, 0))
    return name, s + ")\n"


def sym_prop(idx, key, val, y, hide=False):
    h = " hide" if hide else ""
    return (f'    (property\n      "{key}"\n      "{val}"\n      (id {idx})\n'
            f'      (at 0 {y} 0)\n      (effects (font (size 1.27 1.27) ){h})\n    )\n')


def symbols(fp_nema, fp_standoff):
    left = [(0, "VCC"), (1, "OUT"), (2, "GND")]
    right = [(3, "DIR"), (4, "SCL"), (5, "SDA"), (6, "GPO")]
    a = '  (symbol "AS5600_Module"\n    (in_bom yes)\n    (on_board yes)\n'
    a += sym_prop(0, "Reference", "U", 8.89) + sym_prop(1, "Value", "AS5600 Module", -8.89)
    a += sym_prop(2, "Footprint", f"{LIB_NAME}:{fp_name(DEFAULT_VARIANT)}", -11.43, True)
    a += sym_prop(3, "Datasheet", "https://ams-osram.com/products/sensors/position-sensors/ams-as5600-position-sensor", -13.97, True)
    a += '    (symbol "AS5600_Module_0_1"\n'
    a += ('      (rectangle (start -5.08 6.35) (end 5.08 -6.35)\n'
          '        (stroke (width 0) (type default)) (fill (type background)))\n')
    for n, (i, nm) in enumerate(left):
        y = 3.81 - n * 2.54
        a += (f'      (pin {PIN_TYPES[i]} line (at -7.62 {y:.2f} 0) (length 2.54)\n'
              f'        (name "{nm}" (effects (font (size 1.27 1.27))))\n'
              f'        (number "{i + 1}" (effects (font (size 1.27 1.27)))))\n')
    for n, (i, nm) in enumerate(right):
        y = 3.81 - n * 2.54
        a += (f'      (pin {PIN_TYPES[i]} line (at 7.62 {y:.2f} 180) (length 2.54)\n'
              f'        (name "{nm}" (effects (font (size 1.27 1.27))))\n'
              f'        (number "{i + 1}" (effects (font (size 1.27 1.27)))))\n')
    a += "    )\n  )\n"
    m = '  (symbol "NEMA17_40"\n    (in_bom no)\n    (on_board yes)\n'
    m += sym_prop(0, "Reference", "M", 6.35) + sym_prop(1, "Value", "NEMA17 40mm", -6.35)
    m += sym_prop(2, "Footprint", f"{LIB_NAME}:{fp_nema}", -8.89, True)
    m += sym_prop(3, "Datasheet", "", -11.43, True)
    m += ('    (symbol "NEMA17_40_0_1"\n'
          '      (rectangle (start -5.08 3.81) (end 5.08 -3.81)\n'
          '        (stroke (width 0) (type default)) (fill (type background)))\n'
          '      (circle (center 0 0) (radius 1.27)\n'
          '        (stroke (width 0) (type default)) (fill (type none)))\n'
          '    )\n  )\n')
    h = '  (symbol "Standoff_M3_8"\n    (in_bom yes)\n    (on_board yes)\n'
    h += sym_prop(0, "Reference", "SO", 3.81) + sym_prop(1, "Value", "M3x8 brass standoff", -3.81)
    h += sym_prop(2, "Footprint", f"{LIB_NAME}:{fp_standoff}", -6.35, True)
    h += sym_prop(3, "Datasheet", "https://datasheet.lcsc.com/datasheet/pdf/e07224c868ba92d8b3bc22a0f41c0dcb.pdf?productCode=C551328", -8.89, True)
    h += sym_prop(4, "Manufacturer", "Shuntian", -11.43, True)
    h += sym_prop(5, "MPN", "M3*8", -13.97, True)
    h += sym_prop(6, "LCSC Part", "C551328", -16.51, True)
    h += ('    (symbol "Standoff_M3_8_0_1"\n'
          '      (rectangle (start -2.54 2.54) (end 2.54 -2.54)\n'
          '        (stroke (width 0) (type default)) (fill (type background)))\n'
          '      (circle (center 0 0) (radius 0.9)\n'
          '        (stroke (width 0) (type default)) (fill (type none)))\n'
          '    )\n  )\n')
    return ("(kicad_symbol_lib\n  (version 20211014)\n"
            "  (generator sixeyes_add_custom_parts)\n\n" + a + "\n" + m + "\n" + h + ")\n")


def register():
    entries = {
        "fp-lib-table": f'  (lib (name "{LIB_NAME}") (type "KiCad") '
                        f'(uri "${{KIPRJMOD}}/../../custom_lib/{LIB_NAME}.pretty") (options "") (descr ""))',
        "sym-lib-table": f'  (lib (name "{LIB_NAME}") (type "KiCad") '
                         f'(uri "${{KIPRJMOD}}/../../custom_lib/{LIB_NAME}.kicad_sym") (options "") (descr ""))',
    }
    for fname, entry in entries.items():
        p = PROJECT / fname
        t = p.read_text(encoding="utf-8")
        if f'(name "{LIB_NAME}")' in t:
            print(f"{fname}: already registered")
            continue
        k = t.rstrip().rfind(")")
        p.write_text(t[:k].rstrip() + "\n" + entry + "\n)\n", encoding="utf-8")
        print(f"{fname}: registered {LIB_NAME}")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--register", action="store_true")
    args = ap.parse_args()
    for f in [NEMA_STEP, STANDOFF_STEP] + [ASSETS / v["step"] for v in VARIANTS.values()]:
        if not f.exists():
            sys.exit(f"missing {f}")
    pretty = LIB_DIR / f"{LIB_NAME}.pretty"
    shapes = LIB_DIR / f"{LIB_NAME}.3dshapes"
    sym = LIB_DIR / f"{LIB_NAME}.kicad_sym"
    pretty.mkdir(parents=True, exist_ok=True)
    shapes.mkdir(parents=True, exist_ok=True)
    if sym.exists():
        others = set(re.findall(r'^  \(symbol "([^"]+)"', sym.read_text(encoding="utf-8"), re.M)) \
            - {"AS5600_Module", "NEMA17_40", "Standoff_M3_8", "Standoff_M3_15", "Standoff_M3_16"}  # superseded
        if others:
            sys.exit(f"{sym.name} holds other symbols {others}; not overwriting")
    archive = LIB_DIR / "_archive"
    for key in VARIANTS:
        if key in ACTIVE_VARIANTS:
            continue
        archive.mkdir(exist_ok=True)
        for src in (pretty / f"{fp_name(key)}.kicad_mod", shapes / f"AS5600_module_{key}.step"):
            if src.exists():
                shutil.move(str(src), str(archive / src.name))
    for key, v in VARIANTS.items():
        if key not in ACTIVE_VARIANTS:
            continue
        n, f = as5600_footprint(key)
        (pretty / f"{n}.kicad_mod").write_text(f, encoding="utf-8")
        shutil.copy(ASSETS / v["step"], shapes / f"AS5600_module_{key}.step")
        print(f"  {key}: {v['step']}  module underside z={module_z(v):.2f}, top z={module_z(v) + v['top']:.2f}")
    (shapes / "AS5600_module.step").unlink(missing_ok=True)   # old single-model copy
    n2, f2 = nema_footprint()
    (pretty / f"{n2}.kicad_mod").write_text(f2, encoding="utf-8")
    shutil.copy(NEMA_STEP, shapes / "NEMA17_40mm.step")
    n3, f3 = standoff_footprint()
    (pretty / f"{n3}.kicad_mod").write_text(f3, encoding="utf-8")
    shutil.copy(STANDOFF_STEP, shapes / "M3x8_brass_standoff.step")
    sym.write_text(symbols(n2, n3), encoding="utf-8")
    # drop footprints / models left by earlier runs (names changed); archived ones are untouched
    keep = {f"{n}.kicad_mod" for n in {fp_name(k) for k in ACTIVE_VARIANTS} | {n2, n3}}
    for f in pretty.glob("*.kicad_mod"):
        if f.name not in keep and f.name.split("_")[0] in ("AS5600", "Standoff", "NEMA17"):
            f.unlink()
    keep_step = {f"AS5600_module_{k}.step" for k in ACTIVE_VARIANTS} | {"NEMA17_40mm.step", STANDOFF_STEP.name}
    for f in shapes.glob("*.step"):
        if f.name not in keep_step and f.name.split("_")[0] in ("AS5600", "M3x15", "M3x16", "M3x8"):
            f.unlink()
    v = VARIANTS[MOTOR_VARIANT]
    print(f"wrote {LIB_DIR}")
    print(f"  motor rear face z={MOTOR_FACE_Z:.2f} mm ({STANDOFF_LEN:g} standoff + {MOTOR_INDENT:g} indentation)")
    if "chip_top" in v:
        chip = module_z(v) + v["chip_top"]
        mag = MOTOR_FACE_Z - SHAFT_TO_IC - chip
        print(f"  chip top z={chip:.2f}; with {SHAFT_TO_IC:g} mm shaft-to-IC the shaft/magnet end is at "
              f"z={chip + SHAFT_TO_IC:.2f}, i.e. it must protrude {mag:.2f} mm below the motor rear face")
    reach = 1.6 + STANDOFF_LEN          # head underside -> recess floor (PCB + standoff)
    for length, washer in ((12, 0.0), (14, 0.0), (14, 0.5), (16, 0.0)):
        eng = length - reach - washer
        ok = ("bottoms out" if eng > MOTOR_THREAD else "marginal (< 3 mm)" if eng < 3 else "ok")
        print(f"  screw M3x{length}{' +%.1f washer' % washer if washer else ''}: "
              f"engagement {eng:.1f} mm of {MOTOR_THREAD:g} -> {ok}")
    if args.register:
        register()
    else:
        print("not registered; run with --register (KiCad closed) or add the library by hand")


if __name__ == "__main__":
    main()
