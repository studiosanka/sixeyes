# 1. Overview: what you are building

SixEyes is an open-source robotic arm designed to be built by one person for **under about S$250**,
not counting the power supply and a computer (this is [still being verified](reference/BUDGET.md)). The design target is to hold
**500 g at 500 mm reach**.

## How it is built

- **Four joint boards, one design.** The same PCB is built four times: Base, Shoulder L, Shoulder R and
  Elbow. They are linked by a two-wire CAN bus. The Base board connects to your computer over USB.
- **Stepper motors.** Four NEMA17 motors drive the joints. The shoulder uses two motors sharing one
  gearbox.
- **3D-printed cycloidal gearboxes.** Each stepper joint has a 1:25 reduction, which gives the torque a
  small motor cannot deliver on its own.
- **Position feedback.** Each board reads a magnet on the rear end of the motor shaft with an AS5600 sensor. An ordinary single-shaft motor is used.
- **Hand-solderable electronics.** The board uses through-hole parts and plug-in modules (ESP32-C6 Super
  Mini, TMC2209 driver, AS5600), so no surface-mount soldering is needed.

## Where the project is today

| Part | State |
|---|---|
| Board design (circuit, parts list) | written (v1.2) |
| KiCad schematic and PCB layout | finished (v1.2), with signed drawings and fabrication files; not yet built or tested |
| Cost check (S$250 claim) | in progress: the [budget](reference/BUDGET.md) has everything priced except the mechanical parts |
| Firmware | builds for all four board roles; **not yet tested on hardware**, several parts are stubs |
| 3D-printable parts | not published yet |
| Build guides | PCB, firmware and assembly guides are drafts; the mechanics guide is not written yet |

This is a work in progress. Expect gaps, and check the status of each guide before you buy parts.

Next: [Parts and bill of materials](02-parts-and-bom.md).
