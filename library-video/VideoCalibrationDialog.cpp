#include "VideoCalibrationDialog.h"
#include "./ui_VideoCalibrationDialog.h"
#include <QDateTime>   // ¡Necesario para nombrar los archivos de imagen!
#include <QDir>        // ¡Necesario para listar los archivos!
#include <QFileDialog> // ¡Necesario para el diálogo de selección de carpeta!
#include <QGridLayout> // Para la nueva organización
#include <QIcon>
#include <QImage>
#include <QLabel>      // ¡Necesario para mostrar los nombres de los archivos!
#include <QMessageBox> // ¡Necesario para los mensajes de aviso!
#include <QPixmap>
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
