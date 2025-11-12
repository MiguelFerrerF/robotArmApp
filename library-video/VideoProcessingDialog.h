#ifndef VIDEOPROCESSINGDIALOG_H
#define VIDEOPROCESSINGDIALOG_H

#include "VideoCaptureHandler.h"
#include <QDialog>
#include <QPixmap>
#include <QPoint> // Necesario para QPoint
#include <QResizeEvent>
#include <QSize>

namespace Ui
{
class VideoProcessingDialog;
}

class VideoProcessingDialog : public QDialog
{
  Q_OBJECT

public:
  VideoProcessingDialog(QWidget* parent = nullptr);
  ~VideoProcessingDialog();

private slots:
  void on_startButton_clicked();
  void on_resetButton_clicked();

  void on_propertiesSupported(CameraPropertiesSupport support);
  void on_rangesSupported(const CameraPropertyRanges& ranges);
  void on_cameraOpenFailed(int cameraId, const QString& errorMsg);

  void on_checkBoxFocoAuto_toggled(bool checked);
  void on_checkBoxExposicionAuto_toggled(bool checked);
  void on_checkBoxSegmentacion_toggled(bool checked);
  void on_horizontalSliderFoco_sliderMoved(int value);
  void on_horizontalSliderExposicion_sliderMoved(int value);
  void on_horizontalSliderBrillo_sliderMoved(int value);
  void on_horizontalSliderContraste_sliderMoved(int value);
  void on_horizontalSliderSaturacion_sliderMoved(int value);
  void on_horizontalSliderNitidez_sliderMoved(int value);

  void handleNewPixmap(const QPixmap& pixmap);

private:
  Ui::VideoProcessingDialog* ui;

  QPixmap m_currentPixmap;
  QPoint m_cropPointTL = QPoint(160, 120); // Top-Left
  QPoint m_cropPointTR = QPoint(420, 120); // Top-Right
  QPoint m_cropPointBL = QPoint(120, 361); // Bottom-Left
  QPoint m_cropPointBR = QPoint(480, 361); // Bottom-Right

  bool m_applyPerspectiveCorrection = true; // Establecer a true para activar por defecto (o añadir checkbox)

  CameraPropertiesSupport m_support;
  CameraPropertyRanges    m_ranges;

  bool m_applySegmentacion = false;
  void applySegmentacion(QPixmap& pixmap);

  // Función de recorte rectangular (no se usa en la perspectiva, pero se
  // mantiene por si acaso)
  QPixmap cropPixmap(const QPixmap& original, int x, int y, int width, int height);

  void updateVideoLabel();
  void updateStartButtonState();

  // Declaración correcta: Retorna un QPixmap con la imagen transformada
  QPixmap applyPerspectiveCrop(const QPixmap& original, const QPoint& tl, const QPoint& tr, const QPoint& bl, const QPoint& br);

  void setAllControlsEnabled(bool enabled);

  QSize parseResolution(const QString& text);

  int mapSliderToOpenCV(int sliderValue, const PropertyRange& range);
  int mapOpenCVToSlider(double openCVValue, const PropertyRange& range);
};
#endif // VIDEOPROCESSINGDIALOG_H