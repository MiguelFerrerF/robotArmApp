/**
 * @file VideoCalibrationDialog.cpp
 * @author Miguel Ferrer
 * @brief   Dialog for Camera Calibration and 3D Localization Manager.
 *
 * This file implements the VideoCalibrationDialog class, which provides
 * a user interface for camera calibration using chessboard patterns.
 * It also contains the VideoCalibrationWorker class that performs
 * the calibration computations in a separate thread.
 *
 * @version 0.1
 * @date 2026-01-19
 *
 * @copyright Copyright (c) 2026
 *
 */
#include "VideoCalibrationDialog.h"
#include "./ui_VideoCalibrationDialog.h"

#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
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

const QString DEFAULT_CALIB_DIR = "calibration/camera";

// ################################################################################################
//                             VIDEOCALIBRATIONWORKER
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
std::vector<cv::Point3f> VideoCalibrationWorker::createObjectPoints(cv::Size boardSize, float squareSize) const
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
 * @brief Detects chessboard corners in the provided image.
 *
 * Converts the image to grayscale, detects chessboard corners, and refines
 * their positions to sub-pixel accuracy.
 *
 * @param[in] image Input image in which to detect corners.
 * @param[in] boardSize Number of internal corners (width x height).
 * @param[in] squareSize Physical size of the square edge.
 * @param[out] corners Detected corner points.
 * @return true if corners were found, false otherwise.
 */
bool VideoCalibrationWorker::processImageForCorners(const cv::Mat& image, cv::Size boardSize, float squareSize, std::vector<cv::Point2f>& corners)
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
 * @brief Performs camera calibration using detected image and object points.
 *
 * Wraps around OpenCV's `cv::calibrateCamera` function to compute the camera
 * matrix and distortion coefficients.
 *
 * @param[in] boardSize Size of the images used for calibration.
 * @param[in] imagePoints 2D points detected in the images.
 * @param[in] objectPoints Corresponding 3D points in the world coordinate system.
 * @param[out] result Struct to store calibration results.
 * @return true if calibration was successful, false otherwise.
 */
bool VideoCalibrationWorker::runCalibration(cv::Size boardSize, std::vector<std::vector<cv::Point2f>>& imagePoints,
                                            std::vector<std::vector<cv::Point3f>>& objectPoints, VideoCalibrationResult& result)
{
  if (imagePoints.size() < 5) {
    return false;
  }

  std::vector<cv::Mat> rvecs, tvecs;

  cv::TermCriteria criteria(cv::TermCriteria::EPS + cv::TermCriteria::MAX_ITER, 100, 1e-6);

  int flags = cv::CALIB_USE_LU;

  result.rms = cv::calibrateCamera(objectPoints, imagePoints, boardSize, result.cameraMatrix, result.distCoeffs, rvecs, tvecs, flags, criteria);
  result.newCameraMatrix = cv::getOptimalNewCameraMatrix(result.cameraMatrix, result.distCoeffs, boardSize, 1, boardSize, &result.roi);

  return true;
}

/**
 * @brief Saves the calibration results to YAML files.
 *
 * Stores the camera matrix, distortion coefficients, and new camera matrix
 * in separate YAML files within the default calibration directory.
 *
 * @param[in] cameraMatrixFile Filename for the camera matrix.
 * @param[in] distCoeffsFile Filename for the distortion coefficients.
 * @param[in] cameraMatrix Computed camera matrix.
 * @param[in] distCoeffs Computed distortion coefficients.
 * @param[in] newCameraMatrix Computed optimized camera matrix.
 */
