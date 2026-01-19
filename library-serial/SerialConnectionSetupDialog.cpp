/**
 * @file SerialConnectionSetupDialog.cpp
 * @author Miguel Ferrer
 * @brief  Dialog window for configuring the serial port connection.
 *
 * This file contains the implementation of the SerialConnectionSetupDialog class,
 * which provides a graphical interface for the user to:
 * 1. Scan and list available serial ports on the system.
 * 2. Select a specific port and Baud Rate.
 * 3. Initiate the connection via the SerialPortHandler.
 * It manages setting persistence, ensuring that the last used port and
 * baud rate are remembered between application sessions.
 *
 * @version 0.1
 * @date 2026-01-19
 *
 * @copyright Copyright (c) 2026
 *
 */
#include "SerialConnectionSetupDialog.h"
#include "SerialPortHandler.h"
#include "ui_SerialConnectionSetupDialog.h"
#include <QSettings>

/**
 * @brief Constructor.
 * Initializes the UI and loads the last known configuration from QSettings.
 *
 * It attempts to restore the 'serial/port' and 'serial/baudRate' keys
 * defined in the "InBiot/QualityTest" registry path.
 */
SerialConnectionSetupDialog::SerialConnectionSetupDialog(QWidget* parent) : QDialog(parent), ui(new Ui::SerialConnectionSetupDialog)
{
  ui->setupUi(this);
  this->setWindowTitle("Setup Serial Connection");

  QSettings settings("InBiot", "QualityTest");
  QString   lastPort     = settings.value("serial/port", "").toString();
  int       lastBaudRate = settings.value("serial/baudRate", 9600).toInt();

  refreshPorts();

  int portIndex = ui->comboBoxPort->findText(lastPort);
  if (portIndex != -1) {
    ui->comboBoxPort->setCurrentIndex(portIndex);
  }
  ui->comboBoxBaudRate->setCurrentText(QString::number(lastBaudRate));
}

/**
 * @brief Destructor.
 * Cleans up the UI resources.
 */
SerialConnectionSetupDialog::~SerialConnectionSetupDialog()
{
  delete ui;
}

/**
 * @brief Populates the port selection list.
 *
 * Iterates through `QSerialPortInfo::availablePorts()`.
 * It implements a smart-selection logic: if no previous port is saved,
 * it prioritizes ports with "UART" in their description to help the user.
 */
void SerialConnectionSetupDialog::refreshPorts()
{
  ui->comboBoxPort->clear();
  const auto ports       = QSerialPortInfo::availablePorts();
  int        serialIndex = -1;

  for (int i = 0; i < ports.size(); ++i) {
    const QSerialPortInfo& port            = ports[i];
    QString                portDescription = port.portName() + " - " + port.description();
    ui->comboBoxPort->addItem(portDescription);

    if (serialIndex == -1 && port.description().contains("UART", Qt::CaseInsensitive)) {
      serialIndex = i;
    }
  }

  if (serialIndex != -1) {
    ui->comboBoxPort->setCurrentIndex(serialIndex);
  }
}

/**
 * @brief Attempts to establish the connection.
 *
 * 1. Parses the port name from the combo box string (splitting at " - ").
 * 2. Calls the singleton `SerialPortHandler` to configure and connect.
 * 3. If successful, saves the configuration to `QSettings` for next time.
 * 4. If failed, emits `errorOccurred`.
 */
void SerialConnectionSetupDialog::on_pushButtonConnect_clicked()
{
  QString portName = ui->comboBoxPort->currentText().split(" - ")[0];
  qint32  baudRate = ui->comboBoxBaudRate->currentText().toInt();

  if (portName.isEmpty()) {
    emit errorOccurred("No port selected, please select a port.");
    return;
  }

  SerialPortHandler::instance().configurePort(portName, baudRate);
  if (SerialPortHandler::instance().connectSerial()) {
    QSettings settings("InBiot", "QualityTest");
    settings.setValue("serial/port", portName);
    settings.setValue("serial/baudRate", baudRate);
    accept();
  }
  else
    emit errorOccurred("Unable to open port " + portName + " with baud rate " + QString::number(baudRate));
  refreshPorts();
}
