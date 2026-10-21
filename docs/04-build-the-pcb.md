# 4. Build the PCB

**Status: draft.** The layout is still being finished, so check every part name against the **silkscreen on your board**. The silkscreen is always right. The names here match the current design.

![The finished joint board on a NEMA 17 motor, front and back](images/joint-board-assembly.png)

*3D render of the joint board on a NEMA 17 motor (the motor is drawn transparent). Left: the front. Right: the back, with the AS5600 encoder module and the four standoffs that carry the motor. The Super Mini and TMC2209 modules are not shown in their sockets.*

## Safety first (read this before you touch anything)

- **Wear safety glasses.** Solder can spit.
- **Work with fresh air.** Use a fan or an open window. Don't breathe the smoke from the solder.
- **The iron is very hot.** Never touch the metal tip. Put it back in its stand every time. Unplug it when you finish.
- **Wash your hands** after soldering, and don't eat or drink while you work.
- **Tie back long hair** and keep wires and sleeves away from the iron.
- **Never power the board without an inline fuse** in the 24 V wire. A backwards supply or a short can start a fire without one.
- **Never plug or unplug the motor, or any module, while the power is on.**
- **Capacitors hold charge.** After power off, wait 30 seconds before you touch the board.
- **Never use a breadboard** for 24 V or motor wires. Use screw terminals or soldered joints.
- **Smell, smoke or a very hot part? Switch off at the power supply first**, then look.
- **Ask a mentor to watch your first power-up.**

## What you need

