// The BOOT button on the Super Mini as a stop button.
//
// Every board has one, and any of them stops the whole arm: the board where it
// is pressed stops its own motor and sends a bus-wide E-STOP (reason "manual
// button"). Base shows that to the host as fault `hw_estop`. Since the Base
// board may sit where nobody can reach it, press the button on whichever board
// you can reach, or wire an external push button to GND on JMC2 pin 9 (the
// GPIO9 line, unconnected on the PCB).
//
// On Base, HOLDING the button for PAIR_HOLD_MS also opens the BLE pairing
// window (a press always stops the arm first, which is the safe order).
//
// This is a software stop, not a safety-rated emergency stop: it needs the
// firmware to be running and is small and easy to miss. The real emergency stop
// is a switch or plug in the 24 V supply line.
//
// GPIO9 is a strapping pin: a button held down while the board RESETS enters
// the flashing mode instead of running. Pressed while running it is a normal input.

#pragma once
#include <cstdint>

namespace StopButton {

constexpr uint32_t PAIR_HOLD_MS = 3000;

void init();
void update(uint32_t now_ms); // call every loop()
bool pressed();               // debounced, true while the button is held down

} // namespace StopButton
