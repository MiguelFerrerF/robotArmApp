#ifndef VIDEOMANAGERDIALOG_H
#define VIDEOMANAGERDIALOG_H

#include "VideoCaptureHandler.h"
#include <QDialog>
#include <QPixmap>
#include <QResizeEvent>
#include <QSize>

namespace Ui
{
class VideoManagerDialog;
}

/**
 * @brief Configuration dialog for camera selection and image tuning.
 *
 * This class provides a graphical interface to:
 * 1. Enumerate and select available video input devices.
 * 2. Configure capture resolution.
 * 3. Tune hardware properties (Brightness, Contrast, Focus, Exposure, etc.).
 *
 * @note This class implements a **Normalization Layer**. Since different cameras use
 * different internal scales (e.g., Exposure might be 0-10000 on one cam and -10 to +10 on another),
 * this dialog maps all properties to a 0-100 slider range for consistent user experience.
 */
class VideoManagerDialog : public QDialog
{
  Q_OBJECT

public:
  VideoManagerDialog(QWidget* parent = nullptr);
  ~VideoManagerDialog();

private slots:
  /**
   * @brief Toggles the camera state (Start/Stop).
   */
  void on_startButton_clicked();

  /**
   * @brief Resets all image parameters to their default values (usually 50%).
   */
  void on_resetButton_clicked();

  // --- Handlers for VideoCaptureHandler signals ---

  /**
   * @brief Received when a camera is opened. Determines which UI controls to enable.
   */
  void on_propertiesSupported(CameraPropertiesSupport support);

  /**
   * @brief Received when a camera reports its minimum/maximum values.
   * Updates the internal mapping logic for the sliders.
   */
  void on_rangesSupported(const CameraPropertyRanges& ranges);

  void on_cameraOpenFailed(int cameraId, const QString& errorMsg);

  // --- UI Slider/Checkbox Events ---
  // These slots capture user input, convert the 0-100 value to hardware units,
  // and forward the request to the VideoCaptureHandler.

  void on_checkBoxFocoAuto_toggled(bool checked);
  void on_checkBoxExposicionAuto_toggled(bool checked);
  void on_horizontalSliderFoco_sliderMoved(int value);
  void on_horizontalSliderExposicion_sliderMoved(int value);
  void on_horizontalSliderBrillo_sliderMoved(int value);
  void on_horizontalSliderContraste_sliderMoved(int value);
  void on_horizontalSliderSaturacion_sliderMoved(int value);
  void on_horizontalSliderNitidez_sliderMoved(int value);

private:
  Ui::VideoManagerDialog* ui;

  QPixmap m_currentPixmap;

  CameraPropertiesSupport m_support;
  CameraPropertyRanges    m_ranges;

  void updateVideoLabel();

  /**
   * @brief Synchronizes the Start/Stop button with the actual backend state.
   * Useful if the dialog is closed and reopened while the camera is running.
   */
  void updateStartButtonState();

  /**
   * @brief Bulk enable/disable of parameter controls.
   */
  void setAllControlsEnabled(bool enabled);

  /**
   * @brief Parses a string like "1920x1080" into a QSize object.
   */
  QSize parseResolution(const QString& text);

  /**
   * @brief Maps a UI Slider value [0, 100] to the specific Hardware Range [min, max].
   * Formula: \f$ V_{hw} = V_{slider} \times \frac{Max - Min}{100} + Min \f$
   */
  int mapSliderToOpenCV(int sliderValue, const PropertyRange& range);
  
  /**
   * @brief Maps a Hardware value [min, max] to a UI Slider value [0, 100].
   */
  int mapOpenCVToSlider(double openCVValue, const PropertyRange& range);
};
#endif // VIDEOMANAGERDIALOG_H