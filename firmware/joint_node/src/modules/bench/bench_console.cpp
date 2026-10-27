#include "bench_console.h"

#ifdef BENCH_CONSOLE

#include "modules/config/joint_config.h"
#include "modules/drivers/tmc2209/tmc2209_driver.h"
#include "modules/motor_control/motor_controller.h"
#include "modules/motor_control/step_generator.h"
#include "modules/safety/can_safety_task.h"
#include "modules/telemetry/node_telemetry.h"
#include <Arduino.h>
#include <cstdlib>
#include <cstring>

namespace BenchConsole {

namespace {

char line_[64];
size_t len_ = 0;
uint32_t last_trace_ms_ = 0;
bool trace_ = false;

void printHelp() {
  Serial.println(
      "Bench console. Commands:\n"
      "  en            enable the motor (holds the shaft)\n"
      "  dis           disable the motor (shaft is free)\n"
      "  zero          call the present shaft pose 0 deg (no homing); needed before goto\n"
      "  goto <deg>    move to an angle at the OUTPUT shaft (joint limits apply)\n"
      "  rel <deg>     move by an angle at the output shaft\n"
      "  stop          hold where it is now\n"
      "  home          run sensorless homing (drives into a stop; prints SG_RESULT)\n"
      "  cur <mA>      set the driver's RMS run current\n"
      "  sg            read StallGuard once\n"
      "  trace on|off  print a status line every 200 ms\n"
      "  reset         clear stall, homing-failed and stop-button (BOOT) latches\n"
      "  status        print the position, the encoder and their difference");
}

void printStatus() {
  MotorController &m = MotorController::instance();
  const int32_t steps = StepGenerator::instance().position();
  const int32_t counts = NodeTelemetry::instance().counts();
  const float enc_out_deg = motorCountsToOutputRad(counts, JOINT_NODE_ID) * RAD_TO_DEG_F;
  const float step_out_deg = m.currentOutputDeg();
  Serial.printf(
      "mode=%s en=%d homed=%d stall=%d v=%.0f sps steps=%ld (%.3f deg out) enc=%ld counts (%.3f deg out) "
      "diff=%.3f deg out%s\n",
      m.benchModeName(), m.isEnabled(), m.isHomed(), m.isStallDetected(), m.benchVelocitySps(),
      static_cast<long>(steps), step_out_deg, static_cast<long>(counts), enc_out_deg, step_out_deg - enc_out_deg,
      NodeTelemetry::instance().encoderFault() ? "  [ENCODER FAULT]" : "");
}

void run(char *cmd) {
  char *arg = strchr(cmd, ' ');
  if (arg) {
    *arg++ = '\0';
    while (*arg == ' ') arg++;
  }
  MotorController &m = MotorController::instance();

  if (!strcmp(cmd, "help") || !strcmp(cmd, "?")) {
    printHelp();
  } else if (!strcmp(cmd, "en")) {
    CanSafetyTask::instance().requestEnable();
    Serial.println("enabled");
  } else if (!strcmp(cmd, "dis")) {
    m.disable();
    Serial.println("disabled");
  } else if (!strcmp(cmd, "zero")) {
    if (!m.isEnabled()) CanSafetyTask::instance().requestEnable();
    m.benchZeroHere();
    Serial.println("zeroed: the shaft is now 0 deg");
  } else if (!strcmp(cmd, "goto") && arg) {
    m.setTargetPosition(static_cast<float>(atof(arg)));
    Serial.println("ok");
  } else if (!strcmp(cmd, "rel") && arg) {
    m.setTargetPosition(m.currentOutputDeg() + static_cast<float>(atof(arg)));
    Serial.println("ok");
  } else if (!strcmp(cmd, "stop")) {
    m.holdHere();
    Serial.println("holding");
  } else if (!strcmp(cmd, "home")) {
    Serial.println(m.startHoming() ? "homing..." : "could not start homing (see the message above)");
  } else if (!strcmp(cmd, "cur") && arg) {
    TMC2209Driver::instance().setCurrent(0, static_cast<uint16_t>(atoi(arg)));
  } else if (!strcmp(cmd, "sg")) {
    uint16_t sg = 0;
    if (TMC2209Driver::instance().readSGResult(0, sg)) {
      Serial.printf("SG_RESULT=%u\n", sg);
    } else {
      Serial.println("SG_RESULT: no reading (no UART, or the read failed)");
    }
  } else if (!strcmp(cmd, "trace")) {
    trace_ = arg && !strcmp(arg, "on");
    Serial.println(trace_ ? "trace on" : "trace off");
  } else if (!strcmp(cmd, "reset")) {
    m.resetFault();
    CanSafetyTask::instance().clearBusFault(); // also the stop-button latch (BOOT button)
    Serial.println("latches cleared");
  } else if (!strcmp(cmd, "status")) {
    printStatus();
  } else if (cmd[0] != '\0') {
    Serial.println("unknown command, type help");
  }
}

} // namespace

void init() {
  Serial.println("\n*** BENCH BUILD: the motor can be enabled without a bus. Not for the arm. ***");
  printHelp();
}

void update() {
  while (Serial.available() > 0) {
    const int c = Serial.read();
    if (c == '\n' || c == '\r') {
      line_[len_] = '\0';
      len_ = 0;
      run(line_);
    } else if (len_ < sizeof(line_) - 1) {
      line_[len_++] = static_cast<char>(c);
    }
  }
  if (trace_ && static_cast<uint32_t>(millis() - last_trace_ms_) >= 200) {
    last_trace_ms_ = millis();
    printStatus();
  }
}

} // namespace BenchConsole

#endif // BENCH_CONSOLE
