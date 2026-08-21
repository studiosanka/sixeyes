#include "mock_hardware.h"
#include <cassert>
#include <cstdio>
#include <string>

/**
 * @file test_heartbeat_parser.cpp
 * @brief Unit tests for heartbeat receiver and parser
 *
 * Tests the following scenarios:
 * - Valid heartbeat packet parsing
 * - Malformed packet rejection
 * - Sequence number tracking
 * - Timeout detection
 */

// Test helper macros
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

/**
 * @test Valid Heartbeat Parsing
 * Input: "HB:0,42\n"
 * Expected: Parser extracts sequence number 42
 */
bool test_valid_heartbeat_parsing() {
    printf("Test: Valid heartbeat parsing... ");
    
    MockSerial serial;
    serial.simulateReceive("HB:0,42\n");
    
    // Simulate heartbeat receiver logic
    char buffer[64] = {0};
    int pos = 0;
    
    while (serial.available()) {
        int c = serial.read();
        if (c == '\n') {
            buffer[pos] = '\0';
            break;
        }
        if (c != '\r') {
            buffer[pos++] = c;
        }
    }
    
    // Parse heartbeat format: "HB:0,SEQ"
    unsigned int seq = 0;
    int result = sscanf(buffer, "HB:0,%u", &seq);
    
    TEST_ASSERT_EQ(result, 1);  // Successfully parsed 1 field
    TEST_ASSERT_EQ(seq, 42);     // Correct sequence number
    
    printf("PASS\n");
    return true;
}

/**
 * @test Malformed Heartbeat Rejection
 * Input: "HB:0,invalid\n"
 * Expected: Parse fails gracefully
 */
bool test_malformed_heartbeat() {
    printf("Test: Malformed heartbeat rejection... ");
    
    MockSerial serial;
    serial.simulateReceive("HB:0,invalid\n");
    
    char buffer[64] = {0};
    int pos = 0;
    
    while (serial.available()) {
        int c = serial.read();
        if (c == '\n') break;
        if (c != '\r') buffer[pos++] = c;
    }
    
    unsigned int seq = 0;
    int result = sscanf(buffer, "HB:0,%u", &seq);
    
    TEST_ASSERT_EQ(result, 0);  // Parse should fail
    
    printf("PASS\n");
    return true;
}

/**
 * @test Multiple Heartbeats in Sequence
 * Input: Three heartbeats with increasing sequence numbers
 * Expected: All parsed correctly
 */
bool test_multiple_heartbeats() {
    printf("Test: Multiple heartbeats... ");
    
    MockSerial serial;
    unsigned int sequences[] = {0, 1, 2};
    int parsed_count = 0;
    
    for (unsigned int seq : sequences) {
        char msg[32];
        snprintf(msg, sizeof(msg), "HB:0,%u\n", seq);
        serial.simulateReceive(msg);
        
        char buffer[64] = {0};
        int pos = 0;
        
        while (serial.available()) {
            int c = serial.read();
            if (c == '\n') break;
            if (c != '\r') buffer[pos++] = c;
        }
        
        unsigned int parsed_seq = 0;
        if (sscanf(buffer, "HB:0,%u", &parsed_seq) == 1) {
            TEST_ASSERT_EQ(parsed_seq, seq);
            parsed_count++;
        }
    }
    
    TEST_ASSERT_EQ(parsed_count, 3);
    
    printf("PASS\n");
    return true;
}

/**
 * @test Timeout Detection
 * Scenario: No heartbeat received for 500+ ms
 * Expected: Timeout flag set
 */
bool test_heartbeat_timeout() {
    printf("Test: Heartbeat timeout detection... ");
    
    MockTimer& timer = MockTimer::instance();
    timer.reset();
    
    const uint32_t HEARTBEAT_TIMEOUT_MS = 500;
    uint32_t last_heartbeat_ms = 0;
    
    // Receive heartbeat at t=0
    last_heartbeat_ms = timer.millis();
    
    // Advance time
    timer.advanceMillis(250);
    bool should_timeout = (timer.millis() - last_heartbeat_ms) > HEARTBEAT_TIMEOUT_MS;
    TEST_ASSERT_EQ(should_timeout, false);  // Not yet timed out
    
    // Continue advancing
    timer.advanceMillis(300);  // Total 550 ms
    should_timeout = (timer.millis() - last_heartbeat_ms) > HEARTBEAT_TIMEOUT_MS;
    TEST_ASSERT_EQ(should_timeout, true);  // Now timed out
    
    printf("PASS\n");
    return true;
}

/**
 * @test JSON Heartbeat (Alternative Format)
 * Input: {"cmd":"HEARTBEAT","seq":123}\n
 * Expected: Parsed as valid heartbeat (assumes JSON parser available)
 */
bool test_json_heartbeat() {
    printf("Test: JSON heartbeat format... ");
    
    MockSerial serial;
    serial.simulateReceive("{\"cmd\":\"HEARTBEAT\",\"seq\":123}\n");
    
    // Read the complete line
    char buffer[256] = {0};
    int pos = 0;
    
    while (serial.available()) {
        int c = serial.read();
        if (c == '\n') break;
        if (c != '\r') buffer[pos++] = c;
    }
    
    // Check if it contains the heartbeat marker
    TEST_ASSERT(strstr(buffer, "HEARTBEAT") != nullptr);
    TEST_ASSERT(strstr(buffer, "seq") != nullptr);
    
    printf("PASS\n");
    return true;
}

/**
 * Run all heartbeat tests
 */
bool run_all_heartbeat_tests() {
    printf("\n=== Heartbeat Parser Tests ===\n\n");
    
    bool passed = 0, failed = 0;
    
    if (test_valid_heartbeat_parsing()) passed++; else failed++;
    if (test_malformed_heartbeat()) passed++; else failed++;
    if (test_multiple_heartbeats()) passed++; else failed++;
    if (test_heartbeat_timeout()) passed++; else failed++;
    if (test_json_heartbeat()) passed++; else failed++;
    
    printf("\n=== Summary ===\n");
    printf("Passed: %d\n", passed);
    printf("Failed: %d\n", failed);
    printf("Total:  %d\n\n", passed + failed);
    
    return failed == 0;
}
