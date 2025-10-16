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

  if (m_camera) {
    // Conectar señales relevantes de QCamera
    connect(m_camera, &QCamera::activeChanged, this,
            &VideoCameraHandler::handleActiveChanged);
    connect(m_camera, &QCamera::errorOccurred, this,
            &VideoCameraHandler::handleCameraError);
    connect(m_camera, &QCamera::focusModeChanged, this,
            &VideoCameraHandler::handleFocusModeChanged);
    connect(m_camera, &QCamera::zoomFactorChanged, this,
            &VideoCameraHandler::handleZoomFactorChanged);
    connect(m_camera, &QCamera::exposureModeChanged, this,
            &VideoCameraHandler::handleExposureModeChanged);
    connect(m_camera, &QCamera::whiteBalanceModeChanged, this,
            &VideoCameraHandler::handleWhiteBalanceModeChanged);
    connect(m_camera, &QCamera::colorTemperatureChanged, this,
            &VideoCameraHandler::handleColorTemperatureChanged);
  }

  connect(m_videoSink, &QVideoSink::videoFrameChanged, this,
          &VideoCameraHandler::processFrame);

  m_camera->start();
  if (!m_camera->isActive()) {
    emit errorOccurred("Failed to start the camera.");
    stopCamera();
    return false;
  }

  m_currentCameraName = cameraName;

  // Cargar configuración previa si existe
  setupCameraSettings();
  getCameraInfo();

  return true;
}

