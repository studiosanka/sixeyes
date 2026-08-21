#include "uart_leader.h"
#include <Arduino.h>
#include <cstring>

// CRC32 (IEEE 802.3) implementation
static uint32_t crc32_compute(const uint8_t *data, size_t len) {
  static uint32_t table[256];
  static bool table_computed = false;
  if (!table_computed) {
    for (uint32_t i = 0; i < 256; i++) {
      uint32_t c = i;
      for (size_t j = 0; j < 8; j++) {
        if (c & 1)
          c = 0xEDB88320U ^ (c >> 1);
        else
          c = c >> 1;
      }
      table[i] = c;
    }
    table_computed = true;
  }
  uint32_t c = 0xFFFFFFFFU;
  for (size_t i = 0; i < len; ++i) {
    c = table[(c ^ data[i]) & 0xFF] ^ (c >> 8);
  }
  return c ^ 0xFFFFFFFFU;
}

#define SYNC0 0xAA
#define SYNC1 0x55

UartLeader &UartLeader::instance() {
  static UartLeader inst;
  return inst;
}

UartLeader::UartLeader() {}

void UartLeader::init(unsigned long baud) {
  Serial1.begin(baud, SERIAL_8N1, LEADER_UART_RX_PIN, LEADER_UART_TX_PIN);
  Serial.println("UartLeader: Serial1 started");
}

bool UartLeader::available() { return Serial1.available() > 0; }

bool UartLeader::readAbsoluteTargets(
    std::array<float, NUM_STEPPERS> &out_targets, uint32_t &out_seq) {
  // Non-blocking parser: accumulate bytes and process when a full packet is
  // available.
  while (Serial1.available()) {
    if (rx_len_ >= RX_BUFFER_SIZE) {
      // buffer overflow: reset
      rx_len_ = 0;
      state_ = ParseState::FIND_SYNC;
      return false;
    }
    rx_buf_[rx_len_++] = (uint8_t)Serial1.read();
  }

  size_t idx = 0;
  while (idx < rx_len_) {
    switch (state_) {
    case ParseState::FIND_SYNC: {
      if (idx + 1 >= rx_len_) {
        idx = rx_len_;
        break;
      }
      if (rx_buf_[idx] == SYNC0 && rx_buf_[idx + 1] == SYNC1) {
        idx += 2;
        state_ = ParseState::READ_HEADER;
      } else {
        idx += 1;
      }
      break;
    }
    case ParseState::READ_HEADER: {
      const size_t header_size = 1 + 4 + 8 + 2;
      const size_t header_remaining = (idx <= rx_len_) ? (rx_len_ - idx) : 0;
      if (header_remaining < header_size) {
        idx = rx_len_;
        break;
      }
      msg_type_ = rx_buf_[idx];
      seq_ = (uint32_t)rx_buf_[idx + 1] | ((uint32_t)rx_buf_[idx + 2] << 8) |
             ((uint32_t)rx_buf_[idx + 3] << 16) |
             ((uint32_t)rx_buf_[idx + 4] << 24);
      timestamp_ = 0;
      for (int b = 0; b < 8; ++b) {
        timestamp_ |= (uint64_t)rx_buf_[idx + 5 + b] << (8 * b);
      }
      payload_len_ =
          (uint16_t)rx_buf_[idx + 13] | ((uint16_t)rx_buf_[idx + 14] << 8);
      expected_len_ = 2 + header_size + payload_len_ + 4;
      if (payload_len_ > 256) {
        idx = idx + 1;
        state_ = ParseState::FIND_SYNC;
        break;
      }
      idx += header_size;
      state_ = ParseState::READ_PAYLOAD;
      break;
    }
    case ParseState::READ_PAYLOAD: {
      const size_t have = rx_len_ - idx;
      if (have < payload_len_) {
        idx = rx_len_;
        break;
      }
      idx += payload_len_;
      state_ = ParseState::READ_CRC;
      break;
    }
    case ParseState::READ_CRC: {
      const size_t header_size = 1 + 4 + 8 + 2;
      if (rx_len_ - idx < 4) {
        idx = rx_len_;
        break;
      }
      uint32_t received_crc = (uint32_t)rx_buf_[idx] |
                              ((uint32_t)rx_buf_[idx + 1] << 8) |
                              ((uint32_t)rx_buf_[idx + 2] << 16) |
                              ((uint32_t)rx_buf_[idx + 3] << 24);
      size_t packet_end = idx + 4; // exclusive
      size_t msg_type_pos =
          packet_end - (header_size + payload_len_ + 4) + 2; // +2 to skip sync
      if (msg_type_pos + 1 > rx_len_) {
        rx_len_ = 0;
        state_ = ParseState::FIND_SYNC;
        idx = 0;
        break;
      }
      const uint8_t *crc_base = rx_buf_ + msg_type_pos;
      size_t crc_len = header_size + payload_len_;
      uint32_t calc_crc = crc32_compute(crc_base, crc_len);
      if (calc_crc != received_crc) {
        if (rx_len_ > 0) {
          memmove(rx_buf_, rx_buf_ + 1, rx_len_ - 1);
          rx_len_ -= 1;
        }
        state_ = ParseState::FIND_SYNC;
        idx = 0;
        break;
      }

      const uint8_t *payload_ptr = rx_buf_ + msg_type_pos + header_size;
      if (msg_type_ == 0x01) {
        if (payload_len_ == NUM_STEPPERS * sizeof(float)) {
          for (size_t i = 0; i < NUM_STEPPERS; ++i) {
            float v;
            uint8_t tmp[4];
            tmp[0] = payload_ptr[i * 4 + 0];
            tmp[1] = payload_ptr[i * 4 + 1];
            tmp[2] = payload_ptr[i * 4 + 2];
            tmp[3] = payload_ptr[i * 4 + 3];
            memcpy(&v, tmp, sizeof(float));
            out_targets[i] = v;
          }
          out_seq = seq_;
          size_t consumed = packet_end;
          size_t remain = rx_len_ - consumed;
          if (remain > 0)
            memmove(rx_buf_, rx_buf_ + consumed, remain);
          rx_len_ = remain;
          state_ = ParseState::FIND_SYNC;
          return true;
        } else {
          rx_len_ = 0;
          state_ = ParseState::FIND_SYNC;
          idx = 0;
          break;
        }
      } else if (msg_type_ == 0x02) {
        size_t consumed = packet_end;
        size_t remain = rx_len_ - consumed;
        if (remain > 0)
          memmove(rx_buf_, rx_buf_ + consumed, remain);
        rx_len_ = remain;
        state_ = ParseState::FIND_SYNC;
        break;
      } else {
        rx_len_ = 0;
        state_ = ParseState::FIND_SYNC;
        idx = 0;
        break;
      }
      break;
    }
    }
  }

  return false;
}

void UartLeader::sendAck(uint32_t seq) {
  Serial1.print("ACK:");
  Serial1.println(seq);
}
