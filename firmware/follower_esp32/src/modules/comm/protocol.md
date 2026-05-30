UART protocol (follower expects absolute joint targets)

- Packet: [SYNC][SEQ][PAYLOAD_LEN][PAYLOAD][CRC32]
- PAYLOAD: JSON or compact binary describing absolute joint angles/positions, timestamp, and optional flags
- Sequence numbers must be monotonic; follower SHOULD ACK by seq.
- Only absolute targets are permitted. No delta commands.

Heartbeat: separate short message `HB` with node identifier and seq.

See docs/protocols/uart_protocol.md for canonical spec.
