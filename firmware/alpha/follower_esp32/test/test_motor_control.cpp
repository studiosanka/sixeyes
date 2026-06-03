#include "mock_hardware.h"
#include <cassert>
#include <cstdio>
#include <cmath>

/**
 * @file test_motor_control.cpp
 * @brief Unit tests for motor PID controller
 *
 * Tests:
 * - PID gains application
 * - Proportional response
 * - Integral wind-up protection
 * - Error settling
 * - Target tracking accuracy
 */

#define TEST_ASSERT(condition) \
    if (!(condition)) { \
        printf("FAIL: %s:%d\n", __FILE__, __LINE__); \
        return false; \
    }

#define TEST_ASSERT_NEAR(actual, expected, tolerance) \
    if (fabs((actual) - (expected)) > (tolerance)) { \
        printf("FAIL: %s:%d - Expected ~%.2f, got %.2f (tol: %.2f)\n", \
            __FILE__, __LINE__, (double)(expected), (double)(actual), (double)(tolerance)); \
        return false; \
    }

/**
 * @class SimplePIDController
 * Minimal PID implementation for testing
 */
class SimplePIDController {
public:
    float kp = 2.0f;
    float ki = 0.05f;
    float kd = 0.1f;
    
    struct State {
        float position = 0.0f;
        float velocity = 0.0f;
        float error = 0.0f;
        float error_integral = 0.0f;
        float error_derivative = 0.0f;
    } state;
    
    float update(float target, float current, float dt) {
        float error = target - current;
        
        // Proportional term
        float p_term = kp * error;
        
        // Integral term with anti-windup
        state.error_integral += error * dt;
        if (state.error_integral > 100.0f) state.error_integral = 100.0f;
        if (state.error_integral < -100.0f) state.error_integral = -100.0f;
        float i_term = ki * state.error_integral;
        
        // Derivative term
        float error_derivative = (error - state.error) / dt;
        state.error_derivative = error_derivative;
        float d_term = kd * error_derivative;
        
        // Update state
        state.error = error;
        state.position = current;
        
        return p_term + i_term + d_term;
    }
    
    void reset() {
        state = State();
    }
};

/**
 * @test PID Proportional Response
 * Scenario: Step input from 0° to 90°, measure P term
 * Expected: P term = Kp * error = 2.0 * 90 = 180
 */
bool test_pid_proportional() {
    printf("Test: PID proportional response... ");
    
    SimplePIDController pid;
    pid.reset();
    
    float target = 90.0f;
    float current = 0.0f;
    
    // Calculate output
    float output = pid.update(target, current, 0.001f);
    
    // P term should dominate initially (I and D are small)
    float expected_p_term = pid.kp * target;  // 2.0 * 90 = 180
    
    // Allow 10% tolerance (due to I and D terms)
    TEST_ASSERT_NEAR(output, expected_p_term, expected_p_term * 0.1f);
    
    printf("PASS\n");
    return true;
}

/**
 * @test PID Integral Wind-up Protection
 * Scenario: Large sustained error with Ki > 0
 * Expected: Integral term saturates at limits
 */
bool test_pid_integral_windup() {
    printf("Test: PID integral anti-windup... ");
    
    SimplePIDController pid;
    pid.reset();
    
    float target = 180.0f;
    float current = 0.0f;
    float dt = 0.001f;  // 1 ms
    
    // Simulate 2 seconds of error with no motion
    for (int i = 0; i < 2000; i++) {
        pid.update(target, current, dt);
    }
    
    // Integral should be clamped, not unbounded
    TEST_ASSERT(pid.state.error_integral <= 100.0f);
    TEST_ASSERT(pid.state.error_integral >= -100.0f);
    
    printf("PASS\n");
    return true;
}

/**
 * @test PID Step Response
 * Scenario: Target changes from 0° to 90°, system reaches target
 * Expected: Error converges to near zero
 */
