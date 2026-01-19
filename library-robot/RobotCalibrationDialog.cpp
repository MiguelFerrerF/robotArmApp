/**
 * @file RobotCalibrationDialog.cpp
 * @author Miguel Ferrer
 * @brief  Dialog for Robot Calibration and Kinematics Manager.
 *
 * This file implements the RobotCalibrationDialog class, which provides
 * a user interface for robot calibration using chessboard patterns.
 * It also contains the RobotCalibrationWorker class that performs
 * the calibration computations in a separate thread.
 *
 * @version 0.1
 * @date 2026-01-19
 *
 * @copyright Copyright (c) 2026
 *
 */
#include "RobotCalibrationDialog.h"
#include "./ui_RobotCalibrationDialog.h"

#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QMessageBox>
#include <QVBoxLayout>

#include <filesystem>
#include <iostream>
#include <opencv2/calib3d.hpp>
#include <opencv2/core/mat.hpp>
#include <opencv2/core/persistence.hpp>
#include <opencv2/core/types.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

namespace fs = std::filesystem;

const QString DEFAULT_CALIB_DIR = "calibration/robot";

// ################################################################################################
//                             ROBOTCALIBRATIONWORKER
// ################################################################################################

/**
 * @brief Generates the 3D world coordinates for the chessboard corners.
 *
 * Creates a vector of 3D points assuming the board is located at Z=0 in the
 * calibration pattern's coordinate system. The points follow the sequence:
 * (0,0,0), (s,0,0), (2s,0,0)... where 's' is the square size.
 *
 * @param[in] boardSize Number of internal corners (width x height).
 * @param[in] squareSize Physical size of the square edge.
 * @return std::vector<cv::Point3f> Vector of 3D coordinates.
 */
std::vector<cv::Point3f> RobotCalibrationWorker::createObjectPoints(cv::Size boardSize, float squareSize) const
{
  std::vector<cv::Point3f> obj;
  for (int i = 0; i < boardSize.height; ++i) {
    for (int j = 0; j < boardSize.width; ++j) {
      obj.emplace_back(j * squareSize, i * squareSize, 0);
    }
  }
  return obj;
}

/**
 * @brief Detects and refines chessboard corners in the provided image.
 *
 * This function attempts to locate the internal corners of a chessboard pattern.
 * If the coarse corners are found, the function converts the image to grayscale
 * and refines the corner locations to sub-pixel accuracy for high-precision calibration.
 *
 * @param[in] image The source image to process (expected to be a BGR matrix).
 * @param[in] boardSize The dimensions of the board (number of internal corners per row and column).
 * @param[in] squareSize The physical size of a square side.
 * @param[out] corners A vector where the calculated floating-point coordinates of the corners will be stored.
 * @return true if the corners were successfully detected and refined; false otherwise.
 */
bool RobotCalibrationWorker::processImageForCorners(const cv::Mat& image, cv::Size boardSize, float squareSize, std::vector<cv::Point2f>& corners)
{
  bool found = cv::findChessboardCorners(image, boardSize, corners, cv::CALIB_CB_ADAPTIVE_THRESH | cv::CALIB_CB_NORMALIZE_IMAGE);

  if (found) {
    cv::Mat gray;
    cv::cvtColor(image, gray, cv::COLOR_BGR2GRAY);
    cv::cornerSubPix(gray, corners, cv::Size(11, 11), cv::Size(-1, -1),
                     cv::TermCriteria(cv::TermCriteria::EPS + cv::TermCriteria::MAX_ITER, 30, 0.001));
    return true;
  }
  return false;
}

/**
 * @brief Saves the camera-to-base transformation matrix to a file.
 *
 * This function creates the calibration directory if it does not exist,
 * then saves the provided transformation matrix (RTcameraBase) to a
 * specified file using OpenCV's FileStorage.
 *
 * @param[in] RTcameraBase The 4x4 transformation matrix from camera to base.
 * @param[in] RTcb The output file name where the matrix will be saved.
 */
