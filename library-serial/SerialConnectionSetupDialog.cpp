// setupconnection.cpp
#include "SerialConnectionSetupDialog.h"
#include "SerialPortHandler.h"
#include "ui_SerialConnectionSetupDialog.h"
#include <QSettings>

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

SerialConnectionSetupDialog::~SerialConnectionSetupDialog()
{
  delete ui;
}

void SerialConnectionSetupDialog::refreshPorts()
{
  ui->comboBoxPort->clear();
  const auto ports       = QSerialPortInfo::availablePorts();
  int        serialIndex = -1;

  for (int i = 0; i < ports.size(); ++i) {
    const QSerialPortInfo& port            = ports[i];
    QString                portDescription = port.portName() + " - " + port.description();
    ui->comboBoxPort->addItem(portDescription);

    // Seleccionar el primer puerto que contenga "UART" en la descripción
    if (serialIndex == -1 && port.description().contains("UART", Qt::CaseInsensitive)) {
      serialIndex = i;
    }
  }

  // Si se encontró un puerto con "SERIAL", seleccionarlo
  if (serialIndex != -1) {
    ui->comboBoxPort->setCurrentIndex(serialIndex);
  }
}

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
