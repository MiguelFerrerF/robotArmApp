/**
 * @file logger.cpp
 * @author  Inbiot Monitoring Team
 * @brief    Implementation file for the LogHandler class
 * @details  This file implements the LogHandler class, which provides static methods for logging messages
 * to a QTextEdit widget with various log levels and formatting options.
 * @details  The LogHandler class supports different log levels such as INFO, SUCCESS, WARNING,
 * ERROR, HIGHLIGHT, and DEBUG. It allows for custom colors, bold text, and font sizes
 * for more specific formatting of log messages.
 * @details  The log messages are formatted with a timestamp and can be appended to the QTextEdit widget
 * in a user-friendly manner. The class is designed to be used throughout the application
 * for consistent logging and to provide a clear and organized way to display messages in the user
 * interface.
 * @details  The LogHandler class is static, meaning its methods can be called without creating an
 * instance of the class.
 * @version 0.1
 * @date 2025-06-24
 *
 * @copyright Copyright (c) 2025
 */
#include "LogHandler.h"
#include <QDateTime>

/**
 * @brief  Formats a log message with a timestamp, color, bold text, and font size.
 * @param message The log message to be formatted.
 * @param color The color of the text (e.g., "red", "green", "blue").
 * @param bold Whether the text should be bold.
 * @param fontSize The font size of the text (e.g., "12px", "14px"). If empty, default size is used.
 * @return QString
 * @details This method creates a formatted string that includes the current timestamp,
 * a specified color, whether the text should be bold, and an optional font size.
 * * The formatted message is suitable for displaying in a QTextEdit widget with HTML styling.
 * * The timestamp is formatted as "[hh:mm:ss] ", and the message is wrapped in a <font> tag
 * with the specified styles.
 */
QString LogHandler::formatMessage(const QString& message, const QString& color, bool bold, const QString& fontSize)
{
  QString timestamp = QDateTime::currentDateTime().toString("[hh:mm:ss] ");
  QString style;

  if (!color.isEmpty())
    style += QString("color:%1;").arg(color);
  if (bold)
    style += "font-weight:bold;";
  if (!fontSize.isEmpty())
    style += QString("font-size:%1;").arg(fontSize);

  if (style.isEmpty()) {
    return timestamp + message;
  }

  return QString("%1<font style='%2'>%3</font>").arg(timestamp, style, message);
}

/**
 * @brief  Logs a message to a QTextEdit widget with a specific log level.
 * @param textEdit  Pointer to the QTextEdit widget where the message will be logged.
 * @param message  The message to log.
 * @param level   The log level that determines the color and style of the message.
 * @details This method appends a formatted log message to the specified QTextEdit widget.
 * The message is styled based on the log level, which determines the color and boldness of the text.
 * @details  The log level can be one of the following:
 *  - SUCCESS:    Green text for successful operations.
 *  - WARNING:    Yellow text for warnings, displayed in bold.
 *  - ERROR:      Light red text for errors, displayed in bold.
 *  - HIGHLIGHT:  Light blue text for highlighted messages.
 *  - DEBUG:      Light gray text for debug messages.
 *  - INFO:       White text for informational messages (default level).
 * @note If the textEdit pointer is null, the method does nothing.
 */
void LogHandler::log(QTextEdit* textEdit, const QString& message, LogLevel level)
{
  if (!textEdit)
    return;

  QString color;
  bool    bold = false;

  switch (level) {
    case SUCCESS:
      color = "#4CAF50"; // Green
      break;
    case WARNING:
      color = "#FFD600"; // yellow
      bold  = true;
      break;
    case ERROR:
      color = "#FF5252"; // light red
      bold  = true;
      break;
    case HIGHLIGHT:
      color = "#40C4FF"; // light blue
      break;
    case DEBUG:
      color = "#8b8b8bff"; // light gray
      break;
    case INFO:
    default:
      color = "#b6b6b6ff"; // white
      break;
  }

  textEdit->append(formatMessage(message, color, bold, ""));
}

/**
 * @brief   Logs a message to a QTextEdit widget with a custom color, bold text, and font size.
 *
 * @param textEdit  Pointer to the QTextEdit widget where the message will be logged.
 * @param message   The message to log.
 * @param color     The color of the text (e.g., "red", "green", "blue").
 * @param bold      Whether the text should be bold.
 * @param fontSize  The font size of the text (e.g., "12px", "14px"). If empty, default size is used.
 */
void LogHandler::log(QTextEdit* textEdit, const QString& message, const QString& color, bool bold, const QString& fontSize)
{
  if (!textEdit)
    return;
  textEdit->append(formatMessage(message, color, bold, fontSize));
}

void LogHandler::info(QTextEdit* textEdit, const QString& message)
{
  log(textEdit, message, INFO);
}

void LogHandler::success(QTextEdit* textEdit, const QString& message)
{
  log(textEdit, message, SUCCESS);
}

void LogHandler::warning(QTextEdit* textEdit, const QString& message)
{
  log(textEdit, message, WARNING);
}

void LogHandler::error(QTextEdit* textEdit, const QString& message)
{
  log(textEdit, message, ERROR);
}

void LogHandler::highlight(QTextEdit* textEdit, const QString& message)
{
  log(textEdit, message, HIGHLIGHT);
}

void LogHandler::debug(QTextEdit* textEdit, const QString& message)
{
  log(textEdit, message, DEBUG);
}

void LogHandler::custom(QTextEdit* textEdit, const QString& message, const QString& color, bool bold, const QString& fontSize)
{
  log(textEdit, message, color, bold, fontSize);
}