void RobotCalibrationWorker::saveCalibration(const std::string& RTcameraBase, const cv::Mat& RTcb) const
{
  QDir().mkpath(DEFAULT_CALIB_DIR);

  std::string RTcameraBasePath = QDir(DEFAULT_CALIB_DIR).filePath(RTcameraBase.c_str()).toStdString();

  qDebug() << "Guardando RT cámara-base en:" << RTcameraBasePath.c_str();

  cv::FileStorage fsCam(RTcameraBasePath, cv::FileStorage::WRITE);
  if (!fsCam.isOpened()) {
    qWarning() << "Error al abrir archivo para RTcameraBase:" << RTcameraBasePath.c_str();
    return;
  }
  fsCam << "RTcameraBase" << RTcb;
  fsCam.release();
}

/**
 * @brief Computes the Base-to-Tool transformation matrix ($T_{base}^{tool}$) from a JSON log file.
 *
 * This function performs Forward Kinematics calculation. It reads the robot's motor angles
 * from the JSON file, converts them to radians, and applies the Denavit-Hartenberg (DH)
 * parameters specific to this robot geometry (a1, a2, a3, a5).
 *
 * The final transformation is computed as:
 * $RT_{bt} = RT_{35} \cdot RT_{23} \cdot RT_{12} \cdot RT_{b1}$
 *
 * @param[in] jsonFilePath Path to the JSON file containing 'motorAngles'.
 * @param[out] Rbt Output 3x3 rotation matrix (Base to Tool).
 * @param[out] Tbt Output 3x1 translation vector (Base to Tool).
 */
void RobotCalibrationWorker::getRTbaseToolFromFile(const std::string& jsonFilePath, cv::Mat& Rbt, cv::Mat& Tbt)
{
  double a1 = 130;
  double a2 = 125;
  double a3 = 125;
  double a5 = 130;

  QFile file(QString::fromStdString(jsonFilePath));
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    qWarning() << "No se pudo abrir el archivo JSON:" << QString::fromStdString(jsonFilePath);
    return;
  }

  QByteArray jsonData = file.readAll();
  file.close();

  QJsonDocument doc = QJsonDocument::fromJson(jsonData);
  if (!doc.isObject()) {
    qWarning() << "El archivo JSON no tiene un objeto raíz válido.";
    return;
  }

  QJsonObject rootObj     = doc.object();
  QJsonArray  motorAngles = rootObj["motorAngles"].toArray();

  // Inicializamos los ángulos
  double q1 = 0, q2 = 0, q3 = 0, q5 = 0;
  for (const QJsonValue& motorVal : motorAngles) {
    QJsonObject motorObj = motorVal.toObject();
    int         idx      = motorObj["motorIndex"].toInt();
    double      defAngle = motorObj["fixedAngle"].toDouble();
    if (idx == 1)
      q1 = defAngle;
    else if (idx == 2)
      q2 = defAngle;
    else if (idx == 3)
      q3 = defAngle;
    else if (idx == 5)
      q5 = defAngle;
  }

  qDebug() << "Ángulos leídos del JSON:"
           << "q1 =" << q1 << ", q2 =" << q2 << ", q3 =" << q3 << ", q5 =" << q5;

  // Convertimos a radianes y cambiamos el signo como en la versión original
  double q1_rad = -q1 * M_PI / 180.0;
  double q2_rad = -q2 * M_PI / 180.0;
  double q3_rad = -q3 * M_PI / 180.0;
  double q5_rad = -q5 * M_PI / 180.0;

  // RTb1 – Base al primer eslabón
  cv::Mat RTb1          = cv::Mat::eye(4, 4, CV_64F);
  RTb1.at<double>(0, 0) = cos(q1_rad);
  RTb1.at<double>(0, 1) = -sin(q1_rad);
  RTb1.at<double>(1, 0) = sin(q1_rad);
  RTb1.at<double>(1, 1) = cos(q1_rad);
  RTb1.at<double>(2, 3) = -a1;

  // RT12 – Primer eslabón al segundo
  cv::Mat RT12          = cv::Mat::eye(4, 4, CV_64F);
  RT12.at<double>(0, 0) = cos(q2_rad);
  RT12.at<double>(0, 2) = sin(q2_rad);
  RT12.at<double>(2, 3) = -a2;
  RT12.at<double>(2, 0) = -sin(q2_rad);
  RT12.at<double>(2, 2) = cos(q2_rad);

  // RT23 – Segundo al tercero
  cv::Mat RT23          = cv::Mat::eye(4, 4, CV_64F);
  RT23.at<double>(0, 0) = cos(q3_rad);
  RT23.at<double>(0, 2) = sin(q3_rad);
  RT23.at<double>(2, 3) = -a3;
  RT23.at<double>(2, 0) = -sin(q3_rad);
  RT23.at<double>(2, 2) = cos(q3_rad);

  // RT35 – Tercer eslabón al efector final
  cv::Mat RT35          = cv::Mat::eye(4, 4, CV_64F);
  RT35.at<double>(0, 0) = cos(q5_rad);
  RT35.at<double>(0, 2) = sin(q5_rad);
  RT35.at<double>(2, 3) = -a5;
  RT35.at<double>(2, 0) = -sin(q5_rad);
  RT35.at<double>(2, 2) = cos(q5_rad);

  // Transformación total
  cv::Mat RTbt = RT35 * RT23 * RT12 * RTb1;

  // Print de la matriz completa para debug
  qDebug() << "Matriz RT base-tool completa:";
  for (int i = 0; i < RTbt.rows; ++i) {
    QString rowStr;
    for (int j = 0; j < RTbt.cols; ++j) {
      rowStr += QString::number(RTbt.at<double>(i, j), 'f', 6) + '\t';
    }
    qDebug() << rowStr;
  }

  // Extraer submatrices
  Rbt = RTbt(cv::Rect(0, 0, 3, 3)).clone(); // 3x3 rotación
  Tbt = RTbt(cv::Rect(3, 0, 1, 3)).clone(); // 3x1 traslación

  // Print para debug
  qDebug() << "Matriz de Rotación Rbt:";
  for (int i = 0; i < Rbt.rows; ++i) {
    QString rowStr;
    for (int j = 0; j < Rbt.cols; ++j) {
      rowStr += QString::number(Rbt.at<double>(i, j), 'f', 6) + "\t";
    }
    qDebug() << rowStr;
  }

  qDebug() << "Vector de Traslación Tbt:";
  for (int i = 0; i < Tbt.rows; ++i) {
    QString rowStr;
    for (int j = 0; j < Tbt.cols; ++j) {
      rowStr += QString::number(Tbt.at<double>(i, j), 'f', 6) + "\t";
    }
    qDebug() << rowStr;
  }
}

