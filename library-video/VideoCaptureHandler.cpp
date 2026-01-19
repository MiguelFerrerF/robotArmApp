#include "VideoCaptureHandler.h"
#include <QDebug>
#include <QDir>
#include <QtMath>
#include <opencv2/core/persistence.hpp>

VideoCaptureHandler& VideoCaptureHandler::instance()
{
  static VideoCaptureHandler instance;
  return instance;
}

/**
 * @brief Constructor.
 * Registers Qt meta-types for signal/slot communication across threads.
 * Automatically attempts to load calibration data on startup.
 */
VideoCaptureHandler::VideoCaptureHandler(QObject* parent) : QThread(parent)
{
  qRegisterMetaType<CameraPropertiesSupport>();
  qRegisterMetaType<CameraPropertyRanges>();
  qRegisterMetaType<CameraInfo>();

  m_isCalibrated = false; // Inicializar
  loadCalibration();      // Cargar la calibración al iniciar

  start(QThread::HighestPriority);
}

/**
 * @brief Destructor.
 * Requests thread interruption and waits for the thread to finish.
 */
VideoCaptureHandler::~VideoCaptureHandler()
{
  requestInterruption();
  wait();
}

/**
 * @brief Checks if the camera is currently running (opened).
 * @return True if the camera is opened, false otherwise.
 */
bool VideoCaptureHandler::isCameraRunning() const
{
  return m_VideoCapture.isOpened();
}

/**
 * @brief Requests a camera state change (Start, Stop, or Switch).
 *
 * This method sets atomic flags that are picked up by the `run()` loop.
 *
 * @param cameraId The ID of the camera to open (0, 1...), or `STOP_CAMERA` (-1) to close.
 * @param resolution The desired capture resolution.
 */
void VideoCaptureHandler::requestCameraChange(int cameraId, const QSize& resolution)
{
  m_requestedWidth  = resolution.width();
  m_requestedHeight = resolution.height();

  m_cameraInfo.width  = resolution.width();
  m_cameraInfo.height = resolution.height();
  m_requestedCamera   = cameraId;

  emit cameraInfoChanged(m_cameraInfo);
}

/**
 * @brief Sets the camera name.
 * @param name The desired camera name.
 */
void VideoCaptureHandler::setCameraName(const std::string& name)
{
  m_cameraInfo.name = name;
  emit cameraInfoChanged(m_cameraInfo);
}

/**
 * @brief Sets the auto-focus mode.
 * @param manual True for manual focus, false for auto.
 */
void VideoCaptureHandler::setAutoFocus(bool manual)
{
  m_requestedAutoFocus     = manual ? true : false;
  m_cameraInfo.isFocusAuto = manual ? true : false;
  emit cameraInfoChanged(m_cameraInfo);
}

/**
 * @brief Sets the auto-exposure mode.
 * @param manual True for manual exposure, false for auto.
 */
void VideoCaptureHandler::setAutoExposure(bool manual)
{
  m_requestedAutoExposure     = manual ? true : false;
  m_cameraInfo.isExposureAuto = manual ? true : false;
  emit cameraInfoChanged(m_cameraInfo);
}

/**
 * @brief Sets the focus value.
 * @param value The desired focus value.
 */
void VideoCaptureHandler::setFocus(int value)
{
  m_requestedFocus   = value;
  m_cameraInfo.focus = value;
  emit cameraInfoChanged(m_cameraInfo);
}

/**
 * @brief Sets the exposure value.
 * @param value The desired exposure value.
 */
void VideoCaptureHandler::setBrightness(int value)
{
  m_requestedBrightness   = value;
  m_cameraInfo.brightness = value;
  emit cameraInfoChanged(m_cameraInfo);
}

/**
 * @brief Sets the contrast value.
 * @param value The desired contrast value.
 */
void VideoCaptureHandler::setContrast(int value)
{
  m_requestedContrast   = value;
  m_cameraInfo.contrast = value;
  emit cameraInfoChanged(m_cameraInfo);
}

/**
 * @brief Sets the saturation value.
 * @param value The desired saturation value.
 */
void VideoCaptureHandler::setSaturation(int value)
{
  m_requestedSaturation   = value;
  m_cameraInfo.saturation = value;
  emit cameraInfoChanged(m_cameraInfo);
}

