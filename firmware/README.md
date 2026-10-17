# Firmware

[`joint_node/`](joint_node/) is the firmware for the joint boards: one PlatformIO project with four build
targets, one per board role (`v1_base`, `v1_shoulder_l`, `v1_shoulder_r`, `v1_elbow`), for the ESP32-C6
Super Mini.

**Status:** it builds for all four roles, but it has **not been tested on hardware yet** and some modules
are stubs. See [`joint_node/docs/README.md`](joint_node/docs/README.md) for what is real and what is not.

To build one role (needs [PlatformIO](https://platformio.org/)):

```
cd firmware/joint_node
pio run -e v1_base
```

A beginner's flashing guide is planned: [docs/05-flash-the-firmware.md](../docs/05-flash-the-firmware.md).

Licence: Apache-2.0, see [LICENSE](../LICENSE).
