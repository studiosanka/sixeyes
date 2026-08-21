#pragma once
#include "modules/config/board_config.h"
#include <Arduino.h>
#include <array>
#include <cstdint>

/**
 * UsbCDC: TinyUSB CDC Telemetry Interface
 *
 * Implements USB CDC (Communication Device Class) for telemetry streaming
 * on ESP32-S3 native USB. Provides:
 *
 * - Binary framing protocol (length-prefixed packets)
 * - Joint telemetry (positions, velocities, currents)
 * - Status and diagnostics
 * - Automatic reconnection handling
 * - Non-blocking USB writes to prevent control loop stalling
 *
 * Packet Format:
 *   [0x55] [LENGTH(2 bytes)] [PAYLOAD] [CHECKSUM]
 *   - 0x55: Frame sync byte
 *   - LENGTH: Big-endian uint16 (payload size, max 512 bytes)
 *   - PAYLOAD: JSON or binary telemetry data
 *   - CHECKSUM: Sum of all payload bytes (mod 256)
 */
class UsbCDC {
public:
  static UsbCDC &instance();
  void init();
  void update(); // Call from main loop or control scheduler

  // Send formatted telemetry (non-blocking, returns false if USB not ready)
  bool sendTelemetry(const char *json_payload);
  bool sendRawData(const uint8_t *data, size_t len);

  // Connection status
  bool isConnected() const;
  uint32_t getPacketsSent() const;
  uint32_t getPacketsDropped() const;

  // Diagnostics
  void printStats();

private:
  UsbCDC();

  // Framing constants
  static constexpr uint8_t FRAME_SYNC = 0x55;
  static constexpr size_t MAX_PAYLOAD = 512;
  static constexpr size_t MAX_FRAME_SIZE =
      1 + 2 + MAX_PAYLOAD + 1; // sync + length + payload + checksum

  // USB CDC buffer
  uint8_t frame_buffer_[MAX_FRAME_SIZE];

  // State tracking
  bool connected_ = false;
  uint32_t packets_sent_ = 0;
  uint32_t packets_dropped_ = 0;
  unsigned long last_connect_check_ms_ = 0;
  static constexpr unsigned long CONNECT_CHECK_INTERVAL_MS = 100;

  // Helper methods
  void checkConnection();
  size_t buildFrame(const uint8_t *payload, size_t payload_len, uint8_t *frame);
  uint8_t calculateChecksum(const uint8_t *data, size_t len);
  bool writeFrame(const uint8_t *frame, size_t frame_len);
};

/**
 * TelemetryPacket: Structured telemetry data for efficient serialization
 */
struct TelemetryPacket {
  uint32_t timestamp_ms;

  // Joint states (6 DOF)
  std::array<float, NUM_STEPPERS> stepper_positions_deg;
  std::array<float, NUM_STEPPERS> stepper_velocities_deg_sec;
  std::array<float, NUM_SERVOS> servo_positions_deg;

  // Status
  uint8_t motors_enabled;
  uint8_t safety_fault;
  uint8_t usb_connected;
};

/**
 * TelemetryCollector: Gathers telemetry from system modules
 */
class TelemetryCollector {
public:
  static TelemetryCollector &instance();

  void init();
  void collect(TelemetryPacket &packet); // Gather current system state

  // Format packet as JSON string
  size_t formatJSON(const TelemetryPacket &packet, char *buf, size_t buf_len);

private:
  TelemetryCollector();
};
