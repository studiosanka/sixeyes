#include "usb_cdc.h"
#include "modules/motor_control/motor_controller.h"
#include "modules/safety/fault_manager.h"
#include "modules/safety/safety_task.h"
#include "modules/servo_control/servo_manager.h"
#include <Arduino.h>
#include <cstring>

// === UsbCDC Implementation ===

UsbCDC &UsbCDC::instance() {
  static UsbCDC inst;
  return inst;
}

UsbCDC::UsbCDC() {}

void UsbCDC::init() {
  Serial.println("UsbCDC: initializing TinyUSB CDC (native USB on ESP32-S3)");
  Serial.println("  - Framing: length-prefixed with SYNC(0x55) and checksum");
  Serial.println("  - Max payload: 512 bytes");
  Serial.println("  - Baud: native USB (full-speed)");

  // On ESP32-S3, native USB CDC is automatically handled by Arduino framework
  // TinyUSB stack is initialized by core.

  connected_ = false;
  packets_sent_ = 0;
  packets_dropped_ = 0;
  last_connect_check_ms_ = millis();
}

void UsbCDC::update() {
  // Check USB connection status periodically
  unsigned long now_ms = millis();
  if ((now_ms - last_connect_check_ms_) >= CONNECT_CHECK_INTERVAL_MS) {
    checkConnection();
    last_connect_check_ms_ = now_ms;
  }
}

void UsbCDC::checkConnection() {
  // ESP32-S3 native USB: Serial is available when host connected
  bool usb_active = Serial.availableForWrite() > 0;

  if (usb_active && !connected_) {
    connected_ = true;
    Serial.println("UsbCDC: USB host connected");
  } else if (!usb_active && connected_) {
    connected_ = false;
    Serial.println("UsbCDC: USB host disconnected");
  }
}

uint8_t UsbCDC::calculateChecksum(const uint8_t *data, size_t len) {
  uint8_t sum = 0;
  for (size_t i = 0; i < len; ++i) {
    sum += data[i];
  }
  return sum;
}

size_t UsbCDC::buildFrame(const uint8_t *payload, size_t payload_len,
                          uint8_t *frame) {
  if (payload_len > MAX_PAYLOAD) {
    return 0;
  }

  size_t idx = 0;
  frame[idx++] = FRAME_SYNC;
  frame[idx++] = (payload_len >> 8) & 0xFF;
  frame[idx++] = payload_len & 0xFF;
  std::memcpy(frame + idx, payload, payload_len);
  idx += payload_len;
  uint8_t checksum = calculateChecksum(payload, payload_len);
  frame[idx++] = checksum;

  return idx;
}

bool UsbCDC::writeFrame(const uint8_t *frame, size_t frame_len) {
  if (!connected_) {
    return false;
  }

  size_t written = Serial.write(frame, frame_len);

  if (written == frame_len) {
    packets_sent_++;
    return true;
  } else {
    packets_dropped_++;
    return false;
  }
}

bool UsbCDC::sendTelemetry(const char *json_payload) {
  if (!connected_) {
    packets_dropped_++;
    return false;
  }

  size_t payload_len = std::strlen(json_payload);
  if (payload_len > MAX_PAYLOAD) {
    Serial.println("UsbCDC: payload too large");
    packets_dropped_++;
    return false;
  }

  size_t frame_len =
      buildFrame((const uint8_t *)json_payload, payload_len, frame_buffer_);

  if (frame_len == 0) {
    packets_dropped_++;
    return false;
  }

  return writeFrame(frame_buffer_, frame_len);
}

bool UsbCDC::sendRawData(const uint8_t *data, size_t len) {
  if (!connected_) {
    packets_dropped_++;
    return false;
  }

  size_t frame_len = buildFrame(data, len, frame_buffer_);

  if (frame_len == 0) {
    packets_dropped_++;
    return false;
  }

  return writeFrame(frame_buffer_, frame_len);
}

bool UsbCDC::isConnected() const { return connected_; }

uint32_t UsbCDC::getPacketsSent() const { return packets_sent_; }

uint32_t UsbCDC::getPacketsDropped() const { return packets_dropped_; }

void UsbCDC::printStats() {
  Serial.print("UsbCDC stats: connected=");
  Serial.print(connected_ ? "yes" : "no");
  Serial.print(" sent=");
  Serial.print(packets_sent_);
  Serial.print(" dropped=");
  Serial.println(packets_dropped_);
}

// === TelemetryCollector Implementation ===

TelemetryCollector &TelemetryCollector::instance() {
  static TelemetryCollector inst;
  return inst;
}

TelemetryCollector::TelemetryCollector() {}

void TelemetryCollector::init() {
  Serial.println("TelemetryCollector: initialized");
}

void TelemetryCollector::collect(TelemetryPacket &packet) {
  packet.timestamp_ms = millis();

  auto stepper_pos = MotorController::instance().getCurrentPositions();
  auto stepper_vel = MotorController::instance().getCurrentVelocities();
  for (int i = 0; i < NUM_STEPPERS; ++i) {
    packet.stepper_positions_deg[i] = stepper_pos[i];
    packet.stepper_velocities_deg_sec[i] = stepper_vel[i];
  }

  auto servo_pos = ServoManager::instance().getPositions();
  for (int i = 0; i < NUM_SERVOS; ++i) {
    packet.servo_positions_deg[i] = servo_pos[i];
  }

  packet.motors_enabled = MotorController::instance().motorsEnabled() ? 1 : 0;
  packet.safety_fault = SafetyTask::instance().isSafeToOperate() ? 0 : 1;
  packet.usb_connected = UsbCDC::instance().isConnected() ? 1 : 0;
}

size_t TelemetryCollector::formatJSON(const TelemetryPacket &packet, char *buf,
                                      size_t buf_len) {
  int written = snprintf(
      buf, buf_len,
      "{\"t\":%lu,\"en\":%d,\"fault\":%d,"
      "\"s\":[%.1f,%.1f,%.1f,%.1f],"
      "\"sv\":[%.1f,%.1f,%.1f,%.1f],"
      "\"servo\":[%.1f,%.1f,%.1f]}",
      packet.timestamp_ms, packet.motors_enabled, packet.safety_fault,
      packet.stepper_positions_deg[0], packet.stepper_positions_deg[1],
      packet.stepper_positions_deg[2], packet.stepper_positions_deg[3],
      packet.stepper_velocities_deg_sec[0],
      packet.stepper_velocities_deg_sec[1],
      packet.stepper_velocities_deg_sec[2],
      packet.stepper_velocities_deg_sec[3], packet.servo_positions_deg[0],
      packet.servo_positions_deg[1], packet.servo_positions_deg[2]);

  if (written < 0)
    return 0;
  return (size_t)written;
}