/**
 * @brief Sets the sharpness value.
 * @param value The desired sharpness value.
 */
void VideoCaptureHandler::setSharpness(int value)
{
  m_requestedSharpness   = value;
  m_cameraInfo.sharpness = value;
  emit cameraInfoChanged(m_cameraInfo);
}

/**
 * @brief Sets the exposure value.
 * @param value The desired exposure value.
 */
void VideoCaptureHandler::setExposure(int value)
{
  m_requestedExposure   = value;
  m_cameraInfo.exposure = value;
  emit cameraInfoChanged(m_cameraInfo);
}

/**
 * @brief Retrieves the range of a camera property.
 * @param propId The OpenCV property ID (e.g., cv::CAP_PROP_BRIGHTNESS).
 * @return A PropertyRange struct with min, max, and current values.
 */
PropertyRange VideoCaptureHandler::getPropertyRange(int propId)
{
  PropertyRange range;
  if (!m_VideoCapture.isOpened()) {
    return range;
  }

  double currentValue = m_VideoCapture.get(propId);
  range.current       = currentValue;

  range.min = 0;
  range.max = 255;
  if (qFuzzyIsNull(range.current)) {
    range.current = 126;
  }
  return range;
}

/**
 * @brief Loads camera intrinsics from YAML files.
 *
 * It looks for `camera_matrix.yml` and `dist_coeffs.yml`.
 * If found, it reads the data and prepares `m_newCameraMatrix` (using `m_newCameraMatrix` from the file)
 * to enable the real-time undistortion pipeline in `run()`.
 */
void VideoCaptureHandler::loadCalibration()
{
  QString dirPath        = "calibration/camera";
  QString camMatrixPath  = QDir(dirPath).filePath("camera_matrix.yml");
  QString distCoeffsPath = QDir(dirPath).filePath("dist_coeffs.yml");

  cv::FileStorage fsCam(camMatrixPath.toStdString(), cv::FileStorage::READ);
  cv::FileStorage fsDist(distCoeffsPath.toStdString(), cv::FileStorage::READ);

  if (fsCam.isOpened() && fsDist.isOpened()) {
    fsCam["m_cameraMatrix"] >> m_cameraMatrix;
    fsCam["m_newCameraMatrix"] >> m_newCameraMatrix;
    fsDist["m_distCoeffs"] >> m_distCoeffs;

    if (!m_cameraMatrix.empty() && !m_distCoeffs.empty() && !m_newCameraMatrix.empty()) {
      m_isCalibrated = true;
      qDebug() << "Calibración cargada exitosamente.";
    }
    else {
      qWarning() << "No se pudieron leer todos los datos de los archivos de calibración.";
    }
  }
  else {
    qWarning() << "No se encontraron archivos de calibración. El vídeo no será corregido.";
  }

  fsCam.release();
  fsDist.release();
}

/**
 * @brief The dedicated thread loop for video acquisition.
 *
 * Implements a non-blocking state machine using atomic variables:
 * 1. **State Check:** Checks if a new camera ID has been requested.
 * 2. **Initialization:** Opens the camera using `CAP_DSHOW` (DirectShow) for Windows compatibility.
 * 3. **Property Sync:** Checks atomics (Brightness, Focus, etc.) and applies them to the hardware if changed.
 * 4. **Capture:** Grabs a frame into `m_frame`.
 * 5. **Processing:**
 * - If `m_isCalibrated` is true, applies `cv::undistort` .
 * - Otherwise, performs a raw copy.
 * 6. **Output:** Converts the OpenCV Matrix to QPixmap and emits `newPixmapCaptured`.
 */
