// Golden vectors for the host-protocol codec.
// COPIED from sixeyes-host/protocol/test_vectors.json (generated there). Do not edit by hand:
// if the protocol changes, regenerate it in sixeyes-host and copy again.

#include "host_codec.h"
#include <Arduino.h>
#include <cstring>

namespace HostCodec {
namespace {
struct CobsVec { const uint8_t *raw; size_t raw_len; const uint8_t *enc; size_t enc_len; };
const uint8_t kCobsRaw0[] = {0x00};
const uint8_t kCobsEnc0[] = {0x01, 0x01};
const uint8_t kCobsRaw1[] = {0x00, 0x00};
const uint8_t kCobsEnc1[] = {0x01, 0x01, 0x01};
const uint8_t kCobsRaw2[] = {0x11, 0x22, 0x00, 0x33};
const uint8_t kCobsEnc2[] = {0x03, 0x11, 0x22, 0x02, 0x33};
const uint8_t kCobsRaw3[] = {0x11, 0x22, 0x33, 0x44};
const uint8_t kCobsEnc3[] = {0x05, 0x11, 0x22, 0x33, 0x44};
const uint8_t kCobsRaw4[] = {0};
const uint8_t kCobsEnc4[] = {0x01};
const CobsVec kCobs[] = {
    {kCobsRaw0, 1, kCobsEnc0, 2},
    {kCobsRaw1, 2, kCobsEnc1, 3},
    {kCobsRaw2, 4, kCobsEnc2, 5},
    {kCobsRaw3, 4, kCobsEnc3, 5},
    {kCobsRaw4, 0, kCobsEnc4, 1},
};

struct CrcVec { const uint8_t *data; size_t len; uint16_t crc; };
const uint8_t kCrcData0[] = {0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39};
const uint8_t kCrcData1[] = {0};
const CrcVec kCrc[] = {
    {kCrcData0, 9, 0x29B1},
    {kCrcData1, 0, 0xFFFF},
};

struct FrameVec { uint8_t type; uint8_t seq; const uint8_t *payload; size_t payload_len; const uint8_t *frame; size_t frame_len; };
const uint8_t kFramePayload0[] = {0x01, 0x00};
const uint8_t kFrameBytes0[] = {0x04, 0x01, 0x07, 0x01, 0x03, 0xD5, 0x44, 0x00};
const uint8_t kFramePayload1[] = {0};
const uint8_t kFrameBytes1[] = {0x05, 0x03, 0x07, 0xBB, 0x38, 0x00};
const uint8_t kFramePayload2[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3F, 0x00, 0x00, 0x00, 0xBF, 0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x80, 0x3E, 0x00, 0x00, 0x48, 0x42};
const uint8_t kFrameBytes2[] = {0x03, 0x04, 0x07, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x02, 0x3F, 0x01, 0x01, 0x02, 0xBF, 0x01, 0x03, 0x80, 0x3F, 0x01, 0x03, 0x80, 0x3E, 0x01, 0x05, 0x48, 0x42, 0xD1, 0x82, 0x00};
const uint8_t kFramePayload3[] = {0};
const uint8_t kFrameBytes3[] = {0x05, 0x05, 0x07, 0x1D, 0x92, 0x00};
const uint8_t kFramePayload4[] = {0};
const uint8_t kFrameBytes4[] = {0x05, 0x07, 0x07, 0x7F, 0xF4, 0x00};
const FrameVec kFrames[] = {
    {1, 7, kFramePayload0, 2, kFrameBytes0, 8},
    {3, 7, kFramePayload1, 0, kFrameBytes1, 6},
    {4, 7, kFramePayload2, 24, kFrameBytes2, 30},
    {5, 7, kFramePayload3, 0, kFrameBytes3, 6},
    {7, 7, kFramePayload4, 0, kFrameBytes4, 6},
};
} // namespace

bool selfTest() {
  int failures = 0;
  uint8_t buf[600];
  size_t n = 0;

  for (size_t i = 0; i < sizeof(kCobs) / sizeof(kCobs[0]); i++) {
    const CobsVec &v = kCobs[i];
    n = cobsEncode(v.raw_len ? v.raw : nullptr, v.raw_len, buf, sizeof(buf));
    if (n != v.enc_len || memcmp(buf, v.enc, n) != 0) {
      Serial.printf("[SELFTEST] COBS encode vector %u FAILED\n", static_cast<unsigned>(i));
      failures++;
    }
    uint8_t back[64];
    size_t back_len = 0;
    if (!cobsDecode(v.enc, v.enc_len, back, sizeof(back), back_len) || back_len != v.raw_len ||
        (v.raw_len && memcmp(back, v.raw, back_len) != 0)) {
      Serial.printf("[SELFTEST] COBS decode vector %u FAILED\n", static_cast<unsigned>(i));
      failures++;
    }
  }

  for (size_t i = 0; i < sizeof(kCrc) / sizeof(kCrc[0]); i++) {
    const CrcVec &v = kCrc[i];
    if (crc16(v.len ? v.data : nullptr, v.len) != v.crc) {
      Serial.printf("[SELFTEST] CRC vector %u FAILED\n", static_cast<unsigned>(i));
      failures++;
    }
  }

  for (size_t i = 0; i < sizeof(kFrames) / sizeof(kFrames[0]); i++) {
    const FrameVec &v = kFrames[i];
    n = encodeFrame(v.type, v.seq, v.payload_len ? v.payload : nullptr, v.payload_len, buf, sizeof(buf));
    if (n != v.frame_len || memcmp(buf, v.frame, n) != 0) {
      Serial.printf("[SELFTEST] frame encode vector %u FAILED\n", static_cast<unsigned>(i));
      failures++;
    }
    FrameParser parser;
    Frame f;
    bool got = false;
    for (size_t k = 0; k < v.frame_len; k++) got = parser.feedByte(v.frame[k], f);
    if (!got || f.type != v.type || f.seq != v.seq || f.length != v.payload_len ||
        (v.payload_len && memcmp(f.payload, v.payload, f.length) != 0)) {
      Serial.printf("[SELFTEST] frame decode vector %u FAILED\n", static_cast<unsigned>(i));
      failures++;
    }
  }

  // A frame with one flipped bit must be rejected, and text before a frame must not hide it.
  {
    FrameParser parser;
    Frame f;
    bool got = false;
    const FrameVec &v = kFrames[0];
    for (size_t k = 0; k < v.frame_len; k++) {
      uint8_t b = v.frame[k];
      if (k == 3) b ^= 0x01; // damage the body
      got = parser.feedByte(b, f) || got;
    }
    if (got) {
      Serial.println("[SELFTEST] damaged frame was accepted: FAILED");
      failures++;
    }
    const char *noise = "boot log line\n";
    for (const char *p = noise; *p; p++) parser.feedByte(static_cast<uint8_t>(*p), f);
    parser.feedByte(0, f); // resync, as the Base board does before its first frame
    got = false;
    for (size_t k = 0; k < v.frame_len; k++) got = parser.feedByte(v.frame[k], f);
    if (!got) {
      Serial.println("[SELFTEST] frame after noise was not recovered: FAILED");
      failures++;
    }
  }

  if (failures == 0) {
    Serial.println("[SELFTEST] host codec: all golden vectors passed");
  } else {
    Serial.printf("[SELFTEST] host codec: %d check(s) FAILED\n", failures);
  }
  return failures == 0;
}

} // namespace HostCodec
