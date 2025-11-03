#include "VideoCalibrationDialog.h"
#include "./ui_VideoCalibrationDialog.h"
#include <QDateTime>   // ¡Necesario para nombrar los archivos de imagen!
#include <QDir>        // ¡Necesario para listar los archivos!
#include <QFileDialog> // ¡Necesario para el diálogo de selección de carpeta!
#include <QLabel>      // ¡Necesario para mostrar los nombres de los archivos!
#include <QMessageBox> // ¡Necesario para los mensajes de aviso!
#include <QVBoxLayout> // ¡Necesario para organizar la lista de archivos!

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
}

VideoCalibrationDialog::~VideoCalibrationDialog()
{
  // Desconectar la señal de newPixmapCaptured para que el QLabel del diálogo no
  // se actualice al cerrarse, dejando que MainWindow tome el control.
  disconnect(&VideoCaptureHandler::instance(), SIGNAL(newPixmapCaptured(QPixmap)), this, nullptr);
  delete ui;
}

void VideoCalibrationDialog::on_startButton_clicked()
{
  VideoCaptureHandler& handler = VideoCaptureHandler::instance();
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
  QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_hhmmsszzz");
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
 * @brief Lee el contenido de la carpeta seleccionada y lo muestra en scrollAreaFiles.
 */
void VideoCalibrationDialog::updateFilesList()
{
  // 1. Limpiar la lista actual
  QWidget* contentWidget = ui->scrollAreaWidgetContents;
  QLayout* layout        = contentWidget->layout();
  if (layout) {
    QLayoutItem* item;
    // Eliminar todos los widgets del layout
    while ((item = layout->takeAt(0)) != nullptr) {
      delete item->widget(); // Borrar el widget (QLabel)
      delete item;           // Borrar el QLayoutItem
    }
  }
  else {
    // Esto no debería pasar si se inicializó correctamente en el constructor,
    // pero es una comprobación de seguridad.
    return;
  }

  // 2. Leer los archivos del directorio
  QDir directory(m_selectedDirectoryPath);
  // Filtrar solo archivos con ciertas extensiones (por ejemplo, imágenes)
  QStringList nameFilters;
  nameFilters << "*.tiff"
              << "*.jpg"
              << "*.jpeg"
              << "*.png";
  QFileInfoList fileList = directory.entryInfoList(nameFilters, QDir::Files, QDir::Name);

  // 3. Agregar los nombres de los archivos como QLabels
  for (const QFileInfo& fileInfo : fileList) {
    QLabel* fileLabel = new QLabel(fileInfo.fileName());
    layout->addWidget(fileLabel);
  }

  // Asegurarse de que el layout se actualice
  contentWidget->update();
}
