#include "RobotControlDialog.h"
#include "ui_RobotControlDialog.h"
#include <QDebug>

RobotControlDialog::RobotControlDialog(QWidget *parent)
    : QDialog(parent), ui(new Ui::RobotControlDialog) {
  ui->setupUi(this);
  qDebug("Ventana de control de robot Creada");

  // Function to connect sliders to labels
  connectSlidersToLabels();
}

RobotControlDialog::~RobotControlDialog() { delete ui; }

void RobotControlDialog::connectSlidersToLabels() {
  // Connect each slider's valueChanged signal to update the corresponding label
  connect(ui->horizontalSlider1, &QSlider::valueChanged, this,
          [this](int value) {
            ui->labelMotor1Slider->setText(QString::number(value));
          });
  connect(ui->horizontalSlider2, &QSlider::valueChanged, this,
          [this](int value) {
            ui->labelMotor2Slider->setText(QString::number(value));
          });
  connect(ui->horizontalSlider3, &QSlider::valueChanged, this,
          [this](int value) {
            ui->labelMotor3Slider->setText(QString::number(value));
          });
  connect(ui->horizontalSlider4, &QSlider::valueChanged, this,
          [this](int value) {
            ui->labelMotor4Slider->setText(QString::number(value));
          });
  connect(ui->horizontalSlider5, &QSlider::valueChanged, this,
          [this](int value) {
            ui->labelMotor5Slider->setText(QString::number(value));
          });
  connect(ui->horizontalSlider6, &QSlider::valueChanged, this,
          [this](int value) {
            ui->labelMotor6Slider->setText(QString::number(value));
          });
}
