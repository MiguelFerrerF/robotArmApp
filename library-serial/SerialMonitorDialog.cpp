// serialmonitor.cpp
#include "SerialMonitorDialog.h"
#include "../library-log/LogHandler.h"
#include "ui_SerialMonitorDialog.h"

SerialMonitorDialog::SerialMonitorDialog(QWidget *parent)
    : QDialog(parent), ui(new Ui::SerialMonitorDialog) {
  ui->setupUi(this);
  ui->textEditSerial->setReadOnly(true);

  // Conectar señales del SerialPortHandler
  SerialPortHandler &serial = SerialPortHandler::instance();
  connect(&serial, &SerialPortHandler::dataReceived, this,
          &SerialMonitorDialog::onDataReceived);
  connect(&serial, &SerialPortHandler::dataSent, this,
          &SerialMonitorDialog::onDataSent);

  m_serialConnected = serial.isConnected();
}

SerialMonitorDialog::~SerialMonitorDialog() { delete ui; }

void SerialMonitorDialog::onDataReceived(const QByteArray &data) {
  if (m_serialConnected) {
    QString text = QString::fromUtf8(data);
    LogHandler::info(ui->textEditSerial, text);
    return;
  }
  m_serialConnected = SerialPortHandler::instance().isConnected();
}

void SerialMonitorDialog::onDataSent(const QByteArray &data) {
  if (m_serialConnected) {
    QString text = QString::fromUtf8(data);
    LogHandler::highlight(ui->textEditSerial, text);
  }
}

void SerialMonitorDialog::onCloseEvent(QCloseEvent *event) {
  QDialog::closeEvent(event);
}

void SerialMonitorDialog::on_sendSerialButton_clicked() {
  if (!m_serialConnected) {
    emit warningOccurred("Serial port is not connected.");
    return;
  }

  QString text = ui->lineEditSend->text();
  if (text.isEmpty()) {
    emit warningOccurred("Cannot send empty data.");
    return;
  }

  QByteArray data = text.toUtf8();
  SerialPortHandler::instance().sendData(data);
  ui->lineEditSend->clear();
}
