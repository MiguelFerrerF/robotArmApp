#ifndef VIDEOPROCESSINGDIALOG_H
#define VIDEOPROCESSINGDIALOG_H

#include "VideoCaptureHandler.h"
#include <QDialog>
#include <QPixmap>
#include <QPoint>
#include <QResizeEvent>
#include <QSize>
#include <vector>

namespace Ui
{
class VideoProcessingDialog;
}

class VideoProcessingDialog : public QDialog
{
  Q_OBJECT

public:
  explicit VideoProcessingDialog(QWidget* parent = nullptr);
  ~VideoProcessingDialog();

private slots:
  // Botones Start / Reset
  void on_startButton_clicked();
  void on_resetButton_clicked();

  // Señales de soporte y cámara
  void on_propertiesSupported(CameraPropertiesSupport support);
  void on_rangesSupported(const CameraPropertyRanges& ranges);
  void on_cameraOpenFailed(int cameraId, const QString& errorMsg);

  // Checkboxes y sliders
  void on_checkBoxFocoAuto_toggled(bool checked);
  void on_checkBoxExposicionAuto_toggled(bool checked);
  void on_checkBoxSegmentacion_toggled(bool checked);
  void on_horizontalSliderFoco_sliderMoved(int value);
  void on_horizontalSliderExposicion_sliderMoved(int value);
  void on_horizontalSliderBrillo_sliderMoved(int value);
  void on_horizontalSliderContraste_sliderMoved(int value);
  void on_horizontalSliderSaturacion_sliderMoved(int value);
  void on_horizontalSliderNitidez_sliderMoved(int value);

  // Captura de video
  void handleNewPixmap(const QPixmap& pixmap);
  void on_videoLabel_clicked(const QPoint& pos);

private:
  Ui::VideoProcessingDialog* ui;

  // Imagen actual y puntos de recorte
  QPixmap m_currentPixmap;
  QPoint  m_cropPointTL{196, 129};
  QPoint  m_cropPointTR{443, 130};
  QPoint  m_cropPointBR{511, 365};
  QPoint  m_cropPointBL{151, 367};

  // Puntos transformados después de aplicar perspectiva
  std::vector<QPoint> m_transformedCropPoints;

  // Configuración
  bool m_applyPerspectiveCorrection = true;
  bool m_applySegmentacion          = false;

  CameraPropertiesSupport m_support;
  CameraPropertyRanges    m_ranges;

  // Selección de punto activo
  enum CornerSelection
  {
    None,
    TL,
    TR,
    BR,
    BL
  };
  CornerSelection m_selectedCorner = None;

  // Métodos internos
  void applySegmentacion(QPixmap& pixmap);
  void drawCropPointsOnLabel();
  void updateVideoLabel();
  void updateStartButtonState();
  void setAllControlsEnabled(bool enabled);

  // Transformación de perspectiva
  QPixmap applyPerspectiveCrop(const QPixmap& original, const QPoint& tl, const QPoint& tr, const QPoint& br, const QPoint& bl,
                               std::vector<QPoint>& transformedPoints);
  void    updatePointInfoLabel();

  // Utilidades
  QSize parseResolution(const QString& text);
  int   mapSliderToOpenCV(int sliderValue, const PropertyRange& range);
  int   mapOpenCVToSlider(double openCVValue, const PropertyRange& range);
};

#endif // VIDEOPROCESSINGDIALOG_H
