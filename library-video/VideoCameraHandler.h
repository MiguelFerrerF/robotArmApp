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
  bool startCamera(const QString &cameraName, int frameRate = 30);
  bool stopCamera();
  bool isCameraRunning() const;
  bool captureFrame();
  QString currentCameraName() const;
  int currentFrameRate() const;

  void setVideoSink(QVideoSink *sink);

signals:
  void errorOccurred(const QString &error);
  void cameraStarted();
  void cameraStopped();
  void frameCaptured(const QImage &frame);

private slots:
  void handleCameraError(QCamera::Error error);
  void onImageCaptured(int id, const QImage &preview);

private:
  explicit VideoCameraHandler(QObject *parent = nullptr);
  ~VideoCameraHandler();

  QCamera *m_camera = nullptr;
  QImageCapture *m_imageCapture = nullptr;
  QVideoSink *m_videoSink = nullptr;

  QMediaCaptureSession m_captureSession;
  QString m_currentCameraName;
  int m_currentFrameRate = 30;
};

#endif // VIDEOCAMERAHANDLER_H
