# follower_esp32

Firmware running on `ESP32-S3-WROOM-1` responsible for deterministic control and safety according to the Technical Reference.

Key modules:
- motor_control
- drivers/tmc2209
- servo_control
- safety/heartbeat_monitor
- comms/uart_leader
- comms/usb_cdc (telemetry)

Motors are disabled by default on boot. See `modules/config/board_config.h`.
