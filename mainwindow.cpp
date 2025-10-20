#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include "library-serial/SerialConnectionSetupDialog.h"
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QProcess>
#include <QSettings>
#include <QVideoFrameFormat>
#include <QLineEdit>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow), m_RobotHandler(new RobotHandler(this)) {
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
  connect(&serial, &SerialPortHandler::dataReceived, this,
          &MainWindow::onDataReceived);
  connect(m_RobotHandler, &RobotHandler::motorAngleChanged, this,
      &MainWindow::onRobotMotorAngleUpdatedFromSerial);
  connect(m_RobotHandler, &RobotHandler::errorOccurred, this,
      &MainWindow::onSerialError);
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

void MainWindow::on_actionLog_triggered() {
  // Ensure the logs directory exists
  QString logsDirPath = QDir::currentPath() + "/logs";
  QDir logsDir(logsDirPath);
  if (!logsDir.exists()) {
    if (!logsDir.mkpath(".")) {
      LogHandler::error(ui->textEditLog, "Failed to create logs directory");
      return;
    }
  }

  // Save log to a file (date-time stamped) in the logs directory
  QString logFileName =
      logsDirPath +
      QString("/log_%1.txt")
          .arg(QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss"));
  QFile logFile(logFileName);
  if (logFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
    QTextStream out(&logFile);
    out << ui->textEditLog->toPlainText();
    logFile.close();
    LogHandler::success(ui->textEditLog, "Log saved to " + logFileName);
    QProcess::startDetached(
        "explorer.exe", {"/select,", QDir::toNativeSeparators(logFileName)});
  } else {
    LogHandler::error(ui->textEditLog, "Failed to save log to file");
  }
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

  disconnectVideoSignals();
  connectVideoSignals();

  if (dialog.exec() != QDialog::Accepted)
    return;
}

void MainWindow::disconnectVideoSignals() {
  VideoCameraHandler &camera = VideoCameraHandler::instance();
  disconnect(&camera, &VideoCameraHandler::frameCaptured, this,
             &MainWindow::onVideoCapture);
  disconnect(&camera, &VideoCameraHandler::cameraStarted, this,
             &MainWindow::onCameraStarted);
  disconnect(&camera, &VideoCameraHandler::cameraStopped, this,
             &MainWindow::onCameraStopped);
  disconnect(&camera, &VideoCameraHandler::errorOccurred, this,
             &MainWindow::onCameraError);
  disconnect(&camera, &VideoCameraHandler::focusModeChanged, this,
             &MainWindow::onFocusModeChanged);
  disconnect(&camera, &VideoCameraHandler::zoomFactorChanged, this,
             &MainWindow::onZoomFactorChanged);
  disconnect(&camera, &VideoCameraHandler::exposureModeChanged, this,
             &MainWindow::onExposureModeChanged);
  disconnect(&camera, &VideoCameraHandler::whiteBalanceModeChanged, this,
             &MainWindow::onWhiteBalanceModeChanged);
  disconnect(&camera, &VideoCameraHandler::colorTemperatureChanged, this,
             &MainWindow::onColorTemperatureChanged);
}

void MainWindow::connectVideoSignals() {
  VideoCameraHandler &camera = VideoCameraHandler::instance();
  connect(&camera, &VideoCameraHandler::frameCaptured, this,
          &MainWindow::onVideoCapture);
  connect(&camera, &VideoCameraHandler::cameraStarted, this,
          &MainWindow::onCameraStarted);
  connect(&camera, &VideoCameraHandler::cameraStopped, this,
          &MainWindow::onCameraStopped);
  connect(&camera, &VideoCameraHandler::errorOccurred, this,
          &MainWindow::onCameraError);
  connect(&camera, &VideoCameraHandler::focusModeChanged, this,
          &MainWindow::onFocusModeChanged);
  connect(&camera, &VideoCameraHandler::zoomFactorChanged, this,
          &MainWindow::onZoomFactorChanged);
  connect(&camera, &VideoCameraHandler::exposureModeChanged, this,
          &MainWindow::onExposureModeChanged);
  connect(&camera, &VideoCameraHandler::whiteBalanceModeChanged, this,
          &MainWindow::onWhiteBalanceModeChanged);
  connect(&camera, &VideoCameraHandler::colorTemperatureChanged, this,
          &MainWindow::onColorTemperatureChanged);
}

void MainWindow::on_actionDisconnectVideo_triggered() {
  VideoCameraHandler::instance().stopCamera();
  ui->labelCamera->clear();
  LogHandler::warning(ui->textEditLog, "Video camera disconnected");
}

void MainWindow::on_actionSettings_triggered() {
  m_VideoSettingsDialog = new VideoSettingsDialog(this);
  m_VideoSettingsDialog->show();
  m_VideoSettingsDialog->raise();
  m_VideoSettingsDialog->activateWindow();
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

void MainWindow::onSerialStatusChanged(bool connected) 
{LogHandler::info(
      ui->textEditLog,
      QString("Serial port %1").arg(connected ? "connected" : "disconnected"));
}

void MainWindow::onSetupConnectionError(const QString &error) {
  LogHandler::error(ui->textEditLog, error);
}

void MainWindow::onSerialMonitorWarning(const QString &warning) {
  LogHandler::warning(ui->textEditLog, warning);
}

void MainWindow::onDataReceived(const QByteArray &data) {
  LogHandler::info(ui->textEditLog, QString("Serial port %1").arg(data));
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

void MainWindow::onRobotMotorAngleUpdatedFromSerial(int motorIndex, int angle) {
    qDebug() << "[MainWindow] onRobotMotorAngleUpdatedFromSerial called for"
        << motorIndex << "angle" << angle;

    if (motorIndex < 1 || motorIndex > 6)
        return;

    QString objName = QString("lineEditAngleMotor%1").arg(motorIndex);

    QLineEdit* le = nullptr;
    if (ui->groupBoxMotorAngle) {
        qDebug() << "[MainWindow] groupBoxMotorAngle exists, childCount:"
            << ui->groupBoxMotorAngle->children().count();
        le = ui->groupBoxMotorAngle->findChild<QLineEdit*>(objName);
        if (!le) {
            // listar children para depuración
            qDebug() << "[MainWindow] groupBoxMotorAngle children objectNames:";
            for (QObject* child : ui->groupBoxMotorAngle->children()) {
                qDebug() << " -" << child->objectName() << "(" << child->metaObject()->className() << ")";
            }
        }
    }
    else {
        qDebug() << "[MainWindow] ui->groupBoxMotorAngle is NULL, fallback to this->findChild";
        le = this->findChild<QLineEdit*>(objName);
    }

    if (le) {
        le->setText(QString::number(angle) + QChar(0x00B0));
        LogHandler::info(ui->textEditLog,
            QString("Updated UI for motor %1 with angle %2")
            .arg(motorIndex)
            .arg(angle));
        qDebug() << "[MainWindow] Updated" << objName << "to" << angle;
    }
    else {
        LogHandler::warning(ui->textEditLog,
            QString("UI widget not found: %1").arg(objName));
        qDebug() << "[MainWindow] UI widget not found:" << objName;
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

void MainWindow::onCameraStarted() {
  LogHandler::success(ui->textEditLog, "Camera started successfully");

  QString cameraName = VideoCameraHandler::instance().currentCameraName();
  if (!cameraName.isEmpty()) {
    LogHandler::info(ui->textEditLog, "Using camera: " + cameraName);
    ui->lineEditCameraName->setText(cameraName);
  }
}

void MainWindow::onCameraStopped() {
  LogHandler::warning(ui->textEditLog, "Camera stopped");
  ui->lineEditCameraName->clear();
  ui->labelCamera->clear();
}

void MainWindow::onCameraError(const QString &error) {
  LogHandler::error(ui->textEditLog, "Camera Error: " + error);
}

void MainWindow::onFocusModeChanged(const QString &mode) {
  LogHandler::info(ui->textEditLog,
                   QString("Camera focus mode changed to: %1").arg(mode));
  ui->lineEditFocusMode->setText(mode);
}

void MainWindow::onZoomFactorChanged(float zoom) {
  LogHandler::info(ui->textEditLog,
                   QString("Camera zoom factor changed to: %1").arg(zoom));
  ui->lineEditZoomFactor->setText(QString::number(zoom, 'f', 2));
}

void MainWindow::onExposureModeChanged(const QString &mode) {
  LogHandler::info(ui->textEditLog,
                   QString("Camera exposure mode changed to: %1").arg(mode));
  ui->lineEditExposureMode->setText(mode);
}

void MainWindow::onWhiteBalanceModeChanged(const QString &mode) {
  LogHandler::info(
      ui->textEditLog,
      QString("Camera white balance mode changed to: %1").arg(mode));
  ui->lineEditWhiteBalance->setText(mode);
}

void MainWindow::onColorTemperatureChanged(int temperature) {
  LogHandler::info(
      ui->textEditLog,
      QString("Camera color temperature changed to: %1").arg(temperature));
  ui->lineEditColorTemperature->setText(QString::number(temperature));
}
