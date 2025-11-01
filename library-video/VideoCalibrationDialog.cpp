#include "VideoCalibrationDialog.h"
#include "./ui_VideoCalibrationDialog.h"

VideoCalibrationDialog::VideoCalibrationDialog(QWidget* parent) : QDialog(parent), ui(new Ui::VideoCalibrationDialog)
{
  ui->setupUi(this);
  this->setWindowTitle("Camera Calibration");

  this->setWindowFlags(this->windowFlags() | Qt::WindowMinimizeButtonHint | Qt::WindowMaximizeButtonHint);

  VideoCaptureHandler& handler = VideoCaptureHandler::instance();

  // Conexión para recibir nuevos pixmaps capturados (Temporal mientras el
  // diálogo está abierto)
  connect(&handler, &VideoCaptureHandler::newPixmapCaptured, this, [=](const QPixmap& pixmap) {
    m_currentPixmap = pixmap;
    updateVideoLabel();
  });
}

VideoCalibrationDialog::~VideoCalibrationDialog()
{
  // Desconectar la señal de newPixmapCaptured para que el QLabel del diálogo no
  // se actualice al cerrarse, dejando que MainWindow tome el control.
  disconnect(&VideoCaptureHandler::instance(), SIGNAL(newPixmapCaptured(QPixmap)), this, nullptr);
  delete ui;
}

void VideoCalibrationDialog::on_startButton_clicked()
{
  VideoCaptureHandler& handler = VideoCaptureHandler::instance();
}

void VideoCalibrationDialog::updateVideoLabel()
{
  if (m_currentPixmap.isNull()) {
    return;
  }
  ui->videoLabel->setPixmap(m_currentPixmap.scaled(ui->videoLabel->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
}
