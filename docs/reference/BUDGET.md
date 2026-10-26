# Budget: the S$250 claim

The project claims a whole-arm build for about **S$250** (prints, mechanics and electronics; power supply, computer and shipping excluded, see [scope](#scope)). This page is where that claim is checked.

**How the check works:** each part gets a **low, average and high price** taken from real AliExpress listings in a [price survey](price-survey.csv). The budget is calculated at the **average**, and the low and high totals show how far prices can move. Marketplace prices change daily, so the survey is dated and the ranges are there to show that.

**Verdict so far: not yet verified.** The mechanical parts (filament, bearings, inserts, screws) are not priced yet. The priced parts already use S$186.39 at average prices, so **S$63.61 is left** for the mechanical parts (S$75.64 at low prices, S$10.94 at high prices).

## How to read this page

| Term | Meaning |
|---|---|
| **Low** | The lowest price seen for one unit (the discounted price shown on the listing). |
| **Average** | The mean of the prices seen for one unit, as shown on the listings on the survey date. **The budget total uses this.** |
| **High** | The highest price seen, including the struck-through list price (the price a listing goes back to when its discount ends). |
| `SURVEY` | Priced from the listings in the [price survey](price-survey.csv); `n` is the number of listings. |
| `ESTIMATE` | A price with no or few listings behind it (a quote or your own estimate). |
| `FIXED` | A price that does not move (the LCSC parts). |
| `TBD` | No price yet. Counts as unknown, not as zero. |

Rules for keeping it honest:

- **Quantities come from the arm [bill of materials](../../hardware/bom/arm_BOM.csv).** This page only adds prices, so change the BOM first when the design changes.
- Prices are for **one complete arm** (4 joint boards). Where a part is only sold in a pack (the magnets, the LCSC minimum order quantities), the pack is what is counted.
- Prices are in **S$**, with tax and shipping excluded. US$ prices use the rate in [Inputs](#inputs).
- The **Where to buy** column links to the listings the prices came from. Listings and discounts change, so check the price before you buy, and update the survey and this page if it has moved.
- [`price-survey.csv`](price-survey.csv) records, for each listing, the date, price, struck-through price and how the price was obtained (screenshots taken by Vincent, read by Claude).

## Inputs

| Input | Value | Date |
|---|---|---|
| US$ to S$ rate | 1 US$ = S$1.2782 | 2026-10-21 (open.er-api.com) |
| Arm size | 4 joint boards (Base, Shoulder L, Shoulder R, Elbow), 4 NEMA 17 motors, 3 servos on the elbow board | |

## Scope

| Included | Not included |
|---|---|
| Joint PCBs and all their parts | 24 V power supply |
| Modules, motors, servos, cables, wire | Computer / USB cable used to flash |
| 3D-printing filament | Printer, soldering iron, crimper, other tools |
| Bearings, heat inserts, screws | Your own time |
| | Shipping and import tax on any purchase |
| | Heatsinks for the XL4016 (small items, left out of this calculation) |

## 1. PCB and board parts

| Item | Qty | Low (S$ each) | Average (S$ each) | High (S$ each) | Cost at average (S$) | Status | Where to buy | Notes |
|---|---|---|---|---|---|---|---|---|
| Bare PCBs (JLCPCB, 4 layers, 1.6 mm, HASL) | 1 | 10.23 | 12.62 | 15.00 | 12.62 | `ESTIMATE` n=1 | [JLCPCB](https://jlcpcb.com/) | One order of 5 pcs. Low is the quote of US$8.00 (= S$10.23, 2026-10-21), possibly a first-order offer; high is your estimate; average is the middle of the two. |
| LCSC parts for 4 boards | 1 | 65.00 | 65.00 | 65.00 | 65.00 | `FIXED` | [BOM](../../hardware/pcb/bom/v1.2_BOM_LCSC.csv) | US$50.85 (= S$65.00 at 1 US$ = S$1.2782), qty-1 prices including minimum order quantities, checked 2026-10-21. |
| **Subtotal** | | | | | **77.62** | | | Low 75.23, high 80.00 |

## 2. Modules and electronics that are not on LCSC

| Item | Qty | Low (S$ each) | Average (S$ each) | High (S$ each) | Cost at average (S$) | Status | Where to buy | Notes |
|---|---|---|---|---|---|---|---|---|
| ESP32-C6 Super Mini | 4 | 3.92 | 4.30 | 8.01 | 17.20 | `SURVEY` n=2 | [1](https://www.aliexpress.com/item/1005009007946552.html), [2](https://www.aliexpress.com/item/1005006685958843.html) | Check that the chip is the ESP32-C6FH4 and the pin map matches the design. |
| TMC2209 driver module (MKS V2.0) | 4 | 3.31 | 3.99 | 6.67 | 15.96 | `SURVEY` n=2 | [1](https://www.aliexpress.com/item/1005008704098385.html) | BTT V1.2 is also supported. The 4-pack option has no spare module. |
| AS5600 module, header pins fitted | 4 | 1.99 | 1.99 | 4.80 | 7.96 | `SURVEY` n=1 | [1](https://www.aliexpress.com/item/1005012105119654.html) | The header pins stay on and are soldered to the board. |
| Magnet, 6 x 2 mm N42 radial (1 pack of 10, 4 used) | 1 | 6.95 | 6.95 | 7.47 | 6.95 | `SURVEY` n=1 | [1](https://www.aliexpress.com/item/1005012687239354.html) | Sold as a pack of 10; you must buy the pack (S$6.95, list S$7.47). Check the field at the chip on the first motor; a 3 mm magnet may be needed. |
| Motor lead, XH2.54 4-pin to 6-pin | 4 | 0.91 | 0.91 | 0.98 | 3.64 | `SURVEY` n=1 | [1](https://www.aliexpress.com/item/32811275446.html) | The 4-pin XH end fits JDR3. |
| Inline fuse holder with 7.5 A fuse (24 V lead) | 1 | 1.57 | 1.57 | 3.14 | 1.57 | `SURVEY` n=1 | [1](https://www.aliexpress.com/item/1005008490074072.html) | Mini blade (automotive) holder with a 7.5 A fuse, S$1.57 on 2026-10-27 (list S$3.14). Blade fuses are fast-acting and rated 32 V DC, fine for 24 V. |
| Wire, 22 AWG silicone, 5 m (CAN bus) | 2 | 2.66 | 2.66 | 2.66 | 5.32 | `SURVEY` n=1 | [1](https://www.aliexpress.com/item/1005006250522147.html) | Two 5 m sets (S$2.66 each, no list price shown), so two colours can be twisted into the CAN pair. |
| Wire, 16 AWG PVC (UL1007 type), 5 m (24 V power) | 2 | 4.53 | 4.53 | 4.53 | 9.06 | `SURVEY` n=1 | [1](https://www.aliexpress.com/item/1005012122785303.html) | One red and one black 5 m roll (S$4.53 each, no list price shown). The price was read for the grey variant; red and black may differ slightly. PVC is stiffer than silicone. |
| **Subtotal** | | | | | **67.66** | | | Low 63.42, high 106.83 |

## 3. Motors and servos

| Item | Qty | Low (S$ each) | Average (S$ each) | High (S$ each) | Cost at average (S$) | Status | Where to buy | Notes |
|---|---|---|---|---|---|---|---|---|
| NEMA 17 stepper motor, 40 mm | 4 | 5.08 | 6.43 | 9.21 | 25.72 | `SURVEY` n=3 | [1](https://www.aliexpress.com/item/33023193026.html), [2](https://www.aliexpress.com/item/32869490787.html), [3](https://www.aliexpress.com/item/1005008459399126.html) | Single-shaft motors are fine: the magnet goes on the open rear shaft end (see the [design reference §6](v1.2_PCB_Design_Reference.md)). Not yet checked on a real motor. Torques are the sellers' claims. |
| MG996R servo | 3 | 5.13 | 5.13 | 5.13 | 15.39 | `SURVEY` n=1 | [1](https://www.aliexpress.com/item/1005002990973782.html) | One sample and no list price shown, so low, average and high are the same. Check the variant (MG995, MG996R, 180 or 360 degrees). |
| **Subtotal** | | | | | **41.11** | | | Low 35.71, high 52.23 |

## 4. Mechanical parts

The full lists will live in [`hardware/mechanical/`](../../hardware/mechanical/) (one CSV per list). This page only keeps one line per group so the total stays visible.

| Group | Detail list | Cost (S$) | Status | Notes |
|---|---|---|---|---|
| PETG filament | 2 rolls (1 kg each), colour TBD | TBD | `TBD` | Check the real print mass from the slicer for all parts (including supports and failed prints) and write it here. |
| Bearings | `bearings.csv` (not written yet) | TBD | `TBD` | Type, size, qty per gearbox, qty in total. |
| Heat-set inserts | `inserts.csv` (not written yet) | TBD | `TBD` | M3 length and qty. |
| Screws, nuts, washers | `fasteners.csv` (not written yet) | TBD | `TBD` | Includes the M3x14 screws for the motor standoffs. |
| Other (grease, dowel pins, tape) | | TBD | `TBD` | |
| **Subtotal** | | **TBD** | | |

## Summary

| Section | At low prices (S$) | **At average prices (S$)** | At high prices (S$) | `TBD` lines |
|---|---|---|---|---|
| 1. PCB and board parts | 75.23 | **77.62** | 80.00 | 0 |
| 2. Modules and electronics | 63.42 | **67.66** | 106.83 | 0 |
| 3. Motors and servos | 35.71 | **41.11** | 52.23 | 0 |
| 4. Mechanical parts | not priced | **not priced** | not priced | 5 |
| **Total priced so far** | **174.36** | **186.39** | **239.06** | **5** |
| **Claim** | 250.00 | 250.00 | 250.00 | |
| **Left for the mechanical parts** | **75.64** | **63.61** | **10.94** | |

One thing makes the average total optimistic: several lines (AS5600, magnets, motor leads, servos) have only one listing behind them. The motor price assumes the magnet can go on the rear shaft end of an ordinary single-shaft motor; if the first real motor shows that it cannot, the motor line must be re-priced. If a price changes, a line is added or the room left turns out too small for the mechanical parts, say so here and either lower the cost or change the claim in the [README](../../README.md) and the [overview](../01-overview.md).

## Changelog

| Date | Change |
|---|---|
| 2026-10-21 | Page created with the LCSC estimate, listing prices and a US$8 PCB quote. Shipping and import tax removed from the scope. |
| 2026-10-27 | Changed to planning prices at the high end of the usual range, with no listing links. Added the wire lines and a S$15 PCB estimate. LCSC kept as fixed. |
| 2026-10-27 | Heatsink and fuse removed from the calculation. Motors S$15 each, servos S$5 each. |
| 2026-10-27 | Single-shaft motors accepted (magnet on the open rear shaft end); the dual-shaft caveat was removed. |
| 2026-10-27 | Inline fuse added back (7.5 A mini blade holder, S$1.57). Heatsinks stay out. |
| 2026-10-27 | 24 V power wire changed from 14 AWG to 16 AWG and priced from a listing (red and black, 5 m each, S$4.53 each). |
| 2026-10-27 | CAN wire priced from a listing (2 x 5 m, 22 AWG, S$2.66 each). |
| 2026-10-27 | Price survey added ([price-survey.csv](price-survey.csv)). The budget now uses the average survey price, shows the low and high ranges, and links to the AliExpress listings. |
