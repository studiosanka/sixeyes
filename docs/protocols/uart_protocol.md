# UART Protocol (canonical)

This file is the canonical UART protocol sketch for Leader → Follower absolute joint target messages.

Design constraints (must be followed):
- Only absolute joint targets allowed (no delta commands).
- Include sequence number and CRC32 for integrity.
- Heartbeat is a short, frequent packet; heartbeat timeout is ~500 ms.

Packet suggestion:
- SYNC (0xAA 0x55)
- MSG_TYPE (1 byte)
- SEQ (uint32)
- TIMESTAMP (uint64)
- PAYLOAD_LEN (uint16)
- PAYLOAD (binary or JSON)
- CRC32 (uint32) computed over MSG_TYPE..PAYLOAD

ACKs: follower should ACK valid SEQ numbers; on invalid packets, NACK with error code.
