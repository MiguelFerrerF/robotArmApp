/**
 * @file logger.h
 * @author Inbiot Monitoring Team
 * @brief Header file for the LogHandler class
 * @details This class provides static methods for logging messages with different log levels
 * and formatting options. It supports logging to a QTextEdit widget with customizable colors,
 * bold text, and font sizes. The log levels include INFO, SUCCESS, WARNING, ERROR, HIGHLIGHT, and DEBUG.
 * It also includes convenience methods for common log types.
 * @details The LogHandler class is designed to be used throughout the application for consistent logging
 * and to provide a clear and organized way to display messages in the user interface.
 * @version 0.1
 * @date 2025-06-24
 *
 * @copyright Copyright (c) 2025
 *
 */
#ifndef LOGHANDLER_H
#define LOGHANDLER_H

#include <QString>
#include <QTextEdit>

class LogHandler {
public:
  /**
   * @enum LogLevel
   * @brief Represents the severity level of a log message.
   *
   * The LogLevel enumeration defines various levels of logging severity
   * that can be used to categorize log messages within the application.
   *
   * - INFO:     Informational messages that highlight the progress of the application.
   * - SUCCESS:  Messages indicating successful completion of an operation.
   * - WARNING:  Messages that indicate a potential issue or unexpected situation.
   * - ERROR:    Messages that indicate a failure or error in the application.
   * - HIGHLIGHT:Messages that need to be emphasized or brought to attention.
   * - DEBUG:    Detailed messages intended for debugging purposes.
   */
  enum LogLevel
  {
    INFO,
    SUCCESS,
    WARNING,
    ERROR,
    HIGHLIGHT,
    DEBUG
  };

  /**
   * @defgroup LoggerMethods LogHandler Methods
   * @brief Methods for logging messages with specific colors and styles.
   * @details These methods provide convenience functions for logging messages with predefined styles.
   * They can be used to log messages with specific colors and styles without needing to specify the
   *  log level each time.
   * @{
   */
  /**
   * @brief Logs an informational message.
   * @param textEdit Pointer to the QTextEdit widget.
   * @param message The message to log.
   */
  static void info(QTextEdit* textEdit, const QString& message);
  /**
   * @brief  Logs a success message.
   * @param textEdit  Pointer to the QTextEdit widget.
   * @param message  The message to log.
   */
  static void success(QTextEdit* textEdit, const QString& message);
  /**
   * @brief Logs a warning message.
   * @param textEdit Pointer to the QTextEdit widget.
   * @param message The message to log.
   */
  static void warning(QTextEdit* textEdit, const QString& message);
  /**
   * @brief Logs an error message.
   * @param textEdit  Pointer to the QTextEdit widget.
   * @param message  The message to log.
   */
  static void error(QTextEdit* textEdit, const QString& message);
  /**
   * @brief Logs a highlighted message.
   * @param textEdit Pointer to the QTextEdit widget.
   * @param message The message to log.
   */
  static void highlight(QTextEdit* textEdit, const QString& message);
  /**
   * @brief Logs a debug message.
   * @param textEdit Pointer to the QTextEdit widget.
   * @param message The message to log.
   */
  static void debug(QTextEdit* textEdit, const QString& message);
  /**
   * @brief Logs a message with a custom color, bold text, and font size.
   * @param textEdit Pointer to the QTextEdit widget.
   * @param message The message to log.
   * @param color The color of the text (e.g., "red", "green", "blue").
   * @param bold Whether the text should be bold.
   * @param fontSize The font size of the text (e.g., "12px", "14px").
   */
  static void custom(QTextEdit* textEdit, const QString& message, const QString& color, bool bold = false, const QString& fontSize = "12px");
  /**
   * @}
   */

private:
  static QString formatMessage(const QString& message, const QString& color, bool bold, const QString& fontSize);

  static void log(QTextEdit* textEdit, const QString& message, LogLevel level = INFO);
  static void log(QTextEdit* textEdit, const QString& message, const QString& color, bool bold = false, const QString& fontSize = "");
};

#endif // LOGHANDLER_H
