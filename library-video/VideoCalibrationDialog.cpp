#include "VideoCalibrationDialog.h"
#include "./ui_VideoCalibrationDialog.h"

// Headers de Qt
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QLabel>
#include <QMessageBox>
#include <QVBoxLayout>

// Headers de OpenCV y Standard
#include <filesystem>
#include <iostream>
#include <opencv2/calib3d.hpp>          // cv::findChessboardCorners, cv::calibrateCamera
#include <opencv2/core/mat.hpp>         // cv::Mat
#include <opencv2/core/persistence.hpp> // cv::FileStorage
#include <opencv2/core/types.hpp>       // cv::Size, cv::TermCriteria
#include <opencv2/imgcodecs.hpp>        // cv::imread
#include <opencv2/imgproc.hpp>          // cv::cvtColor, cv::cornerSubPix, getOptimalNewCameraMatrix

namespace fs = std::filesystem;

const QString DEFAULT_CALIB_DIR = "calibration/camera"; // Mantenemos el nombre de carpeta que usa la lógica de
                                                        // guardado

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

bool VideoCalibrationWorker::runCalibration(cv::Size boardSize, std::vector<std::vector<cv::Point2f>>& imagePoints,
                                            std::vector<std::vector<cv::Point3f>>& objectPoints, VideoCalibrationResult& result)
{
  if (imagePoints.size() < 5) {
    return false;
  }

  std::vector<cv::Mat> rvecs, tvecs;

  // 1. Definir Criterios de Terminación más estrictos
  cv::TermCriteria criteria(cv::TermCriteria::EPS + cv::TermCriteria::MAX_ITER, 100, 1e-6);

  // 2. Definir Banderas (Flags) de Calibración
  // int flags = cv::CALIB_FIX_ASPECT_RATIO | cv::CALIB_RATIONAL_MODEL | cv::CALIB_ZERO_TANGENT_DIST | cv::CALIB_USE_LU;
  int flags = cv::CALIB_USE_LU;

  // 3. Llamada a la función de calibración principal con Criterios y Banderas
  result.rms = cv::calibrateCamera(objectPoints, imagePoints, boardSize, result.cameraMatrix, result.distCoeffs, rvecs, tvecs, flags, criteria);

  // 4. Calcular la Matriz de Cámara Óptima
  result.newCameraMatrix = cv::getOptimalNewCameraMatrix(result.cameraMatrix, result.distCoeffs, boardSize, 1, boardSize, &result.roi);

  return true;
}

/**
 * @brief Guarda la matriz de cámara y los coeficientes de distorsión.
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
  fsCam << "m_newCameraMatrix" << newCameraMatrix; // <-- AÑADIDO
  fsCam.release();

  // Guardar coeficientes de distorsión
  cv::FileStorage fsDist(distCoeffsPath, cv::FileStorage::WRITE);
  if (!fsDist.isOpened()) {
    qWarning() << "Error al abrir archivo para m_distCoeffs:" << distCoeffsPath.c_str();
    return;
  }
  fsDist << "m_distCoeffs" << distCoeffs;
  fsDist.release();
}

/**
 * @brief Slot principal del worker: realiza la calibración.
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

  m_workerThread->start(); // Iniciar el hilo

  // Conexión para recibir nuevos pixmaps capturados (Temporal mientras el
  // diálogo está abierto)
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

void VideoCalibrationDialog::updateVideoLabel()
{
  if (m_currentPixmap.isNull()) {
    return;
  }
  ui->videoLabel->setPixmap(m_currentPixmap.scaled(ui->videoLabel->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
}

void VideoCalibrationDialog::on_pushButtonSelectDirectory_clicked()
{
  QString newDirPath = QFileDialog::getExistingDirectory(this, tr("Seleccionar Carpeta para Calibración"), m_selectedDirectoryPath);

  if (!newDirPath.isEmpty()) {
    m_selectedDirectoryPath = newDirPath;
    updateFilesList();
  }
}

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
 * @brief Carga las matrices de calibración.
 */
bool VideoCalibrationDialog::loadCalibration(const std::string& camMatrixPath)
{
  cv::FileStorage fs(camMatrixPath, cv::FileStorage::READ);
  if (!fs.isOpened()) {
    return false;
  }

  fs["m_cameraMatrix"] >> m_cameraMatrix;
  fs["m_newCameraMatrix"] >> m_newCameraMatrix; // <-- AÑADIDO
  fs.release();

  // También cargamos los coeficientes si es posible
  QString         distCoeffsPath = QDir(DEFAULT_CALIB_DIR).filePath("dist_coeffs.yml");
  cv::FileStorage fsDist(distCoeffsPath.toStdString(), cv::FileStorage::READ);
  if (fsDist.isOpened()) {
    fsDist["m_distCoeffs"] >> m_distCoeffs;
    fsDist.release();
  }

  return !m_cameraMatrix.empty();
}

