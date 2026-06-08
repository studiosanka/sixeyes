#pragma once
#include "modules/config/board_config.h"
#include <array>
#include <cstdint>

class UartLeader {
public:
  static UartLeader &instance();
  void init(unsigned long baud = 115200);
  bool available();
  // Parse into absolute joint array; returns true on success
  bool readAbsoluteTargets(std::array<float, NUM_STEPPERS> &out_targets,
                           uint32_t &out_seq);
  void sendAck(uint32_t seq);

private:
  UartLeader();
  // Parser state (non-blocking across calls)
  enum class ParseState : uint8_t {
    FIND_SYNC,
    READ_HEADER,
    READ_PAYLOAD,
    READ_CRC
  };
  static constexpr size_t RX_BUFFER_SIZE = 512;
  ParseState state_ = ParseState::FIND_SYNC;
  uint8_t rx_buf_[RX_BUFFER_SIZE];
  size_t rx_len_ = 0;       // bytes currently in buffer
  size_t expected_len_ = 0; // total expected packet length when header known
  // temporary header fields
  uint8_t msg_type_ = 0;
  uint32_t seq_ = 0;
  uint64_t timestamp_ = 0;
  uint16_t payload_len_ = 0;
};
