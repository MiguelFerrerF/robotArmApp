#include "VideoProcessingDialog.h"
#include "./ui_VideoProcessingDialog.h"
#include <QCameraDevice>
#include <QMediaDevices>
#include <QMessageBox>
#include <QtMath>

VideoProcessingDialog::VideoProcessingDialog(QWidget* parent) : QDialog(parent), ui(new Ui::VideoProcessingDialog)
{
  ui->setupUi(this);
  this->setWindowTitle("Camera Manager");

  this->setWindowFlags(this->windowFlags() | Qt::WindowMinimizeButtonHint | Qt::WindowMaximizeButtonHint);

  VideoCaptureHandler& handler = VideoCaptureHandler::instance();

  // Conexión para recibir nuevos pixmaps capturados (Temporal mientras el
  // diálogo está abierto)
  connect(&handler, &VideoCaptureHandler::newPixmapCaptured, this, [=](const QPixmap& pixmap) {
    m_currentPixmap = pixmap;
    updateVideoLabel();
  });

  // Conexiones de soporte (Necesarias para configurar la UI)
  connect(&handler, &VideoCaptureHandler::propertiesSupported, this, &VideoProcessingDialog::on_propertiesSupported);
  connect(&handler, &VideoCaptureHandler::rangesSupported, this, &VideoProcessingDialog::on_rangesSupported);
  connect(&handler, &VideoCaptureHandler::cameraOpenFailed, this, &VideoProcessingDialog::on_cameraOpenFailed);

  // Rellenar ComboBox de cámaras
  QStringList cameraNames;
  for (const QCameraDevice& camera : QMediaDevices::videoInputs()) {
    cameraNames << camera.description();
  }
  ui->comboBoxCameras->addItems(cameraNames);

  if (cameraNames.isEmpty()) {
    ui->startButton->setEnabled(false);
    ui->videoLabel->setText("No se han detectado cámaras.");
  }

  // Si ya hay una cámara corriendo, cargamos su estado en la UI.
  updateStartButtonState();

  setAllControlsEnabled(false);
}

VideoProcessingDialog::~VideoProcessingDialog()
{
  // Desconectar la señal de newPixmapCaptured para que el QLabel del diálogo no
  // se actualice al cerrarse, dejando que MainWindow tome el control.
  disconnect(&VideoCaptureHandler::instance(), SIGNAL(newPixmapCaptured(QPixmap)), this, nullptr);
  delete ui;
}

// --- NUEVO: Actualiza el botón de Start/Stop al abrir el diálogo ---
void VideoProcessingDialog::updateStartButtonState()
{
  bool isRunning = VideoCaptureHandler::instance().isCameraRunning();
  ui->startButton->setChecked(isRunning);
  ui->startButton->setText(isRunning ? "Stop" : "Start");
  ui->comboBoxCameras->setEnabled(!isRunning);
  ui->comboBoxResolution->setEnabled(!isRunning);

  if (isRunning) {
    ui->videoLabel->setText(""); // Limpiar el texto si hay vídeo
  }
}

void VideoProcessingDialog::on_startButton_clicked()
{
  VideoCaptureHandler& handler = VideoCaptureHandler::instance();
  if (ui->startButton->isChecked()) {

    int     cameraId   = ui->comboBoxCameras->currentIndex();
    QString resText    = ui->comboBoxResolution->currentText();
    QSize   resolution = parseResolution(resText);

    handler.requestCameraChange(cameraId, resolution);
    handler.setCameraName(ui->comboBoxCameras->currentText().toStdString());

    ui->startButton->setText("Stop");
    ui->comboBoxCameras->setEnabled(false);
    ui->comboBoxResolution->setEnabled(false);
  }
  else {
    // Estado: OFF (Detener)
    handler.requestCameraChange(-1, QSize());

    ui->startButton->setText("Start");
    ui->comboBoxCameras->setEnabled(true);
    ui->comboBoxResolution->setEnabled(true);

    m_currentPixmap = QPixmap();
    ui->videoLabel->clear();
    ui->videoLabel->setText("Cámara detenida.");
  }
}

