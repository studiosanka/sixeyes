# SixEyes documentation

Start here if you want to build the arm. Follow the guides in order. Pages marked **coming soon** are
planned but not written yet.

| Step | Guide | Status |
|---|---|---|
| 1 | [Overview: what you are building](01-overview.md) | draft |
| 2 | [Parts and bill of materials](02-parts-and-bom.md) | draft |
| 3 | [Print the mechanics](03-print-the-mechanics.md) | coming soon |
| 4 | [Build the PCB](04-build-the-pcb.md) | draft (check against the final silkscreen) |
| 5 | [Flash the firmware](05-flash-the-firmware.md) | coming soon |
| 6 | [Assemble and test](06-assemble-and-test.md) | coming soon |

## Reference (when you need the detail)

| Document | What it covers |
|---|---|
| [PCB design reference (v1.2)](reference/v1.2_PCB_Design_Reference.md) | The joint board: circuit, pin map, parts, wiring, motor mounting |
| [Budget: the S$250 claim](reference/BUDGET.md) | Every cost line for the whole arm, with a status (confirmed, estimated, unknown) and the running total |
| [Gearbox torque budget](reference/GEARBOX_TORQUE_BUDGET.md) | Torque needed per joint and what the NEMA17 motors and 1:25 cycloidal gearboxes provide |
| [CAN message protocol](reference/CAN_MESSAGE_PROTOCOL.md) | How the four joint boards talk to each other (draft) |
| [IC and diode pin verification](reference/ic-pin-verification.md) | How each IC's pins were checked against its datasheet, and what is still unverified |
| [TMC2209 module schematics](reference/tmc2209-module-schematics.md) | The two supported driver modules compared, with their schematic PDFs |
| [TMC2209 modules: what we tried and learned](reference/tmc2209-modules.md) | Test results, the stalling and UART investigation, and open questions |
| [Datasheets and links](reference/datasheets.md) | Manufacturer pages for the main parts |

## Where things are

- `hardware/`: the PCB (KiCad project, libraries, bill of materials), mechanical files, helper scripts.
- `firmware/`: the code that runs on each joint board.

## Licences

Documentation: CC-BY-4.0 ([docs/LICENSE](LICENSE)). Hardware designs: CERN-OHL-S-2.0
([hardware/LICENSE](../hardware/LICENSE)). Software and firmware: Apache-2.0 ([LICENSE](../LICENSE)).
