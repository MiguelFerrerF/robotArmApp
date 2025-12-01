#include "RobotCalibrationDialog.h"
#include "./ui_RobotCalibrationDialog.h"
// Headers de Qt
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

const QString DEFAULT_CALIB_DIR = "calibration/robot"; // Mantenemos el nombre de carpeta que usa la lógica de
                                                       // guardado

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
 * @brief Guarda la RT cámara-base.
 */
void RobotCalibrationWorker::saveCalibration(const std::string& RTcameraBase, const cv::Mat& RTcb) const
{
  QDir().mkpath(DEFAULT_CALIB_DIR);

  std::string RTcameraBasePath = QDir(DEFAULT_CALIB_DIR).filePath(RTcameraBase.c_str()).toStdString();

  qDebug() << "Guardando RT cámara-base en:" << RTcameraBasePath.c_str();

  // Guardar RT cámara-base
  cv::FileStorage fsCam(RTcameraBasePath, cv::FileStorage::WRITE);
  if (!fsCam.isOpened()) {
    qWarning() << "Error al abrir archivo para RTcameraBase:" << RTcameraBasePath.c_str();
    return;
  }
  fsCam << "RTcameraBase" << RTcb;
  fsCam.release();
}

void RobotCalibrationWorker::getRTbaseToolFromFile(const std::string& jsonFilePath, cv::Mat& Rbt, cv::Mat& Tbt)
{
  // Constantes de la geometría del robot
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
 * @brief Slot principal del worker: realiza la calibración.
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

  // Read camera matrix and distortion coefficients from previous
  // calibration
  if (!loadCalibration(result)) {
    emit calibrationError(tr("Error al cargar la calibración de la cámara. Asegúrese de que "
                             "los archivos existen y son válidos."));
    return;
  }

  int processedCount = 0;
  for (int index = 0; index < fileList.size(); ++index) {
    // Comprobar si el hilo debe detenerse
    if (QThread::currentThread()->isInterruptionRequested())
      return;

    cv::Mat image = cv::imread(fileList[index].absoluteFilePath().toStdString());
    if (image.empty()) {
      emit progressUpdate(tr("Error al cargar imagen: %1").arg(fileList[index].fileName()));
      continue;
    }

    if (processImageForCorners(image, boardSize, squareSize, imagePoints)) {
      processedCount++;

      // Calculamos la RT panel-cámara
      cv::Mat R, T;
      cv::solvePnP(objectPoints, imagePoints, result.cameraMatrix, result.distCoeffs, R, T);
      cv::Rodrigues(R, R); // Convertir a vector de rotación si es necesario
      Rpc.push_back(R);
      Tpc.push_back(T);

      // Calculamos la RT base-tool
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

    // Imprimir en consola (como tenías antes)
    std::cout << "Matriz de Cámara (Óptima):\\n" << result.cameraMatrix << std::endl;
    std::cout << "Coeficientes de Distorsión:\\n" << result.distCoeffs << std::endl;

    // Comprobar que se cargaron las TRES matrices
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

void RobotCalibrationDialog::updateVideoLabel()
{
  if (m_currentPixmap.isNull()) {
    return;
  }
  ui->videoLabel->setPixmap(m_currentPixmap.scaled(ui->videoLabel->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
}

void RobotCalibrationDialog::on_pushButtonSelectDirectory_clicked()
{
  QString newDirPath = QFileDialog::getExistingDirectory(this, tr("Seleccionar Carpeta para Calibración"), m_selectedDirectoryPath);

  if (!newDirPath.isEmpty()) {
    m_selectedDirectoryPath = newDirPath;
    updateFilesList();
  }
}

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

  // Guardar un archivo json con la configuración actual y el nombre de la
  // imagen
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

bool RobotCalibrationDialog::saveMotorAnglesToJson(const RobotConfig::RobotSettings& settings, const QString& filePath)
{
  QJsonObject rootObject;
  QJsonArray  motorsArray;

  // Asumiendo que el array motors tiene 6 elementos
  for (int i = 0; i < 6; ++i) {
    QJsonObject motorObject;
    motorObject["motorIndex"]   = i + 1;
    motorObject["currentAngle"] = settings.motors[i].currentAngle; // Ángulo actual del motor
    motorObject["defaultAngle"] = settings.motors[i].defaultAngle; // Angulo por defecto (offset)
    motorObject["fixedAngle"]   = settings.motors[i].fixedAngle;   // ángulo con el offset aplicado

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
 * @brief Carga las matrices de calibración.
 */
bool RobotCalibrationDialog::loadCalibration(const std::string& camMatrixPath)
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
void RobotCalibrationDialog::displayCalibrationResults(const cv::Mat& cameraMatrix, const cv::Mat& distCoeffs, const cv::Mat& newCameraMatrix,
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
void RobotCalibrationDialog::loadExistingCalibration()
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
void RobotCalibrationDialog::on_progressUpdate(const QString& message)
{
  ui->textEditInfo->append(message);
}

/**
 * @brief Slot para recibir errores del worker.
 */
void RobotCalibrationDialog::on_calibrationError(const QString& message)
{
  ui->textEditInfo->append(tr("\n--- ERROR DE CALIBRACIÓN ---"));
  ui->textEditInfo->append(message);
  ui->startButton->setEnabled(true); // Re-habilitar el botón
}

/**
 * @brief Slot para recibir los resultados finales del worker.
 */
void RobotCalibrationDialog::on_calibrationFinished(const RobotCalibrationResult& result)
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
