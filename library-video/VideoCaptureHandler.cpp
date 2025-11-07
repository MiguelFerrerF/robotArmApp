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

VideoCaptureHandler::VideoCaptureHandler(QObject* parent) : QThread(parent)
{
  qRegisterMetaType<CameraPropertiesSupport>();
  qRegisterMetaType<CameraPropertyRanges>();
  qRegisterMetaType<CameraInfo>();

  m_isCalibrated = false; // Inicializar
  loadCalibration();      // Cargar la calibración al iniciar

  start(QThread::HighestPriority);
}

VideoCaptureHandler::~VideoCaptureHandler()
{
  requestInterruption();
  wait();
}

bool VideoCaptureHandler::isCameraRunning() const
{
  return m_VideoCapture.isOpened();
}

void VideoCaptureHandler::requestCameraChange(int cameraId, const QSize& resolution)
{
  m_requestedWidth  = resolution.width();
  m_requestedHeight = resolution.height();

  m_cameraInfo.width  = resolution.width();
  m_cameraInfo.height = resolution.height();
  m_requestedCamera   = cameraId;

  emit cameraInfoChanged(m_cameraInfo);
}

void VideoCaptureHandler::setCameraName(const std::string& name)
{
  m_cameraInfo.name = name;
  emit cameraInfoChanged(m_cameraInfo);
}
void VideoCaptureHandler::setAutoFocus(bool manual)
{
  m_requestedAutoFocus     = manual ? true : false;
  m_cameraInfo.isFocusAuto = manual ? true : false;
  emit cameraInfoChanged(m_cameraInfo);
}
void VideoCaptureHandler::setAutoExposure(bool manual)
{
  m_requestedAutoExposure     = manual ? true : false;
  m_cameraInfo.isExposureAuto = manual ? true : false;
  emit cameraInfoChanged(m_cameraInfo);
}
void VideoCaptureHandler::setFocus(int value)
{
  m_requestedFocus   = value;
  m_cameraInfo.focus = value;
  emit cameraInfoChanged(m_cameraInfo);
}
void VideoCaptureHandler::setBrightness(int value)
{
  m_requestedBrightness   = value;
  m_cameraInfo.brightness = value;
  emit cameraInfoChanged(m_cameraInfo);
}
void VideoCaptureHandler::setContrast(int value)
{
  m_requestedContrast   = value;
  m_cameraInfo.contrast = value;
  emit cameraInfoChanged(m_cameraInfo);
}
void VideoCaptureHandler::setSaturation(int value)
{
  m_requestedSaturation   = value;
  m_cameraInfo.saturation = value;
  emit cameraInfoChanged(m_cameraInfo);
}
void VideoCaptureHandler::setSharpness(int value)
{
  m_requestedSharpness   = value;
  m_cameraInfo.sharpness = value;
  emit cameraInfoChanged(m_cameraInfo);
}
void VideoCaptureHandler::setExposure(int value)
{
  m_requestedExposure   = value;
  m_cameraInfo.exposure = value;
  emit cameraInfoChanged(m_cameraInfo);
}
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