/**
 * @brief Main execution slot for the calibration process.
 *
 * This function orchestrates the Hand-Eye calibration workflow:
 * 1. Scans the directory for matching pairs of Image (.tiff) and Data (.json) files.
 * 2. Loads existing camera intrinsics (Camera Matrix, Distortion) to ensure accurate PnP solving.
 * 3. Iterates through valid file pairs:
 * - Detects chessboard corners in the image.
 * - Solves the PnP problem to find the Pattern-to-Camera transform ($T_{pc}$).
 * - Calculates the Base-to-Tool transform ($T_{bt}$) from the JSON motor data.
 * 4. Accumulates these transforms into rotation/translation vectors.
 * 5. Uses `cv::calibrateHandEye` (TSAI method) to solve for the Camera-to-Base transform ($T_{cb}$).
 * 6. Saves the result and emits the finish signal.
 *
 * @param[in] directoryPath Directory containing the dataset.
 * @param[in] boardSize Chessboard dimensions.
 * @param[in] squareSize Square physical size.
 */
void RobotCalibrationWorker::doCalibration(const QString& directoryPath, cv::Size boardSize, float squareSize)
{
  QDir        directory(directoryPath);
  QStringList nameFilters;
  nameFilters << "*.tiff";
  QStringList nameJsonFilters;
  nameJsonFilters << "*.json";
  QFileInfoList fileList     = directory.entryInfoList(nameFilters, QDir::Files, QDir::Name);
  QFileInfoList jsonFileList = directory.entryInfoList(nameJsonFilters, QDir::Files, QDir::Name);

  if (fileList.size() < 5) {
    emit calibrationError(tr("Se necesitan al menos 5 imágenes válidas. Solo se encontraron %1.").arg(fileList.size()));
    return;
  }
  emit progressUpdate(tr("Iniciando calibración con %1 imágenes...").arg(fileList.size()));

  std::vector<cv::Mat>     Rpc, Rbt, Tpc, Tbt;
  std::vector<cv::Point2f> imagePoints;
  std::vector<cv::Point3f> objectPoints = createObjectPoints(boardSize, squareSize);
  RobotCalibrationResult   result;

  if (!loadCalibration(result)) {
    emit calibrationError(tr("Error al cargar la calibración de la cámara. Asegúrese de que "
                             "los archivos existen y son válidos."));
    return;
  }

  int processedCount = 0;
  for (int index = 0; index < fileList.size(); ++index) {
    if (QThread::currentThread()->isInterruptionRequested())
      return;

    cv::Mat image = cv::imread(fileList[index].absoluteFilePath().toStdString());
    if (image.empty()) {
      emit progressUpdate(tr("Error al cargar imagen: %1").arg(fileList[index].fileName()));
      continue;
    }

    if (processImageForCorners(image, boardSize, squareSize, imagePoints)) {
      processedCount++;

      cv::Mat R, T;
      cv::solvePnP(objectPoints, imagePoints, result.cameraMatrix, result.distCoeffs, R, T);
      cv::Rodrigues(R, R);
      Rpc.push_back(R);
      Tpc.push_back(T);

      cv::Mat RR, TT;
      getRTbaseToolFromFile(jsonFileList[index].absoluteFilePath().toStdString(), RR, TT);
      Rbt.push_back(RR);
      Tbt.push_back(TT);
      emit progressUpdate(tr("Procesando imagen: %1").arg(fileList[index].fileName()));
    }
  }

  result.processedCount = processedCount;

  if (processedCount < 5) {
    emit calibrationError(tr("Solo se pudieron encontrar esquinas en %1 "
                             "imágenes. La calibración no se realizará.")
                            .arg(processedCount));
    return;
  }

  emit progressUpdate(tr("Esquinas detectadas correctamente en %1 "
                         "imágenes.\nEjecutando calibración...")
                        .arg(processedCount));

  qDebug() << "Iniciando calibración hand-eye";

  // Calculamos la RT cámara-base usando calibración hand-eye
  cv::Mat Rcam2base, Tcam2base;
  cv::calibrateHandEye(Rbt, Tbt, Rpc, Tpc, Rcam2base, Tcam2base, cv::CALIB_HAND_EYE_TSAI);
  result.RTcb = cv::Mat::eye(4, 4, CV_64F);
  Rcam2base.copyTo(result.RTcb(cv::Rect(0, 0, 3, 3)));
  Tcam2base.copyTo(result.RTcb(cv::Rect(3, 0, 1, 3)));

  qDebug() << "Calibración hand-eye completada.";
  qDebug() << "Matriz RT cámara-base:";
  for (int i = 0; i < result.RTcb.rows; ++i) {
    QString rowStr;
    for (int j = 0; j < result.RTcb.cols; ++j) {
      rowStr += QString::number(result.RTcb.at<double>(i, j), 'f', 6) + '\t';
    }
    qDebug() << rowStr;
  }

  // Pasamos la RT cámara-base a saveCalibration
  saveCalibration("RT_camera_base.yml", result.RTcb);
  emit progressUpdate(tr("Archivos de calibración guardados en la carpeta '%1'.").arg(DEFAULT_CALIB_DIR));
  emit calibrationFinished(result);
}

