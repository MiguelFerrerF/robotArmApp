#include "VideoManagerDialog.h"
#include "./ui_VideoManagerDialog.h"
#include <QCameraDevice>
#include <QMediaDevices>
#include <QMessageBox>
#include <QtMath>

/**
 * @brief Constructor.
 *
 * Initializes the UI, populates the camera list using `QMediaDevices`,
 * and connects to the singleton `VideoCaptureHandler`.
 * It also checks if a camera is currently running to restore the UI state.
 */
VideoManagerDialog::VideoManagerDialog(QWidget* parent) : QDialog(parent), ui(new Ui::VideoManagerDialog)
{
  ui->setupUi(this);
  this->setWindowTitle("Camera Manager");

  this->setWindowFlags(this->windowFlags() | Qt::WindowMinimizeButtonHint | Qt::WindowMaximizeButtonHint);

  VideoCaptureHandler& handler = VideoCaptureHandler::instance();
  connect(&handler, &VideoCaptureHandler::newPixmapCaptured, this, [=](const QPixmap& pixmap) {
    m_currentPixmap = pixmap;
    updateVideoLabel();
  });

  connect(&handler, &VideoCaptureHandler::propertiesSupported, this, &VideoManagerDialog::on_propertiesSupported);
  connect(&handler, &VideoCaptureHandler::rangesSupported, this, &VideoManagerDialog::on_rangesSupported);
  connect(&handler, &VideoCaptureHandler::cameraOpenFailed, this, &VideoManagerDialog::on_cameraOpenFailed);

  QStringList cameraNames;
  for (const QCameraDevice& camera : QMediaDevices::videoInputs()) {
    cameraNames << camera.description();
  }
  ui->comboBoxCameras->addItems(cameraNames);

  if (cameraNames.isEmpty()) {
    ui->startButton->setEnabled(false);
    ui->videoLabel->setText("No se han detectado cámaras.");
  }

  updateStartButtonState();
  setAllControlsEnabled(false);
}

/**
 * @brief Destructor.
 *
 * Disconnects from the `VideoCaptureHandler` signals and cleans up the UI.
 */
VideoManagerDialog::~VideoManagerDialog()
{
  disconnect(&VideoCaptureHandler::instance(), SIGNAL(newPixmapCaptured(QPixmap)), this, nullptr);
  delete ui;
}

/**
 * @brief Updates the video display label with the latest captured pixmap.
 *
 * Scales the pixmap to fit the label while maintaining aspect ratio.
 */
void VideoManagerDialog::updateStartButtonState()
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

/**
 * @brief Handles the Start/Stop action.
 *
 * - **Start:** Reads the selected camera index and resolution, then requests the
 * `VideoCaptureHandler` to open the device.
 * - **Stop:** Requests the handler to close the camera (-1) and resets the UI preview.
 */
void VideoManagerDialog::on_startButton_clicked()
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
    handler.requestCameraChange(-1, QSize());

    ui->startButton->setText("Start");
    ui->comboBoxCameras->setEnabled(true);
    ui->comboBoxResolution->setEnabled(true);

    m_currentPixmap = QPixmap();
    ui->videoLabel->clear();
    ui->videoLabel->setText("Cámara detenida.");
  }
}

/**
 * @brief Resets all image parameters to their default values (50%).
 * Also triggers the corresponding slots to apply these defaults to the camera.
 */
void VideoManagerDialog::on_resetButton_clicked()
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

/**
 * @brief Handles camera open failures.
 *
 * Displays an error message and resets the Start/Stop button and camera selection.
 *
 * @param cameraId The ID of the camera that failed to open.
 * @param errorMsg The error message detailing the failure.
 */
void VideoManagerDialog::on_cameraOpenFailed(int cameraId, const QString& errorMsg)
{
  Q_UNUSED(cameraId);
  QMessageBox::critical(this, "Error de Cámara", tr("No se pudo iniciar la cámara seleccionada. Detalle: %1").arg(errorMsg));

  ui->startButton->setChecked(false);
  ui->startButton->setText("Start OpenCV");
  ui->comboBoxCameras->setEnabled(true);
  ui->comboBoxResolution->setEnabled(true);
}

/**
 * @brief Adapts the UI sliders to the connected camera's capabilities.
 *
 * This slot is triggered when the backend reports the specific ranges of the hardware.
 * It iterates through all properties (Brightness, Focus, etc.), calculates the
 * correct slider position relative to the current hardware value, and enables/disables
 * sliders based on hardware support.
 *
 * @param ranges The min/max/current values reported by OpenCV.
 */
