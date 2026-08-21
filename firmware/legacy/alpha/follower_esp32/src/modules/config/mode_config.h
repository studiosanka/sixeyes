/**
 * @file mode_config.h
 * @brief Firmware operation mode configuration
 *
 * Defines which operation mode the firmware should compile for:
 * - VLA_INFERENCE: Laptop ROS2 → Follower (AI task execution)
 * - TELEOPERATION: Leader → Follower (real-time mirroring)
 *
 * Set at compile time via platformio.ini:
 *   build_flags = -DOPERATION_MODE=VLA_INFERENCE
 *
 * Usage:
 *   #if OPERATION_MODE == VLA_INFERENCE
 *     // VLA-specific code
 *   #endif
 */

#ifndef MODE_CONFIG_H
#define MODE_CONFIG_H

// ============================================================================
// OPERATION MODE SELECTION
// ============================================================================

// Mode definitions (use for comparison with OPERATION_MODE)
#define MODE_VLA_INFERENCE 1 // AI task execution from laptop ROS2
#define MODE_TELEOPERATION 2 // Real-time mirroring from leader arm

// Current operation mode (set via platformio.ini build flag)
#ifndef OPERATION_MODE
#define OPERATION_MODE MODE_VLA_INFERENCE // Default: VLA inference mode
#endif

// ============================================================================
// MODE-SPECIFIC PARAMETERS
// ============================================================================

#if OPERATION_MODE == MODE_VLA_INFERENCE

// ========== VLA INFERENCE MODE CONFIG ==========

// Heartbeat monitoring (ROS2 safety node)
#define MODE_HEARTBEAT_TIMEOUT_MS 500 // Lose ROS2 after 500ms without heartbeat
#define MODE_HEARTBEAT_RX_FREQUENCY_HZ 50 // Expected heartbeat rate (50+ Hz)

// Command receipt
#define MODE_COMMAND_TIMEOUT_MS 500    // Lose ROS2 connection after 500ms
#define MODE_MAX_JSON_MESSAGE_SIZE 512 // Max command message size

// Status transmission
#define MODE_STATUS_TX_FREQUENCY_HZ 10 // Send status at 10 Hz
#define MODE_STATUS_TX_INTERVAL_MS 100 // 100ms between status updates

// Communication medium
#define MODE_COMMS_SERIAL_RATE 115200 // USB-CDC baud rate
#define MODE_COMMS_TIMEOUT_MS 100     // UART read timeout (100ms)

#elif OPERATION_MODE == MODE_TELEOPERATION

// ========== TELEOPERATION MODE CONFIG ==========

// Command receipt (real-time mirroring)
#define MODE_COMMAND_TIMEOUT_MS 100        // Much stricter: 100ms timeout
#define MODE_HEARTBEAT_RX_FREQUENCY_HZ 100 // Expect 100 Hz command stream

// Telemetry transmission (high-bandwidth feedback)
#define MODE_TELEMETRY_TX_FREQUENCY_HZ 100 // Send telemetry at 100 Hz
#define MODE_TELEMETRY_TX_INTERVAL_MS 10   // 10ms between telemetry updates

// Communication medium (same as VLA, but higher throughput expected)
#define MODE_COMMS_SERIAL_RATE 115200
#define MODE_MAX_JSON_MESSAGE_SIZE 512

// Latency tracking
#define MODE_LATENCY_WARNING_THRESHOLD_MS 50 // Warn if >50ms latency detected

#else

#error                                                                         \
    "OPERATION_MODE not recognized. Set via: -DOPERATION_MODE=MODE_VLA_INFERENCE or MODE_TELEOPERATION"

#endif

// ============================================================================
// COMMON PARAMETERS (All modes)
// ============================================================================

// Control loop timing
// Use board_config.h for canonical CONTROL_LOOP_HZ to avoid duplicate
// definitions
#include "board_config.h"

// Derived timing values
#define CONTROL_LOOP_PERIOD_MS (1000 / CONTROL_LOOP_HZ) // e.g. 2.5 ms

// Motor safety
#define MOTOR_DISABLE_LATENCY_MS                                               \
  (CONTROL_LOOP_PERIOD_MS) // Must disable in 1 cycle

// Telemetry rate (collected regardless of mode)
#define TELEMETRY_COLLECTION_HZ 100 // Collect sensor data at 100 Hz

// ============================================================================
// UTILITY MACROS
// ============================================================================

// Compile-time mode checking
#define IS_VLA_MODE() (OPERATION_MODE == MODE_VLA_INFERENCE)
#define IS_TELEOPERATION_MODE() (OPERATION_MODE == MODE_TELEOPERATION)

// Mode name for debugging
#define MODE_NAME()                                                            \
  (OPERATION_MODE == MODE_VLA_INFERENCE   ? "VLA_INFERENCE"                    \
   : OPERATION_MODE == MODE_TELEOPERATION ? "TELEOPERATION"                    \
                                          : "UNKNOWN")

#endif // MODE_CONFIG_H
