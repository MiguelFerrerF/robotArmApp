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

struct CameraPropertiesSupport {
  bool autoFocus = false;
  bool focus = true;
  bool autoExposure = false;
  bool exposure = false;
  bool brightness = false;
  bool contrast = false;
  bool saturation = false;
  bool sharpness = false;
};
Q_DECLARE_METATYPE(CameraPropertiesSupport)

// --- NUEVO: Estructuras para rangos dinámicos de propiedades ---
struct PropertyRange {
  double min = 0;
  double max = 255;
  double current = 0; // Valor actual o por defecto
};

struct CameraPropertyRanges {
  PropertyRange brightness;
  PropertyRange contrast;
  PropertyRange saturation;
  PropertyRange sharpness;
  PropertyRange focus;
  PropertyRange exposure;
};
Q_DECLARE_METATYPE(CameraPropertyRanges)
// --- FIN NUEVO ---

class VideoCaptureHandler : public QThread {
  Q_OBJECT
public:
  VideoCaptureHandler(QObject *parent = nullptr);
  ~VideoCaptureHandler();

  void requestCameraChange(int cameraId, const QSize &resolution);

  void setAutoFocus(bool manual);
  void setAutoExposure(bool manual);
  void setBrightness(int value);
  void setContrast(int value);
  void setSaturation(int value);
  void setSharpness(int value);
  void setFocus(int value);
  void setExposure(int value);

signals:
  void newPixmapCaptured(const QPixmap &pixmap);
  void propertiesSupported(CameraPropertiesSupport support);
  void rangesSupported(const CameraPropertyRanges &ranges);
  void cameraOpenFailed(int cameraId, const QString &errorMsg);

protected:
  void run() override;

private:
  QPixmap m_pixmap;
  cv::Mat m_frame;
  cv::VideoCapture m_VideoCapture;

  int m_currentCameraId{ID_CAMERA_DEFAULT};

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

  QImage cvMatToQImage(const cv::Mat &inMat);
  QPixmap cvMatToQPixmap(const cv::Mat &inMat);

  PropertyRange getPropertyRange(int propId);
};

#endif // VIDEOCAPTUREHANDLER_H