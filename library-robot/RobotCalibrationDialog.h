#ifndef ROBOTCALIBRATIONDIALOG_H
#define ROBOTCALIBRATIONDIALOG_H

#include "../library-video/VideoCaptureHandler.h"
#include "RobotConfig.h"
#include <QDialog>
#include <QPixmap>
#include <QResizeEvent>
#include <QSize>
#include <QString>
#include <QThread>
#include <opencv2/opencv.hpp>

namespace Ui
{
class RobotCalibrationDialog;
}

/**
 * @brief Container for the results of the robot Hand-Eye calibration process.
 */
struct RobotCalibrationResult
{
  double   rms = -1.0;         ///< Root Mean Square error of the calibration.
  cv::Mat  cameraMatrix;       ///< Intrinsic camera matrix.
  cv::Mat  distCoeffs;         ///< Lens distortion coefficients.
  cv::Mat  newCameraMatrix;    ///< Optimized camera matrix based on the free scaling parameter.
  cv::Rect roi;                ///< Region of Interest containing valid pixels in the undistorted image.
  int      processedCount = 0; ///< Number of image pairs successfully used for calibration.
  cv::Mat  RTcb;               ///< Resulting 4x4 homogeneous transformation matrix from Camera to Base.
};
Q_DECLARE_METATYPE(RobotCalibrationResult)

/**
 * @brief Worker class responsible for executing the Hand-Eye calibration algorithms in a background thread.
 *
 * This class handles the heavy lifting of:
 * 1. Detecting chessboard corners.
 * 2. Computing the PnP (Perspective-n-Point) from the calibration pattern.
 * 3. Computing Forward Kinematics from robot motor logs.
 * 4. Solving the Hand-Eye calibration equation ($AX = XB$).
 */
class RobotCalibrationWorker : public QObject
{
  Q_OBJECT
public:
  RobotCalibrationWorker(QObject* parent = nullptr) : QObject(parent)
  {
    qRegisterMetaType<RobotCalibrationResult>();
  }

public slots:
  /**
   * @brief Starts the calibration process using images and data from the specified directory.
   *
   * @param[in] directoryPath The filesystem path containing pairs of images (.tiff) and robot data (.json).
   * @param[in] boardSize The logical dimensions of the chessboard (internal corners, e.g., 9x6).
   * @param[in] squareSize The physical size of a chessboard square (usually in mm).
   */
  void doCalibration(const QString& directoryPath, cv::Size boardSize, float squareSize);

signals:
  void calibrationFinished(const RobotCalibrationResult& result);
  void calibrationError(const QString& message);
  void progressUpdate(const QString& message);

private:
  std::vector<cv::Point3f> createObjectPoints(cv::Size boardSize, float squareSize) const;
  bool                     processImageForCorners(const cv::Mat& image, cv::Size boardSize, float squareSize, std::vector<cv::Point2f>& corners);

  void saveCalibration(const std::string& RT_camera_base, const cv::Mat& RTcb) const;
  bool loadCalibration(RobotCalibrationResult& result);
  void getRTbaseToolFromFile(const std::string& jsonFilePath, cv::Mat& Rbt, cv::Mat& Tbt);
};

/**
 * @brief Main Dialog for the Robot Calibration GUI.
 *
 * Manages the user interaction for capturing images, selecting directories,
 * and visualizing the progress and results of the calibration.
 */
class RobotCalibrationDialog : public QDialog
{
  Q_OBJECT

public:
  RobotCalibrationDialog(QWidget* parent = nullptr, RobotConfig::RobotSettings* settings = nullptr);
  ~RobotCalibrationDialog();

private slots:
  void on_startButton_clicked();
  void on_pushButtonSelectDirectory_clicked();
  void on_pushButtonCaptureImage_clicked();

  void on_calibrationFinished(const RobotCalibrationResult& result);
  void on_calibrationError(const QString& message);
  void on_progressUpdate(const QString& message);

private:
  Ui::RobotCalibrationDialog* ui;

  QPixmap m_currentPixmap;
  QString m_selectedDirectoryPath;

  cv::Size m_calibrationBoardSize = cv::Size(9, 6);
  float    m_squareSize           = 10.0f;

  cv::Mat m_cameraMatrix;
  cv::Mat m_distCoeffs;
  cv::Mat m_newCameraMatrix;

  QThread*                m_workerThread = nullptr;
  RobotCalibrationWorker* m_worker       = nullptr;

  RobotConfig::RobotSettings* m_robotSettings;

  void updateVideoLabel();
  void updateFilesList();
  void displayCalibrationResults(const cv::Mat& cameraMatrix, const cv::Mat& distCoeffs, const cv::Mat& newCameraMatrix, double rms);

  bool saveMotorAnglesToJson(const RobotConfig::RobotSettings& settings, const QString& filePath);

  bool loadCalibration(const std::string& filename);
  void loadExistingCalibration();
};
#endif // ROBOTCALIBRATIONDIALOG_H