/**
 * @brief Loads existing camera calibration data from files.
 *
 * This function attempts to read the camera matrix and distortion coefficients
 * from predefined YAML files. If successful, it populates the provided
 * RobotCalibrationResult structure with the loaded data.
 *
 * @param[out] result The structure to populate with the loaded calibration data.
 * @return true if the calibration data was successfully loaded; false otherwise.
 */
bool RobotCalibrationWorker::loadCalibration(RobotCalibrationResult& result)
{
  QString dirPath        = "calibration/camera";
  QString camMatrixPath  = QDir(dirPath).filePath("camera_matrix.yml");
  QString distCoeffsPath = QDir(dirPath).filePath("dist_coeffs.yml");

  qDebug() << "Cargando archivos de calibración desde:" << camMatrixPath << "y" << distCoeffsPath;

  cv::FileStorage fsCam(camMatrixPath.toStdString(), cv::FileStorage::READ);
  cv::FileStorage fsDist(distCoeffsPath.toStdString(), cv::FileStorage::READ);

  if (fsCam.isOpened() && fsDist.isOpened()) {
    fsCam["m_newCameraMatrix"] >> result.cameraMatrix;
    fsDist["m_distCoeffs"] >> result.distCoeffs;

    if (!result.cameraMatrix.empty() && !result.distCoeffs.empty()) {
      qDebug() << "Calibración cargada exitosamente.";
    }
    else {
      qWarning() << "No se pudieron leer todos los datos de los archivos de "
                    "calibración.";
      return false;
    }
  }
  else {
    qWarning() << "No se encontraron archivos de calibración. El vídeo no será "
                  "corregido.";
    return false;
  }

  fsCam.release();
  fsDist.release();
  return true;
}

