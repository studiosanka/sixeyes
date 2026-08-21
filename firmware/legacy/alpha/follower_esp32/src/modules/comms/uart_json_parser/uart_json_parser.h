#pragma once

#include "modules/config/board_config.h"
#include "uart_json_messages.h"
#include <Arduino.h>
#include <cstdint>
#include <functional>

/**
 * @file uart_json_parser.h
 * @brief JSON message parser for UART communication
 *
 * Provides non-blocking parsing of JSON messages from UART with optional
 * handlers (callbacks) for different message types.
 *
 * Design:
 * - Line-buffered input (messages end with \n)
 * - Callback-based message routing
 * - Extensible message types
 * - Minimal memory overhead
 * - Non-blocking parsing (no delays in control loop)
 */

class UartJsonParser {
public:
  /**
   * @brief Message handler callback type
   * Handler signature: void handler(const BaseMessage& msg)
   * Called when a valid message of the registered type is parsed.
   */
  using MessageHandler = std::function<void(const BaseMessage &)>;

  /**
   * @brief Error handler callback type
   * Called when a parse error occurs.
   */
  using ErrorHandler = std::function<void(const char *error, uint32_t line)>;

  /**
   * @brief Singleton instance access
   */
  static UartJsonParser &instance();

  /**
   * @brief Initialize the parser
   * @param uart Pointer to HardwareSerial port (typically &Serial for USB-CDC)
   * @param buffer_size Maximum message size (default 512 bytes)
   */
  void init(HardwareSerial *uart, size_t buffer_size = 512);

  /**
   * @brief Update parser; process any available data
   * Non-blocking call that checks for complete lines and parses JSON.
   * Should be called regularly from the control loop.
   * @return Number of complete messages parsed
   */
  uint8_t update();

  /**
   * @brief Register handler for a message type
   * @param type MessageType to handle
   * @param handler Callback function
   */
  void registerHandler(MessageType type, MessageHandler handler);

  /**
   * @brief Unregister handler for a message type
   * @param type MessageType to stop handling
   */
  void unregisterHandler(MessageType type);

  /**
   * @brief Set global error handler
   * @param handler Callback for parse errors
   */
  void setErrorHandler(ErrorHandler handler);

  /**
   * @brief Get last error message
   * @return Error description (empty if no error)
   */
  const char *getLastError() const;

  /**
   * @brief Get parse statistics
   * @return Total messages parsed
   */
  uint32_t getMessageCount() const { return message_count_; }

  /**
   * @brief Get parse error statistics
   * @return Total parse errors
   */
  uint32_t getErrorCount() const { return error_count_; }

  /**
   * @brief Check if parser is ready
   * @return true if initialized and ready
   */
  bool isReady() const { return uart_ != nullptr; }

  /**
   * @brief Clear message buffer (useful for recovery)
   */
  void clearBuffer();

private:
  UartJsonParser();

  // UART interface
  HardwareSerial *uart_ = nullptr;

  // Line buffer for input
  static constexpr size_t DEFAULT_BUFFER_SIZE = 512;
  uint8_t *rx_buffer_ = nullptr;
  size_t buffer_size_ = DEFAULT_BUFFER_SIZE;
  size_t buffer_pos_ = 0;

  // Message handlers
  static constexpr uint8_t MAX_HANDLERS = 16;
  MessageHandler handlers_[MAX_HANDLERS] = {};
  MessageType handler_types_[MAX_HANDLERS] = {};
  uint8_t handler_count_ = 0;

  // Error handling
  ErrorHandler error_handler_;
  char last_error_[128] = {0};

  // Statistics
  uint32_t message_count_ = 0;
  uint32_t error_count_ = 0;
  uint32_t line_number_ = 0;

  // Parsing state
  bool has_complete_line_ = false;
  size_t line_end_pos_ = 0;

  /**
   * @brief Process a complete line of JSON
   * @param json_line Null-terminated JSON string
   * @return true on successful parse
   */
  bool parseLine(const char *json_line);

  /**
   * @brief Parse JSON and dispatch to appropriate handler
   * @param json_doc_ptr Pointer to parsed JsonDocument
   */
  void dispatchMessage(const BaseMessage &msg);

  /**
   * @brief Report a parse error
   * @param error_msg Error description
   */
  void reportError(const char *error_msg);

  /**
   * @brief Find and register available handlers
   * @param type Message type detected
   * @return true if handler found and executed
   */
  bool callHandler(const BaseMessage &msg);
};

/**
 * @brief Helper function to send JSON response
 * Usage: sendJsonResponse(Serial, msg);
 */
template <typename T>
inline void sendJsonResponse(HardwareSerial &uart, const T &msg) {
  // Note: Full implementation in uart_json_parser.cpp
  // This is a template stub for type safety
  (void)uart;
  (void)msg;
}