void VideoCalibrationWorker::saveCalibration(const std::string& cameraMatrixFile, const std::string& distCoeffsFile, const cv::Mat& cameraMatrix,
                                             const cv::Mat& distCoeffs, const cv::Mat& newCameraMatrix) const
{
  QDir().mkpath(DEFAULT_CALIB_DIR);

  std::string cameraMatrixPath = QDir(DEFAULT_CALIB_DIR).filePath(cameraMatrixFile.c_str()).toStdString();
  std::string distCoeffsPath   = QDir(DEFAULT_CALIB_DIR).filePath(distCoeffsFile.c_str()).toStdString();

  // Guardar matriz de cámara
  cv::FileStorage fsCam(cameraMatrixPath, cv::FileStorage::WRITE);
  if (!fsCam.isOpened()) {
    qWarning() << "Error al abrir archivo para m_cameraMatrix:" << cameraMatrixPath.c_str();
    return;
  }
  fsCam << "m_cameraMatrix" << cameraMatrix;
  fsCam << "m_newCameraMatrix" << newCameraMatrix;
  fsCam.release();

  cv::FileStorage fsDist(distCoeffsPath, cv::FileStorage::WRITE);
  if (!fsDist.isOpened()) {
    qWarning() << "Error al abrir archivo para m_distCoeffs:" << distCoeffsPath.c_str();
    return;
  }
  fsDist << "m_distCoeffs" << distCoeffs;
  fsDist.release();
}

/**
 * @brief Main calibration routine executed in a separate thread.
 *
 * Scans the specified directory for .tiff images, detects chessboard corners,
 * and performs camera calibration. Emits signals to report progress, errors,
 * and completion.
 *
 * @param[in] directoryPath Path containing the .tiff images.
 * @param[in] boardSize Logical dimensions of the board (inner corners).
 * @param[in] squareSize Physical size of squares (used to define the object coordinate scale).
 */
void VideoCalibrationWorker::doCalibration(const QString& directoryPath, cv::Size boardSize, float squareSize)
{
  QDir        directory(directoryPath);
  QStringList nameFilters;
  nameFilters << "*.tiff";
  QFileInfoList fileList = directory.entryInfoList(nameFilters, QDir::Files, QDir::Name);

  if (fileList.size() < 5) {
    emit calibrationError(tr("Se necesitan al menos 5 imágenes válidas. Solo se encontraron %1.").arg(fileList.size()));
    return;
  }
  emit progressUpdate(tr("Iniciando calibración con %1 imágenes...").arg(fileList.size()));

  std::vector<std::vector<cv::Point2f>> imagePoints;
  std::vector<std::vector<cv::Point3f>> objectPoints;
  VideoCalibrationResult                result;

  // Necesitamos el tamaño de la imagen para getOptimalNewCameraMatrix ---
  cv::Size imageSize;

  int processedCount = 0;
  for (const QFileInfo& fileInfo : fileList) {
    // Comprobar si el hilo debe detenerse
    if (QThread::currentThread()->isInterruptionRequested())
      return;

    cv::Mat image = cv::imread(fileInfo.absoluteFilePath().toStdString());
    if (image.empty()) {
      emit progressUpdate(tr("Error al cargar imagen: %1").arg(fileInfo.fileName()));
      continue;
    }

    // Guardar el tamaño de la primera imagen
    if (processedCount == 0) {
      imageSize = image.size();
      qDebug() << "Tamaño de imagen detectado para calibración:" << imageSize.width << "x" << imageSize.height;
    }
    std::vector<cv::Point2f> cornersImg;
    std::vector<cv::Point3f> cornersObj = createObjectPoints(boardSize, squareSize);
    if (processImageForCorners(image, boardSize, squareSize, cornersImg)) {
      processedCount++;
      imagePoints.push_back(cornersImg);
      objectPoints.push_back(cornersObj);
      emit progressUpdate(tr("Procesando imagen: %1").arg(fileInfo.fileName()));
    }
  }

  result.processedCount = processedCount;

  if (processedCount < 5) {
    emit calibrationError(tr("Solo se pudieron encontrar esquinas en %1 imágenes. La calibración no se realizará.").arg(processedCount));
    return;
  }

  emit progressUpdate(tr("Esquinas detectadas correctamente en %1 imágenes.\nEjecutando calibración...").arg(processedCount));

  // Pasamos el imageSize a runCalibration
  if (runCalibration(imageSize, imagePoints, objectPoints, result)) {
    // Pasamos la newCameraMatrix a saveCalibration
    saveCalibration("camera_matrix.yml", "dist_coeffs.yml", result.cameraMatrix, result.distCoeffs, result.newCameraMatrix);
    emit progressUpdate(tr("Archivos de calibración guardados en la carpeta '%1'.").arg(DEFAULT_CALIB_DIR));
    emit calibrationFinished(result);
  }
  else
    emit calibrationError(tr("Falló la calibración. Se necesitan al menos 5 conjuntos de puntos válidos."));
}

