// serialmonitor.cpp
#include "SerialMonitorDialog.h"
#include "../library-log/LogHandler.h"
#include "ui_SerialMonitorDialog.h"

/**
 * @brief Constructor.
 *
 * Initializes the UI and establishes signal/slot connections with the
 * SerialPortHandler singleton. It enables window minimize/maximize flags
 * for better usability during debugging sessions.
 */
SerialMonitorDialog::SerialMonitorDialog(QWidget* parent) : QDialog(parent), ui(new Ui::SerialMonitorDialog)
{
  ui->setupUi(this);
  ui->textEditSerial->setReadOnly(true);

  this->setWindowFlags(this->windowFlags() | Qt::WindowMinimizeButtonHint | Qt::WindowMaximizeButtonHint);

  SerialPortHandler& serial = SerialPortHandler::instance();
  connect(&serial, &SerialPortHandler::dataReceived, this, &SerialMonitorDialog::onDataReceived);
  connect(&serial, &SerialPortHandler::dataSent, this, &SerialMonitorDialog::onDataSent);

  m_serialConnected = serial.isConnected();
}

/**
 * @brief Destructor.
 *
 * Cleans up the UI resources.
 */
SerialMonitorDialog::~SerialMonitorDialog()
{
  delete ui;
}

/**
 * @brief Displays received data in the console.
 *
 * Converts the raw bytes to a UTF-8 string and appends it to the log view.
 * It uses `LogHandler::info` to format the text (typically standard color).
 *
 * @note This method also lazily updates the local `m_serialConnected` state.
 */
void SerialMonitorDialog::onDataReceived(const QByteArray& data)
{
  if (m_serialConnected) {
    QString text = QString::fromUtf8(data);
    LogHandler::info(ui->textEditSerial, text);
    return;
  }
  m_serialConnected = SerialPortHandler::instance().isConnected();
}

/**
 * @brief Displays sent data in the console.
 *
 * Uses `LogHandler::highlight` to visually distinguish outgoing commands
 * (e.g., using a different color or bold text) from incoming data.
 */
void SerialMonitorDialog::onDataSent(const QByteArray& data)
{
  if (m_serialConnected) {
    QString text = QString::fromUtf8(data);
    LogHandler::highlight(ui->textEditSerial, text);
  }
}

/**
 * @brief Handles the dialog close event.
 *
 * Currently, it just calls the base class implementation.
 */
void SerialMonitorDialog::onCloseEvent(QCloseEvent* event)
{
  QDialog::closeEvent(event);
}

/**
 * @brief Manually sends a command to the device.
 *
 * 1. Checks if the serial port is connected.
 * 2. Validates that the input is not empty.
 * 3. Converts the string to UTF-8 bytes.
 * 4. Calls `SerialPortHandler::sendData`.
 * 5. Clears the input field for the next command.
 */
void SerialMonitorDialog::on_sendSerialButton_clicked()
{
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
