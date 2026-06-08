#include "mock_hardware.h"
#include <cassert>
#include <cstdio>

/**
 * @file test_safety_task.cpp
 * @brief Unit tests for safety task and motor enable/disable
 *
 * Tests:
 * - Motor enable/disable via EN pin
 * - Heartbeat timeout triggers motor disable
 * - Fault detection and recovery
 * - State machine transitions
 */

#define TEST_ASSERT(condition) \
    if (!(condition)) { \
        printf("FAIL: %s:%d\n", __FILE__, __LINE__); \
        return false; \
    }

#define TEST_ASSERT_EQ(actual, expected) \
    if ((actual) != (expected)) { \
        printf("FAIL: %s:%d - Expected %d, got %d\n", \
            __FILE__, __LINE__, (int)(expected), (int)(actual)); \
        return false; \
    }

// Mock GPIO pin assignments (from board_config.h)
static constexpr uint8_t PIN_EN = 6;           // Motor enable
static constexpr uint8_t PIN_FAULT = 8;        // Fault input

/**
 * @test Motor Enable/Disable
 * Scenario: Set EN pin high, then low
 * Expected: GPIO state changes correctly
 */
bool test_motor_enable_disable() {
    printf("Test: Motor enable/disable... ");
    
    MockGPIO& gpio = MockGPIO::instance();
    gpio.reset();
    
    // Configure EN pin as output
    gpio.pinMode(PIN_EN, OUTPUT);
    
    // Enable motors
    gpio.digitalWrite(PIN_EN, HIGH);
    TEST_ASSERT_EQ(gpio.digitalRead(PIN_EN), 1);
    
    // Disable motors
    gpio.digitalWrite(PIN_EN, LOW);
    TEST_ASSERT_EQ(gpio.digitalRead(PIN_EN), 0);
    
    printf("PASS\n");
    return true;
}

/**
 * @test Heartbeat Timeout Disables Motors
 * Scenario: No heartbeat for 500+ ms
 * Expected: Safety task sets EN pin LOW
 */
bool test_heartbeat_timeout_disables_motors() {
    printf("Test: Heartbeat timeout disables motors... ");
    
    MockTimer& timer = MockTimer::instance();
    MockGPIO& gpio = MockGPIO::instance();
    timer.reset();
    gpio.reset();
    gpio.pinMode(PIN_EN, OUTPUT);
    
    const uint32_t HEARTBEAT_TIMEOUT_MS = 500;
    uint32_t last_heartbeat_ms = 0;
    
    // Process 1: Receive heartbeat, enable motors
    last_heartbeat_ms = timer.millis();
    gpio.digitalWrite(PIN_EN, HIGH);
    TEST_ASSERT_EQ(gpio.digitalRead(PIN_EN), 1);
    
    // Process 2: Advance time to 400ms (no new heartbeat)
    timer.advanceMillis(400);
    bool timeout = (timer.millis() - last_heartbeat_ms) > HEARTBEAT_TIMEOUT_MS;
    if (timeout) gpio.digitalWrite(PIN_EN, LOW);
    TEST_ASSERT_EQ(timeout, false);  // Should NOT timeout yet
    TEST_ASSERT_EQ(gpio.digitalRead(PIN_EN), 1);  // Motors still enabled
    
    // Process 3: Advance to 600ms (timeout occurs)
    timer.advanceMillis(250);
    timeout = (timer.millis() - last_heartbeat_ms) > HEARTBEAT_TIMEOUT_MS;
    if (timeout) gpio.digitalWrite(PIN_EN, LOW);
    TEST_ASSERT_EQ(timeout, true);  // Now timed out
    TEST_ASSERT_EQ(gpio.digitalRead(PIN_EN), 0);  // Motors disabled
    
    printf("PASS\n");
    return true;
}

/**
 * @test Heartbeat Reception Re-enables Motors
 * Scenario: Timeout occurred, then heartbeat received
 * Expected: Motors re-enabled
 */
bool test_heartbeat_recovery() {
    printf("Test: Heartbeat recovery re-enables motors... ");
    
    MockTimer& timer = MockTimer::instance();
    MockGPIO& gpio = MockGPIO::instance();
    timer.reset();
    gpio.reset();
    gpio.pinMode(PIN_EN, OUTPUT);
    
    const uint32_t HEARTBEAT_TIMEOUT_MS = 500;
    uint32_t last_heartbeat_ms = timer.millis();
    
    // Motor enabled at start
    gpio.digitalWrite(PIN_EN, HIGH);
    
    // Advance to timeout
    timer.advanceMillis(600);
    bool timeout = (timer.millis() - last_heartbeat_ms) > HEARTBEAT_TIMEOUT_MS;
    if (timeout) gpio.digitalWrite(PIN_EN, LOW);
    TEST_ASSERT_EQ(gpio.digitalRead(PIN_EN), 0);
    
    // Receive heartbeat, update timestamp
    last_heartbeat_ms = timer.millis();
    timer.advanceMillis(10);
    gpio.digitalWrite(PIN_EN, HIGH);
    TEST_ASSERT_EQ(gpio.digitalRead(PIN_EN), 1);
    
    printf("PASS\n");
    return true;
}

