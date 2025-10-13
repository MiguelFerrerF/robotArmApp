#include <QApplication>
#include <QCamera>
#include <QCameraDevice>
#include <QCameraFormat>
#include <QComboBox>
#include <QImage>
#include <QLabel>
#include <QMainWindow>
#include <QMediaCaptureSession>
#include <QMediaDevices>
#include <QPushButton>
#include <QSlider>
#include <QVBoxLayout>
#include <QVideoFrame>
#include <QVideoSink>

class CameraWindow : public QMainWindow {
  Q_OBJECT

public:
  CameraWindow(QWidget *parent = nullptr) : QMainWindow(parent) {
    auto *widget = new QWidget(this);
    auto *layout = new QVBoxLayout(widget);

    deviceBox = new QComboBox(this);
    layout->addWidget(new QLabel("Selecciona dispositivo:"));
    layout->addWidget(deviceBox);

    videoLabel = new QLabel(this);
    videoLabel->setText("Vista previa de la cámara");
    videoLabel->setAlignment(Qt::AlignCenter);
    videoLabel->setMinimumSize(640, 480);
    videoLabel->setStyleSheet("background-color: black;");
    layout->addWidget(videoLabel);

    startButton = new QPushButton("Iniciar cámara", this);
    layout->addWidget(startButton);

    brillo = new QSlider(Qt::Horizontal, this);
    brillo->setRange(-100, 100);
    layout->addWidget(new QLabel("Brillo"));
    layout->addWidget(brillo);

    widget->setLayout(layout);
    setCentralWidget(widget);

    // Enumerar dispositivos
    for (const QCameraDevice &device : QMediaDevices::videoInputs()) {
      deviceBox->addItem(device.description(), QVariant::fromValue(device));
    }

    connect(startButton, &QPushButton::clicked, this,
            &CameraWindow::iniciarCamara);
    connect(brillo, &QSlider::valueChanged, this, &CameraWindow::ajustarBrillo);
  }

private slots:
  void iniciarCamara() {
    if (camera) {
      camera->stop();
      delete camera;
      delete videoSink;
    }

    QCameraDevice device = deviceBox->currentData().value<QCameraDevice>();
    camera = new QCamera(device);
    videoSink = new QVideoSink(this);

    // Configurar la sesión de captura
    captureSession.setCamera(camera);
    captureSession.setVideoSink(videoSink);

    // Conectar la señal de frame nuevo
    connect(videoSink, &QVideoSink::videoFrameChanged, this,
            &CameraWindow::mostrarFrame);

    camera->start();

    videoLabel->setText("Cámara iniciada: " + device.description());
  }

  void mostrarFrame(const QVideoFrame &frame) {
    if (!frame.isValid())
      return;

    QVideoFrame copyFrame(frame);
    copyFrame.map(QVideoFrame::ReadOnly);
    QImage image = copyFrame.toImage();
    copyFrame.unmap();

    if (image.isNull())
      return;

    // Escalar la imagen al tamaño del QLabel manteniendo proporción
    QPixmap pixmap = QPixmap::fromImage(image).scaled(
        videoLabel->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);

    videoLabel->setPixmap(pixmap);
  }

  void ajustarBrillo(int value) {
    if (camera)
      camera->setExposureCompensation(value / 100.0);
  }

private:
  QComboBox *deviceBox;
  QLabel *videoLabel;
  QPushButton *startButton;
  QSlider *brillo;
  QCamera *camera = nullptr;
  QVideoSink *videoSink = nullptr;
  QMediaCaptureSession captureSession;
};

#include "main.moc"

int main(int argc, char *argv[]) {
  QApplication app(argc, argv);
  CameraWindow window;
  window.resize(800, 600);
  window.show();
  return app.exec();
}
