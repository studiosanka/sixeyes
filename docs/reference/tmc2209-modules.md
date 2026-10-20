# TMC2209 modules: what we tried and learned

A log of the work on the stepper driver modules, so the next person doesn't repeat it. For the schematics themselves, see [the module schematics comparison](tmc2209-module-schematics.md).

## Where we ended up

- The joint board supports **two modules only: BIGTREETECH TMC2209 V1.2 and MKS TMC2209 V2.0**. Their schematics were checked.
- **UART mode is the recommended mode** (software current and step size, no pot to adjust, faults readable). The board still works **without UART** as a fallback: the firmware then drives the MS pins (1/8 or 1/16 microsteps) and refuses to enable the driver if the pot is set too high. The ADC and VREF wiring stay on the board for that fallback.
- **UART has been confirmed once, on an unidentified "V985" module at VM 9 V** (see below), but **not** on a BIGTREETECH V1.2 or MKS V2.0, and not yet at 24 V.

## What we tested (breadboard, ESP32-S3 test rig)

The test rig is a standalone BLE-controlled STEP/DIR rig (not in this repository). It was extended with a UART probe that sends a raw read request to all four UART addresses and prints the bytes it gets back.

| Modules tried | Two modules bought online, **neither confirmed as any known model**. One had no silkscreen at all (listed as BigTreeTech-style, unconfirmed), the other was marked "V985 TMC2209" |
|---|---|
| Probe result | A clean echo of our own request at every address (`05 00 06 6F` and so on), and **no reply from the chip**, with VM on, on either PDN pin |
| Ruled out | The ESP32 and the firmware (a loopback test gave exact echoes), the resistor (1 kΩ in series with TX), the ground path, swapping TX and RX, and either PDN pin |
| Mistakes that looked like a dead module | RX wired to the wrong side of the 1 kΩ, a resistor removed so TX overpowered the chip's reply, a breadboard contact giving garbage |
| V985 pads (continuity test) | Same layout as the BIGTREETECH: three pads for PDN and three for SPREAD. The middle PDN pad goes to chip pin 14, and header pins 4 and 5 each go to one outer pad. A 0 Ω resistor already joins **pin 4 to pin 14**, and the SPREAD resistor joins SPREAD to GND |
| Still open | Why the V985 stayed silent with its UART link already in place on pin 4. Its pad links were only inspected afterwards, and the unmarked module's were never inspected |

**Lesson for builders:** the line can look perfect and the chip still stays silent. The module's own solder jumper (T1 on the BIGTREETECH) decides whether a header pin reaches the chip.

## UART confirmed working (V985, VM 9 V, 2026-10-20)

With the motor disconnected, VM at 9 V, TX through 1 kΩ and RX direct on header pin 4, the probe got a valid reply at address 0: `05 00 06 6F` (our echo) then `05 FF 06 21 00 02 41 9D` (IOIN, version byte 0x21 = TMC2209). The firmware then reported "TMC2209 on UART: ok". The IOIN bits show SPREAD low (stealthChop) and MS1/MS2 low.

It also replied at **VM = 12 V** (two boots, after being silent in a run where VM was not confirmed as 24 V). So far UART works at 9 V and 12 V and has been silent at 24 V.

Not explained: earlier tests on the same pin were silent at 24 V. Possible causes are the 24 V supply or its ground, or the wiring. Measured at the time: 60 Ω between module pin 4 and GPIO18 (a bad contact) and VDD sagging to 2.7 V when VM was applied. Both need fixing before trusting any result. **Next test: raise VM to 24 V with the motor disconnected and probe again.**

## Stalling at speed (STEP/DIR mode, 24 V, 8 microsteps, 1:25 gearbox)

- Smooth up to about 60 deg/s on the output shaft, then rough, then stopped at higher speed or acceleration.
- **More current is not the fix.** The supply reading is not the motor current (the driver converts voltage into current), and torque falls with speed anyway.
- Most likely cause: **stealthChop**, the quiet mode the module uses by default, has less torque at speed than spreadCycle. In standalone mode it is chosen by a module pad, not software. Other suspects: the real Vref (never measured), acceleration, and resonance near 300 rpm at the motor.
- **Fix to try:** bridge T2 toward VDD (BIGTREETECH) or fit R9 (MKS) for spreadCycle.

## A safety incident

During an early test on a breadboard, the pot was raised in small steps and a module caught fire underneath. The cause was never confirmed. The most likely causes are a short or poor contact on the breadboard, a voltage spike on VM, or a shorted pot. This is why the build guide says: use an inline fuse, never use a breadboard for 24 V or motor wires, set Vref with a meter and start low, and never connect or disconnect the motor while powered.

## Facts worth keeping

- **BIGTREETECH V1.1 manual (published by the maker, not one of our test modules):** UART is on header pin 4 at the factory. Vref range is 0.2 to 2.2 V, the factory setting is 1.2 V (about 0.9 A), and turning the pot **counter-clockwise raises** it. VM is 4.75 to 28 V.
- **Current from Vref:** with 0.11 Ω sense resistors, `I_rms ≈ Vref ÷ 1.41`, and the chip's maximum is 1.77 A RMS. A small module without a heatsink cannot hold that. A generic 40 mm NEMA17 is usually rated about 1.5 A per phase (sellers disagree: check yours).
- **Microstep pins disagree:** the Trinamic datasheet and BIGTREETECH's own tables (V1.1 and V1.2 even differ from each other) disagree on three of the four MS1/MS2 combinations. All sources agree that both pins low gives 1/8 and both high gives 1/16, so those are the only two we use.
- **SPREAD:** in UART mode a high level on the SPREAD pin is believed to invert the register setting, so leave it at its default when using UART.
- **ESP32-C6 pins:** GPIO5 and GPIO15 are strapping pins, but with the default eFuses the strap only affects SDIO timing (GPIO5) or JTAG source (GPIO15), so they are safe for UART TX and MS1. GPIO16 is the boot-log pin and can wiggle MS2 during boot while the driver is disabled.

## Module jumpers ship with a resistor already fitted

A photo of the underside of a BIGTREETECH module shows a small component (taken to be a 0 Ω resistor) already fitted in both the PDN and SPREAD pad groups. This matches the V1.1 manual, which says UART is connected to pin 4 at the factory and that using pin 5 means moving the resistor. So for SPREAD, **move** the resistor to the VDD pad. Never leave it on GND and also join VDD: that shorts the module's logic supply. It is not known whether the modules we tested had the same factory links, because neither was confirmed as a BIGTREETECH module, and the photo is not confirmed to be of either one. **Check each module's pads with a meter before relying on this.**

## Open questions

1. Does UART work on a BIGTREETECH V1.2 (T1 bridged) and an MKS V2.0? Nobody has tested either.
2. Does changing the MS pins while powered change the resolution, and what do the other combinations do on the real chip?
3. Does the MKS pin-4 resistor (R8) really read 0 Ω?
4. Why did the V985 not answer on UART with pin 4 linked at the factory? Candidates: the link is not really 0 Ω (a series resistor of about 1 kΩ plus our 1 kΩ TX resistor is the known "2 kΩ" problem), a counterfeit or damaged chip, or something else on the module. Its chip marking has not been checked.