// ################################################################################################
//                             VIDEOCALIBRATIONDIALOG
// ################################################################################################

VideoCalibrationDialog::VideoCalibrationDialog(QWidget* parent, RobotHandler* robotHandlerInstance)
  : QDialog(parent), ui(new Ui::VideoCalibrationDialog), m_robotHandlerInstance(robotHandlerInstance)
{
  ui->setupUi(this);
  this->setWindowTitle("Camera Calibration");

  this->setWindowFlags(this->windowFlags() | Qt::WindowMinimizeButtonHint | Qt::WindowMaximizeButtonHint);

  VideoCaptureHandler& handler = VideoCaptureHandler::instance();

  m_workerThread = new QThread(this);
  m_worker       = new VideoCalibrationWorker();
  m_worker->moveToThread(m_workerThread);

  connect(m_workerThread, &QThread::finished, m_worker, &QObject::deleteLater);
  connect(m_worker, &VideoCalibrationWorker::calibrationFinished, this, &VideoCalibrationDialog::on_calibrationFinished);
  connect(m_worker, &VideoCalibrationWorker::calibrationError, this, &VideoCalibrationDialog::on_calibrationError);
  connect(m_worker, &VideoCalibrationWorker::progressUpdate, this, &VideoCalibrationDialog::on_progressUpdate);

  m_workerThread->start();

  connect(&handler, &VideoCaptureHandler::newPixmapCaptured, this, [=](const QPixmap& pixmap) {
    m_currentPixmap = pixmap;
    updateVideoLabel();
  });

  // Configurar el layout para la lista de archivos.
  QWidget* contentWidget = ui->scrollAreaWidgetContents;
  if (!contentWidget->layout()) {
    QVBoxLayout* layout = new QVBoxLayout(contentWidget);
    layout->setAlignment(Qt::AlignTop);
    layout->setContentsMargins(5, 5, 5, 5);
    layout->setSpacing(2);
  }

  // Cargar calibración existente si está disponible
  loadExistingCalibration();
  // Inicializar la ruta de la carpeta de calibración
  m_selectedDirectoryPath = QDir::current().filePath(DEFAULT_CALIB_DIR);
  updateFilesList();
}

VideoCalibrationDialog::~VideoCalibrationDialog()
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
 * @brief Updates the video label with the latest captured pixmap.
 *
 * Scales the current pixmap to fit the label while maintaining aspect ratio.
 * Uses smooth transformation for better quality.
 */