// ################################################################################################
//                                     ROBOTCALIBRATIONDIALOG
// ################################################################################################

RobotCalibrationDialog::RobotCalibrationDialog(QWidget* parent, RobotConfig::RobotSettings* settings)
  : QDialog(parent), ui(new Ui::RobotCalibrationDialog), m_robotSettings(settings)
{
  ui->setupUi(this);
  this->setWindowTitle("Robot Calibration");

  this->setWindowFlags(this->windowFlags() | Qt::WindowMinimizeButtonHint | Qt::WindowMaximizeButtonHint);

  VideoCaptureHandler& handler = VideoCaptureHandler::instance();

  m_workerThread = new QThread(this);
  m_worker       = new RobotCalibrationWorker();
  m_worker->moveToThread(m_workerThread);

  connect(m_workerThread, &QThread::finished, m_worker, &QObject::deleteLater);
  connect(m_worker, &RobotCalibrationWorker::calibrationFinished, this, &RobotCalibrationDialog::on_calibrationFinished);
  connect(m_worker, &RobotCalibrationWorker::calibrationError, this, &RobotCalibrationDialog::on_calibrationError);
  connect(m_worker, &RobotCalibrationWorker::progressUpdate, this, &RobotCalibrationDialog::on_progressUpdate);

  m_workerThread->start();

  connect(&handler, &VideoCaptureHandler::newPixmapCaptured, this, [=](const QPixmap& pixmap) {
    m_currentPixmap = pixmap;
    updateVideoLabel();
  });

  QWidget* contentWidget = ui->scrollAreaWidgetContents;
  if (!contentWidget->layout()) {
    QVBoxLayout* layout = new QVBoxLayout(contentWidget);
    layout->setAlignment(Qt::AlignTop);
    layout->setContentsMargins(5, 5, 5, 5);
    layout->setSpacing(2);
  }

  loadExistingCalibration();
  m_selectedDirectoryPath = QDir::current().filePath(DEFAULT_CALIB_DIR);
  updateFilesList();
}

RobotCalibrationDialog::~RobotCalibrationDialog()
{
  disconnect(&VideoCaptureHandler::instance(), SIGNAL(newPixmapCaptured(QPixmap)), this, nullptr);

  if (m_workerThread && m_workerThread->isRunning()) {
    m_workerThread->requestInterruption();
    m_workerThread->wait(1000);
    if (m_workerThread->isRunning()) {
      m_workerThread->terminate();
      m_workerThread->wait();
    }
  }

  delete ui;
}

/**
 * @brief Updates the video label with the current pixmap.
 *
 * This function scales the current pixmap to fit the size of the video label
 * while maintaining the aspect ratio and using smooth transformation for better quality.
 */
