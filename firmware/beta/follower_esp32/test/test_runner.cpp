/**
 * @file test_runner.cpp
 * @brief Main test runner for all unit tests
 *
 * Compiles and runs all unit tests on the host machine.
 * No hardware required.
 *
 * Compile with:
 *   g++ -std=c++17 -o test_runner test_runner.cpp \
 *       test_heartbeat_parser.cpp test_safety_task.cpp test_motor_control.cpp
 *
 * Run with:
 *   ./test_runner
 */

#include <cstdio>
#include <cstdlib>

// Forward declarations of test suites
extern bool run_all_heartbeat_tests();
extern bool run_all_safety_tests();
extern bool run_all_motor_control_tests();

/**
 * @brief Main test runner
 * Executes all test suites and reports summary
 */
int main() {
    printf("\n");
    printf("╔═════════════════════════════════════════════════════╗\n");
    printf("║   SixEyes Firmware Unit Test Suite                  ║\n");
    printf("║   Platform: Linux/Windows/macOS                     ║\n");
    printf("║   Date: February 25, 2026                           ║\n");
    printf("╚═════════════════════════════════════════════════════╝\n");
    
    int total_passed = 0;
    int total_failed = 0;
    
    // Run heartbeat parser tests
    bool hb_pass = run_all_heartbeat_tests();
    if (!hb_pass) total_failed++;
    if (hb_pass) total_passed++;
    
    // Run safety task tests
    bool safety_pass = run_all_safety_tests();
    if (!safety_pass) total_failed++;
    if (safety_pass) total_passed++;
    
    // Run motor control tests
    bool motor_pass = run_all_motor_control_tests();
    if (!motor_pass) total_failed++;
    if (motor_pass) total_passed++;
    
    // Overall summary
    printf("\n");
    printf("╔═════════════════════════════════════════════════════╗\n");
    printf("║   OVERALL TEST SUMMARY                              ║\n");
    printf("╚═════════════════════════════════════════════════════╝\n");
    printf("\nTest Suites Passed: %d / 3\n", total_passed);
    printf("Test Suites Failed: %d / 3\n", total_failed);
    
    if (total_failed == 0) {
        printf("\n✅ ALL TESTS PASSED!\n\n");
        return EXIT_SUCCESS;
    } else {
        printf("\n❌ SOME TESTS FAILED!\n\n");
        return EXIT_FAILURE;
    }
}
