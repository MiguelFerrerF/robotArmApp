#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include "library-serial/SerialConnectionSetupDialog.h"
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QLineEdit>
#include <QMessageBox>
#include <QProcess>
#include <QSettings>
#include <QTimer>
#include <QVideoFrameFormat>

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent), ui(new Ui::MainWindow), m_RobotHandler(new RobotHandler(this, &m_robotSettings))
{
  ui->setupUi(this);
  this->setWindowTitle("Robot Arm Controller");

  setupConnections();
  connectVideoSignals();
}

MainWindow::~MainWindow()
{
  disconnectVideoSignals();
  delete m_RobotHandler;
  delete m_SerialMonitorDialog;
  delete m_RobotControl;
  delete m_VideoManagerDialog;
  delete m_VideoCalibrationDialog;
  delete ui;
}

/**
 * @brief Establishes the core signal/slot connections for the application infrastructure.
 * Wires the SerialHandler and RobotHandler to the Main Window to ensure
 * logs and status updates are propagated to the dashboard.
 */
void MainWindow::setupConnections()
{
  SerialPortHandler& serial = SerialPortHandler::instance();
  connect(&serial, &SerialPortHandler::errorOccurred, this, &MainWindow::onSerialError);
  connect(&serial, &SerialPortHandler::connectionStatusChanged, this, &MainWindow::onSerialStatusChanged);
  connect(&serial, &SerialPortHandler::dataReceived, this, &MainWindow::onDataReceived);
  connect(m_RobotHandler, &RobotHandler::motorAngleChanged, this, &MainWindow::onRobotMotorAngleUpdatedFromSerial);
  connect(m_RobotHandler, &RobotHandler::motorOffsetsChanged, this, &MainWindow::onRobotMotorOffsetsReadFromMemory);
  connect(m_RobotHandler, &RobotHandler::errorOccurred, this, &MainWindow::onSerialError);
  connect(m_RobotHandler, &RobotHandler::efectorPositionChanged, this, &MainWindow::onEfectorPositionChanged);
  connect(m_RobotHandler, &RobotHandler::anglesCalculated, this, &MainWindow::onRobotAnglesCalculated);
}

/**
 * @brief Opens the Serial Monitor dialog.
 * If it doesn't exist, it creates a new instance.
 *
 * The dialog is then shown and brought to the front.
 */
void MainWindow::on_actionSerial_triggered()
{
  if (!m_SerialMonitorDialog) {
    m_SerialMonitorDialog = new SerialMonitorDialog(this);
    connect(m_SerialMonitorDialog, &SerialMonitorDialog::warningOccurred, this, &MainWindow::onSerialMonitorWarning);
  }
  m_SerialMonitorDialog->show();
  m_SerialMonitorDialog->raise();
  m_SerialMonitorDialog->activateWindow();
}

/**
 * @brief Saves the current log displayed in the log text edit to a timestamped file.
 * The log file is saved in a "logs" directory within the current working directory.
 * If the directory does not exist, it is created.
 * After saving, the log file location is opened in the system file explorer.
 */
