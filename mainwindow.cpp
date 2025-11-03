#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include "library-serial/SerialConnectionSetupDialog.h"
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QLineEdit>
#include <QProcess>
#include <QSettings>
#include <QVideoFrameFormat>

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent), ui(new Ui::MainWindow), m_RobotHandler(new RobotHandler(this))
{
  ui->setupUi(this);
  this->setWindowTitle("Robot Arm Controller");

  setupConnections();
  connectVideoSignals();

  // Inicializamos el CalibrationHandler con el tablero 9x6 y cuadrados de 10mm
  calibrationHandler = new CalibrationHandler(cv::Size(9, 6), 10.0f);

  // Conectar el boton al slot
  connect(ui->calibrateButton, &QPushButton::clicked, this, &MainWindow::onCalibrateButtonClicked);
}

MainWindow::~MainWindow()
{
  disconnectVideoSignals();
  delete m_RobotHandler;
  delete m_SerialMonitorDialog;
  delete m_RobotControl;
  delete m_VideoManagerDialog;
  delete calibrationHandler;
  delete ui;
}

void MainWindow::onCalibrateButtonClicked()
{
  QString folderPath = QDir::currentPath() + "/images/calibration"; // Carpeta con im�genes de calibraci�n
  QDir    dir(folderPath);

  if (!dir.exists()) {
    qDebug() << "La carpeta de imagenes no existe:" << folderPath;
    return;
  }

  QStringList filters;
  filters << "*.jpg"
          << "*.png"
          << "*.tif";
  QFileInfoList fileList = dir.entryInfoList(filters, QDir::Files);

  if (fileList.isEmpty()) {
    qDebug() << "No se encontraron im�genes en" << folderPath;
    return;
  }

  // A�adir todas las im�genes al CalibrationHandler
  for (const QFileInfo& fileInfo : fileList) {
    cv::Mat image = cv::imread(fileInfo.absoluteFilePath().toStdString());
    if (!image.empty()) {
      calibrationHandler->addImage(image);
    }
    else {
      qDebug() << "No se pudo leer la imagen:" << fileInfo.fileName();
    }
  }

  // Ejecutar calibraci�n
  if (calibrationHandler->runCalibration()) {
    // Guardar resultados
    calibrationHandler->saveCalibration("cameraMatrix.yml", "distCoeffs.yml");
    qDebug() << "Calibraci�n completada y guardada correctamente.";
  }
  else {
    qDebug() << "Calibraci�n fallida.";
  }
}

void MainWindow::setupConnections()
{
  SerialPortHandler& serial = SerialPortHandler::instance();
  connect(&serial, &SerialPortHandler::errorOccurred, this, &MainWindow::onSerialError);
  connect(&serial, &SerialPortHandler::connectionStatusChanged, this, &MainWindow::onSerialStatusChanged);
  connect(&serial, &SerialPortHandler::dataReceived, this, &MainWindow::onDataReceived);
  connect(m_RobotHandler, &RobotHandler::motorAngleChanged, this, &MainWindow::onRobotMotorAngleUpdatedFromSerial);
  connect(m_RobotHandler, &RobotHandler::errorOccurred, this, &MainWindow::onSerialError);
  connect(m_RobotHandler, &RobotHandler::efectorPositionChanged, this, &MainWindow::onEfectorPositionChanged);
}

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

