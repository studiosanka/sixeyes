#include "host_codec.h"
#include <cstring>

namespace HostCodec {

uint16_t crc16(const uint8_t *data, size_t n) {
  uint16_t crc = 0xFFFF;
  for (size_t i = 0; i < n; i++) {
    crc ^= static_cast<uint16_t>(data[i]) << 8;
    for (int b = 0; b < 8; b++) {
      crc = (crc & 0x8000) ? static_cast<uint16_t>((crc << 1) ^ 0x1021)
                           : static_cast<uint16_t>(crc << 1);
    }
  }
  return crc;
}

size_t cobsEncode(const uint8_t *in, size_t n, uint8_t *out, size_t cap) {
  if (cap < 1) return 0;
  size_t code_idx = 0; // where the current block's length byte will go
  size_t w = 1;        // next free output byte
  uint8_t code = 1;
  for (size_t i = 0; i < n; i++) {
    if (in[i] == 0) {
      out[code_idx] = code;
      code_idx = w;
      if (w >= cap) return 0;
      out[w++] = 0;
      code = 1;
    } else {
      if (w >= cap) return 0;
      out[w++] = in[i];
      code++;
      if (code == 0xFF) { // a block holds at most 254 data bytes
        out[code_idx] = code;
        code_idx = w;
        if (w >= cap) return 0;
        out[w++] = 0;
        code = 1;
      }
    }
  }
  out[code_idx] = code;
  return w;
}

bool cobsDecode(const uint8_t *in, size_t n, uint8_t *out, size_t cap, size_t &out_len) {
  size_t i = 0;
  size_t w = 0;
  while (i < n) {
    const uint8_t code = in[i];
    if (code == 0) return false;
    const size_t end = i + code;
    if (end > n) return false;
    const size_t count = code - 1u;
    if (w + count > cap) return false;
    memcpy(out + w, in + i + 1, count);
    w += count;
    i = end;
    if (i < n && code != 0xFF) { // the zero that this block stood in for
      if (w >= cap) return false;
      out[w++] = 0;
    }
  }
  out_len = w;
  return true;
}

size_t encodeFrame(uint8_t type, uint8_t seq, const uint8_t *payload, size_t n,
                   uint8_t *out, size_t cap) {
  if (n > kMaxPayload) return 0;
  uint8_t body[kMaxPayload + 4];
  body[0] = type;
  body[1] = seq;
  if (n) memcpy(body + 2, payload, n);
  const uint16_t crc = crc16(body, n + 2);
  body[n + 2] = static_cast<uint8_t>(crc & 0xFF);
  body[n + 3] = static_cast<uint8_t>(crc >> 8);
  const size_t enc = cobsEncode(body, n + 4, out, cap);
  if (enc == 0 || enc >= cap) return 0;
  out[enc] = 0; // delimiter
  return enc + 1;
}

bool FrameParser::feedByte(uint8_t b, Frame &out) {
  if (b != 0) {
    if (len_ < kMaxEncoded) {
      buf_[len_++] = b;
    } else {
      overflow_ = true; // too long to be a real frame; drop it at the delimiter
    }
    return false;
  }
  if (len_ == 0) return false; // empty frame: ignore
  uint8_t body[kMaxPayload + 4];
  size_t body_len = 0;
  bool ok = !overflow_ && cobsDecode(buf_, len_, body, sizeof(body), body_len) && body_len >= 4;
  if (ok) {
    const uint16_t got = static_cast<uint16_t>(body[body_len - 2] | (body[body_len - 1] << 8));
    ok = crc16(body, body_len - 2) == got;
  }
  len_ = 0;
  overflow_ = false;
  if (!ok) {
    bad_frames_++;
    return false;
  }
  out.type = body[0];
  out.seq = body[1];
  out.length = body_len - 4;
  if (out.length) memcpy(out.payload, body + 2, out.length);
  return true;
}

} // namespace HostCodec