void VideoCaptureHandler::run()
{
  while (!isInterruptionRequested()) {
    int requestedCamId = m_requestedCamera.exchange(NO_OP_CAMERA);

    // --- Camera Switching Logic ---
    if (requestedCamId != NO_OP_CAMERA) {
      m_VideoCapture.release();
      if (requestedCamId >= START_CAMERA) {
        int reqWidth  = m_requestedWidth.load();
        int reqHeight = m_requestedHeight.load();

        if (!m_VideoCapture.open(requestedCamId, cv::CAP_DSHOW)) {
          qWarning() << "No se pudo abrir la cámara" << requestedCamId;
          emit cameraOpenFailed(requestedCamId, "Error al abrir la cámara con CAP_DSHOW.");
        }

        if (m_VideoCapture.isOpened()) {
          if (reqWidth > 0 && reqHeight > 0) {
            m_VideoCapture.set(cv::CAP_PROP_FRAME_WIDTH, reqWidth);
            m_VideoCapture.set(cv::CAP_PROP_FRAME_HEIGHT, reqHeight);
            qDebug() << "Solicitando resolución:" << reqWidth << "x" << reqHeight;
          }

          CameraPropertiesSupport support;
          support.brightness   = (m_VideoCapture.get(cv::CAP_PROP_BRIGHTNESS) != 0);
          support.contrast     = (m_VideoCapture.get(cv::CAP_PROP_CONTRAST) != 0);
          support.saturation   = (m_VideoCapture.get(cv::CAP_PROP_SATURATION) != 0);
          support.sharpness    = (m_VideoCapture.get(cv::CAP_PROP_SHARPNESS) != 0);
          support.autoExposure = (m_VideoCapture.get(cv::CAP_PROP_AUTO_EXPOSURE) != 0);
          support.exposure     = (m_VideoCapture.get(cv::CAP_PROP_EXPOSURE) != 0);
          support.autoFocus    = (m_VideoCapture.get(cv::CAP_PROP_AUTOFOCUS) != 0);
          support.focus        = (m_VideoCapture.get(cv::CAP_PROP_FOCUS) == 0);

          emit propertiesSupported(support);

          CameraPropertyRanges ranges;
          ranges.brightness = getPropertyRange(cv::CAP_PROP_BRIGHTNESS);
          ranges.contrast   = getPropertyRange(cv::CAP_PROP_CONTRAST);
          ranges.saturation = getPropertyRange(cv::CAP_PROP_SATURATION);
          ranges.sharpness  = getPropertyRange(cv::CAP_PROP_SHARPNESS);
          ranges.exposure   = getPropertyRange(cv::CAP_PROP_EXPOSURE);
          ranges.focus      = getPropertyRange(cv::CAP_PROP_FOCUS);
          emit rangesSupported(ranges);

          m_cameraInfo.brightness = static_cast<int>(m_VideoCapture.get(cv::CAP_PROP_BRIGHTNESS));
          m_cameraInfo.contrast   = static_cast<int>(m_VideoCapture.get(cv::CAP_PROP_CONTRAST));
          m_cameraInfo.saturation = static_cast<int>(m_VideoCapture.get(cv::CAP_PROP_SATURATION));
          m_cameraInfo.sharpness  = static_cast<int>(m_VideoCapture.get(cv::CAP_PROP_SHARPNESS));
          m_cameraInfo.focus      = static_cast<int>(m_VideoCapture.get(cv::CAP_PROP_FOCUS));
          m_cameraInfo.exposure   = static_cast<int>(m_VideoCapture.get(cv::CAP_PROP_EXPOSURE));
          emit cameraInfoChanged(m_cameraInfo);

          m_requestedAutoFocus    = STOP_CAMERA;
          m_requestedFocus        = STOP_CAMERA;
          m_requestedBrightness   = STOP_CAMERA;
          m_requestedContrast     = STOP_CAMERA;
          m_requestedSaturation   = STOP_CAMERA;
          m_requestedSharpness    = STOP_CAMERA;
          m_requestedAutoExposure = STOP_CAMERA;
          m_requestedExposure     = STOP_CAMERA;
        }
        m_currentCameraId = requestedCamId;
      }
      else if (requestedCamId == STOP_CAMERA) {
        m_currentCameraId = NULL_CAMERA;
      }
    }

    if (m_VideoCapture.isOpened()) {
      // --- Property Application Logic ---
      // Check every atomic property; if it's not STOP_CAMERA, apply it.
      int reqValue = STOP_CAMERA;
      reqValue     = m_requestedBrightness.exchange(STOP_CAMERA);
      if (reqValue != STOP_CAMERA)
        m_VideoCapture.set(cv::CAP_PROP_BRIGHTNESS, reqValue);
      reqValue = m_requestedContrast.exchange(STOP_CAMERA);
      if (reqValue != STOP_CAMERA)
        m_VideoCapture.set(cv::CAP_PROP_CONTRAST, reqValue);
      reqValue = m_requestedSaturation.exchange(STOP_CAMERA);
      if (reqValue != STOP_CAMERA)
        m_VideoCapture.set(cv::CAP_PROP_SATURATION, reqValue);
      reqValue = m_requestedSharpness.exchange(STOP_CAMERA);
      if (reqValue != STOP_CAMERA)
        m_VideoCapture.set(cv::CAP_PROP_SHARPNESS, reqValue);
      reqValue = m_requestedAutoExposure.exchange(STOP_CAMERA);
      if (reqValue != STOP_CAMERA)
        m_VideoCapture.set(cv::CAP_PROP_AUTO_EXPOSURE, reqValue);
      reqValue = m_requestedExposure.exchange(STOP_CAMERA);
      if (reqValue != STOP_CAMERA)
        m_VideoCapture.set(cv::CAP_PROP_EXPOSURE, reqValue);
      reqValue = m_requestedAutoFocus.exchange(STOP_CAMERA);
      if (reqValue != STOP_CAMERA)
        m_VideoCapture.set(cv::CAP_PROP_AUTOFOCUS, reqValue);
      reqValue = m_requestedFocus.exchange(STOP_CAMERA);
      if (reqValue != STOP_CAMERA)
        m_VideoCapture.set(cv::CAP_PROP_FOCUS, reqValue);

      m_VideoCapture >> m_frame;
      if (!m_frame.empty()) {
        cv::Mat correctedFrame;

        // Apply lens correction using the loaded matrices
        if (m_isCalibrated) {
          cv::undistort(m_frame,            // Input (Distorted)
                        correctedFrame,     // Output (Corrected)
                        m_cameraMatrix,     // Intrinsic K
                        m_distCoeffs,       // Distortion D
                        m_newCameraMatrix); // Optimal K
        }
        else {
          correctedFrame = m_frame.clone();
        }
        m_pixmap = cvMatToQPixmap(correctedFrame);
        emit newPixmapCaptured(m_pixmap);
      }
      QThread::msleep(10);
    }
    else {
      QThread::msleep(100);
    }
  }

  m_VideoCapture.release();
  qDebug() << "VideoCaptureHandler::run() - Hilo terminado y cámara liberada.";
}