void MainWindow::on_actionLog_triggered()
{
  // Ensure the logs directory exists
  QString logsDirPath = QDir::currentPath() + "/logs";
  QDir    logsDir(logsDirPath);
  if (!logsDir.exists()) {
    if (!logsDir.mkpath(".")) {
      LogHandler::error(ui->textEditLog, "Failed to create logs directory");
      return;
    }
  }

  // Save log to a file (date-time stamped) in the logs directory
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

void MainWindow::on_actionDisconnectSerial_triggered()
{
  if (SerialPortHandler::instance().isConnected()) {
    SerialPortHandler::instance().disconnectSerial();
    LogHandler::warning(ui->textEditLog, "Disconnected from serial port");
  }
  else
    LogHandler::warning(ui->textEditLog, "No serial port connected");
}

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

void MainWindow::on_actionDisconnectVideo_triggered()
{
  VideoCaptureHandler::instance().requestCameraChange(-1, QSize()); // Petición de STOP
  ui->labelCamera->clear();
}

void MainWindow::on_actionCalibrationVideo_triggered()
{
  if (!m_VideoCalibrationDialog) {
    m_VideoCalibrationDialog = new VideoCalibrationDialog(this);
  }

  m_VideoCalibrationDialog->show();
  m_VideoCalibrationDialog->raise();
  m_VideoCalibrationDialog->activateWindow();
}

void MainWindow::connectVideoSignals()
{
  VideoCaptureHandler& handler = VideoCaptureHandler::instance();

  // Conexión principal para mostrar el vídeo en la GUI
  connect(&handler, &VideoCaptureHandler::newPixmapCaptured, this, [this](const QPixmap& pixmap) { this->onVideoCapture(pixmap.toImage()); });

  // Conexiones de info de la cámara (para actualizar la GUI principal)
  // connect(&handler, &VideoCaptureHandler::propertiesSupported, this,
  //         &MainWindow::onPropertiesSupported); // Asumiendo que existe un
  //         slot
  //                                              // para esto

  connect(&handler, &VideoCaptureHandler::cameraInfoChanged, this, &MainWindow::onCameraInfoChanged);

  // Conectamos las señales de error para el log principal
  connect(&handler, &VideoCaptureHandler::cameraOpenFailed, this,
          [this](int, const QString& err) { LogHandler::error(ui->textEditLog, "Camera Error: " + err); });
}

void MainWindow::disconnectVideoSignals()
{
  VideoCaptureHandler& handler = VideoCaptureHandler::instance();
  handler.~VideoCaptureHandler();
}

void MainWindow::on_actionControlRobot_triggered()
{
  qDebug("Control robot pulsado");
  if (!m_RobotControl) {
    m_RobotControl = new RobotControlDialog(this, &m_robotSettings);
  }
  m_RobotControl->show();
  m_RobotControl->raise();
  m_RobotControl->activateWindow();

  // Disconnect previous connections if any to avoid duplicates
  disconnect(m_RobotControl, &RobotControlDialog::errorOccurred, this, &MainWindow::onRobotControlError);
  disconnect(m_RobotControl, &RobotControlDialog::motorAngleChanged, this, &MainWindow::onRobotMotorAngleChanged);
  disconnect(m_RobotControl, &RobotControlDialog::allMotorsReset, this, &MainWindow::onAllMotorsReset);

  // Connect signals from RobotControlDialog
  connect(m_RobotControl, &RobotControlDialog::errorOccurred, this, &MainWindow::onRobotControlError);
  connect(m_RobotControl, &RobotControlDialog::motorAngleChanged, this, &MainWindow::onRobotMotorAngleChanged);
  connect(m_RobotControl, &RobotControlDialog::allMotorsReset, this, &MainWindow::onAllMotorsReset);
}

void MainWindow::on_actionCalibrateRobot_triggered()
{
  LogHandler::info(ui->textEditLog, "Robot calibration started");
  int calibratedAngles[] = {90, 90, 98, 90, 142, 82};
  for (int i = 0; i < 6; ++i) {
    if (SerialPortHandler::instance().isConnected()) {
      QString command = QString("SETUP:SERVO%1:%2").arg(i + 1).arg(calibratedAngles[i]);
      SerialPortHandler::instance().sendData(command.toUtf8());
      LogHandler::info(ui->textEditLog, QString("Sent calibration to motor %1: %2").arg(i + 1).arg(command.trimmed()));
    }
    else {
      LogHandler::warning(ui->textEditLog, "Cannot send calibration: Serial port not connected");
      break;
    }
  }
}

void MainWindow::onSerialError(const QString& error)
{
  LogHandler::error(ui->textEditLog, "Serial Error: " + error);
}

void MainWindow::onSerialStatusChanged(bool connected)
{
  LogHandler::info(ui->textEditLog, QString("Serial port %1").arg(connected ? "connected" : "disconnected"));
}

void MainWindow::onSetupConnectionError(const QString& error)
{
  LogHandler::error(ui->textEditLog, error);
}

void MainWindow::onSerialMonitorWarning(const QString& warning)
{
  LogHandler::warning(ui->textEditLog, warning);
}

void MainWindow::onDataReceived(const QByteArray& data)
{
  LogHandler::info(ui->textEditLog, QString("Serial port %1").arg(data));
}

void MainWindow::onRobotControlError(const QString& error)
{
  LogHandler::error(ui->textEditLog, "Robot Control Error: " + error);
}

void MainWindow::onRobotMotorAngleChanged(int motorIndex, int angle)
{
  // send command to robot via serial
  if (SerialPortHandler::instance().isConnected()) {
    QString command = QString("SETUP:SERVO%1:%2").arg(motorIndex).arg(angle);
    SerialPortHandler::instance().sendData(command.toUtf8());
    LogHandler::info(ui->textEditLog, QString("Sent command to motor %1: %2").arg(motorIndex).arg(command.trimmed()));
  }
  else {
    LogHandler::warning(ui->textEditLog, "Cannot send command: Serial port not connected");
  }
}

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
      // listar children para depuraci�n
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

void MainWindow::onEfectorPositionChanged(double x, double y, double z)
{
  ui->lineEditX->setText(QString::number(x, 'f', 2) + " mm");
  ui->lineEditY->setText(QString::number(y, 'f', 2) + " mm");
  ui->lineEditZ->setText(QString::number(z, 'f', 2) + " mm");
}

void MainWindow::onAllMotorsReset()
{
  LogHandler::info(ui->textEditLog, "All motors have been reset to default");
}

void MainWindow::onVideoCapture(const QImage& image)
{
  m_lastCapturedFrame = image;
  if (image.isNull())
    return;

  // Scale the image to fit the label while maintaining aspect ratio
  QPixmap pixmap = QPixmap::fromImage(image).scaledToWidth(ui->labelCamera->width(), Qt::SmoothTransformation);
  ui->labelCamera->setPixmap(pixmap);
}

void MainWindow::onCameraStarted()
{
  LogHandler::success(ui->textEditLog, "Camera started successfully");
}

void MainWindow::onCameraStopped()
{
  LogHandler::warning(ui->textEditLog, "Camera stopped");
  ui->lineEditCameraName->clear();
  ui->labelCamera->clear();
}

void MainWindow::onCameraError(const QString& error)
{
  LogHandler::error(ui->textEditLog, "Camera Error: " + error);
}

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

void MainWindow::on_pushButtonCaptureImage_clicked()
{
  if (m_lastCapturedFrame.isNull()) {
    qDebug() << "No hay imagen disponible para capturar";
    return;
  }

  // Carpeta donde guardar las im�genes
  QString dirPath = QDir::currentPath() + "/images/calibration";
  QDir    dir(dirPath);
  if (!dir.exists()) {
    if (!dir.mkpath(".")) {
      qDebug() << "No se pudo crear la carpeta 'images/calibration'";
      return;
    }
    else {
      qDebug() << "Carpeta 'images/calibration' creada con �xito";
    }
  }

  // Buscar el siguiente n�mero disponible
  QStringList files     = dir.entryList(QStringList() << "image*.tif", QDir::Files);
  int         maxNumber = 0;

  for (const QString& file : files) {
    QString numStr = file;
    numStr.remove("image"); // quitar prefijo
    numStr.chop(4);         // quitar ".tif"
    bool ok;
    int  num = numStr.toInt(&ok);
    if (ok && num > maxNumber) {
      maxNumber = num;
    }
  }

  int     nextNumber = maxNumber + 1;
  QString fileName   = dirPath + "/image" + QString::number(nextNumber) + ".tif";

  // Guardar la imagen
  if (m_lastCapturedFrame.save(fileName)) {
    qDebug() << "Imagen capturada y guardada:" << fileName;
  }
  else {
    qDebug() << "No se pudo guardar la imagen";
  }
}
