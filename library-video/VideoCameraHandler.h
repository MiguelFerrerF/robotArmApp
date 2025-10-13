#ifndef VIDEOCAMERAHANDLER_H
#define VIDEOCAMERAHANDLER_H

#include <QCamera>
#include <QCameraDevice>
#include <QImageCapture>
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
  void frameCaptured(const QImage &image);

private slots:
  void handleCameraError(QCamera::Error error, const QString &errorString);

private:
  explicit VideoCameraHandler(QObject *parent = nullptr);
  ~VideoCameraHandler();

  QCamera *m_camera = nullptr;
  QVideoSink *m_videoSink = nullptr;
  QMediaCaptureSession m_captureSession;

  QString m_currentCameraName;

  void processFrame(const QVideoFrame &frame);
};

#endif // VIDEOCAMERAHANDLER_H
