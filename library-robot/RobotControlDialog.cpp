#include "RobotControlDialog.h"
#include "ui_RobotControlDialog.h"
#include <QDebug>

RobotControlDialog::RobotControlDialog(QWidget *parent,
                                       RobotConfig::RobotSettings *settings)
    : QDialog(parent), ui(new Ui::RobotControlDialog),
      m_robotSettings(settings) {
  ui->setupUi(this);
  this->setWindowTitle("Robot Control");

  // Function to connect sliders to labels
  connectSlidersToLabels();
}

RobotControlDialog::~RobotControlDialog() { delete ui; }

void RobotControlDialog::connectSlidersToLabels() {
  connect(ui->horizontalSlider1, &QSlider::valueChanged, this,
          [this](int value) { setLabelToSliderValue(1, value); });
  connect(ui->horizontalSlider2, &QSlider::valueChanged, this,
          [this](int value) { setLabelToSliderValue(2, value); });
  connect(ui->horizontalSlider3, &QSlider::valueChanged, this,
          [this](int value) { setLabelToSliderValue(3, value); });
  connect(ui->horizontalSlider4, &QSlider::valueChanged, this,
          [this](int value) { setLabelToSliderValue(4, value); });
  connect(ui->horizontalSlider5, &QSlider::valueChanged, this,
          [this](int value) { setLabelToSliderValue(5, value); });
  connect(ui->horizontalSlider6, &QSlider::valueChanged, this,
          [this](int value) { setLabelToSliderValue(6, value); });
}

void RobotControlDialog::setLabelToSliderValue(int motorIndex, int value) {
  switch (motorIndex) {
  case 1:
    ui->labelMotor1Slider->setText(QString::number(value));
    break;
  case 2:
    ui->labelMotor2Slider->setText(QString::number(value));
    break;
  case 3:
    ui->labelMotor3Slider->setText(QString::number(value));
    break;
  case 4:
    ui->labelMotor4Slider->setText(QString::number(value));
    break;
  case 5:
    ui->labelMotor5Slider->setText(QString::number(value));
    break;
  case 6:
    ui->labelMotor6Slider->setText(QString::number(value));
    break;
  default:
    emit errorOccurred("Invalid motor index");
    break;
  }
  if (m_robotSettings) {
    m_robotSettings->motors[motorIndex - 1].desiredAngle = value;
  }
}

void RobotControlDialog::on_pushButtonReset_clicked() {
  if (!m_robotSettings) {
    emit errorOccurred("Robot settings not initialized.");
    return;
  }

  // Reset sliders and labels to default angles
  ui->horizontalSlider1->setValue(m_robotSettings->motors[0].defaultAngle);
  ui->horizontalSlider2->setValue(m_robotSettings->motors[1].defaultAngle);
  ui->horizontalSlider3->setValue(m_robotSettings->motors[2].defaultAngle);
  ui->horizontalSlider4->setValue(m_robotSettings->motors[3].defaultAngle);
  ui->horizontalSlider5->setValue(m_robotSettings->motors[4].defaultAngle);
  ui->horizontalSlider6->setValue(m_robotSettings->motors[5].defaultAngle);

  emit allMotorsReset();
}

void RobotControlDialog::on_pushButtonSetup_clicked() {
  if (!m_robotSettings) {
    emit errorOccurred("Robot settings not initialized.");
    return;
  }

  emit motorAngleChanged(1, ui->horizontalSlider1->value());
  emit motorAngleChanged(2, ui->horizontalSlider2->value());
  emit motorAngleChanged(3, ui->horizontalSlider3->value());
  emit motorAngleChanged(4, ui->horizontalSlider4->value());
  emit motorAngleChanged(5, ui->horizontalSlider5->value());
  emit motorAngleChanged(6, ui->horizontalSlider6->value());
}
