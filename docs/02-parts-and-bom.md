# 2. Parts and bill of materials

**Status: draft.** The electronics list is complete for the v1.2 board; the mechanical parts list is not
written yet.

## Electronics (per joint board, four boards in total)

- The full list with LCSC part numbers is in
  [`hardware/pcb/bom/v1.2_BOM_LCSC.csv`](../hardware/pcb/bom/v1.2_BOM_LCSC.csv). Quantities in the file are
  for all four boards. It comes to about US$44 for the arm at single-quantity LCSC prices, before shipping.
- Parts that are **not on LCSC** (buy from a local shop or a marketplace) are listed in the section
  "Not on LCSC" of the [PCB design reference](reference/v1.2_PCB_Design_Reference.md): the ESP32-C6 Super
  Mini, the TMC2209 driver module, the AS5600 module with male headers, a small diametric magnet, a TO-220
  heatsink, JST XHP-4 plugs and crimp contacts for the motor lead, M3x14 screws with 0.5 mm washers, and
  wire.
- You also need four NEMA17 motors and a 24 V power supply (not in the cost estimate).

## Mechanical

Coming soon: the list of printed parts, bearings and fasteners for the 1:25 cycloidal gearboxes.

Next: [Print the mechanics](03-print-the-mechanics.md).
