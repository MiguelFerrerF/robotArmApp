#ifndef VIDEOCALIBRATIONDIALOG_H
#define VIDEOCALIBRATIONDIALOG_H

#include "../library-robot/RobotHandler.h"
#include "VideoCaptureHandler.h"
#include <QDialog>
#include <QPixmap>
#include <QResizeEvent>
#include <QSettings>
#include <QSize>
#include <QString>
#include <QThread>
#include <opencv2/opencv.hpp>

namespace Ui
{
class VideoCalibrationDialog;
}

/**
 * @brief Container for camera intrinsic calibration results.
 */
struct VideoCalibrationResult
{
  double   rms = -1.0;         ///< Root Mean Square reprojection error.
  cv::Mat  cameraMatrix;       ///< Intrinsic Matrix (K) containing focal lengths and optical center.
  cv::Mat  distCoeffs;         ///< Distortion coefficients (k1, k2, p1, p2, k3).
  cv::Mat  newCameraMatrix;    ///< Optimized matrix based on the free scaling parameter (alpha).
  cv::Rect roi;                ///< Valid Region of Interest.
  int      processedCount = 0; ///< Number of images used.
};
Q_DECLARE_METATYPE(VideoCalibrationResult)

/**
 * @brief Background worker for Camera Intrinsic Calibration.
 *
 * Runs the standard OpenCV calibration pipeline:
 * 1. Corner detection (`findChessboardCorners`).
 * 2. Sub-pixel refinement (`cornerSubPix`).
 * 3. Camera calibration (`calibrateCamera`).
 */
class VideoCalibrationWorker : public QObject
{
  Q_OBJECT
public:
  VideoCalibrationWorker(QObject* parent = nullptr) : QObject(parent)
  {
    qRegisterMetaType<VideoCalibrationResult>();
  }

public slots:
  /**
   * @brief Starts the batch processing of images for calibration.
   * @param directoryPath Path containing the .tiff images.
   * @param boardSize Logical dimensions of the board (inner corners).
   * @param squareSize Physical size of squares (used to define the object coordinate scale).
   */
  void doCalibration(const QString& directoryPath, cv::Size boardSize, float squareSize);

signals:
  void calibrationFinished(const VideoCalibrationResult& result);
  void calibrationError(const QString& message);
  void progressUpdate(const QString& message); // Para mostrar el progreso

private:
  // Métodos de calibración movidos del VideoCalibrationDialog
  std::vector<cv::Point3f> createObjectPoints(cv::Size boardSize, float squareSize) const;
  bool                     processImageForCorners(const cv::Mat& image, cv::Size boardSize, float squareSize, std::vector<cv::Point2f>& corners);

  /**
   * @brief Wrapper around `cv::calibrateCamera`.
   */
  bool runCalibration(cv::Size boardSize, std::vector<std::vector<cv::Point2f>>& imagePoints, std::vector<std::vector<cv::Point3f>>& objectPoints,
                      VideoCalibrationResult& result);

  void saveCalibration(const std::string& cameraMatrixFile, const std::string& distCoeffsFile, const cv::Mat& cameraMatrix, const cv::Mat& distCoeffs,
                       const cv::Mat& newCameraMatrix) const;
};

/**
 * @brief GUI Dialog for Camera Calibration and 3D Localization Manager.
 *
 * Besides managing the UI for calibration, this class acts as the **Computer Vision Bridge**.
 * It contains the logic to transform 2D pixel clicks into 3D Robot Base coordinates
 * by performing Ray Casting against a calibrated reference plane.
 */
class VideoCalibrationDialog : public QDialog
{
  Q_OBJECT

public:
  VideoCalibrationDialog(QWidget* parent = nullptr, RobotHandler* robotHandlerInstance = nullptr);
  ~VideoCalibrationDialog();

  /**
   * @brief Calculates the 3D position and orientation of an object from image points.
   * * This is the core "Vision-to-Motion" function. It takes two 2D points (centroid and orientation),
   * projects them into 3D space, and commands the robot via the RobotHandler.
   *
   * @param centroid The pixel coordinates of the object's center.
   * @param pointRecta A second pixel coordinate used to determine the object's rotation angle.
   */
  void calculateObjectPosition(QPoint centroid, QPoint pointRecta);

private slots:
  void on_startButton_clicked();
  void on_pushButtonSelectDirectory_clicked();
  void on_pushButtonCaptureImage_clicked();
  void on_calibrationFinished(const VideoCalibrationResult& result);
  void on_calibrationError(const QString& message);
  void on_progressUpdate(const QString& message);

signals:
  /**
   * @brief Emitted when a 2D point has been successfully resolved to a 3D coordinate in the robot's base frame.
   */
  void piecePositionCalculated(const cv::Point3d& positionInBase); // Señal para la posición calculada

private:
  Ui::VideoCalibrationDialog* ui;
  RobotHandler*               m_robotHandlerInstance = nullptr;

  QPixmap  m_currentPixmap;
  QString  m_selectedDirectoryPath;
  cv::Size m_calibrationBoardSize = cv::Size(9, 6);
  float    m_squareSize           = 10.0f;

  cv::Mat m_cameraMatrix;
  cv::Mat m_distCoeffs;
  cv::Mat m_newCameraMatrix;
  cv::Mat k;

  QThread*                m_workerThread = nullptr;
  VideoCalibrationWorker* m_worker       = nullptr;

  // --- CACHING VARIABLES ---
  // These variables store the geometric relationship of the "Work Plane"
  // to avoid recalculating it on every object detection.
  bool    m_isPlaneCalibrated = false;
  cv::Mat m_intrinsicK;  ///< Intrinsic Camera Matrix.
  cv::Mat m_distCoeffsD; ///< Distortion Coefficients.
  cv::Mat m_planeR;      ///< Rotation of the Work Plane relative to Camera.
  cv::Mat m_planeT;      ///< Translation of the Work Plane relative to Camera.
  cv::Mat m_RTcb;        ///< Transformation Matrix: Camera -> Robot Base.

  /**
   * @brief Lazy-loader for plane calibration.
   * Checks RAM -> QSettings -> File System (recalculation) to find the plane's pose.
   */
  bool ensurePlaneCalibrationLoaded();

  void updateVideoLabel();
  void updateFilesList();
  void displayCalibrationResults(const cv::Mat& cameraMatrix, const cv::Mat& distCoeffs, const cv::Mat& newCameraMatrix, double rms);

  bool loadCalibration(const std::string& filename);
  void loadExistingCalibration();

  /**
   * @brief Transforms a point from Camera Coordinates to Robot Base Coordinates.
   * Applies $P_{base} = RT_{cb} \cdot P_{cam}$ and adds physical offsets.
   */
  cv::Point3d getPiecePositionInBaseCoordinates(const cv::Point3d& result3D, const cv::Mat& RTcb);
};
#endif // VIDEOCALIBRATIONDIALOG_H