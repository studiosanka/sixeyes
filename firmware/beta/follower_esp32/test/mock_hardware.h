#pragma once

/**
 * @file mock_hardware.h
 * @brief Mock hardware interfaces for testing without real hardware
 *
 * Provides fake implementations of:
 * - HardwareSerial (UART)
 * - GPIO operations
 * - Timer/PWM functions
 * - FreeRTOS primitives
 *
 * Allows unit tests to run on desktop with full firmware logic
 */

#include <cstdint>
#include <cstring>
#include <queue>
#include <vector>
#include <functional>

/**
 * @class MockSerial
 * Simulates Arduino HardwareSerial for testing
 */
class MockSerial {
public:
    MockSerial() : available_count_(0), tx_buffer_pos_(0) {}
    
    // Simulate available data
    void simulateReceive(const char* data) {
        for (size_t i = 0; i < strlen(data); i++) {
            rx_queue_.push(static_cast<uint8_t>(data[i]));
        }
        available_count_ = rx_queue_.size();
    }
    
    // Check if data available
    int available() {
        available_count_ = rx_queue_.size();
        return available_count_;
    }
    
    // Read one byte
    int read() {
        if (rx_queue_.empty()) return -1;
        
        uint8_t byte = rx_queue_.front();
        rx_queue_.pop();
        available_count_--;
        return byte;
    }
    
    // Write data
    size_t write(uint8_t byte) {
        if (tx_buffer_pos_ < sizeof(tx_buffer_) - 1) {
            tx_buffer_[tx_buffer_pos_++] = byte;
            tx_buffer_[tx_buffer_pos_] = '\0';
            return 1;
        }
        return 0;
    }
    
    // Print functions
    void print(const char* str) {
        while (*str) write(*str++);
    }
    
    void println(const char* str) {
        print(str);
        write('\n');
    }
    
    void println() {
        write('\n');
    }
    
    // Get transmitted data
    const char* getTransmitted() const {
        return tx_buffer_;
    }
    
    void clearTransmitted() {
        memset(tx_buffer_, 0, sizeof(tx_buffer_));
        tx_buffer_pos_ = 0;
    }
    
    size_t getTxLength() const { return tx_buffer_pos_; }
    
private:
    std::queue<uint8_t> rx_queue_;
    int available_count_ = 0;
    char tx_buffer_[512] = {0};
    size_t tx_buffer_pos_ = 0;
};

/**
 * @class MockGPIO
 * Simulates GPIO pin operations
 */
class MockGPIO {
public:
    struct PinState {
        bool is_output = false;
        bool level = false;
        uint32_t frequency = 0;  // For PWM
        uint32_t duty = 0;       // For PWM
    };
    
    static MockGPIO& instance() {
        static MockGPIO gpio;
        return gpio;
    }
    
    void pinMode(uint8_t pin, uint8_t mode) {
        if (pin >= NUM_PINS_) return;
        pins_[pin].is_output = (mode == OUTPUT);
    }
    
    void digitalWrite(uint8_t pin, uint8_t level) {
        if (pin >= NUM_PINS_) return;
        pins_[pin].level = (level != 0);
    }
    
    int digitalRead(uint8_t pin) {
        if (pin >= NUM_PINS_) return 0;
        return pins_[pin].level ? 1 : 0;
    }
    
    void configureTimer(uint8_t pin, uint32_t freq, uint32_t duty) {
        if (pin >= NUM_PINS_) return;
        pins_[pin].frequency = freq;
        pins_[pin].duty = duty;
    }
    
    const PinState& getPin(uint8_t pin) const {
        static PinState dummy;
        if (pin >= NUM_PINS_) return dummy;
        return pins_[pin];
    }
    
    void reset() {
        for (auto& pin : pins_) {
            pin = PinState();
        }
    }
    
private:
    static constexpr size_t NUM_PINS_ = 48;
    PinState pins_[NUM_PINS_];
};

/**
 * @class MockTimer
 * Simulates FreeRTOS timers and delays
 */
class MockTimer {
public:
    static MockTimer& instance() {
        static MockTimer timer;
        return timer;
    }
    
    // Simulate passage of time
    void advanceMillis(uint32_t ms) {
        current_time_ms_ += ms;
    }
    
    uint32_t millis() const {
        return current_time_ms_;
    }
    
    uint32_t micros() const {
        return current_time_ms_ * 1000;
    }
    
    void reset() {
        current_time_ms_ = 0;
    }
    
private:
    uint32_t current_time_ms_ = 0;
};

/**
 * @class MockFreeRTOS
 * Simulates minimal FreeRTOS primitives
 */
class MockFreeRTOS {
public:
    using TaskFunction = std::function<void(void*)>;
    
    enum class TaskState { SUSPENDED, RUNNING, BLOCKED };
    
    struct Task {
        TaskFunction func;
        void* param;
        TaskState state;
        uint32_t priority;
    };
    
    static MockFreeRTOS& instance() {
        static MockFreeRTOS rtos;
        return rtos;
    }
    
    // Create task simulation
    int createTask(TaskFunction func, void* param, uint32_t priority = 1) {
        Task task{func, param, TaskState::RUNNING, priority};
        tasks_.push_back(task);
        return tasks_.size() - 1;
    }
    
    // Execute all tasks once (for testing)
    void runAllTasks() {
        for (auto& task : tasks_) {
            if (task.state == TaskState::RUNNING && task.func) {
                task.func(task.param);
            }
        }
    }
    
    size_t getTaskCount() const { return tasks_.size(); }
    
    void reset() {
        tasks_.clear();
    }
    
private:
    std::vector<Task> tasks_;
};

/**
 * @class TestFixture
 * Base class for all unit tests with setup/teardown
 */
class TestFixture {
public:
    virtual ~TestFixture() = default;
    
    virtual void setUp() {
        mock_serial_.clearTransmitted();
        MockGPIO::instance().reset();
        MockTimer::instance().reset();
        MockFreeRTOS::instance().reset();
    }
    
    virtual void tearDown() {}
    
protected:
    MockSerial mock_serial_;
};