void VideoManagerDialog::on_rangesSupported(const CameraPropertyRanges& ranges)
{
  m_ranges = ranges;

  // Map hardware values to UI [0-100]
  ui->horizontalSliderBrillo->setValue(qBound(0, mapOpenCVToSlider(ranges.brightness.current, ranges.brightness), 100));
  ui->horizontalSliderContraste->setValue(qBound(0, mapOpenCVToSlider(ranges.contrast.current, ranges.contrast), 100));
  ui->horizontalSliderSaturacion->setValue(qBound(0, mapOpenCVToSlider(ranges.saturation.current, ranges.saturation), 100));
  ui->horizontalSliderNitidez->setValue(qBound(0, mapOpenCVToSlider(ranges.sharpness.current, ranges.sharpness), 100));
  ui->horizontalSliderExposicion->setValue(qBound(0, mapOpenCVToSlider(ranges.exposure.current, ranges.exposure), 100));
  ui->horizontalSliderFoco->setValue(qBound(0, mapOpenCVToSlider(ranges.focus.current, ranges.focus), 100));

  // Enable controls only if supported by hardware
  ui->checkBoxFocoAuto->setEnabled(m_support.autoFocus);
  ui->horizontalSliderBrillo->setEnabled(m_support.brightness);
  ui->horizontalSliderContraste->setEnabled(m_support.contrast);
  ui->horizontalSliderSaturacion->setEnabled(m_support.saturation);
  ui->horizontalSliderNitidez->setEnabled(m_support.sharpness);
  ui->checkBoxExposicionAuto->setEnabled(m_support.autoExposure);

  ui->horizontalSliderFoco->setEnabled(m_support.focus && !ui->checkBoxFocoAuto->isChecked());
  ui->horizontalSliderExposicion->setEnabled(m_support.exposure && !ui->checkBoxExposicionAuto->isChecked());
}

/**
 * @brief Updates the supported properties based on backend feedback.
 *
 * This slot is triggered when the backend reports which properties
 * (Focus, Exposure, etc.) are supported by the connected camera.
 *
 * @param support The support flags for each property.
 */
void VideoManagerDialog::on_propertiesSupported(CameraPropertiesSupport support)
{
  m_support = support;
}

/**
 * @brief Handles the Auto Focus checkbox toggle.
 *
 * Sends the new auto-focus state to the `VideoCaptureHandler`
 * and enables/disables the Focus slider accordingly.
 *
 * @param checked True if Auto Focus is enabled, false otherwise.
 */
void VideoManagerDialog::on_checkBoxFocoAuto_toggled(bool checked)
{
  VideoCaptureHandler::instance().setAutoFocus(checked);
  ui->horizontalSliderFoco->setEnabled(m_support.focus && !checked);
}

/**
 * @brief Handles the Auto Exposure checkbox toggle.
 *
 * Sends the new auto-exposure state to the `VideoCaptureHandler`
 * and enables/disables the Exposure slider accordingly.
 *
 * @param checked True if Auto Exposure is enabled, false otherwise.
 */
void VideoManagerDialog::on_checkBoxExposicionAuto_toggled(bool checked)
{
  VideoCaptureHandler::instance().setAutoExposure(checked);
  ui->horizontalSliderExposicion->setEnabled(m_support.exposure && !checked);
}

/**
 * @brief Handles the Focus slider movement.
 *
 * Maps the UI slider value [0-100] to the hardware range
 * and sends the new focus value to the `VideoCaptureHandler`.
 *
 * @param value The new slider value (0-100).
 */
void VideoManagerDialog::on_horizontalSliderFoco_sliderMoved(int value)
{
  int openCVValue = mapSliderToOpenCV(value, m_ranges.focus);
  VideoCaptureHandler::instance().setFocus(openCVValue);
}

/**
 * @brief Handles the Brightness slider movement.
 *
 * Maps the UI slider value [0-100] to the hardware range
 * and sends the new brightness value to the `VideoCaptureHandler`.
 *
 * @param value The new slider value (0-100).
 */
void VideoManagerDialog::on_horizontalSliderBrillo_sliderMoved(int value)
{
  int openCVValue = mapSliderToOpenCV(value, m_ranges.brightness);
  VideoCaptureHandler::instance().setBrightness(openCVValue);
}

