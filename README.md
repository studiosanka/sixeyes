# SixEyes

**A ~S$250 6-DOF robotic arm with 500 g @ 500 mm capability, designed to be built at home.**

SixEyes is an open-source 5-DOF arm plus a gripper, built from 3D-printed cycloidal gearboxes, NEMA17
stepper motors and a hand-solderable PCB. The ~S$250 figure is the whole-arm build budget (prints,
mechanics and electronics; power supply and computer excluded), and 500 g at 500 mm is the design target.

> **Work in progress.** The board design and PCB layout (v1.2) are finished but not yet built or tested. The firmware builds
> but has not run on hardware yet, and the printable parts and build guides are not published yet. Check the
> [status](#status) before you buy anything.

| Front | Back |
|:-:|:-:|
| <img src="docs/images/joint-board-top-iso.png" alt="Joint board front on a NEMA 17 motor" width="100%"> | <img src="docs/images/joint-board-bottom-iso.png" alt="Joint board back on a NEMA 17 motor" width="100%"> |

*3D render of the joint board on a NEMA 17 motor (the motor is drawn transparent). Left: the front. Right: the back, with the AS5600 encoder module and the four standoffs that carry the motor. The Super Mini and TMC2209 modules are not shown in their sockets.*

## What you will build

One **Universal Joint PCB** design (ESP32-C6 Super Mini, TMC2209 driver and AS5600 encoder as plug-in or
solder-on modules, fully through-hole) is built four times and linked by a CAN bus:

| Board | Populated | Role |
|---|---|---|
| Base | Stepper 1 driver, USB-C | Bus master, link to your computer |
| Shoulder L | Stepper 2A driver | CAN node |
| Shoulder R | Stepper 2B driver | CAN node, inverse-direction logic |
| Elbow | Stepper 3 driver, 3× servo headers (open-loop) | CAN node, closest to the wrist and gripper |

- **Bus:** CAN at 1 Mbps, a linear chain Base → Shoulder L → Shoulder R → Elbow.
- **Motors:** four NEMA17 (40 mm class) behind 1:25 printed cycloidal reductions. The shoulder uses two
  motors sharing one gearbox.
- **Feedback:** an AS5600 magnetic encoder reads a magnet on each motor shaft.

## Ways to drive the arm

The Base board has two links: **USB** to a computer and **Bluetooth** to a phone. Only one of them is in
control at a time, and the arm says which in every status message. That gives three modes:

| Mode | Who drives | Link | What it is for | Software to use |
|---|---|---|---|---|
| **1. Phone** | Your phone | Bluetooth | Move the gripper by moving your phone; record a movement and replay it, with no computer | [sixeyes-drive](https://github.com/studiosanka/sixeyes-drive) |
| **2. Phone, computer records** | Your phone | Bluetooth for the phone, USB for the computer | Record training data (joint positions and what the phone commanded) while a person teleoperates | [lerobot-sixeyes](https://github.com/studiosanka/lerobot-sixeyes) (listen-only), on top of [sixeyes-host](https://github.com/studiosanka/sixeyes-host) |
| **3. Computer** | A program on your computer | USB | Scripts, motion planning, or a trained AI policy driving the arm | [sixeyes-host](https://github.com/studiosanka/sixeyes-host) (Python), [sixeyes_ros2](https://github.com/studiosanka/sixeyes_ros2) (ROS 2 and MoveIt), [lerobot-sixeyes](https://github.com/studiosanka/lerobot-sixeyes) (LeRobot policy) |

The arm needs no computer for Mode 1. Every tool also has a **practice arm**, so you can try the software
before the hardware is built.

### Software repositories

| Repository | What it is | Used in mode |
|---|---|---|
| [sixeyes-host](https://github.com/studiosanka/sixeyes-host) | The USB protocol (version 1), a Python library, a simulated arm and the `sixeyes-bringup` tool (ports, home, jog, check encoders, monitor). Every other tool builds on it | 2, 3 (and the protocol for 1) |
| [sixeyes-drive](https://github.com/studiosanka/sixeyes-drive) | A web page for Chrome on an Android phone: hold a button and move the phone, record and replay | 1 |
| [lerobot-sixeyes](https://github.com/studiosanka/lerobot-sixeyes) | Plugins for [LeRobot](https://github.com/huggingface/lerobot): record data (Mode 2) and run a policy (Mode 3) | 2, 3 |
| [sixeyes_ros2](https://github.com/studiosanka/sixeyes_ros2) | ROS 2 and MoveIt control, in a mock arm, Gazebo or the real arm | 3 |
| [sixeyes_description](https://github.com/studiosanka/sixeyes_description) | The arm's shape and joint limits (URDF), shared by ROS 2 and the phone page. Placeholder numbers until the CAD is final | 1, 3 |

How the firmware in this repository fits in: it speaks the host protocol on both links
([`firmware/joint_node`](firmware/joint_node/docs/README.md)), arbitrates who is in control, and can store
motions on the arm itself. The Bluetooth service is described there. All of these repositories are
work in progress and **none has run on a real arm yet**; each one's README has its own status table.

## Start here

1. Read the [documentation index](docs/README.md) and follow the numbered guides.
2. The parts list with LCSC part numbers is [`hardware/pcb/bom/v1.2_BOM_LCSC.csv`](hardware/pcb/bom/v1.2_BOM_LCSC.csv).
3. The circuit and wiring are explained in the [PCB design reference](docs/reference/v1.2_PCB_Design_Reference.md).
4. The S$250 claim is checked line by line in the [budget](docs/reference/BUDGET.md).

## Repository map

```
hardware/    PCB (KiCad project, libraries, bill of materials), mechanical files, helper scripts
firmware/    joint_node: the code that runs on the four joint boards
docs/        build guides (start with docs/README.md) and reference documents
```

The software that talks to the arm lives in separate repositories under
[studiosanka](https://github.com/studiosanka); see [Ways to drive the arm](#ways-to-drive-the-arm).

Earlier generations of the project (Alpha and Beta boards, experiments) are kept for reference on the
`archive/dev-2026-10` branch.

## Status

| Part | State |
|---|---|
| Board design v1.2 (circuit, parts list) | ✅ written |
| KiCad schematic and PCB layout | ✅ v1.2 complete (checks clean, not yet built or tested) |
| Manufacturing files (Gerbers) | ✅ v1.2 in [`hardware/pcb/gerbers/`](hardware/pcb/gerbers/) |
| [Budget (S$250 claim)](docs/reference/BUDGET.md) | 🚧 being verified; S$186 priced at average survey prices, S$64 left for the mechanical parts |
| Firmware (`firmware/joint_node`) | 🚧 builds for all four board roles, with the host link, Bluetooth, homing and motion slots; not yet run on hardware |
| Host software (USB, phone, ROS 2, LeRobot) | 🚧 written in [separate repositories](#software-repositories); tested against practice arms only |
| CAN protocol | 🚧 draft |
| 3D-printable parts | ⬜ not yet published |
| Build guides | 🚧 outlines (overview and parts list drafted) |

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md) for the usage terms and contribution policy.

## Licence

SixEyes uses three licences:

| | Licence |
|---|---|
| Firmware and software | [Apache 2.0](LICENSE) |
| Hardware designs (KiCad, Gerbers, STL) | [CERN-OHL-S-2.0](hardware/LICENSE) |
| Documentation | [CC-BY-4.0](docs/LICENSE) |

See [CONTRIBUTING.md](CONTRIBUTING.md) for what is required if you manufacture or sell hardware based on these
designs.

## AI assistance

The firmware, software and documentation in this repository were developed with AI assistance from
[Claude](https://www.anthropic.com/claude) (Anthropic) and [GitHub Copilot](https://github.com/features/copilot).
The designs, decisions and review are by the Studio Sanka team.

## Credits

| Role | Contributor |
|---|---|
| Project Lead | Vincent Santosa ([@markantosa](https://github.com/markantosa)) |
| Firmware & System Architecture | Vincent Santosa ([@markantosa](https://github.com/markantosa)) |
| Joint PCB Design (v1) and earlier Follower PCB Design | Vincent Santosa ([@markantosa](https://github.com/markantosa)) |
| Leader PCB Design (Alpha/Beta, archived) | Audrey ([@odriyy](https://github.com/odriyy)) |
| Mechanical Design (cycloidal drive, original design) | Ren Jie ([@trenjie03](https://github.com/trenjie03)) |
| Mechanical Design (continuing the cycloidal drive design) | Chai ([@sevenovaa](https://github.com/sevenovaa)) |

## Contact

Bugs and questions: https://github.com/studiosanka/sixeyes/issues

---

Built by [Studio Sanka](https://github.com/studiosanka), a non-profit mechatronics design collaborative in
Singapore. **Last updated:** October 2026.
