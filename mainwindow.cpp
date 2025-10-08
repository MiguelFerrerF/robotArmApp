#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include "library-serial/SerialConnectionSetupDialog.h"
#include <QDebug>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow) {
  ui->setupUi(this);
  this->setWindowTitle("Robot Arm Controller");
}

MainWindow::~MainWindow() { delete ui; }

void MainWindow::on_actionSerial_triggered() {
  if (!m_SerialMonitorDialog) {
    m_SerialMonitorDialog = new SerialMonitorDialog(this);
    connect(m_SerialMonitorDialog, &SerialMonitorDialog::warningOccurred, this,
            &MainWindow::onSerialMonitorWarning);
  }
  m_SerialMonitorDialog->show();
  m_SerialMonitorDialog->raise();
  m_SerialMonitorDialog->activateWindow();
}

void MainWindow::on_actionConnectSerial_triggered() {
  if (SerialPortHandler::instance().isConnected()) {
    LogHandler::info(ui->textEditLog, "Serial port already connected");
    return;
  }

  SerialConnectionSetupDialog dialog(this);
  connect(&dialog, &SerialConnectionSetupDialog::errorOccurred, this,
          &MainWindow::onSetupConnectionError);

  if (dialog.exec() == QDialog::Accepted)
    LogHandler::success(ui->textEditLog,
                        "Serial connection configured successfully");
}

void MainWindow::on_actionDisconnectSerial_triggered() {
  if (SerialPortHandler::instance().isConnected()) {
    SerialPortHandler::instance().disconnectSerial();
    LogHandler::warning(ui->textEditLog, "Disconnected from serial port");
  } else
    LogHandler::warning(ui->textEditLog, "No serial port connected");
}

void MainWindow::on_actionConnectVideo_triggered() {
  qDebug("Conectar video en curso");
}

void MainWindow::on_actionControl_triggered() {
  qDebug("Control robot pulsado");
  if (!m_RobotControl) {
    m_RobotControl = new RobotControlDialog(this);
  }
  m_RobotControl->show();
  m_RobotControl->raise();
  m_RobotControl->activateWindow();
}

void MainWindow::onSerialError(const QString &error) {
  LogHandler::error(ui->textEditLog, "Serial Error: " + error);
}

void MainWindow::onSerialStatusChanged(bool connected) {
  LogHandler::info(
      ui->textEditLog,
      QString("Serial port %1").arg(connected ? "connected" : "disconnected"));
}

void MainWindow::onSetupConnectionError(const QString &error) {
  LogHandler::error(ui->textEditLog, error);
}

void MainWindow::onSerialMonitorWarning(const QString &warning) {
  LogHandler::warning(ui->textEditLog, warning);
}
