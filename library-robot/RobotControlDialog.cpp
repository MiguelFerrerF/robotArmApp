#include "RobotControlDialog.h"
#include "ui_RobotControlDialog.h"
#include <QCoreApplication>
#include <QDebug>
#include <QIntValidator>
#include <QPixmap>
#include <QSlider>

RobotControlDialog::RobotControlDialog(QWidget *parent,
                                       RobotConfig::RobotSettings *settings)
    : QDialog(parent), ui(new Ui::RobotControlDialog),
      m_robotSettings(settings) {
  ui->setupUi(this);
  this->setWindowTitle("Robot Control");

  // Imagen principal del robot (se mantiene el QLabel de la imagen)
  QPixmap pixmap(QCoreApplication::applicationDirPath() + "/images/robot.png");
  ui->labelRobotImage->setAlignment(Qt::AlignCenter);
  ui->labelRobotImage->setScaledContents(true);
  ui->labelRobotImage->setPixmap(pixmap.scaled(500, 500, Qt::KeepAspectRatio));

  connectSlidersToLineEdits();
  connectLineEditsToSliders();
}

RobotControlDialog::~RobotControlDialog() { delete ui; }

void RobotControlDialog::connectSlidersToLineEdits() {
  QSlider *sliders[] = {ui->horizontalSlider1, ui->horizontalSlider2,
                        ui->horizontalSlider3, ui->horizontalSlider4,
                        ui->horizontalSlider5, ui->horizontalSlider6};
  for (int i = 0; i < 6; ++i) {
    connect(sliders[i], &QSlider::valueChanged, this, [this, i](int value) {
      static bool updating = false;
      if (updating)
        return;
      updating = true;
      setLineEditToSliderValue(i + 1, value);
      updating = false;
    });
  }
}

void RobotControlDialog::connectLineEditsToSliders() {

  // Solo permitir números entre 0 y 180 en los QLineEdit
  QLineEdit *lineEdits[] = {ui->lineEditMotor1, ui->lineEditMotor2,
                            ui->lineEditMotor3, ui->lineEditMotor4,
                            ui->lineEditMotor5, ui->lineEditMotor6};

  QIntValidator *validator = new QIntValidator(0, 180, this);
  for (int i = 0; i < 6; ++i) {
    lineEdits[i]->setValidator(validator);
  }
  QSlider *sliders[] = {ui->horizontalSlider1, ui->horizontalSlider2,
                        ui->horizontalSlider3, ui->horizontalSlider4,
                        ui->horizontalSlider5, ui->horizontalSlider6};
  for (int i = 0; i < 6; ++i) {
    connect(lineEdits[i], &QLineEdit::editingFinished, this,
            [lineEdit = lineEdits[i], slider = sliders[i]]() {
              int value = lineEdit->text().toInt();
              if (value < 0 || value > 180) {
                value = qBound(0, value, 180);
              }
              slider->setValue(value);
              lineEdit->setText(QString::number(value));
            });
  }
}

void RobotControlDialog::setLineEditToSliderValue(int motorIndex, int value) {
  if (!m_robotSettings) {
    emit errorOccurred("Robot settings not initialized.");
    return;
  }
  switch (motorIndex) {
  case 1:
    ui->lineEditMotor1->setText(QString::number(value));
    if (!m_isAll)
      emit motorAngleChanged(1, value);
    break;
  case 2:
    ui->lineEditMotor2->setText(QString::number(value));
    if (!m_isAll)
      emit motorAngleChanged(2, value);
    break;
  case 3:
    ui->lineEditMotor3->setText(QString::number(value));
    if (!m_isAll)
      emit motorAngleChanged(3, value);
    break;
  case 4:
    ui->lineEditMotor4->setText(QString::number(value));
    if (!m_isAll)
      emit motorAngleChanged(4, value);
    break;
  case 5:
    ui->lineEditMotor5->setText(QString::number(value));
    if (!m_isAll)
      emit motorAngleChanged(5, value);
    break;
  case 6:
    ui->lineEditMotor6->setText(QString::number(value));
    if (!m_isAll)
      emit motorAngleChanged(6, value);
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

void RobotControlDialog::on_pushButtonAllSingle_clicked() {
  m_isAll = !m_isAll;
  if (m_isAll) {
    ui->pushButtonAllSingle->setText("All");
    ui->pushButtonSetup->setEnabled(true);
  } else {
    ui->pushButtonAllSingle->setText("Single");
    ui->pushButtonSetup->setEnabled(false);
  }
}