bool VideoCameraHandler::stopCamera() {
  if (!m_camera)
    return false;

  // Guardar configuración actual antes de detener la cámara
  saveCameraSettings();

  m_camera->stop();
  delete m_camera;
  delete m_videoSink;

  m_camera = nullptr;
  m_videoSink = nullptr;
  m_currentCameraName.clear();

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

void VideoCameraHandler::handleActiveChanged(bool active) {
  if (active) {
    emit cameraStarted();
  } else {
    emit cameraStopped();
  }
}

void VideoCameraHandler::handleCameraError(QCamera::Error error,
                                           const QString &errorString) {
  if (m_camera)
    emit errorOccurred(errorString);
}

void VideoCameraHandler::handleFocusModeChanged() {
  QCamera::FocusMode mode = m_camera->focusMode();
  switch (mode) {
  case QCamera::FocusMode::FocusModeAuto:
    emit focusModeChanged("Auto");
    break;
  case QCamera::FocusMode::FocusModeAutoNear:
    emit focusModeChanged("Auto Near");
    break;
  case QCamera::FocusMode::FocusModeAutoFar:
    emit focusModeChanged("Auto Far");
    break;
  case QCamera::FocusMode::FocusModeHyperfocal:
    emit focusModeChanged("Hyperfocal");
    break;
  case QCamera::FocusMode::FocusModeInfinity:
    emit focusModeChanged("Infinity");
    break;
  case QCamera::FocusMode::FocusModeManual:
    emit focusModeChanged("Manual");
    break;
  default:
    emit focusModeChanged("Unknown");
    break;
  }
}

void VideoCameraHandler::handleZoomFactorChanged(float zoom) {
  emit zoomFactorChanged(zoom);
}

void VideoCameraHandler::handleExposureModeChanged() {
  QCamera::ExposureMode mode = m_camera->exposureMode();
  switch (mode) {
  case QCamera::ExposureMode::ExposureAuto:
    emit exposureModeChanged("Auto");
    break;
  case QCamera::ExposureMode::ExposureManual:
    emit exposureModeChanged("Manual");
    break;
  case QCamera::ExposureMode::ExposurePortrait:
    emit exposureModeChanged("Portrait");
    break;
  case QCamera::ExposureMode::ExposureNight:
    emit exposureModeChanged("Night");
    break;
  case QCamera::ExposureMode::ExposureSports:
    emit exposureModeChanged("Sports");
    break;
  case QCamera::ExposureMode::ExposureSnow:
    emit exposureModeChanged("Snow");
    break;
  case QCamera::ExposureMode::ExposureBeach:
    emit exposureModeChanged("Beach");
    break;
  case QCamera::ExposureMode::ExposureAction:
    emit exposureModeChanged("Action");
    break;
  case QCamera::ExposureMode::ExposureLandscape:
    emit exposureModeChanged("Landscape");
    break;
  case QCamera::ExposureMode::ExposureNightPortrait:
    emit exposureModeChanged("Night Portrait");
    break;
  case QCamera::ExposureMode::ExposureTheatre:
    emit exposureModeChanged("Theatre");
    break;
  case QCamera::ExposureMode::ExposureSunset:
    emit exposureModeChanged("Sunset");
    break;
  case QCamera::ExposureMode::ExposureSteadyPhoto:
    emit exposureModeChanged("Steady Photo");
    break;
  case QCamera::ExposureMode::ExposureFireworks:
    emit exposureModeChanged("Fireworks");
    break;
  case QCamera::ExposureMode::ExposureParty:
    emit exposureModeChanged("Party");
    break;
  case QCamera::ExposureMode::ExposureCandlelight:
    emit exposureModeChanged("Candlelight");
    break;
  case QCamera::ExposureMode::ExposureBarcode:
    emit exposureModeChanged("Barcode");
    break;
  default:
    emit exposureModeChanged("Unknown");
    break;
  }
}

void VideoCameraHandler::handleWhiteBalanceModeChanged() {
  QCamera::WhiteBalanceMode mode = m_camera->whiteBalanceMode();
  switch (mode) {
  case QCamera::WhiteBalanceMode::WhiteBalanceAuto:
    emit whiteBalanceModeChanged("Auto");
    break;
  case QCamera::WhiteBalanceMode::WhiteBalanceManual:
    emit whiteBalanceModeChanged("Manual");
    break;
  case QCamera::WhiteBalanceMode::WhiteBalanceSunlight:
    emit whiteBalanceModeChanged("Sunlight");
    break;
  case QCamera::WhiteBalanceMode::WhiteBalanceCloudy:
    emit whiteBalanceModeChanged("Cloudy");
    break;
  case QCamera::WhiteBalanceMode::WhiteBalanceShade:
    emit whiteBalanceModeChanged("Shade");
    break;
  case QCamera::WhiteBalanceMode::WhiteBalanceTungsten:
    emit whiteBalanceModeChanged("Tungsten");
    break;
  case QCamera::WhiteBalanceMode::WhiteBalanceFluorescent:
    emit whiteBalanceModeChanged("Fluorescent");
    break;
  case QCamera::WhiteBalanceMode::WhiteBalanceFlash:
    emit whiteBalanceModeChanged("Flash");
    break;
  case QCamera::WhiteBalanceMode::WhiteBalanceSunset:
    emit whiteBalanceModeChanged("Sunset");
    break;
  default:
    emit whiteBalanceModeChanged("Unknown");
    break;
  }
}

void VideoCameraHandler::handleColorTemperatureChanged() {
  int temperature = m_camera->colorTemperature();
  emit colorTemperatureChanged(temperature);
}

void VideoCameraHandler::setupCameraSettings() {
  if (m_currentCameraName.isEmpty())
    return;

  QSettings settings("RobotArmApp", "VideoCameraSettings");
  settings.beginGroup(m_currentCameraName);

  // Configurar enfoque
  m_camera->setFocusMode(
      static_cast<QCamera::FocusMode>(settings.value("FocusMode", 0).toInt()));

  // Configurar zoom
  m_camera->setZoomFactor(settings.value("ZoomFactor", 1.0).toFloat());

  // Configurar exposición
  m_camera->setExposureMode(static_cast<QCamera::ExposureMode>(
      settings.value("ExposureMode", 0).toInt()));

  // Configurar balance de blancos
  m_camera->setWhiteBalanceMode(static_cast<QCamera::WhiteBalanceMode>(
      settings.value("WhiteBalanceMode", 0).toInt()));

  // Configurar temperatura de color
  m_camera->setColorTemperature(
      settings.value("ColorTemperature", 6500).toInt());

  settings.endGroup();
}

void VideoCameraHandler::saveCameraSettings() {
  if (m_currentCameraName.isEmpty())
    return;

  QSettings settings("RobotArmApp", "VideoCameraSettings");
  settings.beginGroup(m_currentCameraName);

  // Guardar enfoque
  settings.setValue("FocusMode", m_camera->focusMode());

  // Guardar zoom
  settings.setValue("ZoomFactor", m_camera->zoomFactor());

  // Guardar exposición
  settings.setValue("ExposureMode", m_camera->exposureMode());

  // Guardar balance de blancos
  settings.setValue("WhiteBalanceMode", m_camera->whiteBalanceMode());

  // Guardar temperatura de color
  settings.setValue("ColorTemperature", m_camera->colorTemperature());

  settings.endGroup();
}

void VideoCameraHandler::getCameraInfo() {
  if (!m_camera)
    return;

  handleFocusModeChanged();
  handleZoomFactorChanged(m_camera->zoomFactor());
  handleExposureModeChanged();
  handleWhiteBalanceModeChanged();
  handleColorTemperatureChanged();
}