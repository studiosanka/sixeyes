# 2. Parts and bill of materials

**Status: draft.** The electronics, motors, servos and wire are listed. The mechanical parts (printed parts, bearings, inserts, screws, filament) are not written yet and show as `TBD`.

## Bill of materials for the whole arm

This is the shopping list for one arm (four joint boards). The same list is in [`hardware/bom/arm_BOM.csv`](../hardware/bom/arm_BOM.csv); the table below is made from it. It has **no prices**: costs are in the [budget](reference/BUDGET.md), which uses these quantities.

| Group | Item | Specification | Qty | Where to buy | In [budget](reference/BUDGET.md) | Notes |
|---|---|---|---|---|---|---|
| PCB | Joint board PCB v1.2 (bare) | 4 layers, 1.6 mm, HASL, 44.76 x 58.41 mm, 0.3 mm drills | 4 (order 5) | JLCPCB, files in hardware/pcb/gerbers | yes | One design, built four times: Base, Shoulder L, Shoulder R, Elbow. |
| PCB | LCSC parts for the 4 boards | 26 lines, see hardware/pcb/bom/v1.2_BOM_LCSC.csv (resistors, capacitors, regulators, CAN chip, inductor, terminals, headers, standoffs) | 1 order | LCSC | yes | Quantities in that file are for all four boards and include minimum order quantities. |
| Modules | ESP32-C6 Super Mini | ESP32-C6FH4, 4 MB, USB-C; check the pin layout matches the design | 4 | AliExpress / Shopee / Lazada | yes | Plugs into JMC1 and JMC2 sockets. |
| Modules | TMC2209 stepper driver module | BIGTREETECH V1.2 or MKS V2.0 only, StepStick footprint | 4 | AliExpress / Shopee / Lazada | yes | Other modules are not supported. UART mode is recommended. |
| Modules | AS5600 magnetic encoder module | With male header pins fitted (soldered flat to the back of the board) | 4 | AliExpress / Shopee / Lazada | yes | The header plastic sets the gap between the two boards. |
| Modules | Diametric magnet | 6 mm diameter, about 2 to 3 mm thick, diametrically magnetised (N42 NdFeB) | 4 (sold in packs) | AliExpress / Shopee / Lazada | yes | Glued to the rear shaft end of the motor. Thickness to be settled on the first real motor. |
| Modules | Heatsink for UBK1 (XL4016) | TO-220 heatsink with silicone pad and shoulder washer; insulated, never grounded | 4 | Local electronics store | no | Left out of the budget calculation. |
| Motors and servos | NEMA 17 stepper motor | 40 mm body, 1.5 to 1.7 A, 4-lead, at least about 0.4 N.m, ordinary single-shaft with an accessible rear shaft end | 4 | AliExpress / Shopee / Lazada | yes | The shoulder uses two motors on one gearbox. The rear shaft end must be reachable for the magnet. |
| Motors and servos | MG996R servo | 180 degree, metal gear | 3 | AliExpress / Shopee / Lazada | yes | Elbow board only. Check the variant: MG995, MG996R, 180 or 360 degrees. |
| Motors and servos | Motor lead | JST XH 2.54 mm 4-pin to the motor's 6-pin plug, about 10 cm | 4 | AliExpress | yes | The 4-pin XH end fits JDR3. |
| Wire and power | Wire for 24 V | 16 AWG stranded, red and black, about 5 m of each | 2 rolls | AliExpress / local | yes | Silicone or PVC. One wire per terminal. |
| Wire and power | Wire for CAN | 22 AWG, two colours, about 5 m of each, twisted by hand into a pair | 2 rolls | AliExpress / local | yes | 120 ohm termination is on the board. |
| Wire and power | Ferrules | For 16 AWG and 22 AWG wire, assorted kit | 1 kit | Local / AliExpress | no | One wire per terminal; the number needed is still to be counted. |
| Wire and power | 24 V supply | Regulated 24 V DC, at least 6.5 A, no higher than about 26 V | 1 | Local | no | Not in the budget. See the power budget. |
| Wire and power | Inline fuse and holder | Mini blade fuse holder with a 7.5 A blade fuse, in the 24 V (red) lead at the supply | 1 | AliExpress / local | yes | Required by the build guide. Blade fuses are fast-acting, rated 32 V DC. For the first bench test use a current-limited supply or a 5 A blade fuse. |
| Wire and power | USB-C cable | For flashing the Super Mini (USB only, no 24 V connected) | 1 | Local | no | Not in the budget. |
| Fasteners (electronics) | M3 x 14 screws | Hold the motor on the standoffs | 16 | Local / AliExpress | no | The M3 x 8 standoffs are on the LCSC list. Not priced yet. |
| Mechanical | 3D-printed parts | Printed parts for the arm and the 1:25 cycloidal gearboxes (files not published yet) | TBD | Print yourself | TBD | See the print guide when it is written. |
| Mechanical | PETG filament | 1 kg rolls | 2 (to be confirmed from the slicer) | Local / AliExpress | TBD | Check the real print mass. |
| Mechanical | Bearings | Types and sizes to be decided with the gearbox design | TBD | AliExpress / local | TBD | Needs bearings.csv. |
| Mechanical | Heat-set inserts | M3, length to be decided | TBD | AliExpress / local | TBD | Needs inserts.csv. |
| Mechanical | Screws, nuts and washers | For the gearboxes and the arm structure | TBD | AliExpress / local | TBD | Needs fasteners.csv. |
| Mechanical | Other | Grease, dowel pins, tape | TBD | Local | TBD |  |

## Notes

- **LCSC parts.** The full list with LCSC part numbers is [`hardware/pcb/bom/v1.2_BOM_LCSC.csv`](../hardware/pcb/bom/v1.2_BOM_LCSC.csv). Quantities in that file are for all four boards. The LCSC cart for all four boards comes to about **US$51** (checked 2026-10-21), before shipping. That includes the **minimum order quantities**: many small parts are sold in packs, so you will have spares, and the 6.2 kΩ resistor was down to exactly its minimum in stock. See [order quantities and minimums](reference/v1.2_PCB_Design_Reference.md#order-quantities-and-minimums-lcsc-checked-2026-10-21).
- **Parts not on LCSC.** Buy them from a local shop or a marketplace. More detail on each is in the "Not on LCSC" section of the [PCB design reference](reference/v1.2_PCB_Design_Reference.md).
- **Motors.** Ordinary single-shaft 40 mm NEMA17 motors are used; the magnet goes on the open rear shaft end. This is not yet checked on a real motor (see the [design reference §6](reference/v1.2_PCB_Design_Reference.md)).
- **Cost.** The cost of the whole arm, line by line, with low, average and high prices from a dated price survey, is in the [budget](reference/BUDGET.md). Power and wire sizing is in the [power budget](reference/POWER_BUDGET.md).

## Mechanical

Coming soon: the list of printed parts, bearings, heat-set inserts and fasteners for the 1:25 cycloidal gearboxes. Their cost is the one part of the [budget](reference/BUDGET.md) that is still unpriced.

Next: [Print the mechanics](03-print-the-mechanics.md).
