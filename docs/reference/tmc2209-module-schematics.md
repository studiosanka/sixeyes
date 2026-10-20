# TMC2209 module schematics: BIGTREETECH V1.2 vs MKS V2.0

These are the two driver modules the SixEyes joint board supports. Everything below is read from their schematic PDFs, which are stored next to this file:

- [BIGTREETECH TMC2209 V1.2 schematic (PDF)](BIGTREETECH_TMC2209_V1.2_schematic.pdf)
- [MKS TMC2209 V2.0 schematic (PDF)](MKS_TMC2209_V2.0_schematic.pdf)

**Nothing here was tested on a real V1.2 or V2.0 module.** For what we did measure on other modules, see [TMC2209 modules: what we tried and learned](tmc2209-modules.md).

## Licences of the two PDFs

| File | Licence |
|---|---|
| MKS V2.0 | The sheet says "Released under the Creative Commons Attribution Share-Alike License" (CC BY-SA 4.0) and "Designed by Stephan Watterott (Watterott electronic)". So it is a derivative of Watterott's open design. Credit them if you reuse it. |
| BIGTREETECH V1.2 | BigTreeTech publishes it in a [public repository](https://github.com/bigtreetech/BIGTREETECH-TMC2209-V1.2) that has **no licence file**. It is included here for reference only. If that is a problem, replace the PDF with the link. |

## What is the same on both modules

| Item | Both modules |
|---|---|
| Chip | TMC2209, 0.11 Ω sense resistors, one per coil (BRA and BRB to ground) |
| Power row | VM, GND, A2, A1, B1, B2, VIO, GND (same order, same wiring) |
| Signal row | EN, MS1, MS2, PDN, PDN, CLK, STEP, DIR (the CLK header pin is "NC" on the MKS until a resistor is fitted) |
| EN | pulled up to the logic supply through 20 kΩ |
| MS1, MS2, STEP, DIR | wired straight to the chip |
| PDN_UART and CLK | each pulled to GND through 20 kΩ on the module |
| VREF | 5 V through 20 kΩ, then a 20 kΩ trim pot to ground, wiper to the chip's VREF (0 to 2.5 V; on the BIGTREETECH the wiper goes through a solder jumper R8) |
| Top pads | VREF, DIAG and INDEX are single pads, **not** on the 2×8 header (BIGTREETECH: J1, J2, J5. MKS: JP5, JP4, JP3) |

## What is different

| Item | BIGTREETECH V1.2 | MKS V2.0 |
|---|---|---|
| **PDN_UART (header pins 4 and 5)** | Each pin goes to a side of a 3-pad jumper **T1**. The middle pad is PDN_UART. The module ships with a 0 Ω resistor on it (V1.1 manual: UART on pin 4 by default; to use pin 5, move the resistor). Seen as fitted in a photo of the underside of a BIGTREETECH module; which two pads it joins was not confirmed. | One pin goes directly to PDN_UART. The other goes through a resistor (**R8**). The text layer of the PDF shows a "0Ω" label, so R8 is probably a 0 Ω link, but confirm with a meter on a real module. |
| **SPREAD pin (chopper mode)** | 3-pad jumper **T2**: GND side = stealthChop, VDD side = spreadCycle. A 0 Ω resistor appears to be fitted already, so for spreadCycle move it to the VDD pad; leaving it on GND and also joining VDD shorts the logic supply. | Only a pad for a pull-up resistor (**R9**, marked NC) to +5 V. Fit it for spreadCycle; empty = stealthChop. |
| **CLK header pin** | Straight to the chip's CLK. | Through a DNP resistor (**R5**) to the chip's CLK, so the header pin does nothing until it is fitted. |
| **Microstep table printed on the sheet** | MS1/MS2: 0,0 = 1/8; 0,1 = 1/2; 1,0 = 1/4; 1,1 = 1/16 (the V1.1 sheet swaps the middle two rows) | none printed |
| **Pot direction** | counter-clockwise raises Vref (V1.1 manual); Vref range 0.2 to 2.2 V | not stated in the schematic |

**Reading the MKS drawing.** The labels overlap near the PDN and CLK resistors, so this is the one place I could be wrong. I read the header pin listed as pin 5 on the JP1 symbol (4th from EN) as the direct one, and pin 4 (5th from EN) as the one through R8. An earlier note had it the other way round. The SixEyes design does not depend on it, because it ties both header pins to the same net.

## What the SixEyes PCB does with this

- **Header pins 4 and 5** are tied to the UART net (TX through 1 kΩ, RX direct). On the BIGTREETECH, bridging T1 connects the chosen pin to the chip. On the MKS, the direct pin carries the signal, and R8 sits in parallel with a wire, so it has no effect.
- **CLK** is tied to GND on the PCB (the modules already pull it down).
- **SPREAD** is not on the header. In the recommended UART mode, leave it at its default and choose the chopper mode in software. Only in the no-UART fallback does the builder set it on the module: move the T2 resistor to the VDD pad (BIGTREETECH) or fit R9 (MKS).
- **MS1, MS2** are driven by the ESP32. Only 1/8 (both low) and 1/16 (both high) are used, because every source agrees on those two.
- **VREF** is wired from the module's top pad to an ADC pin so the firmware can check the pot setting.

See the [PCB design reference §7](v1.2_PCB_Design_Reference.md) for the pin tables and the [build guide](../04-build-the-pcb.md) for the builder's steps.

## Other modules

Any other TMC2209 module may wire pins 4, 5, CLK or SPREAD differently. They are **not supported** until someone checks their schematic the same way.