/**
 * @brief Función auxiliar para mostrar los resultados de la calibración.
 */
void VideoCalibrationDialog::displayCalibrationResults(const cv::Mat& cameraMatrix, const cv::Mat& distCoeffs, const cv::Mat& newCameraMatrix,
                                                       double rms)
{
  // Mostrar Matriz de Cámara
  QString           camMatrixStr = "Matriz de Cámara (Original):\n";
  std::stringstream ssCam;
  ssCam << cameraMatrix;
  camMatrixStr += QString::fromStdString(ssCam.str());
  // Mostrar Matriz Óptima
  if (!newCameraMatrix.empty()) {
    ssCam.str(std::string()); // Limpiar el stringstream
    ssCam << newCameraMatrix;
    camMatrixStr += "\n\nMatriz de Cámara (Óptima):\n" + QString::fromStdString(ssCam.str());
  }
  ui->textEditInfo->append(camMatrixStr);

  // Mostrar Coeficientes de Distorsión
  QString           distCoeffsStr = "Coeficientes de Distorsión:\n";
  std::stringstream ssDist;
  ssDist << distCoeffs;
  distCoeffsStr += QString::fromStdString(ssDist.str());
  ui->textEditInfo->append(distCoeffsStr);

  // Mostrar RMS
  if (rms > 0.0)
    ui->textEditInfo->append(tr("Calibración Exitosa (RMS error: %1)").arg(rms));
}

/**
 * @brief Comprueba si existe un archivo de calibración y lo carga al inicio.
 */
