# Hardware

Everything needed to build the electronics and the mechanics of the arm.

| Folder | What is in it | Status |
|---|---|---|
| [`pcb/kicad/`](pcb/kicad/) | KiCad project for the joint board (open `SixEyes_v1.2.kicad_pro`) | layout in progress |
| [`pcb/libraries/`](pcb/libraries/) | Symbols, footprints and 3D models the project uses (nothing to install) | ready |
| [`pcb/bom/`](pcb/bom/) | Bill of materials with LCSC part numbers | ready (v1.2) |
| [`pcb/gerbers/`](pcb/gerbers/) | Files to send to a PCB manufacturer | coming soon |
| [`mechanical/stl/`](mechanical/stl/) | 3D-printable parts | coming soon |
| [`tools/`](tools/) | Scripts that generate the custom KiCad parts | for maintainers |

Open the KiCad project with KiCad 10. The project finds its libraries relative to itself.

The circuit is explained in the [PCB design reference](../docs/reference/v1.2_PCB_Design_Reference.md).

## Licence

Hardware designs are licensed under CERN-OHL-S-2.0, see [LICENSE](LICENSE).