void VideoCalibrationDialog::updateVideoLabel()
{
  if (m_currentPixmap.isNull()) {
    return;
  }
  ui->videoLabel->setPixmap(m_currentPixmap.scaled(ui->videoLabel->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
}

/**
 * @brief Slot triggered when the "Select Directory" button is clicked.
 *
 * Opens a directory selection dialog and updates the selected directory path.
 * Refreshes the file list to show images from the new directory.
 */
void VideoCalibrationDialog::on_pushButtonSelectDirectory_clicked()
{
  QString newDirPath = QFileDialog::getExistingDirectory(this, tr("Seleccionar Carpeta para Calibración"), m_selectedDirectoryPath);

  if (!newDirPath.isEmpty()) {
    m_selectedDirectoryPath = newDirPath;
    updateFilesList();
  }
}

/**
 * @brief Slot triggered when the "Capture Image" button is clicked.
 *
 * Saves the current pixmap from the video feed to the selected directory
 * with a timestamped filename. Updates the file list upon successful save.
 */
void VideoCalibrationDialog::on_pushButtonCaptureImage_clicked()
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
}

/**
 * @brief Updates the list of image files displayed in the scroll area.
 *
 * Scans the selected directory for image files and creates thumbnails
 * for each image. Arranges thumbnails in a grid layout within the scroll area.
 */
void VideoCalibrationDialog::updateFilesList()
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
 * @brief Loads camera calibration data from a YAML file.
 *
 * Reads the camera matrix and distortion coefficients from the specified
 * YAML file and stores them in member variables.
 *
 * @param[in] camMatrixPath Path to the YAML file containing calibration data.
 * @return true if loading was successful, false otherwise.
 */
bool VideoCalibrationDialog::loadCalibration(const std::string& camMatrixPath)
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
 * @brief Displays the calibration results in the text edit.
 *
 * Formats and appends the camera matrix, distortion coefficients,
 * and RMS error to the text edit widget.
 *
 * @param[in] cameraMatrix Original camera matrix.
 * @param[in] distCoeffs Distortion coefficients.
 * @param[in] newCameraMatrix Optimized camera matrix.
 * @param[in] rms Root Mean Square error of the calibration.
 */
void VideoCalibrationDialog::displayCalibrationResults(const cv::Mat& cameraMatrix, const cv::Mat& distCoeffs, const cv::Mat& newCameraMatrix,
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
 * @brief Loads existing calibration data if available.
 *
 * Checks for the presence of calibration files in the default directory
 * and loads them if found. Displays the loaded calibration results.
 */
void VideoCalibrationDialog::loadExistingCalibration()
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
 * Validates the selected directory and initiates the calibration process
 * by invoking the worker's `doCalibration` method in a separate thread.
 */
void VideoCalibrationDialog::on_startButton_clicked()
{
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

  QMetaObject::invokeMethod(m_worker, "doCalibration", Qt::QueuedConnection, Q_ARG(QString, m_selectedDirectoryPath),
                            Q_ARG(cv::Size, m_calibrationBoardSize), Q_ARG(float, m_squareSize));
}

/**
 * @brief  Slot to handle progress updates from the calibration worker.
 *
 * Appends progress messages to the text edit widget.
 *
 * @param[in] message Progress message to display.
 */
void VideoCalibrationDialog::on_progressUpdate(const QString& message)
{
  ui->textEditInfo->append(message);
}

/**
 * @brief Slot to handle calibration errors from the worker.
 *
 * Appends error messages to the text edit widget and re-enables
 * the start button.
 *
 * @param[in] message Error message to display.
 */
void VideoCalibrationDialog::on_calibrationError(const QString& message)
{
  ui->textEditInfo->append(tr("\n--- ERROR DE CALIBRACIÓN ---"));
  ui->textEditInfo->append(message);
  ui->startButton->setEnabled(true); // Re-habilitar el botón
}

/**
 * @brief Slot to handle the completion of the calibration process.
 *
 * Saves the calibration results locally, displays them in the text edit,
 * and re-enables the start button.
 *
 * @param[in] result Struct containing the calibration results.
 */
void VideoCalibrationDialog::on_calibrationFinished(const VideoCalibrationResult& result)
{
  // 1. Guardar localmente (para la carga la próxima vez)
  m_cameraMatrix    = result.cameraMatrix;
  m_distCoeffs      = result.distCoeffs;
  m_newCameraMatrix = result.newCameraMatrix;

  // 2. Mostrar los resultados
  displayCalibrationResults(result.cameraMatrix, result.distCoeffs, result.newCameraMatrix, result.rms);
  ui->textEditInfo->append(tr("\nProceso de calibración finalizado."));

  // 3. Re-habilitar el botón
  ui->startButton->setEnabled(true);
}

struct Ray
{
  cv::Point3f origin;
  cv::Point3f direction;
};
struct Plane
{
  cv::Point3f normal;
  cv::Point3f point;
};

/**
 * @brief Transforms a 3D point using rotation and translation.
 *
 * Applies the transformation defined by the rotation matrix and translation vector
 * to the given 3D point.
 *
 * @param[in] point The 3D point to transform.
 * @param[in] R_mat Rotation matrix (3x3).
 * @param[in] T_vec Translation vector (3x1).
 * @return Transformed 3D point.
 */
cv::Point3f transformPoint(const cv::Point3f& point, const cv::Mat& R_mat, const cv::Mat& T_vec)
{
  cv::Mat pointMat  = (cv::Mat_<double>(3, 1) << point.x, point.y, point.z);
  cv::Mat resultMat = R_mat * pointMat + T_vec;

  return cv::Point3f(resultMat.at<double>(0), resultMat.at<double>(1), resultMat.at<double>(2));
}

/**
 * @brief Back-projects a 2D pixel into a 3D ray.
 * * Uses the Camera Intrinsic Matrix (K)  to convert pixel coordinates $(u, v)$
 * into a normalized direction vector $(x, y, 1)$ in camera space.
 * * @param pixel The 2D point on the image plane.
 * @param K The 3x3 Intrinsic Matrix.
 * @return Ray Origin (0,0,0) and normalized Direction.
 */
Ray generateRayFromPixel(const cv::Point2f& pixel, const cv::Mat& K)
{
  Ray ray;
  ray.origin = cv::Point3f(0, 0, 0);

  double fx = K.at<double>(0, 0);
  double fy = K.at<double>(1, 1);
  double cx = K.at<double>(0, 2);
  double cy = K.at<double>(1, 2);

  float x = (pixel.x - cx) / fx;
  float y = (pixel.y - cy) / fy;
  float z = 1.0f;

  float norm    = std::sqrt(x * x + y * y + z * z);
  ray.direction = cv::Point3f(x / norm, y / norm, z / norm);

  return ray;
}

/**
 * @brief   Defines a plane from three 3D points.
 *
 * Calculates the normal vector of the plane using the cross product
 * of two vectors formed by the three points.
 * It uses the points provided by the chessboard corners in 3D space.
 *
 * @param p1 First point
 * @param p2 Second point
 * @param p3 Third point
 * @return Plane defined by the three points. (point and normal)
 */
Plane definePlaneFromPoints(const cv::Point3f& p1, const cv::Point3f& p2, const cv::Point3f& p3)
{
  Plane plane;
  plane.point = p1;

  cv::Point3f v1 = p2 - p1;
  cv::Point3f v2 = p3 - p1;

  plane.normal = v1.cross(v2);

  float norm = std::sqrt(plane.normal.x * plane.normal.x + plane.normal.y * plane.normal.y + plane.normal.z * plane.normal.z);
  if (norm > 0)
    plane.normal /= norm;

  return plane;
}

/**
 * @brief   Computes the intersection of a ray with a plane.
 *
 * Uses the parametric equation of the ray and the plane equation to find
 * the intersection point.
 *
 * @param ray The ray defined by an origin and direction.
 * @param plane The plane defined by a point and normal vector.
 * @return The intersection point in 3D space.
 */
cv::Point3f intersectRayWithPlane(const Ray& ray, const Plane& plane)
{
  cv::Point3f diff  = plane.point - ray.origin;
  float       prod1 = diff.dot(plane.normal);
  float       prod2 = ray.direction.dot(plane.normal);

  if (std::abs(prod2) < 1e-6) {
    qDebug() << "Advertencia: El rayo es paralelo al plano.";
    return cv::Point3f(0, 0, 0);
  }

  float t = prod1 / prod2;
  return ray.origin + ray.direction * t;
}

/**
 * @brief Ensures all necessary geometric matrices are loaded into memory.
 *
 * Implements a **Optimization Strategy**:
 * 1. **RAM Check:** If `m_isPlaneCalibrated` is true, returns immediately.
 * 2. **Cache Check:** Looks in `QSettings` ("Calibration/PlaneRvec...") for previously calculated plane pose.
 * 3. **Calculation (Fallback):** If no cache exists, it loads `camera_plane_image.tiff`,
 * detects corners, runs `solvePnP` to find the plane, and saves the result to `QSettings`.
 *
 * This avoids the heavy image processing step on every user click.
 */
bool VideoCalibrationDialog::ensurePlaneCalibrationLoaded()
{
  if (m_isPlaneCalibrated)
    return true;

  QSettings settings("TuEmpresa", "RobotApp");
  QString   dirPath = "calibration/camera";

  if (m_intrinsicK.empty()) {
    QString camMatrixPath  = QDir(dirPath).filePath("camera_matrix.yml");
    QString distCoeffsPath = QDir(dirPath).filePath("dist_coeffs.yml");

    cv::FileStorage fsCam(camMatrixPath.toStdString(), cv::FileStorage::READ);
    if (fsCam.isOpened()) {
      fsCam["m_newCameraMatrix"] >> m_intrinsicK;
      fsCam.release();
    }
    else
      return false;

    cv::FileStorage fsDist(distCoeffsPath.toStdString(), cv::FileStorage::READ);
    if (fsDist.isOpened()) {
      fsDist["m_distCoeffs"] >> m_distCoeffsD;
      fsDist.release();
    }
    else
      return false;
  }

  if (settings.contains("Calibration/PlaneRvec_0") && settings.contains("Calibration/PlaneTvec_0")) {
    cv::Mat rvec = cv::Mat::zeros(3, 1, CV_64F);
    m_planeT     = cv::Mat::zeros(3, 1, CV_64F);

    for (int i = 0; i < 3; i++) {
      rvec.at<double>(i)     = settings.value(QString("Calibration/PlaneRvec_%1").arg(i)).toDouble();
      m_planeT.at<double>(i) = settings.value(QString("Calibration/PlaneTvec_%1").arg(i)).toDouble();
    }
    cv::Rodrigues(rvec, m_planeR);
    qDebug() << "Calibración del plano cargada desde QSettings.";
  }
  else {
    qDebug() << "Calculando calibración del plano desde imagen (Proceso pesado)...";

    QString camPlaneImagePath = QDir(dirPath).filePath("camera_plane_image.tiff");
    cv::Mat planeImage        = cv::imread(camPlaneImagePath.toStdString());
    if (planeImage.empty())
      return false;

    cv::Size                 boardSize(9, 6);
    float                    squareSize = 10.0f;
    std::vector<cv::Point2f> imagePoints;

    bool found = cv::findChessboardCorners(planeImage, boardSize, imagePoints, cv::CALIB_CB_ADAPTIVE_THRESH | cv::CALIB_CB_NORMALIZE_IMAGE);
    if (!found)
      return false;

    cv::Mat gray;
    cv::cvtColor(planeImage, gray, cv::COLOR_BGR2GRAY);
    cv::cornerSubPix(gray, imagePoints, cv::Size(11, 11), cv::Size(-1, -1),
                     cv::TermCriteria(cv::TermCriteria::EPS + cv::TermCriteria::MAX_ITER, 30, 0.001));

    std::vector<cv::Point3f> objectPoints;
    for (int i = 0; i < boardSize.height; ++i) {
      for (int j = 0; j < boardSize.width; ++j) {
        objectPoints.emplace_back(j * squareSize, i * squareSize, 0);
      }
    }

    cv::Mat rvec;
    cv::solvePnP(objectPoints, imagePoints, m_intrinsicK, m_distCoeffsD, rvec, m_planeT);
    cv::Rodrigues(rvec, m_planeR);

    for (int i = 0; i < 3; i++) {
      settings.setValue(QString("Calibration/PlaneRvec_%1").arg(i), rvec.at<double>(i));
      settings.setValue(QString("Calibration/PlaneTvec_%1").arg(i), m_planeT.at<double>(i));
    }
    qDebug() << "Nueva calibración de plano guardada en QSettings.";
  }

  if (m_RTcb.empty()) {
    QString         RTcameraBasePath = "calibration/robot/RT_camera_base.yml";
    cv::FileStorage fsRT(RTcameraBasePath.toStdString(), cv::FileStorage::READ);
    if (fsRT.isOpened()) {
      fsRT["RTcameraBase"] >> m_RTcb;
      fsRT.release();
    }
    else {
      qDebug() << "Error: No se pudo cargar RT_camera_base.yml";
      return false;
    }
  }

  m_isPlaneCalibrated = true;
  return true;
}

/**
 * @brief Core logic for 3D Object Localization.
 *
 * This function bridges the 2D Vision domain and the 3D Robot domain .
 *
 * **Workflow:**
 * 1. **Load Geometry:** Ensures Intrinsic matrix (K), Plane Pose (R, T), and Hand-Eye matrix ($RT_{cb}$) are ready.
 * 2. **Define Plane:** Reconstructs the mathematical plane of the work surface relative to the camera.
 * 3. **Ray Casting:** Casts rays from the `centroid` and `pointRecta` pixels.
 * 4. **Intersection:** Finds where these rays hit the work table (Z=0 in object space).
 * 5. **Transformation:** Converts the intersection points from Camera Frame to Robot Base Frame.
 * 6. **Orientation:** Calculates the angle of the object vector relative to the X-axis.
 * 7. **Execution:** Emits the position signal and triggers Inverse Kinematics on the RobotHandler.
 *
 * @param centroid Pixel coordinates of the object center.
 * @param pointRecta Pixel coordinates indicating the object's orientation.
 */
void VideoCalibrationDialog::calculateObjectPosition(QPoint centroid, QPoint pointRecta)
{
  if (!ensurePlaneCalibrationLoaded()) {
    qDebug() << "Error: No se pudo cargar la calibración del plano.";
    return;
  }

  float squareSize = 10.0f;

  cv::Point3f p1_obj(0, 0, 0);
  cv::Point3f p2_obj(squareSize * 5, 0, 0);
  cv::Point3f p3_obj(0, squareSize * 5, 0);

  cv::Point3f p1_cam = transformPoint(p1_obj, m_planeR, m_planeT);
  cv::Point3f p2_cam = transformPoint(p2_obj, m_planeR, m_planeT);
  cv::Point3f p3_cam = transformPoint(p3_obj, m_planeR, m_planeT);

  Plane plane = definePlaneFromPoints(p1_cam, p2_cam, p3_cam);

  cv::Point2f centroidCv(centroid.x(), centroid.y());
  cv::Point2f pointRectaCv(pointRecta.x(), pointRecta.y());

  Ray rayCentroid = generateRayFromPixel(centroidCv, m_intrinsicK);
  Ray rayPoint    = generateRayFromPixel(pointRectaCv, m_intrinsicK);

  cv::Point3f result3D_centroid = intersectRayWithPlane(rayCentroid, plane);
  cv::Point3f result3D_point    = intersectRayWithPlane(rayPoint, plane);

  cv::Point3d centroid_inbase = getPiecePositionInBaseCoordinates(result3D_centroid, m_RTcb);
  cv::Point3d point_inbase    = getPiecePositionInBaseCoordinates(result3D_point, m_RTcb);

  emit piecePositionCalculated(centroid_inbase);

  if (m_robotHandlerInstance) {
    m_robotHandlerInstance->inverseCinematic(centroid_inbase);
  }

  cv::Point3d direction = point_inbase - centroid_inbase;
  cv::Point3d dir_norm  = direction / cv::norm(direction);
  cv::Point3d x_axis(1.0, 0.0, 0.0);

  // double dot       = dir_norm.x * x_axis.x + dir_norm.y * x_axis.y + dir_norm.z * x_axis.z;
  // double angle_rad = acos(dot);
  // double angle_deg = angle_rad * 180.0 / CV_PI;
}

/**
 * @brief Applies the Coordinate Transformation Chain: Camera -> Base.
 *
 * \f$ P_{base} = RT_{cb} \times P_{cam} \f$
 *
 * Also applies specific physical offsets (hardcoded adjustments) to fine-tune
 * the final gripping position (e.g., Z-height safety limits).
 *
 * @param result3D The 3D point in the Camera coordinate system.
 * @param RTcb The 4x4 Homogeneous Transformation Matrix (Camera to Base).
 * @return cv::Point3d The final target point for the robot.
 */
cv::Point3d VideoCalibrationDialog::getPiecePositionInBaseCoordinates(const cv::Point3d& result3D, const cv::Mat& RTcb)
{
  cv::Mat pointCam = (cv::Mat_<double>(4, 1) << result3D.x, result3D.y, result3D.z, 1.0);

  cv::Mat     pointBase = RTcb * pointCam;
  cv::Point3d piecePosition;
  piecePosition.x = pointBase.at<double>(0);
  piecePosition.y = pointBase.at<double>(1);
  piecePosition.z = pointBase.at<double>(2);

  piecePosition.z -= 65.0; // Adjust Z position (in mm) for gripping height
  piecePosition.x += 40.0; // Adjust X position (in mm)
  piecePosition.y -= 0.0;  // Adjust Y position (in mm)

  if (piecePosition.z < -0.0) {
    piecePosition.z = 0.0;
  }
  if (piecePosition.z > 6.0) {
    piecePosition.z = 6.0;
  }
  return piecePosition;
}