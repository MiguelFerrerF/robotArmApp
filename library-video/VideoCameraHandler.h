#ifndef VIDEOCAMERAHANDLER_H
#define VIDEOCAMERAHANDLER_H

#include <QCamera>
#include <QCameraDevice>
#include <QMediaCaptureSession>
#include <QMediaDevices>
#include <QObject>
#include <QVideoFrame>
#include <QVideoSink>

class VideoCameraHandler : public QObject {
  Q_OBJECT
public:
  static VideoCameraHandler &instance();

  QStringList availableCameras() const;
  bool startCamera(const QString &cameraName);
  bool stopCamera();
  bool isCameraRunning() const;
  QString currentCameraName() const;

signals:
  void errorOccurred(const QString &error);
  void cameraStarted();
  void cameraStopped();
  void focusModeChanged(const QString &mode);
  void zoomFactorChanged(float zoom);
  void exposureModeChanged(const QString &mode);
  void whiteBalanceModeChanged(const QString &mode);
  void colorTemperatureChanged(int temperature);

  void frameCaptured(const QImage &image);

private slots:
  void handleActiveChanged(bool active);
  void handleCameraError(QCamera::Error error, const QString &errorString);
  void handleFocusModeChanged();
  void handleZoomFactorChanged(float zoom);
  void handleExposureModeChanged();
  void handleWhiteBalanceModeChanged();
  void handleColorTemperatureChanged();

  void processFrame(const QVideoFrame &frame);

private:
  explicit VideoCameraHandler(QObject *parent = nullptr);
  ~VideoCameraHandler();

  QCamera *m_camera = nullptr;
  QVideoSink *m_videoSink = nullptr;
  QMediaCaptureSession m_captureSession;

  QString m_currentCameraName;

  void setupCameraSettings();
  void saveCameraSettings();
  void getCameraInfo();
};

#endif // VIDEOCAMERAHANDLER_H
