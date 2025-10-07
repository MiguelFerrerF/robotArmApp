// serialmanager.cpp
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

bool SerialPortHandler::disconnectSerial()
{
  if (m_serial.isOpen()) {
    m_serial.close();
    emit connectionStatusChanged(false);
    return true;
  }
  return false;
}

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

bool SerialPortHandler::isConnected() const
{
  return m_serial.isOpen();
}

QString SerialPortHandler::getPortName() const
{
  return m_serial.portName();
}

int SerialPortHandler::getBaudRate() const
{
  return m_serial.baudRate();
}

void SerialPortHandler::handleReadyRead()
{
  while (m_serial.canReadLine()) {
    QByteArray line = m_serial.readLine(); // Leer línea por línea
    emit       dataReceived(line);         // Emitir señal para cada línea recibida
  }
}

void SerialPortHandler::handleError(QSerialPort::SerialPortError error)
{
  if (error != QSerialPort::NoError) {
    emit errorOccurred(m_serial.errorString());
  }
}
