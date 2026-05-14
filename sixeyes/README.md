# SixEyes

Open-source 6 DOF robotic arm for Vision–Language–Action (VLA) research.

NodeMesh-specific firmware and IC-design artifacts were moved to: [https://github.com/SixEyes-Open-Source/nodemesh](https://github.com/SixEyes-Open-Source/nodemesh).

This repository follows the authoritative system specification in `SixEyes Technical Reference.txt` and implements a distributed embodied AI platform where the laptop handles intelligence and ESP32 devices handle deterministic control and safety.

Repository layout (top-level):

```
sixeyes/
├── firmware/
│   ├── camera_esp32s3/
│   ├── follower_esp32/
│   └── leader_esp32/
├── ros2_ws/
│   └── src/
├── simulation/
├── hardware_assets/
│   ├── 3d_print_stl/
│   └── pcb_schematic_gerber/
├── docs/
└── tools/
```

See `docs/` for the safety checklist and build instructions.