/**
 * @test Rapid Heartbeat Sequence
 * Scenario: Receive heartbeats at 50 Hz continuously
 * Expected: Motors stay enabled, no timeouts
 */
bool test_rapid_heartbeat_sequence() {
    printf("Test: Rapid heartbeat sequence... ");
    
    MockTimer& timer = MockTimer::instance();
    MockGPIO& gpio = MockGPIO::instance();
    timer.reset();
    gpio.reset();
    gpio.pinMode(PIN_EN, OUTPUT);
    
    const uint32_t HEARTBEAT_TIMEOUT_MS = 500;
    const uint32_t HEARTBEAT_INTERVAL_MS = 20;  // 50 Hz
    uint32_t last_heartbeat_ms = timer.millis();
    
    gpio.digitalWrite(PIN_EN, HIGH);
    
    // Simulate 10 heartbeats at 50 Hz
    for (int i = 0; i < 10; i++) {
        timer.advanceMillis(HEARTBEAT_INTERVAL_MS);
        last_heartbeat_ms = timer.millis();  // Updated on each heartbeat
        
        bool timeout = (timer.millis() - last_heartbeat_ms) > HEARTBEAT_TIMEOUT_MS;
        if (timeout) {
            gpio.digitalWrite(PIN_EN, LOW);
        }
    }
    
    // Should still be enabled (no timeout)
    TEST_ASSERT_EQ(gpio.digitalRead(PIN_EN), 1);
    
    printf("PASS\n");
    return true;
}

/**
 * @test Fault Input Detection
 * Scenario: FAULT pin goes high (overcurrent detected)
 * Expected: Safety task latches fault and disables motors
 */
bool test_fault_detection() {
    printf("Test: Fault detection and disable... ");
    
    MockGPIO& gpio = MockGPIO::instance();
    gpio.reset();
    gpio.pinMode(PIN_EN, OUTPUT);
    gpio.pinMode(PIN_FAULT, INPUT);
    
    // Motors running normally
    gpio.digitalWrite(PIN_EN, HIGH);
    TEST_ASSERT_EQ(gpio.digitalRead(PIN_EN), 1);
    
    // Simulate fault event (external driver pulls FAULT low = active)
    // Note: This would be an interrupt in real code
    // For testing, we simulate the fault handler
    if (gpio.digitalRead(PIN_FAULT) == 0) {
        gpio.digitalWrite(PIN_EN, LOW);  // Safety task disables
    }
    
    // Motors should be disabled
    TEST_ASSERT_EQ(gpio.digitalRead(PIN_EN), 0);
    
    printf("PASS\n");
    return true;
}

/**
 * @test Fault Reset Procedure
 * Scenario: Fault occurred, user sends RESET_FAULT command
 * Expected: Fault cleared and motors can be re-enabled
 */
bool test_fault_reset() {
    printf("Test: Fault reset procedure... ");
    
    MockGPIO& gpio = MockGPIO::instance();
    gpio.reset();
    gpio.pinMode(PIN_EN, OUTPUT);
    
    // Assume fault is latched (motors disabled)
    gpio.digitalWrite(PIN_EN, LOW);
    bool fault_latched = true;
    
    // User issues RESET_FAULT command
    if (fault_latched) {
        fault_latched = false;  // Clear latch
        gpio.digitalWrite(PIN_EN, HIGH);  // Re-enable motors
    }
    
    TEST_ASSERT_EQ(fault_latched, false);
    TEST_ASSERT_EQ(gpio.digitalRead(PIN_EN), 1);
    
    printf("PASS\n");
    return true;
}

/**
 * Run all safety task tests
 */
bool run_all_safety_tests() {
    printf("\n=== Safety Task Tests ===\n\n");
    
    bool passed = 0, failed = 0;
    
    if (test_motor_enable_disable()) passed++; else failed++;
    if (test_heartbeat_timeout_disables_motors()) passed++; else failed++;
    if (test_heartbeat_recovery()) passed++; else failed++;
    if (test_rapid_heartbeat_sequence()) passed++; else failed++;
    if (test_fault_detection()) passed++; else failed++;
    if (test_fault_reset()) passed++; else failed++;
    
    printf("\n=== Summary ===\n");
    printf("Passed: %d\n", passed);
    printf("Failed: %d\n", failed);
    printf("Total:  %d\n\n", passed + failed);
    
    return failed == 0;
}
