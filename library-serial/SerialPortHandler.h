#ifndef SERIALPORTHANDLER_H
#define SERIALPORTHANDLER_H

#include <QObject>
#include <QSerialPort>
#include <QSerialPortInfo>

class SerialPortHandler : public QObject {
  Q_OBJECT
public:
  static SerialPortHandler& instance();

  void    configurePort(const QString& portName, qint32 baudRate);
  bool    connectSerial();
  bool    disconnectSerial();
  void    sendData(const QByteArray& data);
  bool    isConnected() const;
  QString getPortName() const;
  int     getBaudRate() const;

signals:
  void dataReceived(const QByteArray& data); 
  void dataSent(const QByteArray& data);
  void connectionStatusChanged(bool connected);
  void errorOccurred(const QString& error);

private:
  explicit SerialPortHandler(QObject* parent = nullptr);
  QSerialPort m_serial;

private slots:
  void handleReadyRead();
  void handleError(QSerialPort::SerialPortError error);
};

#endif // SERIALPORTHANDLER_H
