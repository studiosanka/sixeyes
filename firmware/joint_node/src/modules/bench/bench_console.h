// Bench console: type commands over the serial monitor to move ONE motor on
// the bench, with no arm, no CAN bus and no Base board. Built only by the
// v1_bench environment (-DBENCH_CONSOLE); the arm's firmware never contains it.
//
// It exists to run the motion test from the host handoff ("one joint follows
// a target, smoothly") and to tune sensorless homing before the motor is on
// the arm. See the "Bench test" section of firmware/joint_node/docs/README.md.
//
// !!! This build lets the motor be enabled without a bus heartbeat. Use it with
// the motor off the arm, or with the gearbox free to turn. !!!

#pragma once

#ifdef BENCH_CONSOLE
namespace BenchConsole {

void init();
void update(); // call from loop(): reads typed lines and runs them

} // namespace BenchConsole
#endif