void VideoCalibrationDialog::loadExistingCalibration()
{
  // Ruta del archivo de la matriz de cámara a buscar
  QString camMatrixFile = "camera_matrix.yml";
  QString camMatrixPath = QDir(DEFAULT_CALIB_DIR).filePath(camMatrixFile);

  if (QFile::exists(camMatrixPath)) {
    ui->textEditInfo->setText(tr("¡Calibración existente detectada!"));

    if (loadCalibration(camMatrixPath.toStdString())) {
      // Usamos la nueva función auxiliar para mostrar (con la nueva matriz)
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
 * @brief Inicia el proceso de calibración en el Worker Thread.
 */
void VideoCalibrationDialog::on_startButton_clicked()
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

  // 2. Bloquear la UI y limpiar
  ui->textEditInfo->clear();
  ui->startButton->setEnabled(false); // Deshabilitar el botón para evitar doble click

  // 3. Iniciar el trabajo en el hilo (NO BLOQUEANTE)
  QMetaObject::invokeMethod(m_worker, "doCalibration", Qt::QueuedConnection, Q_ARG(QString, m_selectedDirectoryPath),
                            Q_ARG(cv::Size, m_calibrationBoardSize), Q_ARG(float, m_squareSize));
}

/**
 * @brief Slot para recibir mensajes de progreso del worker.
 */
void VideoCalibrationDialog::on_progressUpdate(const QString& message)
{
  ui->textEditInfo->append(message);
}

/**
 * @brief Slot para recibir errores del worker.
 */
void VideoCalibrationDialog::on_calibrationError(const QString& message)
{
  ui->textEditInfo->append(tr("\n--- ERROR DE CALIBRACIÓN ---"));
  ui->textEditInfo->append(message);
  ui->startButton->setEnabled(true); // Re-habilitar el botón
}

/**
 * @brief Slot para recibir los resultados finales del worker.
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

// =========================================================
// 1. ESTRUCTURAS Y FUNCIONES AUXILIARES
// =========================================================

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

// Transforma un punto de coordenadas del Objeto (Tablero) a coordenadas de la Cámara
cv::Point3f transformPoint(const cv::Point3f& point, const cv::Mat& R_mat, const cv::Mat& T_vec)
{
  // Convertir punto a Matriz 3x1
  cv::Mat pointMat = (cv::Mat_<double>(3, 1) << point.x, point.y, point.z);

  // Fórmula: P_cam = R * P_obj + T
  cv::Mat resultMat = R_mat * pointMat + T_vec;

  return cv::Point3f(resultMat.at<double>(0), resultMat.at<double>(1), resultMat.at<double>(2));
}

// Genera un rayo desde un píxel de la imagen proyectado al espacio 3D
Ray generateRayFromPixel(const cv::Point2f& pixel, const cv::Mat& K)
{
  Ray ray;
  ray.origin = cv::Point3f(0, 0, 0); // El origen es el centro óptico de la cámara

  double fx = K.at<double>(0, 0);
  double fy = K.at<double>(1, 1);
  double cx = K.at<double>(0, 2);
  double cy = K.at<double>(1, 2);

  // Desproyectar el píxel a coordenadas normalizadas
  float x = (pixel.x - cx) / fx;
  float y = (pixel.y - cy) / fy;
  float z = 1.0f;

  // Normalizar el vector de dirección
  float norm    = std::sqrt(x * x + y * y + z * z);
  ray.direction = cv::Point3f(x / norm, y / norm, z / norm);

  return ray;
}

// Define un plano matemático a partir de 3 puntos en el espacio
Plane definePlaneFromPoints(const cv::Point3f& p1, const cv::Point3f& p2, const cv::Point3f& p3)
{
  Plane plane;
  plane.point = p1;

  // Vectores sobre el plano
  cv::Point3f v1 = p2 - p1;
  cv::Point3f v2 = p3 - p1;

  // Producto cruz para hallar la normal
  plane.normal = v1.cross(v2);

  // Normalizar la normal
  float norm = std::sqrt(plane.normal.x * plane.normal.x + plane.normal.y * plane.normal.y + plane.normal.z * plane.normal.z);
  if (norm > 0)
    plane.normal /= norm;

  return plane;
}

// Calcula la intersección entre un Rayo y un Plano
cv::Point3f intersectRayWithPlane(const Ray& ray, const Plane& plane)
{
  cv::Point3f diff  = plane.point - ray.origin;
  float       prod1 = diff.dot(plane.normal);
  float       prod2 = ray.direction.dot(plane.normal);

  // Evitar división por cero (rayo paralelo al plano)
  if (std::abs(prod2) < 1e-6) {
    qDebug() << "Advertencia: El rayo es paralelo al plano.";
    return cv::Point3f(0, 0, 0);
  }

  float t = prod1 / prod2;
  // P = O + t*D
  return ray.origin + ray.direction * t;
}

// =========================================================
// 2. TU FUNCIÓN PRINCIPAL
// =========================================================

// Nueva función para gestionar la carga "pesada" y persistencia
bool VideoCalibrationDialog::ensurePlaneCalibrationLoaded()
{
  if (m_isPlaneCalibrated)
    return true; // Ya está en RAM

  QSettings settings("TuEmpresa", "RobotApp");
  QString   dirPath = "calibration/camera";

  // --- A. CARGAR INTRÍNSECOS (Siempre necesario leer archivos YML) ---
  // Esto es rápido, no hace falta cachear en QSettings, pero sí en variables miembro
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

  // --- B. CARGAR EXTRÍNSECOS (PLANO) DESDE QSETTINGS ---
  // Verificamos si ya calculamos la pose del plano anteriormente
  if (settings.contains("Calibration/PlaneRvec_0") && settings.contains("Calibration/PlaneTvec_0")) {
    // Cargar desde QSettings (rápido)
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
    // --- C. CALCULAR EXTRÍNSECOS (Lento: Procesar Imagen TIFF) ---
    qDebug() << "Calculando calibración del plano desde imagen (Proceso pesado)...";

    QString camPlaneImagePath = QDir(dirPath).filePath("camera_plane_image.tiff");
    cv::Mat planeImage        = cv::imread(camPlaneImagePath.toStdString());
    if (planeImage.empty())
      return false;

    cv::Size                 boardSize(9, 6);
    float                    squareSize = 10.0f;
    std::vector<cv::Point2f> imagePoints;

    // Detectar esquinas
    bool found = cv::findChessboardCorners(planeImage, boardSize, imagePoints, cv::CALIB_CB_ADAPTIVE_THRESH | cv::CALIB_CB_NORMALIZE_IMAGE);
    if (!found)
      return false;

    // Subpix
    cv::Mat gray;
    cv::cvtColor(planeImage, gray, cv::COLOR_BGR2GRAY);
    cv::cornerSubPix(gray, imagePoints, cv::Size(11, 11), cv::Size(-1, -1),
                     cv::TermCriteria(cv::TermCriteria::EPS + cv::TermCriteria::MAX_ITER, 30, 0.001));

    // Puntos Objeto
    std::vector<cv::Point3f> objectPoints;
    for (int i = 0; i < boardSize.height; ++i) {
      for (int j = 0; j < boardSize.width; ++j) {
        objectPoints.emplace_back(j * squareSize, i * squareSize, 0);
      }
    }

    // SolvePnP
    cv::Mat rvec;
    cv::solvePnP(objectPoints, imagePoints, m_intrinsicK, m_distCoeffsD, rvec, m_planeT);
    cv::Rodrigues(rvec, m_planeR);

    // --- GUARDAR EN QSETTINGS ---
    for (int i = 0; i < 3; i++) {
      settings.setValue(QString("Calibration/PlaneRvec_%1").arg(i), rvec.at<double>(i));
      settings.setValue(QString("Calibration/PlaneTvec_%1").arg(i), m_planeT.at<double>(i));
    }
    qDebug() << "Nueva calibración de plano guardada en QSettings.";
  }

  // --- D. CARGAR MATRIZ ROBOT-CAMARA ---
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

// La función ligera que llama el Main (Refactorizada)
void VideoCalibrationDialog::calculateObjectPosition(QPoint centroid, QPoint pointRecta)
{
  // 1. Asegurar que tenemos los datos matemáticos cargados
  if (!ensurePlaneCalibrationLoaded()) {
    qDebug() << "Error: No se pudo cargar la calibración del plano.";
    return;
  }

  // A partir de aquí, todo es cálculo matemático puro (muy rápido)

  // --- LÓGICA DE INTERSECCIÓN 3D (Reutilizando m_planeR, m_planeT, m_intrinsicK) ---
  float squareSize = 10.0f;

  // Puntos del plano Z=0 en espacio objeto
  cv::Point3f p1_obj(0, 0, 0);
  cv::Point3f p2_obj(squareSize * 5, 0, 0);
  cv::Point3f p3_obj(0, squareSize * 5, 0);

  // Transformar a cámara usando caché
  cv::Point3f p1_cam = transformPoint(p1_obj, m_planeR, m_planeT);
  cv::Point3f p2_cam = transformPoint(p2_obj, m_planeR, m_planeT);
  cv::Point3f p3_cam = transformPoint(p3_obj, m_planeR, m_planeT);

  // Definir plano
  Plane plane = definePlaneFromPoints(p1_cam, p2_cam, p3_cam);

  // Ray casting con los puntos pasados por argumento
  cv::Point2f centroidCv(centroid.x(), centroid.y());
  cv::Point2f pointRectaCv(pointRecta.x(), pointRecta.y());

  Ray rayCentroid = generateRayFromPixel(centroidCv, m_intrinsicK);
  Ray rayPoint    = generateRayFromPixel(pointRectaCv, m_intrinsicK);

  // Intersección
  cv::Point3f result3D_centroid = intersectRayWithPlane(rayCentroid, plane);
  cv::Point3f result3D_point    = intersectRayWithPlane(rayPoint, plane);

  // Cinemática Inversa y Ángulo
  cv::Point3d centroid_inbase = getPiecePositionInBaseCoordinates(result3D_centroid, m_RTcb);
  cv::Point3d point_inbase    = getPiecePositionInBaseCoordinates(result3D_point, m_RTcb);

  // Emitir señal con la posición de la pieza
  emit piecePositionCalculated(centroid_inbase);

  // Calcular cinemática inversa
  if (m_robotHandlerInstance) {
    m_robotHandlerInstance->inverseCinematic(centroid_inbase);
  }

  // Calculo de ángulo
  cv::Point3d direction = point_inbase - centroid_inbase;
  cv::Point3d dir_norm  = direction / cv::norm(direction);
  cv::Point3d x_axis(1.0, 0.0, 0.0);

  double dot       = dir_norm.x * x_axis.x + dir_norm.y * x_axis.y + dir_norm.z * x_axis.z;
  double angle_rad = acos(dot);
  double angle_deg = angle_rad * 180.0 / CV_PI;

  // std::cout << "Angle respect X-axis calculated via external trigger: " << angle_deg << " degrees" << std::endl;
}

// =========================================================

cv::Point3d VideoCalibrationDialog::getPiecePositionInBaseCoordinates(const cv::Point3d& result3D, const cv::Mat& RTcb)
{
  cv::Mat pointCam = (cv::Mat_<double>(4, 1) << result3D.x, result3D.y, result3D.z, 1.0);

  cv::Mat     pointBase = RTcb * pointCam;
  cv::Point3d piecePosition;
  piecePosition.x = pointBase.at<double>(0);
  piecePosition.y = pointBase.at<double>(1);
  piecePosition.z = pointBase.at<double>(2);

  // offset
  piecePosition.z -= 65.0; // Ajuste de altura (en mm) según sea necesario
  piecePosition.x += 40.0; // Ajuste de posición X (en mm) según sea necesario
  piecePosition.y -= 0.0;  // Ajuste de posición Y (en mm) según sea necesario

  // Evitar valores de Z negativos
  if (piecePosition.z < -0.0) {
    piecePosition.z = 0.0;
  }
  // Evitar valores de Z mayores a 6mm
  if (piecePosition.z > 6.0) {
    piecePosition.z = 6.0;
  }

  // qDebug() << "Posición de la pieza en coordenadas de la base del robot:"
  //          << "(" << piecePosition.x << ", " << piecePosition.y << ", " << piecePosition.z << ")";
  return piecePosition;
}