/**
 * @brief Handles the Contrast slider movement.
 *
 * Maps the UI slider value [0-100] to the hardware range
 * and sends the new contrast value to the `VideoCaptureHandler`.
 *
 * @param value The new slider value (0-100).
 */
void VideoManagerDialog::on_horizontalSliderContraste_sliderMoved(int value)
{
  int openCVValue = mapSliderToOpenCV(value, m_ranges.contrast);
  VideoCaptureHandler::instance().setContrast(openCVValue);
}

/**
 * @brief Handles the Saturation slider movement.
 *
 * Maps the UI slider value [0-100] to the hardware range
 * and sends the new saturation value to the `VideoCaptureHandler`.
 *
 * @param value The new slider value (0-100).
 */
void VideoManagerDialog::on_horizontalSliderSaturacion_sliderMoved(int value)
{
  int openCVValue = mapSliderToOpenCV(value, m_ranges.saturation);
  VideoCaptureHandler::instance().setSaturation(openCVValue);
}

/**
 * @brief Handles the Sharpness slider movement.
 *
 * Maps the UI slider value [0-100] to the hardware range
 * and sends the new sharpness value to the `VideoCaptureHandler`.
 *
 * @param value The new slider value (0-100).
 */
void VideoManagerDialog::on_horizontalSliderNitidez_sliderMoved(int value)
{
  int openCVValue = mapSliderToOpenCV(value, m_ranges.sharpness);
  VideoCaptureHandler::instance().setSharpness(openCVValue);
}

/**
 * @brief Handles the Exposure slider movement.
 *
 * Maps the UI slider value [0-100] to the hardware range
 * and sends the new exposure value to the `VideoCaptureHandler`.
 *
 * @param value The new slider value (0-100).
 */
void VideoManagerDialog::on_horizontalSliderExposicion_sliderMoved(int value)
{
  int openCVValue = mapSliderToOpenCV(value, m_ranges.exposure);
  VideoCaptureHandler::instance().setExposure(openCVValue);
}

/**
 * @brief Enables or disables all parameter controls.
 *
 * Useful when starting/stopping the camera to prevent user interaction
 * during state transitions.
 *
 * @param enabled True to enable controls, false to disable.
 */
void VideoManagerDialog::setAllControlsEnabled(bool enabled)
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

/**
 * @brief Updates the video display label with the latest captured pixmap.
 *
 * Scales the pixmap to fit the label while maintaining aspect ratio.
 */
void VideoManagerDialog::updateVideoLabel()
{
  if (m_currentPixmap.isNull()) {
    return;
  }
  ui->videoLabel->setPixmap(m_currentPixmap.scaled(ui->videoLabel->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
}

/**
 * @brief Parses a resolution string (e.g., "1920x1080") into a QSize object.
 *
 * If the string is "Default" or invalid, returns QSize(0, 0).
 *
 * @param text The resolution string from the UI.
 * @return The parsed QSize object.
 */
QSize VideoManagerDialog::parseResolution(const QString& text)
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

/**
 * @brief Converts a normalized slider value to the camera's native range.
 *
 * Used when sending user input to the backend.
 *
 * @param sliderValue The UI value [0, 100].
 * @param range The hardware constraints [min, max].
 * @return The calculated value in hardware units.
 */
int VideoManagerDialog::mapSliderToOpenCV(int sliderValue, const PropertyRange& range)
{

  double outputRange = range.max - range.min;
  double scaleFactor = outputRange / 100.0;

  double mappedValue = (sliderValue * scaleFactor) + range.min;

  return qBound(static_cast<int>(range.min), static_cast<int>(mappedValue), static_cast<int>(range.max));
}

/**
 * @brief Converts a native camera value to a normalized slider position.
 *
 * Used when updating the UI to reflect the camera's current state.
 *
 * @param openCVValue The hardware value.
 * @param range The hardware constraints.
 * @return The normalized value [0, 100].
 */
int VideoManagerDialog::mapOpenCVToSlider(double openCVValue, const PropertyRange& range)
{
  double inputValue = openCVValue - range.min;
  double inputRange = range.max - range.min;

  if (qFuzzyIsNull(inputRange)) {
    return 50; 
  }

  double normalizedValue = inputValue / inputRange;
  int sliderValue = static_cast<int>(normalizedValue * 100.0);

  return qBound(0, sliderValue, 100);
}