void VideoProcessingDialog::on_resetButton_clicked()
{
  ui->checkBoxFocoAuto->setChecked(true);
  ui->checkBoxExposicionAuto->setChecked(true);
  ui->horizontalSliderBrillo->setValue(50);
  ui->horizontalSliderContraste->setValue(50);
  ui->horizontalSliderSaturacion->setValue(50);
  ui->horizontalSliderNitidez->setValue(50);
  // Foco y Exposición automáticos
  on_checkBoxFocoAuto_toggled(true);
  on_checkBoxExposicionAuto_toggled(true);
  on_horizontalSliderBrillo_sliderMoved(50);
  on_horizontalSliderContraste_sliderMoved(50);
  on_horizontalSliderSaturacion_sliderMoved(50);
  on_horizontalSliderNitidez_sliderMoved(50);
}

void VideoProcessingDialog::on_cameraOpenFailed(int cameraId, const QString& errorMsg)
{
  Q_UNUSED(cameraId);
  QMessageBox::critical(this, "Error de Cámara", tr("No se pudo iniciar la cámara seleccionada. Detalle: %1").arg(errorMsg));

  // Resetear el botón de inicio/parada y habilitar la selección de cámara
  ui->startButton->setChecked(false);
  ui->startButton->setText("Start OpenCV");
  ui->comboBoxCameras->setEnabled(true);
  ui->comboBoxResolution->setEnabled(true);
}

void VideoProcessingDialog::on_rangesSupported(const CameraPropertyRanges& ranges)
{
  m_ranges = ranges;

  // Configurar los Sliders (escala de 0 a 100 para la GUI)
  // ... (Lógica de configuración de rangos y valores sin cambios) ...

  // Brillo
  ui->horizontalSliderBrillo->setValue(qBound(0, mapOpenCVToSlider(ranges.brightness.current, ranges.brightness), 100));

  // Contraste
  ui->horizontalSliderContraste->setValue(qBound(0, mapOpenCVToSlider(ranges.contrast.current, ranges.contrast), 100));

  // Saturación
  ui->horizontalSliderSaturacion->setValue(qBound(0, mapOpenCVToSlider(ranges.saturation.current, ranges.saturation), 100));

  // Nitidez
  ui->horizontalSliderNitidez->setValue(qBound(0, mapOpenCVToSlider(ranges.sharpness.current, ranges.sharpness), 100));

  // Exposición
  ui->horizontalSliderExposicion->setValue(qBound(0, mapOpenCVToSlider(ranges.exposure.current, ranges.exposure), 100));

  // Foco
  ui->horizontalSliderFoco->setValue(qBound(0, mapOpenCVToSlider(ranges.focus.current, ranges.focus), 100));

  // Habilitar o deshabilitar controles según el soporte
  ui->checkBoxFocoAuto->setEnabled(m_support.autoFocus);
  ui->horizontalSliderBrillo->setEnabled(m_support.brightness);
  ui->horizontalSliderContraste->setEnabled(m_support.contrast);
  ui->horizontalSliderSaturacion->setEnabled(m_support.saturation);
  ui->horizontalSliderNitidez->setEnabled(m_support.sharpness);
  ui->checkBoxExposicionAuto->setEnabled(m_support.autoExposure);

  ui->horizontalSliderFoco->setEnabled(m_support.focus && !ui->checkBoxFocoAuto->isChecked());
  ui->horizontalSliderExposicion->setEnabled(m_support.exposure && !ui->checkBoxExposicionAuto->isChecked());
}
// nuevo slot)
void VideoProcessingDialog::on_propertiesSupported(CameraPropertiesSupport support)
{
  m_support = support;
}

// Slots de Foco
void VideoProcessingDialog::on_checkBoxFocoAuto_toggled(bool checked)
{
  VideoCaptureHandler::instance().setAutoFocus(checked);
  ui->horizontalSliderFoco->setEnabled(m_support.focus && !checked);
}

void VideoProcessingDialog::on_checkBoxExposicionAuto_toggled(bool checked)
{
  VideoCaptureHandler::instance().setAutoExposure(checked);
  ui->horizontalSliderExposicion->setEnabled(m_support.exposure && !checked);
}

void VideoProcessingDialog::on_horizontalSliderFoco_sliderMoved(int value)
{
  int openCVValue = mapSliderToOpenCV(value, m_ranges.focus);
  VideoCaptureHandler::instance().setFocus(openCVValue);
}

