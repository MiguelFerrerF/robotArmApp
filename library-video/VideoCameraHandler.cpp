#include "VideoCameraHandler.h"
#include <QDebug>
#include <QSettings>

VideoCameraHandler &VideoCameraHandler::instance() {
  static VideoCameraHandler instance;
  return instance;
}

VideoCameraHandler::VideoCameraHandler(QObject *parent)
    : QObject(parent), m_camera(nullptr), m_videoSink(nullptr) {}

VideoCameraHandler::~VideoCameraHandler() { stopCamera(); }

QStringList VideoCameraHandler::availableCameras() const {
  QStringList cameraNames;
  for (const QCameraDevice &camera : QMediaDevices::videoInputs()) {
    cameraNames << camera.description();
  }
  return cameraNames;
}

bool VideoCameraHandler::startCamera(const QString &cameraName) {
  if (m_camera && m_videoSink && m_camera->isActive()) {
    emit errorOccurred("Camera is already running.");
    return false;
  }

  QCameraDevice selectedCamera;
  bool found = false;
  for (const QCameraDevice &camera : QMediaDevices::videoInputs()) {
    if (camera.description() == cameraName) {
      selectedCamera = camera;
      found = true;
      break;
    }
  }
  if (!found) {
    emit errorOccurred("Selected camera not found.");
    return false;
  }

  m_camera = new QCamera(selectedCamera);
  m_videoSink = new QVideoSink(this);

  m_captureSession.setCamera(m_camera);
  m_captureSession.setVideoSink(m_videoSink);

  connect(m_camera, &QCamera::errorOccurred, this,
          &VideoCameraHandler::handleCameraError);
<<<<<<< HEAD
  connect(m_imageCapture, &QImageCapture::imageCaptured, this,
          &VideoCameraHandler::onImageCaptured);

  m_captureSession.setCamera(m_camera);
  m_captureSession.setImageCapture(m_imageCapture);
  if (m_videoSink)
     m_captureSession.setVideoSink(m_videoSink);
=======
  connect(m_videoSink, &QVideoSink::videoFrameChanged, this,
          &VideoCameraHandler::processFrame);
>>>>>>> acd5341490aa9dabd720bac693ec47e42c7b5b83

  m_camera->start();
  if (!m_camera->isActive()) {
    emit errorOccurred("Failed to start the camera.");
    stopCamera();
    return false;
  }

  m_currentCameraName = cameraName;
  emit cameraStarted();
  return true;
}

bool VideoCameraHandler::stopCamera() {
  if (!m_camera)
    return false;

  m_camera->stop();
  delete m_camera;
  delete m_videoSink;

  m_camera = nullptr;
  m_videoSink = nullptr;
  m_currentCameraName.clear();

  emit cameraStopped();
  return true;
}

bool VideoCameraHandler::isCameraRunning() const {
  return m_camera && m_camera->isActive();
}

void VideoCameraHandler::processFrame(const QVideoFrame &frame) {
  if (!frame.isValid())
    return;

  QVideoFrame copyFrame(frame);
  copyFrame.map(QVideoFrame::ReadOnly);
  QImage image = copyFrame.toImage();
  copyFrame.unmap();

  if (image.isNull())
    return;

  emit frameCaptured(image);
}

QString VideoCameraHandler::currentCameraName() const {
  return m_currentCameraName;
}

void VideoCameraHandler::handleCameraError(QCamera::Error error,
                                           const QString &errorString) {
  Q_UNUSED(error);
  if (m_camera)
    emit errorOccurred(errorString);
}
