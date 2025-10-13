#include "VideoCameraHandler.h"
#include <QDebug>
#include <QSettings>

VideoCameraHandler &VideoCameraHandler::instance() {
  static VideoCameraHandler instance;
  return instance;
}

VideoCameraHandler::VideoCameraHandler(QObject *parent)
    : QObject(parent), m_camera(nullptr), m_imageCapture(nullptr) {}

VideoCameraHandler::~VideoCameraHandler() { stopCamera(); }

QStringList VideoCameraHandler::availableCameras() const {
  QStringList cameraNames;
  for (const QCameraDevice &camera : QMediaDevices::videoInputs()) {
    cameraNames << camera.description();
  }
  return cameraNames;
}

bool VideoCameraHandler::startCamera(const QString &cameraName, int frameRate) {
  if (m_camera && m_camera->isActive()) {
    emit errorOccurred("Camera is already running.");
    return false;
  }

  const auto cameras = QMediaDevices::videoInputs();
  QCameraDevice selectedCamera;
  bool found = false;

  for (const QCameraDevice &camera : cameras) {
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
  m_imageCapture = new QImageCapture(this);

  connect(m_camera, &QCamera::errorOccurred, this,
          &VideoCameraHandler::handleCameraError);
  connect(m_imageCapture, &QImageCapture::imageCaptured, this,
          &VideoCameraHandler::onImageCaptured);

  m_captureSession.setCamera(m_camera);
  m_captureSession.setImageCapture(m_imageCapture);
  if (m_videoSink)
    m_captureSession.setVideoSink(m_videoSink);

  m_camera->start();

  if (!m_camera->isActive()) {
    emit errorOccurred("Failed to start the camera.");
    stopCamera();
    return false;
  }

  m_currentCameraName = cameraName;
  m_currentFrameRate = frameRate;

  emit cameraStarted();
  return true;
}

bool VideoCameraHandler::stopCamera() {
  if (!m_camera)
    return false;

  m_camera->stop();

  delete m_imageCapture;
  delete m_camera;
  m_imageCapture = nullptr;
  m_camera = nullptr;
  m_currentCameraName.clear();
  m_currentFrameRate = 30;

  emit cameraStopped();
  return true;
}

bool VideoCameraHandler::isCameraRunning() const {
  return m_camera && m_camera->isActive();
}

bool VideoCameraHandler::captureFrame() {
  if (!m_imageCapture || !m_camera || !m_camera->isActive()) {
    emit errorOccurred("Camera not active or image capture not available.");
    return false;
  }

  if (m_imageCapture->isReadyForCapture()) {
    m_imageCapture->capture();
    return true;
  } else {
    emit errorOccurred("Camera not ready for capture.");
    return false;
  }
}

void VideoCameraHandler::onImageCaptured(int id, const QImage &preview) {
  Q_UNUSED(id);
  emit frameCaptured(preview);
}

QString VideoCameraHandler::currentCameraName() const {
  return m_currentCameraName;
}

int VideoCameraHandler::currentFrameRate() const { return m_currentFrameRate; }

void VideoCameraHandler::setVideoSink(QVideoSink *sink) {
  m_videoSink = sink;
  if (m_camera && m_captureSession.camera()) {
    m_captureSession.setVideoSink(sink);
  }
}

void VideoCameraHandler::handleCameraError(QCamera::Error error) {
  Q_UNUSED(error);
  if (m_camera)
    emit errorOccurred(m_camera->errorString());
}
