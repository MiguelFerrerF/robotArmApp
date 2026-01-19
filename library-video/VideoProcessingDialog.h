#ifndef VIDEOPROCESSINGDIALOG_H
#define VIDEOPROCESSINGDIALOG_H

#include "VideoCaptureHandler.h"
#include <QDialog>
#include <QPixmap>
#include <QPoint>
#include <QResizeEvent>
#include <QSettings>
#include <QSize>
#include <vector>

namespace Ui
{
class VideoProcessingDialog;
}

/**
 * @brief GUI Dialog and Processing Engine for Object Detection.
 *
 * This class implements a complete Computer Vision pipeline  to detect objects
 * and determine their position and orientation.
 *
 * **Key Features:**
 * 1. **Perspective Correction:** Allows the user to define a Region of Interest (ROI) using 4 points (TL, TR, BR, BL)
 * to warp the image into a flat "top-down" view.
 * 2. **Segmentation:** Uses Edge Detection (Canny) and Contour analysis to find objects.
 * 3. **Feature Extraction:** Calculates the Centroid (X, Y) and Orientation Angle (Theta) of the detected piece.
 * 4. **Coordinate Mapping:** Automatically transforms coordinates from the "Cropped" space back to the "Original"
 * camera frame for robot calibration.
 */
class VideoProcessingDialog : public QDialog
{
  Q_OBJECT

public:
  explicit VideoProcessingDialog(QWidget* parent = nullptr);
  ~VideoProcessingDialog();

  /**
   * @brief Gets the last calculated centroid in the Original Image coordinate system.
   * @return QPoint (x, y) pixels.
   */
  QPoint getCentroid() const
  {
    return m_lastCentroidOriginal;
  }

  /**
   * @brief Gets the reference point indicating the object's orientation in the Original Image frame.
   * @return QPoint (x, y) pixels.
   */
  QPoint getPointRecta() const
  {
    return m_lastPointRectaOriginal;
  }

private slots:
  // --- UI Interaction Slots ---
  void on_ButtonpointBL_clicked();
  void on_ButtonpointBR_clicked();
  void on_ButtonpointTL_clicked();
  void on_ButtonpointTR_clicked();

  /**
   * @brief Main processing trigger.
   * Called automatically whenever the VideoCaptureHandler emits a new frame.
   * Runs the crop -> segment -> display pipeline.
   * @param pixmap The raw image from the camera.
   */
  void handleNewPixmap(const QPixmap& pixmap);

  /**
   * @brief Handles mouse clicks on the video feed to define crop corners.
   * @param pos The click coordinates relative to the label widget.
   */
  void on_videoLabel_clicked(const QPoint& pos);

signals:
  /**
   * @brief Emitted when the object's rotation angle changes.
   * @param angle The angle in degrees [0-180].
   */
  void angleUpdated(double angle);

  /**
   * @brief Emitted when valid object features are detected.
   * @param centroid The center of mass of the object (in Original Frame).
   * @param pointRecta A point along the major axis indicating orientation (in Original Frame).
   */
  void piecePointsUpdated(const QPoint& centroid, const QPoint& pointRecta);

  /**
   * @brief Emitted to update the main UI with the annotated image (contours drawn).
   */
  void processedImageReady(const QImage& image);

private:
  Ui::VideoProcessingDialog* ui;

  cv::Mat m_lastPerspectiveMatrix; ///< Stores the 3x3 Homography matrix used for warping.

  // Imagen actual y puntos de recorte
  QPixmap m_currentPixmap;
  QPoint  m_cropPointTL{175, 83};
  QPoint  m_cropPointTR{423, 81};
  QPoint  m_cropPointBR{484, 285};
  QPoint  m_cropPointBL{131, 302};

  QPoint m_lastCentroidOriginal;
  QPoint m_lastPointRectaOriginal;

  std::vector<QPoint> m_transformedCropPoints;

  bool m_applyPerspectiveCorrection = true;

  // Selection state for defining the 4 corners
  enum CornerSelection
  {
    None,
    TL, // Top-Left
    TR, // Top-Right
    BR, // Bottom-Right
    BL  // Bottom-Left
  };
  CornerSelection m_selectedCorner = None;

  // --- Internal Processing Methods ---

  /**
   * @brief Runs the OpenCV segmentation pipeline (Canny -> FindContours -> Moments).
   */
  void applySegmentacion(QPixmap& pixmap);

  void drawCropPointsOnLabel();
  void updatePointInfoLabel();

  /**
   * @brief Warps the image based on the 4 selected corner points.
   * @return The cropped and rectified QPixmap.
   */
  QPixmap applyPerspectiveCrop(const QPixmap& original, const QPoint& tl, const QPoint& tr, const QPoint& br, const QPoint& bl,
                               std::vector<QPoint>& transformedPoints);

  /**
   * @brief Maps a point from the "Cropped/Warped" space back to the "Original" space.
   * applies $P_{orig} = M^{-1} \cdot P_{crop}$.
   */
  QPoint transformCropPointToOriginal(const cv::Point2f& cropPoint);
};

#endif // VIDEOPROCESSINGDIALOG_H