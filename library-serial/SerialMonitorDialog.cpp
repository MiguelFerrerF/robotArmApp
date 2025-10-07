// serialmonitor.cpp
#include "SerialMonitorDialog.h"
#include "../library-log/LogHandler.h"
#include "SerialPortHandler.h"
#include "ui_SerialMonitorDialog.h"

SerialMonitorDialog::SerialMonitorDialog(QWidget* parent) : QDialog(parent), ui(new Ui::SerialMonitorDialog)
{
  ui->setupUi(this);
  ui->textEditSerial->setReadOnly(true);

  // Conectar señales del SerialPortHandler
  SerialPortHandler& serial = SerialPortHandler::instance();
  connect(&serial, &SerialPortHandler::dataReceived, this, &SerialMonitorDialog::onDataReceived);
  connect(&serial, &SerialPortHandler::dataSent, this, &SerialMonitorDialog::onDataSent);
  connect(&serial, &SerialPortHandler::connectionStatusChanged, this, [this](bool connected) { updateButtonState(connected ? Stop : Start); });

  // Configurar el estado inicial del botón
  m_serialConnected = serial.isConnected();
  updateButtonState(serial.isConnected() ? Stop : Start);
}

SerialMonitorDialog::~SerialMonitorDialog()
{
  delete ui;
}

void SerialMonitorDialog::onDataReceived(const QByteArray& data)
{
  if (m_serialConnected && serialStarted) {
    QString text = QString::fromUtf8(data);
    LogHandler::info(ui->textEditSerial, text);
    return;
  }
  m_serialConnected = SerialPortHandler::instance().isConnected();
}

void SerialMonitorDialog::onDataSent(const QByteArray& data)
{
  if (m_serialConnected) {
    QString text = QString::fromUtf8(data);
    LogHandler::highlight(ui->textEditSerial, text);
  }
}

void SerialMonitorDialog::onStopSerialButtonClicked(bool checked)
{
  if (checked) {
    updateButtonState(Start);
    m_serialConnected = false;
    emit warningOccurred("Serial view stopped");
  }
  else {
    updateButtonState(Stop);
    m_serialConnected = true;
    emit warningOccurred("Serial view started");
  }
}

void SerialMonitorDialog::onCloseEvent(QCloseEvent* event)
{
  QDialog::closeEvent(event);
}

void SerialMonitorDialog::updateButtonState(ButtonState state)
{
  if (state == Start) {
    ui->stopSerialButton->setText("Start");
    ui->stopSerialButton->setStyleSheet("background-color: green");
    serialStarted = false;
  }
  else if (state == Stop) {
    ui->stopSerialButton->setText("Stop");
    ui->stopSerialButton->setStyleSheet("background-color: red");
    serialStarted = true;
  }
}