void RobotCalibrationDialog::updateVideoLabel()
{
  if (m_currentPixmap.isNull()) {
    return;
  }
  ui->videoLabel->setPixmap(m_currentPixmap.scaled(ui->videoLabel->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
}

/**
 * @brief Slot triggered when the "Select Directory" button is clicked.
 *
 * This function opens a directory selection dialog, allowing the user to choose
 * a directory for calibration images. If a valid directory is selected, it updates
 * the internal state and refreshes the list of files displayed in the UI.
 */
void RobotCalibrationDialog::on_pushButtonSelectDirectory_clicked()
{
  QString newDirPath = QFileDialog::getExistingDirectory(this, tr("Seleccionar Carpeta para Calibración"), m_selectedDirectoryPath);

  if (!newDirPath.isEmpty()) {
    m_selectedDirectoryPath = newDirPath;
    updateFilesList();
  }
}

/**
 * @brief Captures the current camera frame and saves it along with robot configuration.
 *
 * Saves the current image as a TIFF file and creates a corresponding JSON file
 * containing the current angles of the robot motors. Both files share the same
 * timestamp to ensure they are processed as a pair during calibration.
 */
void RobotCalibrationDialog::on_pushButtonCaptureImage_clicked()
{
  if (m_selectedDirectoryPath.isEmpty()) {
    QMessageBox::warning(this, tr("Advertencia de Carpeta"), tr("Por favor, selecciona primero una carpeta de destino."));
    return;
  }

  if (m_currentPixmap.isNull()) {
    QMessageBox::warning(this, tr("Advertencia de Captura"), tr("No hay ninguna imagen de la cámara disponible para guardar."));
    return;
  }

  QString timestamp = QDateTime::currentDateTime().toString("dd_hhmmss");
  QString fileName  = QString("capture_%1.tiff").arg(timestamp);
  QString filePath  = QDir(m_selectedDirectoryPath).filePath(fileName);

  if (m_currentPixmap.save(filePath, "TIFF")) {
    ui->textEditInfo->append(tr("Captura guardada: %1").arg(fileName));
    updateFilesList();
  }
  else {
    QMessageBox::critical(this, tr("Error de Guardado"), tr("No se pudo guardar la imagen en: %1").arg(filePath));
  }

  if (m_robotSettings) {
    QString jsonFileName = QString("capture_%1.json").arg(timestamp);
    QString jsonFilePath = QDir(m_selectedDirectoryPath).filePath(jsonFileName);

    if (saveMotorAnglesToJson(*m_robotSettings, jsonFilePath)) {
      ui->textEditInfo->append(tr("Configuración de ángulos guardada: %1").arg(jsonFileName));
    }
    else {
      QMessageBox::critical(this, tr("Error de Guardado de Configuración"), tr("No se pudo guardar la configuración JSON en: %1").arg(jsonFilePath));
    }
  }
}

/**
 * @brief Serializes the robot's motor settings to a JSON file.
 *
 * Stores current angle, default angle (offset), and fixed angle for all 6 axes.
 * This data is required later to reconstruct the robot's physical pose via Forward Kinematics.
 *
 * @param[in] settings The current robot settings structure.
 * @param[in] filePath The destination path for the .json file.
 * @return true if the file was written successfully.
 */
bool RobotCalibrationDialog::saveMotorAnglesToJson(const RobotConfig::RobotSettings& settings, const QString& filePath)
{
  QJsonObject rootObject;
  QJsonArray  motorsArray;

  for (int i = 0; i < 6; ++i) {
    QJsonObject motorObject;
    motorObject["motorIndex"]   = i + 1;
    motorObject["currentAngle"] = settings.motors[i].currentAngle;
    motorObject["defaultAngle"] = settings.motors[i].defaultAngle;
    motorObject["fixedAngle"]   = settings.motors[i].fixedAngle;

    motorsArray.append(motorObject);
  }

  rootObject["timestamp"]   = QDateTime::currentDateTime().toString(Qt::ISODate);
  rootObject["motorAngles"] = motorsArray;

  QJsonDocument doc(rootObject);
  QFile         file(filePath);

  if (file.open(QIODevice::WriteOnly)) {
    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();
    return true;
  }

  qWarning() << "Error al abrir o escribir archivo JSON:" << filePath;
  return false;
}

/**
 * @brief Updates the visual file list with thumbnails.
 *
 * clear the current grid layout and regenerates it by creating thumbnails
 * for all supported image files in the selected directory.
 * Used to give the user visual feedback on how many samples have been collected.
 */
void RobotCalibrationDialog::updateFilesList()
{
  QWidget* contentWidget = ui->scrollAreaWidgetContents;
  QLayout* layout        = contentWidget->layout();

  QGridLayout* gridLayout = qobject_cast<QGridLayout*>(layout);
  if (!gridLayout) {
    if (layout) {
      QLayoutItem* item;
      while ((item = layout->takeAt(0)) != nullptr) {
        delete item->widget();
        delete item;
      }
      delete layout;
    }
    gridLayout = new QGridLayout(contentWidget);
    gridLayout->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    gridLayout->setSpacing(10);
  }

  QLayoutItem* item;
  while ((item = gridLayout->takeAt(0)) != nullptr) {
    delete item->widget();
    delete item;
  }

  QDir        directory(m_selectedDirectoryPath);
  QStringList nameFilters;
  nameFilters << "*.png"
              << "*.jpg"
              << "*.jpeg"
              << "*.tiff";
  QFileInfoList fileList = directory.entryInfoList(nameFilters, QDir::Files, QDir::Name);

  const int THUMBNAIL_WIDTH = 120;
  int       MAX_COLUMNS     = contentWidget->width() / (THUMBNAIL_WIDTH + 10);
  MAX_COLUMNS               = qMax(1, MAX_COLUMNS);
  int row                   = 0;
  int col                   = 0;

  for (const QFileInfo& fileInfo : fileList) {
    QString filePath = fileInfo.absoluteFilePath();

    QPixmap originalPixmap(filePath);
    if (originalPixmap.isNull()) {
      continue;
    }
    QPixmap thumbnail = originalPixmap.scaled(THUMBNAIL_WIDTH, THUMBNAIL_WIDTH, Qt::KeepAspectRatio, Qt::SmoothTransformation);

    QWidget*     itemWidget = new QWidget();
    QVBoxLayout* itemLayout = new QVBoxLayout(itemWidget);
    itemLayout->setAlignment(Qt::AlignCenter);
    itemLayout->setContentsMargins(0, 0, 0, 0);

    QLabel* imageLabel = new QLabel();
    imageLabel->setPixmap(thumbnail);
    imageLabel->setAlignment(Qt::AlignCenter);
    imageLabel->setFixedSize(THUMBNAIL_WIDTH, THUMBNAIL_WIDTH);

    QLabel* nameLabel = new QLabel(fileInfo.fileName());
    nameLabel->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    nameLabel->setFixedWidth(THUMBNAIL_WIDTH);
    nameLabel->setWordWrap(true);

    itemLayout->addWidget(imageLabel);
    itemLayout->addWidget(nameLabel);

    gridLayout->addWidget(itemWidget, row, col);

    col++;
    if (col >= MAX_COLUMNS) {
      col = 0;
      row++;
    }
  }

  contentWidget->adjustSize();
}

/**
 * @brief Loads camera calibration data from a specified file.
 *
 * This function reads the camera matrix and distortion coefficients
 * from a YAML file. It populates the member variables m_cameraMatrix
 * and m_distCoeffs accordingly.
 *
 * @param[in] camMatrixPath The path to the camera matrix YAML file.
 * @return true if the calibration data was successfully loaded; false otherwise.
 */
bool RobotCalibrationDialog::loadCalibration(const std::string& camMatrixPath)
{
  cv::FileStorage fs(camMatrixPath, cv::FileStorage::READ);
  if (!fs.isOpened()) {
    return false;
  }

  fs["m_cameraMatrix"] >> m_cameraMatrix;
  fs["m_newCameraMatrix"] >> m_newCameraMatrix;
  fs.release();

  QString         distCoeffsPath = QDir(DEFAULT_CALIB_DIR).filePath("dist_coeffs.yml");
  cv::FileStorage fsDist(distCoeffsPath.toStdString(), cv::FileStorage::READ);
  if (fsDist.isOpened()) {
    fsDist["m_distCoeffs"] >> m_distCoeffs;
    fsDist.release();
  }

  return !m_cameraMatrix.empty();
}

/**
 * @brief Helper function to format and display calibration matrices in the text log.
 *
 * This function formats the camera matrix, distortion coefficients,
 * and optimized camera matrix (if provided) into a readable string
 * and appends it to the text edit widget for user visibility.
 *
 * @param[in] cameraMatrix The original intrinsic matrix.
 * @param[in] distCoeffs The distortion coefficients.
 * @param[in] newCameraMatrix The optimized/new intrinsic matrix.
 * @param[in] rms The Root Mean Square error of the calibration (0.0 if not available).
 */
void RobotCalibrationDialog::displayCalibrationResults(const cv::Mat& cameraMatrix, const cv::Mat& distCoeffs, const cv::Mat& newCameraMatrix,
                                                       double rms)
{
  QString           camMatrixStr = "Matriz de Cámara (Original):\n";
  std::stringstream ssCam;
  ssCam << cameraMatrix;
  camMatrixStr += QString::fromStdString(ssCam.str());

  if (!newCameraMatrix.empty()) {
    ssCam.str(std::string());
    ssCam << newCameraMatrix;
    camMatrixStr += "\n\nMatriz de Cámara (Óptima):\n" + QString::fromStdString(ssCam.str());
  }
  ui->textEditInfo->append(camMatrixStr);

  QString           distCoeffsStr = "Coeficientes de Distorsión:\n";
  std::stringstream ssDist;
  ssDist << distCoeffs;
  distCoeffsStr += QString::fromStdString(ssDist.str());
  ui->textEditInfo->append(distCoeffsStr);

  if (rms > 0.0)
    ui->textEditInfo->append(tr("Calibración Exitosa (RMS error: %1)").arg(rms));
}

/**
 * @brief Loads existing calibration data if available and displays it.
 *
 * This function checks for the presence of a camera matrix file in the
 * default calibration directory. If found, it loads the calibration data
 * and displays it in the text edit widget.
 */
void RobotCalibrationDialog::loadExistingCalibration()
{
  QString camMatrixFile = "camera_matrix.yml";
  QString camMatrixPath = QDir(DEFAULT_CALIB_DIR).filePath(camMatrixFile);

  if (QFile::exists(camMatrixPath)) {
    ui->textEditInfo->setText(tr("¡Calibración existente detectada!"));

    if (loadCalibration(camMatrixPath.toStdString())) {
      displayCalibrationResults(m_cameraMatrix, m_distCoeffs, m_newCameraMatrix, 0.0);
    }
    else {
      ui->textEditInfo->append(tr("Advertencia: No se pudo cargar la calibración."));
    }
  }
  else {
    ui->textEditInfo->setText(tr("No se ha encontrado ninguna calibración previa en la carpeta '%1'.").arg(DEFAULT_CALIB_DIR));
  }
}

/**
 * @brief Slot triggered when the "Start Calibration" button is clicked.
 *
 * This function performs initial validations on the selected directory
 * and the number of images available. If valid, it clears the log,
 * disables the start button to prevent multiple clicks, and invokes
 * the calibration process in a separate thread.
 */
void RobotCalibrationDialog::on_startButton_clicked()
{
  // 1. Validaciones
  if (m_selectedDirectoryPath.isEmpty()) {
    QMessageBox::warning(this, tr("Advertencia"),
                         tr("Por favor, selecciona una carpeta con imágenes de "
                            "tablero de ajedrez primero."));
    return;
  }

  QDir        directory(m_selectedDirectoryPath);
  QStringList nameFilters;
  nameFilters << "*.png"
              << "*.jpg"
              << "*.jpeg"
              << "*.tiff";
  QFileInfoList fileList = directory.entryInfoList(nameFilters, QDir::Files, QDir::Name);

  if (fileList.size() < 5) {
    QMessageBox::warning(this, tr("Advertencia"), tr("Se necesitan al menos 5 imágenes válidas. Solo se encontraron %1.").arg(fileList.size()));
    return;
  }

  ui->textEditInfo->clear();
  ui->startButton->setEnabled(false);
  ui->textEditInfo->append(tr("Iniciando proceso de calibración..."));
  QMetaObject::invokeMethod(m_worker, "doCalibration", Qt::QueuedConnection, Q_ARG(QString, m_selectedDirectoryPath),
                            Q_ARG(cv::Size, m_calibrationBoardSize), Q_ARG(float, m_squareSize));
}

/**
 * @brief Slot to receive progress updates from the worker.
 *
 * Appends the received message to the text edit widget for user visibility.
 *
 * @param[in] message The progress message to display.
 */
void RobotCalibrationDialog::on_progressUpdate(const QString& message)
{
  ui->textEditInfo->append(message);
}

/**
 * @brief Slot to receive error messages from the worker.
 *
 * Appends the received error message to the text edit widget and
 * re-enables the start button for retrying.
 *
 * @param[in] message The error message to display.
 */
void RobotCalibrationDialog::on_calibrationError(const QString& message)
{
  ui->textEditInfo->append(tr("\n--- ERROR DE CALIBRACIÓN ---"));
  ui->textEditInfo->append(message);
  ui->startButton->setEnabled(true);
}

/**
 * @brief Slot triggered when the calibration process is finished.
 *
 * This function saves the calibration results locally, displays
 * the results in the text edit widget, and re-enables the start button.
 *
 * @param[in] result The result structure containing calibration data.
 */
void RobotCalibrationDialog::on_calibrationFinished(const RobotCalibrationResult& result)
{
  m_cameraMatrix    = result.cameraMatrix;
  m_distCoeffs      = result.distCoeffs;
  m_newCameraMatrix = result.newCameraMatrix;

  displayCalibrationResults(result.cameraMatrix, result.distCoeffs, result.newCameraMatrix, result.rms);
  ui->textEditInfo->append(tr("\nProceso de calibración finalizado."));
  ui->startButton->setEnabled(true);
}