void VideoCaptureHandler::loadCalibration()
{
  QString dirPath        = "calibration";
  QString camMatrixPath  = QDir(dirPath).filePath("camera_matrix.yml");
  QString distCoeffsPath = QDir(dirPath).filePath("dist_coeffs.yml");

  qDebug() << "Cargando archivos de calibración desde:" << camMatrixPath << "y" << distCoeffsPath;

  cv::FileStorage fsCam(camMatrixPath.toStdString(), cv::FileStorage::READ);
  cv::FileStorage fsDist(distCoeffsPath.toStdString(), cv::FileStorage::READ);

  if (fsCam.isOpened() && fsDist.isOpened()) {
    fsCam["m_cameraMatrix"] >> m_cameraMatrix;
    fsCam["m_newCameraMatrix"] >> m_newCameraMatrix; // <-- ¡NUEVO! Cargar
    fsDist["m_distCoeffs"] >> m_distCoeffs;

    // Imprimir en consola (como tenías antes)
    std::cout << "Matriz de Cámara (Original):\\n" << m_cameraMatrix << std::endl;
    std::cout << "Matriz de Cámara (Óptima):\\n" << m_newCameraMatrix << std::endl;
    std::cout << "Coeficientes de Distorsión:\\n" << m_distCoeffs << std::endl;

    // Comprobar que se cargaron las TRES matrices
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

void VideoCaptureHandler::run()
{
  while (!isInterruptionRequested()) {
    int requestedCamId = m_requestedCamera.exchange(NO_OP_CAMERA);
    if (requestedCamId != NO_OP_CAMERA) {

      m_VideoCapture.release(); // Cerrar cámara anterior

      if (requestedCamId >= START_CAMERA) {

        // Cargar la resolución solicitada
        int reqWidth  = m_requestedWidth.load();
        int reqHeight = m_requestedHeight.load();

        if (!m_VideoCapture.open(requestedCamId, cv::CAP_DSHOW)) {
          qWarning() << "No se pudo abrir la cámara" << requestedCamId;
          emit cameraOpenFailed(requestedCamId, "Error al abrir la cámara con CAP_DSHOW.");
        }

        if (m_VideoCapture.isOpened()) {
          // Aplicar la resolución solicitada
          if (reqWidth > 0 && reqHeight > 0) {
            m_VideoCapture.set(cv::CAP_PROP_FRAME_WIDTH, reqWidth);
            m_VideoCapture.set(cv::CAP_PROP_FRAME_HEIGHT, reqHeight);
            qDebug() << "Solicitando resolución:" << reqWidth << "x" << reqHeight;
          }

          // 1. Comprobación de propiedades y emite qué propiedades son
          // soportadas
          CameraPropertiesSupport support;
          support.brightness   = (m_VideoCapture.get(cv::CAP_PROP_BRIGHTNESS) != 0);
          support.contrast     = (m_VideoCapture.get(cv::CAP_PROP_CONTRAST) != 0);
          support.saturation   = (m_VideoCapture.get(cv::CAP_PROP_SATURATION) != 0);
          support.sharpness    = (m_VideoCapture.get(cv::CAP_PROP_SHARPNESS) != 0);
          support.autoExposure = (m_VideoCapture.get(cv::CAP_PROP_AUTO_EXPOSURE) != 0);
          support.exposure     = (m_VideoCapture.get(cv::CAP_PROP_EXPOSURE) != 0);
          support.autoFocus    = (m_VideoCapture.get(cv::CAP_PROP_AUTOFOCUS) != 0);
          support.focus        = (m_VideoCapture.get(cv::CAP_PROP_FOCUS) == 0); // No funciona en todas

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

        cv::Mat correctedFrame; // Fotograma corregido

        if (m_isCalibrated) {
          cv::undistort(m_frame,            // 1. Entrada
                        correctedFrame,     // 2. Salida
                        m_cameraMatrix,     // 3. Matriz Original
                        m_distCoeffs,       // 4. Coeficientes
                        m_newCameraMatrix); // 5. Matriz Óptima
        }
        else {
          // Si no hay calibración, solo clonar
          correctedFrame = m_frame.clone();
        }

        // Emitir la imagen corregida
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

QImage VideoCaptureHandler::cvMatToQImage(const cv::Mat& inMat)
{
  switch (inMat.type()) {
    case CV_8UC4: {
      QImage image(inMat.data, inMat.cols, inMat.rows, static_cast<int>(inMat.step), QImage::Format_ARGB32);
      return image;
    }
    case CV_8UC3: {
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

QPixmap VideoCaptureHandler::cvMatToQPixmap(const cv::Mat& inMat)
{
  return QPixmap::fromImage(cvMatToQImage(inMat));
}