void MainWindow::on_actionLog_triggered()
{
  QString logsDirPath = QDir::currentPath() + "/logs";
  QDir    logsDir(logsDirPath);
  if (!logsDir.exists()) {
    if (!logsDir.mkpath(".")) {
      LogHandler::error(ui->textEditLog, "Failed to create logs directory");
      return;
    }
  }

  QString logFileName = logsDirPath + QString("/log_%1.txt").arg(QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss"));
  QFile   logFile(logFileName);
  if (logFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
    QTextStream out(&logFile);
    out << ui->textEditLog->toPlainText();
    logFile.close();
    LogHandler::success(ui->textEditLog, "Log saved to " + logFileName);
    QProcess::startDetached("explorer.exe", {"/select,", QDir::toNativeSeparators(logFileName)});
  }
  else {
    LogHandler::error(ui->textEditLog, "Failed to save log to file");
  }
}

/**
 * @brief Initiates the serial connection setup process.
 * If already connected, logs a message and returns.
 * Otherwise, opens the SerialConnectionSetupDialog for user configuration.
 * On successful configuration, logs a success message.
 */
void MainWindow::on_actionConnectSerial_triggered()
{
  if (SerialPortHandler::instance().isConnected()) {
    LogHandler::info(ui->textEditLog, "Serial port already connected");
    return;
  }

  SerialConnectionSetupDialog dialog(this);
  connect(&dialog, &SerialConnectionSetupDialog::errorOccurred, this, &MainWindow::onSetupConnectionError);

  if (dialog.exec() == QDialog::Accepted)
    LogHandler::success(ui->textEditLog, "Serial connection configured successfully");
}

/**
 * @brief Disconnects the current serial connection if connected.
 * Logs a warning if no connection is active.
 */
void MainWindow::on_actionDisconnectSerial_triggered()
{
  if (SerialPortHandler::instance().isConnected()) {
    SerialPortHandler::instance().disconnectSerial();
    LogHandler::warning(ui->textEditLog, "Disconnected from serial port");
  }
  else
    LogHandler::warning(ui->textEditLog, "No serial port connected");
}

/**
 * @brief Opens the Video Manager dialog for camera connection and management.
 * If the dialog does not exist, it creates a new instance.
 * The dialog is then shown and brought to the front.
 */
void MainWindow::on_actionConnectVideo_triggered()
{
  if (!m_VideoManagerDialog) {
    m_VideoManagerDialog = new VideoManagerDialog(this);
  }

  // Si la cámara ya está corriendo, el diálogo mostrará el stream actual.
  m_VideoManagerDialog->show();
  m_VideoManagerDialog->raise();
  m_VideoManagerDialog->activateWindow();
}

/**
 * @brief Disconnects the video feed by requesting the VideoCaptureHandler to stop the camera.
 * Clears the camera label in the UI.
 */
void MainWindow::on_actionDisconnectVideo_triggered()
{
  VideoCaptureHandler::instance().requestCameraChange(-1, QSize()); // Petición de STOP
  ui->labelCamera->clear();
}

/**
 * @brief Opens the Video Calibration dialog for camera calibration tasks.
 * If the dialog does not exist, it creates a new instance.
 * The dialog is then shown and brought to the front.
 */
void MainWindow::on_actionCalibrationVideo_triggered()
{
  if (!m_VideoCalibrationDialog) {
    m_VideoCalibrationDialog = new VideoCalibrationDialog(this, m_RobotHandler);
  }

  m_VideoCalibrationDialog->show();
  m_VideoCalibrationDialog->raise();
  m_VideoCalibrationDialog->activateWindow();
}

/**
 * @brief Configures the Vision-to-Motion pipeline.
 *
 * This function is the "brain" of the automatic mode. It performs dynamic signal wiring:
 * 1. Disconnects standard video feeds.
 * 2. Connects `VideoProcessingDialog` outputs (centroid, angle) to the UI.
 * 3. Connects the detection signal (`piecePointsUpdated`) to the **Calibration Logic**.
 * 4. The Calibration Logic (`calculateObjectPosition`) computes the 3D coordinate.
 * 5. The Calibration Dialog emits `piecePositionCalculated`, which updates the `RobotHandler`.
 * 6. The `RobotHandler` computes Inverse Kinematics and emits `anglesCalculated`.
 */
void MainWindow::on_actionProcessingVideo_triggered()
{
  if (!m_VideoProcessingDialog) {
    m_VideoProcessingDialog = new VideoProcessingDialog(this);

    // Cleanup old connections
    disconnect(&VideoCaptureHandler::instance(), &VideoCaptureHandler::newPixmapCaptured, this, nullptr);
    disconnect(m_VideoProcessingDialog, &VideoProcessingDialog::angleUpdated, this, nullptr);
    disconnect(m_VideoProcessingDialog, &VideoProcessingDialog::processedImageReady, this, nullptr);
    disconnect(m_VideoProcessingDialog, &VideoProcessingDialog::piecePointsUpdated, this, nullptr);

    // Wire: Processor -> UI (View)
    connect(m_VideoProcessingDialog, &VideoProcessingDialog::processedImageReady, this, &MainWindow::onVideoCapture);

    // Wire: Processor -> Calibration -> Robot
    connect(m_VideoProcessingDialog, &VideoProcessingDialog::angleUpdated, this,
            [this](double angle) { ui->lineEditAngle->setText(QString::number((180 - angle), 'f', 2)); });
    connect(m_VideoProcessingDialog, &VideoProcessingDialog::piecePointsUpdated, this, [this](const QPoint& centroid, const QPoint& pointRecta) {
      ui->lineEditCentroidX->setText(QString::number(centroid.x()));
      ui->lineEditCentroidY->setText(QString::number(centroid.y()));

      // Lazy load Calibration Dialog
      if (!m_VideoCalibrationDialog) {
        m_VideoCalibrationDialog = new VideoCalibrationDialog(this, m_RobotHandler);
        disconnect(m_VideoCalibrationDialog, &VideoCalibrationDialog::piecePositionCalculated, this, nullptr);
        connect(m_VideoCalibrationDialog, &VideoCalibrationDialog::piecePositionCalculated, this, [this](const cv::Point3d& positionInBase) {
          ui->lineEditDesiredX->setText(QString::number(positionInBase.x, 'f', 2));
          ui->lineEditDesiredY->setText(QString::number(positionInBase.y, 'f', 2));
          ui->lineEditDesiredZ->setText(QString::number(positionInBase.z, 'f', 2));
        });
      }

      // Trigger the 2D->3D Math conversion
      m_VideoCalibrationDialog->calculateObjectPosition(centroid, pointRecta);
    });
  }
  else {
    m_VideoProcessingDialog->show();
    m_VideoProcessingDialog->raise();
    m_VideoProcessingDialog->activateWindow();
  }
}

/**
 * @brief Connects the video capture signals to the main window slots.
 * Sets up the necessary connections to handle new video frames and camera info updates.
 */
void MainWindow::connectVideoSignals()
{
  VideoCaptureHandler& handler = VideoCaptureHandler::instance();

  // Standard "Live View" connection
  connect(&handler, &VideoCaptureHandler::newPixmapCaptured, this, [this](const QPixmap& pixmap) { this->onVideoCapture(pixmap.toImage()); });
  connect(&handler, &VideoCaptureHandler::cameraInfoChanged, this, &MainWindow::onCameraInfoChanged);
  connect(&handler, &VideoCaptureHandler::cameraOpenFailed, this,
          [this](int, const QString& err) { LogHandler::error(ui->textEditLog, "Camera Error: " + err); });
}

/**
 * @brief Disconnects all video-related signals from the main window.
 * This is typically used when switching to a different video processing mode.
 */
void MainWindow::disconnectVideoSignals()
{
  VideoCaptureHandler& handler = VideoCaptureHandler::instance();
  handler.~VideoCaptureHandler();
}

/**
 * @brief Opens the Robot Control dialog for manual robot manipulation.
 * If the dialog does not exist, it creates a new instance.
 * The dialog is then shown and brought to the front.
 *
 * Additionally, it connects relevant signals from the RobotControlDialog
 * to the MainWindow slots to handle errors and motor adjustments.
 * It also requests the current motor offsets from the robot via serial command.
 */
void MainWindow::on_actionControlRobot_triggered()
{
  qDebug("Control robot pulsado");
  if (!m_RobotControl) {
    m_RobotControl = new RobotControlDialog(this, &m_robotSettings);
  }
  m_RobotControl->show();
  m_RobotControl->raise();
  m_RobotControl->activateWindow();

  disconnect(m_RobotControl, &RobotControlDialog::errorOccurred, this, &MainWindow::onRobotControlError);
  disconnect(m_RobotControl, &RobotControlDialog::motorAngleChanged, this, &MainWindow::onRobotMotorAngleChanged);
  disconnect(m_RobotControl, &RobotControlDialog::allMotorsReset, this, &MainWindow::onAllMotorsReset);
  disconnect(m_RobotControl, &RobotControlDialog::motorOffsetChanged, this, &MainWindow::onRobotMotorOffsetChanged);
  disconnect(m_RobotControl, &RobotControlDialog::placePositionChanged, this, &MainWindow::onRobotPlacePositionChanged);

  connect(m_RobotControl, &RobotControlDialog::errorOccurred, this, &MainWindow::onRobotControlError);
  connect(m_RobotControl, &RobotControlDialog::motorAngleChanged, this, &MainWindow::onRobotMotorAngleChanged);
  connect(m_RobotControl, &RobotControlDialog::allMotorsReset, this, &MainWindow::onAllMotorsReset);
  connect(m_RobotControl, &RobotControlDialog::motorOffsetChanged, this, &MainWindow::onRobotMotorOffsetChanged);
  connect(m_RobotControl, &RobotControlDialog::placePositionChanged, this, &MainWindow::onRobotPlacePositionChanged);

  if (SerialPortHandler::instance().isConnected()) {
    QString commandOffset = "READ:OFFSETS";
    SerialPortHandler::instance().sendData(commandOffset.toUtf8());
  }
  else {
    LogHandler::warning(ui->textEditLog, "No se puede enviar comando: puerto serie no conectado");
  }
}

/**
 * @brief Opens the Robot Calibration dialog for calibrating robot motors.
 * If the dialog does not exist, it creates a new instance.
 * The dialog is then shown and brought to the front.
 */
void MainWindow::on_actionCalibrateRobot_triggered()
{
  if (!m_RobotCalibrationDialog) {
    m_RobotCalibrationDialog = new RobotCalibrationDialog(this, &m_robotSettings);
  }

  m_RobotCalibrationDialog->show();
  m_RobotCalibrationDialog->raise();
  m_RobotCalibrationDialog->activateWindow();
}

/**
 * @brief Handles serial errors by logging them to the UI log text edit.
 *
 * @param error The error message received from the serial handler.
 */
void MainWindow::onSerialError(const QString& error)
{
  LogHandler::error(ui->textEditLog, "Serial Error: " + error);
}

/**
 * @brief Handles changes in the serial connection status.
 * Logs the connection or disconnection event to the UI log text edit.
 *
 * @param connected True if the serial port is connected, false if disconnected.
 */
void MainWindow::onSerialStatusChanged(bool connected)
{
  LogHandler::info(ui->textEditLog, QString("Serial port %1").arg(connected ? "connected" : "disconnected"));
}

/**
 * @brief Handles errors that occur during the setup of the serial connection.
 * Logs the error message to the UI log text edit.
 *
 * @param error The error message received during connection setup.
 */
void MainWindow::onSetupConnectionError(const QString& error)
{
  LogHandler::error(ui->textEditLog, error);
}

/**
 * @brief Handles warnings emitted by the Serial Monitor dialog.
 * Logs the warning message to the UI log text edit.
 *
 * @param warning The warning message received from the Serial Monitor.
 */
void MainWindow::onSerialMonitorWarning(const QString& warning)
{
  LogHandler::warning(ui->textEditLog, warning);
}

/**
 * @brief Handles incoming data received from the serial port.
 * Logs the received data to the UI log text edit.
 *
 * @param data The data received from the serial port.
 */
void MainWindow::onDataReceived(const QByteArray& data)
{
  LogHandler::info(ui->textEditLog, QString("Serial port %1").arg(data));
}

/**
 * @brief Handles errors emitted by the Robot Control dialog.
 * Logs the error message to the UI log text edit.
 *
 * @param error The error message received from the Robot Control dialog.
 */
void MainWindow::onRobotControlError(const QString& error)
{
  LogHandler::error(ui->textEditLog, "Robot Control Error: " + error);
}

/**
 * @brief Handles manual changes to robot motor angles from the Robot Control dialog.
 *
 * Sends the appropriate command to the robot hardware via the serial port.
 * Logs the command sent or a warning if the serial port is not connected.
 * The command format is `SETUP:SERVO{motorIndex}:{angle}`.
 *
 * @param motorIndex The index of the motor being adjusted (1-based).
 * @param angle The new angle value for the motor.
 */
void MainWindow::onRobotMotorAngleChanged(int motorIndex, int angle)
{
  if (SerialPortHandler::instance().isConnected()) {
    QString command = QString("SETUP:SERVO%1:%2").arg(motorIndex).arg(angle);
    SerialPortHandler::instance().sendData(command.toUtf8());
    LogHandler::info(ui->textEditLog, QString("Sent command to motor %1: %2").arg(motorIndex).arg(command.trimmed()));
  }
  else {
    LogHandler::warning(ui->textEditLog, "Cannot send command: Serial port not connected");
  }
}

/**
 * @brief Handles changes to robot motor offsets from the Robot Control dialog.
 *
 * Sends the appropriate command to the robot hardware via the serial port.
 * Logs the command sent or a warning if the serial port is not connected.
 * The command format is `SETUP:OFFSET{motorIndex}:{newOffset}`.
 *
 * @param motorIndex The index of the motor whose offset is being adjusted (1-based).
 * @param newOffset The new offset value for the motor.
 */
void MainWindow::onRobotMotorOffsetChanged(int motorIndex, int newOffset)
{
  if (SerialPortHandler::instance().isConnected()) {
    QString command = QString("SETUP:OFFSET%1:%2").arg(motorIndex).arg(newOffset);
    SerialPortHandler::instance().sendData(command.toUtf8());
    LogHandler::info(ui->textEditLog, QString("Sent command to motor %1: %2").arg(motorIndex).arg(command.trimmed()));
  }
  else {
    LogHandler::warning(ui->textEditLog, "Cannot send command: Serial port not connected");
  }
}

/**
 * @brief Handles changes to the robot's place position from the Robot Control dialog.
 *
 * Sends the appropriate command to the robot hardware via the serial port.
 * Logs the command sent or a warning if the serial port is not connected.
 * The command format is `SETUP:PLACE{motorIndex}:{position}`.
 *
 * @param motorIndex The index of the motor whose place position is being adjusted (1-based).
 * @param position The new place position value for the motor.
 */
void MainWindow::onRobotPlacePositionChanged(int motorIndex, int position)
{
  if (SerialPortHandler::instance().isConnected()) {
    QString command = QString("SETUP:PLACE%1:%2").arg(motorIndex).arg(position);
    SerialPortHandler::instance().sendData(command.toUtf8());
    LogHandler::info(ui->textEditLog, QString("Sent command to motor %1: %2").arg(motorIndex).arg(command.trimmed()));
  }
  else {
    LogHandler::warning(ui->textEditLog, "Cannot send command: Serial port not connected");
  }
}

/**
 * @brief Handles real-time feedback from the robot hardware.
 *
 * When the robot sends an `ANGLE_WITH_OFFSET` message, this slot finds the
 * corresponding QLineEdit in the dashboard (via `findChild`) and updates it.
 * This ensures the UI stays in sync if the robot moves autonomously.
 */
void MainWindow::onRobotMotorAngleUpdatedFromSerial(int motorIndex, int angle)
{
  qDebug() << "[MainWindow] onRobotMotorAngleUpdatedFromSerial called for" << motorIndex << "angle" << angle;

  if (motorIndex < 1 || motorIndex > 6)
    return;

  QString objName = QString("lineEditAngleMotor%1").arg(motorIndex);

  QLineEdit* le = nullptr;
  if (ui->groupBoxMotorAngle) {
    qDebug() << "[MainWindow] groupBoxMotorAngle exists, childCount:" << ui->groupBoxMotorAngle->children().count();
    le = ui->groupBoxMotorAngle->findChild<QLineEdit*>(objName);
    if (!le) {
      qDebug() << "[MainWindow] groupBoxMotorAngle children objectNames:";
      for (QObject* child : ui->groupBoxMotorAngle->children()) {
        qDebug() << " -" << child->objectName() << "(" << child->metaObject()->className() << ")";
      }
    }
  }
  else {
    qDebug() << "[MainWindow] ui->groupBoxMotorAngle is NULL, fallback to "
                "this->findChild";
    le = this->findChild<QLineEdit*>(objName);
  }

  if (le) {
    le->setText(QString::number(angle) + QChar(0x00B0));
    LogHandler::info(ui->textEditLog, QString("Updated UI for motor %1 with angle %2").arg(motorIndex).arg(angle));
    qDebug() << "[MainWindow] Updated" << objName << "to" << angle;
  }
  else {
    LogHandler::warning(ui->textEditLog, QString("UI widget not found: %1").arg(objName));
    qDebug() << "[MainWindow] UI widget not found:" << objName;
  }
}

/**
 * @brief Handles motor offset updates received from the robot hardware.
 *
 * Updates the internal robot settings with the new offset value for the specified motor.
 * Calls `setupOffsets` on the RobotHandler to apply the new offsets.
 * Logs the offset update to the UI log text edit.
 *
 * @param motorIndex The index of the motor whose offset was updated (1-based).
 * @param offset The new offset value for the motor.
 */
void MainWindow::onRobotMotorOffsetsReadFromMemory(int motorIndex, int offset)
{
  if (motorIndex < 1 || motorIndex > 6)
    return;

  m_robotSettings.motors[motorIndex - 1].defaultAngle = offset;
  m_RobotControl->setupOffsets();
  LogHandler::info(ui->textEditLog, QString("Updated offset for motor %1 with value %2").arg(motorIndex).arg(offset));
}

/**
 * @brief Updates the UI with the current end-effector position.
 *
 * This slot is called whenever the RobotHandler emits the `efectorPositionChanged` signal.
 * It updates the corresponding QLineEdit widgets in the UI to reflect the new X, Y, Z coordinates.
 *
 * @param x The X coordinate of the end-effector.
 * @param y The Y coordinate of the end-effector.
 * @param z The Z coordinate of the end-effector.
 */
void MainWindow::onEfectorPositionChanged(double x, double y, double z)
{
  ui->lineEditX->setText(QString::number(x, 'f', 2));
  ui->lineEditY->setText(QString::number(y, 'f', 2));
  ui->lineEditZ->setText(QString::number(z, 'f', 2));
}

/**
 * @brief Updates the UI with the calculated robot joint angles.
 *
 * This slot is called whenever the RobotHandler emits the `anglesCalculated` signal.
 * It updates the corresponding QLineEdit widgets in the UI to reflect the new joint angles.
 *
 * @param q1 The angle for joint 1.
 * @param q2 The angle for joint 2.
 * @param q3 The angle for joint 3.
 * @param q5 The angle for joint 5.
 */
void MainWindow::onRobotAnglesCalculated(int q1, int q2, int q3, int q5)
{
  if (!m_VideoProcessingDialog) {
    return;
  }

  ui->lineEditQ1->setText(QString::number(q1));
  ui->lineEditQ2->setText(QString::number(q2));
  ui->lineEditQ3->setText(QString::number(q3));
  ui->lineEditQ5->setText(QString::number(q5));
}

/**
 * @brief Handles the event when all motors are reset to their default positions.
 *
 * Logs an informational message to the UI log text edit indicating that
 * all motors have been reset.
 */
void MainWindow::onAllMotorsReset()
{
  LogHandler::info(ui->textEditLog, "All motors have been reset to default");
}

/**
 * @brief Slot to handle new video frames captured from the camera.
 *
 * This function updates the camera display label with the new image.
 * It scales the image to fit the label while maintaining the aspect ratio.
 *
 * @param image The new frame captured from the camera as a QImage.
 */
void MainWindow::onVideoCapture(const QImage& image)
{
  m_lastCapturedFrame = image;
  if (image.isNull())
    return;

  QPixmap pixmap = QPixmap::fromImage(image).scaledToWidth(ui->labelCamera->width(), Qt::SmoothTransformation);
  ui->labelCamera->setPixmap(pixmap);
}

/**
 * @brief Slot to handle the event when the camera starts successfully.
 *
 * Logs a success message to the UI log text edit indicating that
 * the camera has started.
 */
void MainWindow::onCameraStarted()
{
  LogHandler::success(ui->textEditLog, "Camera started successfully");
}

/**
 * @brief Slot to handle the event when the camera stops.
 *
 * Logs a warning message to the UI log text edit indicating that
 * the camera has stopped. Also clears the camera name and display label.
 */
void MainWindow::onCameraStopped()
{
  LogHandler::warning(ui->textEditLog, "Camera stopped");
  ui->lineEditCameraName->clear();
  ui->labelCamera->clear();
}

/**
 * @brief Slot to handle camera errors.
 *
 * Logs an error message to the UI log text edit indicating the
 * specific error that occurred with the camera.
 *
 * @param error The error message received from the camera handler.
 */
void MainWindow::onCameraError(const QString& error)
{
  LogHandler::error(ui->textEditLog, "Camera Error: " + error);
}

/**
 * @brief Updates the UI with the current camera information.
 *
 * This slot is called whenever the VideoCaptureHandler emits the `cameraInfoChanged` signal.
 * It updates various QLineEdit widgets in the UI to reflect the current camera settings.
 *
 * @param info The CameraInfo struct containing the current camera settings.
 */
void MainWindow::onCameraInfoChanged(const CameraInfo& info)
{
  ui->lineEditCameraName->setText(QString::fromStdString(info.name));
  if (info.width > 0 && info.height > 0) {
    ui->lineEditResolution->setText(QString("%1 x %2 px").arg(info.width).arg(info.height));
  }
  else {
    ui->lineEditResolution->setText("Default");
  }
  ui->lineEditBrightness->setText(QString("%1 %").arg(static_cast<int>((info.brightness / 255.0) * 100)));
  ui->lineEditContrast->setText(QString("%1 %").arg(static_cast<int>((info.contrast / 255.0) * 100)));
  ui->lineEditSaturation->setText(QString("%1 %").arg(static_cast<int>((info.saturation / 255.0) * 100)));
  ui->lineEditSharpness->setText(QString("%1 %").arg(static_cast<int>((info.sharpness / 255.0) * 100)));
  if (!info.isFocusAuto)
    ui->lineEditFocus->setText(QString("%1 %").arg(static_cast<int>((info.focus / 255.0) * 100)));
  else
    ui->lineEditFocus->setText("Auto");
  if (!info.isExposureAuto)
    ui->lineEditExposure->setText(QString("%1 %").arg(static_cast<int>((info.exposure / 255.0) * 100)));
  else
    ui->lineEditExposure->setText("Auto");
}

/**
 * @brief Toggles the video processing mode on or off.
 *
 * When activated, it sets up the necessary connections for video processing,
 * updates the UI button text and style, and initializes the VideoProcessingDialog.
 * When deactivated, it restores the original video feed connections and resets the button.
 *
 * @param checked True if the button is toggled on (processing mode), false otherwise.
 */
void MainWindow::on_pushButtonStartProcessing_toggled(bool checked)
{
  if (checked) {
    ui->pushButtonStartProcessing->setText("Stop Processing");
    ui->pushButtonStartProcessing->setStyleSheet("background-color: red; color: white;");

    if (!m_VideoProcessingDialog) {
      m_VideoProcessingDialog = new VideoProcessingDialog(this);
    }
    disconnect(&VideoCaptureHandler::instance(), &VideoCaptureHandler::newPixmapCaptured, this, nullptr);
    disconnect(m_VideoProcessingDialog, &VideoProcessingDialog::angleUpdated, this, nullptr);
    disconnect(m_VideoProcessingDialog, &VideoProcessingDialog::processedImageReady, this, nullptr);
    disconnect(m_VideoProcessingDialog, &VideoProcessingDialog::piecePointsUpdated, this, nullptr);

    connect(m_VideoProcessingDialog, &VideoProcessingDialog::processedImageReady, this, &MainWindow::onVideoCapture);
    connect(m_VideoProcessingDialog, &VideoProcessingDialog::angleUpdated, this,
            [this](double angle) { ui->lineEditAngle->setText(QString::number((180 - angle), 'f', 2)); });
    connect(m_VideoProcessingDialog, &VideoProcessingDialog::piecePointsUpdated, this, [this](const QPoint& centroid, const QPoint& pointRecta) {
      ui->lineEditCentroidX->setText(QString::number(centroid.x()));
      ui->lineEditCentroidY->setText(QString::number(centroid.y()));

      if (!m_VideoCalibrationDialog) {
        m_VideoCalibrationDialog = new VideoCalibrationDialog(this, m_RobotHandler);
        disconnect(m_VideoCalibrationDialog, &VideoCalibrationDialog::piecePositionCalculated, this, nullptr);
        connect(m_VideoCalibrationDialog, &VideoCalibrationDialog::piecePositionCalculated, this, [this](const cv::Point3d& positionInBase) {
          ui->lineEditDesiredX->setText(QString::number(positionInBase.x, 'f', 2));
          ui->lineEditDesiredY->setText(QString::number(positionInBase.y, 'f', 2));
          ui->lineEditDesiredZ->setText(QString::number(positionInBase.z, 'f', 2));
        });
      }
      m_VideoCalibrationDialog->calculateObjectPosition(centroid, pointRecta);
    });
  }
  else {
    ui->pushButtonStartProcessing->setText("Start Processing");
    ui->pushButtonStartProcessing->setStyleSheet("");
    disconnect(&VideoCaptureHandler::instance(), &VideoCaptureHandler::newPixmapCaptured, this, nullptr);
    connectVideoSignals();
  }
}

/**
 * @brief Constructs and sends the final Pick-and-Place command.
 *
 * 1. Validates the object's orientation angle (must be reachable by the gripper).
 * 2. Retrieves the calculated Inverse Kinematics angles (Q1, Q2, Q3, Q5).
 * 3. Adds the calibrated offsets (`defaultAngle`) to translate logical angles to physical servo values.
 * 4. Formats the `PLACE` protocol string and transmits via Serial.
 */
void MainWindow::on_pushButtonPickAndPlace_clicked()
{
  bool   ok;
  double angle = ui->lineEditAngle->text().toDouble(&ok);
  if (!ok || angle < -60.0 || angle > 60.0) {
    QMessageBox::warning(this, "Invalid Angle", "The angle must be between -60 and 60 degrees.");
    return;
  }

  // Change the color of the button to indicate action
  ui->pushButtonPickAndPlace->setStyleSheet("background-color: green; color: white;");
  QTimer::singleShot(18000, this, [this]() { ui->pushButtonPickAndPlace->setStyleSheet(""); });

  if (SerialPortHandler::instance().isConnected()) {
    QString command = QString("PLACE:%1:%2:%3:%4:%5:%6")
                        .arg(m_robotSettings.motors[0].defaultAngle + ui->lineEditQ1->text().toInt())
                        .arg(m_robotSettings.motors[1].defaultAngle + ui->lineEditQ2->text().toInt())
                        .arg(m_robotSettings.motors[2].defaultAngle + ui->lineEditQ3->text().toInt())
                        .arg(m_robotSettings.motors[3].defaultAngle)
                        .arg(m_robotSettings.motors[4].defaultAngle + ui->lineEditQ5->text().toInt())
                        .arg("0");
    SerialPortHandler::instance().sendData(command.toUtf8());
    LogHandler::info(ui->textEditLog, QString("Sent pick and place command: %1").arg(command.trimmed()));
  }
  else {
    LogHandler::warning(ui->textEditLog, "Cannot send command: Serial port not connected");
  }
}
