#!/usr/bin/env python3
"""Generate a brass hex standoff STEP (needs `pip install cadquery`).

Female-female M3 hex standoff (LCSC C551328, Shuntian M3*8), 8 mm, 4.7 mm across
flats, through bore at the M3 tap size (2.5 mm; threads are not modelled). Z up, base on z=0, extends to z=LENGTH.
Writes `Custom Assets/M3x8_brass_standoff.step`; add_custom_parts.py copies it into
the SixEyes_custom library.
"""
import sys
from pathlib import Path

import cadquery as cq

LENGTH = 8.0           # mm
ACROSS_FLATS = 4.7     # mm, Shuntian datasheet (LCSC C551328, same datasheet as the whole M3*3-30 family): 4.70 +-0.1 measured 4.65
BORE = 2.5             # mm, M3 tap drill
CHAMFER = 0.3          # mm, end edges
BRASS = (0.71, 0.60, 0.25)

OUT = (Path(__file__).resolve().parent.parent / "hardware_assets" / "pcb_project_files" /
       "v1.0" / "SixEyes_v1.2" / "Custom Assets" / "M3x8_brass_standoff.step")


def main():
    across_corners = ACROSS_FLATS / 0.8660254037844386   # polygon() takes circumscribed dia
    body = (cq.Workplane("XY").polygon(6, across_corners).extrude(LENGTH)
            .faces(">Z or <Z").edges().chamfer(CHAMFER)
            .faces(">Z").workplane().hole(BORE))
    asm = cq.Assembly(name="M3x8_brass_standoff").add(body, name="standoff",
                                                       color=cq.Color(*BRASS))
    asm.save(str(OUT))
    bb = body.val().BoundingBox()
    print(f"wrote {OUT}\n  {bb.xlen:.2f} x {bb.ylen:.2f} x {bb.zlen:.2f} mm, volume {body.val().Volume():.0f} mm3")


if __name__ == "__main__":
    sys.exit(main())
