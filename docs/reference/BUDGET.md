# Budget: the S$250 claim

The project claims a whole-arm build for about **S$250** (prints, mechanics and electronics; power supply and computer excluded, see [scope](#scope)). This page is where that claim is checked. Every line has a **status**, so you can see what is a real price and what is still a guess.

**Verdict so far: not yet verified.** Most lines are still `TBD`. The verdict is filled in from the [summary](#summary) table when no `TBD` or `EST` line is left.

## How to read this page

Each line has a status:

| Status | Meaning |
|---|---|
| `OK` | Price taken from a real listing or an invoice, with the date and link/source. |
| `EST` | Estimated from a similar listing or a past purchase. Not confirmed. |
| `TBD` | No price yet. Counts as unknown, not as zero. |

Rules for keeping it honest:

- Prices are for **one complete arm** (4 joint boards), at the quantity you must really buy (minimum order and pack sizes included), not the fraction you use. Leftovers are shown in the Notes column.
- Prices are in **S$**. Foreign prices are converted at the rate written in [Inputs](#inputs). **Shipping and import tax are not counted** (see [scope](#scope)); lines that show a shipping fee in the listing note it instead.
- Put the **date checked** and a **source** (shop and link, or "invoice") on every `OK` line. Prices change; an old date is a warning.
- The totals are shown three ways: confirmed only (`OK`), plus estimates (`OK` + `EST`), and the number of `TBD` lines left.

## Inputs

| Input | Value | Date |
|---|---|---|
| US$ to S$ rate | 1 US$ = S$1.2782 | 2026-10-21 (open.er-api.com) |
| Arm size | 4 joint boards (Base, Shoulder L, Shoulder R, Elbow), 4 NEMA 17 motors, 3 servos on the elbow board | |

## Scope

| Included | Not included |
|---|---|
| Joint PCBs and all their parts | 24 V power supply |
| Modules, motors, servos, cables | Computer / USB cable used to flash |
| 3D-printing filament | Printer, soldering iron, crimper, other tools |
| Bearings, heat inserts, screws | Your own time |
| | Shipping and import tax on any purchase |

## 1. PCB and board parts

| Item | Qty | Source | Cost (S$) | Status | Notes |
|---|---|---|---|---|---|
| Bare PCBs (JLCPCB, 4 layers, 1.6 mm, HASL) | 5 | JLCPCB quote | 10.23 | `OK` | Quote US$8.00 (= S$10.23), 2026-10-21, 5 pcs, shipping not included. May be a first-order offer, so later orders could cost more. Replace with the invoice after ordering. |
| LCSC parts for 4 boards | 26 lines | LCSC | 65.00 | `EST` | **US$50.85** (= S$65.00 at the rate above) at qty-1 prices incl. minimum order quantities, checked 2026-10-21. Detail: [`v1.2_BOM_LCSC.csv`](../../hardware/pcb/bom/v1.2_BOM_LCSC.csv). |
| **Subtotal** | | | **75.23** | | includes the LCSC estimate |

## 2. Modules and electronics that are not on LCSC

| Item | Qty | Source (link) | Unit (S$) | Cost (S$) | Status | Notes |
|---|---|---|---|---|---|---|
| ESP32-C6 Super Mini | 4 | [AliExpress, ICFLY Store (item 1005009007946552)](https://www.aliexpress.com/item/1005009007946552.html) | 3.92 | 15.68 | `OK` | Cart price 2026-10-21, listing sold as 1 pc per unit. Ships separately from a global seller: see the shipping line below. |
| TMC2209 driver module, MKS V2.0, pack of 4 | 4 | [AliExpress, BEGRINT 3D Printer Store (item 1005008704098385)](https://www.aliexpress.com/item/1005008704098385.html) | 13.22 | 13.22 | `OK` | Cart price 2026-10-21 for the 4-pack option (no spare), one-time offer, 1 pack left. Check on arrival that the modules are the MKS V2.0 (see [supported modules](tmc2209-module-schematics.md)). |
| AS5600 module (Estardyn), with male header pins fitted | 4 | [AliExpress, bundle deal (item 1005012105119654)](https://www.aliexpress.com/item/1005012105119654.html) | 1.99 | 7.96 | `OK` | Cart price 2026-10-21, free shipping. The male header pins stay on and are soldered to the board; the pin plastic is the gap between the two boards, as the design assumes. |
| Diametric magnet, N42 NdFeB, 6 x 2 mm, radial | 10 (4 needed) | [AliExpress, HHCX Huahai Chuangxin (item 1005012687239354)](https://www.aliexpress.com/item/1005012687239354.html) | 6.95 | 6.95 | `OK` | Cart price 2026-10-21 for a pack of 10, 6 spare. **Check:** the design assumed about 6 x 2.5 mm; 2 mm is thinner, which changes the field at the chip. Verify on the first motor. |
| TO-220 heatsink for the XL4016 | 4 | TBD | TBD | TBD | `TBD` | |
| Motor lead: XH2.54 4-pin to 6-pin terminal, 10 cm, white | 5 (4 needed) | [AliExpress, Super 3D (item 32811275446)](https://www.aliexpress.com/item/32811275446.html) | 0.91 | 4.55 | `OK` | Cart price 2026-10-21, one spare. 4-pin XH end fits JDR3. Shipping is free because it ships with the other AliExpress-shipped items in the cart. |
| 24 V inline fuse and holder | 1 | TBD | TBD | TBD | `TBD` | Outside the board. Required by the build guide. |
| Wire (24 V, CAN, motor, servo) | | TBD | TBD | TBD | `TBD` | |
| **Subtotal** | | | | **TBD** | | |

## 3. Motors and servos

| Item | Qty | Source (link) | Unit (S$) | Cost (S$) | Status | Notes |
|---|---|---|---|---|---|---|
| NEMA 17 stepper motor | 4 | TBD | TBD | TBD | `TBD` | Needs to match the [torque budget](GEARBOX_TORQUE_BUDGET.md): record the model, rated current and holding torque here. |
| MG996R servo | 3 | TBD | TBD | TBD | `TBD` | 3 confirmed. |
| **Subtotal** | | | | **TBD** | | |

## 4. Mechanical parts

The full lists will live in [`hardware/mechanical/`](../../hardware/mechanical/) (one CSV per list). This page only keeps one line per group so the total stays visible.

| Group | Detail list | Cost (S$) | Status | Notes |
|---|---|---|---|---|
| PETG filament | 2 rolls (1 kg each), colour TBD | TBD | `TBD` | Check the real print mass from the slicer for all parts (including supports and failed prints) and write it here. |
| Bearings | `bearings.csv` (not written yet) | TBD | `TBD` | Type, size, qty per gearbox, qty in total. |
| Heat-set inserts | `inserts.csv` (not written yet) | TBD | `TBD` | M3 length and qty. |
| Screws, nuts, washers | `fasteners.csv` (not written yet) | TBD | `TBD` | Includes the M3x14 screws and 0.5 mm washers for the motor standoffs. |
| Other (grease, dowel pins, tape) | | TBD | `TBD` | |
| **Subtotal** | | **TBD** | | |

## Summary

| Section | Confirmed (`OK`) | With estimates (`OK` + `EST`) | `TBD` lines |
|---|---|---|---|
| 1. PCB and board parts | S$10.23 | S$75.23 | 0 |
| 2. Modules and electronics | S$48.36 | S$48.36 | 3 |
| 3. Motors and servos | S$0 | S$0 | 2 |
| 4. Mechanical parts | S$0 | S$0 | 5 |
| **Total (so far, not complete)** | **S$58.59** | **S$123.59** | **10** |
| **Claim** | | **S$250** | |

Update the counts and totals whenever a line changes. If the total goes over S$250, say so here and either lower the cost or change the claim in the [README](../../README.md) and the [overview](../01-overview.md).

## Changelog

| Date | Change |
|---|---|
| 2026-10-21 | Page created. Only the LCSC estimate is filled in. |
| 2026-10-21 | Shipping and import tax removed from the budget scope. PCB quote US$8 added. |
| 2026-10-21 | MKS TMC2209 4-pack option added (S$13.22). |
| 2026-10-21 | MKS TMC2209 listing removed (to be re-sourced). AS5600 header pins confirmed as intended. |
| 2026-10-21 | AliExpress cart added: ESP32-C6, MKS TMC2209, AS5600, magnets, motor leads (S$53.56 with shipping). |
| 2026-10-21 | Exchange rate set. Motor lead priced from AliExpress. LCSC estimate converted. |
