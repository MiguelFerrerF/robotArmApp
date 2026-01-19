/**
 * @file SerialPortHandler.cpp
 * @author Miguel Ferrer
 * @brief  Singleton class that manages serial port communication.
 *
 * This file implements the SerialPortHandler class, which wraps around
 * QSerialPort to provide a singleton interface for serial communication.
 * It enforces a line-based protocol and emits signals for data reception
 * and transmission events.
 *
 * @version 0.1
 * @date 2026-01-19
 *
 * @copyright Copyright (c) 2026
 *
 */
#include "SerialPortHandler.h"
#include <QDebug>

SerialPortHandler& SerialPortHandler::instance()
{
  static SerialPortHandler instance;
  return instance;
}

SerialPortHandler::SerialPortHandler(QObject* parent) : QObject(parent)
{
  connect(&m_serial, &QSerialPort::readyRead, this, &SerialPortHandler::handleReadyRead);
  connect(&m_serial, &QSerialPort::errorOccurred, this, &SerialPortHandler::handleError);
}

/**
 * @brief Configures the physical layer properties.
 *
 * Enforces the "8N1" standard (8 Data bits, No Parity, 1 Stop bit) which is
 * the most common configuration for Arduino/Microcontroller UARTs.
 */
void SerialPortHandler::configurePort(const QString& portName, qint32 baudRate)
{
  if (portName.isEmpty()) {
    emit errorOccurred("Port name cannot be empty.");
    return;
  }
  if (baudRate <= 0) {
    emit errorOccurred("Invalid baud rate specified.");
    return;
  }
  m_serial.setPortName(portName);
  m_serial.setBaudRate(baudRate);
  m_serial.setDataBits(QSerialPort::Data8);
  m_serial.setParity(QSerialPort::NoParity);
  m_serial.setStopBits(QSerialPort::OneStop);
  m_serial.setFlowControl(QSerialPort::NoFlowControl);
}

/**
 * @brief Opens the serial port for communication.
 *
 * Validates that the port name and baud rate have been set before attempting
 * to open the port. Emits errorOccurred signal on failure.
 *
 * @return true if the port was successfully opened or was already open; false on error.
 */
bool SerialPortHandler::connectSerial()
{
  if (m_serial.isOpen()) {
    return true;
  }

  QString portName = getPortName();
  if (portName.isEmpty()) {
    emit errorOccurred("No serial port selected.");
    return false;
  }

  if (m_serial.baudRate() == 0) {
    emit errorOccurred("Baud rate is not set.");
    return false;
  }

  if (m_serial.open(QIODevice::ReadWrite)) {
    emit connectionStatusChanged(true);
    return true;
  }
  else {
    emit errorOccurred(m_serial.errorString());
    return false;
  }
}

/**
 * @brief Closes the serial port connection.
 * @return true if the port was closed; false if it was not open.
 */
bool SerialPortHandler::disconnectSerial()
{
  if (m_serial.isOpen()) {
    m_serial.close();
    emit connectionStatusChanged(false);
    return true;
  }
  return false;
}

/**
 * @brief Queues data for transmission.
 * @note Splits data by newline characters and sends them sequentially.
 * @param[in] data The bytes to send.
 */
void SerialPortHandler::sendData(const QByteArray& data)
{
  if (m_serial.isOpen()) {
    QList<QByteArray> lines = data.split('\n'); // Dividir los datos en líneas
    for (const QByteArray& line : lines) {
      m_serial.write(line + '\n'); // Enviar cada línea seguida de un salto de línea
      emit dataSent(line);         // Emitir señal para cada línea enviada
    }
  }
}

/**
 * @brief Checks if the serial port is currently connected.
 * @return true if the port is open; false otherwise.
 */
bool SerialPortHandler::isConnected() const
{
  return m_serial.isOpen();
}

/**
 * @brief Retrieves the configured port name.
 * @return The system name of the serial port (e.g., "COM3", "/dev/ttyUSB0").
 */
QString SerialPortHandler::getPortName() const
{
  return m_serial.portName();
}

/**
 * @brief Retrieves the configured baud rate.
 * @return The communication speed (e.g., 9600, 115200).
 */
int SerialPortHandler::getBaudRate() const
{
  return m_serial.baudRate();
}

/**
 * @brief Internal slot called by the OS interrupt/event loop when bytes arrive.
 */
void SerialPortHandler::handleReadyRead()
{
  while (m_serial.canReadLine()) {
    QByteArray line = m_serial.readLine(); // Leer línea por línea
    emit       dataReceived(line);         // Emitir señal para cada línea recibida
  }
}

/**
 * @brief Internal slot called when a serial port error occurs.
 * @param error The specific error that occurred.
 */
void SerialPortHandler::handleError(QSerialPort::SerialPortError error)
{
  if (error != QSerialPort::NoError) {
    emit errorOccurred(m_serial.errorString());
  }
}
