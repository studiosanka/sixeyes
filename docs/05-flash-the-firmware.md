# 5. Flash the firmware

**Status: draft.** The firmware builds for all four board roles, but it has **not been tested on a real SixEyes board yet**, and the motor movement is not written yet (see [what works](#what-works-today)). Flashing and checking that the driver talks UART does work today.

## Before you start

- **Flash with USB only.** Don't connect the 24 V supply or the motor while you flash.
- Install [VS Code](https://code.visualstudio.com/) and the **PlatformIO** extension.
- Get this repository (`git clone https://github.com/studiosanka/sixeyes`) and open the folder `firmware/joint_node` in VS Code.

## Pick the right role

The same firmware runs on every board. Each board needs the build for **its own role**:

| Board | Environment |
|---|---|
| Base | `v1_base` |
| Shoulder L | `v1_shoulder_l` |
| Shoulder R | `v1_shoulder_r` |
| Elbow | `v1_elbow` |

## Flash it

1. Plug the Super Mini into your computer with a USB-C cable. It uses its own USB port.
2. In a terminal, in `firmware/joint_node`, run (use your role from the table):
   ```
   pio run -e v1_base -t upload
   ```
3. If it can't find the board, **hold BOOT, tap RESET, release BOOT**, and run the command again.
4. Open the serial monitor:
   ```
   pio device monitor
   ```
   If you don't see the start-up messages, **press RESET** with the monitor open.

## Check that UART works (the important check)

After a reset, the monitor must show the driver starting. **This is what you want to see:**

```
TMC2209Driver: initializing single-wire PDN_UART interface...
  Motor 0: UART ok, initialized (disabled)
```

**If you see this instead, stop:**

```
  - WARNING: UART not answering. UART mode is the recommended mode.
```

The board is not ready, and the motor must not be used. Switch off and check, in order:

1. The driver module's PDN link (**T1** on the BIGTREETECH) joins the middle pad to pin 4 or 5. See [Build the PCB, Step 5](04-build-the-pcb.md).
2. The driver module is the right way round (EN marker) and has motor power (VM) connected. **The chip does not answer UART without VM.**
3. No solder bridges on the headers.

## What works today

- The board starts, sets the driver to a safe **500 mA RMS** run current, and holds the motor **disabled** at boot.
- It reads the AS5600 encoder.
- The CAN safety logic and the E-stop path are written.

**Not written yet:** the step generation (the motor will not move), homing, and the translation between the computer link and CAN. Check the [firmware README](../firmware/joint_node/docs/README.md) for the current list.

## Next

[Assemble and test](06-assemble-and-test.md).
