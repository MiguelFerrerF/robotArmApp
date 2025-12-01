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
   // Checkboxes y sliders
  void on_checkBoxSegmentacion_toggled(bool checked);
  void on_ButtonpointBL_clicked();
  void on_ButtonpointBR_clicked();
  void on_ButtonpointTL_clicked();
  void on_ButtonpointTR_clicked();

  // Captura de video
  void handleNewPixmap(const QPixmap& pixmap);
  void on_videoLabel_clicked(const QPoint& pos);

signals:
  void PointChanged(QPoint point);

private:
  Ui::VideoProcessingDialog* ui;

  cv::Mat m_lastPerspectiveMatrix;

  // Imagen actual y puntos de recorte
  QPixmap m_currentPixmap;
  QPoint  m_cropPointTL{175, 83};
  QPoint  m_cropPointTR{423, 81};
  QPoint  m_cropPointBR{484, 285};
  QPoint  m_cropPointBL{131, 302};

  QPoint m_lastCentroidOriginal;   // Centroide en coordenadas del frame original
  QPoint m_lastPointRectaOriginal; // Punto de la recta en coordenadas del frame original

  // Puntos transformados después de aplicar perspectiva
  std::vector<QPoint> m_transformedCropPoints;

  // Configuración
  bool m_applyPerspectiveCorrection = true;

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
  void updatePointInfoLabel();
 
  // Transformación de perspectiva
  QPixmap applyPerspectiveCrop(const QPixmap& original, const QPoint& tl, const QPoint& tr, const QPoint& br, const QPoint& bl,
                               std::vector<QPoint>& transformedPoints);
  // Nueva función para transformar un punto del crop al frame original
  QPoint transformCropPointToOriginal(const cv::Point2f& cropPoint);
};

#endif // VIDEOPROCESSINGDIALOG_H