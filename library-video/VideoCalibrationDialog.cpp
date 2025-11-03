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
#include <opencv2/imgproc.hpp>          // cv::cvtColor, cv::cornerSubPix

namespace fs = std::filesystem;

const QString DEFAULT_CALIB_DIR = "calibration"; // Mantenemos el nombre de carpeta que usa la lógica de guardado

VideoCalibrationDialog::VideoCalibrationDialog(QWidget* parent) : QDialog(parent), ui(new Ui::VideoCalibrationDialog)
{
  ui->setupUi(this);
  this->setWindowTitle("Camera Calibration");

  this->setWindowFlags(this->windowFlags() | Qt::WindowMinimizeButtonHint | Qt::WindowMaximizeButtonHint);

  VideoCaptureHandler& handler = VideoCaptureHandler::instance();

  // Conexión para recibir nuevos pixmaps capturados (Temporal mientras el
  // diálogo está abierto)
  connect(&handler, &VideoCaptureHandler::newPixmapCaptured, this, [=](const QPixmap& pixmap) {
    m_currentPixmap = pixmap;
    updateVideoLabel();
  });

  // Configurar el layout para la lista de archivos.
  // Es crucial para que los QLabels de los archivos se listen correctamente.
  QWidget* contentWidget = ui->scrollAreaWidgetContents;
  if (!contentWidget->layout()) {
    // Usar un QVBoxLayout para apilar las etiquetas de los archivos
    QVBoxLayout* layout = new QVBoxLayout(contentWidget);
    layout->setAlignment(Qt::AlignTop); // Alinear arriba
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
  // Desconectar la señal de newPixmapCaptured para que el QLabel del diálogo no
  // se actualice al cerrarse, dejando que MainWindow tome el control.
  disconnect(&VideoCaptureHandler::instance(), SIGNAL(newPixmapCaptured(QPixmap)), this, nullptr);
  delete ui;
}

void VideoCalibrationDialog::updateVideoLabel()
{
  if (m_currentPixmap.isNull()) {
    return;
  }
  ui->videoLabel->setPixmap(m_currentPixmap.scaled(ui->videoLabel->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
}

/**
 * @brief Abre el diálogo de selección de carpeta y actualiza la lista de archivos.
 */
void VideoCalibrationDialog::on_pushButtonSelectDirectory_clicked()
{
  // Abrir el diálogo para seleccionar un directorio
  QString newDirPath = QFileDialog::getExistingDirectory(this, tr("Seleccionar Carpeta para Calibración"), m_selectedDirectoryPath);

  if (!newDirPath.isEmpty()) {
    // Guardar la ruta seleccionada
    m_selectedDirectoryPath = newDirPath;
    // Actualizar la lista de archivos de la nueva carpeta
    updateFilesList();
  }
}

/**
 * @brief Guarda la imagen actual en la carpeta seleccionada.
 */
void VideoCalibrationDialog::on_pushButtonCaptureImage_clicked()
{
  if (m_selectedDirectoryPath.isEmpty()) {
    // Mostrar un aviso si no se ha seleccionado ninguna carpeta
    QMessageBox::warning(this, tr("Advertencia de Carpeta"), tr("Por favor, selecciona primero una carpeta de destino."));
    return;
  }

  if (m_currentPixmap.isNull()) {
    // Mostrar un aviso si no hay imagen (cámara no iniciada o no ha capturado aún)
    QMessageBox::warning(this, tr("Advertencia de Captura"), tr("No hay ninguna imagen de la cámara disponible para guardar."));
    return;
  }

  // Generar un nombre de archivo único con la fecha y hora
  QString timestamp = QDateTime::currentDateTime().toString("dd_hhmmss");
  QString fileName  = QString("capture_%1.tiff").arg(timestamp);
  QString filePath  = QDir(m_selectedDirectoryPath).filePath(fileName);

  // Guardar la imagen
  if (m_currentPixmap.save(filePath, "TIFF")) {
    // Éxito: Mostrar mensaje de confirmación
    ui->textEditInfo->append(tr("Captura guardada: %1").arg(fileName));
    // Actualizar la lista para incluir la nueva imagen
    updateFilesList();
  }
  else {
    // Error al guardar
    QMessageBox::critical(this, tr("Error de Guardado"), tr("No se pudo guardar la imagen en: %1").arg(filePath));
  }
}

/**
 * @brief Lee el contenido de la carpeta seleccionada y lo muestra en scrollAreaFiles como miniaturas.
 */
void VideoCalibrationDialog::updateFilesList()
{
  QWidget* contentWidget = ui->scrollAreaWidgetContents;
  QLayout* layout        = contentWidget->layout();

  // Si el layout no existe o no es un QGridLayout, lo creamos/reemplazamos.
  QGridLayout* gridLayout = qobject_cast<QGridLayout*>(layout);
  if (!gridLayout) {
    if (layout) {
      // Si es un layout diferente, lo limpiamos antes de reemplazarlo
      QLayoutItem* item;
      while ((item = layout->takeAt(0)) != nullptr) {
        delete item->widget();
        delete item;
      }
      delete layout;
    }
    gridLayout = new QGridLayout(contentWidget);
    gridLayout->setAlignment(Qt::AlignLeft | Qt::AlignTop); // Alinear de izquierda a arriba
    gridLayout->setSpacing(10);                             // Espacio entre miniaturas
  }

  // 1. Limpiar los widgets (miniaturas) actuales
  QLayoutItem* item;
  while ((item = gridLayout->takeAt(0)) != nullptr) {
    delete item->widget(); // Borrar el widget contenedor de la miniatura
    delete item;           // Borrar el QLayoutItem
  }

  // 2. Leer los archivos del directorio
  QDir        directory(m_selectedDirectoryPath);
  QStringList nameFilters;
  nameFilters << "*.png"
              << "*.jpg"
              << "*.jpeg"
              << "*.tiff";
  QFileInfoList fileList = directory.entryInfoList(nameFilters, QDir::Files, QDir::Name);

  // 3. Configuración para el diseño de cuadrícula
  const int THUMBNAIL_WIDTH = 120;                                             // Ancho fijo para la miniatura y su widget
  int       MAX_COLUMNS     = contentWidget->width() / (THUMBNAIL_WIDTH + 10); // Calcular dinámicamente el número de columnas
  MAX_COLUMNS               = qMax(1, MAX_COLUMNS);                            // Asegurarse de que haya al menos una columna
  int row                   = 0;
  int col                   = 0;

  // 4. Agregar las miniaturas y títulos
  for (const QFileInfo& fileInfo : fileList) {
    QString filePath = fileInfo.absoluteFilePath();

    // a. Cargar imagen y generar miniatura
    QPixmap originalPixmap(filePath);
    if (originalPixmap.isNull()) {
      // Si falla la carga, pasamos al siguiente archivo
      continue;
    }
    QPixmap thumbnail = originalPixmap.scaled(THUMBNAIL_WIDTH, THUMBNAIL_WIDTH, Qt::KeepAspectRatio, Qt::SmoothTransformation);

    // b. Crear el widget contenedor para cada miniatura
    QWidget*     itemWidget = new QWidget();
    QVBoxLayout* itemLayout = new QVBoxLayout(itemWidget);
    itemLayout->setAlignment(Qt::AlignCenter);
    itemLayout->setContentsMargins(0, 0, 0, 0);

    // c. QLabel para la miniatura
    QLabel* imageLabel = new QLabel();
    imageLabel->setPixmap(thumbnail);
    imageLabel->setAlignment(Qt::AlignCenter);
    imageLabel->setFixedSize(THUMBNAIL_WIDTH, THUMBNAIL_WIDTH); // Mantiene el tamaño para la consistencia

    // d. QLabel para el nombre del archivo
    QLabel* nameLabel = new QLabel(fileInfo.fileName());
    nameLabel->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    nameLabel->setFixedWidth(THUMBNAIL_WIDTH); // Restringir el ancho
    nameLabel->setWordWrap(true);              // Permitir que el nombre largo salte de línea

    // e. Ensamblar el widget
    itemLayout->addWidget(imageLabel);
    itemLayout->addWidget(nameLabel);

    // f. Agregar al QGridLayout
    gridLayout->addWidget(itemWidget, row, col);

    // g. Actualizar la posición de la cuadrícula
    col++;
    if (col >= MAX_COLUMNS) {
      col = 0;
      row++;
    }
  }

  // Asegurarse de que el QScrollArea actualice su contenido
  contentWidget->adjustSize();
}
// ----------------------------------------------------------------------
// MÉTODOS DE CALIBRACIÓN INTERNOS (Antes en CalibrationHandler)
// ----------------------------------------------------------------------

/**
 * @brief Crea los puntos 3D de referencia del tablero.
 */
std::vector<cv::Point3f> VideoCalibrationDialog::createObjectPoints() const
{
  std::vector<cv::Point3f> obj;
  for (int i = 0; i < m_calibrationBoardSize.height; ++i) {
    for (int j = 0; j < m_calibrationBoardSize.width; ++j) {
      obj.emplace_back(j * m_squareSize, i * m_squareSize, 0);
    }
  }
  return obj;
}

/**
 * @brief Carga una imagen y detecta las esquinas del tablero.
 * @return true si se encontraron las esquinas, false en caso contrario.
 */
bool VideoCalibrationDialog::processImageForCorners(const cv::Mat& image)
{
  std::vector<cv::Point2f> corners;
  bool found = cv::findChessboardCorners(image, m_calibrationBoardSize, corners, cv::CALIB_CB_ADAPTIVE_THRESH | cv::CALIB_CB_NORMALIZE_IMAGE);

  if (found) {
    cv::Mat gray;
    cv::cvtColor(image, gray, cv::COLOR_BGR2GRAY);
    cv::cornerSubPix(gray, corners, cv::Size(11, 11), cv::Size(-1, -1),
                     cv::TermCriteria(cv::TermCriteria::EPS + cv::TermCriteria::MAX_ITER, 30, 0.001));

    m_imagePoints.push_back(corners);
    m_objectPoints.push_back(createObjectPoints());

    // Se puede usar ui->textEditInfo->append("Esquinas detectadas...") si se desea
    return true;
  }
  return false;
}

/**
 * @brief Ejecuta el proceso de calibración de la cámara.
 * @return true si la calibración fue exitosa (con suficientes imágenes), false en caso contrario.
 */
bool VideoCalibrationDialog::runCalibration()
{
  if (m_imagePoints.size() < 5) {
    return false;
  }

  std::vector<cv::Mat> rvecs, tvecs;
  double               rms = cv::calibrateCamera(m_objectPoints, m_imagePoints, m_calibrationBoardSize, m_cameraMatrix, m_distCoeffs, rvecs, tvecs);

  if (!rvecs.empty()) {
    m_rvec = rvecs[0];
    m_tvec = tvecs[0];
  }

  // El valor RMS (error de reproyección) es un indicador clave
  qDebug() << "RMS error de calibración: " << rms;

  return true;
}

/**
 * @brief Guarda la matriz de cámara y los coeficientes de distorsión en archivos separados.
 */
void VideoCalibrationDialog::saveCalibration(const std::string& cameraMatrixFile, const std::string& distCoeffsFile) const
{
  // Crear carpeta "CalibrationResults" si no existe
  QDir().mkpath(DEFAULT_CALIB_DIR);

  // Rutas completas de los archivos
  std::string folderName       = DEFAULT_CALIB_DIR.toStdString();
  std::string cameraMatrixPath = QDir(DEFAULT_CALIB_DIR).filePath(cameraMatrixFile.c_str()).toStdString();
  std::string distCoeffsPath   = QDir(DEFAULT_CALIB_DIR).filePath(distCoeffsFile.c_str()).toStdString();

  // Guardar matriz de cámara
  cv::FileStorage fsCam(cameraMatrixPath, cv::FileStorage::WRITE);
  if (!fsCam.isOpened()) {
    qWarning() << "Error al abrir archivo para m_cameraMatrix:" << cameraMatrixPath.c_str();
    return;
  }
  fsCam << "m_cameraMatrix" << m_cameraMatrix;
  fsCam.release();

  // Guardar coeficientes de distorsión
  cv::FileStorage fsDist(distCoeffsPath, cv::FileStorage::WRITE);
  if (!fsDist.isOpened()) {
    qWarning() << "Error al abrir archivo para m_distCoeffs:" << distCoeffsPath.c_str();
    return;
  }
  fsDist << "m_distCoeffs" << m_distCoeffs;
  fsDist.release();
}

/**
 * @brief Carga las matrices de calibración. (Simplificamos para cargar solo las principales)
 */
bool VideoCalibrationDialog::loadCalibration(const std::string& camMatrixPath)
{
  cv::FileStorage fs(camMatrixPath, cv::FileStorage::READ);
  if (!fs.isOpened()) {
    return false;
  }

  fs["m_cameraMatrix"] >> m_cameraMatrix;
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

// ----------------------------------------------------------------------
// LÓGICA DEL DIÁLOGO (on_startButton_clicked y loadExistingCalibration)
// ----------------------------------------------------------------------

// ... (El constructor DEBE llamar a loadExistingCalibration al final) ...

/**
 * @brief Comprueba si existe un archivo de calibración y lo carga al inicio.
 */
void VideoCalibrationDialog::loadExistingCalibration()
{
  // Ruta del archivo de la matriz de cámara a buscar
  QString camMatrixFile = "camera_matrix.yml";
  QString camMatrixPath = QDir(DEFAULT_CALIB_DIR).filePath(camMatrixFile);

  if (QFile::exists(camMatrixPath)) {
    ui->textEditInfo->setText(tr("¡Calibración existente detectada! \nCargando datos..."));

    if (loadCalibration(camMatrixPath.toStdString())) {
      ui->textEditInfo->append(tr("\n--- DATOS DE CALIBRACIÓN CARGADOS ---"));

      // Mostrar Matriz de Cámara
      QString           camMatrixStr = "Matriz de Cámara:\n";
      std::stringstream ssCam;
      ssCam << m_cameraMatrix; // Usar el operador stream de OpenCV
      camMatrixStr += QString::fromStdString(ssCam.str());
      ui->textEditInfo->append(camMatrixStr);

      // Mostrar Coeficientes de Distorsión
      QString           distCoeffsStr = "Coeficientes de Distorsión:\n";
      std::stringstream ssDist;
      ssDist << m_distCoeffs; // Usar el operador stream de OpenCV
      distCoeffsStr += QString::fromStdString(ssDist.str());
      ui->textEditInfo->append(distCoeffsStr);
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
 * @brief Comprueba 5 imágenes, realiza la calibración y guarda los resultados.
 */
void VideoCalibrationDialog::on_startButton_clicked()
{
  // Lógica de validación (igual que antes)
  if (m_selectedDirectoryPath.isEmpty()) {
    QMessageBox::warning(this, tr("Advertencia"), tr("Por favor, selecciona una carpeta con imágenes de tablero de ajedrez primero."));
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
  ui->textEditInfo->append(tr("Iniciando calibración con %1 imágenes...").arg(fileList.size()));

  // 1. Limpiar los puntos de datos anteriores
  m_imagePoints.clear();
  m_objectPoints.clear();

  // 2. Cargar las imágenes y buscar esquinas
  int processedCount = 0;
  for (const QFileInfo& fileInfo : fileList) {
    cv::Mat image = cv::imread(fileInfo.absoluteFilePath().toStdString());
    if (!image.empty() && processImageForCorners(image)) {
      processedCount++;
    }
  }

  if (processedCount < 5) {
    ui->textEditInfo->append(tr("Solo se pudieron encontrar esquinas en %1 imágenes. La calibración no se realizará.").arg(processedCount));
    return;
  }
  ui->textEditInfo->append(tr("Esquinas detectadas correctamente en %1 imágenes.").arg(processedCount));

  // 3. Ejecutar la calibración
  if (runCalibration()) {
    // 4. Mostrar los resultados
    ui->textEditInfo->append(tr("\n--- RESULTADOS DE LA CALIBRACIÓN ---"));
    ui->textEditInfo->append(
      tr("Calibración Exitosa (RMS error: %1)")
        .arg((m_rvec.empty() ? 0.0 : m_rvec.at<double>(0)) // Valor de RMS no está disponible directamente, usamos el primer valor
                                                           // del vector de rotación como proxy si no se calculó el RMS al ejecutar
             ));

    // Mostrar Matriz de Cámara
    QString camMatrixStr = "Matriz de Cámara:\n";

    // Mostrar Coeficientes de Distorsión
    QString           distCoeffsStr = "Coeficientes de Distorsión:\n";
    std::stringstream ssDist;
    ssDist << m_distCoeffs; // Usar el operador stream de OpenCV
    distCoeffsStr += QString::fromStdString(ssDist.str());
    ui->textEditInfo->append(distCoeffsStr);

    // 5. Guardar los resultados en la carpeta "CalibrationResults"
    saveCalibration("camera_matrix.yml", "dist_coeffs.yml");

    ui->textEditInfo->append(tr("\nArchivos de calibración guardados en la carpeta '%1'.").arg(DEFAULT_CALIB_DIR));
  }
  else {
    ui->textEditInfo->append(tr("Falló la calibración. Se necesitan al menos 5 conjuntos de puntos válidos."));
  }
}