- Soldering iron with a fine tip and a stand, solder, tweezers, flush cutters
- Multimeter, magnifying glass, safety glasses
- Plastic screwdriver (for the module's current pot)
- Inline fuse holder and a **3 A slow-blow fuse** for the first bench test, and a bench supply with a current limit if you have one
- The board and the parts in the [bill of materials](../hardware/pcb/bom/v1.2_BOM_LCSC.csv). The signed [schematic](../hardware/pcb/drawings/SXE-PCB-v1.2-SCH.pdf) and [layout plots](../hardware/pcb/drawings/SXE-PCB-v1.2-GBR.pdf) show where everything goes
- **One supported driver module: BIGTREETECH TMC2209 V1.2 or MKS TMC2209 V2.0.** Other modules are not supported.

## Rules for every part

1. Match the part to the **silkscreen outline**.
2. Check **polarity** (stripe, dot, notch) *before* you solder.
3. Solder **one pin**, check the part sits flat and straight, then solder the rest.
4. A good joint is shiny and shaped like a small cone. Dull or ball-shaped means reheat it.
5. Trim the long legs with flush cutters after soldering.

## Step 1: the inductor (L1 / LBK1)

It is the only surface-mount part, and it needs space, so it goes **first**.

1. Put a little solder on one pad.
2. Hold the inductor with tweezers on the pads and reheat that pad until it settles flat.
3. Solder the other pad. Check both joints have melted solder, not just a blob.

## Step 2: the back side

Turn the board over. Follow the **back silkscreen** exactly.

1. **Resistors first:** **R2 (6.2 kΩ)**, **R5 (10 kΩ)**, and **RSV1, RSV2, RSV3 (1 kΩ)** on the **Elbow only**. Solder from the opposite side, then trim the legs.
2. **The TVS diode DPW1 (P6KE30A).** **The stripe on the part must match the stripe on the silkscreen.** It protects the 24 V input, so a backwards one will not protect anything.
3. **Then the AS5600 module (U1).** It has **no sockets**: its pins are soldered **straight into the board**.
   - Match the module to the silkscreen outline, with its pins in the right holes and the right way round.
   - Keep it **flat and square** to the board while you solder one pin, then check, then do the rest. The distance between the sensor chip and the motor magnet depends on it.
   - Don't bend the module after it is soldered.

## Step 3: the front side, small to tall

Work in this order. Tick each group when it is done.

1. **Resistors:** **R1 (1.5 kΩ)**, **R3 (1 kΩ)**, **R4 (10 kΩ)** and **RCN1 (120 Ω)**. Use the multimeter if you can't read the colours.
2. **Diodes:** **D1 (1N5819)** and **DBK1 (MBR1060)**. **The stripe on the part must match the stripe on the silkscreen.** For DBK1, test first: the lead that **beeps to the metal tab** is the cathode, and it goes in the **square pad**.
3. **Small capacitors:** 1 µF (C1, C2, CC1), 100 nF (CCN1, CCN2, CIN_LD1) and 33 nF (CF1). They have no polarity.
4. **The CAN chip UCN1 (MCP2562):** the **notch or dot is pin 1**. Match it to the notch on the silkscreen.
5. **Headers and sockets:**
   - JMC1, JMC2 (the Super Mini) and JDR1, JDR2 (the driver module). **Tip:** plug the sockets onto the module first and solder them with the module in place, so they stay parallel.
   - **JDR4**, a 2-pin socket next to the EN marker. The module's VREF and DIAG pads connect here (Step 6).
   - JCN3 (2-pin jumper), and JSV1 to JSV3 (servo headers) on the **Elbow only**.
6. **Terminals:** the screw terminals JPW1, JPW2, JCN1, JCN2 with the wire openings facing the board edge. The JST motor connector JDR3 with its opening facing the edge.
7. **Electrolytic capacitors (COUT_LD1, CDR1, COUT1, CIN1):** the **stripe is the minus side**. Match it to the marked side on the silkscreen. A backwards capacitor can burst.
8. **The two big regulators (ULD1 LM2940 and UBK1 XL4016E1):** the **metal tab goes on the side marked on the silkscreen**. UBK1's tab is a live switching node: **never connect its heatsink to ground.**

## Step 4: double-check everything

Do this before any power. Use the magnifying glass.

- [ ] Every electrolytic capacitor stripe matches the silkscreen
- [ ] Every diode stripe matches the silkscreen (D1, DBK1 on the front, DPW1 on the back)
- [ ] The UCN1 notch matches pin 1 on the silkscreen
- [ ] The AS5600 module is flat, square, and soldered on every pin (back side)
- [ ] The metal tab of ULD1 and UBK1 is on the marked side
- [ ] JDR3 and the screw terminals face the board edge
- [ ] Every joint is shiny and no two pins touch
- [ ] **Short test with the multimeter set to beep, power off, no modules in:** there must be **no beep** between 24 V and GND (JPW1), and none between the 5 V pad and GND (JMC2 pins 1 and 2). If it beeps, find the short before going on.

## Step 5: prepare the driver module (UART mode is recommended)

**Use UART mode.** You then set the motor current and the step size **in software**, the firmware can read faults from the driver, and you never have to turn the little pot. The no-UART rows below are a **fallback** only, for a module that will not talk.

Use a fine tip and very little solder, and check every change with the multimeter (beep test).

**BIGTREETECH TMC2209 V1.2**

Turn the module over. Near the **PDN** and **SPRE** labels there are two groups of three small pads (T1 and T2). **Each already has a tiny 0 Ω resistor** joining two of the three pads. Look at them before you change anything.

| | UART mode (recommended) | Fallback: no UART |
|---|---|---|
| **T1** (PDN, three pads) | Usually **nothing to do**. The factory resistor already joins the middle pad to one header pin (pin 4). Test: meter on beep, touch header pin 4 and the middle pad. A beep means UART is connected. If not, try pin 5 | Leave as it is |
| **T2** (SPREAD, three pads) | **Leave as it is.** The mode (quiet or strong) is chosen in software | **Move the resistor** so it joins the middle pad and the **VDD** pad. See the warning below |

> **Warning: never leave the T2 resistor on the GND side and also join the VDD side.** That shorts VDD to GND through the module. **Move** the resistor, don't add a second link.
>
> To find the VDD pad: touch one probe on the module's VDD header pin (7th pin of the power row, next to the last GND) and the other on each outer T2 pad. The one that beeps is VDD. Then heat both ends of the resistor with the iron, slide it onto the middle and VDD pads, and let it cool. Check again: **middle to VDD beeps, middle to GND does not.**

**MKS TMC2209 V2.0**

| | UART mode (recommended) | Fallback: no UART |
|---|---|---|
| **R9** (empty pad, 5 V pull-up) | **Leave empty** | **Fill it** (a solder blob across the pads) for more torque at speed |
| Everything else | Nothing to do | Nothing to do |

**Leave the little pot alone** in UART mode: the firmware switches the chip to digital current, so the pot does nothing.

You will confirm UART when you flash the firmware (Step 9): the Super Mini must print **"TMC2209 on UART: ok"**. If it says "UART not answering", the motor is **not safe to run yet**: switch off, re-check the T1 test above, and ask for help. Only use the fallback if you cannot fix it.

## Step 6: plug in the modules (check the EN marker)

1. **Super Mini:** the silkscreen outline is labelled **SuperMini ESP32-C6**. The USB-C end goes to the end marked on the silkscreen, and it must sit flat in both sockets.
2. **Driver module:** the outline is labelled **TMC2209**. Find the **EN pin** on the module. It must be at the **EN marker** on the board, and the VM pin must be at the same end. **A module the wrong way round is destroyed when you power it.** Look at both rows.
3. Do **not** connect the motor yet.
4. **JDR4** (next to the EN marker) is a 2-pin socket for the module's **VREF** (pin 1) and **DIAG** (pin 2) pads. Join those two module pads to JDR4 with two short wires, or solder a 2-pin header onto the module's pads so it plugs in (BIGTREETECH: J1 and J2; MKS: JP5 and JP4). **VREF must go to JDR4 pin 1.**

## Step 7: first power-up and checks

1. **Fit the inline fuse** in the 24 V wire. Set your bench supply to 24 V and a current limit of **0.5 A**, or ask your mentor for help.
2. Check the polarity of the supply wires, then connect them to JPW1. Switch on.
3. **Measure with the multimeter** (red probe on the point, black on GND). Switch off if anything is far off.
   - 24 V at JPW1 (the supply voltage)
   - about 6.4 V at the buck output (near COUT1)
   - about 5 V on the Super Mini's 5 V pad (JMC2 pin 1)
   - 3.3 V on JMC2 pin 3 once the Super Mini is plugged in
4. **Switch off** and wait 30 seconds.

## Step 8: set the motor current

**In UART mode (recommended)** you set the current in software, and the chip reports back what it is using.

1. The firmware starts at a safe **500 mA RMS** run current and a lower hold current.
2. **Never set it above your motor's rated current.** A generic 40 mm motor is usually about 1.5 A. Check yours.
3. **Raise it in small steps**, watching the motor and the driver. If the driver or motor gets too hot to touch, switch off and lower it.
4. Connect the motor **only with the power off.**

**Fallback (no UART only): the pot.** The firmware refuses to turn the driver on if the pot is set too high.

1. With the **motor still disconnected**, switch on and put the red probe on the board's **VREF pad** and the black on GND.
2. Turn the module's small pot with the **plastic screwdriver**. **Counter-clockwise raises** the voltage. Move it a tiny bit at a time.
3. **Start low**, at about **0.7 V** (roughly 0.5 A). **Vref ≈ 1.4 × the amps you want.**
4. **Never go above your motor's rated current**, and never above 2.2 V.
5. Switch off, wait 30 seconds, then connect the motor.

## Step 9: next

[Flash the firmware](05-flash-the-firmware.md) and check the boot message says **"TMC2209 on UART: ok"**, then [assemble and test](06-assemble-and-test.md).

**What is on the board:** the circuit and pin details are in the [PCB design reference](reference/v1.2_PCB_Design_Reference.md). The module schematics are in [TMC2209 module schematics](reference/tmc2209-module-schematics.md).
