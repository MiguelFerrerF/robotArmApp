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

class VideoManagerDialog : public QDialog
{
  Q_OBJECT

public:
  VideoManagerDialog(QWidget* parent = nullptr);
  ~VideoManagerDialog();

private slots:
  void on_startButton_clicked();
  void on_resetButton_clicked();

  void on_propertiesSupported(CameraPropertiesSupport support);
  void on_rangesSupported(const CameraPropertyRanges& ranges);
  void on_cameraOpenFailed(int cameraId, const QString& errorMsg);

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
  void updateStartButtonState(); // NUEVO: Para actualizar el estado del botón

  void setAllControlsEnabled(bool enabled);

  QSize parseResolution(const QString& text);

  int mapSliderToOpenCV(int sliderValue, const PropertyRange& range);
  int mapOpenCVToSlider(double openCVValue, const PropertyRange& range);
};
#endif // VIDEOMANAGERDIALOG_H