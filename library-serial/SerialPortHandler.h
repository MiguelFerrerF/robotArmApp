#ifndef SERIALPORTHANDLER_H
#define SERIALPORTHANDLER_H

#include <QObject>
#include <QSerialPort>
#include <QSerialPortInfo>

/**
 * @brief Singleton wrapper around QSerialPort to manage hardware communication.
 *
 * This class provides a centralized point of access for the serial port resource.
 * It ensures that multiple components (e.g., RobotHandler, SerialMonitor) use
 * the same connection instance.
 *
 * @note This handler enforces a **Line-Based Protocol**. It reads data using `readLine()`
 * and expects messages to be terminated by a newline character.
 */
class SerialPortHandler : public QObject
{
  Q_OBJECT
public:
  /**
   * @brief Access the singleton instance of the handler.
   * @return Reference to the static SerialPortHandler.
   */
  static SerialPortHandler& instance();

  /**
   * @brief Sets the connection parameters.
   *
   * Configures the internal QSerialPort with the specified name and baud rate.
   * Other parameters are set to defaults: Data8, NoParity, OneStop, NoFlowControl (8N1).
   *
   * @param[in] portName The system name of the port (e.g., "COM3", "/dev/ttyUSB0").
   * @param[in] baudRate The communication speed (e.g., 9600, 115200).
   */
  void configurePort(const QString& portName, qint32 baudRate);

  /**
   * @brief Attempts to open the serial port with the current configuration.
   * @return true if the port was successfully opened or was already open; false on error.
   */
  bool connectSerial();

  /**
   * @brief Closes the serial port connection.
   * @return true if the port was closed; false if it was not open.
   */
  bool disconnectSerial();

  /**
   * @brief Queues data for transmission.
   * @note Splits data by newline characters and sends them sequentially.
   * @param[in] data The bytes to send.
   */
  void sendData(const QByteArray& data);

  bool    isConnected() const;
  QString getPortName() const;
  int     getBaudRate() const;

signals:
  /**
   * @brief Emitted when a complete line of text is received (terminated by \\n).
   * @param data The line of bytes received.
   */
  void dataReceived(const QByteArray& data);

  /**
   * @brief Emitted immediately after data is written to the hardware buffer.
   * @param data The specific chunk of bytes sent.
   */
  void dataSent(const QByteArray& data);

  /**
   * @brief Notification of connection state changes.
   * @param connected true if open, false if closed.
   */
  void connectionStatusChanged(bool connected);

  void errorOccurred(const QString& error);

private:
  explicit SerialPortHandler(QObject* parent = nullptr);
  QSerialPort m_serial;

private slots:
  /**
   * @brief Internal slot called by the OS interrupt/event loop when bytes arrive.
   */
  void handleReadyRead();
  void handleError(QSerialPort::SerialPortError error);
};

#endif // SERIALPORTHANDLER_H
