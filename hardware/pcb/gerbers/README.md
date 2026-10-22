# Manufacturing files (Gerbers)

This folder is for the **files you send to a PCB manufacturer**: the Gerber zip and the drill files.

**Status:** v1.2 is here as [`SXE-PCB-v1.2-FAB.zip`](SXE-PCB-v1.2-FAB.zip). Upload the zip as it is. It was regenerated on 22 Oct 2026 after the terminal pinouts were added to the silkscreen. The layout is also viewable as a signed PDF in [`../drawings/`](../drawings/) (`SXE-PCB-v1.2-GBR.pdf`), but that PDF is not the fabrication data.

The zip holds 12 files: 9 Gerber layers (F.Cu, In1.Cu, In2.Cu, B.Cu, both masks, both silkscreens, Edge.Cuts), the job file, and two drill files (`PTH` plated, `NPTH` unplated: 4 mounting holes of 3.2 mm).

SHA-256 of the zip: `55d9d320bc2b96c9f05b07b41ead38bd22f445212e3355bffbb4d7b2bdc384d2`

![The bare board, front](../../docs/images/bare-pcb-top.png)

*3D render of the bare board, front. The back is in the [build guide](../../docs/04-build-the-pcb.md).*

Order settings used for v1.2 (JLCPCB):

| Setting | Value |
|---|---|
| Layers | 4 |
| Thickness | 1.6 mm, default stackup (7628, 0.5 oz inner copper) |
| Surface finish | HASL |
| Impedance control | none |
| Smallest drill | 0.3 mm |
| Board size | about 44.8 × 58.4 mm |

See [Build the PCB](../../../docs/04-build-the-pcb.md) for what to do with the board once it arrives.
