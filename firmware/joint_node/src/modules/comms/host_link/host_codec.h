// Framing for the host protocol: COBS + CRC-16/CCITT-FALSE.
//
// A frame on the wire is  COBS( type | seq | payload | crc16 )  followed by 0x00.
// The CRC covers type, seq and payload; integers are little-endian.
// Source of truth: PROTOCOL.md in the sixeyes-host repo. This file ports the
// same few lines to C++ without heap allocation so it can run on the Base
// board; golden vectors for it are checked by HostCodec::selfTest().

#pragma once
#include <cstddef>
#include <cstdint>

namespace HostCodec {

constexpr size_t kMaxEncoded = 512; // longest encoded frame we accept (as in the protocol)
constexpr size_t kMaxPayload = 128; // longest payload we accept; the biggest real one is 56 bytes

uint16_t crc16(const uint8_t *data, size_t n);

// Returns the encoded length, or 0 if `out` is too small.
size_t cobsEncode(const uint8_t *in, size_t n, uint8_t *out, size_t cap);

// Returns false on malformed input or if `out` is too small.
bool cobsDecode(const uint8_t *in, size_t n, uint8_t *out, size_t cap, size_t &out_len);

// Builds a complete frame including the trailing 0x00. Returns its length, or
// 0 if `out` is too small. `out` needs at least n + 8 + n / 254 bytes.
size_t encodeFrame(uint8_t type, uint8_t seq, const uint8_t *payload, size_t n,
                   uint8_t *out, size_t cap);

struct Frame {
  uint8_t type = 0;
  uint8_t seq = 0;
  uint8_t payload[kMaxPayload];
  size_t length = 0; // payload length
};

// Feeds bytes one at a time and reports each complete, CRC-valid frame.
// Empty frames are ignored (that is how it resynchronises after noise, such
// as text log lines that were printed before the host started talking).
class FrameParser {
public:
  // Returns true when `out` now holds a valid frame.
  bool feedByte(uint8_t b, Frame &out);
  uint32_t badFrames() const { return bad_frames_; }

private:
  uint8_t buf_[kMaxEncoded];
  size_t len_ = 0;
  bool overflow_ = false;
  uint32_t bad_frames_ = 0;
};

// Checks the codec against the protocol's golden vectors and prints the
// result to Serial. Runs at boot only with -DHOST_LINK_SELFTEST (see the
// v1_base_selftest environment in platformio.ini).
bool selfTest();

} // namespace HostCodec
