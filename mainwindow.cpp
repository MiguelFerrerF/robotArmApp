#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include "library-serial/SerialConnectionSetupDialog.h"
#include <QDebug>
#include <QVideoFrameFormat>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow) {
  ui->setupUi(this);
  this->setWindowTitle("Robot Arm Controller");

  setupConnections();
}

MainWindow::~MainWindow() {
  VideoCameraHandler::instance().stopCamera();
  delete ui;
}

void MainWindow::setupConnections() {
  SerialPortHandler &serial = SerialPortHandler::instance();
  connect(&serial, &SerialPortHandler::errorOccurred, this,
          &MainWindow::onSerialError);
  connect(&serial, &SerialPortHandler::connectionStatusChanged, this,
          &MainWindow::onSerialStatusChanged);
}

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
  VideoConnectionDialog dialog(this);
  connect(&dialog, &VideoConnectionDialog::errorOccurred, this,
          [this](const QString &err) {
            LogHandler::error(ui->textEditLog, "Video Error: " + err);
          });

  if (dialog.exec() != QDialog::Accepted)
    return;

  VideoCameraHandler &camera = VideoCameraHandler::instance();
  disconnect(&camera, &VideoCameraHandler::frameCaptured, this,
             &MainWindow::onVideoCapture);
  // Connect signal from VideoCameraHandler
  connect(&camera, &VideoCameraHandler::frameCaptured, this,
          &MainWindow::onVideoCapture);

  LogHandler::success(ui->textEditLog, "Camera connected successfully");
}

void MainWindow::on_actionDisconnectVideo_triggered() {
  VideoCameraHandler::instance().stopCamera();
  ui->labelCamera->clear();
  LogHandler::warning(ui->textEditLog, "Video camera disconnected");
}

void MainWindow::on_actionControl_triggered() {
  qDebug("Control robot pulsado");
  if (!m_RobotControl) {
    m_RobotControl = new RobotControlDialog(this, &m_robotSettings);
  }
  m_RobotControl->show();
  m_RobotControl->raise();
  m_RobotControl->activateWindow();

  // Disconnect previous connections if any to avoid duplicates
  disconnect(m_RobotControl, &RobotControlDialog::errorOccurred, this,
             &MainWindow::onRobotControlError);
  disconnect(m_RobotControl, &RobotControlDialog::motorAngleChanged, this,
             &MainWindow::onRobotMotorAngleChanged);
  disconnect(m_RobotControl, &RobotControlDialog::allMotorsReset, this,
             &MainWindow::onAllMotorsReset);

  // Connect signals from RobotControlDialog
  connect(m_RobotControl, &RobotControlDialog::errorOccurred, this,
          &MainWindow::onRobotControlError);
  connect(m_RobotControl, &RobotControlDialog::motorAngleChanged, this,
          &MainWindow::onRobotMotorAngleChanged);
  connect(m_RobotControl, &RobotControlDialog::allMotorsReset, this,
          &MainWindow::onAllMotorsReset);
}

void MainWindow::on_actionCalibrateRobot_triggered() {
  LogHandler::info(ui->textEditLog, "Robot calibration started");
  int calibratedAngles[] = {90, 90, 98, 90, 142, 82};
  for (int i = 0; i < 6; ++i) {
    if (SerialPortHandler::instance().isConnected()) {
      QString command =
          QString("SETUP:SERVO%1:%2").arg(i + 1).arg(calibratedAngles[i]);
      SerialPortHandler::instance().sendData(command.toUtf8());
      LogHandler::info(ui->textEditLog,
                       QString("Sent calibration to motor %1: %2")
                           .arg(i + 1)
                           .arg(command.trimmed()));
    } else {
      LogHandler::warning(ui->textEditLog,
                          "Cannot send calibration: Serial port not connected");
      break;
    }
  }
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

void MainWindow::onRobotControlError(const QString &error) {
  LogHandler::error(ui->textEditLog, "Robot Control Error: " + error);
}

void MainWindow::onRobotMotorAngleChanged(int motorIndex, int angle) {
  // send command to robot via serial
  if (SerialPortHandler::instance().isConnected()) {
    QString command = QString("SETUP:SERVO%1:%2").arg(motorIndex).arg(angle);
    SerialPortHandler::instance().sendData(command.toUtf8());
    LogHandler::info(ui->textEditLog, QString("Sent command to motor %1: %2")
                                          .arg(motorIndex)
                                          .arg(command.trimmed()));
  } else {
    LogHandler::warning(ui->textEditLog,
                        "Cannot send command: Serial port not connected");
  }
}

void MainWindow::onAllMotorsReset() {
  LogHandler::info(ui->textEditLog, "All motors have been reset to default");
}

void MainWindow::onVideoCapture(const QImage &image) {
  if (image.isNull())
    return;

  // Scale the image to fit the label while maintaining aspect ratio
  QPixmap pixmap = QPixmap::fromImage(image).scaledToWidth(
      ui->labelCamera->width(), Qt::SmoothTransformation);
  ui->labelCamera->setPixmap(pixmap);
}
