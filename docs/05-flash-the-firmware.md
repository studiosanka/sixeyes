# 5. Flash the firmware

**Status: coming soon.** A step-by-step guide for beginners is planned.

The firmware for the joint boards is in [`firmware/joint_node/`](../firmware/joint_node/). It is a
PlatformIO project with four build targets, one per board role: `v1_base`, `v1_shoulder_l`,
`v1_shoulder_r` and `v1_elbow`. It builds, but it has **not been tested on real hardware yet**, and some
parts are still unfinished (see the project's own README).

Next: [Assemble and test](06-assemble-and-test.md).
