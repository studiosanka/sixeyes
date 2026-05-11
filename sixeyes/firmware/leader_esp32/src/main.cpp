// Leader ESP32-C6 SuperMini teleoperation streamer
// Role: sample leader-arm joint inputs and publish JOINT_STATE at 100 Hz.

#include <Arduino.h>
#include <ArduinoJson.h>
#include <array>
#include <cstring>

namespace {
constexpr uint32_t BAUD_RATE = 115200;
constexpr uint32_t STREAM_HZ = 100;
constexpr uint32_t STREAM_PERIOD_MS = 1000 / STREAM_HZ;
constexpr size_t NUM_JOINTS = 6;
constexpr uint16_t ADC_MAX = 4095;

// Leader ADC pins for 6 joint channels (Technical Reference 2).
constexpr uint8_t JOINT_ADC_PINS[NUM_JOINTS] = {1, 2, 3, 4, 5, 6};
constexpr const char *CMD_CAPTURE_ZERO = "CAPTURE_ZERO";
constexpr const char *CMD_HOME_ZERO = "HOME_ZERO";

uint32_t sequence = 0;
uint32_t last_stream_ms = 0;
std::array<float, NUM_JOINTS> zero_offsets_deg = {0, 0, 0, 0, 0, 0};
bool zero_captured = false;

float applyZeroOffset(float raw_deg, float zero_deg) {
  float relative_deg = raw_deg - zero_deg;
  if (relative_deg < -180.0f)
    relative_deg = -180.0f;
  if (relative_deg > 180.0f)
    relative_deg = 180.0f;
  return relative_deg;
}

float adcToDegrees(uint16_t raw) {
  const float ratio = static_cast<float>(raw) / static_cast<float>(ADC_MAX);
  return ratio * 180.0f;
}

bool readJointDegrees(float out_degrees[NUM_JOINTS], uint8_t out_valid[NUM_JOINTS]) {
  bool all_valid = true;

  for (size_t i = 0; i < NUM_JOINTS; ++i) {
    const int raw = analogRead(JOINT_ADC_PINS[i]);
    if (raw < 0 || raw > ADC_MAX) {
      out_degrees[i] = 0.0f;
      out_valid[i] = 0;
      all_valid = false;
      continue;
    }

    const float raw_deg = adcToDegrees(static_cast<uint16_t>(raw));
    out_degrees[i] = zero_captured ? applyZeroOffset(raw_deg, zero_offsets_deg[i])
                                   : raw_deg;
    out_valid[i] = 1;
  }

  return all_valid;
}

void captureCurrentPoseAsZero() {
  bool any_valid = false;

  for (size_t i = 0; i < NUM_JOINTS; ++i) {
    const int raw = analogRead(JOINT_ADC_PINS[i]);
    if (raw < 0 || raw > ADC_MAX) {
      continue;
    }

    zero_offsets_deg[i] = adcToDegrees(static_cast<uint16_t>(raw));
    any_valid = true;
  }

  if (any_valid) {
    zero_captured = true;
    Serial.println("[CAL] Leader pot zero captured from current pose");
  } else {
    Serial.println("[CAL] Leader pot zero capture failed (no valid ADC reads)");
  }
}

void handleSerialCommand(const char *line) {
  if (!line || line[0] == '\0') {
    return;
  }

  if (strcmp(line, CMD_CAPTURE_ZERO) == 0 || strcmp(line, CMD_HOME_ZERO) == 0) {
    captureCurrentPoseAsZero();
    return;
  }

  if (line[0] == '{') {
    JsonDocument cmd_doc;
    const DeserializationError err = deserializeJson(cmd_doc, line);
    if (!err && cmd_doc["cmd"].is<const char *>()) {
      const char *cmd = cmd_doc["cmd"].as<const char *>();
      if (strcmp(cmd, CMD_CAPTURE_ZERO) == 0 || strcmp(cmd, CMD_HOME_ZERO) == 0) {
        captureCurrentPoseAsZero();
      }
    }
  }
}

void pollSerialCommands() {
  static char line_buf[128] = {};
  static size_t line_len = 0;

  while (Serial.available() > 0) {
    const int incoming = Serial.read();
    if (incoming < 0) {
      continue;
    }

    const char ch = static_cast<char>(incoming);
    if (ch == '\r') {
      continue;
    }

    if (ch == '\n') {
      line_buf[line_len] = '\0';
      handleSerialCommand(line_buf);
      line_len = 0;
      continue;
    }

    if (line_len < (sizeof(line_buf) - 1)) {
      line_buf[line_len++] = ch;
    } else {
      line_len = 0;
    }
  }
}

void publishJointState() {
  float joint_degrees[NUM_JOINTS] = {};
  uint8_t valid_mask[NUM_JOINTS] = {};
  readJointDegrees(joint_degrees, valid_mask);

  JsonDocument doc;
  doc["cmd"] = "JOINT_STATE";
  doc["seq"] = sequence++;
  doc["ts"] = millis();

  JsonArray joints = doc["leader_joints"].to<JsonArray>();
  JsonArray valid = doc["valid_mask"].to<JsonArray>();

  for (size_t i = 0; i < NUM_JOINTS; ++i) {
    joints.add(joint_degrees[i]);
    valid.add(valid_mask[i]);
  }

  serializeJson(doc, Serial);
  Serial.println();
}
} // namespace

void setup() {
  Serial.begin(BAUD_RATE);
  delay(100);

  analogReadResolution(12);
  analogSetAttenuation(ADC_ATTEN_DB_11);

  for (size_t i = 0; i < NUM_JOINTS; ++i) {
    pinMode(JOINT_ADC_PINS[i], INPUT);
  }

  Serial.println("SixEyes leader_esp32 (ESP32-C6 SuperMini) starting (teleoperation mode)");
  Serial.println("Streaming JOINT_STATE at 100 Hz");
  Serial.println("Send HOME_ZERO or CAPTURE_ZERO to latch current pot pose");
}

void loop() {
  pollSerialCommands();

  const uint32_t now = millis();
  if ((now - last_stream_ms) >= STREAM_PERIOD_MS) {
    last_stream_ms = now;
    publishJointState();
  }
}