/**
 * @brief Converts an OpenCV Mat (BGR/Gray) to a Qt QImage (RGB).
 *
 * Handles memory mapping and channel swapping (BGR -> RGB) required
 * for displaying OpenCV images in Qt widgets.
 * @param inMat The input cv::Mat image.
 * @return The converted QImage. If the type is unsupported, returns a null QImage.
 */
QImage VideoCaptureHandler::cvMatToQImage(const cv::Mat& inMat)
{
  switch (inMat.type()) {
    case CV_8UC4: {
      QImage image(inMat.data, inMat.cols, inMat.rows, static_cast<int>(inMat.step), QImage::Format_ARGB32);
      return image;
    }
    case CV_8UC3: {
      // OpenCV uses BGR, Qt uses RGB. rgbSwapped() fixes the colors.
      QImage image(inMat.data, inMat.cols, inMat.rows, static_cast<int>(inMat.step), QImage::Format_RGB888);
      return image.rgbSwapped();
    }
    case CV_8UC1: {
      QImage image(inMat.data, inMat.cols, inMat.rows, static_cast<int>(inMat.step), QImage::Format_Grayscale8);

      return image;
    }
    default:
      qWarning() << "ASM::cvMatToQImage() - cv::Mat image type not handled in "
                    "switch:"
                 << inMat.type();
      break;
  }
  return QImage();
}

/**
 * @brief Converts an OpenCV Mat to a QPixmap.
 * @param inMat The input cv::Mat image.
 * @return The converted QPixmap.
 */
QPixmap VideoCaptureHandler::cvMatToQPixmap(const cv::Mat& inMat)
{
  return QPixmap::fromImage(cvMatToQImage(inMat));
}