bool test_pid_step_response() {
    printf("Test: PID step response and settling... ");
    
    SimplePIDController pid;
    pid.reset();
    
    float target = 90.0f;
    float current = 0.0f;
    float dt = 0.01f;  // 10 ms (100 Hz control loop)
    
    // Simulate motor response: current position approaches target
    // Simple model: dPos/dt = 0.5 * output (lag)
    float velocity = 0.0f;
    
    for (int step = 0; step < 500; step++) {  // 5 seconds
        float output = pid.update(target, current, dt);
        
        // Simple motor model: velocity = output * gain
        velocity = output * 0.5f;
        current += velocity * dt;
        
        // Limit position to valid range
        if (current > 180.0f) current = 180.0f;
        if (current < 0.0f) current = 0.0f;
    }
    
    // After 5 seconds, should be very close to target
    float error = fabs(target - current);
    TEST_ASSERT(error < 5.0f);  // Within 5° of target
    
    printf("PASS\n");
    return true;
}

/**
 * @test PID Gain Tuning
 * Scenario: Adjust gains and measure settling behavior
 * Expected: Increased Kp = faster response (but more overshoot)
 */
bool test_pid_gain_adjustment() {
    printf("Test: PID gain tuning... ");
    
    // Test with conservative gains
    SimplePIDController pid_slow;
    pid_slow.kp = 1.0f;
    pid_slow.ki = 0.01f;
    pid_slow.kd = 0.05f;
    pid_slow.reset();
    
    // Test with aggressive gains
    SimplePIDController pid_fast;
    pid_fast.kp = 5.0f;
    pid_fast.ki = 0.1f;
    pid_fast.kd = 0.2f;
    pid_fast.reset();
    
    float target = 90.0f;
    float current_slow = 0.0f, current_fast = 0.0f;
    float dt = 0.01f;
    
    // Step response - measure at 1 second
    for (int i = 0; i < 100; i++) {
        float out_slow = pid_slow.update(target, current_slow, dt);
        float out_fast = pid_fast.update(target, current_fast, dt);
        
        current_slow += out_slow * 0.01f * dt;
        current_fast += out_fast * 0.01f * dt;
    }
    
    // Faster PID should reach target quicker
    TEST_ASSERT(current_fast > current_slow);
    TEST_ASSERT(current_fast > 0.1f);
    
    printf("PASS\n");
    return true;
}

/**
 * @test Multi-Motor Coordination
 * Scenario: Control 4 motors simultaneously
 * Expected: Each motor tracks its target independently
 */
bool test_multi_motor_coordination() {
    printf("Test: Multi-motor coordination... ");
    
    SimplePIDController motors[4];
    float targets[4] = {0.0f, 90.0f, 180.0f, 45.0f};
    float positions[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    
    float dt = 0.01f;
    
    // Run control loop
    for (int step = 0; step < 500; step++) {
        for (int motor = 0; motor < 4; motor++) {
            float output = motors[motor].update(targets[motor], positions[motor], dt);
            positions[motor] += output * 0.01f * dt;
        }
    }
    
    // Each motor should be close to its target
    for (int i = 0; i < 4; i++) {
        TEST_ASSERT(fabs(positions[i] - targets[i]) < 5.0f);
    }
    
    printf("PASS\n");
    return true;
}

/**
 * @test Zero Error Convergence
 * Scenario: Motor at target, PID maintains position
 * Expected: Output approaches zero, no drift
 */
bool test_pid_zero_error() {
    printf("Test: PID steady-state at target... ");
    
    SimplePIDController pid;
    pid.reset();
    
    float target = 90.0f;
    float current = 90.0f;  // Already at target
    float dt = 0.01f;
    
    // Run for 1 second
    for (int i = 0; i < 100; i++) {
        float output = pid.update(target, current, dt);
        
        // With zero error, output should be zero
        TEST_ASSERT(fabs(output) < 0.1f);
    }
    
    printf("PASS\n");
    return true;
}

/**
 * Run all motor control tests
 */
bool run_all_motor_control_tests() {
    printf("\n=== Motor Control Tests ===\n\n");
    
    bool passed = 0, failed = 0;
    
    if (test_pid_proportional()) passed++; else failed++;
    if (test_pid_integral_windup()) passed++; else failed++;
    if (test_pid_step_response()) passed++; else failed++;
    if (test_pid_gain_adjustment()) passed++; else failed++;
    if (test_multi_motor_coordination()) passed++; else failed++;
    if (test_pid_zero_error()) passed++; else failed++;
    
    printf("\n=== Summary ===\n");
    printf("Passed: %d\n", passed);
    printf("Failed: %d\n", failed);
    printf("Total:  %d\n\n", passed + failed);
    
    return failed == 0;
}
