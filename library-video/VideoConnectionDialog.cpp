#include "VideoConnectionDialog.h"
#include "VideoCameraHandler.h"
#include "ui_VideoConnectionDialog.h"
#include <QDebug>
#include <QSettings>

VideoConnectionDialog::VideoConnectionDialog(QWidget *parent)
    : QDialog(parent), ui(new Ui::VideoConnectionDialog) {
  ui->setupUi(this);
  setWindowTitle("Setup Video Connection");

  // Cargar configuración previa
  QSettings settings("upna", "robotArmApp");
  QString lastCamera = settings.value("video/camera", "").toString();

  // Llenar combos
  refreshCameras();

  // Seleccionar última cámara usada
  int cameraIndex = ui->comboBoxCamera->findText(lastCamera);
  if (cameraIndex != -1)
    ui->comboBoxCamera->setCurrentIndex(cameraIndex);
}

VideoConnectionDialog::~VideoConnectionDialog() { delete ui; }

void VideoConnectionDialog::refreshCameras() {
  ui->comboBoxCamera->clear();
  const auto cameras = VideoCameraHandler::instance().availableCameras();
  for (const QString &camera : cameras) {
    ui->comboBoxCamera->addItem(camera);
  }
}

void VideoConnectionDialog::on_pushButtonConnect_clicked() {
  QString cameraName = ui->comboBoxCamera->currentText();
  if (cameraName.isEmpty()) {
    emit errorOccurred("No camera selected, please select a camera.");
    return;
  }

  if (VideoCameraHandler::instance().startCamera(cameraName)) {
    // Guardar selección
    QSettings settings("upna", "robotArmApp");
    settings.setValue("video/camera", cameraName);
    accept();
  } else {
    emit errorOccurred("Unable to start camera " + cameraName);
  }
}

void VideoConnectionDialog::showEvent(QShowEvent *event) {
  QDialog::showEvent(event);
  refreshCameras();
}
