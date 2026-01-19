#ifndef VIDEOCAPTUREHANDLER_H
#define VIDEOCAPTUREHANDLER_H

#include <QImage>
#include <QMetaType>
#include <QPixmap>
#include <QSize>
#include <QThread>
#include <atomic>
#include <opencv2/opencv.hpp>

#define ID_CAMERA_DEFAULT 0

#define START_CAMERA 0
#define STOP_CAMERA -1
#define NO_OP_CAMERA -2

#define NULL_CAMERA -1

/**
 * @brief Structure indicating which camera features are supported by the hardware.
 * Used to enable/disable UI controls dynamically.
 */
struct CameraPropertiesSupport
{
  bool autoFocus    = false;
  bool focus        = true;
  bool autoExposure = false;
  bool exposure     = false;
  bool brightness   = false;
  bool contrast     = false;
  bool saturation   = false;
  bool sharpness    = false;
};
Q_DECLARE_METATYPE(CameraPropertiesSupport)

/**
 * @brief Defines the valid range and current value for a specific camera property.
 */
struct PropertyRange
{
  double min     = 0;
  double max     = 255;
  double current = 0;
};
Q_DECLARE_METATYPE(PropertyRange)

/**
 * @brief Aggregation of ranges for all controllable camera properties.
 */
struct CameraPropertyRanges
{
  PropertyRange brightness;
  PropertyRange contrast;
  PropertyRange saturation;
  PropertyRange sharpness;
  PropertyRange focus;
  PropertyRange exposure;
};
Q_DECLARE_METATYPE(CameraPropertyRanges)

/**
 * @brief Snapshot of the current camera state and configuration.
 */
struct CameraInfo
{
  std::string name           = "";
  bool        isFocusAuto    = true;
  bool        isExposureAuto = true;
  int         width          = -1;
  int         height         = -1;
  int         brightness     = -1;
  int         contrast       = -1;
  int         saturation     = -1;
  int         sharpness      = -1;
  int         focus          = -1;
  int         exposure       = -1;
};
Q_DECLARE_METATYPE(CameraInfo)

/**
 * @brief Singleton class that manages video capture in a dedicated thread.
 *
 * This class handles the low-level interaction with OpenCV's `VideoCapture`.
 * It runs on a separate thread (QThread) to prevent the GUI from freezing during frame acquisition.
 *
 * **Key Features:**
 * - **Thread-Safe Control:** Uses `std::atomic` variables to accept requests (change camera, update brightness)
 * from the main thread without blocking execution.
 * - **Calibration Support:** Automatically loads calibration matrices and applies
 * lens distortion correction (`cv::undistort`) to every frame if calibration files exist.
 * - **Hardware Abstraction:** Queries the camera for supported features (Focus, Exposure, etc.)
 * and normalizes ranges for the UI.
 */
class VideoCaptureHandler : public QThread
{
  Q_OBJECT
public:
  /**
   * @brief Access the singleton instance.
   */
  static VideoCaptureHandler& instance();

  ~VideoCaptureHandler();

  /**
   * @brief Requests a camera state change (Start, Stop, or Switch).
   *
   * This method sets atomic flags that are picked up by the `run()` loop.
   *
   * @param cameraId The ID of the camera to open (0, 1...), or `STOP_CAMERA` (-1) to close.
   * @param resolution The desired capture resolution.
   */
  void requestCameraChange(int cameraId, const QSize& resolution);

  // --- Property Setters (Thread-Safe) ---
  // These methods update atomic variables. The actual hardware change happens
  // in the next iteration of the run() loop.
  void setCameraName(const std::string& name);
  void setAutoFocus(bool manual);
  void setAutoExposure(bool manual);
  void setBrightness(int value);
  void setContrast(int value);
  void setSaturation(int value);
  void setSharpness(int value);
  void setFocus(int value);
  void setExposure(int value);

  bool isCameraRunning() const;

signals:
  /**
   * @brief Emitted for every processed frame.
   * @param pixmap The video frame, potentially undistorted and converted to QPixmap.
   */
  void newPixmapCaptured(const QPixmap& pixmap);

  /**
   * @brief Emitted when a camera is opened to report which features it supports.
   */
  void propertiesSupported(CameraPropertiesSupport support);

  void cameraInfoChanged(const CameraInfo& values);
  void rangesSupported(CameraPropertyRanges ranges);
  void cameraOpenFailed(int cameraId, const QString& errorMsg);

protected:
  /**
   * @brief The main acquisition loop.
   * Contains the logic for:
   * 1. Opening/Closing the camera.
   * 2. Applying requested property changes.
   * 3. Capturing frames.
   * 4. Applying lens correction (undistort).
   * 5. Emitting the result.
   */
  void run() override;

private:
  explicit VideoCaptureHandler(QObject* parent = nullptr);

  QPixmap          m_pixmap;
  cv::Mat          m_frame;
  cv::VideoCapture m_VideoCapture;

  int m_currentCameraId{ID_CAMERA_DEFAULT};

  CameraInfo m_cameraInfo;

  // --- Atomic Request Flags ---
  std::atomic<int> m_requestedCamera{NO_OP_CAMERA};
  std::atomic<int> m_requestedWidth{0};
  std::atomic<int> m_requestedHeight{0};

  std::atomic<int> m_requestedAutoFocus{STOP_CAMERA};
  std::atomic<int> m_requestedFocus{STOP_CAMERA};
  std::atomic<int> m_requestedAutoExposure{STOP_CAMERA};
  std::atomic<int> m_requestedExposure{STOP_CAMERA};
  std::atomic<int> m_requestedBrightness{STOP_CAMERA};
  std::atomic<int> m_requestedContrast{STOP_CAMERA};
  std::atomic<int> m_requestedSaturation{STOP_CAMERA};
  std::atomic<int> m_requestedSharpness{STOP_CAMERA};

  // --- Calibration Data ---
  bool    m_isCalibrated;
  cv::Mat m_cameraMatrix;    ///< Intrinsic camera parameters.
  cv::Mat m_distCoeffs;      ///< Lens distortion coefficients.
  cv::Mat m_newCameraMatrix; ///< Optimized intrinsic matrix for image scaling.

  /**
   * @brief Loads YAML files from the "calibration/camera" directory.
   */
  void loadCalibration(); // Función auxiliar

  QImage  cvMatToQImage(const cv::Mat& inMat);
  QPixmap cvMatToQPixmap(const cv::Mat& inMat);

  PropertyRange getPropertyRange(int propId);
};

#endif // VIDEOCAPTUREHANDLER_H