void VideoProcessingDialog::on_horizontalSliderBrillo_sliderMoved(int value)
{
  int openCVValue = mapSliderToOpenCV(value, m_ranges.brightness);
  VideoCaptureHandler::instance().setBrightness(openCVValue);
}

void VideoProcessingDialog::on_horizontalSliderContraste_sliderMoved(int value)
{
  int openCVValue = mapSliderToOpenCV(value, m_ranges.contrast);
  VideoCaptureHandler::instance().setContrast(openCVValue);
}

void VideoProcessingDialog::on_horizontalSliderSaturacion_sliderMoved(int value)
{
  int openCVValue = mapSliderToOpenCV(value, m_ranges.saturation);
  VideoCaptureHandler::instance().setSaturation(openCVValue);
}

void VideoProcessingDialog::on_horizontalSliderNitidez_sliderMoved(int value)
{
  int openCVValue = mapSliderToOpenCV(value, m_ranges.sharpness);
  VideoCaptureHandler::instance().setSharpness(openCVValue);
}

void VideoProcessingDialog::on_horizontalSliderExposicion_sliderMoved(int value)
{
  int openCVValue = mapSliderToOpenCV(value, m_ranges.exposure);
  VideoCaptureHandler::instance().setExposure(openCVValue);
}

void VideoProcessingDialog::setAllControlsEnabled(bool enabled)
{
  ui->checkBoxFocoAuto->setEnabled(enabled);
  ui->horizontalSliderFoco->setEnabled(enabled);
  ui->horizontalSliderBrillo->setEnabled(enabled);
  ui->horizontalSliderContraste->setEnabled(enabled);
  ui->horizontalSliderSaturacion->setEnabled(enabled);
  ui->horizontalSliderNitidez->setEnabled(enabled);
  ui->checkBoxExposicionAuto->setEnabled(enabled);
  ui->horizontalSliderExposicion->setEnabled(enabled);

  if (!enabled) {
    ui->checkBoxExposicionAuto->setChecked(true);
    ui->checkBoxFocoAuto->setChecked(true);
  }
}

void VideoProcessingDialog::updateVideoLabel()
{
  if (m_currentPixmap.isNull()) {
    return;
  }
  ui->videoLabel->setPixmap(m_currentPixmap.scaled(ui->videoLabel->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
}

QSize VideoProcessingDialog::parseResolution(const QString& text)
{
  if (text == "Default") {
    return QSize(0, 0);
  }
  QStringList parts = text.split('x');
  if (parts.size() == 2) {
    bool ok1, ok2;
    int  w = parts[0].toInt(&ok1);
    int  h = parts[1].toInt(&ok2);
    if (ok1 && ok2) {
      return QSize(w, h);
    }
  }
  return QSize(0, 0);
}

int VideoProcessingDialog::mapSliderToOpenCV(int sliderValue, const PropertyRange& range)
{
  // Escala de [0, 100] (slider) a [range.min, range.max] (OpenCV)
  double outputRange = range.max - range.min;
  double scaleFactor = outputRange / 100.0;

  // Mapeo lineal: (ValorSlider * FactorEscala) + Mínimo
  double mappedValue = (sliderValue * scaleFactor) + range.min;

  // Asegurar que el valor se mantiene dentro del rango de OpenCV
  return qBound(static_cast<int>(range.min), static_cast<int>(mappedValue), static_cast<int>(range.max));
}

int VideoProcessingDialog::mapOpenCVToSlider(double openCVValue, const PropertyRange& range)
{
  // Escala de [range.min, range.max] (OpenCV) a [0, 100] (slider)
  double inputValue = openCVValue - range.min;
  double inputRange = range.max - range.min;

  if (qFuzzyIsNull(inputRange)) {
    return 50; // Evitar división por cero, devolver centro por defecto
  }

  double normalizedValue = inputValue / inputRange;

  // Mapeo al rango de 0 a 100
  int sliderValue = static_cast<int>(normalizedValue * 100.0);

  // Asegurar que el valor se mantiene dentro del rango del slider
  return qBound(0, sliderValue, 100);
}
