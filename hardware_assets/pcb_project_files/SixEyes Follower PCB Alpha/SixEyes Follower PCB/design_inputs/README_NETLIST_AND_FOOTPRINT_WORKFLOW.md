# SixEyes Follower PCB Netlist + Footprint Workflow

This workflow uses the design package pin map and footprint policy to build the actual KiCad netlist from schematic.

## Files in this folder

- `follower_nodemesh_netlist.csv`
  - Source-of-truth connection table (RefDes, pin, net)
- `follower_nodemesh_footprint_map.csv`
  - Footprint mapping by reference pattern
- `follower_nodemesh_external.net`
  - Generated KiCad external netlist for first-pass PCB connectivity import
- `../SixEyes_Custom.pretty/WireJumper_1x02_P2.54mm.kicad_mod`
  - Custom THT wire-jumper footprint
- `../fp-lib-table`
  - Project-level footprint library table including `SixEyes_Custom`

## Important note

KiCad generates the real project netlist from the `.kicad_sch` schematic. The CSV netlist here is an implementation input, not a direct KiCad netlist replacement.

The `follower_nodemesh_external.net` file is provided for first-pass bring-up when the schematic is not finished yet.

## Step-by-step in KiCad (v8/v9/v10 style)

1. Open project: `SixEyes Follower PCB.kicad_pro`.
2. Open schematic editor for `SixEyes Follower PCB.kicad_sch`.
3. Place symbols for all blocks listed in `follower_nodemesh_netlist.csv`.
4. Wire pins and add global net labels exactly as named in the CSV.
5. Annotate schematic references.
6. Run ERC and fix all pin/net errors.
7. Open Footprint Assignment tool.
8. Apply footprints using `follower_nodemesh_footprint_map.csv`.
9. Verify custom jumper footprint resolves as `SixEyes_Custom:WireJumper_1x02_P2.54mm`.
10. Update PCB from schematic (this imports the generated netlist into PCB).
11. In PCB editor, run DRC and fix missing footprint/unconnected issues.

## Optional fast path: import external netlist now

1. Open PCB editor from `SixEyes Follower PCB.kicad_pro`.
2. Use the PCB netlist import flow and select `design_inputs/follower_nodemesh_external.net`.
3. Import as a first-pass connectivity baseline.
4. Place and route as needed, then migrate to full schematic-driven update later.

## External netlist caveats

- U1 and U2-U5 are represented with single-row socket placeholders; final module/socket dual-row realization must be finalized in schematic.
- NC pins (Jx_MOTOR pins 5 and 6) are intentionally excluded from nets.
- This netlist is for connectivity bootstrap; the long-term source of truth should still be `SixEyes Follower PCB.kicad_sch`.

## Net names that must be exact

- STEP_J1, DIR_J1, PDN_J1
- STEP_J2, DIR_J2, PDN_J2
- STEP_J3, DIR_J3, PDN_J3
- STEP_J4, DIR_J4, PDN_J4
- EN_ALL
- SERVO_WRIST_PITCH, SERVO_WRIST_YAW, SERVO_GRIPPER
- UART_LEADER_RX, UART_LEADER_TX
- SD_SCK, SD_MISO, SD_MOSI, SD_CS
- +24V_MOTOR, +6V6_SERVO, +3V3_LOGIC, GND

## Single-layer prototype reminders

- Prefer THT footprints only for this spin.
- Use jumper footprints for unavoidable crossings.
- Stitch split ground regions with explicit jumper links.
- Keep +24V and motor-current routes wide and short.
