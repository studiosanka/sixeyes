# IC and diode pin verification

A record of how each active part on the joint board was checked against its datasheet. The "netlist" column is what the schematic actually connects (read from KiCad's exported netlist). The "datasheet" column is where the pin function came from.

**How to read the status:** *verified* means the netlist matches a datasheet we actually read. *partly* means the pins agree with a secondary source or with the design reference, but we could not obtain the manufacturer's datasheet. Nothing here replaces a bench test.

| Part | Ref | Datasheet used | Status |
|---|---|---|---|
| MCP2562-E/P CAN transceiver (PDIP-8) | UCN1 | Microchip DS20005167C, Table 1-2 | **verified** |
| LM2940CT-5.0 5 V regulator (TO-220) | ULD1 | TI SNVS769J, pin functions (NDE package) | **verified** |
| AS5600 magnetic encoder (module) | U1 | ams/Infineon AS5600 v1-06 (chip). Module wiring from the module's own schematic | **verified at chip level**, module wiring checked earlier |
| 1N5819 Schottky diode | D1 | Vishay 88525 (1N5817/5818/5819) | **verified** (orientation only) |
| XL4016E1 buck regulator (TO-220-5) | UBK1 | not obtained. Two independent sources agree on the pin order (design reference and a datasheet summary, Rev 1.2) | **partly** |
| MBR1060 Schottky (TO-220AC) | DBK1 | not obtained | **partly**: test the part with a meter (see below) |
| P6KE30A TVS (DO-15) | DPW1 | not obtained | **partly**: standard one-direction part, band = cathode |
| TMC2209 driver module | JDR1, JDR2 | module schematics (see [tmc2209-module-schematics.md](tmc2209-module-schematics.md)) | verified for BIGTREETECH V1.2 and MKS V2.0 only |
| ESP32-C6 Super Mini | JMC1, JMC2 | ESP32-C6 datasheet v1.5 for strapping pins; the header order comes from a pinout graphic, not the board's schematic | **partly**: confirm on your board |

## MCP2562-E/P (UCN1), PDIP-8

| Pin | Datasheet | Netlist | OK |
|---|---|---|---|
| 1 | TXD | CAN_TX (GPIO14) | yes |
| 2 | VSS | GND | yes |
| 3 | VDD | +5V | yes |
| 4 | RXD | CAN_RX (GPIO20) | yes |
| 5 | VIO (MCP2562 only) | +3V3 | yes |
| 6 | CANL | CAN- | yes |
| 7 | CANH | CAN+ | yes |
| 8 | STBY | GND (normal mode) | yes |

## LM2940CT-5.0 (ULD1), TO-220

| Pin | Datasheet | Netlist | OK |
|---|---|---|---|
| 1 | IN | +6.5V | yes |
| 2 | GND | GND | yes |
| 3 | OUT | +5V (47 µF low-ESR on the output) | yes |
| tab | pin 4, GND | not connected on the board | note: the tab is at ground, so a heatsink on it is at ground |

## XL4016E1 (UBK1), TO-220-5

| Pin | Function (two sources) | Netlist | OK |
|---|---|---|---|
| 1 | GND | GND | yes |
| 2 | FB | FB_BK (1.5 kΩ to GND, 6.2 kΩ plus 33 nF to +6.5V: about 6.4 V) | yes |
| 3 | SW | SW_BK (inductor, catch diode cathode) | yes |
| 4 | VC | VC_BK (1 µF to VIN) | yes |
| 5 | VIN | +24V | yes |

Not confirmed from the manufacturer's PDF: which node the metal tab connects to. The build guide says to **insulate its heatsink and never ground it**, which is safe either way. The listing's 8 A and 12 A revisions differ in ratings, so check that the part you receive matches the BOM.

## AS5600 module (U1)

| Module pin | Chip function | Netlist | OK |
|---|---|---|---|
| VCC | VDD (3.3 V mode, the module links VDD5V and VDD3V3) | +3V3 | yes |
| OUT | analog/PWM output | not connected | unused |
| GND | GND | GND | yes |
| DIR | direction (GND = increasing clockwise) | GND | yes |
| SCL | SCL | SCL (GPIO19) | yes |
| SDA | SDA | SDA (GPIO18) | yes |
| GPO | PGO (internal pull-up) | not connected | unused |

## Diodes

- **D1, 1N5819:** anode on +5V, cathode to the Super Mini's 5 V pad.
- **DPW1, P6KE30A:** cathode (band) on +24V, anode on GND. It is a one-direction part, so a reversed supply conducts through it. That is why the inline fuse is mandatory.
- **DBK1, MBR1060:** the netlist puts pin 1 (the square pad) on the cathode, tied to SW_BK, and pin 2 on GND. **Before soldering, test the real part:** the lead that beeps to the metal tab is the cathode, and it goes in the **square pad**.

## What this check cannot prove

It does not check part values or power ratings, the polarity marks of the electrolytic capacitors, whether a footprint matches the real package outline, or the real module dimensions. It also reads connectivity only: it cannot tell if a symbol's pin numbers disagree with the real package unless a datasheet was available, which is why the "partly" rows are flagged.
